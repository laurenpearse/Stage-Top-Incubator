#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31865.h>

// =====================================================================
//  CLOSED-LOOP INCUBATOR CONTROL  +  OPEN-LOOP HEATED BASE
//
//  Chamber air  -> PT1000 (MAX31865)  -> wall heaters   -> setpoint 39.0 C
//  ITO glass    -> thermistor A0      -> glass heater   -> setpoint 48.0 C
//  Base glass   -> NO SENSOR          -> fixed duty, command "b"
//
//  Logged only (no control):
//    A2     well / reservoir water temperature, tip at the bottom
//    A3     ambient room temperature
//
//  A1 is not connected (thermistor failed).
//  The SCD30 has been removed (module failed), so there is no CO2, no RH,
//  no dew point and no fog margin in this build.
//
//  BASE HEATER SAFETY.  There is no thermistor on the base pane, so it is
//  guarded indirectly by the chamber air. With the base at 45% the chamber
//  loop was already down at ~3% duty, i.e. almost no authority left to pull
//  the air back down. A base runaway therefore shows up as T_cham rising
//  above setpoint. Above BASE_SOFT_T the duty is walked down to
//  BASE_FALLBACK; above BASE_HARD_T it is cut outright.
//
//  Measured base calibration (chamber 39 C, ambient ~19.5 C):
//      0%  -> 26.1 C     20% -> 36.2 C     45% -> 46.7 C
//      least-squares:  T_base ~= 26.4 + 0.456 * duty
//  Only valid at a similar chamber setpoint and ambient.
// =====================================================================

// =====================================================================
//  SENSOR PLACEMENT
// =====================================================================
const char* PT_NAME = "T_cham";   // PT1000, chamber air centre

const char* THERM_NAME[4] = {
  "T_glass",      // A0 - ITO glass lid                 <-- GLASS CONTROL SENSOR
  "",             // A1 - NOT CONNECTED (failed)
  "T_well",       // A2 - in the well water, tip at the bottom
  "T_amb"         // A3 - OUTSIDE the chamber, room air
};
const bool THERM_USED[4] = { true, false, true, true };

const int CH_GLASS = 0;
const int CH_WELL  = 2;
const int CH_AMB   = 3;

// =====================================================================
//  OUTPUT PINS
// =====================================================================
const int HEATER_PIN = D4;   // PB5  - wall heaters
const int GLASS_PIN  = D8;   // PA9  - ITO glass lid
const int BASE_PIN   = D6;   // PB10 - base glass, open loop

// =====================================================================
//  CONTROLLER PARAMETERS   (duty in PERCENT, 0 to 100)
// =====================================================================
const float KP_CHAM = 7.90f,  KI_CHAM = 1.02e-2f;
const float KP_GLAS = 14.99f, KI_GLAS = 4.31e-2f;
const float B_WEIGHT = 0.5f;
const float TS = 1.0f;

const float FF_H_C =  3.054f, FF_H_G = -1.302f;
const float FF_G_C = -3.377f, FF_G_G =  4.024f;

// =====================================================================
//  SAFETY
// =====================================================================
const float T_TRIP        = 55.0f;  // chamber or lid glass above this -> everything off
const float SP_MAX        = 50.0f;  // highest setpoint the commands will accept
const int   BAD_LIMIT     = 5;      // consecutive bad reads before tripping a loop
const float MARGIN_MIN    = 0.5f;   // warn if T_glass - T_cham drops below this

const float BASE_DUTY_MAX = 45.0f;  // never exceed the highest verified duty
const float BASE_SOFT_T   = 41.0f;  // T_cham above this -> back the base duty off gradually
const float BASE_HARD_T   = 43.0f;  // T_cham above this -> base heater off at once
const float BASE_RAMP     = 0.5f;   // % of duty removed per second while over the soft limit
const float BASE_FALLBACK = 35.0f;  // the back-off stops here, it does not run down to zero
const int   BASE_GUARD_N  = 5;      // consecutive seconds over the soft limit before backing off

//  Base calibration, for the confirmation message only. Not a measurement.
const float BASE_CAL_A = 26.4f, BASE_CAL_B = 0.456f;

// =====================================================================
//  THERMISTOR CONSTANTS (TE GA10K3MCD1)
// =====================================================================
const float R0 = 10000.0, T0 = 298.15, BETA = 3976.0;
const float V_SUPPLY = 3.29;

float R_FIXED[4]  = { 9840.0, 9980.0, 9890.0, 9940.0 };
float T_OFFSET[4] = { -0.06, +0.51, -0.25, -0.23 };

const int NUM_THERM = 4;
const int SAMPLES   = 16;

// =====================================================================
Adafruit_ADS1115 ads;
Adafruit_MAX31865 rtd = Adafruit_MAX31865(10, 11, 12, 13);   // CS, SDI, SDO, CLK
#define RREF      4300.0
#define RNOMINAL  1000.0
const int PT_SAMPLES = 2;

const unsigned long INTERVAL = 1000;
unsigned long lastLog = 0;
int rowCount = 0;
uint8_t lastFault = 0;

// =====================================================================
//  CONTROLLER STATE
// =====================================================================
enum Mode { MODE_OFF, MODE_PID, MODE_MAN, MODE_TRIP };
Mode mode = MODE_OFF;

struct Loop {
  const char* name;
  float kp, ki, b;
  float sp;
  float integ;
  float duty;
  float amb0;
  int   bad;
  bool  tripped;
};

Loop cham  = { "chamber", KP_CHAM, KI_CHAM, B_WEIGHT, 39.0f, 0, 0, 20.0f, 0, false };
Loop glas  = { "glass",   KP_GLAS, KI_GLAS, B_WEIGHT, 48.0f, 0, 0, 20.0f, 0, false };

//  Base heater: open loop, independent of mode, so you can set it while the
//  chamber and glass loops stay closed.
float baseDuty    = 0.0f;
float baseSetDuty = 0.0f;   // what you commanded, before any guard back-off
bool  baseTripped = false;
bool  baseBackingOff = false;
int   baseGuardCount = 0;

char cmdBuf[48];
int  cmdLen = 0;
int  marginWarn = 0;
float lastAmb = NAN;

void logEvent(const char* what);   // defined further down

// ---------------------------------------------------------------------
void applyOutputs() {
  int h = (int)(cham.duty * 255.0f / 100.0f + 0.5f);
  int g = (int)(glas.duty * 255.0f / 100.0f + 0.5f);
  int b = (int)(baseDuty  * 255.0f / 100.0f + 0.5f);
  analogWrite(HEATER_PIN, constrain(h, 0, 255));
  analogWrite(GLASS_PIN,  constrain(g, 0, 255));
  analogWrite(BASE_PIN,   constrain(b, 0, 255));
}

float readVolts(uint8_t ch) {
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += ads.computeVolts(ads.readADC_SingleEnded(ch));
  return sum / SAMPLES;
}

float readThermistor(uint8_t ch) {
  float v = readVolts(ch);
  if (v < 0.01 || v > V_SUPPLY - 0.01) return NAN;
  float rTherm = R_FIXED[ch] * v / (V_SUPPLY - v);
  float tK = 1.0 / (1.0 / T0 + log(rTherm / R0) / BETA);
  return tK - 273.15 + T_OFFSET[ch];
}

// ---------------------------------------------------------------------
//  PI with setpoint weighting + clamping anti-windup
// ---------------------------------------------------------------------
void updateLoop(Loop &L, float y) {
  float e  = L.sp - y;
  float ep = L.b * (L.sp - L.amb0) - (y - L.amb0);
  float raw = L.kp * ep + L.integ + L.ki * e * TS;
  float u   = constrain(raw, 0.0f, 100.0f);
  if (u == raw) L.integ += L.ki * e * TS;
  L.duty = u;
}

void preloadIntegrators(float amb) {
  float dc = cham.sp - amb;
  float dg = glas.sp - amb;
  float uh = constrain(FF_H_C * dc + FF_H_G * dg, 0.0f, 100.0f);
  float ug = constrain(FF_G_C * dc + FF_G_G * dg, 0.0f, 100.0f);
  cham.integ = constrain(uh + cham.kp * (1.0f - cham.b) * dc, 0.0f, 100.0f);
  glas.integ = constrain(ug + glas.kp * (1.0f - glas.b) * dg, 0.0f, 100.0f);
  cham.amb0 = amb;
  glas.amb0 = amb;
}

// ---------------------------------------------------------------------
//  PRINTING
// ---------------------------------------------------------------------
void printPaddedDec(float v, int width, int dec) {
  char buf[20];
  if (isnan(v)) { strcpy(buf, "---"); }
  else {
    bool neg = v < 0;
    float a = neg ? -v : v;
    long  mul = (dec == 1) ? 10 : 100;
    long  whole = (long)a;
    long  frac  = (long)((a - whole) * mul + 0.5f);
    if (frac >= mul) { frac -= mul; whole += 1; }
    if (dec == 1) snprintf(buf, sizeof(buf), "%s%ld.%01ld", neg ? "-" : "", whole, frac);
    else          snprintf(buf, sizeof(buf), "%s%ld.%02ld", neg ? "-" : "", whole, frac);
  }
  for (int i = strlen(buf); i < width; i++) Serial.print(' ');
  Serial.print(buf);
}

void printPaddedStr(const char* s, int width) {
  for (int i = strlen(s); i < width; i++) Serial.print(' ');
  Serial.print(s);
}

const char* modeStr() {
  switch (mode) {
    case MODE_PID:  return "PID";
    case MODE_MAN:  return "MAN";
    case MODE_TRIP: return "TRIP";
    default:        return "OFF";
  }
}

void printHeader() {
  Serial.println();
  printPaddedStr("t_s", 6);
  printPaddedStr("clock", 10);
  printPaddedStr("mode", 6);
  printPaddedStr("heat%", 7);
  printPaddedStr("glass%", 7);
  printPaddedStr("base%", 7);
  printPaddedStr(PT_NAME, 9);
  for (int i = 0; i < NUM_THERM; i++) if (THERM_USED[i]) printPaddedStr(THERM_NAME[i], 9);
  printPaddedStr("marg", 7);
  printPaddedStr("flt", 5);
  Serial.println();
  Serial.println("--------------------------------------------------------------------------------------");
}

void printHelp() {
  Serial.println("#");
  Serial.println("#  COMMANDS");
  Serial.println("#    go       start closed-loop control (integrators preloaded from ambient)");
  Serial.println("#    off      everything off, loops reset, base heater off");
  Serial.println("#    sc 39    set CHAMBER setpoint, degC   (max 50)");
  Serial.println("#    sg 48    set GLASS setpoint, degC     (max 50)");
  Serial.println("#    b45      BASE glass duty %, open loop (max 45, 0 to switch off)");
  Serial.println("#             independent of mode. If T_cham goes over 41 C the duty is");
  Serial.println("#             backed off 0.5 %/s down to 35 %; over 43 C it is cut outright");
  Serial.println("#    h25      MANUAL heater duty %  (leaves closed loop)");
  Serial.println("#    g20      MANUAL glass duty %   (leaves closed loop)");
  Serial.println("#    st       print setpoints and controller internals");
  Serial.println("#    n text   write a note into the log");
  Serial.println("#    ?        this help");
  Serial.println("#");
  Serial.println("#  COLUMNS");
  Serial.println("#    T_cham  PT1000, chamber air (control)   T_glass  ITO lid (control)");
  Serial.println("#    T_well  well / reservoir water          T_amb    room air");
  Serial.println("#    marg    T_glass - T_cham");
  Serial.println("#    base%   base glass duty, OPEN LOOP, no sensor on the pane");
  Serial.println("#");
}

void eventStamp() {
  Serial.print("#  >>> t=");
  Serial.print(millis() / 1000);
  Serial.print("s   ");
}

void logEvent(const char* what) {
  eventStamp();
  Serial.println(what);
  rowCount = 999;
}

void printStatus() {
  Serial.println("#");
  Serial.print("#  mode=");         Serial.println(modeStr());
  Serial.print("#  chamber: sp=");  Serial.print(cham.sp, 2);
  Serial.print(" Kp=");             Serial.print(cham.kp, 3);
  Serial.print(" Ki=");             Serial.print(cham.ki, 5);
  Serial.print(" integ=");          Serial.print(cham.integ, 2);
  Serial.print(" duty=");           Serial.print(cham.duty, 1);
  Serial.print(" amb0=");           Serial.print(cham.amb0, 2);
  Serial.print(" bad=");            Serial.println(cham.bad);
  Serial.print("#  glass:   sp=");  Serial.print(glas.sp, 2);
  Serial.print(" Kp=");             Serial.print(glas.kp, 3);
  Serial.print(" Ki=");             Serial.print(glas.ki, 5);
  Serial.print(" integ=");          Serial.print(glas.integ, 2);
  Serial.print(" duty=");           Serial.print(glas.duty, 1);
  Serial.print(" amb0=");           Serial.print(glas.amb0, 2);
  Serial.print(" bad=");            Serial.println(glas.bad);
  Serial.print("#  base:    duty="); Serial.print(baseDuty, 1);
  Serial.print(" % of commanded ");  Serial.print(baseSetDuty, 1);
  Serial.print(" %  (est T_base ");  Serial.print(BASE_CAL_A + BASE_CAL_B * baseDuty, 1);
  Serial.print(" C)  backing off="); Serial.print(baseBackingOff ? "yes" : "no");
  Serial.print("  tripped=");        Serial.println(baseTripped ? "yes" : "no");
  Serial.print("#  trip=");         Serial.print(T_TRIP, 1);
  Serial.print(" C   base soft=");  Serial.print(BASE_SOFT_T, 1);
  Serial.print(" C  hard=");        Serial.print(BASE_HARD_T, 1);
  Serial.print(" C   sp_max=");     Serial.print(SP_MAX, 1);
  Serial.println(" C");
  Serial.println("#");
  rowCount = 999;
}

void allOff(const char* why) {
  cham.duty = 0; glas.duty = 0; baseDuty = 0; baseSetDuty = 0;
  cham.integ = 0; glas.integ = 0;
  cham.bad = 0; glas.bad = 0;
  cham.tripped = false; glas.tripped = false; baseTripped = false;
  baseBackingOff = false;
  baseGuardCount = 0;
  applyOutputs();
  logEvent(why);
}

// ---------------------------------------------------------------------
void handleCommand(float ambNow) {
  cmdBuf[cmdLen] = '\0';
  char* p = cmdBuf;
  while (*p == ' ') p++;

  if (strncmp(p, "go", 2) == 0) {
    float a = isnan(ambNow) ? 20.0f : ambNow;
    preloadIntegrators(a);
    cham.bad = 0; glas.bad = 0;
    cham.tripped = false; glas.tripped = false;
    mode = MODE_PID;
    eventStamp();
    Serial.print("CLOSED LOOP ON   sp_cham=");  Serial.print(cham.sp, 1);
    Serial.print("  sp_glass=");                Serial.print(glas.sp, 1);
    Serial.print("  amb0=");                    Serial.print(a, 2);
    Serial.print("  preload h=");               Serial.print(cham.integ, 1);
    Serial.print("  g=");                       Serial.print(glas.integ, 1);
    Serial.print("  base duty=");               Serial.println(baseDuty, 1);
    rowCount = 999;
  }
  else if (strncmp(p, "off", 3) == 0) {
    mode = MODE_OFF;
    allOff("ALL OFF, loops reset, base heater off");
  }
  else if (strncmp(p, "st", 2) == 0) {
    printStatus();
  }
  else if (p[0] == 's' && (p[1] == 'c' || p[1] == 'g')) {
    bool isCham = (p[1] == 'c');
    float v = atof(p + 2);
    if (v > 10.0f && v < SP_MAX) {
      if (isCham) cham.sp = v; else glas.sp = v;
      eventStamp();
      Serial.print(isCham ? "CHAMBER" : "GLASS");
      Serial.print(" SETPOINT -> ");  Serial.print(v, 2);  Serial.println(" C");
      rowCount = 999;
    } else {
      Serial.print("#  setpoint must be between 10 and "); Serial.println(SP_MAX, 1);
    }
  }
  else if (p[0] == 'b' || p[0] == 'B') {
    float v = atof(p + 1);
    if (v >= 0 && v <= BASE_DUTY_MAX) {
      baseDuty = v;
      baseSetDuty = v;
      baseTripped = false;
      baseBackingOff = false;
      baseGuardCount = 0;
      applyOutputs();
      eventStamp();
      Serial.print("BASE GLASS -> "); Serial.print(v, 1);
      Serial.print(" % open loop, est T_base ");
      Serial.print(BASE_CAL_A + BASE_CAL_B * v, 1); Serial.println(" C");
      rowCount = 999;
    } else {
      Serial.print("#  base duty must be 0-"); Serial.println(BASE_DUTY_MAX, 0);
    }
  }
  else if (p[0] == 'h' || p[0] == 'H') {
    float v = atof(p + 1);
    if (v >= 0 && v <= 100) {
      mode = MODE_MAN; cham.duty = v; cham.integ = 0; applyOutputs();
      eventStamp(); Serial.print("MANUAL: heaters -> "); Serial.print(v, 1); Serial.println(" %");
      rowCount = 999;
    } else Serial.println("#  duty must be 0-100");
  }
  else if (p[0] == 'g' || p[0] == 'G') {
    float v = atof(p + 1);
    if (v >= 0 && v <= 100) {
      mode = MODE_MAN; glas.duty = v; glas.integ = 0; applyOutputs();
      eventStamp(); Serial.print("MANUAL: glass -> "); Serial.print(v, 1); Serial.println(" %");
      rowCount = 999;
    } else Serial.println("#  duty must be 0-100");
  }
  else if (p[0] == 'n' || p[0] == 'N') {
    eventStamp(); Serial.print("NOTE: "); Serial.println(p + 1);
    rowCount = 999;
  }
  else if (p[0] == '?') { printHelp(); rowCount = 999; }
  else if (cmdLen > 0) Serial.println("#  unknown command, type ? for help");

  cmdLen = 0;
}

void readSerial(float ambNow) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (cmdLen > 0) handleCommand(ambNow); }
    else if (cmdLen < (int)sizeof(cmdBuf) - 1) cmdBuf[cmdLen++] = c;
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(HEATER_PIN, OUTPUT);
  pinMode(GLASS_PIN,  OUTPUT);
  pinMode(BASE_PIN,   OUTPUT);
  cham.duty = 0; glas.duty = 0; baseDuty = 0;
  applyOutputs();

  Serial.println();
  Serial.println("# ===== Incubator closed-loop control + open-loop heated base =====");
  Serial.println("#  chamber: G = 32.7/(775s+1), lambda=300s  -> Kp=7.90  Ki=1.02e-2  b=0.5");
  Serial.println("#  glass:   G = 38.7/(348s+1), lambda= 60s  -> Kp=14.99 Ki=4.31e-2  b=0.5");
  Serial.print  ("#  base:    OPEN LOOP, no sensor on the pane, max ");
  Serial.print(BASE_DUTY_MAX, 0); Serial.print(" %, backs off to ");
  Serial.print(BASE_FALLBACK, 0); Serial.print(" % above T_cham ");
  Serial.print(BASE_SOFT_T, 1); Serial.print(" C, cut above ");
  Serial.print(BASE_HARD_T, 1); Serial.println(" C");
  Serial.println("#           calibration: T_base ~= 26.4 + 0.456*duty  (chamber 39 C, amb ~19.5 C)");
  Serial.print  ("#  duty in PERCENT, Ts = 1 s, clamping anti-windup, trip at ");
  Serial.print(T_TRIP, 1); Serial.println(" C");
  Serial.print  ("#  default setpoints: chamber ");
  Serial.print(cham.sp, 1); Serial.print(" C, glass "); Serial.print(glas.sp, 1);
  Serial.println(" C");
  Serial.println("#  A1 not connected (thermistor failed)");
  Serial.println("#  SCD30 removed (module failed): no CO2, RH, dew point or fog margin");

  Wire.begin();
  if (!ads.begin(0x48)) {
    Serial.println("#  ADS1115 NOT FOUND - check I2C wiring");
    while (1) delay(1000);
  }
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.println("#  ADS1115 ok");

  rtd.begin(MAX31865_2WIRE);
  rtd.enable50Hz(true);
  rtd.setThresholds(0x1234, 0x5678);
  uint16_t lo = rtd.getLowerThreshold(), hi = rtd.getUpperThreshold();
  rtd.setThresholds(0, 0xFFFF);
  if (lo == 0x1234 && hi == 0x5678) {
    Serial.println("#  MAX31865 ok");
  } else {
    Serial.println("#  *** MAX31865 NOT TALKING - control will not start ***");
    Serial.print("#      wrote 0x1234/0x5678, read back 0x");
    Serial.print(lo, HEX); Serial.print("/0x"); Serial.println(hi, HEX);
  }

  float w0 = readThermistor(CH_WELL);
  if (isnan(w0)) Serial.println("#  *** well thermistor (A2) reads open or shorted - check wiring ***");
  else { Serial.print("#  well thermistor ok, reading "); Serial.print(w0, 2); Serial.println(" C"); }

  printHelp();
}

// ---------------------------------------------------------------------
void loop() {
  readSerial(lastAmb);

  if (millis() - lastLog < INTERVAL) return;
  lastLog += INTERVAL;
  if (millis() - lastLog > INTERVAL) lastLog = millis();

  // ---- read everything ONCE, so control and log always agree ----
  uint32_t rawSum = 0;
  uint8_t fault = 0;
  int good = 0;
  for (int i = 0; i < PT_SAMPLES; i++) {
    uint16_t r = rtd.readRTD();
    uint8_t f = rtd.readFault();
    if (f) { rtd.clearFault(); fault = f; }
    else   { rawSum += r; good++; }
  }
  float tPT = good ? rtd.calculateTemperature(rawSum / good, RNOMINAL, RREF) : NAN;
  if (good) fault = 0;

  float tTh[NUM_THERM];
  for (int i = 0; i < NUM_THERM; i++) tTh[i] = THERM_USED[i] ? readThermistor(i) : NAN;
  float tGlass = tTh[CH_GLASS];
  float tAmb   = tTh[CH_AMB];
  if (!isnan(tAmb)) lastAmb = tAmb;
  float margin = (isnan(tGlass) || isnan(tPT)) ? NAN : (tGlass - tPT);

  // ---- base heater guard, via the chamber air ----
  //  Soft limit: back the duty off slowly so the run continues instead of
  //  dying. Hard limit: cut outright. No PT1000: cut, because there is then
  //  no guard at all on an unmonitored heater.
  if (baseDuty > 0.0f) {
    if (isnan(tPT)) {
      baseDuty = 0.0f; baseTripped = true; baseBackingOff = false; baseGuardCount = 0;
      applyOutputs();
      logEvent("*** PT1000 LOST - base heater off (no guard available) ***");
    } else if (tPT > BASE_HARD_T) {
      baseDuty = 0.0f; baseTripped = true; baseBackingOff = false; baseGuardCount = 0;
      applyOutputs();
      logEvent("*** CHAMBER ABOVE HARD LIMIT - base heater off ***");
    } else if (tPT > BASE_SOFT_T && baseDuty > BASE_FALLBACK) {
      if (++baseGuardCount >= BASE_GUARD_N) {
        if (!baseBackingOff) {
          baseBackingOff = true;
          logEvent("*** chamber above soft limit - backing the base duty off ***");
        }
        baseDuty = max(BASE_FALLBACK, baseDuty - BASE_RAMP);
        applyOutputs();
        if (baseDuty <= BASE_FALLBACK) {
          baseBackingOff = false;
          logEvent("*** base duty held at the fallback value ***");
        }
      }
    } else {
      baseGuardCount = 0;
      if (baseBackingOff) {
        baseBackingOff = false;
        logEvent("chamber back under the soft limit, base duty held (not restored)");
      }
    }
  }

  // ---- safety: over-temperature on the controlled channels ----
  if ((!isnan(tPT)    && tPT    > T_TRIP) ||
      (!isnan(tGlass) && tGlass > T_TRIP)) {
    if (mode != MODE_TRIP) {
      mode = MODE_TRIP;
      allOff("*** OVER-TEMPERATURE TRIP - ALL OFF ***");
    }
  }

  // ---- control ----
  if (mode == MODE_PID) {
    // chamber loop
    if (isnan(tPT)) {
      if (++cham.bad >= BAD_LIMIT) {
        cham.duty = 0; cham.tripped = true;
        if (cham.bad == BAD_LIMIT) logEvent("*** PT1000 LOST - heaters off ***");
      }
    } else {
      if (cham.tripped) { cham.tripped = false; logEvent("PT1000 recovered, heaters resumed"); }
      cham.bad = 0;
      updateLoop(cham, tPT);
    }

    // glass loop
    if (isnan(tGlass)) {
      if (++glas.bad >= BAD_LIMIT) {
        glas.duty = 0; glas.tripped = true;
        if (glas.bad == BAD_LIMIT) logEvent("*** GLASS THERMISTOR LOST - glass heater off ***");
      }
    } else {
      if (glas.tripped) { glas.tripped = false; logEvent("Glass sensor recovered, glass heater resumed"); }
      glas.bad = 0;
      updateLoop(glas, tGlass);
    }

    applyOutputs();

    // ---- glass-chamber margin warning ----
    if (!isnan(margin) && margin < MARGIN_MIN) {
      if (++marginWarn == 10) logEvent("*** WARNING: glass-chamber margin below limit ***");
    } else {
      marginWarn = 0;
    }
  }

  // ---- log ----
  if (rowCount >= 20) { printHeader(); rowCount = 0; }
  rowCount++;

  unsigned long s = millis() / 1000;
  char clk[12], num[12];
  snprintf(clk, sizeof(clk), "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

  snprintf(num, sizeof(num), "%lu", s);  printPaddedStr(num, 6);
  printPaddedStr(clk, 10);
  printPaddedStr(modeStr(), 6);
  printPaddedDec(cham.duty, 7, 1);
  printPaddedDec(glas.duty, 7, 1);
  printPaddedDec(baseDuty,  7, 1);
  printPaddedDec(tPT, 9, 2);
  for (int i = 0; i < NUM_THERM; i++) if (THERM_USED[i]) printPaddedDec(tTh[i], 9, 2);
  printPaddedDec(margin, 7, 2);
  snprintf(num, sizeof(num), "%02X", fault); printPaddedStr(num, 5);
  Serial.println();

  if (fault != lastFault) {
    Serial.print("#  PT1000 fault changed to 0x"); Serial.println(fault, HEX);
    lastFault = fault;
  }
}
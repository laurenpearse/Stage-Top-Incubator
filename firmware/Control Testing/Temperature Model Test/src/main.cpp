#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31865.h>

// =====================================================================
//  CLOSED-LOOP INCUBATOR CONTROL
//
//  Chamber air  -> PT1000 (MAX31865)  -> wall heaters   -> setpoint 39.0 C
//  ITO glass    -> thermistor A0      -> glass heater   -> setpoint 48.0 C
//
//  PI with lambda tuning, setpoint weighting and clamping anti-windup.
//  Gains derived from the measured step-test models:
//      chamber   G = 32.7 / (775 s + 1)      lambda = 300 s
//      glass     G = 38.7 / (348 s + 1)      lambda =  60 s
// =====================================================================

// =====================================================================
//  SENSOR PLACEMENT
// =====================================================================
const char* PT_NAME = "T_cham";   // PT1000, chamber air centre, above plate lid

const char* THERM_NAME[4] = {
  "T_glass",      // A0 - taped to the ITO glass, near a corner   <-- GLASS CONTROL SENSOR
  "T_wellC",      // A1 - centre well, in the water, tip at the well bottom
  "T_wellE",      // A2 - edge well, in the water, tip at the well bottom
  "T_amb"         // A3 - OUTSIDE the chamber, room air
};
const int CH_GLASS = 0;    // index into THERM_NAME / ADS channel
const int CH_AMB   = 3;

// =====================================================================
//  OUTPUT PINS
// =====================================================================
const int HEATER_PIN = D4;   // PB5 - wall heaters
const int GLASS_PIN  = D8;   // PA9 - ITO glass  (move to D5 when CO2 is wired)

// =====================================================================
//  CONTROLLER PARAMETERS   (duty in PERCENT, 0 to 100)
// =====================================================================
//  chamber: Kp = tau/(K*lambda) = 775/(32.7*300) = 0.0790  -> 7.90 %/K
//           Ki = 1/(K*lambda)   = 1/(32.7*300)   = 1.02e-4 -> 1.02e-2 %/K/s
//  glass:   Kp = 348/(38.7*60)  = 0.1499                   -> 14.99 %/K
//           Ki = 1/(38.7*60)    = 4.31e-4                  -> 4.31e-2 %/K/s
const float KP_CHAM = 7.90f,  KI_CHAM = 1.02e-2f;
const float KP_GLAS = 14.99f, KI_GLAS = 4.31e-2f;
const float B_WEIGHT = 0.5f;          // setpoint weight, both loops

const float TS = 1.0f;                // control sample time, s  (matches log rate)

//  Static feedforward, from the inverse of the measured gain matrix
//      [ Tc-Ta ]   [ 0.510  0.165 ] [ uh ]
//      [ Tg-Ta ] = [ 0.428  0.387 ] [ ug ]     (K per % duty)
const float FF_H_C =  3.054f, FF_H_G = -1.302f;
const float FF_G_C = -3.377f, FF_G_G =  4.024f;

// =====================================================================
//  SAFETY
// =====================================================================
const float T_TRIP     = 55.0f;   // any control sensor above this -> everything off
const float SP_MAX     = 50.0f;   // highest setpoint the commands will accept
const int   BAD_LIMIT  = 5;       // consecutive bad reads before tripping a loop
const float MARGIN_MIN = 0.5f;    // warn if T_glass - T_cham drops below this

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
  float amb0;      // ambient latched when the loop started
  int   bad;
  bool  tripped;
};

Loop cham  = { "chamber", KP_CHAM, KI_CHAM, B_WEIGHT, 39.0f, 0, 0, 20.0f, 0, false };
Loop glas  = { "glass",   KP_GLAS, KI_GLAS, B_WEIGHT, 48.0f, 0, 0, 20.0f, 0, false };

char cmdBuf[48];
int  cmdLen = 0;
int  marginWarn = 0;
float lastAmb = NAN;        // most recent good ambient reading

// ---------------------------------------------------------------------
void applyOutputs() {
  int h = (int)(cham.duty * 255.0f / 100.0f + 0.5f);
  int g = (int)(glas.duty * 255.0f / 100.0f + 0.5f);
  analogWrite(HEATER_PIN, constrain(h, 0, 255));
  analogWrite(GLASS_PIN,  constrain(g, 0, 255));
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
  float e  = L.sp - y;                                        // integral sees full error
  float ep = L.b * (L.sp - L.amb0) - (y - L.amb0);            // P sees weighted setpoint
  float raw = L.kp * ep + L.integ + L.ki * e * TS;
  float u   = constrain(raw, 0.0f, 100.0f);
  if (u == raw) L.integ += L.ki * e * TS;                     // integrate only when not saturated
  L.duty = u;
}

//  Preload the integrators to the steady-state duty PLUS the offset the
//  negative proportional term creates at setpoint (because b < 1).
//  Without the second term a warm start clips at 0 and stalls.
void preloadIntegrators(float amb) {
  float dc = cham.sp - amb;
  float dg = glas.sp - amb;
  float uh = constrain(FF_H_C * dc + FF_H_G * dg, 0.0f, 100.0f);
  float ug = constrain(FF_G_C * dc + FF_G_G * dg, 0.0f, 100.0f);
  cham.integ = uh + cham.kp * (1.0f - cham.b) * dc;
  glas.integ = ug + glas.kp * (1.0f - glas.b) * dg;
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
  printPaddedStr(PT_NAME, 9);
  for (int i = 0; i < NUM_THERM; i++) printPaddedStr(THERM_NAME[i], 9);
  printPaddedStr("marg", 7);
  printPaddedStr("flt", 5);
  Serial.println();
  Serial.println("------------------------------------------------------------------------------------------------");
}

void printHelp() {
  Serial.println("#");
  Serial.println("#  COMMANDS");
  Serial.println("#    go       start closed-loop control (integrators preloaded from ambient)");
  Serial.println("#    off      everything off, loops reset");
  Serial.println("#    sc 39    set CHAMBER setpoint, degC   (max 50)");
  Serial.println("#    sg 48    set GLASS setpoint, degC     (max 50)");
  Serial.println("#    h25      MANUAL heater duty %  (leaves closed loop)");
  Serial.println("#    g20      MANUAL glass duty %   (leaves closed loop)");
  Serial.println("#    st       print setpoints and controller internals");
  Serial.println("#    n text   write a note into the log");
  Serial.println("#    ?        this help");
  Serial.println("#");
}

//  NOTE: every message that prints a number uses Serial.print, not snprintf.
//  snprintf("%f") prints nothing under newlib-nano, which is the STM32duino default.
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
  Serial.print("#  trip=");         Serial.print(T_TRIP, 1);
  Serial.print(" C   sp_max=");     Serial.print(SP_MAX, 1);
  Serial.println(" C");
  Serial.println("#");
  rowCount = 999;
}

void allOff(const char* why) {
  cham.duty = 0; glas.duty = 0;
  cham.integ = 0; glas.integ = 0;
  cham.bad = 0; glas.bad = 0;
  cham.tripped = false; glas.tripped = false;
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
    Serial.print("  g=");                       Serial.println(glas.integ, 1);
    rowCount = 999;
  }
  else if (strncmp(p, "off", 3) == 0) {
    mode = MODE_OFF;
    allOff("ALL OFF, loops reset");
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
  cham.duty = 0; glas.duty = 0;
  applyOutputs();

  Serial.println();
  Serial.println("# ===== Incubator closed-loop control =====");
  Serial.println("#  chamber: G = 32.7/(775s+1), lambda=300s  -> Kp=7.90  Ki=1.02e-2  b=0.5");
  Serial.println("#  glass:   G = 38.7/(348s+1), lambda= 60s  -> Kp=14.99 Ki=4.31e-2  b=0.5");
  Serial.print  ("#  duty in PERCENT, Ts = 1 s, clamping anti-windup, trip at ");
  Serial.print(T_TRIP, 1); Serial.println(" C");
  Serial.print  ("#  default setpoints: chamber ");
  Serial.print(cham.sp, 1); Serial.print(" C, glass "); Serial.print(glas.sp, 1);
  Serial.println(" C");

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

  printHelp();
}

// ---------------------------------------------------------------------
void loop() {
  readSerial(lastAmb);                 // commands are served between samples too

  if (millis() - lastLog < INTERVAL) return;
  lastLog += INTERVAL;
  if (millis() - lastLog > INTERVAL) lastLog = millis();   // never try to catch up

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
  for (int i = 0; i < NUM_THERM; i++) tTh[i] = readThermistor(i);
  float tGlass = tTh[CH_GLASS];
  float tAmb   = tTh[CH_AMB];
  if (!isnan(tAmb)) lastAmb = tAmb;
  float margin = (isnan(tGlass) || isnan(tPT)) ? NAN : (tGlass - tPT);

  // ---- safety: over-temperature ----
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
      // else: hold the previous duty, do not integrate
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

    // ---- condensation margin warning ----
    if (!isnan(margin) && margin < MARGIN_MIN) {
      if (++marginWarn == 10) logEvent("*** WARNING: glass-chamber margin below limit, condensation risk ***");
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
  printPaddedDec(tPT, 9, 2);
  for (int i = 0; i < NUM_THERM; i++) printPaddedDec(tTh[i], 9, 2);
  printPaddedDec(margin, 7, 2);
  snprintf(num, sizeof(num), "%02X", fault); printPaddedStr(num, 5);
  Serial.println();

  if (fault != lastFault) {
    Serial.print("#  PT1000 fault changed to 0x"); Serial.println(fault, HEX);
    lastFault = fault;
  }
}
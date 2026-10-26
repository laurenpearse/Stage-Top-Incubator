#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31865.h>

// =====================================================================
//  WELL EVENNESS TEST  -  closed-loop chamber + glass, open-loop heated base
//
//  Chamber air  -> PT1000 (MAX31865)  -> wall heaters  -> setpoint 39.0 C
//  ITO glass    -> thermistor A0      -> glass heater  -> setpoint 48.0 C
//  Base glass   -> NO SENSOR          -> base heater   -> fixed 45 % duty
//
//  Question: with the heated base on, how different are a near-centre well
//  and a corner well?  A2 = E8 (near centre), A3 = H12 (corner).
//
//  WHAT IS DIFFERENT FROM THE PREVIOUS BUILD
//    * A2 was the base glass probe. It is now well E8.
//    * A3 was ambient. It is now well H12. There is NO ambient measurement.
//    * The base heater therefore has NO temperature feedback of its own.
//      Its over-temperature cut-out now watches the CHAMBER PT1000 instead,
//      which is the only sensor that sees the heat the base is adding.
//    * The integrator preload needs an ambient figure. Set it with the "a"
//      command before "go", or it uses AMB_ASSUMED.
//    * A1 is the dead channel and is logged as n/c.
// =====================================================================

// =====================================================================
//  SENSOR PLACEMENT
// =====================================================================
const char* PT_NAME = "T_cham";   // PT1000, chamber air centre, above plate lid

const char* THERM_NAME[4] = {
  "T_glass",      // A0 - taped to the ITO glass lid   <-- GLASS CONTROL SENSOR
  "n/c",          // A1 - FAILED, not connected
  "T_E8",         // A2 - well E8,  near plate centre, tip at the well bottom
  "T_H12"         // A3 - well H12, plate corner,      tip at the well bottom
};
const int CH_GLASS = 0;
const int CH_E8    = 2;
const int CH_H12   = 3;

// =====================================================================
//  OUTPUT PINS
// =====================================================================
const int HEATER_PIN = D4;   // PB5  - wall heaters
const int GLASS_PIN  = D8;   // PA9  - ITO glass lid
const int BASE_PIN   = D6;   // PB10 - ITO glass base, under the aperture

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
//  SETPOINTS FOR THIS TEST
// =====================================================================
const float SP_CHAM_DEFAULT = 39.0f;
const float SP_GLAS_DEFAULT = 48.0f;

// =====================================================================
//  BASE HEATER
// =====================================================================
//  Fixed duty for this test. The calibration from the earlier run,
//  T_base = 26.4 + 0.456 * duty, puts 45 % at roughly 47 C on the pane.
const float BASE_DUTY_SET = 45.0f;
const float BASE_DUTY_MAX = 60.0f;

//  The base pane has no thermistor in this configuration, so the cut-out
//  watches the chamber instead. 44 C is 5 C above the chamber setpoint and
//  well below the 55 C global trip.
const float BASE_CUT_CHAM = 44.0f;

// =====================================================================
//  AMBIENT
// =====================================================================
//  There is no ambient sensor. This value is only used to preload the
//  integrators at "go". Set it with "a21.5" before starting if you know the
//  room temperature; an error of a degree or two is harmless.
float ambAssumed = 20.0f;

// =====================================================================
//  SAFETY
// =====================================================================
const float T_TRIP     = 55.0f;
const float SP_MAX     = 50.0f;
const int   BAD_LIMIT  = 5;
//  A single dropped PT1000 sample is not evidence of anything. The base is
//  only cut after this many CONSECUTIVE failed reads.
const int   PT_BAD_BASE = 15;
const float MARGIN_MIN = 0.5f;

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
Adafruit_MAX31865 rtd = Adafruit_MAX31865(10, 11, 12, 13);
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

Loop cham = { "chamber", KP_CHAM, KI_CHAM, B_WEIGHT, SP_CHAM_DEFAULT, 0, 0, 20.0f, 0, false };
Loop glas = { "glass",   KP_GLAS, KI_GLAS, B_WEIGHT, SP_GLAS_DEFAULT, 0, 0, 20.0f, 0, false };

float baseDuty    = 0.0f;
bool  baseTripped = false;
int   ptBad       = 0;     // consecutive bad PT1000 reads

char cmdBuf[48];
int  cmdLen = 0;
int  marginWarn = 0;

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
  for (int i = 0; i < NUM_THERM; i++) printPaddedStr(THERM_NAME[i], 9);
  printPaddedStr("dWell", 8);     // T_E8 - T_H12, the number this test is for
  printPaddedStr("marg", 7);
  printPaddedStr("flt", 5);
  Serial.println();
  Serial.println("-------------------------------------------------------------------------------------------------------------------");
}

void printHelp() {
  Serial.println("#");
  Serial.println("#  WELL EVENNESS TEST.  A2 = well E8 (centre), A3 = well H12 (corner).");
  Serial.println("#  No ambient sensor and no base glass sensor in this configuration.");
  Serial.println("#");
  Serial.println("#  COMMANDS");
  Serial.println("#    a21.5    tell the firmware the room temperature, for the preload only");
  Serial.println("#    go       start closed-loop control");
  Serial.println("#    b        BASE heater -> 45 % (the duty this test uses)");
  Serial.println("#    b0       BASE heater off");
  Serial.println("#    b30      BASE heater -> any duty up to 60 %");
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
  Serial.print(" integ=");          Serial.print(cham.integ, 2);
  Serial.print(" duty=");           Serial.print(cham.duty, 1);
  Serial.print(" amb0=");           Serial.print(cham.amb0, 2);
  Serial.print(" bad=");            Serial.println(cham.bad);
  Serial.print("#  glass:   sp=");  Serial.print(glas.sp, 2);
  Serial.print(" integ=");          Serial.print(glas.integ, 2);
  Serial.print(" duty=");           Serial.print(glas.duty, 1);
  Serial.print(" amb0=");           Serial.print(glas.amb0, 2);
  Serial.print(" bad=");            Serial.println(glas.bad);
  Serial.print("#  base:    duty="); Serial.print(baseDuty, 1);
  Serial.print(" % (open loop, no sensor)  tripped=");
  Serial.print(baseTripped ? "yes" : "no");
  Serial.print("  ptBad="); Serial.println(ptBad);
  Serial.print("#  assumed ambient="); Serial.print(ambAssumed, 2); Serial.println(" C");
  Serial.print("#  trip=");         Serial.print(T_TRIP, 1);
  Serial.print(" C   base cut-out on chamber="); Serial.print(BASE_CUT_CHAM, 1);
  Serial.println(" C");
  Serial.println("#");
  rowCount = 999;
}

void allOff(const char* why) {
  cham.duty = 0; glas.duty = 0; baseDuty = 0;
  cham.integ = 0; glas.integ = 0;
  cham.bad = 0; glas.bad = 0;
  cham.tripped = false; glas.tripped = false; baseTripped = false;
  applyOutputs();
  logEvent(why);
}

// ---------------------------------------------------------------------
void handleCommand() {
  cmdBuf[cmdLen] = '\0';
  char* p = cmdBuf;
  while (*p == ' ') p++;

  if (strncmp(p, "go", 2) == 0) {
    preloadIntegrators(ambAssumed);
    cham.bad = 0; glas.bad = 0;
    cham.tripped = false; glas.tripped = false;
    mode = MODE_PID;
    eventStamp();
    Serial.print("CLOSED LOOP ON   sp_cham=");  Serial.print(cham.sp, 1);
    Serial.print("  sp_glass=");                Serial.print(glas.sp, 1);
    Serial.print("  amb_assumed=");             Serial.print(ambAssumed, 2);
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
  else if (p[0] == 'a' || p[0] == 'A') {
    float v = atof(p + 1);
    if (v > 0.0f && v < 40.0f) {
      ambAssumed = v;
      eventStamp();
      Serial.print("ASSUMED AMBIENT -> "); Serial.print(v, 2); Serial.println(" C");
      rowCount = 999;
    } else {
      Serial.println("#  ambient must be between 0 and 40");
    }
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
    //  bare "b" means the duty this test is specified at
    float v = (p[1] == '\0') ? BASE_DUTY_SET : atof(p + 1);
    if (v >= 0 && v <= BASE_DUTY_MAX) {
      baseDuty = v;
      baseTripped = false;
      applyOutputs();
      eventStamp();
      Serial.print("BASE GLASS -> "); Serial.print(v, 1);
      Serial.println(" % (open loop, no base sensor)");
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

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (cmdLen > 0) handleCommand(); }
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
  Serial.println("# ===== WELL EVENNESS TEST: E8 vs H12, heated base at 45 % =====");
  Serial.println("#  A0 = glass (control)   A1 = n/c   A2 = well E8   A3 = well H12");
  Serial.println("#  NO ambient sensor, NO base glass sensor.");
  Serial.print  ("#  setpoints: chamber "); Serial.print(cham.sp, 1);
  Serial.print  (" C, glass ");             Serial.print(glas.sp, 1);
  Serial.println(" C");
  Serial.print  ("#  base heater runs OPEN LOOP at "); Serial.print(BASE_DUTY_SET, 0);
  Serial.println(" % when you type b.");
  Serial.print  ("#  base cut-out: chamber above "); Serial.print(BASE_CUT_CHAM, 1);
  Serial.print  (" C, or PT1000 lost for "); Serial.print(PT_BAD_BASE);
  Serial.println(" consecutive samples -> base off.");
  Serial.print  ("#  global trip at "); Serial.print(T_TRIP, 1); Serial.println(" C");

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
  }

  float e0 = readThermistor(CH_E8);
  float h0 = readThermistor(CH_H12);
  if (isnan(e0)) Serial.println("#  *** well E8 probe (A2) reads open or shorted ***");
  else { Serial.print("#  well E8  (A2) ok, reading "); Serial.print(e0, 2); Serial.println(" C"); }
  if (isnan(h0)) Serial.println("#  *** well H12 probe (A3) reads open or shorted ***");
  else { Serial.print("#  well H12 (A3) ok, reading "); Serial.print(h0, 2); Serial.println(" C"); }
  if (!isnan(e0) && !isnan(h0)) {
    Serial.print("#  starting difference E8 - H12 = ");
    Serial.print(e0 - h0, 2); Serial.println(" C  (both should be at room temperature now)");
  }

  printHelp();
}

// ---------------------------------------------------------------------
void loop() {
  readSerial();

  if (millis() - lastLog < INTERVAL) return;
  lastLog += INTERVAL;
  if (millis() - lastLog > INTERVAL) lastLog = millis();

  // ---- read everything ONCE ----
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
  if (isnan(tPT)) ptBad++; else ptBad = 0;

  float tTh[NUM_THERM];
  for (int i = 0; i < NUM_THERM; i++) tTh[i] = readThermistor(i);
  float tGlass = tTh[CH_GLASS];
  float tE8    = tTh[CH_E8];
  float tH12   = tTh[CH_H12];
  float dWell  = (isnan(tE8) || isnan(tH12)) ? NAN : (tE8 - tH12);
  float margin = (isnan(tGlass) || isnan(tPT)) ? NAN : (tGlass - tPT);

  // ---- safety: base heater cut-out, watching the CHAMBER ----
  //  The base pane has no sensor of its own in this configuration, so the
  //  chamber is the only evidence that the base is overheating things. A single
  //  dropped PT1000 sample is ignored; only a sustained loss cuts the base.
  if (baseDuty > 0.0f) {
    if (ptBad >= PT_BAD_BASE) {
      baseDuty = 0.0f; baseTripped = true; applyOutputs();
      logEvent("*** PT1000 LOST FOR 15 s - base heater off, no guard available ***");
    } else if (!isnan(tPT) && tPT > BASE_CUT_CHAM) {
      baseDuty = 0.0f; baseTripped = true; applyOutputs();
      logEvent("*** CHAMBER ABOVE BASE CUT-OUT - base heater off ***");
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
  printPaddedDec(baseDuty,  7, 1);
  printPaddedDec(tPT, 9, 2);
  for (int i = 0; i < NUM_THERM; i++) printPaddedDec(tTh[i], 9, 2);
  printPaddedDec(dWell, 8, 2);
  printPaddedDec(margin, 7, 2);
  snprintf(num, sizeof(num), "%02X", fault); printPaddedStr(num, 5);
  Serial.println();

  if (fault != lastFault) {
    Serial.print("#  PT1000 fault changed to 0x"); Serial.println(fault, HEX);
    lastFault = fault;
  }
}
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_BMP280.h>
#include <IWatchdog.h>

// =====================================================================
//  24 HOUR HUMIDITY AND STABILITY RUN
//
//  Chamber air  -> PT1000 (MAX31865)  -> wall heaters  -> setpoint 39.0 C
//  ITO glass    -> thermistor A3      -> glass heater  -> setpoint 48.0 C
//  Base glass   -> NO SENSOR          -> base heater   -> fixed 30 % duty
//  Well E8      -> thermistor A2      -> logged only
//  Room air     -> BMP280             -> logged only
//  Chamber RH   -> DHT22 on PC0       -> logged only
//
//  A0 and A1 thermistors are dead. Glass control is on A3.
//  Humidity is passive: the water in the wells is the only source.
// =====================================================================

// =====================================================================
//  THERMISTOR CHANNELS (on the ADS1115)
// =====================================================================
const int CH_E8    = 2;           // A2 - well E8, tip at the well bottom
const int CH_GLASS = 3;           // A3 - taped to the ITO glass lid

// =====================================================================
//  OUTPUT PINS
// =====================================================================
const int HEATER_PIN = D4;   // PB5  - wall heaters
const int GLASS_PIN  = D8;   // PA9  - ITO glass lid
const int BASE_PIN   = D6;   // PB10 - ITO glass base

// =====================================================================
//  DHT22
// =====================================================================
//  pin 1 VDD -> 3V3,  pin 2 DATA -> A5 (PC0),  pin 3 NC,  pin 4 GND
//  10 kOhm resistor between DATA and 3V3.
#define DHT_PIN PC0
const uint32_t DHT_EVERY_TICKS = 3;     // datasheet minimum is 2 s

float    g_rh      = NAN;
float    g_tdht    = NAN;
uint32_t g_dhtFail = 0;

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
//  SETPOINTS
// =====================================================================
const float SP_CHAM_DEFAULT = 39.0f;
const float SP_GLAS_DEFAULT = 48.0f;

// =====================================================================
//  BASE HEATER
// =====================================================================
const float BASE_DUTY_SET = 0.0f;    // bare "b" uses this
const float BASE_DUTY_MAX = 60.0f;
const float BASE_CUT_CHAM = 44.0f;    // base cut-out, watching the chamber

// =====================================================================
//  SAFETY
// =====================================================================
const float T_TRIP      = 55.0f;
const float SP_MAX      = 50.0f;
const int   BAD_LIMIT   = 5;
const int   PT_BAD_BASE = 15;
const float MARGIN_MIN  = 0.5f;    // glass must stay this far above the air

// =====================================================================
//  THERMISTOR CONSTANTS (TE GA10K3MCD1)
// =====================================================================
const float R0 = 10000.0, T0 = 298.15, BETA = 3976.0;
const float V_SUPPLY = 3.29;

float R_FIXED[4]  = { 9840.0, 9980.0, 9890.0, 9940.0 };
float T_OFFSET[4] = { -0.06, +0.51, -0.25, -0.23 };

const int SAMPLES = 16;

// =====================================================================
Adafruit_ADS1115 ads;
Adafruit_MAX31865 rtd = Adafruit_MAX31865(10, 11, 12, 13);
Adafruit_BMP280 bmp;
bool  bmpOK = false;
const float BMP_T_OFFSET = 0.0f;    // trim if it disagrees when everything is cold
//  The BMP280 shares the I2C bus and sits on a lead, so its reads are the
//  most likely thing to glitch. Anything outside this band is rejected.
const float AMB_MIN = 0.0f, AMB_MAX = 60.0f;
const uint32_t AMB_EVERY_TICKS = 10;   // ambient moves slowly, read it rarely
float g_amb = NAN;
uint32_t g_ambBad = 0;

#define RREF      4300.0
#define RNOMINAL  1000.0
const int PT_SAMPLES = 2;

const unsigned long INTERVAL = 1000;    // CONTROL rate, do not change (Ki assumes 1 s)
const uint32_t LOG_EVERY = 5;           // log one row every 5 control ticks
const int HEADER_EVERY = 720;           // reprint the header every hour of logged rows

unsigned long lastLog = 0;
uint32_t tickCount = 0;
int rowCount = 0;
uint8_t lastFault = 0;

//  Only used if the BMP280 is missing.
float ambAssumed = 20.0f;

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
int   ptBad       = 0;

char cmdBuf[48];
int  cmdLen = 0;
int  marginWarn = 0;

// ---------------------------------------------------------------------
//  DHT22 DRIVER
// ---------------------------------------------------------------------
static bool dhtWaitWhile(int level, uint32_t timeout_us, uint32_t *elapsed)
{
  uint32_t t0 = micros();
  while (digitalRead(DHT_PIN) == level) {
    if ((micros() - t0) > timeout_us) return false;
  }
  if (elapsed) *elapsed = micros() - t0;
  return true;
}

static bool dht22Once(float *rh, float *tc)
{
  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(2);
  pinMode(DHT_PIN, INPUT_PULLUP);

  if (!dhtWaitWhile(HIGH, 100, nullptr)) return false;
  if (!dhtWaitWhile(LOW,  150, nullptr)) return false;
  if (!dhtWaitWhile(HIGH, 150, nullptr)) return false;

  for (int i = 0; i < 40; i++) {
    uint32_t highUs;
    if (!dhtWaitWhile(LOW,  100, nullptr)) return false;
    if (!dhtWaitWhile(HIGH, 150, &highUs)) return false;
    data[i / 8] <<= 1;
    if (highUs > 45) data[i / 8] |= 1;
  }

  if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) return false;

  *rh = (((uint16_t)data[0] << 8) | data[1]) * 0.1f;
  int16_t raw = (((uint16_t)(data[2] & 0x7F)) << 8) | data[3];
  *tc = (data[2] & 0x80) ? -raw * 0.1f : raw * 0.1f;
  return true;
}

bool dht22Read(float *rh, float *tc)
{
  for (int a = 0; a < 3; a++) {
    if (dht22Once(rh, tc)) return true;
    delay(5);
  }
  return false;
}

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

//  Raw read with a sanity check. A failed I2C transaction makes the library
//  return a wild number, so anything implausible is thrown away.
float readAmbientRaw() {
  if (!bmpOK) return NAN;
  float v = bmp.readTemperature() + BMP_T_OFFSET;
  if (isnan(v) || v < AMB_MIN || v > AMB_MAX) { g_ambBad++; return NAN; }
  return v;
}

//  Last good value, so one glitched read does not blank the column.
float readAmbient() { return g_amb; }

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
  printPaddedStr("t_s", 7);
  printPaddedStr("clock", 10);
  printPaddedStr("mode", 6);
  printPaddedStr("heat%", 7);
  printPaddedStr("glass%", 7);
  printPaddedStr("base%", 7);
  printPaddedStr("T_cham", 9);
  printPaddedStr("T_glass", 9);
  printPaddedStr("T_E8", 9);
  printPaddedStr("T_amb", 9);
  printPaddedStr("RH%", 8);
  printPaddedStr("T_dht", 9);
  printPaddedStr("flt", 5);
  Serial.println();
  Serial.println("------------------------------------------------------------------------------------------------");
}

void printHelp() {
  Serial.println("#");
  Serial.println("#  COMMANDS");
  Serial.println("#    go       start closed-loop control");
  Serial.println("#    b        BASE heater -> 30 %");
  Serial.println("#    b0       BASE heater off");
  Serial.println("#    b35      BASE heater -> any duty up to 60 %");
  Serial.println("#    off      everything off, loops reset");
  Serial.println("#    sc 39    set CHAMBER setpoint, degC   (max 50)");
  Serial.println("#    sg 48    set GLASS setpoint, degC     (max 50)");
  Serial.println("#    h25      MANUAL heater duty %  (leaves closed loop)");
  Serial.println("#    g20      MANUAL glass duty %   (leaves closed loop)");
  Serial.println("#    a21.5    fallback room temperature, only if the BMP280 is missing");
  Serial.println("#    st       controller internals");
  Serial.println("#    n text   write a note into the log");
  Serial.println("#    ?        this help");
  Serial.println("#");
  Serial.println("#  COLUMNS");
  Serial.println("#    T_cham   chamber air, PT1000        T_glass  ITO lid, A3  (control)");
  Serial.println("#    T_E8     well E8, A2                T_amb    room, BMP280");
  Serial.println("#    RH%      chamber humidity, DHT22    T_dht    DHT22 air temperature");
  Serial.println("#    flt      PT1000 fault code, 00 is healthy");
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
  rowCount = HEADER_EVERY;
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
  Serial.print(" %  tripped=");      Serial.print(baseTripped ? "yes" : "no");
  Serial.print("  ptBad=");          Serial.println(ptBad);
  Serial.print("#  dht22 failed reads: "); Serial.print(g_dhtFail);
  Serial.print("   bmp280 rejected reads: "); Serial.println(g_ambBad);
  Serial.print("#  ambient: BMP280=");
  if (bmpOK) Serial.print(readAmbient(), 2); else Serial.print("---");
  Serial.print(" C   typed=");       Serial.print(ambAssumed, 2);
  Serial.print(" C   frozen at go: "); Serial.print(cham.amb0, 2); Serial.println(" C");
  Serial.println("#");
  rowCount = HEADER_EVERY;
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
    float ta = readAmbient();
    bool  measured = !isnan(ta);
    float ambUse = measured ? ta : ambAssumed;
    preloadIntegrators(ambUse);
    cham.bad = 0; glas.bad = 0;
    cham.tripped = false; glas.tripped = false;
    mode = MODE_PID;
    eventStamp();
    Serial.print("CLOSED LOOP ON   sp_cham="); Serial.print(cham.sp, 1);
    Serial.print("  sp_glass=");               Serial.print(glas.sp, 1);
    Serial.print(measured ? "  amb_measured=" : "  amb_TYPED=");
    Serial.print(ambUse, 2);
    Serial.print("  preload h=");              Serial.print(cham.integ, 1);
    Serial.print("  g=");                      Serial.print(glas.integ, 1);
    Serial.print("  base duty=");              Serial.println(baseDuty, 1);
    rowCount = HEADER_EVERY;
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
      Serial.print("FALLBACK AMBIENT -> "); Serial.print(v, 2); Serial.println(" C");
      rowCount = HEADER_EVERY;
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
      rowCount = HEADER_EVERY;
    } else {
      Serial.print("#  setpoint must be between 10 and "); Serial.println(SP_MAX, 1);
    }
  }
  else if (p[0] == 'b' || p[0] == 'B') {
    float v = (p[1] == '\0') ? BASE_DUTY_SET : atof(p + 1);
    if (v >= 0 && v <= BASE_DUTY_MAX) {
      baseDuty = v;
      baseTripped = false;
      applyOutputs();
      eventStamp();
      Serial.print("BASE GLASS -> "); Serial.print(v, 1); Serial.println(" %");
      rowCount = HEADER_EVERY;
    } else {
      Serial.print("#  base duty must be 0-"); Serial.println(BASE_DUTY_MAX, 0);
    }
  }
  else if (p[0] == 'h' || p[0] == 'H') {
    float v = atof(p + 1);
    if (v >= 0 && v <= 100) {
      mode = MODE_MAN; cham.duty = v; cham.integ = 0; applyOutputs();
      eventStamp(); Serial.print("MANUAL: heaters -> "); Serial.print(v, 1); Serial.println(" %");
      rowCount = HEADER_EVERY;
    } else Serial.println("#  duty must be 0-100");
  }
  else if (p[0] == 'g' || p[0] == 'G') {
    float v = atof(p + 1);
    if (v >= 0 && v <= 100) {
      mode = MODE_MAN; glas.duty = v; glas.integ = 0; applyOutputs();
      eventStamp(); Serial.print("MANUAL: glass -> "); Serial.print(v, 1); Serial.println(" %");
      rowCount = HEADER_EVERY;
    } else Serial.println("#  duty must be 0-100");
  }
  else if (p[0] == 'n' || p[0] == 'N') {
    eventStamp(); Serial.print("NOTE: "); Serial.println(p + 1);
    rowCount = HEADER_EVERY;
  }
  else if (p[0] == '?') { printHelp(); rowCount = HEADER_EVERY; }
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

  //  If the previous run died inside a hung I2C transaction, say so and
  //  pick the control back up instead of sitting there with the heaters
  //  wherever they were left.
  bool wdogReset = IWatchdog.isReset(true);

  pinMode(HEATER_PIN, OUTPUT);
  pinMode(GLASS_PIN,  OUTPUT);
  pinMode(BASE_PIN,   OUTPUT);
  pinMode(DHT_PIN,    INPUT_PULLUP);
  cham.duty = 0; glas.duty = 0; baseDuty = 0;
  applyOutputs();

  Serial.println();
  Serial.println("# ===== 24 HOUR HUMIDITY AND STABILITY RUN =====");
  Serial.print  ("#  setpoints: chamber "); Serial.print(cham.sp, 1);
  Serial.print  (" C, glass ");             Serial.print(glas.sp, 1);
  Serial.println(" C");
  Serial.print  ("#  base heater open loop at "); Serial.print(BASE_DUTY_SET, 0);
  Serial.println(" % when you type b");
  Serial.print  ("#  base cut-out: chamber above "); Serial.print(BASE_CUT_CHAM, 1);
  Serial.print  (" C.   global trip: "); Serial.print(T_TRIP, 1); Serial.println(" C");

  Wire.begin();
  //  50 kHz instead of the default. The BMP280 is on a long lead next to the
  //  heater wiring, and a slower bus tolerates the extra capacitance and noise.
  Wire.setClock(50000);

  if (!ads.begin(0x48)) {
    Serial.println("#  ADS1115 NOT FOUND - check I2C wiring");
    while (1) delay(1000);
  }
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.println("#  ADS1115 ok");

  if      (bmp.begin(0x76)) { bmpOK = true; Serial.println("#  BMP280 ok at 0x76"); }
  else if (bmp.begin(0x77)) { bmpOK = true; Serial.println("#  BMP280 ok at 0x77"); }
  else    Serial.println("#  *** BMP280 NOT FOUND - ambient will read --- , type a<room temp> ***");
  if (bmpOK) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::FILTER_X4,
                    Adafruit_BMP280::STANDBY_MS_1000);
    delay(200);
    g_amb = readAmbientRaw();
    Serial.print("#  ambient reads "); Serial.print(g_amb, 2); Serial.println(" C");
  }

  rtd.begin(MAX31865_2WIRE);
  rtd.enable50Hz(true);
  rtd.setThresholds(0x1234, 0x5678);
  uint16_t lo = rtd.getLowerThreshold(), hi = rtd.getUpperThreshold();
  rtd.setThresholds(0, 0xFFFF);
  if (lo == 0x1234 && hi == 0x5678) Serial.println("#  MAX31865 ok");
  else Serial.println("#  *** MAX31865 NOT TALKING - control will not start ***");

  delay(1500);
  float rh0, t0d;
  if (dht22Read(&rh0, &t0d)) {
    g_rh = rh0; g_tdht = t0d;
    Serial.print("#  DHT22 ok, "); Serial.print(rh0, 1);
    Serial.print(" % RH at ");     Serial.print(t0d, 1); Serial.println(" C");
  } else {
    Serial.println("#  *** DHT22 NOT RESPONDING - check the 10k pull-up and PC0 ***");
  }

  float g0 = readThermistor(CH_GLASS);
  float e0 = readThermistor(CH_E8);
  if (isnan(g0)) Serial.println("#  *** GLASS probe (A3) open or shorted - CONTROL WILL NOT WORK ***");
  else { Serial.print("#  glass  (A3) ok, reading "); Serial.print(g0, 2); Serial.println(" C"); }
  if (isnan(e0)) Serial.println("#  *** well E8 probe (A2) open or shorted ***");
  else { Serial.print("#  E8     (A2) ok, reading "); Serial.print(e0, 2); Serial.println(" C"); }

  printHelp();

  //  8 second watchdog. If anything hangs, the board resets, which puts
  //  every heater back to zero. A reset is a much better failure than a
  //  hang with the heaters still on.
  IWatchdog.begin(8000000);

  if (wdogReset) {
    logEvent("*** WATCHDOG RESET - something hung. Resuming control. ***");
    baseDuty = BASE_DUTY_SET;
    float ta = readAmbientRaw();
    preloadIntegrators(isnan(ta) ? ambAssumed : ta);
    mode = MODE_PID;
    applyOutputs();
  }

  rowCount = HEADER_EVERY;        // header prints on the first logged row
}

// ---------------------------------------------------------------------
void loop() {
  IWatchdog.reload();
  readSerial();

  if (millis() - lastLog < INTERVAL) return;
  lastLog += INTERVAL;
  if (millis() - lastLog > INTERVAL) lastLog = millis();
  tickCount++;

  // ---- PT1000 ----
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

  // ---- thermistors and ambient ----
  float tGlass = readThermistor(CH_GLASS);
  float tE8    = readThermistor(CH_E8);
  float tAmb   = readAmbient();
  float margin = (isnan(tGlass) || isnan(tPT)) ? NAN : (tGlass - tPT);

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
      if (++marginWarn == 10) logEvent("*** WARNING: glass barely above the air, condensation risk ***");
    } else {
      marginWarn = 0;
    }
  }

  // ---- BMP280, every 10 ticks, after the controllers ----
  if ((tickCount % AMB_EVERY_TICKS) == 0) {
    float a = readAmbientRaw();
    if (!isnan(a)) g_amb = a;          // keep the last good value on a glitch
  }

  // ---- DHT22, after the controllers ----
  if ((tickCount % DHT_EVERY_TICKS) == 0) {
    float rh, tc;
    if (dht22Read(&rh, &tc)) { g_rh = rh; g_tdht = tc; }
    else { g_rh = NAN; g_tdht = NAN; g_dhtFail++; }
  }

  // ---- base heater cut-out, watching the chamber ----
  if (baseDuty > 0.0f) {
    if (ptBad >= PT_BAD_BASE) {
      baseDuty = 0.0f; baseTripped = true; applyOutputs();
      logEvent("*** PT1000 LOST FOR 15 s - base heater off ***");
    } else if (!isnan(tPT) && tPT > BASE_CUT_CHAM) {
      baseDuty = 0.0f; baseTripped = true; applyOutputs();
      logEvent("*** CHAMBER ABOVE BASE CUT-OUT - base heater off ***");
    }
  }

  // ---- global over-temperature trip ----
  if ((!isnan(tPT)    && tPT    > T_TRIP) ||
      (!isnan(tGlass) && tGlass > T_TRIP)) {
    if (mode != MODE_TRIP) {
      mode = MODE_TRIP;
      allOff("*** OVER-TEMPERATURE TRIP - ALL OFF ***");
    }
  }

  // ---- log, every LOG_EVERY ticks ----
  //  Control still runs at 1 Hz. Logging at 5 s keeps the file and the
  //  terminal manageable over 24 hours, and the thermal time constants are
  //  minutes, so nothing is lost.
  if ((tickCount % LOG_EVERY) != 0) return;

  if (rowCount >= HEADER_EVERY) { printHeader(); rowCount = 0; }
  rowCount++;

  unsigned long s = millis() / 1000;
  char clk[12], num[12];
  snprintf(clk, sizeof(clk), "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

  snprintf(num, sizeof(num), "%lu", s);  printPaddedStr(num, 7);
  printPaddedStr(clk, 10);
  printPaddedStr(modeStr(), 6);
  printPaddedDec(cham.duty, 7, 1);
  printPaddedDec(glas.duty, 7, 1);
  printPaddedDec(baseDuty,  7, 1);
  printPaddedDec(tPT,    9, 2);
  printPaddedDec(tGlass, 9, 2);
  printPaddedDec(tE8,    9, 2);
  printPaddedDec(tAmb,   9, 2);
  printPaddedDec(g_rh,   8, 1);
  printPaddedDec(g_tdht, 9, 2);
  snprintf(num, sizeof(num), "%02X", fault); printPaddedStr(num, 5);
  Serial.println();

  if (fault != lastFault) {
    Serial.print("#  PT1000 fault changed to 0x"); Serial.println(fault, HEX);
    lastFault = fault;
  }
}
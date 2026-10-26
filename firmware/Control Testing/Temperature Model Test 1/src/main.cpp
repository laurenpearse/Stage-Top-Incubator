#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31865.h>

// =====================================================================
//  SENSOR PLACEMENT
// =====================================================================
//  PT1000 (MAX31865) -> CHAMBER AIR, centre, above the plate lid
const char* PT_NAME = "T_cham";

const char* THERM_NAME[4] = {
  "T_glass",      // A0 - taped to the ITO glass, near a corner
  "T_well",       // A1 - in a well, in the water
  "T_side",       // A2 - chamber air, side wall (future control sensor spot)
  "T_amb"         // A3 - OUTSIDE the chamber, room air
};

// =====================================================================
//  OUTPUT PINS
// =====================================================================
const int HEATER_PIN = D4;   // PB5 - wall heaters
const int GLASS_PIN  = D8;   // PA9 - ITO glass  (move to D5 when CO2 is wired)

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

const unsigned long INTERVAL = 1000;
unsigned long lastLog = 0;
int rowCount = 0;
uint8_t lastFault = 0;

int heaterPct = 0;
int glassPct  = 0;

char cmdBuf[40];
int  cmdLen = 0;

// ---------------------------------------------------------------------
void applyOutputs() {
  analogWrite(HEATER_PIN, (heaterPct * 255) / 100);
  analogWrite(GLASS_PIN,  (glassPct  * 255) / 100);
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

// right-align a float (or "---") in a field of given width
void printPadded(float v, int width) {
  char buf[16];
  if (isnan(v)) {
    strcpy(buf, "---");
  } else {
    bool neg = v < 0;
    float a = neg ? -v : v;
    long whole = (long)a;
    long frac  = (long)((a - whole) * 100.0 + 0.5);
    if (frac >= 100) { frac -= 100; whole += 1; }
    snprintf(buf, sizeof(buf), "%s%ld.%02ld", neg ? "-" : "", whole, frac);
  }
  for (int i = strlen(buf); i < width; i++) Serial.print(' ');
  Serial.print(buf);
}

void printPaddedStr(const char* s, int width) {
  for (int i = strlen(s); i < width; i++) Serial.print(' ');
  Serial.print(s);
}

void printHeader() {
  Serial.println();
  printPaddedStr("t_s", 6);
  printPaddedStr("clock", 10);
  printPaddedStr("heat%", 7);
  printPaddedStr("glass%", 7);
  printPaddedStr(PT_NAME, 9);
  for (int i = 0; i < NUM_THERM; i++) printPaddedStr(THERM_NAME[i], 9);
  printPaddedStr("flt", 5);
  Serial.println();
  Serial.println("--------------------------------------------------------------------------");
}

void printHelp() {
  Serial.println("#");
  Serial.println("#  COMMANDS  (type into the serial monitor, then press ENTER)");
  Serial.println("#    h10     wall heaters to 10% duty");
  Serial.println("#    h0      wall heaters off");
  Serial.println("#    g15     glass to 15% duty");
  Serial.println("#    off     everything off");
  Serial.println("#    n text  write a note into the log");
  Serial.println("#    ?       show this help again");
  Serial.println("#");
}

void logEvent(const char* what) {
  Serial.print("#  >>> t=");
  Serial.print(millis() / 1000);
  Serial.print("s   ");
  Serial.println(what);
  rowCount = 999;              // force a fresh header after an event
}

void handleCommand() {
  cmdBuf[cmdLen] = '\0';
  char* p = cmdBuf;
  while (*p == ' ') p++;
  char msg[60];

  if (p[0] == 'h' || p[0] == 'H') {
    int v = atoi(p + 1);
    if (v >= 0 && v <= 100) {
      heaterPct = v; applyOutputs();
      snprintf(msg, sizeof(msg), "HEATERS set to %d%%", heaterPct); logEvent(msg);
    } else Serial.println("#  duty must be 0-100");
  }
  else if (p[0] == 'g' || p[0] == 'G') {
    int v = atoi(p + 1);
    if (v >= 0 && v <= 100) {
      glassPct = v; applyOutputs();
      snprintf(msg, sizeof(msg), "GLASS set to %d%%", glassPct); logEvent(msg);
    } else Serial.println("#  duty must be 0-100");
  }
  else if (strncmp(p, "off", 3) == 0) {
    heaterPct = 0; glassPct = 0; applyOutputs();
    logEvent("ALL OFF");
  }
  else if (p[0] == 'n' || p[0] == 'N') {
    snprintf(msg, sizeof(msg), "NOTE: %s", p + 1); logEvent(msg);
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
  applyOutputs();

  Serial.println();
  Serial.println("# ===== Incubator step test =====");

  Wire.begin();
  if (!ads.begin(0x48)) {
    Serial.println("#  ADS1115 NOT FOUND - check I2C wiring");
    while (1) delay(1000);
  }
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.println("#  ADS1115 ok");

  rtd.begin(MAX31865_2WIRE);
  rtd.setThresholds(0x1234, 0x5678);
  uint16_t lo = rtd.getLowerThreshold(), hi = rtd.getUpperThreshold();
  rtd.setThresholds(0, 0xFFFF);
  if (lo == 0x1234 && hi == 0x5678) {
    Serial.println("#  MAX31865 ok");
  } else {
    Serial.println("#  *** MAX31865 NOT TALKING ***");
    Serial.print("#      wrote 0x1234/0x5678, read back 0x");
    Serial.print(lo, HEX); Serial.print("/0x"); Serial.println(hi, HEX);
    Serial.println("#      0xFFFF/0xFFFF means SDO stuck high: check 3V3, VIN and the 4 SPI wires");
  }

  printHelp();
}

void loop() {
  readSerial();
  if (millis() - lastLog < INTERVAL) return;
  lastLog += INTERVAL;

  if (rowCount >= 20) { printHeader(); rowCount = 0; }
  rowCount++;

  unsigned long s = millis() / 1000;
  char clk[12];
  snprintf(clk, sizeof(clk), "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  char num[12];

  snprintf(num, sizeof(num), "%lu", s);   printPaddedStr(num, 6);
  printPaddedStr(clk, 10);
  snprintf(num, sizeof(num), "%d", heaterPct); printPaddedStr(num, 7);
  snprintf(num, sizeof(num), "%d", glassPct);  printPaddedStr(num, 7);

  uint16_t raw = rtd.readRTD();
  uint8_t fault = rtd.readFault();
  float tPT = fault ? NAN : rtd.calculateTemperature(raw, RNOMINAL, RREF);
  printPadded(tPT, 9);

  for (int i = 0; i < NUM_THERM; i++) printPadded(readThermistor(i), 9);

  snprintf(num, sizeof(num), "%02X", fault);  printPaddedStr(num, 5);
  Serial.println();

  if (fault != lastFault) {
    Serial.print("#  PT1000 fault changed to 0x"); Serial.println(fault, HEX);
    lastFault = fault;
  }
  if (fault) rtd.clearFault();
}
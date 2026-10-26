#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"

// =====================================================================
// CO2 STEP TEST  -  Nucleo L476RG
//
// Wiring
//   Relay IN        -> D7 (PA8)   (LOW = relay ON = solenoid OPEN)
//   ExplorIR pin 1  -> D8 (PA9)
//   ExplorIR pin 2  -> D2 (PA10)
//   ExplorIR pin 3  -> GND,  pin 4 -> 3V3
//   SCD30 SCL -> D15 (PB8), SDA -> D14 (PB9), VDD -> 3V3, GND -> GND
//
// Commands (type the letter, press enter)
//   o = open valve (closes by itself after 60 s)
//   c = close valve
//   p = one 0.25 s pulse
//   s = start the step test
//   x = emergency stop (valve closed, locked until reset)
// =====================================================================

#define RELAY_PIN D7
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ---------- Test settings ----------
const uint32_t PULSE_MS        = 250;               // valve open time per pulse
const uint32_t PULSE_PERIOD_MS = 60000UL;           // one pulse every 60 s
const uint32_t BASELINE_MS     = 10UL * 60 * 1000;  // 10 min valve closed
const uint32_t STEP_MS         = 150UL * 60 * 1000; // 150 min pulsing
const uint32_t DECAY_MS        = 120UL * 60 * 1000; // 120 min valve closed

// ---------- Safety ----------
const uint32_t CO2_LIMIT_PPM      = 30000;    // stop above 3 %
const uint32_t MAX_PULSE_MS       = 2000;
const uint32_t MANUAL_OPEN_MAX_MS = 60000UL;
const uint8_t  MAX_READ_FAILS     = 5;

const uint32_t PRINT_INTERVAL_MS  = 1000;
const uint32_t RETRY_INTERVAL_MS  = 5000;     // retry missing sensors every 5 s

// ---------- Sensors ----------
Uart CO2Serial(PA10, PA9);   // (RX, TX)
SCD30 scd30;
uint32_t multiplier = 100;
bool explorOk = false;
bool scdOk    = false;

// ---------- State ----------
enum Mode { IDLE, MANUAL_OPEN, BASELINE, STEP, DECAY, DONE, ABORTED };
const char *modeName[] = { "IDLE", "MANUAL", "BASELINE", "STEP", "DECAY", "DONE", "STOPPED" };

Mode     mode        = IDLE;
bool     valveIsOpen = false;
uint32_t phaseStart  = 0;
uint32_t lastPulse   = 0;
uint32_t manualStart = 0;
uint32_t lastPrint   = 0;
uint32_t lastRetry   = 0;
uint32_t pulseCount  = 0;
uint8_t  readFails   = 0;

float scdCO2 = NAN, scdT = NAN, scdRH = NAN;

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------
void fmtTime(uint32_t ms, char *buf) {
  uint32_t s = ms / 1000;
  sprintf(buf, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
}

void msg(const char *text) {          // event messages stand out from readings
  Serial.print(">> ");
  Serial.println(text);
}

void valveSet(bool open) {
  digitalWrite(RELAY_PIN, open ? RELAY_ON : RELAY_OFF);
  digitalWrite(LED_BUILTIN, open ? HIGH : LOW);
  valveIsOpen = open;
}

void stopTest(const char *reason) {
  valveSet(false);
  mode = ABORTED;
  Serial.print(">> STOPPED: ");
  Serial.println(reason);
  msg("Valve closed. Reset the board to continue.");
}

void doPulse(uint32_t ms) {
  if (ms > MAX_PULSE_MS) ms = MAX_PULSE_MS;
  valveSet(true);
  delay(ms);
  valveSet(false);
  pulseCount++;
}

void startPhase(Mode m) {
  mode = m;
  phaseStart = millis();
  Serial.println();
  Serial.print(">> ===== ");
  Serial.print(modeName[m]);
  Serial.println(" =====");
  if (m == STEP) lastPulse = phaseStart - PULSE_PERIOD_MS;  // first pulse now
  if (m == DONE) { valveSet(false); msg("Test complete. Valve closed."); }
}

// ---------------------------------------------------------------------
// ExplorIR
// ---------------------------------------------------------------------
void flushSensor() {
  while (CO2Serial.available()) CO2Serial.read();
}

bool sensorCommand(const char *cmd, char expect, uint32_t *value) {
  flushSensor();
  CO2Serial.print(cmd);
  CO2Serial.print("\r\n");

  unsigned long start = millis();
  String line = "";

  while (millis() - start < 500) {
    if (CO2Serial.available()) {
      char c = CO2Serial.read();
      if (c == '\n') {
        int idx = line.indexOf(expect);
        if (idx >= 0) {
          if (value) *value = line.substring(idx + 1).toInt();
          return true;
        }
        line = "";
      } else if (c != '\r') {
        line += c;
      }
    }
  }
  return false;
}

bool explorInit() {
  if (!sensorCommand("K 2", 'K', NULL)) return false;     // polling mode
  if (!sensorCommand(".", '.', &multiplier)) multiplier = 100;
  sensorCommand("A 16", 'A', NULL);                       // default filter
  return true;
}

// ---------------------------------------------------------------------
// SCD30
// ---------------------------------------------------------------------
bool scdInit() {
  if (!scd30.begin(Wire, false)) return false;            // ASC off
  scd30.setMeasurementInterval(2);
  return true;
}

// ---------------------------------------------------------------------
// Serial commands
// ---------------------------------------------------------------------
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;

    if (c == 'x') { stopTest("you pressed x"); continue; }

    if (c == 'c') {
      valveSet(false);
      if (mode == MANUAL_OPEN) mode = IDLE;
      msg("Valve CLOSED");
      continue;
    }

    if (mode == ABORTED) { msg("Locked after stop. Reset the board."); continue; }
    if (mode != IDLE)    { msg("Busy. Only c (close) or x (stop) work right now."); continue; }

    if (c == 'o') {
      manualStart = millis();
      mode = MANUAL_OPEN;
      valveSet(true);
      msg("Valve OPEN. Closes by itself in 60 s. Press c to close sooner.");
    } else if (c == 'p') {
      msg("Pulse (0.25 s)");
      doPulse(PULSE_MS);
    } else if (c == 's') {
      if (!explorOk) { msg("Can't start: ExplorIR not responding. Check wiring."); continue; }
      pulseCount = 0;
      startPhase(BASELINE);
    } else {
      msg("Unknown command. Use o, c, p, s or x.");
    }
  }
}

// ---------------------------------------------------------------------
// Read sensors, check safety, print one readable line
// ---------------------------------------------------------------------
void readAndPrint() {
  uint32_t now = millis();

  // Retry sensors that aren't working yet
  if (now - lastRetry >= RETRY_INTERVAL_MS) {
    lastRetry = now;
    if (!explorOk && explorInit()) { explorOk = true; readFails = 0; msg("ExplorIR connected"); }
    if (!scdOk && scdInit())       { scdOk = true;                   msg("SCD30 connected"); }
  }

  // ExplorIR
  uint32_t raw = 0;
  bool okE = explorOk && sensorCommand("Z", 'Z', &raw);
  uint32_t ppmE = raw * multiplier;
  if (explorOk) {
    if (okE) readFails = 0;
    else if (++readFails >= MAX_READ_FAILS) { explorOk = false; msg("ExplorIR stopped responding"); }
  }

  // SCD30 (updates every 2 s)
  if (scdOk && scd30.dataAvailable()) {
    scdCO2 = scd30.getCO2();
    scdT   = scd30.getTemperature();
    scdRH  = scd30.getHumidity();
  }

  // Safety
  bool gasPossible = (mode == BASELINE || mode == STEP || mode == MANUAL_OPEN);
  if (gasPossible && !explorOk) stopTest("ExplorIR not responding");
  if (mode != ABORTED &&
      ((okE && ppmE > CO2_LIMIT_PPM) || (scdOk && scdCO2 > CO2_LIMIT_PPM))) {
    stopTest("CO2 above 3 %");
  }

  // Print
  char tNow[12], tPhase[12], line[220];
  fmtTime(now, tNow);
  fmtTime(now - phaseStart, tPhase);

  char explorStr[32], scdStr[48];
  // (integer maths only: STM32duino's sprintf doesn't print floats by default)
  if (okE) sprintf(explorStr, "%6lu ppm (%lu.%02lu %%)", ppmE, ppmE / 10000, (ppmE % 10000) / 100);
  else     sprintf(explorStr, "   no reply         ");

  if (scdOk && !isnan(scdCO2)) {
    long c  = lroundf(scdCO2);
    long t  = lroundf(scdT * 10);
    long rh = lroundf(scdRH * 10);
    sprintf(scdStr, "%6ld ppm  %3ld.%01ld C  %3ld.%01ld %%RH", c, t / 10, labs(t % 10), rh / 10, rh % 10);
  } else {
    sprintf(scdStr, "   no reply");
  }

  bool showPhaseTime = (mode == BASELINE || mode == STEP || mode == DECAY);

  sprintf(line, "%s | %-8s %s | valve %-6s | pulses %3lu | ExplorIR %s | SCD30 %s",
          tNow,
          modeName[mode],
          showPhaseTime ? tPhase : "        ",
          valveIsOpen ? "OPEN" : "closed",
          pulseCount,
          explorStr,
          scdStr);
  Serial.println(line);
}

// ---------------------------------------------------------------------
void setup() {
  digitalWrite(RELAY_PIN, RELAY_OFF);          // valve closed before pin is an output
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  pinMode(LED_BUILTIN, OUTPUT);
  valveSet(false);

  Serial.begin(115200);
  CO2Serial.begin(9600);
  Wire.begin();
  Wire.setClock(50000);
  delay(1500);

  Serial.println();
  Serial.println("==========================================");
  Serial.println(" CO2 STEP TEST");
  Serial.println("==========================================");

  explorOk = explorInit();
  scdOk    = scdInit();

  Serial.print(" ExplorIR : "); Serial.println(explorOk ? "OK" : "NOT FOUND (will keep retrying)");
  Serial.print(" SCD30    : "); Serial.println(scdOk    ? "OK" : "NOT FOUND (will keep retrying)");
  Serial.print(" Pulse    : "); Serial.print(PULSE_MS); Serial.print(" ms every ");
  Serial.print(PULSE_PERIOD_MS / 1000); Serial.println(" s");
  Serial.print(" Phases   : baseline "); Serial.print(BASELINE_MS / 60000);
  Serial.print(" min, step ");             Serial.print(STEP_MS / 60000);
  Serial.print(" min, decay ");            Serial.print(DECAY_MS / 60000);
  Serial.println(" min");
  Serial.println(" Safety   : stops above 3 % CO2");
  Serial.println("------------------------------------------");
  Serial.println(" o = open   c = close   p = pulse");
  Serial.println(" s = start  x = emergency stop");
  Serial.println("==========================================");
  Serial.println();
}

void loop() {
  uint32_t now = millis();

  handleSerial();

  if (mode == MANUAL_OPEN && now - manualStart >= MANUAL_OPEN_MAX_MS) {
    valveSet(false);
    mode = IDLE;
    msg("60 s up. Valve CLOSED.");
  }

  switch (mode) {
    case BASELINE:
      if (now - phaseStart >= BASELINE_MS) startPhase(STEP);
      break;
    case STEP:
      if (now - phaseStart >= STEP_MS) { startPhase(DECAY); break; }
      if (now - lastPulse >= PULSE_PERIOD_MS) { lastPulse = now; doPulse(PULSE_MS); }
      break;
    case DECAY:
      if (now - phaseStart >= DECAY_MS) startPhase(DONE);
      break;
    default:
      break;
  }

  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = now;
    readAndPrint();
  }
}
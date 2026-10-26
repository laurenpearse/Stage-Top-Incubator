#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "SparkFun_SCD30_Arduino_Library.h"

// =====================================================================
//  I2C BENCH TEST: ADS1115 + SCD30 on the same bus
//
//  Does three things, in this order:
//    1. Reads the idle level of SDA and SCL before touching the bus.
//       If either is low, something is holding or shorting the line and
//       nothing else will work. It tells you which one.
//    2. Scans the bus and lists every address that answers.
//       Expect 0x48 (ADS1115) and 0x61 (SCD30).
//    3. Brings both devices up and prints a reading every second.
//
//  Commands:
//    s   rescan the bus
//    r   re-read the idle pin levels (unplug things and watch it change)
//    i   re-initialise both devices
//    ?   help
// =====================================================================

Adafruit_ADS1115 ads;
SCD30 scd30;

bool adsOk = false;
bool scdOk = false;

const int SAMPLES = 8;

char cmdBuf[24];
int  cmdLen = 0;

unsigned long lastPrint = 0;

// ---------------------------------------------------------------------
//  Read SDA and SCL as plain digital inputs, before Wire takes them over.
//  Both should read HIGH if the pull-ups are working and nothing is
//  holding the bus.
// ---------------------------------------------------------------------
void checkBusIdle() {
  Wire.end();
  pinMode(PIN_WIRE_SDA, INPUT);
  pinMode(PIN_WIRE_SCL, INPUT);
  delay(5);
  int sda = digitalRead(PIN_WIRE_SDA);
  int scl = digitalRead(PIN_WIRE_SCL);

  Serial.println();
  Serial.print("#  idle bus:  SDA = ");  Serial.print(sda ? "HIGH" : "LOW");
  Serial.print("    SCL = ");            Serial.println(scl ? "HIGH" : "LOW");

  if (sda && scl) {
    Serial.println("#  both lines idle high - bus looks healthy");
  } else {
    Serial.println("#  *** a line is being held low ***");
    if (!scl) Serial.println("#    SCL low  -> a device is stuck mid clock-stretch, or SCL is shorted to GND");
    if (!sda) Serial.println("#    SDA low  -> a device is unpowered and clamping, or SDA is shorted to GND");
    Serial.println("#    Unplug one device at a time and press 'r' to see which one releases it.");
  }

  Wire.begin();
  Wire.setClock(50000);
}

// ---------------------------------------------------------------------
void scanBus() {
  Serial.println();
  Serial.println("#  scanning...");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("#    found 0x");
      if (a < 16) Serial.print('0');
      Serial.print(a, HEX);
      if (a == 0x48) Serial.print("   <- ADS1115");
      if (a == 0x61) Serial.print("   <- SCD30");
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("#    nothing answered. Check wiring, power and pull-ups.");
  Serial.print("#  devices found: "); Serial.println(found);
  Serial.println();
}

// ---------------------------------------------------------------------
void initDevices() {
  Serial.println("#  initialising...");

  adsOk = ads.begin(0x48);
  if (adsOk) {
    ads.setGain(GAIN_ONE);
    ads.setDataRate(RATE_ADS1115_128SPS);
    Serial.println("#    ADS1115 ok");
  } else {
    Serial.println("#    ADS1115 FAILED");
  }

  //  SCD30 tops out at 100 kHz and stretches the clock for up to 150 ms,
  //  so the bus stays slow. false = auto self-calibration off.
  scdOk = scd30.begin(Wire, false);
  if (scdOk) {
    scd30.setMeasurementInterval(2);
    Serial.println("#    SCD30 ok (give it a minute before the readings mean anything)");
  } else {
    Serial.println("#    SCD30 FAILED");
  }
  Serial.println();
}

// ---------------------------------------------------------------------
void printHelp() {
  Serial.println("#");
  Serial.println("#  s   rescan the bus");
  Serial.println("#  r   re-read the idle SDA/SCL levels");
  Serial.println("#  i   re-initialise both devices");
  Serial.println("#  ?   this help");
  Serial.println("#");
}

void handleCommand() {
  cmdBuf[cmdLen] = '\0';
  char c = cmdBuf[0];
  if      (c == 's') scanBus();
  else if (c == 'r') checkBusIdle();
  else if (c == 'i') initDevices();
  else if (c == '?') printHelp();
  else if (cmdLen > 0) Serial.println("#  s = scan, r = read lines, i = init, ? = help");
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

  Serial.println();
  Serial.println("# ===== I2C bench test: ADS1115 + SCD30 =====");

  checkBusIdle();
  scanBus();
  initDevices();
  printHelp();

  Serial.println("  A0_V     A1_V     A2_V     A3_V      CO2      T_scd    RH");
  Serial.println("-----------------------------------------------------------------");
}

// ---------------------------------------------------------------------
void loop() {
  readSerial();

  if (millis() - lastPrint < 1000) return;
  lastPrint = millis();

  //  ADS1115: all four channels as volts
  for (int ch = 0; ch < 4; ch++) {
    if (!adsOk) { Serial.print("     ---"); continue; }
    float sum = 0;
    for (int i = 0; i < SAMPLES; i++) sum += ads.computeVolts(ads.readADC_SingleEnded(ch));
    float v = sum / SAMPLES;
    Serial.print("  ");
    if (v >= 0 && v < 10) Serial.print(' ');
    Serial.print(v, 4);
  }

  //  SCD30
  if (!scdOk) {
    Serial.print("      ---      ---      ---");
  } else if (scd30.dataAvailable()) {
    float co2 = scd30.getCO2();
    float t   = scd30.getTemperature();
    float rh  = scd30.getHumidity();
    Serial.print("   ");  Serial.print(co2, 0);
    Serial.print("     ");  Serial.print(t, 2);
    Serial.print("    ");   Serial.print(rh, 1);
  } else {
    Serial.print("   (no new SCD30 sample)");
  }

  Serial.println();
}
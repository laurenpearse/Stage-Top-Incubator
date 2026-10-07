#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31865.h>

// ---------------- ADS1115 (thermistors) ----------------
// I2C: SCL -> D15 (PB8), SDA -> D14 (PB9), ADDR -> GND (0x48)
// Each channel: 3V3 -> R_fixed -> Ax -> thermistor -> GND

Adafruit_ADS1115 ads;

// Thermistor constants (TE GA10K3MCD1)
const float R0   = 10000.0;   // resistance at 25 C (ohms)
const float T0   = 298.15;    // 25 C in kelvin
const float BETA = 3976.0;    // Beta 25/85

// Supply voltage feeding the dividers (measured with multimeter)
const float V_SUPPLY = 3.29;

// Measured fixed resistor values for each ADC channel
float R_FIXED[4] = {
  9840.0,    // A0
  9980.0,    // A1
  9890.0,    // A2
  9940.0     // A3
};

// Calibration offsets from PT1000 comparison test (5 Oct 2026, ~23.6 C)
float T_OFFSET[4] = {
  -0.06,   // A0 (T1)
  +0.51,   // A1 (T2)
  -0.25,   // A2 (T3)
  -0.23    // A3 (T4)
};
const int NUM_THERM = 4;
const int SAMPLES = 16;       // readings averaged per channel

// ---------------- MAX31865 (PT1000) ----------------
// Software SPI: CS -> D10, SDI -> D11, SDO -> D12, CLK -> D13
Adafruit_MAX31865 rtd = Adafruit_MAX31865(10, 11, 12, 13);

#define RREF      4300.0   // reference resistor on the PT1000 board
#define RNOMINAL  1000.0   // PT1000 resistance at 0 C

const unsigned long INTERVAL = 1000;   // ms between readings
unsigned long lastPrint = 0;

// Read an ADS1115 channel several times and return the average voltage
float readVolts(uint8_t ch) {
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += ads.computeVolts(ads.readADC_SingleEnded(ch));
  }
  return sum / SAMPLES;
}

// Convert one thermistor channel to temperature. Returns NAN if open/shorted.
float readThermistor(uint8_t ch) {
  float v = readVolts(ch);
  if (v < 0.01 || v > V_SUPPLY - 0.01) return NAN;

  float rTherm = R_FIXED[ch] * v / (V_SUPPLY - v);              // voltage -> resistance
  float tK = 1.0 / (1.0 / T0 + log(rTherm / R0) / BETA);        // resistance -> temperature
  return tK - 273.15 + T_OFFSET[ch];;
}

void setup() {
  Serial.begin(115200);
  delay(2000);   // time to open the serial monitor
  Serial.println("\nThermistor + PT1000 comparison test");

  // ADS1115
  Wire.begin();
  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not found. Check I2C wiring.");
    while (1) delay(1000);
  }
  ads.setGain(GAIN_ONE);                 // +/-4.096 V range, 0.125 mV per step
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.println("ADS1115 OK");

  // MAX31865
  rtd.begin(MAX31865_2WIRE);
  rtd.setThresholds(0x1234, 0x5678);     // read-back test
  bool ok = (rtd.getLowerThreshold() == 0x1234) && (rtd.getUpperThreshold() == 0x5678);
  rtd.setThresholds(0, 0xFFFF);
  Serial.println(ok ? "MAX31865 OK" : "MAX31865 NOT responding - check power and SPI wiring");
}

void loop() {
  if (millis() - lastPrint < INTERVAL) return;
  lastPrint = millis();

  // Timestamp
  unsigned long s = millis() / 1000;
  char timeStr[12];
  sprintf(timeStr, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  Serial.print(timeStr);

  // PT1000 reference
  uint16_t raw = rtd.readRTD();
  float tPT = rtd.calculateTemperature(raw, RNOMINAL, RREF);
  uint8_t fault = rtd.readFault();

  Serial.print("   PT: ");
  Serial.print(tPT, 2);
  Serial.print(" C");

  // Thermistors
  for (int i = 0; i < NUM_THERM; i++) {
    float t = readThermistor(i);
    Serial.print("   T");
    Serial.print(i + 1);
    Serial.print(": ");
    if (isnan(t)) Serial.print("ERR");
    else { Serial.print(t, 2); Serial.print(" C"); }
  }
  Serial.println();

  if (fault) {
    Serial.print("PT1000 fault 0x");
    Serial.println(fault, HEX);
    rtd.clearFault();
  }
}
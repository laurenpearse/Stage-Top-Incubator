#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// ADS1115 on I2C: SCL -> D15 (PB8), SDA -> D14 (PB9), ADDR -> GND (0x48)
// Each channel: 3V3 -> R_fixed -> Ax -> thermistor -> GND

Adafruit_ADS1115 ads;

// Thermistor constants (TE GA10K3MCD1)
const float R0   = 10000.0;   // resistance at 25 C (ohms)
const float T0   = 298.15;    // 25 C in kelvin
const float BETA = 3976.0;    // Beta 25/85

// Supply voltage feeding the dividers (measured with multimeter)
const float V_SUPPLY = 3.29; //ya

// Measured fixed resistor values for each ADC channel
float R_FIXED[4] = {
  9840.0,    // A0
  9980.0,   // A1
  9890.0,    // A2
  9940.0     // A3
};

const int NUM_THERM = 4;
const int SAMPLES = 16;       // readings averaged per channel

// Read a channel several times and return the average voltage
float readVolts(uint8_t ch) {
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += ads.computeVolts(ads.readADC_SingleEnded(ch));
  }
  return sum / SAMPLES;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nThermistor test starting");

  Wire.begin();
  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not found. Check wiring.");
    while (1) delay(1000);
  }

  ads.setGain(GAIN_ONE);                 // +/-4.096 V range, 0.125 mV per step
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.println("ADS1115 found");
}

void loop() {
  unsigned long s = millis() / 1000;
  char timeStr[12];
  sprintf(timeStr, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  Serial.print(timeStr);

  for (int i = 0; i < NUM_THERM; i++) {
    float v = readVolts(i);

    Serial.print("   T");
    Serial.print(i + 1);
    Serial.print(": ");

    // Catch open or shorted thermistors
    if (v < 0.01 || v > V_SUPPLY - 0.01) {
      Serial.print("ERR");
      continue;
    }

    // Voltage -> resistance (voltage divider)
    float rTherm = R_FIXED[i] * v / (V_SUPPLY - v);

    // Resistance -> temperature (Beta equation)
    float tK = 1.0 / (1.0 / T0 + log(rTherm / R0) / BETA);
    float tC = tK - 273.15;

    Serial.print(tC, 2);
    Serial.print(" C");
  }
  Serial.println();

  delay(1000);
}
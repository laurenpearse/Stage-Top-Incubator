#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"

// SCD30 on I2C1
// VDD -> 3V3, GND -> GND
// SCL -> D15 (PB8), SDA -> D14 (PB9)
// 4.7k to 10k pull-ups from SDA and SCL to 3V3

SCD30 airSensor;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nSCD30 test starting");

  Wire.begin();
  Wire.setClock(50000);   // SCD30 max is 100 kHz, 50 kHz is safer with clock stretching

  // false = automatic self calibration OFF
  if (!airSensor.begin(Wire, false)) {
    Serial.println("SCD30 not found. Check wiring and pull-ups.");
    while (1) delay(1000);
  }

  airSensor.setMeasurementInterval(2);   // seconds, 2 is the minimum
  Serial.println("SCD30 found, first reading in ~2 s");
}

void loop() {
  if (airSensor.dataAvailable()) {
    unsigned long s = millis() / 1000;
    char timeStr[12];
    sprintf(timeStr, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

    Serial.print(timeStr);
    Serial.print("   CO2: ");
    Serial.print(airSensor.getCO2());
    Serial.print(" ppm   Temp: ");
    Serial.print(airSensor.getTemperature(), 1);
    Serial.print(" C   RH: ");
    Serial.print(airSensor.getHumidity(), 1);
    Serial.println(" %");
  }
  delay(200);
}
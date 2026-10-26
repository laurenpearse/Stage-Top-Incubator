#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin();

  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found");
    while (true) delay(1000);
  }
  Serial.print("Chip ID: 0x");
  Serial.println(bmp.sensorID(), HEX);  // should print 0x58
}

void loop() {
  Serial.print("Temp: ");
  Serial.print(bmp.readTemperature());
  Serial.println(" C");

  Serial.print("Pressure: ");
  Serial.print(bmp.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("Altitude: ");
  Serial.print(bmp.readAltitude(1013.25));
  Serial.println(" m");

  Serial.println();
  delay(1000);
}
#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin();
}

void loop() {
  Serial.println("Scanning...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println("Nothing found");
  delay(2000);
}
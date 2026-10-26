#include <Arduino.h>
#include <Adafruit_MAX31865.h>

Adafruit_MAX31865 rtd = Adafruit_MAX31865(10, 11, 12, 13); // CS, SDI, SDO, CLK

#define RREF      4300.0   // reference resistor on the PT1000 board
#define RNOMINAL  1000.0   // PT1000 resistance at 0 C

const unsigned long INTERVAL = 1000;   // ms between readings

unsigned long lastPrint = 0;
unsigned long startTime = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);  // time to open the serial monitor
  rtd.begin(MAX31865_2WIRE);

  // Read-back test: write two values and check they come back
  rtd.setThresholds(0x1234, 0x5678);
  bool ok = (rtd.getLowerThreshold() == 0x1234) && (rtd.getUpperThreshold() == 0x5678);
  rtd.setThresholds(0, 0xFFFF);
  Serial.println(ok ? "MAX31865 OK" : "MAX31865 NOT responding - check power and SPI wiring");

  startTime = millis();
}

void loop() {
  if (millis() - lastPrint >= INTERVAL) {
    lastPrint = millis();

    uint16_t raw = rtd.readRTD();
    float resistance = raw / 32768.0 * RREF;
    float temp = rtd.calculateTemperature(raw, RNOMINAL, RREF);
    uint8_t fault = rtd.readFault();
    float t = (millis() - startTime) / 1000.0;

    Serial.print("Time: ");          Serial.print(t, 1);
    Serial.print(" s   Resistance: "); Serial.print(resistance, 2);
    Serial.print(" ohm   Temperature: "); Serial.print(temp, 2);
    Serial.println(" C");

    if (fault) {
      Serial.print("Fault 0x"); Serial.println(fault, HEX);
      rtd.clearFault();
    }
  }
}
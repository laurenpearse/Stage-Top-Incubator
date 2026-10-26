#include <Arduino.h>

// ExplorIR-M fresh air zero calibration
// Sensor pin 1 (Rx_In)  -> D8 / PA9
// Sensor pin 2 (Tx_Out) -> D2 / PA10
// Sensor pin 3 (0V)     -> GND
// Sensor pin 4 (V+)     -> 3V3

Uart CO2Serial(PA10, PA9);   // (RX, TX)

uint32_t multiplier = 100;

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

void setup() {
  Serial.begin(115200);
  CO2Serial.begin(9600);
  delay(1500);

  Serial.println("\nExplorIR-M fresh air zero");

  if (sensorCommand("K 2", 'K', NULL)) Serial.println("Polling mode set");
  else Serial.println("No reply to K 2, check wiring");

  if (sensorCommand(".", '.', &multiplier)) {
    Serial.print("Multiplier = ");
    Serial.println(multiplier);
  }

  // Datasheet recommends filter = 32 for zeroing
  if (sensorCommand("A 32", 'A', NULL)) Serial.println("Filter set to 32");
  else Serial.println("No reply to A 32");

  Serial.println("Leave it in fresh air for ~5 min until the reading is steady.");
  Serial.println("Then type 'c' and press enter to zero at 400 ppm.\n");
}

void loop() {
  // Zero when 'c' is typed
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c') {
      Serial.println("\nZeroing in fresh air (400 ppm)...");
      if (sensorCommand("G", 'G', NULL)) Serial.println("Zero done\n");
      else Serial.println("No reply to G\n");
    }
  }

  // Print reading every second so you can watch it settle
  uint32_t raw;
  if (sensorCommand("Z", 'Z', &raw)) {
    uint32_t ppm = raw * multiplier;
    Serial.print("Raw: ");
    Serial.print(raw);
    Serial.print("   CO2: ");
    Serial.print(ppm);
    Serial.println(" ppm");
  } else {
    Serial.println("No CO2 reading");
  }

  delay(1000);
}
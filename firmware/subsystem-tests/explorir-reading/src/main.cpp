#include <Arduino.h>

// ExplorIR-M-E-100 CO2 sensor test on Nucleo L476RG (STM32duino)
// Sensor pin 1 (Rx_In)  -> D8 / PA9  (our TX)
// Sensor pin 2 (Tx_Out) -> D2 / PA10 (our RX)
// Sensor pin 3 (0V)     -> GND
// Sensor pin 4 (V+)     -> 3V3

Uart CO2Serial(PA10, PA9);   // (RX, TX)   // (RX, TX)

uint32_t multiplier = 100;   // 100% sensor should report 100, we read it at startup

// Clear out anything sitting in the receive buffer
void flushSensor() {
  while (CO2Serial.available()) CO2Serial.read();
}

// Send a command, wait for the reply line that contains the expected letter,
// then pull the number out of it. Replies look like " Z 00450"
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
        line = "";               // wrong line, keep waiting
      } else if (c != '\r') {
        line += c;
      }
    }
  }
  return false;                  // timed out
}

void setup() {
  Serial.begin(115200);          // USB serial to your PC (ST-LINK)
  CO2Serial.begin(9600);         // sensor UART, 8N1
  delay(1500);                   // sensor needs ~1.2 s to give first reading

  Serial.println("\nExplorIR-M test starting");

  // Put sensor into polling mode (it streams by default)
  if (sensorCommand("K 2", 'K', NULL)) {
    Serial.println("Polling mode set");
  } else {
    Serial.println("No reply to K 2. Check wiring, try swapping D2 and D8.");
  }

  // Read the scaling factor
  if (sensorCommand(".", '.', &multiplier)) {
    Serial.print("Multiplier = ");
    Serial.println(multiplier);
  } else {
    Serial.println("Couldn't read multiplier, assuming 100");
    multiplier = 100;
  }
}

void loop() {
  uint32_t raw;

  // Elapsed time since startup as hh:mm:ss
  unsigned long s = millis() / 1000;
  char timeStr[12];
  sprintf(timeStr, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

  Serial.print(timeStr);
  Serial.print("   ");

  if (sensorCommand("Z", 'Z', &raw)) {
    uint32_t ppm = raw * multiplier;
    Serial.print("Raw: ");
    Serial.print(raw);
    Serial.print("   CO2: ");
    Serial.print(ppm);
    Serial.print(" ppm   (");
    Serial.print(ppm / 10000.0, 2);
    Serial.println(" %)");
  } else {
    Serial.println("No CO2 reading");
  }

  delay(1000);
}
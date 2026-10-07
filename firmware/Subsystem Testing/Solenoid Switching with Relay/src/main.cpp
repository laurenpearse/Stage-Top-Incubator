#include <Arduino.h>

#define RELAY_PIN D7
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

const unsigned long SWITCH_TIME = 5000;   // 5s
int cycle = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  Serial.println("--- Board started ---");
}

void loop() {
  cycle++;
  Serial.print("Cycle "); Serial.print(cycle); Serial.println(": Relay ON");
  digitalWrite(RELAY_PIN, RELAY_ON);
  
  delay(SWITCH_TIME);

  Serial.print("Cycle "); Serial.print(cycle); Serial.println(": Relay OFF");
  digitalWrite(RELAY_PIN, RELAY_OFF);
  
  delay(SWITCH_TIME);
}
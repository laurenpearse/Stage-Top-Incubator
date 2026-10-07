#include <Arduino.h>

const int HEATER_PIN = D4;              // heater on D4

const unsigned long ON_TIME  = 10000;   // heater on for 10 s
const unsigned long OFF_TIME = 20000;   // heater off for 20 s

const unsigned long BLINK_FAST = 200;   // LED toggle rate when heater ON
const unsigned long BLINK_SLOW = 1000;  // LED toggle rate when heater OFF

bool heaterOn = false;
bool ledState = false;
unsigned long heaterTimer = 0;
unsigned long ledTimer = 0;

void setup() {
  digitalWrite(HEATER_PIN, LOW);        // start with heater off
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  heaterTimer = millis();
}

void loop() {
  unsigned long now = millis();

  // ---- Heater cycle ----
  if (heaterOn && now - heaterTimer >= ON_TIME) {
    heaterOn = false;
    digitalWrite(HEATER_PIN, LOW);
    heaterTimer = now;
  }
  else if (!heaterOn && now - heaterTimer >= OFF_TIME) {
    heaterOn = true;
    digitalWrite(HEATER_PIN, HIGH);
    heaterTimer = now;
  }

  // ---- LED blink ----
  unsigned long blinkRate = heaterOn ? BLINK_FAST : BLINK_SLOW;
  if (now - ledTimer >= blinkRate) {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
    ledTimer = now;
  }
}
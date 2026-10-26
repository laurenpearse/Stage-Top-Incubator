#include <Arduino.h>
#include <math.h>

const float R_FIXED = 996.9;   // put your measured resistor value here
const float R0 = 1000.0;        // PT1000 resistance at 0 C
const float A = 3.9083e-3;      // Callendar-Van Dusen coefficients
const float B = -5.775e-7;

unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);     // use the full 12-bit ADC (0 to 4095)
}

float readPT1000() {
  long sum = 0;
  for (int i = 0; i < 64; i++) sum += analogRead(A0);   // average 64 readings to reduce noise
  float code = sum / 64.0;
  return R_FIXED * code / (4095.0 - code);              // voltage divider -> resistance
}

float resistanceToTemp(float R) {
  // R = R0(1 + A*T + B*T^2), solved for T (valid for 0 C and above)
  return (-A + sqrt(A * A - 4 * B * (1 - R / R0))) / (2 * B);
}

void loop() {
  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    float R = readPT1000();
    float T = resistanceToTemp(R);
    Serial.print("Resistance: "); Serial.print(R);
    Serial.print(" ohm   Temperature: "); Serial.print(T);
    Serial.println(" C");
  }
}
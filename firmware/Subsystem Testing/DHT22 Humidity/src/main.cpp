#include <Arduino.h>

#define DHT_PIN PC0

static bool waitWhile(int level, uint32_t timeout_us, uint32_t *elapsed)
{
    uint32_t t0 = micros();
    while (digitalRead(DHT_PIN) == level) {
        if ((micros() - t0) > timeout_us) return false;
    }
    if (elapsed) *elapsed = micros() - t0;
    return true;
}

static bool dht22Once(float *rh, float *tc)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};

    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, LOW);
    delay(2);
    pinMode(DHT_PIN, INPUT_PULLUP);

    if (!waitWhile(HIGH, 100, nullptr)) return false;
    if (!waitWhile(LOW,  150, nullptr)) return false;
    if (!waitWhile(HIGH, 150, nullptr)) return false;

    for (int i = 0; i < 40; i++) {
        uint32_t highUs;
        if (!waitWhile(LOW,  100, nullptr)) return false;
        if (!waitWhile(HIGH, 150, &highUs)) return false;
        data[i / 8] <<= 1;
        if (highUs > 45) data[i / 8] |= 1;
    }

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) return false;

    *rh = (((uint16_t)data[0] << 8) | data[1]) * 0.1f;
    int16_t raw = (((uint16_t)(data[2] & 0x7F)) << 8) | data[3];
    *tc = (data[2] & 0x80) ? -raw * 0.1f : raw * 0.1f;
    return true;
}

bool dht22Read(float *rh, float *tc)
{
    for (int a = 0; a < 3; a++) {
        if (dht22Once(rh, tc)) return true;
        delay(5);
    }
    return false;
}

static uint32_t g_n = 0, g_fail = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(DHT_PIN, INPUT_PULLUP);
    delay(1500);

    Serial.println();
    Serial.println("DHT22 humidity log, 2 s samples");
    Serial.println("-----------------------------------");
    Serial.printf("%7s %8s %8s %7s\n", "t_min", "rh_pct", "t_degC", "fails");
}

void loop()
{
    char fT[12], fR[12], fC[12];
    float rh, tc;
    bool ok = dht22Read(&rh, &tc);
    if (!ok) g_fail++;

    dtostrf(g_n / 60.0f, 7, 2, fT);
    if (ok) { dtostrf(rh, 8, 1, fR); dtostrf(tc, 8, 1, fC); }
    else    { strcpy(fR, "     NaN"); strcpy(fC, "     NaN"); }

    Serial.printf("%s %s %s %7lu\n", fT, fR, fC, (unsigned long)g_fail);

    g_n += 2;
    delay(2000);
}
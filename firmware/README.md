# Firmware

Firmware for the STM32L476RG Nucleo-64, written with PlatformIO. All projects except `First STM32 Test` use the Arduino framework (STM32duino) in C++; `First STM32 Test` uses ST's HAL in C. Each folder in `Subsystem Testing/` is a separate PlatformIO project used to bring up and test one part of the system before integration, as described in the commissioning chapter of the report.

To build a project, open its folder in VS Code with the PlatformIO extension and run Build/Upload. Library dependencies are listed in each `platformio.ini` and are downloaded automatically. Serial output is at 115 200 baud unless stated otherwise in `platformio.ini`.

## Subsystem test projects

| Folder | What it tests |
|---|---|
| `First STM32 Test` | First board bring-up, blinking the on-board LED with ST's HAL |
| `PT1000 Reading Voltage Divider` | Early PT1000 reading through a voltage divider |
| `Max31865 and PT1000` | PT1000 read through the MAX31865 RTD converter (SPI) |
| `Thermistor Readings` | Thermistors read through the ADS1115 ADC (I2C) |
| `PT1000 and Thermistors` | PT1000 and thermistors together, used for thermistor calibration |
| `Heater Switching Control` | Switching a heater on and off through its driver circuit (10 s on, 20 s off) |
| `Solenoid Switching with Relay` | 230 V CO2 solenoid switched through the relay |
| `EXPLORIR-M-E-100 Reading` | ExplorIR-M CO2 sensor over UART |
| `Explorir Zeroing` | Fresh-air zero of the ExplorIR-M |
| `SCD30 Readings` | SCD30 CO2, temperature and humidity sensor (I2C) |
| `SCD30 and ADC1115` | SCD30 and ADS1115 sharing one I2C bus, with bus checks; `logs/` holds the serial output |
| `Humidity Sensor` | BMP280 temperature and pressure sensor (I2C, address 0x76) |
| `DHT22 Humidity` | DHT22 humidity sensor (single wire); `logs/` holds the serial output |

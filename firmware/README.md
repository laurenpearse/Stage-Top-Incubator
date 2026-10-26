# Firmware

Firmware for the STM32L476RG Nucleo-64, written with PlatformIO. All projects except `first-stm32-test` use the Arduino framework (STM32duino) in C++; `first-stm32-test` uses ST's HAL in C.

- `subsystem-tests/` brings up and tests each sensor and actuator on its own (commissioning chapter).
- `control-tests/` holds the step tests, closed-loop runs, CO2 test and long run (results chapters). The `logs/` folder in each project holds the serial output from those runs.

To build a project, open its folder in VS Code with the PlatformIO extension and run Build, then Upload. Libraries are listed in each `platformio.ini` and download automatically. Serial output is at 115 200 baud unless `platformio.ini` says otherwise.

## Subsystem tests

| Folder | What it tests |
|---|---|
| `first-stm32-test` | First board bring-up, blinking the on-board LED with ST's HAL |
| `pt1000-voltage-divider` | Early PT1000 reading through a voltage divider |
| `pt1000-max31865` | PT1000 read through the MAX31865 RTD converter (SPI) |
| `thermistors-ads1115` | Thermistors read through the ADS1115 ADC (I2C) |
| `pt1000-and-thermistors` | PT1000 and thermistors together, used for thermistor calibration |
| `heater-switching` | Switching a heater on and off through its driver circuit (10 s on, 20 s off) |
| `solenoid-relay` | 230 V CO2 solenoid switched through the relay |
| `explorir-reading` | ExplorIR-M CO2 sensor over UART |
| `explorir-zeroing` | Fresh-air zero of the ExplorIR-M |
| `scd30-reading` | SCD30 CO2, temperature and humidity sensor (I2C) |
| `scd30-and-ads1115` | SCD30 and ADS1115 sharing one I2C bus, with bus checks |
| `bmp280` | BMP280 temperature and pressure sensor (I2C, address 0x76) |
| `dht22` | DHT22 humidity sensor (single wire) |

## Control tests

| Folder | What it does |
|---|---|
| `thermal-step-tests` | Open-loop step tests of the wall and glass heaters, used to fit the thermal models |
| `closed-loop-temperature` | Closed-loop PI control of the chamber air (PT1000) and the ITO glass, with lambda tuning, setpoint weighting and anti-windup |
| `closed-loop-with-base-and-humidity` | Closed-loop chamber and glass with the heated base at a fixed duty, logging the well water and room temperature |
| `heated-base` | Closed-loop chamber and glass with the heated base at a manually set duty |
| `plate-evenness` | Compares a near-centre well and a corner well with the heated base on |
| `long-humidity-run` | Long humidity and stability run at 39 °C, logging the DHT22 and BMP280; `longRun.log` is the main run |
| `co2-step-test` | CO2 valve step test with the ExplorIR-M and SCD30 |

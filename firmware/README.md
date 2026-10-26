# Firmware

Firmware for the STM32L476RG Nucleo-64, written with PlatformIO. All projects except `First STM32 Test` use the Arduino framework (STM32duino) in C++; `First STM32 Test` uses ST's HAL in C. The projects are split into two folders: `Subsystem Testing/` for bringing up and testing each part on its own (commissioning chapter), and `Control Testing/` for the step tests and closed-loop runs (results chapters).

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

## Control test projects

Each folder in `Control Testing/` is a PlatformIO project used for the system identification and closed-loop tests in the report. The `logs/` folder in each project holds the serial output from those runs.

| Folder | What it does |
|---|---|
| `Temperature Model Test 1` | Open-loop step tests of the wall and glass heaters, used to fit the thermal models |
| `Temperature Model Test` | Closed-loop PI control of the chamber air (PT1000) and the ITO glass, with lambda tuning, setpoint weighting and anti-windup; logs include the step tests and the first closed-loop runs |
| `Temperature and Humidity` | Closed-loop chamber and glass with the heated base at a fixed duty, logging the well water and room temperature |
| `Temperature Heated Base Test` | Closed-loop chamber and glass with the heated base at a manually set duty |
| `Temperature Heated Base eveness` | Plate evenness test, comparing a near-centre well and a corner well with the heated base on |
| `Long Run` | Long humidity and stability run at 39 °C, logging the DHT22 and BMP280; `longRun.log` is the main run |
| `CO2 Step Test` | CO2 valve step test with the ExplorIR-M and SCD30 |

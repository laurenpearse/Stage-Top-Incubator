# Stage-Top Incubator for Live-Cell Imaging of Mycobacteria

EEE4022S final year project (topic RAV2026-07), Department of Electrical Engineering, University of Cape Town, 2026.

**Author:** Lauren Pearse
**Supervisor:** R.A. Verrinder
**Co-supervisor:** Dr M.K. Mason, Molecular Mycobacteriology Research Unit

## About the project

A low-cost stage-top incubator was designed, built and tested for time-lapse imaging of *Mycobacterium smegmatis* in a standard well plate on an inverted microscope. The chamber holds the sample at 37 °C, keeps the humidity high to limit evaporation, and has a CO2 channel for later host-cell infection work. All control runs on a single STM32 microcontroller.

The design was based on first-principles models of the chamber, the heated glass lid and the gas volume. Proportional-integral (PI) controllers were tuned from these models with lambda tuning, then retuned from open-loop step tests that showed strong coupling between the two temperature loops.

## Main results

| Test | Result |
|---|---|
| Chamber air at 37 °C | 37.028 ± 0.038 °C over 80 min |
| Chamber air at 39 °C | within 0.074 °C over 6.5 h |
| Recovery after opening the chamber | back to setpoint in 1.6 min |
| Heated glass lid | 48.00 ± 0.10 °C while the chamber moved by 3.8 °C |
| Sample (centre well) with base heater | within 0.19 °C of 37 °C |
| Humidity with no humidified supply | 77.3 %, against a calculated limit of 78 % |
| Build cost | R 13 527 |

Six of the ten requirements were met and one was partly met. The main findings were:

- **Holding the air at setpoint does not hold the sample at setpoint.** With the air at 37 °C, the water in the wells sat at 34.4 °C. Heating the plate from below fixed this.
- **Humidity is limited by the water temperature.** Air can only be saturated at the temperature of the water beneath it, so cooler wells cap the humidity. Reaching 90 % needs warmer water or a humidified air supply.
- **The CO2 loop was not tested with gas** because the gas connection leaked. A sensitivity study in the report shows what to measure first on the next build.

## Hardware

| Part | Used for |
|---|---|
| STM32L476RG Nucleo-64 | Controller |
| PT1000 + MAX31865 (SPI) | Reference temperature sensor and bench control sensor |
| Four NTC thermistors + ADS1115 (I2C) | Measuring sensors, calibrated against the PT1000 |
| Polyimide film heaters | Chamber wall heating |
| ITO-coated glass | Heated, transparent lid |
| Sensirion SCD30 (I2C) | CO2, temperature and humidity |
| ExplorIR-M (UART) | CO2 |
| DHT22, BMP280 | Humidity and pressure for the long humidity run |
| CO2 cylinder, regulator, 230 V solenoid valve with relay | Gas supply |
| 3D-printed PETG enclosure | Chamber body |

The full bill of materials and wiring list are in the appendices of the report.

## Repository structure

```
firmware/
  subsystem-tests/   One PlatformIO project per sensor or actuator, used for commissioning
  control-tests/     Step tests, closed-loop runs, CO2 test and long run, with serial logs
simulation/          MATLAB and Simulink controller design models
data/                Processed test data and the MATLAB scripts that make the report figures
hardware/
  cad/               Enclosure CAD (STEP)
  bom/               Initial component list (the final bill of materials is in the report)
docs/design-notes/   Working notes on heating, sensor selection and the CO2 path
report/              LaTeX source of the report
```

Each of `firmware/`, `simulation/` and `data/` has its own README listing what every folder contains.

## Building the firmware

1. Install VS Code and the PlatformIO extension.
2. Open one of the project folders in `firmware/subsystem-tests/` or `firmware/control-tests/`.
3. Connect the Nucleo board over USB and run **Build**, then **Upload**.
4. Open the serial monitor at 115 200 baud to see the readings.

Libraries are listed in each project's `platformio.ini` and are downloaded automatically.

## Further work

The main changes recommended for the next build are:

1. Heat the plate from below under closed-loop control, and map the temperature of every well.
2. Supply the humidity through a separate pumped, humidified air circuit.
3. Fix the gas connection and run the CO2 loop with gas, measuring the chamber leak first.
4. Replace the jumper wiring with a printed circuit board before another long run.
5. Revise the enclosure based on the first build.
6. Test on a microscope, including light transmission through the lid and focal drift.
7. Recalibrate the sensors near 37 °C, then image a live culture.

The full report covers the design, models, tests and results in detail. Its LaTeX source is in `report/`.

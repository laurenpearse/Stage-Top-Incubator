# Data

Processed test data from the project, with the MATLAB script that makes the report figures from each data set. Each folder is self-contained: open MATLAB in the folder and run the script, and the figures are saved next to it.

The raw serial logs from each run are kept with the firmware that produced them, in `firmware/Control Testing/<project>/logs/` and `firmware/Subsystem Testing/<project>/logs/`.

| Folder | Report chapter | Contents | Script |
|---|---|---|---|
| `characterization/01-sensor-commissioning` | Commissioning | PT1000 ice bath tests, thermistor calibration against the PT1000, ExplorIR-M and SCD30 breath and room tests | `sensorPlots.m` (figures M1 to M3) |
| `characterization/02-system-identification` | Thermal system identification | Open-loop step tests of the wall heaters and the glass heater | `sysID.m` (figures T1 to T3) |
| `characterization/03-closed-loop` | Closed-loop temperature control | Runs 1 to 4: regulation, setpoint sweep, heated base and plate evenness | `closedLoop.m` (figures C1 to C3) |
| `characterization/04-humidity-long-run` | CO2 and humidity | The 6.5 hour humidity run at 39 °C | `LongRun.m` (figures H1 to H3) |

`04-humidity-long-run/earlier-figures` holds earlier versions of the humidity figures.

# Stage-Top Incubator for Bacterial Microscopy (RAV2026-07)

Final-year mechatronics project: a compact, modifiable stage-top incubator for long-duration, live-cell microscopy of mycobacteria (*M. smegmatis*, with an eye toward future *M. tuberculosis* / host-cell infection work). The system holds a sealed optical chamber at a stable temperature, humidity and CO2 level over multi-day imaging runs, using decoupled PID control on an STM32 platform.

Supervisors: Mandy, Robyn Verrinder.

## Repo structure

```
firmware/     STM32 embedded code (temperature / CO2 / humidity control loops, sensor drivers, logging)
hardware/     Chassis CAD, schematics, bill of materials
docs/         Thesis chapters (incl. literature review) and supporting figures
data/         Characterization results and raw run logs
scripts/      Analysis / plotting scripts for data in data/
notes/        Loose working notes, scratchpad, supervisor meeting notes
```

Each subfolder has its own README with more detail on what goes there and, for firmware/hardware, current status.

## Status

- [x] Literature review drafted
- [ ] Control system architecture (decoupled PID: temp / CO2 / humidity)
- [ ] Firmware
- [ ] Hardware / chassis
- [ ] Characterization (temperature stability, drift, humidity hold, gas accuracy)

## Getting started

Firmware is developed in STM32CubeIDE — open or create the project inside `firmware/`. CAD files live in `hardware/cad/` (note the format/tool used at the top of that folder's README once you pick one). Thesis chapters are plain text/LaTeX in `docs/thesis/chapters/`.

# Firmware

STM32 embedded code for the three environmental control loops (temperature, CO2, humidity) plus sensor drivers and host-side logging.

- `src/` — application source (.c)
- `inc/` — headers (.h)
- `lib/` — third-party or vendor libraries (HAL, sensor SDKs, etc.)

If you create the project in STM32CubeIDE, either point the project location at this folder directly, or generate it elsewhere and copy `Core/Src` → `src/`, `Core/Inc` → `inc/`, `Drivers/` → `lib/` before committing — whichever fits your CubeIDE workflow better. Keep CubeMX-generated build folders (`Debug/`, `Release/`) out of git; they're already covered by the root `.gitignore`.

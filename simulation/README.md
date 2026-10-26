# Simulation

MATLAB and Simulink models used to design the controllers in the theory chapter of the report. Open MATLAB in a folder and run its script; figures are saved to that folder's `figures/` subfolder.

| Folder | Contents |
|---|---|
| `chamber` | Chamber air temperature model and PI loop (`temp_chamber.slx`), frequency analysis (`TempChamberFrequencyAnalysis.m`) and design notes |
| `glass` | ITO glass temperature model and PI loop (`glassTemp.slx`), frequency analysis (`GlassFrequencyAnalysis.m`) and design notes |
| `chamber-and-glass` | Coupled chamber and glass model (`GlassAndChamberNeat.slx` is the final version) and `TemperatureFigures.m`, which makes the temperature control figures (T1 to T9) |
| `co2` | CO2 chamber and valve model (`CO2control.slx`), the controller plan, `CO2figures.m` (figures C1 to C7) and `updatedCO2.m`, which makes the updated CO2 figures used in the report (`figures/updated`, including C8) |

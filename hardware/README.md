# Hardware

Mechanical and electrical design for the incubator chassis and chamber.

- `cad/` — 3D models / assemblies (STEP, and native files for whatever CAD tool you use — Fusion 360, SolidWorks, etc.)
- `schematics/` — electrical schematics (PID board, sensor wiring, heater driver circuits)
- `bom/` — bill of materials (spreadsheet or markdown table of parts, suppliers, costs)

Large native CAD binaries can bloat a git repo fast — if a single file starts pushing past a few tens of MB, consider exporting a lightweight STEP/PDF for the repo and keeping the native source file elsewhere (or set up [Git LFS](https://git-lfs.com) for this folder).

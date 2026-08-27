# Data

- `characterization/` — processed results from characterization runs (temperature stability, focal drift, humidity hold, gas accuracy, PID tuning). One subfolder or clearly-named file set per test.
- `logs/` — raw logged data pulled off the device (CSV/serial logs). Treat these as source data — don't hand-edit them; do cleanup in `scripts/` and save cleaned output back into `characterization/`.

If raw logs get large, consider whether they all need to live in git (a `.gitignore` entry plus a note on where the full logs are archived is often more practical than committing gigabytes of CSV).

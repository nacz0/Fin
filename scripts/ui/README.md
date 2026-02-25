# UI Smoke Harness (PowerShell)

This folder defines a minimal action list used by `scripts/ui_smoke.ps1`.

## Action file

Default file: `scripts/ui/smoke_actions.json`

The file is a JSON array. Each item is one action:

- `{"type":"activate"}`
  - Restores the app window and puts it in foreground.
- `{"type":"sleep","ms":700}`
  - Waits a fixed amount of time in milliseconds.
- `{"type":"screenshot","name":"launch"}`
  - Saves PNG screenshot of the app window.
- `{"type":"click","coordMode":"relative","x":0.55,"y":0.40}`
  - Mouse left click.
  - `coordMode="relative"` means `x` and `y` are in range `0.0..1.0` based on window size.
  - `coordMode="absolute"` means screen pixels.
- `{"type":"keys","value":"{F5}"}`
  - Sends keyboard input with .NET SendKeys syntax.

## Notes

- If Fin layout changes, update click coordinates in `smoke_actions.json`.
- Keep actions short and deterministic for stable smoke tests.
- Screenshots and `summary.json` are generated under `artifacts/ui/<run_id>/`.

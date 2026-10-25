# Repository Guidelines

## Project Overview

Mechanical and hardware design repository for a four-wheel Mecanum-drive vehicle with independent swing-arm suspension: SolidWorks geometry, an STM32F429 control-board design maintained through EasyEDA scripts, and generated documentation. `Code/` stays reserved for firmware.

## Project Structure & Module Organization

- `3D/` — SolidWorks 2016 parts (`.SLDPRT`) and assemblies (`.SLDASM`). `Vehicle Assembled.SLDASM` is the top-level assembly, `Vehicle Assembled (1).STEP` its AP203 export, `IMG_*.jpg` the as-built record. `Vehicle Layout.blend` is the generated board/battery/sensor placement scene (regenerate, do not hand-edit).
- `PCB/docs/` — baselines and review reports; `硬件设计方案评审报告_v1.md` is the main deliverable. `PCB/bom/` holds `BOM_v1.csv` and `引脚分配_v1.csv`.
- `PCB/easyeda/` — numbered scripts (`NN_action.js`) that build the dual-board schematic through the local EasyEDA Bridge; the schematic project itself lives in EasyEDA.
- `PCB/tools/gen_doc_and_dxf.py` — regenerates `doc/` (board-outline DXF, Word specs). Treat `doc/` as generated output.
- `tools/` — `step_to_obj.py` converts STEP to OBJ for Blender; `build_blender_layout.py` rebuilds `3D/Vehicle Layout.blend` and re-runs the interference check.
- `Code/` — reserved for vehicle firmware (still empty).

## Build, Test, and Development Commands

CAD edits happen in SolidWorks 2016; other tools can open only the STEP export. Geometry scripts need OpenCASCADE:

```bash
uv venv --python 3.12 ~/.venvs/mecanum-cad
VIRTUAL_ENV=~/.venvs/mecanum-cad uv pip install cadquery-ocp
~/.venvs/mecanum-cad/bin/python tools/step_to_obj.py "3D/Vehicle Assembled (1).STEP" /tmp/mecanum_cad/vehicle.obj 0.3
~/.venvs/mecanum-cad/bin/python PCB/tools/gen_doc_and_dxf.py
blender -b --factory-startup --python tools/build_blender_layout.py -- /tmp/mecanum_cad/vehicle.obj "3D/Vehicle Layout.blend"
```

EDA work goes through the Bridge (`127.0.0.1:49620`):

```bash
python3 ~/.agents/skills/easyeda-api/scripts/bridge-client.py health
python3 ~/.agents/skills/easyeda-api/scripts/bridge-client.py execute --file PCB/easyeda/60_A_power_entry.js
```

## Coding Style & Naming Conventions

- CAD files: Title Case with spaces and a size affix — `M3x72mm Screw.SLDPRT`.
- Python: 4-space indentation, `snake_case`, docstring ending with a usage example.
- EasyEDA JS: tabs, `camelCase`, top-level `await`, always `return` a result object.
- Docs and BOMs carry a `_vN` suffix — `设计基线对齐_v3.md`.

## Testing Guidelines

No automated tests exist. Verify mechanical changes by rebuilding `Vehicle Assembled.SLDASM`, checking mates and interference, then re-exporting the STEP; attach a screenshot. For EDA work, run `health`/`smoke` and the read-only lookups (`40_lookup_v3_parts.js`) before any write script — `60_A_power_entry.js` is not idempotent.

## Commit & Pull Request Guidelines

The repository is not yet under Git. Use short imperative messages under 72 characters, e.g. `Add M4 hinge pad`. Pull requests state what changed and why, list affected parts or documents, and attach a screenshot or the re-exported STEP. Do not commit SolidWorks backup or lock files.

## Agent-Specific Instructions

Binary CAD, STEP, and JPEG files cannot be diffed; state intent and confirm before altering unverifiable geometry. Never rename or relocate files in `3D/`, since assemblies bind by name. Edit generators in `PCB/tools/` instead of hand-editing `doc/`. The Bridge `/execute` endpoint is not an MCP server; pass `--window <id>` when several EDA windows are open, and keep netlist and DRC calls in separate invocations. The root-level `0819f05c4eef4c71ace90d822a990e87` is an unrelated SQLite cache.

# TDPS Workspace Guide

This workspace is organized by function so new members can quickly locate code, reports, hardware files, and vendor resources.

## Top-level layout

- `mid_reports/`: report automation project (scripts, chart generation, outputs).
- `docs/`: vendor reference packages and manuals (renamed directory layers, original internal files preserved).
- `ref_seu_driver_v2_1_2019/`: Altium hardware design project files.
- `slides/`: course lecture and project support slides.
- `.venv/`: local Python environment.

## Quick start (report build)

1. Go to `mid_reports/`.
2. Run `python scripts/build_all.py`.
3. Check generated outputs in `mid_reports/outputs/final/tdps_initial_design_report_generated.*`.
4. Keep hand-written final report as `mid_reports/outputs/final/tdps_initial_design_report.*`.

## Naming conventions

- Directories and newly managed files use `snake_case`.
- Vendor package internal filenames are kept unchanged to reduce breakage risk.
- Generated deliverables are centralized under `mid_reports/outputs/` with functional subfolders.

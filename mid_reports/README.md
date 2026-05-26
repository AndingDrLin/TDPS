# mid_reports Overview

This folder contains the report engineering pipeline for TDPS.

## Directory map

- `scripts/`: build and generation scripts.
- `assets/images/`: generated and reusable figure assets for report embedding.
- `inputs/source_docs/`: source PDFs used to crop schematic/PCB evidence images.
- `outputs/final/`: final deliverables.
- `outputs/review/`: review figures and page snapshots.
- `outputs/tmp_preview/`: temporary previews.
- `outputs/manual/`: manual insertion helper artifacts.

## Main scripts

- `scripts/build_all.py`: one-command pipeline (charts -> DOCX -> PDF -> validation).
- `scripts/build_docx.py`: generate DOCX report.
- `scripts/export_pdf_from_docx.py`: convert DOCX to PDF using LibreOffice.
- `scripts/prepare_schematic_crops.py`: crop schematic and PCB evidence figures.
- `scripts/generate_charts.py`: generate block diagram, flowchart, and gantt chart.
- `scripts/generate_figure_b.py`: generate performance gate comparison chart for review.
- `scripts/make_gantt.py`: standalone legacy gantt export to review folder.

## Output files

- Preferred hand-written final DOCX: `outputs/final/tdps_initial_design_report.docx`
- Preferred hand-written final PDF: `outputs/final/tdps_initial_design_report.pdf`
- Generated DOCX (from scripts): `outputs/final/tdps_initial_design_report_generated.docx`
- Generated PDF (from scripts): `outputs/final/tdps_initial_design_report_generated.pdf`

## Build command

```bash
python scripts/build_all.py
```

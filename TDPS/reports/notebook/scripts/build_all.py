from __future__ import annotations

from pathlib import Path

from build_notebook_docx import build_notebook_docx
from export_notebook_json import export_json
from generate_notebook_figures import generate_figures


def main() -> None:
    base_dir = Path(__file__).resolve().parent.parent
    image_dir = base_dir / "assets" / "images"
    output_dir = base_dir / "outputs"

    print("Step 1/3: Generating notebook figures...")
    generated = generate_figures(image_dir)
    for name, path in generated.items():
        print(f"- {name}: {path}")

    print("Step 2/3: Building notebook DOCX...")
    output = build_notebook_docx(base_dir)

    print("Step 3/3: Exporting notebook JSON...")
    json_output = export_json(output_dir / "team15_ptsd_lab_notebook.json")

    print(f"Done. Notebook ready: {output}")
    print(f"JSON notebook ready: {json_output}")


if __name__ == "__main__":
    main()

"""Legacy placeholder for PDF build.

Current pipeline requirement from user is:
1) Build DOCX first.
2) Export PDF from DOCX.

Use export_pdf_from_docx.py in build_all.py.
"""

from __future__ import annotations

from pathlib import Path

from build_docx import build_docx_report
from export_pdf_from_docx import export_pdf_from_docx


def build_pdf_report(base_dir: Path, image_dir: Path, output_path: Path | None = None) -> Path:
    """Backward-compatible wrapper that rebuilds DOCX then exports PDF."""
    docx_path = build_docx_report(base_dir, image_dir)
    output_path = output_path or (base_dir / "outputs" / "final" / "tdps_initial_design_report_generated.pdf")
    return export_pdf_from_docx(docx_path, output_path)


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    result = build_pdf_report(root, root / "assets" / "images")
    print(f"PDF generated from DOCX: {result}")

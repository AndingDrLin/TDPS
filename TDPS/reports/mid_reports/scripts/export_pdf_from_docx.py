"""Export PDF from DOCX using LibreOffice headless conversion."""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


def _find_soffice() -> str:
    candidates = [
        shutil.which("soffice"),
        "/Applications/LibreOffice.app/Contents/MacOS/soffice",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return candidate
    raise FileNotFoundError(
        "LibreOffice soffice was not found. Install LibreOffice or add soffice to PATH."
    )


def export_pdf_from_docx(docx_path: Path, output_pdf: Path | None = None) -> Path:
    """Convert DOCX to PDF and return output path."""
    if not docx_path.exists():
        raise FileNotFoundError(f"DOCX file does not exist: {docx_path}")

    soffice = _find_soffice()
    output_dir = output_pdf.parent if output_pdf else docx_path.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    command = [
        soffice,
        "--headless",
        "--convert-to",
        "pdf",
        "--outdir",
        str(output_dir),
        str(docx_path),
    ]
    subprocess.run(command, check=True, capture_output=True)

    converted = output_dir / (docx_path.stem + ".pdf")
    if not converted.exists():
        raise FileNotFoundError("DOCX conversion did not produce PDF output.")

    if output_pdf and converted != output_pdf:
        converted.replace(output_pdf)
        converted = output_pdf

    return converted


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    docx = root / "outputs" / "final" / "tdps_initial_design_report_generated.docx"
    pdf = root / "outputs" / "final" / "tdps_initial_design_report_generated.pdf"
    result = export_pdf_from_docx(docx, pdf)
    print(f"PDF exported from DOCX: {result}")

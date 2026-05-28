"""One-command builder for TDPS initial design report artifacts."""
from __future__ import annotations

from pathlib import Path

from PyPDF2 import PdfReader

from build_docx import build_docx_report
from export_pdf_from_docx import export_pdf_from_docx
from generate_charts import GANTT_END_WEEK, GANTT_START_WEEK, generate_all_charts
from report_content import MAX_TECHNICAL_WORDS, technical_word_count

EXPECTED_PDF_PAGES = 5


def _validate_outputs(docx_path: Path, pdf_path: Path, image_paths: dict[str, Path]) -> None:
    missing = [str(path) for path in [docx_path, pdf_path, *image_paths.values()] if not path.exists()]
    if missing:
        raise FileNotFoundError("Missing generated files:\n" + "\n".join(missing))

    word_count = technical_word_count()
    if word_count > MAX_TECHNICAL_WORDS:
        raise ValueError(f"Technical content word count exceeds limit: {word_count} > {MAX_TECHNICAL_WORDS}")

    reader = PdfReader(str(pdf_path))
    page_count = len(reader.pages)
    if page_count != EXPECTED_PDF_PAGES:
        raise ValueError(f"PDF page count mismatch: expected {EXPECTED_PDF_PAGES}, got {page_count}")

    if not (GANTT_START_WEEK == 1 and GANTT_END_WEEK == 15):
        raise ValueError(f"Gantt week coverage mismatch: {GANTT_START_WEEK} to {GANTT_END_WEEK}")

    print("Validation passed.")
    print(f"Technical word count: {word_count}")
    print(f"PDF page count: {page_count}")
    print(f"Gantt coverage: Week {GANTT_START_WEEK} to Week {GANTT_END_WEEK}")


def main() -> None:
    base_dir = Path(__file__).resolve().parent.parent
    image_dir = base_dir / "assets" / "images"
    output_dir = base_dir / "outputs" / "final"
    output_dir.mkdir(parents=True, exist_ok=True)

    print("Step 1/4: Generating charts...")
    image_paths = generate_all_charts(image_dir, dpi=300)

    print("Step 2/4: Building DOCX...")
    docx_path = build_docx_report(base_dir, image_dir)

    print("Step 3/4: Exporting PDF from DOCX...")
    pdf_path = export_pdf_from_docx(docx_path, output_dir / "tdps_initial_design_report_generated.pdf")

    print("Step 4/4: Validating outputs...")
    _validate_outputs(docx_path, pdf_path, image_paths)

    print("All artifacts generated successfully:")
    print(f"- DOCX: {docx_path}")
    print(f"- PDF : {pdf_path}")
    print(f"- Block diagram: {image_paths['block']}")
    print(f"- Flowchart    : {image_paths['flow']}")
    print(f"- Gantt chart  : {image_paths['gantt']}")


if __name__ == "__main__":
    main()

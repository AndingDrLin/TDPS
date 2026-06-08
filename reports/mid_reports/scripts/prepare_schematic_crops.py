"""Prepare cropped schematic screenshots from full-size PCB schematic images."""
from __future__ import annotations

from pathlib import Path

from PIL import Image

SCH_V1_PDF = "schematic_v1_source_2026_04_13.pdf"
SCH_V2_PDF = "schematic_v2_source_2026_04_13.pdf"
PCB2_PDF = "pcb2_layout_source_2026_04_14.pdf"
SOURCE_DOCS_RELATIVE_PATH = ("inputs", "source_docs")


def _crop_by_ratio(src: Path, dst: Path, box_ratio: tuple[float, float, float, float]) -> Path:
    if not src.exists():
        raise FileNotFoundError(f"Missing schematic source image: {src}")

    with Image.open(src) as im:
        w, h = im.size
        x0, y0, x1, y1 = box_ratio
        box = (
            int(x0 * w),
            int(y0 * h),
            int(x1 * w),
            int(y1 * h),
        )
        cropped = im.crop(box)
        dst.parent.mkdir(parents=True, exist_ok=True)
        cropped.save(dst)
    return dst


def _render_pdf_first_page(pdf_path: Path, dst: Path, dpi: int = 300) -> Path:
    try:
        import fitz  # type: ignore
    except ImportError as exc:
        raise ModuleNotFoundError(
            "PyMuPDF is required to render PDF screenshots. Install with: pip install pymupdf"
        ) from exc

    if not pdf_path.exists():
        raise FileNotFoundError(f"Missing schematic PDF: {pdf_path}")

    with fitz.open(pdf_path) as doc:
        if doc.page_count < 1:
            raise ValueError(f"PDF has no pages: {pdf_path}")
        page = doc.load_page(0)
        scale = dpi / 72.0
        pix = page.get_pixmap(matrix=fitz.Matrix(scale, scale), alpha=False)
        dst.parent.mkdir(parents=True, exist_ok=True)
        pix.save(str(dst))
    return dst


def _trim_near_white_margins(src: Path, dst: Path, threshold: int = 245) -> Path:
    with Image.open(src) as im:
        rgb = im.convert("RGB")
        gray = rgb.convert("L")
        mask = gray.point(lambda px: 255 if px < threshold else 0)
        bbox = mask.getbbox()
        if bbox is None:
            cropped = rgb
        else:
            x0, y0, x1, y1 = bbox
            pad = max(12, int(min(rgb.size) * 0.008))
            x0 = max(0, x0 - pad)
            y0 = max(0, y0 - pad)
            x1 = min(rgb.size[0], x1 + pad)
            y1 = min(rgb.size[1], y1 + pad)
            cropped = rgb.crop((x0, y0, x1, y1))
        dst.parent.mkdir(parents=True, exist_ok=True)
        cropped.save(dst)
    return dst


def _ensure_full_image(
    source_docs_dir: Path,
    target_png: Path,
    fallback_pdf_name: str,
    dpi: int = 300,
) -> Path:
    if target_png.exists():
        return target_png

    source_pdf = source_docs_dir / fallback_pdf_name
    raw_png = target_png.with_name(f"{target_png.stem}_raw.png")
    _render_pdf_first_page(source_pdf, raw_png, dpi=dpi)
    _trim_near_white_margins(raw_png, target_png)
    if raw_png.exists():
        raw_png.unlink()
    return target_png


def prepare_schematic_crops(image_dir: Path) -> dict[str, Path]:
    """Create reusable crops for report embedding from V1/V2 schematics."""
    base_dir = image_dir.parent.parent
    source_docs_dir = base_dir.joinpath(*SOURCE_DOCS_RELATIVE_PATH)
    if not source_docs_dir.exists():
        raise FileNotFoundError(f"Missing source docs directory: {source_docs_dir}")

    sch_dir = image_dir / "schematics"
    src_v1 = _ensure_full_image(source_docs_dir, sch_dir / "schematic_v1_full.png", SCH_V1_PDF)
    src_v2 = _ensure_full_image(source_docs_dir, sch_dir / "schematic_v2_full.png", SCH_V2_PDF)
    src_pcb2 = _ensure_full_image(source_docs_dir, sch_dir / "pcb2_full.png", PCB2_PDF)

    outputs = {
        "v1_power": sch_dir / "schematic_v1_power.png",
        "v1_motor": sch_dir / "schematic_v1_motor_driver.png",
        "v2_power": sch_dir / "schematic_v2_power.png",
        "v2_motor": sch_dir / "schematic_v2_motor_driver.png",
        "v2_interface": sch_dir / "schematic_v2_interface.png",
        "v2_lower": sch_dir / "schematic_v2_lower_panoramic.png",
        "pcb2_layout": sch_dir / "pcb2_layout.png",
    }

    # V1 power: landscape strip — XT60 input, fuse, switch, LM2596 buck converter.
    # Kept wide for balanced side-by-side comparison with V2 power.
    _crop_by_ratio(src_v1, outputs["v1_power"], (0.0, 0.02, 0.50, 0.25))
    # V1 motor driver: TB6612 section (tightened to match V2 motor aspect ratio)
    _crop_by_ratio(src_v1, outputs["v1_motor"], (0.01, 0.46, 0.50, 0.84))
    # V2 power: full-width strip — LM74700 + NTMFS protection, LM2596, AMS1117
    _crop_by_ratio(src_v2, outputs["v2_power"], (0.0, 0.0, 0.95, 0.40))
    # V2 motor driver: dual DRV8874PWPR with encoder feedback
    _crop_by_ratio(src_v2, outputs["v2_motor"], (0.02, 0.34, 0.62, 0.78))
    # V2 interface: connector headers for LoRa/Radar/OLED (trimmed above title block)
    _crop_by_ratio(src_v2, outputs["v2_interface"], (0.61, 0.34, 0.97, 0.72))
    # V2 lower panoramic: motor drivers + interface connectors in one wide shot
    _crop_by_ratio(src_v2, outputs["v2_lower"], (0.02, 0.36, 0.98, 0.78))

    # PCB2 layout — trimmed to the routed board area
    _crop_by_ratio(src_pcb2, outputs["pcb2_layout"], (0.05, 0.05, 0.95, 0.95))

    return outputs


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    paths = prepare_schematic_crops(root / "assets" / "images")
    for key, value in paths.items():
        print(f"Prepared {key}: {value}")

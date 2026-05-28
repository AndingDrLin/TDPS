from __future__ import annotations

import json
from pathlib import Path

from notebook_content import ENTRIES


def _entry_to_markdown_lines(entry: dict) -> list[str]:
    lines: list[str] = []
    lines.append(f"Date: {entry['date']} | Page: {entry['page']}")
    lines.append(f"Title: {entry['title']}")
    lines.append(f"Purpose: {entry['purpose']}")
    lines.append("")

    lines.append("1. Brief Procedure:")
    for item in entry["brief_procedure"]:
        lines.append(f"- {item}")
    lines.append("")

    lines.append("2. Workings & Calculations:")
    for item in entry["workings"]:
        lines.append(f"- {item}")
    lines.append("")

    lines.append("3. Engineering Struggle & Code Implementation:")
    for item in entry["engineering_struggle"]:
        lines.append(f"- {item}")
    lines.append("")

    lines.append("Core snippet:")
    lines.extend([f"    {line}" for line in entry["code_snippet"]])
    lines.append("")

    lines.append("4. Observations and Recorded Data:")
    header = entry["observations_table"]["headers"]
    rows = entry["observations_table"]["rows"]
    lines.append("| " + " | ".join(header) + " |")
    lines.append("| " + " | ".join(["---"] * len(header)) + " |")
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")

    lines.append(f"Figure: {entry['figure']['title']}")
    lines.append("")
    lines.append("5. Conclusion & Next Steps:")
    lines.append(entry["conclusion"])
    return lines


def export_json(out_path: Path) -> Path:
    cells = []

    for entry in ENTRIES:
        cells.append(
            {
                "cell_type": "markdown",
                "metadata": {"language": "markdown"},
                "source": _entry_to_markdown_lines(entry),
            }
        )

    payload = {"cells": cells}
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return out_path


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    output = export_json(root / "outputs" / "team15_ptsd_lab_notebook.json")
    print(f"Generated JSON notebook: {output}")

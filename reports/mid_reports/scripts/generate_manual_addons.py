"""Generate publication-style manual-insert charts for TDPS report enhancement.

This script intentionally keeps only two high-value figures:
1) Methodology innovation figure (automation framework)
2) Performance figure (gates vs measured outcomes)
"""
from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch


def _set_common_style() -> None:
    plt.rcParams.update(
        {
            "font.family": "DejaVu Serif",
            "font.size": 11,
            "axes.titlesize": 14,
            "axes.labelsize": 11,
            "axes.linewidth": 0.8,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "legend.fontsize": 10,
        }
    )


def _add_box(
    ax,
    x: float,
    y: float,
    w: float,
    h: float,
    text: str,
    *,
    fill: str,
    edge: str,
    font: str,
) -> None:
    box = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle="round,pad=0.012,rounding_size=0.02",
        linewidth=1.2,
        edgecolor=edge,
        facecolor=fill,
    )
    ax.add_patch(box)
    ax.text(x + w / 2.0, y + h / 2.0, text, ha="center", va="center", fontsize=10.8, color=font)


def _add_arrow(ax, x0: float, y0: float, x1: float, y1: float) -> None:
    arrow = FancyArrowPatch(
        (x0, y0),
        (x1, y1),
        arrowstyle="-|>",
        mutation_scale=12,
        linewidth=1.2,
        color="#9673A6",
    )
    ax.add_patch(arrow)


def _annotate_bars_centered(ax, bars, notes: list[str]) -> None:
    y_min, y_max = ax.get_ylim()
    span = y_max - y_min
    margin = span * 0.025
    ceiling = y_max - span * 0.04

    for bar, note in zip(bars, notes):
        x_center = bar.get_x() + bar.get_width() / 2.0
        y_text = min(bar.get_height() + margin, ceiling)
        ax.text(
            x_center,
            y_text,
            note,
            ha="center",
            va="bottom",
            fontsize=9.4,
            color="black",
            clip_on=True,
        )


def _save_figure(fig, png_path: Path) -> None:
    png_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(png_path, dpi=600, bbox_inches="tight", facecolor="white")


def generate_validation_framework_chart(output_path: Path) -> Path:
    fig, ax = plt.subplots(figsize=(12.5, 5.1), facecolor="white")
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")

    node_fill = "#E1D5E7"
    node_edge = "#9673A6"
    node_font = "black"
    emphasis_fill = "#DAE8FC"
    emphasis_edge = "#6C8EBF"
    emphasis_font = "black"

    _add_box(
        ax,
        0.03,
        0.58,
        0.18,
        0.25,
        "Controller\nline_follow_v1",
        fill=emphasis_fill,
        edge=emphasis_edge,
        font=emphasis_font,
    )
    _add_box(
        ax,
        0.25,
        0.58,
        0.18,
        0.25,
        "Offline harness\nscenario engine",
        fill=node_fill,
        edge=node_edge,
        font=node_font,
    )
    _add_box(
        ax,
        0.47,
        0.58,
        0.20,
        0.25,
        "Unified CLI\nquick / stability / run-config",
        fill=node_fill,
        edge=node_edge,
        font=node_font,
    )
    _add_box(
        ax,
        0.71,
        0.58,
        0.24,
        0.25,
        "Structured outputs\nquick report v4\nstability summary v1",
        fill=node_fill,
        edge=node_edge,
        font=node_font,
    )

    _add_box(
        ax,
        0.25,
        0.18,
        0.20,
        0.25,
        "Baseline freeze\nregression guard <= 5%",
        fill=node_fill,
        edge=node_edge,
        font=node_font,
    )
    _add_box(
        ax,
        0.49,
        0.18,
        0.22,
        0.25,
        "Quality gates\n82/0.94/0.35\n80/93/0.40",
        fill=emphasis_fill,
        edge=emphasis_edge,
        font=emphasis_font,
    )
    _add_box(
        ax,
        0.75,
        0.18,
        0.20,
        0.25,
        "Decision signal\nexit code 0/1/2",
        fill=node_fill,
        edge=node_edge,
        font=node_font,
    )

    _add_arrow(ax, 0.21, 0.705, 0.25, 0.705)
    _add_arrow(ax, 0.43, 0.705, 0.47, 0.705)
    _add_arrow(ax, 0.67, 0.705, 0.71, 0.705)
    _add_arrow(ax, 0.57, 0.58, 0.57, 0.43)
    _add_arrow(ax, 0.81, 0.58, 0.81, 0.43)
    _add_arrow(ax, 0.45, 0.305, 0.49, 0.305)
    _add_arrow(ax, 0.71, 0.305, 0.75, 0.305)

    fig.tight_layout()
    _save_figure(fig, output_path)
    plt.close(fig)
    return output_path


def generate_performance_comparison_chart(output_path: Path) -> Path:
    higher_labels = ["Quick score", "Quick detect %", "Stability min score", "Stability min detect %"]
    higher_gate = [82.0, 94.0, 80.0, 93.0]
    higher_measured = [92.07, 99.70, 91.28, 99.07]

    lower_labels = ["Quick max lost (s)", "Stability max lost (s)"]
    lower_gate = [0.35, 0.40]
    lower_measured = [0.07, 0.07]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13.6, 5.4), facecolor="white")

    gate_fill = "#FFF2CC"
    gate_edge = "#D6B656"
    higher_fill = "#DAE8FC"
    higher_edge = "#6C8EBF"
    lower_fill = "#D5E8D4"
    lower_edge = "#82B366"

    x1 = range(len(higher_labels))
    width = 0.34
    ax1.bar(
        [i - width / 2 for i in x1],
        higher_gate,
        width=width,
        color=gate_fill,
        edgecolor=gate_edge,
        linewidth=1.1,
        label="Gate",
    )
    higher_measured_bars = ax1.bar(
        [i + width / 2 for i in x1],
        higher_measured,
        width=width,
        color=higher_fill,
        edgecolor=higher_edge,
        linewidth=1.1,
        label="Measured",
    )
    ax1.set_xticks(list(x1))
    ax1.set_xticklabels(higher_labels, rotation=10, ha="right")
    ax1.set_ylabel("Metric value")
    ax1.set_title("Higher-is-better metrics")
    ax1.set_ylim(0, max(max(higher_gate), max(higher_measured)) * 1.14)
    ax1.grid(axis="y", alpha=0.18, color="#6C8EBF", linestyle="--", linewidth=0.8)
    ax1.legend(frameon=False, loc="lower right")

    left_notes = []
    for g, m in zip(higher_gate, higher_measured):
        delta_pct = (m / g - 1.0) * 100.0
        left_notes.append(f"PASS (+{delta_pct:.1f}%)")
    _annotate_bars_centered(ax1, list(higher_measured_bars), left_notes)

    x2 = range(len(lower_labels))
    ax2.bar(
        [i - width / 2 for i in x2],
        lower_gate,
        width=width,
        color=gate_fill,
        edgecolor=gate_edge,
        linewidth=1.1,
        label="Gate",
    )
    lower_measured_bars = ax2.bar(
        [i + width / 2 for i in x2],
        lower_measured,
        width=width,
        color=lower_fill,
        edgecolor=lower_edge,
        linewidth=1.1,
        label="Measured",
    )
    ax2.set_xticks(list(x2))
    ax2.set_xticklabels(lower_labels, rotation=10, ha="right")
    ax2.set_ylabel("Seconds")
    ax2.set_title("Lower-is-better metrics")
    ax2.set_ylim(0, max(max(lower_gate), max(lower_measured)) * 1.24)
    ax2.grid(axis="y", alpha=0.18, color="#82B366", linestyle="--", linewidth=0.8)
    ax2.legend(frameon=False, loc="upper right")

    right_notes = []
    for g, m in zip(lower_gate, lower_measured):
        right_notes.append(f"PASS (x{g / m:.1f})")
    _annotate_bars_centered(ax2, list(lower_measured_bars), right_notes)

    fig.tight_layout()
    _save_figure(fig, output_path)
    plt.close(fig)
    return output_path


def generate_all_manual_addons(base_dir: Path) -> dict[str, Path]:
    _set_common_style()
    output_dir = base_dir / "assets" / "images"
    output_dir.mkdir(parents=True, exist_ok=True)

    outputs = {
        "framework": output_dir / "validation_framework.png",
        "performance": output_dir / "performance_gates.png",
    }

    generate_validation_framework_chart(outputs["framework"])
    generate_performance_comparison_chart(outputs["performance"])
    return outputs


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    generated = generate_all_manual_addons(root)
    print("Generated manual report addon charts:")
    for name, path in generated.items():
        print(f"- {name}: {path}")

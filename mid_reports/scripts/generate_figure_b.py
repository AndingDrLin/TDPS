"""Generate Figure B: performance against acceptance gates (phase-0 baseline)."""
from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def _set_global_style() -> None:
    """Apply publication-level style configuration."""
    plt.rcParams.update(
        {
            "figure.dpi": 300,
            "savefig.dpi": 300,
            "font.family": "serif",
            "font.serif": ["Times New Roman", "Times", "DejaVu Serif"],
            "font.sans-serif": ["Arial", "DejaVu Sans"],
            "font.size": 11,
            "xtick.labelsize": 11,
            "ytick.labelsize": 11,
            "axes.labelsize": 13,
            "axes.titlesize": 14,
            "figure.titlesize": 14,
        }
    )


def _style_axis(ax: plt.Axes) -> None:
    """Apply shared axis-level style settings."""
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(axis="y", color="#6C8EBF", linestyle="--", linewidth=0.8, alpha=0.18)
    ax.set_axisbelow(True)


def _annotate_measured_bars(ax: plt.Axes, bars: list[plt.Rectangle], notes: list[str]) -> None:
    """Annotate each measured bar with a PASS label and fixed vertical margin."""
    y_min, y_max = ax.get_ylim()
    margin = (y_max - y_min) * 0.02
    ceiling = y_max - (y_max - y_min) * 0.04
    for bar, note in zip(bars, notes):
        x_center = bar.get_x() + bar.get_width() / 2
        y_top = bar.get_height()
        ax.text(
            x_center,
            min(y_top + margin, ceiling),
            note,
            fontsize=10,
            color="black",
            ha="center",
            va="bottom",
            clip_on=True,
        )


def generate_figure_b(output_path: Path) -> Path:
    """Create the twin grouped bar chart for acceptance-gate performance."""
    _set_global_style()

    fig, axes = plt.subplots(1, 2, figsize=(12.4, 5.7))
    plt.subplots_adjust(wspace=0.25, bottom=0.2)

    gate_fill = "#FFF2CC"
    gate_edge = "#D6B656"
    left_measured_fill = "#DAE8FC"
    left_measured_edge = "#6C8EBF"
    right_measured_fill = "#D5E8D4"
    right_measured_edge = "#82B366"
    width = 0.35

    left_labels = [
        "Quick score",
        "Quick detect %",
        "Stability min score",
        "Stability min detect %",
    ]
    left_gate = [82, 94, 80, 93]
    left_measured = [92.1, 99.7, 91.3, 99.0]
    left_notes = ["PASS (+12.3%)", "PASS (+6.1%)", "PASS (+14.1%)", "PASS (+6.5%)"]

    x_left = np.arange(len(left_labels))
    axes[0].bar(
        x_left - width / 2,
        left_gate,
        width,
        label="Gate",
        color=gate_fill,
        edgecolor=gate_edge,
        linewidth=1.2,
    )
    left_measured_bars = axes[0].bar(
        x_left + width / 2,
        left_measured,
        width,
        label="Measured",
        color=left_measured_fill,
        edgecolor=left_measured_edge,
        linewidth=1.2,
    )

    axes[0].set_title("Higher-is-better metrics")
    axes[0].set_ylabel("Metric value (%)")
    axes[0].set_xticks(x_left)
    axes[0].set_xticklabels(left_labels, rotation=10, ha="right")
    axes[0].set_ylim(0, max(max(left_gate), max(left_measured)) * 1.13)
    _style_axis(axes[0])
    _annotate_measured_bars(axes[0], list(left_measured_bars), left_notes)
    axes[0].legend(loc="lower right", frameon=True, fancybox=False, edgecolor="black")

    right_labels = ["Quick max lost (s)", "Stability max lost (s)"]
    right_gate = [0.35, 0.40]
    right_measured = [0.07, 0.07]
    right_notes = ["PASS (x5.0)", "PASS (x5.7)"]

    x_right = np.arange(len(right_labels))
    axes[1].bar(
        x_right - width / 2,
        right_gate,
        width,
        label="Gate",
        color=gate_fill,
        edgecolor=gate_edge,
        linewidth=1.2,
    )
    right_measured_bars = axes[1].bar(
        x_right + width / 2,
        right_measured,
        width,
        label="Measured",
        color=right_measured_fill,
        edgecolor=right_measured_edge,
        linewidth=1.2,
    )

    axes[1].set_title("Lower-is-better metrics")
    axes[1].set_ylabel("Time (seconds)")
    axes[1].set_xticks(x_right)
    axes[1].set_xticklabels(right_labels, rotation=10, ha="right")
    axes[1].set_ylim(0, max(max(right_gate), max(right_measured)) * 1.20)
    _style_axis(axes[1])
    _annotate_measured_bars(axes[1], list(right_measured_bars), right_notes)
    axes[1].legend(loc="upper right", frameon=True, fancybox=False, edgecolor="black")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)
    return output_path


if __name__ == "__main__":
    base_dir = Path(__file__).resolve().parent.parent
    destination = base_dir / "outputs" / "review" / "figure_b_performance_gates.png"
    generated = generate_figure_b(destination)
    print(f"Generated Figure B: {generated}")
from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from notebook_content import ENTRIES


def _apply_style() -> None:
    plt.rcParams.update(
        {
            "font.family": "DejaVu Sans",
            "axes.facecolor": "#FFFFFF",
            "figure.facecolor": "#FFFFFF",
            "axes.edgecolor": "#444444",
            "axes.labelcolor": "#222222",
            "xtick.color": "#222222",
            "ytick.color": "#222222",
            "grid.color": "#D7D7D7",
        }
    )


def _save(fig: plt.Figure, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(out_path, dpi=220, bbox_inches="tight")
    plt.close(fig)


def _plot_rough_metrics(data: dict, out_path: Path) -> None:
    labels = data["labels"]
    detect = np.array(data["detect_pct"], dtype=float)
    lost = np.array(data["max_lost_s"], dtype=float)
    x = np.arange(len(labels))

    fig, ax1 = plt.subplots(figsize=(8.6, 4.6))
    bars = ax1.bar(x, detect, width=0.56, color="#2A9D8F", label="Detection rate")
    ax1.set_ylabel("Line Detection Rate (%)")
    ax1.set_ylim(75, 100)
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels)
    ax1.grid(axis="y", linestyle="--", linewidth=0.8, alpha=0.7)

    for b, val in zip(bars, detect):
        ax1.text(b.get_x() + b.get_width() / 2.0, val + 0.4, f"{val:.1f}", ha="center", va="bottom", fontsize=9)

    ax2 = ax1.twinx()
    ax2.plot(x, lost, color="#E76F51", marker="o", linewidth=2.0, label="Longest lost")
    ax2.set_ylabel("Longest Lost (s)")
    ax2.set_ylim(0.0, max(1.2, float(lost.max()) + 0.1))

    for idx, val in enumerate(lost):
        ax2.text(idx, val + 0.03, f"{val:.2f}", color="#E76F51", ha="center", va="bottom", fontsize=9)

    fig.suptitle("Early rough-loop metrics before robust recovery tuning", fontsize=12, fontweight="bold")
    _save(fig, out_path)


def _plot_tuning_tradeoff(data: dict, out_path: Path) -> None:
    labels = data["labels"]
    score = np.array(data["score"], dtype=float)
    lost = np.array(data["lost_s"], dtype=float)
    x = np.arange(len(labels))

    fig, ax1 = plt.subplots(figsize=(8.2, 4.5))
    bars = ax1.bar(x, score, width=0.52, color="#457B9D", label="Score")
    ax1.set_ylabel("Score (0-100)")
    ax1.set_ylim(55, 90)
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels)
    ax1.grid(axis="y", linestyle=":", linewidth=0.8)

    for b, val in zip(bars, score):
        ax1.text(b.get_x() + b.get_width() / 2.0, val + 0.5, f"{val:.1f}", ha="center", va="bottom", fontsize=9)

    ax2 = ax1.twinx()
    ax2.plot(x, lost, color="#D62828", marker="D", linewidth=2.0, label="Longest lost")
    ax2.set_ylabel("Longest Lost (s)")
    ax2.set_ylim(0.0, 0.9)

    for idx, val in enumerate(lost):
        ax2.text(idx, val + 0.03, f"{val:.2f}", color="#D62828", ha="center", fontsize=9)

    fig.suptitle("Tuning trade-off: stronger score vs recovery lag risk", fontsize=12, fontweight="bold")
    _save(fig, out_path)


def _plot_gate_comparison(data: dict, out_path: Path) -> None:
    labels = data["labels"]
    score = np.array(data["score"], dtype=float)
    detect = np.array(data["detect_pct"], dtype=float)
    lost = np.array(data["lost_s"], dtype=float)
    x = np.arange(len(labels))

    fig, axes = plt.subplots(1, 3, figsize=(12.4, 4.1), sharex=True)

    axes[0].bar(x, score, color="#4D908E")
    axes[0].axhline(float(data["score_gate"]), color="#B5179E", linestyle="--", linewidth=1.4)
    axes[0].set_title("Score")
    axes[0].set_ylabel("0-100")
    axes[0].set_ylim(70, 100)

    axes[1].bar(x, detect, color="#577590")
    axes[1].axhline(float(data["detect_gate"]), color="#B5179E", linestyle="--", linewidth=1.4)
    axes[1].set_title("Detection")
    axes[1].set_ylabel("%")
    axes[1].set_ylim(90, 101)

    axes[2].bar(x, lost, color="#F3722C")
    axes[2].axhline(float(data["lost_gate"]), color="#B5179E", linestyle="--", linewidth=1.4)
    axes[2].set_title("Max Lost")
    axes[2].set_ylabel("s")
    axes[2].set_ylim(0.0, 0.5)

    for ax in axes:
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=28, ha="right")
        ax.grid(axis="y", linestyle=":", linewidth=0.7)

    fig.suptitle("Gate-focused validation before and after harness-driven fixes", fontsize=12, fontweight="bold")
    _save(fig, out_path)


def _plot_final_vs_previous(data: dict, out_path: Path) -> None:
    labels = data["labels"]
    v08 = np.array(data["v08"], dtype=float)
    v10 = np.array(data["v10"], dtype=float)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11.2, 4.6))

    # Higher is better: score and detection.
    high_labels = labels[:2]
    xh = np.arange(len(high_labels))
    width = 0.33
    ax1.bar(xh - width / 2.0, v08[:2], width=width, color="#8D99AE", label="v0.8")
    ax1.bar(xh + width / 2.0, v10[:2], width=width, color="#2A9D8F", label="v1.0")
    ax1.set_xticks(xh)
    ax1.set_xticklabels(high_labels)
    ax1.set_ylabel("Value")
    ax1.set_title("Higher-is-better metrics")
    ax1.grid(axis="y", linestyle="--", linewidth=0.7)
    ax1.legend(loc="lower right")

    # Lower is better: lost, saturation, rms error.
    low_labels = labels[2:]
    xl = np.arange(len(low_labels))
    ax2.bar(xl - width / 2.0, v08[2:], width=width, color="#8D99AE", label="v0.8")
    ax2.bar(xl + width / 2.0, v10[2:], width=width, color="#E76F51", label="v1.0")
    ax2.set_xticks(xl)
    ax2.set_xticklabels(low_labels)
    ax2.set_ylabel("Value")
    ax2.set_title("Lower-is-better metrics")
    ax2.grid(axis="y", linestyle="--", linewidth=0.7)

    fig.suptitle("Final tuning decision from seeded regression data", fontsize=12, fontweight="bold")
    _save(fig, out_path)


def generate_figures(image_dir: Path) -> dict[str, Path]:
    _apply_style()
    outputs: dict[str, Path] = {}

    for entry in ENTRIES:
        fig_info = entry["figure"]
        out_path = image_dir / fig_info["file"]
        fig_type = fig_info["type"]
        data = fig_info["data"]

        if fig_type == "rough_metrics":
            _plot_rough_metrics(data, out_path)
        elif fig_type == "tuning_tradeoff":
            _plot_tuning_tradeoff(data, out_path)
        elif fig_type == "gate_comparison":
            _plot_gate_comparison(data, out_path)
        elif fig_type == "final_vs_previous":
            _plot_final_vs_previous(data, out_path)
        else:
            raise ValueError(f"Unsupported figure type: {fig_type}")

        outputs[fig_info["file"]] = out_path

    return outputs


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    generated = generate_figures(root / "assets" / "images")
    for name, path in generated.items():
        print(f"Generated {name}: {path}")

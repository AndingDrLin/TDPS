"""Generate updated Gantt chart for TDPS project (Report 02)."""
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import Patch, FancyBboxPatch
from matplotlib.lines import Line2D

# (label, planned_start, planned_end, actual_start, actual_end, progress%)
tasks = [
    ("Requirements & Planning",       1, 2, 1, 2, 100),
    ("System Architecture",           2, 3, 2, 3, 100),
    ("Electrical & PCB Design",       3, 4, 3, 4, 100),
    ("Control Strategy Design",       3, 4, 3, 4, 100),
    ("BOM & Procurement",             3, 5, 3, 6, 100),  # parts arrived 1 wk late
    ("Hardware Build & Basic Motion", 4, 7, 5, 7, 50),   # started 1 wk late, compressed
    ("Task 1: Line Following",        5, 8, 5, 8, 30),
    ("Task 2: LoRa Communication",    6, 9, 6, 9, 10),
    ("Task 3: Radar Detection",       6, 9, 6, 9, 10),
    ("System Integration",            8, 10, 8, 10, 0),
    ("Testing, Tuning & Validation",  7, 12, 7, 12, 0),
    ("Documentation & Report Update", 8, 12, 8, 12, 0),
]

# milestones: (label, planned_week, updated_week, status)
milestones = [
    ("M1\nPlanning\nbaseline",     2,  2, "Done"),
    ("M2\nDesign\nfreeze",         4,  4, "Done"),
    ("M3\nFunctional\nrobot",      8,  8, "On Track"),
    ("M4\nEnd-to-end\nvalidation", 12, 12, "On Track"),
]

today = 6.0  # current week

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "axes.edgecolor": "#888888",
})

fig, ax = plt.subplots(figsize=(14, 8.4))

y_positions = list(range(len(tasks)))[::-1]

color_planned  = "#E3ECF7"
color_planned_e = "#7B8FA8"
color_done     = "#3F8B3D"
color_remain   = "#B5B5B5"
color_today    = "#E07B00"

# alternating row bands
for i, y in enumerate(y_positions):
    if i % 2 == 0:
        ax.axhspan(y - 0.5, y + 0.5, color="#F6F7FA", zorder=0)

for y, t in zip(y_positions, tasks):
    label, ps, pe, as_, ae, prog = t
    # planned window (background)
    ax.barh(y, pe - ps, left=ps, height=0.62,
            color=color_planned, edgecolor=color_planned_e,
            linewidth=0.8, zorder=2)
    # actual bar with progress fill
    dur = ae - as_
    done_w = dur * prog / 100
    if done_w > 0:
        ax.barh(y, done_w, left=as_, height=0.40,
                color=color_done, edgecolor="none", zorder=3)
    if dur - done_w > 0:
        ax.barh(y, dur - done_w, left=as_ + done_w, height=0.40,
                color=color_remain, edgecolor="none", zorder=3)
    # progress label
    ax.text(ae + 0.10, y, f"{prog}%", va="center", ha="left",
            fontsize=8.5, color="#333333", fontweight="bold")
    # slip indicator: dashed connector from planned end to actual end
    if ae != pe:
        ax.annotate("", xy=(ae, y - 0.32), xytext=(pe, y - 0.32),
                    arrowprops=dict(arrowstyle="->",
                                    color="#C62828", lw=1.0,
                                    linestyle=(0, (3, 2))))

# milestones row
ms_colors = {"Done": "#2E7D32", "On Track": "#1565C0", "Delayed": "#C62828"}
ms_y = -1.6
for label, pw, uw, status in milestones:
    # planned ghost marker if shifted
    if pw != uw:
        ax.scatter(pw, ms_y, marker="D", s=80,
                   facecolor="white", edgecolor=ms_colors[status],
                   linewidth=1.2, zorder=4)
        ax.annotate("", xy=(uw, ms_y), xytext=(pw, ms_y),
                    arrowprops=dict(arrowstyle="->",
                                    color=ms_colors[status], lw=1.2))
    ax.scatter(uw, ms_y, marker="D", s=150,
               color=ms_colors[status], edgecolor="black",
               linewidth=0.8, zorder=5)
    ax.text(uw, ms_y - 0.95, label, ha="center", va="top",
            fontsize=7.8, color="#222", linespacing=1.15)

# today line
ax.axvline(today, color=color_today, linestyle="--", linewidth=1.8, zorder=6)
ax.text(today + 0.08, len(tasks) - 0.55, "Today (end of W6)",
        color="#A85A00", fontsize=9.5, fontweight="bold",
        va="bottom", ha="left",
        bbox=dict(boxstyle="round,pad=0.25", fc="#FFF4E0",
                  ec=color_today, lw=0.8))

# axes
ax.set_yticks(y_positions)
ax.set_yticklabels([t[0] for t in tasks], fontsize=9.5)
ax.set_xticks(range(1, 13))
ax.set_xticklabels([f"Week {i}" for i in range(1, 13)], fontsize=9)
ax.set_xlim(0.4, 13.0)
ax.set_ylim(-4.6, len(tasks) - 0.2)
ax.xaxis.tick_top()
ax.xaxis.set_label_position("top")
ax.tick_params(axis="x", length=0, pad=6)
ax.tick_params(axis="y", length=0)
ax.set_axisbelow(True)
ax.grid(axis="x", linestyle=":", color="#BFBFBF", linewidth=0.7, zorder=1)
for s in ["top", "right", "left", "bottom"]:
    ax.spines[s].set_color("#888")
    ax.spines[s].set_linewidth(0.8)

ax.set_title("TDPS Project — Updated Gantt Chart  (Week 1 – Week 12)",
             fontsize=15, fontweight="bold", pad=20, color="#1A1A1A")

# legend below the chart
legend_items = [
    Patch(facecolor=color_planned, edgecolor=color_planned_e, label="Planned window"),
    Patch(facecolor=color_done,    label="Completed work"),
    Patch(facecolor=color_remain,  label="Remaining work"),
    Line2D([0], [0], color=color_today, linestyle="--", lw=1.8, label="Today"),
    Line2D([0], [0], marker="D", color="w",
           markerfacecolor=ms_colors["Done"], markersize=10,
           markeredgecolor="black", label="Milestone — Done"),
    Line2D([0], [0], marker="D", color="w",
           markerfacecolor=ms_colors["On Track"], markersize=10,
           markeredgecolor="black", label="Milestone — On Track"),
    Line2D([0], [0], marker="D", color="w",
           markerfacecolor=ms_colors["Delayed"], markersize=10,
           markeredgecolor="black", label="Milestone — Delayed"),
]
ax.legend(handles=legend_items,
          loc="upper center", bbox_to_anchor=(0.5, -0.04),
          fontsize=8.8, frameon=True, ncol=7,
          framealpha=0.95, edgecolor="#999")

plt.tight_layout()
base_dir = Path(__file__).resolve().parent.parent
out = base_dir / "outputs" / "review" / "updated_gantt_chart_legacy.png"
out.parent.mkdir(parents=True, exist_ok=True)
plt.savefig(out, dpi=240, bbox_inches="tight", facecolor="white")
print("saved", out)

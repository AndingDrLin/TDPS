"""Generate publication-quality diagrams for the TDPS initial design report."""
from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
from graphviz import Digraph
from matplotlib.lines import Line2D
from matplotlib.patches import Patch

GANTT_START_WEEK = 1
GANTT_END_WEEK = 15
CURRENT_WEEK = 6.5

BLOCK_IMAGE_NAME = "block_diagram.png"
FLOW_IMAGE_NAME = "flowchart.png"
GANTT_IMAGE_NAME = "gantt_chart.png"

POWER_NODE_STYLE = {
    "shape": "box",
    "style": "filled,rounded",
    "fillcolor": "#FFF2CC",
    "color": "#D6B656",
    "fontname": "Helvetica",
}

MCU_NODE_STYLE = {
    "shape": "box",
    "style": "filled,rounded",
    "fillcolor": "#DAE8FC",
    "color": "#6C8EBF",
    "fontname": "Helvetica",
}

ACTUATOR_NODE_STYLE = {
    "shape": "box",
    "style": "filled,rounded",
    "fillcolor": "#F8CECC",
    "color": "#B85450",
    "fontname": "Helvetica",
}

SENSOR_NODE_STYLE = {
    "shape": "box",
    "style": "filled,rounded",
    "fillcolor": "#D5E8D4",
    "color": "#82B366",
    "fontname": "Helvetica",
}


def _render_graph(graph: Digraph, output_path: Path) -> Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    graph.directory = str(output_path.parent)
    graph.filename = output_path.stem
    rendered = Path(graph.render(cleanup=True))
    if rendered != output_path and rendered.exists():
        rendered.replace(output_path)
    return output_path


def generate_block_diagram(output_path: Path, dpi: int = 300) -> None:
    """Generate system block diagram using graphviz with strict color rules."""
    graph = Digraph(name="tdps_block", format="png", engine="dot")
    graph.attr(
        rankdir="TB",
        splines="ortho",
        bgcolor="white",
        pad="0.15",
        nodesep="0.40",
        ranksep="0.45",
        dpi=str(dpi),
        fontname="Helvetica",
        labelloc="t",
        label="",
        fontsize="16",
    )

    with graph.subgraph(name="cluster_power") as power:
        power.attr(label="Power Path", color="#D6B656", fontname="Helvetica-Bold", fontsize="14")
        power.node("battery", "Battery\n11.1V Li-ion", **POWER_NODE_STYLE)
        power.node("protection", "Power Protection\nNTMFS5C628NLT1G + LM74700", **POWER_NODE_STYLE)
        power.node("buck5v", "LM2596S\n5V Rail", **POWER_NODE_STYLE)
        power.node("ldo33", "AMS1117-3.3\n3.3V Rail", **POWER_NODE_STYLE)

        with power.subgraph(name="power_rank") as power_rank:
            power_rank.attr(rank="same")
            power_rank.node("battery")
            power_rank.node("protection")
            power_rank.node("buck5v")
            power_rank.node("ldo33")

        power.edge("battery", "protection", color="#D6B656", penwidth="2.0", constraint="true")
        power.edge("protection", "buck5v", color="#D6B656", penwidth="2.0", constraint="true")
        power.edge("buck5v", "ldo33", color="#D6B656", penwidth="2.0", constraint="true")

    with graph.subgraph(name="cluster_data") as data:
        data.attr(label="Data / Control Path", color="#6C8EBF", fontname="Helvetica-Bold", fontsize="14")
        data.node("sensors", "Line Sensors\n+ Encoders", **SENSOR_NODE_STYLE)
        data.node("mcu", "STM32F407 MCU", **MCU_NODE_STYLE)
        data.node("driver", "2x DRV8874PWPR\nMotor Driver", **ACTUATOR_NODE_STYLE)
        data.node("motors", "DC Motors\n+ Chassis", **ACTUATOR_NODE_STYLE)
        data.node("lora", "LoRa Module\n(Mandatory)", **SENSOR_NODE_STYLE)
        data.node("radar", "Radar Module\n(Mandatory)", **SENSOR_NODE_STYLE)

        with data.subgraph(name="data_rank_main") as main_rank:
            main_rank.attr(rank="same")
            main_rank.node("sensors")
            main_rank.node("mcu")
            main_rank.node("driver")
            main_rank.node("motors")

        with data.subgraph(name="data_rank_radio") as radio_rank:
            radio_rank.attr(rank="same")
            radio_rank.node("lora")
            radio_rank.node("radar")

        data.edge("sensors", "mcu", color="#6C8EBF", penwidth="1.8")
        data.edge("mcu", "driver", color="#6C8EBF", penwidth="1.8")
        data.edge("driver", "motors", color="#B85450", penwidth="1.8")
        data.edge("mcu", "lora", color="#82B366", penwidth="1.8")
        data.edge("mcu", "radar", color="#82B366", penwidth="1.8")

    graph.edge("buck5v", "driver", color="#D6B656", penwidth="1.8", constraint="false")
    graph.edge("ldo33", "mcu", color="#D6B656", penwidth="1.8", constraint="false")
    graph.edge("ldo33", "lora", color="#D6B656", penwidth="1.8", constraint="false")
    graph.edge("ldo33", "radar", color="#D6B656", penwidth="1.8", constraint="false")

    # Keep data-path layout stable: sensors on left, MCU center, motors/radio on right.
    graph.edge("sensors", "mcu", style="invis", weight="8")
    graph.edge("mcu", "driver", style="invis", weight="8")
    graph.edge("driver", "motors", style="invis", weight="8")
    graph.edge("lora", "radar", style="invis", weight="4")

    _render_graph(graph, output_path)


def generate_flowchart(output_path: Path, dpi: int = 300) -> None:
    """Generate software flowchart using graphviz with exact required labels."""
    graph = Digraph(name="tdps_flow", format="png", engine="dot")
    graph.attr(
        rankdir="LR",
        newrank="true",
        splines="ortho",
        bgcolor="white",
        pad="0.03",
        nodesep="0.10",
        ranksep="0.14",
        dpi=str(dpi),
        fontname="Helvetica",
        labelloc="t",
        label="",
        fontsize="14",
    )

    process_style = {
        "shape": "box",
        "style": "filled,rounded",
        "fillcolor": "#E1D5E7",
        "color": "#9673A6",
        "fontname": "Helvetica",
        "fontsize": "10",
        "height": "0.34",
        "margin": "0.08,0.05",
    }
    decision_style = {
        "shape": "diamond",
        "style": "filled",
        "fillcolor": "#FFE6CC",
        "color": "#D79B00",
        "fontname": "Helvetica",
        "fontsize": "9",
        "margin": "0.05,0.03",
    }

    graph.node("init", "Init\nPeripherals", **process_style)
    graph.node("pid", "PID Line\nFollowing", **process_style)
    graph.node("radar_decision", "Radar Obstacle\nDetected?", **decision_style)
    graph.node("safe", "Safe Stop /\nReplan", **process_style)
    graph.node("lora", "LoRa Telemetry\nTx", **process_style)
    graph.node("resume", "Resume\nTracking", **process_style)

    graph.edge("init", "pid", color="#9673A6", penwidth="1.8")
    graph.edge("pid", "radar_decision", color="#9673A6", penwidth="1.8")
    graph.edge("radar_decision", "safe", xlabel="\nYes", color="#D79B00", penwidth="1.8")
    graph.edge("safe", "lora", color="#9673A6", penwidth="1.8")
    graph.edge("radar_decision", "lora", xlabel="No", color="#D79B00", penwidth="1.8", constraint="false")
    graph.edge("lora", "resume", color="#9673A6", penwidth="1.8")
    graph.edge("resume", "pid", xlabel="Loop", color="#9673A6", style="dashed", penwidth="1.6", constraint="false")

    _render_graph(graph, output_path)


def generate_gantt_chart(output_path: Path, dpi: int = 300) -> None:
    """Generate Week 1-15 gantt chart with strict white-theme styling."""
    plt.rcParams.update({"font.family": "DejaVu Sans"})
    fig, ax = plt.subplots(figsize=(13.8, 7.2), facecolor="white")
    ax.set_facecolor("white")

    tasks = [
        ("Requirements & System Architecture", 1, 2, 100),
        ("Electrical & PCB Design V1 & V2", 3, 5, 100),
        ("BOM & Procurement", 4, 6, 100),
        ("Task 1: Line Following on Patio", 5, 8, 55),
        ("Task 2 & 3: LoRa & Radar Integration", 6, 9, 35),
        ("System Integration & Testing", 8, 12, 15),
        ("Final System Debugging & Patio Trials", 11, 14, 5),
        ("Final Report & Presentation Preparation", 14, 15, 0),
    ]

    milestones = [
        ("M1", 2, "Planning"),
        ("M2", 5, "Design Freeze"),
        ("M3", 9, "Functional Robot"),
        ("M4", 14, "End-to-End Validation"),
        ("M5", 15, "Final Delivery"),
    ]

    completed_color = "#2CA02C"
    progress_color = "#FF7F0E"
    future_color = "#7F7F7F"

    y_positions = list(range(len(tasks)))[::-1]
    bar_height = 0.58

    for y, (label, start, end, progress) in zip(y_positions, tasks):
        width = end - start
        if progress >= 100:
            color = completed_color
            status = "Completed"
        elif progress > 0:
            color = progress_color
            status = "In Progress"
        else:
            color = future_color
            status = "Future"

        ax.barh(y, width, left=start, height=bar_height, color=color, edgecolor="none", zorder=3)
        ax.text(end + 0.10, y, f"{progress}%", va="center", ha="left", fontsize=9, color="#222222", fontweight="bold")
        ax.text(start + 0.06, y, status, va="center", ha="left", fontsize=8, color="white", fontweight="bold")

    milestone_y = -1.10
    for m_name, week, m_label in milestones:
        ax.scatter(week, milestone_y, marker="D", s=95, color="#1F4E79", edgecolor="black", linewidth=0.7, zorder=4)
        ax.text(week, milestone_y - 0.62, f"{m_name}\n{m_label}", ha="center", va="top", fontsize=8, color="#222222")

    ax.axvline(CURRENT_WEEK, color="#CC3300", linestyle="--", linewidth=1.6, zorder=5)
    ax.text(
        CURRENT_WEEK + 0.08,
        len(tasks) - 0.45,
        "Current Week (W6/7)",
        color="#CC3300",
        fontsize=9,
        fontweight="bold",
        va="bottom",
    )

    ax.set_yticks(y_positions)
    ax.set_yticklabels([t[0] for t in tasks], fontsize=10)
    ax.set_xticks(range(GANTT_START_WEEK, GANTT_END_WEEK + 1))
    ax.set_xticklabels([f"Week {w}" for w in range(GANTT_START_WEEK, GANTT_END_WEEK + 1)], fontsize=9)
    ax.set_xlim(0.6, GANTT_END_WEEK + 0.9)
    ax.set_ylim(-2.6, len(tasks) - 0.2)

    ax.grid(axis="x", color="#BFBFBF", linestyle="-", linewidth=0.8, alpha=0.3)
    ax.set_axisbelow(True)
    ax.tick_params(axis="x", length=0, pad=6)
    ax.tick_params(axis="y", length=0)

    for spine in ["top", "right", "left", "bottom"]:
        ax.spines[spine].set_color("#D0D0D0")
        ax.spines[spine].set_linewidth(0.8)

    ax.set_title("", fontsize=1, pad=2)

    legend_items = [
        Patch(facecolor=completed_color, edgecolor="none", label="Completed"),
        Patch(facecolor=progress_color, edgecolor="none", label="In Progress"),
        Patch(facecolor=future_color, edgecolor="none", label="Future"),
        Line2D([0], [0], color="#CC3300", linestyle="--", linewidth=1.6, label="Current Week"),
    ]
    ax.legend(
        handles=legend_items,
        loc="upper center",
        bbox_to_anchor=(0.5, -0.06),
        ncol=4,
        frameon=False,
        fontsize=9,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_path, dpi=dpi, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def generate_all_charts(image_dir: Path, dpi: int = 300) -> dict[str, Path]:
    """Generate block diagram, software flowchart, and gantt chart images."""
    image_dir.mkdir(parents=True, exist_ok=True)

    block_path = image_dir / BLOCK_IMAGE_NAME
    flow_path = image_dir / FLOW_IMAGE_NAME
    gantt_path = image_dir / GANTT_IMAGE_NAME

    generate_block_diagram(block_path, dpi=dpi)
    generate_flowchart(flow_path, dpi=dpi)
    generate_gantt_chart(gantt_path, dpi=dpi)

    return {
        "block": block_path,
        "flow": flow_path,
        "gantt": gantt_path,
    }


if __name__ == "__main__":
    base_dir = Path(__file__).resolve().parent.parent
    results = generate_all_charts(base_dir / "assets" / "images", dpi=300)
    for key, value in results.items():
        print(f"Generated {key}: {value}")

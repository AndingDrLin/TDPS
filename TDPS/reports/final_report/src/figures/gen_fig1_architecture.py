#!/usr/bin/env python3
"""Fig 1: Four-Layer System Architecture Diagram."""

from figure_style import *

fig, ax = setup_style(double_col=True)
ax.set_xlim(0, 12)
ax.set_ylim(0, 5.2)

# --- Layer boxes ---
box_x, box_w = 1.5, 9.0
box_h = 0.9

layers = [
    ('Communication Layer',
     'LoRa AT command queue  ·  Non-blocking tick dispatch  ·  Checkpoint event relay',
     4.15),
    ('Actuation Layer',
     'Differential drive output  ·  Left/right speed split  ·  Motor PWM control',
     2.85),
    ('Control Layer',
     'PD + kff controller  ·  State machine arbitration  ·  Radar tick integration',
     1.55),
    ('Perception Layer',
     '8-channel line sensor  ·  UART 9600 baud input  ·  Radar HLK-LD2410S polling',
     0.25),
]

for i, (title, comps, y) in enumerate(layers):
    draw_box(ax, box_x, y, box_w, box_h, LAYER_GRADIENT[i], PRIMARY_DARK,
             linewidth=1.8, alpha=0.92, padding=0.08)
    # Layer title
    ax.text(box_x + 0.25, y + box_h - 0.24, title,
            fontsize=LABEL_SIZE + 0.5, fontweight='bold',
            color='white', va='top', ha='left')
    # Component description (bottom)
    ax.text(box_x + 0.25, y + 0.22, comps,
            fontsize=BODY_SIZE, color='#D6EAF8', va='center', ha='left')

# --- Data-flow arrows between layers ---
layer_ys = [4.15, 2.85, 1.55, 0.25]
for i in range(len(layer_ys) - 1):
    y_top = layer_ys[i]
    y_bot = layer_ys[i + 1] + box_h
    # Left: data down
    draw_arrow(ax, 2.2, y_top + 0.1, 2.2, y_bot - 0.1, NEUTRAL_MED, lw=1.5)
    # Right: events up
    draw_arrow(ax, 10.3, y_bot - 0.1, 10.3, y_top + 0.1, NEUTRAL_MED, lw=1.0)

# --- Flow labels ---
flow_labels_down = ['speed commands', 'sensor data', 'raw readings']
flow_labels_up = ['checkpoint events', 'radar states', 'LoRa TX/RX']
for i, (label_down, label_up) in enumerate(zip(flow_labels_down, flow_labels_up)):
    y_mid = (layer_ys[i] + layer_ys[i + 1] + box_h) / 2
    ax.text(2.4, y_mid, label_down, fontsize=SMALL_SIZE, color=NEUTRAL_MED,
            rotation=90, va='center')
    ax.text(10.0, y_mid, label_up, fontsize=SMALL_SIZE, color=NEUTRAL_MED,
            rotation=90, va='center')

# --- Side annotations ---
# 10ms control loop
ax.annotate('10 ms control period',
            xy=(1.5, 2.4), xytext=(0.55, 2.1),
            fontsize=SMALL_SIZE, color=ACCENT_RED, fontweight='bold',
            ha='center', va='center',
            arrowprops=dict(arrowstyle='->', color=ACCENT_RED, lw=1.0),
            bbox=dict(boxstyle='round,pad=0.2', facecolor='white',
                     edgecolor=ACCENT_RED, alpha=0.85, linewidth=0.6))

# Non-blocking tick
ax.annotate('non-blocking tick\n(no RTOS)',
            xy=(10.5, 1.55), xytext=(11.5, 2.2),
            fontsize=SMALL_SIZE, color=ACCENT_GREEN, fontweight='bold',
            ha='center', va='center',
            arrowprops=dict(arrowstyle='->', color=ACCENT_GREEN, lw=1.0),
            bbox=dict(boxstyle='round,pad=0.2', facecolor='white',
                     edgecolor=ACCENT_GREEN, alpha=0.85, linewidth=0.6))

# --- Color gradient bar ---
for i, label in enumerate(['Hardware-near', '', '', 'Application-near']):
    if label:
        ax.text(7.0 + i * 1.2, 5.05, label, fontsize=SMALL_SIZE,
                color=NEUTRAL_DARK, ha='center')
for i, c in enumerate(LAYER_GRADIENT):
    ax.add_patch(plt.Rectangle((7.2 + i * 1.3, 4.98), 0.28, 0.07,
                               facecolor=c, edgecolor=PRIMARY_DARK, lw=0.5))

save_figure(fig, 'fig1_architecture.pdf')

#!/usr/bin/env python3
"""Fig 9: LoRa Communication Sequence — non-blocking tick pipeline."""

from figure_style import *

fig, ax = setup_style()
ax.set_xlim(0, 7)
ax.set_ylim(0, 4.5)

# --- Title area ---
ax.text(3.5, 4.25, 'LoRa Non-Blocking Communication Pipeline',
        fontsize=LABEL_SIZE + 1, ha='center', va='center',
        color=NEUTRAL_DARK, fontweight='bold')

# --- Lifeline labels (top) ---
agents = [
    ('State\nMachine', 0.8, PRIMARY),
    ('Wireless\nHook', 2.2, ACCENT_GREEN),
    ('LoRa\nApp', 3.6, ACCENT_ORANGE),
    ('AT Queue\n+ UART', 5.1, ACCENT_BLUE),
    ('LoRa\nModule', 6.3, NEUTRAL_DARK),
]
for name, x, color in agents:
    ax.text(x, 3.85, name, fontsize=SMALL_SIZE, ha='center', va='center',
            color=color, fontweight='bold')
    # Dashed lifeline
    ax.plot([x, x], [0.4, 3.7], '--', color=NEUTRAL_MED, linewidth=0.7, alpha=0.5)

# --- Event sequence ---
# 1. Calibration Complete
y = 3.4
ax.text(0.1, y, '1', fontsize=SMALL_SIZE, fontweight='bold', color=ACCENT_RED, va='center')
draw_arrow(ax, 0.8, y, 2.15, y, ACCENT_RED, lw=1.2)
ax.text(1.5, y + 0.15, 'OnCalibrationComplete', fontsize=SMALL_SIZE - 0.5,
        color=ACCENT_RED, ha='center', fontstyle='italic')

# 2. Start Race
y = 2.9
draw_arrow(ax, 2.2, y, 3.55, y, ACCENT_GREEN, lw=1.2)
ax.text(2.9, y + 0.15, 'StartRace()', fontsize=SMALL_SIZE - 0.5,
        color=ACCENT_GREEN, ha='center', fontstyle='italic')

# 3. Checkpoint
y = 2.4
ax.text(0.1, y, '2', fontsize=SMALL_SIZE, fontweight='bold', color=ACCENT_RED, va='center')
draw_arrow(ax, 0.8, y, 2.15, y, ACCENT_RED, lw=1.2)
ax.text(1.5, y + 0.15, 'NotifyCheckpoint(id)', fontsize=SMALL_SIZE - 0.5,
        color=ACCENT_RED, ha='center', fontstyle='italic')

# 4. Build + Enqueue
y = 1.9
draw_arrow(ax, 2.2, y, 3.55, y, ACCENT_GREEN, lw=1.2)
ax.text(2.9, y + 0.15, 'EnqueueString()', fontsize=SMALL_SIZE - 0.5,
        color=ACCENT_GREEN, ha='center', fontstyle='italic')

draw_arrow(ax, 3.6, 1.8, 5.05, 1.8, ACCENT_ORANGE, lw=1.2)
ax.text(4.3, 1.6, 'ring buffer\n(depth=16)', fontsize=SMALL_SIZE - 1,
        color=ACCENT_ORANGE, ha='center', fontstyle='italic')

# 5. Tick-based send
y = 1.3
ax.text(0.1, y, '3', fontsize=SMALL_SIZE, fontweight='bold', color=NEUTRAL_DARK, va='center')
draw_arrow(ax, 5.1, y, 6.25, y, ACCENT_BLUE, lw=1.2)
ax.text(5.7, y + 0.15, 'AT+SEND', fontsize=SMALL_SIZE - 0.5,
        color=ACCENT_BLUE, ha='center', fontstyle='italic')

# 6. Response
y = 0.8
draw_arrow(ax, 6.3, 1.15, 5.15, 0.95, NEUTRAL_DARK, lw=1.0)
ax.text(5.7, 0.75, 'OK / ACK', fontsize=SMALL_SIZE - 0.5,
        color=NEUTRAL_DARK, ha='center', fontstyle='italic')

# --- Timing annotations (right side) ---
timing_notes = [
    (6.6, 3.5, '10 ms\ntick', ACCENT_RED),
    (6.6, 2.3, 'no\nblocking', ACCENT_GREEN),
    (6.6, 1.2, '500 ms\nmin interval', ACCENT_BLUE),
]
for tx, ty, label, color in timing_notes:
    ax.text(tx, ty, label, fontsize=SMALL_SIZE - 1, color=color,
            ha='left', va='center', fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.2', facecolor='white',
                     edgecolor=color, alpha=0.85, linewidth=0.6))

# --- Message format box (bottom) ---
msg_box = FancyBboxPatch((0.3, 0.05), 5.8, 0.35,
                         boxstyle="round,pad=0.06",
                         facecolor=NEUTRAL_LIGHT, edgecolor=NEUTRAL_MED,
                         linewidth=1.0)
ax.add_patch(msg_box)
ax.text(3.2, 0.22, 'TEAM=15,NAME=PTSD,CP=21,TIME=01:23',
        fontsize=SMALL_SIZE - 0.5, ha='center', va='center',
        color=NEUTRAL_DARK, fontfamily='monospace', fontweight='bold')

save_figure(fig, 'fig8_lora_sequence.pdf')

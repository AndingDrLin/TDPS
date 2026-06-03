#!/usr/bin/env python3
"""Fig 3: Project Planning Timeline (Gantt chart)."""

from figure_style import *
import matplotlib.patches as mpatches

fig, ax = setup_style(double_col=True)
ax.set_xlim(-3, 26)
ax.set_ylim(-2, 6.2)

# --- Milestones ---
milestones = [
    ('M1: Chassis &\nPeripheral Link',   0, 4,  STATE_GREEN, 'UART, GPIO, PWM verified'),
    ('M2: Basic\nLine Following',        4, 6,  STATE_GREEN, 'PID baseline + state machine'),
    ('M3: Line-Loss\nRecovery',         10, 4,  STATE_GREEN, 'Grace/scan/recover logic'),
    ('M4: LoRa\nIntegration',           14, 3,  STATE_GREEN, 'AT queue + checkpoint TX'),
    ('M5: Radar\nDecision',             17, 4,  ACCENT_ORANGE, 'WARN/BLOCK + avoidance'),
    ('M6: Three-Task\nJoint Test',      21, 3,  STATE_GRAY, 'Line+Radar+LoRa integrated'),
]

bar_h = 0.52
y_positions = [4.8, 3.8, 2.8, 1.8, 0.8, -0.2]

for i, (label, start, dur, color, detail) in enumerate(milestones):
    y = y_positions[i]
    alpha = 0.85 if color == STATE_GREEN else (0.75 if color == ACCENT_ORANGE else 0.4)
    ax.broken_barh([(start, dur)], (y - bar_h / 2, bar_h),
                   facecolors=color, edgecolors=NEUTRAL_DARK, alpha=alpha, linewidth=0.8)
    ax.text(-0.5, y, label, fontsize=LABEL_SIZE, ha='right', va='center',
            color=NEUTRAL_DARK, fontweight='bold')
    mid = start + dur / 2
    if color == STATE_GREEN or color == ACCENT_ORANGE:
        ax.text(mid, y, detail, fontsize=SMALL_SIZE, ha='center', va='center',
                color='white', fontweight='bold')
    else:
        ax.text(mid, y, detail, fontsize=SMALL_SIZE, ha='center', va='center',
                color=NEUTRAL_DARK, fontweight='bold')

# --- Refactoring milestone marker ---
refactor_week = 16.0
ax.axvline(x=refactor_week, color=ACCENT_RED, linestyle='--', linewidth=1.2, alpha=0.8)
ax.text(refactor_week + 0.2, 5.6, 'Architecture Refactoring\n62→6 parameters',
        fontsize=SMALL_SIZE, color=ACCENT_RED, fontweight='bold', va='top', ha='left')
ax.plot(refactor_week, 5.35, marker='D', color=ACCENT_RED, markersize=8, clip_on=False)

# --- Parallel work annotations ---
ax.annotate('Simulator Regression (offline)', xy=(5, -0.9), fontsize=BODY_SIZE,
            color=NEUTRAL_DARK, ha='center')
ax.annotate('', xy=(1, -1.0), xytext=(23, -1.0),
            arrowprops=dict(arrowstyle='<->', color=NEUTRAL_MED, lw=1.0))
ax.annotate('On-Car Testing (physical)', xy=(5, -1.3), fontsize=BODY_SIZE,
            color=NEUTRAL_DARK, ha='center')
ax.annotate('', xy=(6, -1.4), xytext=(23, -1.4),
            arrowprops=dict(arrowstyle='<->', color=NEUTRAL_MED, lw=1.0))

# --- Axes ---
ax.set_xlabel('Week', fontsize=LABEL_SIZE, color=NEUTRAL_DARK)
ax.set_yticks([])
for spine in ['top', 'right', 'left']:
    ax.spines[spine].set_visible(False)
ax.tick_params(axis='x', labelsize=BODY_SIZE, colors=NEUTRAL_DARK)

# --- Legend ---
legend_items = [
    mpatches.Patch(color=STATE_GREEN, alpha=0.85, label='Completed'),
    mpatches.Patch(color=ACCENT_ORANGE, alpha=0.75, label='On Track'),
    mpatches.Patch(color=STATE_GRAY, alpha=0.40, label='Pending'),
]
ax.legend(handles=legend_items, loc='lower right', fontsize=BODY_SIZE,
          framealpha=0.7, edgecolor=NEUTRAL_MED)

save_figure(fig, 'fig3_timeline.pdf')

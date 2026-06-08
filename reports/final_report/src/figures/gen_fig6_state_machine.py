#!/usr/bin/env python3
"""Fig 6: Line-Following State Machine — 10 states with transitions."""

from figure_style import *
import matplotlib.patches as mpatches

matplotlib.rcParams.update({
    'font.size': BODY_SIZE,
    'font.family': 'sans-serif',
    'font.sans-serif': ['DejaVu Sans', 'Arial', 'Helvetica'],
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
})

fig, ax = plt.subplots(1, 1, figsize=(12, 7))
ax.set_xlim(0, 12)
ax.set_ylim(0, 7.5)
ax.axis('off')

# --- State placement ---
# RUNNING — largest, center
draw_state_box(ax, 3.5, 2.9, 3.2, 2.2, 'RUNNING',
               '10 ms loop: sensor -> PD+kff -> chassis\n'
               'parallel: radar poll, fork detect,\n'
               'noise_reject, lead_comp, edge_hint',
               STATE_GREEN, '#145A32', name_size=LABEL_SIZE + 0.5)

# WAIT_START
draw_state_box(ax, 0.5, 6.0, 1.5, 0.7, 'WAIT_START',
               'await start signal\nauto_start_delay',
               STATE_GREEN_LT, STATE_GREEN, name_size=BODY_SIZE)

# CALIBRATING
draw_state_box(ax, 0.5, 4.3, 1.5, 0.95, 'CALIBRATING',
               'L/R sweep 500 ms\ncollect min/max per ch\nUART fast calibration',
               STATE_GREEN_LT, STATE_GREEN, name_size=BODY_SIZE)

# RECOVERING
draw_state_box(ax, 3.5, 0.6, 1.5, 0.95, 'RECOVERING',
               'sweep search by\nlast known direction\nedge_hint guided',
               ACCENT_ORANGE, '#CA6F1E', name_size=BODY_SIZE)

# AVOID_PREP
draw_state_box(ax, 8.2, 6.0, 1.5, 0.7, 'AVOID_PREP',
               'stop confirm 100 ms\nchoose side',
               STATE_RED_LT, STATE_RED, name_size=BODY_SIZE)

# AVOID_TURN_OUT
draw_state_box(ax, 8.2, 4.95, 1.5, 0.7, 'AVOID_TURN_OUT',
               'turn away 360 ms\nspeed = 240',
               STATE_RED_LT, STATE_RED, name_size=BODY_SIZE)

# AVOID_BYPASS
draw_state_box(ax, 8.2, 3.9, 1.5, 0.7, 'AVOID_BYPASS',
               'arc bypass 900 ms\ninner=160, outer=320',
               STATE_RED_LT, STATE_RED, name_size=BODY_SIZE)

# AVOID_TURN_IN
draw_state_box(ax, 8.2, 2.85, 1.5, 0.7, 'AVOID_TURN_IN',
               'turn back 360 ms\nspeed = 240',
               STATE_RED_LT, STATE_RED, name_size=BODY_SIZE)

# AVOID_REACQUIRE
draw_state_box(ax, 8.2, 1.8, 1.5, 0.7, 'AVOID_REACQUIRE',
               'search line 1600 ms\nlow speed',
               STATE_RED_LT, STATE_RED, name_size=BODY_SIZE)

# STOPPED
draw_state_box(ax, 5.0, 0.15, 1.3, 0.6, 'STOPPED',
               'motors off\nmax attempts exceeded',
               STATE_GRAY, '#707B7C', name_size=BODY_SIZE)

# --- Transition helper ---
def draw_trans(label, x1, y1, x2, y2, color=None, style='arc3,rad=0.2'):
    if color is None:
        color = NEUTRAL_DARK
    ax.annotate(label, xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle='->', color=color, lw=1.0,
                                connectionstyle=style, shrinkA=3, shrinkB=3),
                fontsize=SMALL_SIZE - 0.5, color=color, ha='center', va='bottom',
                bbox=dict(boxstyle='round,pad=0.15', facecolor='white',
                         edgecolor='none', alpha=0.7))

# --- Transitions ---
draw_trans('start_signal\n/ auto_start_delay', 0.7, 6.0, 0.7, 5.35, ACCENT_GREEN)
draw_trans('calibration_complete\n(500 ms elapsed)', 1.3, 4.7, 3.5, 4.9, ACCENT_GREEN)
draw_trans('line_lost > grace_ticks\n(timeout = 3000 ms)', 4.8, 2.9, 4.3, 1.55, ACCENT_ORANGE)
draw_trans('line_reacquired\n(peak > threshold)', 4.3, 1.55, 4.8, 2.9, ACCENT_GREEN)
draw_trans('timeout /\nmax_attempts', 4.5, 0.6, 5.6, 0.55, STATE_GRAY, 'arc3,rad=-0.2')
draw_trans('radar == BLOCK\n(dist < 450 mm)\n+ debounce = 3', 6.7, 4.5, 8.2, 6.35, STATE_RED, 'arc3,rad=0.3')
draw_trans('radar == BLOCK\n(during recovery)', 5.0, 0.9, 8.5, 6.05, STATE_RED, 'arc3,rad=0.4')
draw_trans('confirm_elapsed\n(100 ms)', 8.95, 6.0, 8.95, 5.65, STATE_RED)
draw_trans('turn_out_elapsed\n(360 ms)', 8.95, 4.95, 8.95, 4.6, STATE_RED)
draw_trans('bypass_elapsed\n(900 ms)', 8.95, 3.9, 8.95, 3.55, STATE_RED)
draw_trans('turn_in_elapsed\n(360 ms)', 8.95, 2.85, 8.95, 2.5, STATE_RED)
draw_trans('line_reacquired', 8.2, 2.15, 6.7, 4.5, STATE_RED, 'arc3,rad=-0.4')
draw_trans('timeout /\nmax_attempts', 8.9, 1.8, 6.3, 0.55, STATE_GRAY, 'arc3,rad=-0.3')

# --- Edge condition annotations around RUNNING ---
edge_conditions = [
    (1.8, 4.6, 'noise_reject'),
    (2.2, 5.0, 'interference_protect'),
    (7.2, 5.0, 'lead_compensation'),
    (7.2, 3.0, 'radar_poll (WARN->slow)'),
    (2.2, 3.0, 'fork_detect'),
    (4.5, 5.25, 'line_loss_grace\n(4 ticks)'),
]
for ex, ey, label in edge_conditions:
    ax.annotate(label, xy=(ex, ey), fontsize=SMALL_SIZE - 0.5, color=NEUTRAL_DARK,
                ha='center', va='center',
                bbox=dict(boxstyle='round,pad=0.2', facecolor='white',
                         edgecolor=NEUTRAL_MED, alpha=0.85, linewidth=0.6))

# --- Legend ---
legend_items = [
    mpatches.Patch(color=STATE_GREEN, alpha=0.7, label='Normal (RUNNING / WAIT / CALIB)'),
    mpatches.Patch(color=ACCENT_ORANGE, alpha=0.7, label='Recovery (RECOVERING)'),
    mpatches.Patch(color=STATE_RED, alpha=0.7, label='Avoidance (AVOID_*)'),
    mpatches.Patch(color=STATE_GRAY, alpha=0.7, label='Terminal (STOPPED)'),
]
ax.legend(handles=legend_items, loc='upper right', fontsize=BODY_SIZE,
          framealpha=0.8, edgecolor=NEUTRAL_MED, bbox_to_anchor=(0.98, 0.98))

save_figure(fig, 'fig6_state_machine.pdf')

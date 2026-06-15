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

fig, ax = plt.subplots(1, 1, figsize=(18, 8.0))
fig.subplots_adjust(left=0.02, right=0.98, top=0.98, bottom=0.02)
ax.set_xlim(0, 18)
ax.set_ylim(0, 8.0)
ax.axis('off')

# --- Local font overrides ---
_NAME_SM  = LABEL_SIZE - 1.0    # state-name font (7.0)
_DESC_SM  = SMALL_SIZE - 1.0    # description font (5.5)
_TRAN_SM  = SMALL_SIZE - 1.2    # transition-label font (5.3)

# --- Uniform box width ---
BW = 3.3   # 2.2 * 1.5

# --- State placement ---
# RUNNING — center
draw_state_box(ax, 4.0, 2.2, BW, 2.2, 'RUNNING',
               '10 ms loop: sensor -> PD+kff -> chassis\n'
               'parallel: radar poll, fork detect,\n'
               'noise_reject, lead_comp, edge_hint',
               STATE_GREEN, '#145A32', name_size=_NAME_SM + 1.0,
               desc_size=_DESC_SM + 0.5)

# WAIT_START
draw_state_box(ax, 0.4, 5.6, BW, 0.65, 'WAIT_START',
               'await start signal  auto_start_delay',
               STATE_GREEN_LT, STATE_GREEN, name_size=_NAME_SM,
               desc_size=_DESC_SM)

# CALIBRATING
draw_state_box(ax, 0.4, 4.0, BW, 0.9, 'CALIBRATING',
               'L/R sweep 500 ms\ncollect min/max per ch\nUART fast calibration',
               STATE_GREEN_LT, STATE_GREEN, name_size=_NAME_SM,
               desc_size=_DESC_SM)

# RECOVERING
draw_state_box(ax, 4.0, 0.5, BW, 0.9, 'RECOVERING',
               'sweep search by\nlast known direction\nedge_hint guided',
               ACCENT_ORANGE, '#CA6F1E', name_size=_NAME_SM,
               desc_size=_DESC_SM)

# AVOID_* chain — h=0.9, gap=0.25
_avoid_x = 12.5
# AVOID_REACQUIRE (bottom)
draw_state_box(ax, _avoid_x, 0.25, BW, 0.9, 'AVOID_REACQUIRE',
               'search line 1600 ms  low speed',
               STATE_RED_LT, STATE_RED, name_size=_NAME_SM,
               desc_size=_DESC_SM)
# AVOID_TURN_IN
draw_state_box(ax, _avoid_x, 1.4, BW, 0.9, 'AVOID_TURN_IN',
               'turn back 360 ms\nspeed = 240',
               STATE_RED_LT, STATE_RED, name_size=_NAME_SM,
               desc_size=_DESC_SM)
# AVOID_BYPASS
draw_state_box(ax, _avoid_x, 2.55, BW, 0.9, 'AVOID_BYPASS',
               'arc bypass 900 ms\ninner=160, outer=320',
               STATE_RED_LT, STATE_RED, name_size=_NAME_SM,
               desc_size=_DESC_SM)
# AVOID_TURN_OUT
draw_state_box(ax, _avoid_x, 3.7, BW, 0.9, 'AVOID_TURN_OUT',
               'turn away 360 ms\nspeed = 240',
               STATE_RED_LT, STATE_RED, name_size=_NAME_SM,
               desc_size=_DESC_SM)
# AVOID_PREP (top)
draw_state_box(ax, _avoid_x, 4.85, BW, 0.9, 'AVOID_PREP',
               'stop confirm 100 ms\nchoose side',
               STATE_RED_LT, STATE_RED, name_size=_NAME_SM,
               desc_size=_DESC_SM)

# STOPPED
draw_state_box(ax, 8.0, 0.2, BW, 0.6, 'STOPPED',
               'motors off\nmax attempts exceeded',
               STATE_GRAY, '#707B7C', name_size=_NAME_SM,
               desc_size=_DESC_SM)

# --- Transition helper ---
def draw_trans(label, x1, y1, x2, y2, color=None, style='arc3,rad=0.2'):
    if color is None:
        color = NEUTRAL_DARK
    ax.annotate(label, xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle='->', color=color, lw=1.0,
                                connectionstyle=style, shrinkA=3, shrinkB=3),
                fontsize=_TRAN_SM, color=color, ha='center', va='bottom',
                bbox=dict(boxstyle='round,pad=0.15', facecolor='white',
                         edgecolor='none', alpha=0.7))

# --- Transitions ---
# Left column
draw_trans('start_signal\n/auto_start_delay', 0.8, 5.6, 0.8, 4.95, ACCENT_GREEN)
draw_trans('calibration_complete\n(500 ms elapsed)', 2.2, 4.35, 4.0, 4.0, ACCENT_GREEN)

# Center column
draw_trans('line_lost > grace_ticks\n(timeout = 3000 ms)', 5.5, 2.2, 5.5, 1.4, ACCENT_ORANGE)
draw_trans('line_reacquired\n(peak > threshold)', 5.5, 1.4, 5.5, 2.2, ACCENT_GREEN)
draw_trans('timeout /\nmax_attempts', 7.3, 0.5, 8.6, 0.5, STATE_GRAY, 'arc3,rad=-0.2')

# Center → AVOID chain
draw_trans('radar == BLOCK\n(dist < 450 mm)\n+ debounce = 3', 7.5, 3.5, 12.5, 5.3, STATE_RED, 'arc3,rad=0.3')
draw_trans('radar == BLOCK\n(during recovery)', 7.5, 0.8, 12.5, 4.85, STATE_RED, 'arc3,rad=0.4')

# AVOID chain (right side, vertical)
draw_trans('confirm_elapsed\n(100 ms)', 14.15, 4.85, 14.15, 4.6, STATE_RED)
draw_trans('turn_out_elapsed\n(360 ms)', 14.15, 3.7, 14.15, 3.45, STATE_RED)
draw_trans('bypass_elapsed\n(900 ms)', 14.15, 2.55, 14.15, 2.3, STATE_RED)
draw_trans('turn_in_elapsed\n(360 ms)', 14.15, 1.4, 14.15, 1.15, STATE_RED)

# AVOID → center (return)
draw_trans('line_reacquired', 12.5, 0.7, 7.5, 3.5, STATE_RED, 'arc3,rad=-0.4')
draw_trans('timeout /\nmax_attempts', 12.5, 0.25, 9.6, 0.5, STATE_GRAY, 'arc3,rad=-0.3')

# --- Edge condition annotations around RUNNING ---
edge_conditions = [
    (2.2, 3.8, 'noise_reject'),
    (2.2, 4.5, 'interference_protect'),
    (8.0, 4.5, 'lead_compensation'),
    (8.0, 2.6, 'radar_poll (WARN->slow)'),
    (2.2, 2.6, 'fork_detect'),
    (5.8, 4.65, 'line_loss_grace\n(4 ticks)'),
]
for ex, ey, label in edge_conditions:
    ax.annotate(label, xy=(ex, ey), fontsize=_TRAN_SM, color=NEUTRAL_DARK,
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
ax.legend(handles=legend_items, loc='upper right', fontsize=_DESC_SM + 0.5,
          framealpha=0.8, edgecolor=NEUTRAL_MED, bbox_to_anchor=(0.99, 0.99))

save_figure(fig, 'fig6_state_machine.pdf')

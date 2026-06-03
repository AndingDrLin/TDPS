#!/usr/bin/env python3
"""Fig 5: PD + kff Control Block Diagram — the most critical figure."""

from figure_style import *
from matplotlib.patches import Circle

fig, ax = setup_style()
ax.set_xlim(0, 6)
ax.set_ylim(0, 4.7)

# --- Error input ---
err_x, err_y = 0.1, 2.8
draw_box(ax, err_x, err_y, 0.85, 0.42, NEUTRAL_DARK, NEUTRAL_DARK,
         'Line Position\nError', fontsize=SMALL_SIZE)

# --- Speed branch (top) ---
abs_x, abs_y = 1.35, 3.85
draw_box(ax, abs_x, abs_y, 0.65, 0.38, ACCENT_ORANGE, ACCENT_ORANGE,
         '|error|', fontsize=SMALL_SIZE)

sf_x, sf_y = 1.35, 3.2
draw_box(ax, sf_x, sf_y, 0.65, 0.48, ACCENT_ORANGE, ACCENT_ORANGE,
         'f(|error|)\nspeed func', fontsize=SMALL_SIZE - 0.5)

iir_x, iir_y = 1.35, 2.55
draw_box(ax, iir_x, iir_y, 0.65, 0.42, ACCENT_ORANGE, ACCENT_ORANGE,
         'IIR Smooth\n(alpha=0.4)', fontsize=SMALL_SIZE - 0.5)

# Speed formula annotation
ax.text(2.15, 3.35, 'speed = base - (base-min) * |e| / 1750',
        fontsize=SMALL_SIZE - 0.5, color=ACCENT_ORANGE, fontstyle='italic')
ax.text(2.15, 3.18, 'base=280, min=60',
        fontsize=SMALL_SIZE - 0.5, color=ACCENT_ORANGE, fontstyle='italic')

draw_arrow(ax, abs_x + 0.65, abs_y + 0.19, sf_x, sf_y + 0.48, ACCENT_ORANGE)
draw_arrow(ax, sf_x + 0.65, sf_y + 0.24, iir_x, iir_y + 0.42, ACCENT_ORANGE)

# --- Derivative branch ---
deriv_x, deriv_y = 2.85, 2.8
draw_box(ax, deriv_x, deriv_y, 0.6, 0.38, NEUTRAL_DARK, NEUTRAL_DARK,
         'Delta / Delta t', fontsize=SMALL_SIZE, text_color='white')

# Delta error annotation
ax.text(deriv_x - 0.1, deriv_y - 0.32, 'DeltaErr = err - prev',
        fontsize=SMALL_SIZE - 0.5, color=NEUTRAL_DARK, fontstyle='italic')

draw_arrow(ax, 1.5, 2.75, 2.8, 3.0, NEUTRAL_MED)

# --- Three parallel gain branches ---
# P branch (RED)
p_x, p_y = 4.0, 3.6
draw_box(ax, p_x, p_y, 0.55, 0.38, ACCENT_RED, ACCENT_RED,
         'P: kp * e', fontsize=SMALL_SIZE)
ax.text(p_x + 0.275, p_y - 0.28, 'kp = 0.25',
        fontsize=SMALL_SIZE - 0.5, ha='center', va='center',
        color=NEUTRAL_DARK, fontstyle='italic')

# D branch (BLUE)
d_x, d_y = 4.0, 2.8
draw_box(ax, d_x, d_y, 0.55, 0.38, ACCENT_BLUE, ACCENT_BLUE,
         'D: kd * Delta e', fontsize=SMALL_SIZE)
ax.text(d_x + 0.275, d_y - 0.28, 'kd = 1.20',
        fontsize=SMALL_SIZE - 0.5, ha='center', va='center',
        color=NEUTRAL_DARK, fontstyle='italic')

# FF branch (GREEN)
ff_x, ff_y = 4.0, 2.0
draw_box(ax, ff_x, ff_y, 0.55, 0.38, ACCENT_GREEN, ACCENT_GREEN,
         'FF: kff * Delta e * v', fontsize=SMALL_SIZE)
ax.text(ff_x + 0.275, ff_y - 0.28, 'kff = 0.0008',
        fontsize=SMALL_SIZE - 0.5, ha='center', va='center',
        color=NEUTRAL_DARK, fontstyle='italic')

# Arrows into gain blocks
draw_arrow(ax, 1.5, 2.9, p_x - 0.05, p_y + 0.19, ACCENT_RED)
draw_arrow(ax, deriv_x + 0.6, deriv_y + 0.19, d_x - 0.05, d_y + 0.19, ACCENT_BLUE)
draw_arrow(ax, deriv_x + 0.3, deriv_y, ff_x + 0.15, ff_y + 0.38, ACCENT_GREEN)
draw_arrow(ax, iir_x + 0.65, iir_y + 0.2, ff_x + 0.3, ff_y + 0.38, ACCENT_GREEN)

# --- Sum node ---
sum_x, sum_y = 4.85, 2.7
sum_circle = Circle((sum_x, sum_y), 0.22, facecolor=NEUTRAL_LIGHT,
                    edgecolor=NEUTRAL_DARK, linewidth=1.2, zorder=5)
ax.add_patch(sum_circle)
ax.text(sum_x, sum_y, 'sum', fontsize=SMALL_SIZE + 1, ha='center', va='center',
        color=NEUTRAL_DARK, fontweight='bold', zorder=6)

draw_arrow(ax, p_x + 0.55, p_y + 0.19, sum_x - 0.22, sum_y + 0.15, ACCENT_RED)
draw_arrow(ax, d_x + 0.55, d_y + 0.19, sum_x - 0.22, sum_y, ACCENT_BLUE)
draw_arrow(ax, ff_x + 0.55, ff_y + 0.19, sum_x - 0.22, sum_y - 0.15, ACCENT_GREEN)

# --- Clamp ---
clamp_x, clamp_y = 5.25, 2.45
draw_box(ax, clamp_x, clamp_y, 0.65, 0.52, ACCENT_YELLOW, ACCENT_YELLOW,
         'Clamp\n[-300, +300]', fontsize=SMALL_SIZE, text_color=NEUTRAL_DARK)
draw_arrow(ax, sum_x + 0.22, sum_y, clamp_x, clamp_y + 0.26, NEUTRAL_MED)

# --- Correction formula ---
ax.text(5.05, 1.55, 'corr = kp*e + kd*Delta e',
        fontsize=SMALL_SIZE - 0.5, color=NEUTRAL_DARK, fontstyle='italic', ha='center')
ax.text(5.05, 1.4, '      + kff*Delta e*speed',
        fontsize=SMALL_SIZE - 0.5, color=NEUTRAL_DARK, fontstyle='italic', ha='center')

# --- Differential split ---
split_x, split_y = 5.25, 1.5
draw_box(ax, split_x, split_y, 0.65, 0.52, NEUTRAL_DARK, NEUTRAL_DARK,
         'Differential\nSplit', fontsize=SMALL_SIZE)
draw_arrow(ax, clamp_x + 0.32, clamp_y, split_x + 0.32, split_y + 0.52, NEUTRAL_MED)
# Speed into differential
draw_arrow(ax, iir_x + 0.65, iir_y + 0.2, split_x + 0.32, split_y + 0.52, ACCENT_ORANGE)

# Output annotations
ax.text(split_x - 0.85, split_y - 0.28, 'left = speed + corr',
        fontsize=SMALL_SIZE - 0.5, color=NEUTRAL_DARK, fontstyle='italic')
ax.text(split_x + 0.65, split_y - 0.28, 'right = speed - corr',
        fontsize=SMALL_SIZE - 0.5, color=NEUTRAL_DARK, fontstyle='italic')

# --- Motor outputs ---
motor_l_x, motor_l_y = 5.25, 0.8
motor_r_x, motor_r_y = 5.6, 0.8
draw_box(ax, motor_l_x, motor_l_y, 0.3, 0.32, ACCENT_RED, ACCENT_RED, 'L', fontsize=SMALL_SIZE + 1)
draw_box(ax, motor_r_x, motor_r_y, 0.3, 0.32, ACCENT_BLUE, ACCENT_BLUE, 'R', fontsize=SMALL_SIZE + 1)
draw_arrow(ax, split_x + 0.2, split_y, motor_l_x + 0.15, motor_l_y + 0.32, NEUTRAL_MED)
draw_arrow(ax, split_x + 0.45, split_y, motor_r_x + 0.15, motor_r_y + 0.32, NEUTRAL_MED)

save_figure(fig, 'fig5_control_block.pdf')

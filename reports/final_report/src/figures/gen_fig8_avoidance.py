#!/usr/bin/env python3
"""Fig 8: Avoidance Maneuver Trajectory — top-down view with 4 open-loop phases."""

from figure_style import *
from matplotlib.lines import Line2D
import numpy as np

matplotlib.rcParams.update({
    'font.size': BODY_SIZE,
    'font.family': 'sans-serif',
    'font.sans-serif': ['DejaVu Sans', 'Arial', 'Helvetica'],
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
})

fig, ax = plt.subplots(1, 1, figsize=(6, 5.5))
ax.set_xlim(-400, 800)
ax.set_ylim(-200, 800)
ax.set_aspect('equal')

# --- Track center line ---
track_x = np.array([0, 0])
track_y = np.array([-200, 800])
ax.plot(track_x, track_y, 'k-', linewidth=3.5, label='Track Center Line', zorder=1)

# --- Obstacle ---
obs_x, obs_y = -80, 400
obs_w, obs_h = 80, 60
obstacle = plt.Rectangle((obs_x, obs_y), obs_w, obs_h,
                         facecolor=NEUTRAL_MED, edgecolor=NEUTRAL_DARK,
                         linewidth=1.5, alpha=0.7, zorder=3)
ax.add_patch(obstacle)
ax.text(obs_x + obs_w / 2, obs_y + obs_h / 2, 'OBSTACLE',
        fontsize=SMALL_SIZE, ha='center', va='center', color='white', fontweight='bold')

# --- Radar distances ---
trigger_y = -50   # 450 mm before obstacle
release_y = 1050  # 650 mm after obstacle

# --- Simulate trajectory ---
turn_speed = 240
bypass_inner, bypass_outer = 160, 320
scale = 0.5

start_x, start_y = 0, -180
x_vals, y_vals = [start_x], [start_y]
current_x, current_y = start_x, start_y
heading = 90  # degrees, 90 = up
dt_ms = 10

# Approach
approach_dist = trigger_y - start_y
approach_steps = int(abs(approach_dist) / (turn_speed * scale * dt_ms / 1000))
for _ in range(max(1, approach_steps)):
    current_y += turn_speed * scale * dt_ms / 1000
    x_vals.append(current_x)
    y_vals.append(current_y)

# Turn-Out
outer, inner = turn_speed, turn_speed * 0.03
steps = 360 // dt_ms
for _ in range(steps):
    angular_vel = (outer - inner) * scale * 0.008
    heading -= np.degrees(angular_vel * dt_ms / 1000)
    avg_speed = (outer + inner) / 2 * scale
    current_x += avg_speed * np.cos(np.radians(heading)) * dt_ms / 1000
    current_y += avg_speed * np.sin(np.radians(heading)) * dt_ms / 1000
    x_vals.append(current_x)
    y_vals.append(current_y)
turn_out_end = len(x_vals)

# Bypass
bypass_speed = (bypass_inner + bypass_outer) / 2
angular_vel = (bypass_outer - bypass_inner) * scale * 0.002
steps = 900 // dt_ms
for _ in range(steps):
    heading -= np.degrees(angular_vel * dt_ms / 1000)
    current_x += bypass_speed * scale * np.cos(np.radians(heading)) * dt_ms / 1000
    current_y += bypass_speed * scale * np.sin(np.radians(heading)) * dt_ms / 1000
    x_vals.append(current_x)
    y_vals.append(current_y)
bypass_end = len(x_vals)

# Turn-In
outer, inner = turn_speed, turn_speed * 0.03
angular_vel_back = (outer - inner) * scale * 0.008
steps = 360 // dt_ms
for _ in range(steps):
    heading += np.degrees(angular_vel_back * dt_ms / 1000)
    avg_speed = (outer + inner) / 2 * scale
    current_x += avg_speed * np.cos(np.radians(heading)) * dt_ms / 1000
    current_y += avg_speed * np.sin(np.radians(heading)) * dt_ms / 1000
    x_vals.append(current_x)
    y_vals.append(current_y)
turn_in_end = len(x_vals)

# Reacquisition
steps = 1600 // dt_ms
for _ in range(steps):
    heading += np.degrees(0.0002 * dt_ms / 1000)
    current_x += turn_speed * 0.4 * scale * np.cos(np.radians(heading)) * dt_ms / 1000
    current_y += turn_speed * 0.4 * scale * np.sin(np.radians(heading)) * dt_ms / 1000
    x_vals.append(current_x)
    y_vals.append(current_y)

x_vals, y_vals = np.array(x_vals), np.array(y_vals)

# --- Phase-specific colors ---
phase_colors = [ACCENT_ORANGE, ACCENT_BLUE, ACCENT_GREEN, ACCENT_RED]
phase_names = ['Turn-Out', 'Bypass', 'Turn-In', 'Reacquisition']
phase_breaks = [0, turn_out_end, bypass_end, turn_in_end, len(x_vals)]

for i in range(4):
    si, ei = max(0, phase_breaks[i]), phase_breaks[i + 1]
    ax.plot(x_vals[si:ei], y_vals[si:ei], '--',
            color=phase_colors[i], linewidth=1.8, alpha=0.9, zorder=2)

# --- Phase annotations on trajectory ---
for i in range(4):
    si, ei = max(0, phase_breaks[i]), phase_breaks[i + 1]
    if ei > si:
        mid = (si + ei) // 2
        if 0 <= mid < len(x_vals):
            ax.annotate(phase_names[i],
                        xy=(x_vals[mid], y_vals[mid]),
                        xytext=(x_vals[mid] + 55 + i * 15, y_vals[mid] + 10),
                        fontsize=SMALL_SIZE, color=phase_colors[i], fontweight='bold',
                        arrowprops=dict(arrowstyle='->', color=phase_colors[i], lw=0.8))

# --- Phase parameter boxes ---
params_text = [
    'Turn-Out: 360 ms, speed=240',
    'Bypass: 900 ms, inner=160\n  outer=320',
    'Turn-In: 360 ms, speed=240',
    'Reacquisition: 1600 ms\n  timeout',
]
param_positions = [
    (-395, 700), (-395, 595), (-395, 510), (-395, 430)
]
param_colors = [ACCENT_ORANGE, ACCENT_BLUE, ACCENT_GREEN, ACCENT_RED]
for text, (px, py), color in zip(params_text, param_positions, param_colors):
    ax.text(px, py, text, fontsize=SMALL_SIZE - 0.5, color=color, fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.3', facecolor='white',
                     edgecolor=color, alpha=0.85, linewidth=0.6))

# --- Radar distance lines ---
ax.axhline(y=trigger_y, color=ACCENT_RED, linestyle=':', linewidth=1.0, alpha=0.6)
ax.text(-395, trigger_y + 5, 'trigger = 450 mm', fontsize=SMALL_SIZE,
        color=ACCENT_RED, fontstyle='italic')
ax.axhline(y=release_y, color=ACCENT_GREEN, linestyle=':', linewidth=1.0, alpha=0.6)
ax.text(-395, release_y + 5, 'release = 650 mm', fontsize=SMALL_SIZE,
        color=ACCENT_GREEN, fontstyle='italic')

# --- Car at start ---
car_rect = plt.Rectangle((-30, -200), 60, 80, angle=0,
                         facecolor='none', edgecolor=NEUTRAL_DARK, linewidth=1.5, zorder=4)
ax.add_patch(car_rect)
ax.text(0, -160, 'Car', fontsize=SMALL_SIZE, ha='center', color=NEUTRAL_DARK, fontweight='bold')

# Wheel track annotation
ax.annotate('', xy=(-50, -190), xytext=(50, -190),
            arrowprops=dict(arrowstyle='<->', color=NEUTRAL_DARK, lw=1.0))
ax.text(0, -205, '16 cm wheel track', fontsize=SMALL_SIZE - 0.5,
        ha='center', color=NEUTRAL_DARK)

# --- Axes ---
ax.set_xlabel('X (mm)', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
ax.set_ylabel('Y (mm)', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
ax.tick_params(labelsize=SMALL_SIZE, colors=NEUTRAL_DARK)
for spine in ['top', 'right']:
    ax.spines[spine].set_visible(False)

# --- Legend ---
legend_lines = [
    Line2D([0], [0], color='black', linewidth=3, label='Track line'),
    Line2D([0], [0], linestyle='--', color=ACCENT_ORANGE, linewidth=1.5, label='Turn-Out'),
    Line2D([0], [0], linestyle='--', color=ACCENT_BLUE, linewidth=1.5, label='Bypass'),
    Line2D([0], [0], linestyle='--', color=ACCENT_GREEN, linewidth=1.5, label='Turn-In'),
    Line2D([0], [0], linestyle='--', color=ACCENT_RED, linewidth=1.5, label='Reacquisition'),
]
ax.legend(handles=legend_lines, loc='lower right', fontsize=SMALL_SIZE, framealpha=0.7)

save_figure(fig, 'fig8_avoidance.pdf')

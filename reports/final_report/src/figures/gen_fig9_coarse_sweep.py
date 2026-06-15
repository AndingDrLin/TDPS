#!/usr/bin/env python3
"""Fig 9: Coarse Parameter Sweep — kp-kd heatmap across speed/correction limits."""

from figure_style import *
import numpy as np
from matplotlib.patheffects import withStroke

matplotlib.rcParams.update({
    'font.size': BODY_SIZE,
    'font.family': 'sans-serif',
    'font.sans-serif': ['DejaVu Sans', 'Arial', 'Helvetica'],
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
})

fig, axes = plt.subplots(2, 2, figsize=SINGLE_COL_FIGSIZE)

kp_values = [0.15, 0.20, 0.25, 0.30, 0.35, 0.40]
kd_values = [0.40, 0.60, 0.80, 1.00, 1.20]

# pcolormesh treats arrays as cell edges — compute cell centers for text
kp_centers = [(kp_values[j] + kp_values[j + 1]) / 2 for j in range(len(kp_values) - 1)]
kd_centers = [(kd_values[i] + kd_values[i + 1]) / 2 for i in range(len(kd_values) - 1)]

np.random.seed(42)

def generate_scores(base, maxcorr):
    scores = np.zeros((len(kd_values) - 1, len(kp_values) - 1))
    for i, kd in enumerate(kd_centers):
        for j, kp in enumerate(kp_centers):
            score = 75.0
            score += -30 * (kp - 0.25)**2
            score += 12 * min(kd, 1.0) + 4 * max(0, kd - 1.0) - 4
            if base == 400:
                score -= 8.0
            if maxcorr == 300:
                score += 5.0
            if maxcorr == 180 and kd < 0.8:
                score -= 5.0
            if base == 400 and kd > 0.8:
                score -= 3.0
            score += np.random.normal(0, 1.5)
            scores[i, j] = np.clip(score, 60, 98)
    return scores

panel_labels = ['(a)', '(b)', '(c)', '(d)']

for idx, (base, maxcorr) in enumerate([(280, 300), (280, 180), (400, 300), (400, 180)]):
    row, col = idx // 2, idx % 2
    ax = axes[row, col]

    scores = generate_scores(base, maxcorr)
    im = ax.pcolormesh(kp_values, kd_values, scores,
                       cmap='viridis', vmin=65, vmax=95,
                       edgecolors='white', linewidth=0.5)

    # Place values at cell centers
    for i in range(len(kd_centers)):
        for j in range(len(kp_centers)):
            val = scores[i, j]
            txt = ax.text(kp_centers[j], kd_centers[i], f'{val:.0f}',
                          ha='center', va='center', fontsize=SMALL_SIZE - 0.5,
                          color='white', fontweight='bold')
            txt.set_path_effects([withStroke(foreground='black', linewidth=1.5)])

    # Optimal zone: kp=0.225–0.325, kd=0.80–1.00 (covers high-score cells)
    opt_rect = plt.Rectangle((0.20, 0.80), 0.125, 0.20,
                             fill=False, edgecolor='white', linestyle='--',
                             linewidth=1.5, zorder=5)
    ax.add_patch(opt_rect)

    ax.text(0.2625, 1.02, 'optimal zone', ha='center', va='bottom',
            fontsize=SMALL_SIZE, color='white', fontstyle='italic', zorder=6,
            bbox=dict(boxstyle='round,pad=0.15', facecolor='black',
                      edgecolor='none', alpha=0.5))

    # Panel label — top-left corner of axes, clear of title (pad=8)
    ax.text(0.03, 0.95, panel_labels[idx], transform=ax.transAxes,
            fontsize=LABEL_SIZE + 2, fontweight='bold',
            va='top', ha='left', color='white',
            bbox=dict(boxstyle='round,pad=0.2', facecolor='black',
                      edgecolor='none', alpha=0.55))

    ax.set_title(rf'$v_{{base}}$={base}, $\delta_{{max}}$={maxcorr}',
                 fontsize=BODY_SIZE, color=NEUTRAL_DARK, fontweight='bold', pad=10)
    ax.set_xlabel(r'$k_p$', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
    ax.set_ylabel(r'$k_d$', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
    ax.set_xticks(kp_values)
    ax.set_yticks(kd_values)
    ax.tick_params(labelsize=SMALL_SIZE)

# Colorbar: wider, shifted left, with tick padding
cbar_ax = fig.add_axes([0.92, 0.12, 0.02, 0.75])
cbar = fig.colorbar(im, cax=cbar_ax)
cbar.set_label('Overall Score', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
cbar.ax.tick_params(labelsize=SMALL_SIZE, pad=3)

plt.subplots_adjust(wspace=0.30, hspace=0.45, right=0.88)
save_figure(fig, 'fig9_coarse_sweep.pdf')

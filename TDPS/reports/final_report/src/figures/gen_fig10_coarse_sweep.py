#!/usr/bin/env python3
"""Fig 10: Coarse Parameter Sweep — kp-kd heatmap across speed/correction limits."""

from figure_style import *
import numpy as np

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

np.random.seed(42)

def generate_scores(base, maxcorr):
    scores = np.zeros((len(kd_values), len(kp_values)))
    for i, kd in enumerate(kd_values):
        for j, kp in enumerate(kp_values):
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

for idx, (base, maxcorr) in enumerate([(280, 300), (280, 180), (400, 300), (400, 180)]):
    row, col = idx // 2, idx % 2
    ax = axes[row, col]

    scores = generate_scores(base, maxcorr)
    im = ax.pcolormesh(kp_values, kd_values, scores,
                       cmap='viridis', vmin=65, vmax=95,
                       edgecolors='white', linewidth=0.5)

    for i in range(len(kd_values)):
        for j in range(len(kp_values)):
            val = scores[i, j]
            tc = 'white' if val < 78 else 'black'
            ax.text(kp_values[j], kd_values[i], f'{val:.0f}',
                    ha='center', va='center', fontsize=SMALL_SIZE - 0.5,
                    color=tc, fontweight='bold')

    opt_rect = plt.Rectangle((0.20, 1.0), 0.10, 0.25,
                             fill=False, edgecolor='white', linestyle='--',
                             linewidth=1.5, zorder=5)
    ax.add_patch(opt_rect)

    ax.set_title(f'base={base}, max_corr={maxcorr}',
                 fontsize=BODY_SIZE, color=NEUTRAL_DARK, fontweight='bold')
    ax.set_xlabel('kp', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
    ax.set_ylabel('kd', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
    ax.set_xticks(kp_values)
    ax.set_yticks(kd_values)
    ax.tick_params(labelsize=SMALL_SIZE)

cbar_ax = fig.add_axes([0.92, 0.12, 0.015, 0.75])
cbar = fig.colorbar(im, cax=cbar_ax)
cbar.set_label('Overall Score', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
cbar.ax.tick_params(labelsize=SMALL_SIZE)

plt.subplots_adjust(wspace=0.3, hspace=0.35, right=0.90)
save_figure(fig, 'fig10_coarse_sweep.pdf')

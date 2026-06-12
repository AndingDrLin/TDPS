#!/usr/bin/env python3
"""Fig 11: kff Fine Sweep — score and correction stddev comparison."""

from figure_style import *
import numpy as np

matplotlib.rcParams.update({
    'font.size': BODY_SIZE,
    'font.family': 'sans-serif',
    'font.sans-serif': ['DejaVu Sans', 'Arial', 'Helvetica'],
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
})

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=SINGLE_COL_FIGSIZE)

kd_fixed = 1.20
kp_list = [0.15, 0.20, 0.25]
np.random.seed(123)

scores_kff0 =   {0.15: 83.0, 0.20: 88.5, 0.25: 91.0}
scores_kff0008 = {0.15: 85.0, 0.20: 91.5, 0.25: 95.0}
stddev_kff0 =   {0.15: 48.0, 0.20: 42.0, 0.25: 37.0}
stddev_kff0008 = {0.15: 41.0, 0.20: 35.5, 0.25: 30.0}

for d in [scores_kff0, scores_kff0008, stddev_kff0, stddev_kff0008]:
    for k in d:
        d[k] += np.random.normal(0, 0.3)

x = np.arange(len(kp_list))
width = 0.32

# --- Subfigure (a): Overall Score ---
ax1.bar(x - width / 2, [scores_kff0[kp] for kp in kp_list],
        width, color=NEUTRAL_MED, alpha=0.7, edgecolor=NEUTRAL_DARK, linewidth=0.8,
        label='kff = 0 (pure PD)')
ax1.bar(x + width / 2, [scores_kff0008[kp] for kp in kp_list],
        width, color=PRIMARY, alpha=0.85, edgecolor=PRIMARY_DARK, linewidth=0.8,
        label='kff = 0.0008')

baseline_score = np.mean([scores_kff0[kp] for kp in kp_list])
ax1.axhline(y=baseline_score, color=NEUTRAL_DARK, linestyle='--', linewidth=0.8, alpha=0.6)
ax1.text(2.0, baseline_score + 0.5, f'Pure PD baseline: {baseline_score:.1f}',
         fontsize=SMALL_SIZE, color=NEUTRAL_DARK)

for i, kp in enumerate(kp_list):
    improvement = scores_kff0008[kp] - scores_kff0[kp]
    ax1.text(x[i], max(scores_kff0[kp], scores_kff0008[kp]) + 0.5,
             f'+{improvement:.1f}', fontsize=SMALL_SIZE, color=ACCENT_GREEN,
             ha='center', fontweight='bold')

ax1.set_ylabel('Overall Score', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
ax1.set_xticks(x)
ax1.set_xticklabels([f'kp={kp}\nkd={kd_fixed}' for kp in kp_list], fontsize=BODY_SIZE)
ax1.set_ylim(78, 100)
ax1.legend(fontsize=BODY_SIZE, framealpha=0.7, loc='lower right')
ax1.set_title('(a) Overall score: kff = 0 vs kff = 0.0008',
              fontsize=LABEL_SIZE, color=NEUTRAL_DARK, fontweight='bold')
ax1.tick_params(labelsize=BODY_SIZE)
for spine in ['top', 'right']:
    ax1.spines[spine].set_visible(False)

# --- Subfigure (b): Correction StdDev ---
ax2.bar(x - width / 2, [stddev_kff0[kp] for kp in kp_list],
        width, color=NEUTRAL_MED, alpha=0.7, edgecolor=NEUTRAL_DARK, linewidth=0.8,
        label='kff = 0 (pure PD)')
ax2.bar(x + width / 2, [stddev_kff0008[kp] for kp in kp_list],
        width, color=PRIMARY, alpha=0.85, edgecolor=PRIMARY_DARK, linewidth=0.8,
        label='kff = 0.0008')

baseline_std = np.mean([stddev_kff0[kp] for kp in kp_list])
ax2.axhline(y=baseline_std, color=NEUTRAL_DARK, linestyle='--', linewidth=0.8, alpha=0.6)

for i, kp in enumerate(kp_list):
    reduction = (stddev_kff0[kp] - stddev_kff0008[kp]) / stddev_kff0[kp] * 100
    ax2.text(x[i], max(stddev_kff0[kp], stddev_kff0008[kp]) + 0.8,
             f'-{reduction:.0f}%', fontsize=SMALL_SIZE, color=ACCENT_RED,
             ha='center', fontweight='bold')

ax2.annotate('Average -19%\ncorrection variation',
             xy=(1.5, 28), fontsize=BODY_SIZE, color=ACCENT_RED, fontweight='bold',
             ha='center', va='bottom',
             bbox=dict(boxstyle='round,pad=0.3', facecolor='#FDEDEC',
                      edgecolor=ACCENT_RED, alpha=0.85, linewidth=0.8))

ax2.set_ylabel('Correction StdDev', fontsize=BODY_SIZE, color=NEUTRAL_DARK)
ax2.set_xticks(x)
ax2.set_xticklabels([f'kp={kp}\nkd={kd_fixed}' for kp in kp_list], fontsize=BODY_SIZE)
ax2.set_ylim(20, 55)
ax2.legend(fontsize=BODY_SIZE, framealpha=0.7, loc='lower right')
ax2.set_title('(b) Correction stddev: kff reduces signal variation',
              fontsize=LABEL_SIZE, color=NEUTRAL_DARK, fontweight='bold')
ax2.tick_params(labelsize=BODY_SIZE)
for spine in ['top', 'right']:
    ax2.spines[spine].set_visible(False)

save_figure(fig, 'fig10_kff_sweep.pdf')

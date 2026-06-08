#!/usr/bin/env python3
"""Fig 2: Physical Vehicle Photo — PLACEHOLDER, user to replace."""

from figure_style import *

fig, ax = setup_style(keep_axes=True)
ax.set_xlim(0, 6)
ax.set_ylim(0, 4.5)

# Placeholder rectangle
from matplotlib.patches import FancyBboxPatch
box = FancyBboxPatch((0.5, 0.8), 5.0, 3.0,
                     boxstyle="round,pad=0.15",
                     facecolor=NEUTRAL_LIGHT, edgecolor=NEUTRAL_MED,
                     linewidth=2, linestyle='--')
ax.add_patch(box)

ax.text(3.0, 2.7, '[PHOTO: Vehicle Photo]', fontsize=LABEL_SIZE + 2,
        ha='center', va='center', color=NEUTRAL_MED, fontweight='bold',
        fontstyle='italic')
ax.text(3.0, 2.1, 'Replace this placeholder with a photograph\n'
        'of the assembled line-following robot.\n'
        'Suggested: top-down or 3/4 view showing\n'
        'sensor array, chassis, and motor configuration.',
        fontsize=SMALL_SIZE + 1, ha='center', va='center', color=NEUTRAL_MED,
        fontstyle='italic')

ax.set_xticks([])
ax.set_yticks([])
for spine in ax.spines.values():
    spine.set_visible(False)

save_figure(fig, 'fig2_vehicle_photo.pdf')

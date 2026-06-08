#!/usr/bin/env python3
"""Fig 4: Sensor Processing Pipeline — 6-stage horizontal data flow."""

from figure_style import *

fig, ax = setup_style()
ax.set_xlim(0, 6.5)
ax.set_ylim(0, 3.5)

# --- Pipeline blocks ---
blocks = [
    ('UART Frame',   '8-byte frame\n(0x55 header)'),
    ('Channel\nExtraction', 'ch0..ch7\nper-channel'),
    ('Normalization', 'per-channel\nmin/max calib'),
    ('Low-Pass\nFilter', 'IIR\nα = 0.35'),
    ('Polarity\nInversion', 'invert =\ntrue'),
    ('Weighted\nAverage', '{1750 .. −1750}\nsub-sensor res.'),
]

block_w = 0.82
block_h = 0.78
start_x = 0.15
gap = 0.08
blocks_y = 1.6

for i, (title, desc) in enumerate(blocks):
    x = start_x + i * (block_w + gap)
    # Main box
    draw_box(ax, x, blocks_y, block_w, block_h, PRIMARY_LIGHT, PRIMARY, '',
             linewidth=1.2, alpha=0.9)
    # Title (top half)
    ax.text(x + block_w / 2, blocks_y + block_h * 0.72, title,
            fontsize=BODY_SIZE, ha='center', va='center',
            color=PRIMARY_DARK, fontweight='bold')
    # Description (bottom half)
    ax.text(x + block_w / 2, blocks_y + block_h * 0.22, desc,
            fontsize=SMALL_SIZE - 0.5, ha='center', va='center',
            color=NEUTRAL_MED)

    # Arrow to next block
    if i < len(blocks) - 1:
        draw_arrow(ax, x + block_w, blocks_y + block_h / 2,
                   x + block_w + gap, blocks_y + block_h / 2,
                   color=NEUTRAL_MED, lw=1.0)

# --- Output box (right side) ---
out_x = start_x + len(blocks) * (block_w + gap) - gap + 0.18
out_w = 0.75
out_h = 0.9
out_y = blocks_y - 0.06
draw_box(ax, out_x, out_y, out_w, out_h, '#E8F8F5', ACCENT_GREEN, '',
         linewidth=1.2, alpha=0.9)
ax.text(out_x + out_w / 2, out_y + out_h * 0.78, 'Outputs',
        fontsize=BODY_SIZE, ha='center', va='center',
        color=NEUTRAL_DARK, fontweight='bold')
ax.text(out_x + out_w / 2, out_y + out_h * 0.15,
        'position_error\nsignal_sum\npeak_value\ncontrast_value\nline_confidence\nedge_hint',
        fontsize=SMALL_SIZE - 0.5, ha='center', va='center', color=NEUTRAL_DARK)

# Arrow from last block to output
draw_arrow(ax, out_x - gap - 0.05, blocks_y + block_h / 2,
           out_x, out_y + out_h / 2, color=NEUTRAL_MED)

save_figure(fig, 'fig4_sensor_pipeline.pdf')

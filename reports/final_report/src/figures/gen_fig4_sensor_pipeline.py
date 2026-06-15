#!/usr/bin/env python3
"""图 4 — 传感器处理管线流程图。

6 阶段串行管线，每阶段一个圆角框 + 箭头串联，
最右端为输出框。宽画布 + 大间距 + 压缩高度。
"""

from figure_style import *

# ----------------------------------------------------------
# 初始化 — 宽画布，压缩高度
# ----------------------------------------------------------
fig, ax = setup_style()
fig.set_size_inches(8.0, 2.4)

# ----------------------------------------------------------
# 管线参数
# ----------------------------------------------------------
blocks = [
    ("UART Frame",        "8-byte frame\n(0x55 header)"),
    ("Channel\nExtraction", "ch0..ch7\nper-channel"),
    ("Normalization",     "per-channel\nmin/max calib"),
    ("Low-Pass\nFilter",  "IIR\nα = 0.35"),
    ("Polarity\nInversion","invert =\ntrue"),
    ("Weighted\nAverage", "{−1750 .. +1750}\nsub-sensor res."),
]

block_w  = 0.95          # 加宽框体，防止文字溢出
block_h  = 0.65          # 配合压缩高度
gap      = 0.35          # 大幅加大框间距
out_gap  = 0.35
out_w    = 0.50

# 水平布局 — 居中
total_pipe = len(blocks) * block_w + (len(blocks) - 1) * gap
full_w     = total_pipe + out_gap + out_w
ax.set_xlim(0, 8.5)
start_x  = (8.5 - full_w) / 2

# 垂直布局 — 居中于压缩画布
ax.set_ylim(0, 2.2)
blocks_y = (2.2 - block_h) / 2   # ≈ 0.775

# ----------------------------------------------------------
# 绘制管线框
# ----------------------------------------------------------
for i, (title, desc) in enumerate(blocks):
    x = start_x + i * (block_w + gap)

    # 输入框用稍深底色表示数据源
    fc = '#5DADE2' if i == 0 else PRIMARY_LIGHT

    draw_box(ax, x, blocks_y, block_w, block_h,
             facecolor=fc, edgecolor=PRIMARY,
             linewidth=1.3)

    # 阶段编号 — 左上角（带圈数字 Unicode ①~⑥）
    txt_color = 'white' if i == 0 else PRIMARY_DARK
    circled = chr(0x2460 + i)
    ax.text(x + 0.06, blocks_y + block_h - 0.06,
            circled,
            fontsize=BODY_SIZE, ha='left', va='top',
            color=txt_color, fontweight='bold')

    # 阶段名称（标题）— 8pt 加粗，深色
    ax.text(x + block_w / 2, blocks_y + block_h * 0.72,
            title,
            fontsize=LABEL_SIZE, ha='center', va='center',
            fontweight='bold', color=txt_color)

    # 描述文字 — 6.5pt 常规体，灰色
    desc_color = '#EAECEE' if i == 0 else NEUTRAL_MED
    ax.text(x + block_w / 2, blocks_y + block_h * 0.22,
            desc,
            fontsize=SMALL_SIZE, ha='center', va='center',
            fontweight='normal', color=desc_color)

# ----------------------------------------------------------
# 框间箭头 — PRIMARY_DARK，线宽 1.3（深于框体）
# ----------------------------------------------------------
y_mid = blocks_y + block_h / 2
for i in range(len(blocks) - 1):
    x1 = start_x + i * (block_w + gap) + block_w
    x2 = start_x + (i + 1) * (block_w + gap)
    draw_arrow(ax, x1, y_mid, x2, y_mid,
               color=PRIMARY_DARK, lw=1.3)

# ----------------------------------------------------------
# 输出框 — 与管线框 y 中心完全对齐，绿色边框
# ----------------------------------------------------------
x_last = start_x + (len(blocks) - 1) * (block_w + gap)
out_x  = x_last + block_w + out_gap
out_y  = blocks_y

draw_box(ax, out_x, out_y, out_w, block_h,
         facecolor='#E8F8F5', edgecolor=ACCENT_GREEN,
         linewidth=1.5)

# "Outputs" 标题 — 8pt 加粗
ax.text(out_x + out_w / 2, out_y + block_h * 0.88,
        'Outputs',
        fontsize=LABEL_SIZE, ha='center', va='top',
        color=NEUTRAL_DARK, fontweight='bold')

# 变量名逐行绘制 — 等宽字体，均匀分布在框内
var_names = [
    'pos_err',
    'sig_sum',
    'peak',
    'contrast',
    'conf',
    'edge_hint',
]
n_vars   = len(var_names)
line_top = out_y + block_h * 0.70
line_bot = out_y + block_h * 0.10
line_step = (line_top - line_bot) / (n_vars - 1)

for j, vname in enumerate(var_names):
    y_line = line_top - j * line_step
    ax.text(out_x + out_w / 2, y_line, vname,
            fontsize=SMALL_SIZE, ha='center', va='center',
            color=NEUTRAL_DARK, fontfamily='monospace')

# 水平箭头：最后一个管线框 → 输出框
draw_arrow(ax, x_last + block_w, y_mid,
           out_x, y_mid,
           color=PRIMARY_DARK, lw=1.3)

# ----------------------------------------------------------
# 保存
# ----------------------------------------------------------
save_figure(fig, 'fig4_sensor_pipeline.pdf')

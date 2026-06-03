#!/usr/bin/env python3
"""TDPS 最终报告 — 统一图表设计语言。
所有 gen_fig*.py 脚本从此模块导入颜色、字号、辅助函数，
确保 8 张图视觉一致。

Usage:
    from figure_style import *
    fig, ax = setup_style()  # single-column, returns (fig, ax, figsize)
    fig, ax = setup_style(double_col=True)
    draw_box(ax, x, y, w, h, PRIMARY, PRIMARY_DARK, "text")
    save_figure(fig, "output_name")
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
import os

# ============================================================
# 统一调色板
# ============================================================

# 主色调（层架构、标题、边框）
PRIMARY_DARK   = '#003366'
PRIMARY        = '#1A5276'
PRIMARY_LIGHT  = '#85C1E9'

# 语义强调色 — 全报告统一，同一概念同一颜色
ACCENT_RED     = '#C0392B'   # P增益、避障、错误/失败
ACCENT_BLUE    = '#2980B9'   # D增益、数据流
ACCENT_GREEN   = '#27AE60'   # FF前馈、成功/完成
ACCENT_ORANGE  = '#E67E22'   # 速度函数、恢复状态
ACCENT_YELLOW  = '#F39C12'   # 限幅、警告

# 中性色
NEUTRAL_DARK   = '#2C3E50'   # 主要文字、边框
NEUTRAL_MED    = '#7F8C8D'   # 次要文字、箭头
NEUTRAL_LIGHT  = '#EAECEE'   # 浅灰背景

# 状态机专用
STATE_GREEN    = '#1E8449'   # RUNNING/正常
STATE_GREEN_LT = '#D5F5E3'   # WAIT/CALIBRATING
STATE_RED      = '#C0392B'   # AVOID_*
STATE_RED_LT   = '#F5B7B1'   # AVOID_* 浅底
STATE_GRAY     = '#95A5A6'   # STOPPED/终端

# 层架构渐变（硬件→应用，深→浅）
LAYER_GRADIENT = ['#0D3B66', '#1A5276', '#2874A6', '#5499C7']

# ============================================================
# 统一字号
# ============================================================
LABEL_SIZE  = 8     # 坐标轴标签、图例、主元素标题
BODY_SIZE   = 7     # 元素内文本、参数值
SMALL_SIZE  = 6.5   # 注释、次要标注（绝对底线）

# ============================================================
# 统一图片尺寸
# ============================================================
SINGLE_COL_FIGSIZE = (6, 4.5)
DOUBLE_COL_FIGSIZE = (12, 5)

# 输出目录
OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))


def setup_style(double_col=False, keep_axes=False):
    """配置全局 rcParams，返回 (fig, ax) 和合适的 figsize。

    Args:
        double_col: True 使用双栏宽 (12, 5)，False 使用单栏 (6, 4.5)
        keep_axes: True 保留坐标轴（用于数据图），False 隐藏坐标轴（框图）

    Returns:
        (fig, ax): matplotlib figure and axes objects
    """
    figsize = DOUBLE_COL_FIGSIZE if double_col else SINGLE_COL_FIGSIZE

    plt.rcParams.update({
        'font.size': BODY_SIZE,
        'font.family': 'sans-serif',
        'font.sans-serif': ['DejaVu Sans', 'Arial', 'Helvetica'],
        'axes.titlesize': LABEL_SIZE,
        'axes.labelsize': LABEL_SIZE,
        'lines.linewidth': 1.2,
        'pdf.fonttype': 42,
        'ps.fonttype': 42,
    })

    fig, ax = plt.subplots(1, 1, figsize=figsize)
    if not keep_axes:
        ax.axis('off')
    return fig, ax


# ============================================================
# 共享绘图辅助函数
# ============================================================

def draw_box(ax, x, y, w, h, facecolor, edgecolor=None, text='',
             fontsize=None, fontweight='bold', text_color='white',
             linewidth=1.3, alpha=0.92, padding=0.06):
    """绘制圆角矩形，可选居中文字。

    Args:
        ax: matplotlib axes
        x, y, w, h: 位置和大小（数据坐标）
        facecolor, edgecolor: 填充色和边框色（edgecolor=None 则比 facecolor 深）
        text: 居中文字，支持 \n 换行
        fontsize: 字号（默认 BODY_SIZE）
        fontweight: 'bold' 或 'normal'
        text_color: 文字颜色
        linewidth: 边框宽度
        alpha: 透明度
        padding: 圆角半径参数
    """
    if edgecolor is None:
        edgecolor = NEUTRAL_DARK
    if fontsize is None:
        fontsize = BODY_SIZE

    box = FancyBboxPatch((x, y), w, h,
                         boxstyle=f"round,pad={padding}",
                         facecolor=facecolor, edgecolor=edgecolor,
                         linewidth=linewidth, alpha=alpha)
    ax.add_patch(box)

    if text:
        ax.text(x + w / 2, y + h / 2, text,
                fontsize=fontsize, ha='center', va='center',
                color=text_color, fontweight=fontweight)


def draw_arrow(ax, x1, y1, x2, y2, color=None, lw=1.0,
               style='->', connectionstyle=None):
    """绘制箭头。

    Args:
        ax: matplotlib axes
        x1, y1, x2, y2: 起止坐标
        color: 箭头颜色（默认 NEUTRAL_MED）
        lw: 线宽
        style: 箭头样式，'->' 或 '-'
        connectionstyle: 弧线样式，如 'arc3,rad=0.2'
    """
    if color is None:
        color = NEUTRAL_MED

    props = dict(arrowstyle=style, color=color, lw=lw)
    if connectionstyle:
        props['connectionstyle'] = connectionstyle

    ax.annotate('', xy=(x2, y2), xytext=(x1, y1), arrowprops=props)


def draw_state_box(ax, x, y, w, h, name, desc='', fc=None, ec=None,
                   name_size=None, desc_size=None):
    """绘制状态机节点（大 box + 标题 + 描述）。

    Args:
        ax: matplotlib axes
        x, y, w, h: 位置和大小
        name: 状态名称（如 'RUNNING'）
        desc: 描述文字，支持 \n 换行
        fc: 填充色
        ec: 边框色
        name_size: 状态名字号
        desc_size: 描述字号
    """
    if fc is None:
        fc = PRIMARY
    if ec is None:
        ec = PRIMARY_DARK
    if name_size is None:
        name_size = BODY_SIZE
    if desc_size is None:
        desc_size = SMALL_SIZE

    # 判断浅色/深色背景来选择文字颜色
    is_light_bg = fc in [STATE_GREEN_LT, STATE_RED_LT, NEUTRAL_LIGHT]
    tc = NEUTRAL_DARK if is_light_bg else 'white'

    box = FancyBboxPatch((x, y), w, h,
                         boxstyle="round,pad=0.08",
                         facecolor=fc, edgecolor=ec, linewidth=1.5)
    ax.add_patch(box)

    # 状态名（上部）
    if name:
        ax.text(x + w / 2, y + h * 0.72, name,
                fontsize=name_size, ha='center', va='center',
                color=tc, fontweight='bold')

    # 描述文字（下部）
    if desc:
        ax.text(x + w / 2, y + h * 0.28, desc,
                fontsize=desc_size, ha='center', va='center',
                color=tc, fontstyle='italic')


def save_figure(fig, name):
    """保存图片到 figures/ 目录，自动清理并关闭。

    Args:
        fig: matplotlib figure
        name: 文件名（不含路径），如 'fig4_sensor_pipeline.pdf'
    """
    outpath = os.path.join(OUTPUT_DIR, name)
    fig.savefig(outpath, format='pdf', bbox_inches='tight', dpi=300, pad_inches=0.05)
    plt.close(fig)
    print(f'[OK] {outpath}')

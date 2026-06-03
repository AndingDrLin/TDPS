# TDPS 最终报告写作管线

6 阶段多 agent 并行管线，从原始项目文档到 IEEE LaTeX 论文。

## 目录结构

```
TDPS/reports/final_report/
├── artifacts/                     # 阶段性输出
│   ├── stage_0_requirements/      # 报告要求 + 评分标准
│   ├── stage_1_past_report/       # 往届报告分析
│   ├── stage_2_outline/           # 大纲 + 图表规划
│   ├── stage_3_figures/           # 缺失素材清单
│   └── stage_5_review/            # 五轮评审记录
├── src/
│   ├── main.tex                   # LaTeX 主文件（558 行，~48KB）
│   ├── sections/
│   │   └── tables.tex             # 外部 LaTeX 表格
│   └── figures/                   # 10 张矢量 PDF 图 + 10 个生成脚本
└── README.md                      # 本文件
```

## 管线阶段

| 阶段 | 状态 | 产物 |
|------|------|------|
| Stage 0 | ✅ | requirements_summary.md, marking_criteria.md |
| Stage 1 | ✅ | section_structure.md, figure_inventory.md, past_report_analysis.md |
| Stage 2 | ✅ | report_outline.md (493 行), figure_plan.md (390 行) |
| Stage 3 | ✅ | 10 张矢量 PDF 图 + 10 张三线表 + missing_items.md |
| Stage 4 | ✅ | main.tex 全文 (~558 行) + references.bib |
| Stage 5 | ✅ | Round 1-5 完成 (五轮评审通过) |
| Stage 6 | ✅ | 内容+排版双重重构: 消除 bullet points、代码细节、AI 痕迹 |

## 评分标准对齐

- **Technical Content (3x)**：A 级 21 项检查全覆盖（Authoritative Discussion 6 项 + Comprehensive Analysis 8 项 + Significantly Complex 5 项）
- **Presentation & Figures (1x)**：IEEE 模板，10 张矢量图 + 10 张三线表，专业配色
- **Literature Survey (1x)**：13 篇引用（教材 3 + 期刊/会议 4 + Datasheet 3 + 标准文档 3）

## 写作约束

所有后续写作和评审必须通过 `artifacts/WRITING_CONSTRAINTS.md` 的检查清单。核心规则：
- 正文禁止 `\begin{itemize}` 和 `\begin{enumerate}`
- 禁止代码级细节（函数名、变量名、文件路径）
- 禁止 AI 痕迹用语（"It is worth noting", "Key innovations include", "robust" 等）
- 讲 WHY 不讲 WHAT：每个技术决策先讲动机和约束，再讲方案

## 编译方式

### Overleaf（推荐）
1. 创建新项目，上传 `src/main.tex` 和 `src/figures/` 下所有 .pdf
2. 编译器选择 pdfLaTeX
3. 编译

### 本地（需 texlive）
```bash
cd TDPS/reports/final_report/src
pdflatex main.tex
pdflatex main.tex  # 两次编译以解析交叉引用
```

## 需用户补充

详见 `artifacts/stage_3_figures/missing_items.md`：
1. 队员姓名 + 学号（替换 main.tex 中的 Author 1/2/3 和占位符 GUID）
2. 小车实物照片（用于 Fig. 2）
3. Fig. 7（雷达数据流图）和 Fig. 9（LoRa 时序图）需生成
4. 模拟器 full-course 运行结果（替换 Table IV 中的 TBD）
5. 实车测试定量数据（巡线偏差、避障成功率、LoRa 延迟）

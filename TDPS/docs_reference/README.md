# 硬件与厂商参考资料索引

本目录用于本地存放硬件手册、PCB 文件、课程 slides、厂商示例和工具包。为避免仓库过大，Git 默认只跟踪本 README，PDF、rar、zip、exe、图片、PCB 工程和厂商包不提交。

## 常用资料位置

| 资料 | 本地路径 |
|---|---|
| DMG48270C043_04WN 屏幕数据手册 | `TDPS/docs_reference/DMG48270C043_04WN_数据手册.pdf` |
| EWM22A LoRa 用户手册 | `TDPS/docs_reference/EWM22A-xxxBWL22S_UserManual_CN_V1.0.pdf` |
| HLK-LD2410S 用户手册 | `TDPS/docs_reference/HLK-LD2410S-用户手册_V1.04  .pdf` |
| PCB 工程文件 | `TDPS/docs_reference/ProPrj_TDPS PCB_2026-06-03.epro2`（EasyEDA Pro） |
| STM32F407VET6 开发板资料 | `TDPS/docs_reference/STM32F407VET6开发板/` |
| Wheeltec L150 小车底盘资料 | `TDPS/docs_reference/Wheeltec L150小车底盘/` |
| 8 路巡线红外传感器资料 | `TDPS/docs_reference/八路巡线红外传感器/` |
| 课程 slides | `TDPS/docs_reference/course_slides/` |

## 使用建议

- 需要查硬件细节时，先看 `TDPS/agent_docs/01_hardware_reference.md`，再查本目录原始资料。
- 不要把厂商工具包、压缩包、PDF 或 exe 强行加入 Git。
- 如果某份资料成为关键依据，在 `agent_docs` 或 `docs/` 中写摘要和路径，而不是提交原始大文件。

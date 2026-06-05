# Agent 工作注意事项

## 协作原则

- 默认中文回复。
- 先读 `TDPS/agent_docs/00_project_brief.md`、`01_hardware_reference.md`、`02_current_progress.md`，再改代码。
- 固件逻辑改动前先查看 `TDPS/docs/basic_design.md`、`TDPS/docs/line_following.md`、`TDPS/docs/tuning.md`。
- 不要阻塞巡线主循环；LoRa 和雷达服务必须非阻塞 tick 推进。
- 调参优先改 `TDPS/firmware/Src/lf_config.c` 或 `TDPS/firmware/Src/lf_config_profiles.c` 的 profile 参数，每次只改少量参数并记录结果。
- 当前巡线使用 **6 参数简化 PD + kff 架构**（`TDPS_SIMPLE_CONTROL=1`）。6 个核心参数：`kp`, `kd`, `kff`, `base_speed`, `min_speed`, `max_correction`。死区/软区/积分/速度缩放/变化率限幅均已关闭。如需回到旧 62 参数架构，将 `TDPS_SIMPLE_CONTROL` 改为 0。
- 当前实车入口默认使用 `LF_Config_ApplyDebugProfile()`；除非用户明确要求 competition，否则不要切换到 `LF_Config_ApplyCompetitionProfile()`。

## 巡线传感器 D帧协议（重要）

Yahboom 8-LP UART 模式使用 ASCII 协议，命令 `$0,1,1#` 同时请求模拟帧和数字帧：

- **模拟 A帧**：`$Ax1:xxx,x2:xxx,...,x8:xxx#`，xxx = 0~4095
- **数字 D帧**：`$Dx1:x,x2:x,...,x8:x#`，x = 0 或 1

**D帧极性（实测）**：
- `0` = 检测到黑线 = 传感器 LED 灯亮 = **"亮"**
- `1` = 未检测到 = 传感器 LED 灯灭 = **"灭"**

这是最容易踩的坑：D帧的 `0` 才是"亮"，不是 `1`。硬路线脚本的 `route_lane_on_now()` 和 `route_lane_off_now()` 必须遵守这个极性。

## 赛道固定路线脚本

赛道顺序：直角左弯 → 直行 → T字路口右弯 → 直角左弯 → 直行过两个十字路口 → 直角左弯。

硬事件检测只用 D帧数字值（`route_lane_on_now`/`route_lane_off_now`），不依赖模拟 A帧、`instant_norm`、`line_detected` 等：

- **全亮（T口/十字）**：8路 D帧全为 `0`。
- **左直角**：右侧外侧（通道8 = index 7）D帧 `1`（灭），通道 1~6（index 0~5）D帧 `0`（亮）；或左侧外侧（通道1 = index 0）D帧 `1`（灭），通道 3~8（index 2~7）D帧 `0`（亮）。
- 两种缺口统一按左直角处理，原地左转 90°。

路线阶段机（`route_script_try_match`）不走事件冷却，只通过"离开后重新武装"防重复触发。

固定 90°动作参数：`fixed_turn_spin_speed=200`，`fixed_turn_90_ms_left/right=720ms`，`fixed_turn_settle_ms=80ms`。

当前问题：直角处电机抽动后丢线、T口有趋势但不旋转。疑似 `set_opposite_spin_command()` 或 `max_motor_delta`/`motor_deadband` 与原地旋转命令冲突。

## 常用离线构建

在 `TDPS/` 下运行：

```bash
mkdir -p firmware/build/gcc

gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{lf_app,lf_control,lf_sensor,lf_radar,lf_chassis,lf_config,lf_config_profiles,lf_line_features,lf_speed_math,lf_platform_stub,lf_debug_monitor,lf_future_hooks,lf_led_blink,lf_run_log,wireless_hooks,wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_lf_stub.c -o firmware/build/gcc/lf_test -lm
./firmware/build/gcc/lf_test

gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_wl_stub.c -o firmware/build/gcc/wl_test
./firmware/build/gcc/wl_test
```

如安装 CMake：

```bash
cmake -S firmware -B firmware/build -DTDPS_TARGET_MCU=OFF -DTDPS_BUILD_TESTS=ON
cmake --build firmware/build
./firmware/build/test_lf
./firmware/build/test_wl
```

## 仿真说明

`TDPS/simulator/` 来自原 `TDPS-Simulator`。如果脚本仍假设旧路径 `TDPS-Simulator/`，不要在不理解脚本路径逻辑的情况下直接批量替换；先读 `TDPS/simulator/scripts/`，再决定是更新脚本还是从兼容路径运行。最近一次 quick 验证是手动用当前 `simulator/` 路径编译 runner 后运行的。

常见入口文件：

- `TDPS/simulator/scripts/line_follow_cli.sh`
- `TDPS/simulator/scripts/run_line_follow_autotest.sh`
- `TDPS/simulator/scripts/run_line_follow_stability.sh`
- `TDPS/simulator/scripts/run_radar_autotest.sh`
- `TDPS/simulator/scripts/run_wireless_autotest.sh`

## 板级测试顺序

参考 `TDPS/tests/board/tdps_board_test/README.md`：

1. 上电前检查短路。
2. 限流上电，测量 5V 和 3.3V。
3. 用 ST-Link 下载/调试。
4. 看 `g_board_swd.magic`、`uptime_ms`、`sysclk_hz`。
5. 执行被动自检。
6. 测 GPIO、编码器、雷达、巡线、LoRa。
7. 架空轮子后低占空比短时测试电机。
8. 全部单项通过后再跑完整控制。

## Git 与文件管理

- `TDPS/docs_reference/` 只提交 README 索引，本地 PDF、rar、exe、图片、PCB 文件默认不进 Git。
- `TDPS/firmware/build/`、`TDPS/simulator/artifacts/`、报告 outputs 不提交。
- `firmware` 内可以保留必要 STM32/Keil/CubeMX 工程依赖；非 firmware 目录不保留 Keil/CubeMX 附加工程和编译产物。
- 对业务源码做路径移动时，尽量不要同时改内容，便于审查 rename。

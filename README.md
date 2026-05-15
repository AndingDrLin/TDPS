# TDPS

TDPS 是 Team 15 智能巡线小车固件，目标 MCU 为 **STM32F407VET6**。当前固件统一放在 `firmware/`：

- 巡线控制：8 路灰度传感器、标定、归一化、滤波、PID、丢线恢复
- 雷达避障：HLK-LD2410S 串口帧解析，输出 `CLEAR/WARN/BLOCK`，阻塞时进入开环绕障并重新寻线
- 无线通信：EWM22A-900BWL22S LoRa 异步队列、超时重试、可选 ACK

## 任务要求

- 巡线：从起点沿 3 cm 黑线稳定行驶，支持弯道/方框区域，丢线后按最后方向恢复，恢复超时必须停车。
- 雷达避障：在障碍区前通过 HLK-LD2410S 判断前方障碍，`WARN` 时减速，`BLOCK` 时绕开障碍并重新寻线；当前只有前向距离信息，左右绕行方向由配置、上次线方向和失败反向重试决定。
- 无线通信：在检查点事件发生时通过 EWM22A LoRa 发送 `TEAM/NAME/CP/TIME` 报文，发送必须异步推进，不能阻塞巡线控制。
- 调试测试：PC stub、CMake 或 gcc 构建、Simulator 回归和 debug monitor 都应可用于离线/上板验证。

## 快速测试

PC stub 编译（不依赖 STM32 HAL）：

```bash
mkdir -p firmware/build/gcc

gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{lf_app,lf_control,lf_sensor,lf_radar,lf_chassis,lf_config,lf_platform_stub,lf_debug_monitor,lf_future_hooks,wireless_hooks,wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_lf_stub.c -o firmware/build/gcc/lf_test -lm
./firmware/build/gcc/lf_test

gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_wl_stub.c -o firmware/build/gcc/wl_test
./firmware/build/gcc/wl_test
```

如本机安装了 CMake：

```bash
cmake -S firmware -B firmware/build -DTDPS_TARGET_MCU=OFF -DTDPS_BUILD_TESTS=ON
cmake --build firmware/build
./firmware/build/test_lf
./firmware/build/test_wl
```

Simulator 回归脚本依赖 `TDPS-Simulator` 子模块。当前子模块提供的脚本为：

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick
bash TDPS-Simulator/scripts/line_follow_cli.sh full-course \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/single_run/full_course_stress.json \
  20260319 stress
bash TDPS-Simulator/scripts/run_line_follow_stability.sh
```

Simulator runner 现在直接编译统一 `firmware/` 树，固定 8 路传感器，并覆盖 normal/stress 扰动、完整赛道、雷达障碍、检查点和无线队列诊断。

## 关键行为

- 巡线输入固定为 8 路，支持 ADC 模拟量和 8-LP 数字 GPIO 模式。
- 雷达串口读取由 F407 HAL 端口的中断环形缓冲提供，不阻塞主控制环。
- 雷达解析支持保留测试帧、LD2410S 最小帧和已知标准帧目标/距离字段；距离统一换算为 mm。
- `WARN` 状态减速，`BLOCK` 状态进入 `AVOID_PREP -> TURN_OUT -> BYPASS -> TURN_IN -> REACQUIRE`，失败后反向重试。
- 无线发送由异步队列驱动，检查点事件入队后由 `WL_LoRa_Tick()` 非阻塞推进。
- 集成入口：`LF_App_NotifyCheckpoint(checkpoint_id)`。
- 报文格式：`TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<ms>\n`。

## 文档

- 固件入口：`firmware/`
- 整车调参说明：`docs/tuning.md`
- 无线设计文档：`docs/wireless_comm.md`
- 巡线说明：`docs/line_following.md`
- 测试说明：`docs/testing/`

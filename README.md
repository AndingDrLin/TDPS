# TDPS

TDPS 是 Team 15 智能巡线小车固件，目标 MCU 为 **STM32F407VET6**。当前固件统一放在 `firmware/`：

- 巡线控制：8 路灰度传感器、标定、归一化、滤波、PID、丢线恢复
- 雷达避障：HLK-LD2410S 串口帧解析，输出 `CLEAR/WARN/BLOCK`
- 无线通信：EWM22A-900BWL22S LoRa 异步队列、超时重试、可选 ACK

## 快速测试

PC stub 编译（不依赖 STM32 HAL）：

```bash
gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{lf_app,lf_control,lf_sensor,lf_radar,lf_chassis,lf_config,lf_platform_stub,lf_future_hooks,wireless_hooks,wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_lf_stub.c -o lf_test -lm

 gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_wl_stub.c -o wl_test
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
bash TDPS-Simulator/scripts/run_line_follow_autotest.sh
bash TDPS-Simulator/scripts/run_line_follow_stability.sh
```

注意：当前 Simulator 子模块仍按旧巡线工程布局生成 runner，完整联动前需要同步更新子模块的源码路径和 8 路传感器假设。

## 关键行为

- 巡线输入固定为 8 路，支持 ADC 模拟量和 8-LP 数字 GPIO 模式。
- 雷达串口读取由 F407 HAL 端口的中断环形缓冲提供，不阻塞主控制环。
- 无线发送由异步队列驱动，检查点事件入队后由 `WL_LoRa_Tick()` 非阻塞推进。
- 集成入口：`LF_App_NotifyCheckpoint(checkpoint_id)`。
- 报文格式：`TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<ms>\n`。

## 文档

- 固件入口：`firmware/`
- 无线设计文档：`docs/wireless_comm.md`
- 巡线说明：`docs/line_following.md`
- 测试说明：`docs/testing/`

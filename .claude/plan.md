# 弯道处理逻辑重构方案

## 问题诊断

**现象**：直线稳定，U 型弯最后转弯不足，出弯时车偏左于线。

**根因分析**（3 个因素叠加）：

1. **PID 被强制重置**：`process_curve_arc()` 每帧调用 `LF_Control_ResetPid()`，导致出弯瞬间 PID 从零状态启动。第一帧只有 `kp * error ≈ 17.5` 的修正，D 项为零，远不足以修正残留偏差。

2. **curve_arc 差速不足**：当前 `curve_arc_delta=110`，`curve_arc_max_motor_delta=130`，对 U 型弯来说转弯半径偏大。

3. **Probe 机制适得其反**：进弯前用小差速(delta=35)试探 3 帧，浪费了最需要转弯响应的时间窗口，加重了转弯不足。

## 设计方案：精简两阶段 curve_arc

### 核心思路

**删除 Probe 机制，保留 curve_arc 但修复其出弯过渡。** curve_arc 期间不更新 PID 状态（PID 天然冻结），出弯时 PID 从冻结状态恢复，实现平滑过渡。

### 改动前的控制流

```
PID → [进弯] → PROBE(3帧,小差速) → CURVE_ARC(固定差速,每帧reset PID) → [出弯] → PID(零状态重启)
                                                                                    ↑ 突变点
```

### 改动后的控制流

```
PID → [进弯] → CURVE_ARC(固定差速,PID冻结) → [出弯] → PID(从冻结状态恢复)
                                                         ↑ 平滑过渡
```

**关键机制**：curve_arc 不调用 `LF_Control_UpdatePD()`，所以 PID 的 `prev_error`、`filtered_derivative`、`prev_output` 保持在进弯前最后一帧的值。出弯时 PID 的 D 项从合理值开始，而非从零开始。

### 删除的内容

| 删除项 | 文件 | 说明 |
|--------|------|------|
| `LF_RUN_ACTION_CURVE_ARC_PROBE` 枚举值 | `lf_app.c` | 整个 probe 模式 |
| `process_curve_arc_probe()` 函数 | `lf_app.c` | probe 执行函数 |
| probe 分支（`if curve_arc_side != 0` 未确认部分） | `lf_app.c:1250-1252` | 仲裁逻辑 |
| `LF_Chassis_SetCommandWithDeltaLimit()` | `lf_chassis.c` + `.h` | 只有 curve_arc/probe 在用，改为统一用 `SetCommand` |
| `curve_arc_dir_sign` 参数 | `lf_config.h` | 改为自动跟随曲线方向 |
| `curve_arc_probe_speed` / `curve_arc_probe_delta` / `curve_arc_probe_max_motor_delta` | `lf_config.h` | probe 专属参数 |

### 修改的内容

#### 1. `process_curve_arc()` — 简化 + 去 PID reset

```c
static void process_curve_arc(void)
{
    int16_t speed = limit_degraded_speed(g_lf_config.curve_arc_speed);
    int16_t delta = limit_degraded_speed(g_lf_config.curve_arc_delta);
    int16_t motor_delta_limit = g_lf_config.curve_arc_max_motor_delta;
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;

    reset_lead_phase();
    /* 不重置 PID：出弯时从冻结状态恢复，避免 D=0 导致修正不足 */

    correction = (int16_t)((int16_t)s_app.curve_arc_side * delta);
    LF_Control_ComputeMotorCmd(speed, correction, &left_cmd, &right_cmd);
    if (motor_delta_limit <= 0) {
        motor_delta_limit = delta;
    }
    limit_motor_delta(&left_cmd, &right_cmd, motor_delta_limit);
    LF_Platform_SetMotorCommand(-left_cmd, -right_cmd);
    s_app.current_target_speed = speed;
}
```

改动要点：
- **删除** `LF_Control_ResetPid()` → PID 冻结在进弯前状态
- **删除** `curve_arc_dir_sign` → `correction = curve_arc_side * delta`
- **替换** `SetCommandWithDeltaLimit` → 直接调用 `limit_motor_delta` + `SetMotorCommand`

#### 2. `arbitrate_running_action()` 中 curve_arc 部分 — 去 probe 分支

改动前（第 1246-1253 行）：
```c
if (s_app.curve_arc_side != 0 && s_app.curve_arc_count >= confirm_ticks) {
    d.action = LF_RUN_ACTION_CURVE_ARC;
    return d;
}
if (s_app.curve_arc_side != 0) {
    d.action = LF_RUN_ACTION_CURVE_ARC_PROBE;
    return d;
}
```

改动后：
```c
if (s_app.curve_arc_side != 0 && s_app.curve_arc_count >= confirm_ticks) {
    d.action = LF_RUN_ACTION_CURVE_ARC;
    return d;
}
```

#### 3. `process_running()` switch — 删除 probe case

删除：
```c
case LF_RUN_ACTION_CURVE_ARC_PROBE:
    process_curve_arc_probe();
    return;
```

#### 4. `lf_config.h` — 删除字段

删除：
```c
int8_t  curve_arc_dir_sign;
int16_t curve_arc_probe_speed;
int16_t curve_arc_probe_delta;
int16_t curve_arc_probe_max_motor_delta;
```

保留：
```c
bool    curve_arc_enable;
int16_t curve_arc_speed;
int16_t curve_arc_delta;
int16_t curve_arc_max_motor_delta;
uint8_t curve_arc_confirm_ticks;
uint8_t curve_arc_release_ticks;
```

#### 5. `lf_chassis.c` / `lf_chassis.h` — 删除 `SetCommandWithDeltaLimit`

#### 6. `lf_config_profiles.c` — 解决合并冲突 + 更新参数

解决冲突，保留 HEAD 侧，去掉 dir_sign，更新参数值：

```c
g_lf_config.curve_arc_enable         = true;
g_lf_config.curve_arc_speed          = 80;    // 弯道中低速(原105→80)
g_lf_config.curve_arc_delta          = 140;   // 加大差速(原110→140)
g_lf_config.curve_arc_max_motor_delta = 150;  // 加大限制(原130→150)
```

`max_motor_delta`（直线用）保持 70 不变。

#### 7. 测试文件更新

- 删除 `test_curve_arc_probes_before_confirmed_turn` 测试
- 修改 `test_curve_arc_uses_independent_small_delta`：验证 PID 状态在 curve_arc 期间不被重置
- 修改 `test_side_wide_lamps_enter_curve_arc_before_edge_realign`：去掉 dir_sign 设置

### 参数变化总结

| 参数 | 旧值 | 新值 | 说明 |
|------|------|------|------|
| `curve_arc_enable` | true | true | 不变 |
| `curve_arc_dir_sign` | -1 | **删除** | 自动跟随曲线方向 |
| `curve_arc_speed` | 105 | **80** | 弯道更慢，留更多修正余量 |
| `curve_arc_delta` | 110 | **140** | 更大差速，减小转弯半径 |
| `curve_arc_max_motor_delta` | 130 | **150** | 匹配更大的 delta |
| `curve_arc_confirm_ticks` | 3 | **1** | 立即进入弯道模式 |
| `curve_arc_release_ticks` | 4 | 4 | 不变 |
| `curve_arc_probe_speed` | 110 | **删除** | |
| `curve_arc_probe_delta` | 35 | **删除** | |
| `curve_arc_probe_max_motor_delta` | 50 | **删除** | |
| `max_motor_delta`（直线） | 70 | 70 | 不变 |

**净减少 4 个参数**（删 5 个，加 0 个）。

## 改动文件清单

| 文件 | 改动类型 |
|------|----------|
| `TDPS/firmware/Inc/lf_config.h` | 删除 4 个字段 |
| `TDPS/firmware/Inc/lf_chassis.h` | 删除 1 个声明 |
| `TDPS/firmware/Src/lf_app.c` | 删除 probe 函数/枚举/分支；修改 curve_arc 函数 |
| `TDPS/firmware/Src/lf_chassis.c` | 删除 SetCommandWithDeltaLimit |
| `TDPS/firmware/Src/lf_config_profiles.c` | 解决合并冲突 + 更新参数 |
| `TDPS/firmware/test/test_lf_stub.c` | 删除 probe 测试，更新 curve_arc 测试 |

## 验证步骤

1. gcc 编译测试：`gcc ... test_lf_stub.c -o lf_test && ./lf_test`
2. 重点关注：
   - curve_arc 进入时 PID 不被重置（新增断言）
   - curve_arc 直接确认（confirm_ticks=1，无 probe 阶段）
   - curve_arc delta 使用新方向逻辑（无 dir_sign）
3. 合并冲突解决后编译通过

# 巡线子系统技术报告（含可学习代码片段）

## 1. 任务目标与赛道约束
巡线系统的目标是让小车从起点稳定到达终点，并正确通过以下五类关键路段：

1. 长直线。
2. U 型发卡弯。
3. 连续 S 弯。
4. 矩形路线（直线 + 直角角点）。
5. 圆形路线（长时间同向转弯）。

除巡线外，小车还要完成 LoRa 通信和雷达避障。因此，巡线系统必须支持“底盘控制权临时交接”。

## 2. 参考开源方案与吸收内容
本项目参考了以下开源项目，并只吸收可复用的工程方法：

1. [SayanSeth/Line-Follower-Robot](https://github.com/SayanSeth/Line-Follower-Robot) 多路传感器估计线中心、差速控制基本框架。
2. [sametoguten/STM32-Line-Follower-with-PID](https://github.com/sametoguten/STM32-Line-Follower-with-PID) STM32 上的 PID 调参与标定流程。
3. [navidadelpour/line-follower-robot](https://github.com/navidadelpour/line-follower-robot) 模块解耦（驱动、感知、控制、任务逻辑分层）。
4. [FredBill1/RT1064_Smartcar](https://github.com/FredBill1/RT1064_Smartcar)
   与 [ittuann/Enterprise_E](https://github.com/ittuann/Enterprise_E) 比赛级状态机、复杂赛道元素分段处理。

## 3. 硬件选型（巡线 + LoRa + 雷达避障）

### 3.1 主控
推荐主控：`STM32F407VET6`，可选`STM32G431CBT6`或`STM32H743VIT6`。

### 3.2 外设
1. 巡线传感器：8 路反射式阵列（最低可 4 路）。
2. 电机反馈：左右轮编码器。
3. 姿态反馈：IMU（用于角度变化判断，尤其发卡弯和圆形段）。
4. 通信：LoRa 模块（通常 SPI/UART 接口）。
5. 避障：24 GHz 及以上雷达模块（常见 UART/CAN 输出）。

## 4. 软件总体架构
系统分为五层，自顶向下完成所有巡线任务。

1. 任务规划层：主要负责路径规划
2. 行为决策层：判断当前属于直线、弯道、路口、丢线恢复等哪种状态，相当于状态机。
3. 感知层：把传感器原始值转换为“偏差、弯曲趋势、识别可信度”，负责控制小车在线上移动。
4. 控制层：将所有决策和感知信息转化为电机的方向和转速。
5. 硬件抽象层：统一访问电机、传感器、串口和定时器。

## 5. 线路类型

### 5.1 长直线
策略：提高基准速度，减小转向修正灵敏度，防止高速抖动。

### 5.2 U 型180度弯
策略：提前降速，执行较大角度转向，完成后立即重捕线。

### 5.3 连续 S 弯
策略：分段限速 + 提前修正（前馈思想）+ 限制转向变化率，避免左右摆振。

### 5.4 矩形路线
策略：角点前降速，按预设动作模板过角，角点后立刻确认主线。

### 5.5 圆形路线
策略：使用圆形段专用速度表，并限制控制器累积项大小，避免“越修越偏”。

## 6. 关键代码片段

以下代码片段是“教学示例”，用于理解巡线系统如何组织，不直接等同于可烧录工程。

### 6.1 传感器数组计算线中心

```cpp
#include <cstdint>

// 输入：sensorValue[i] 是第 i 个传感器的黑线强度（越大表示越接近黑线）
// 输出：linePosition 是线中心位置，0 表示正中，负值表示线在左，正值表示线在右
float ComputeLinePosition(const uint16_t sensorValue[8]) {
    // weight[i]：第 i 个传感器在横向上的位置权重，单位可理解为“相对位置”
    static const int16_t weight[8] = {
        -3500, -2500, -1500, -500,   // 左侧 4 个传感器
         500,  1500,  2500, 3500     // 右侧 4 个传感器
    };

    int64_t weightedSum = 0; // 分子：权重 * 强度 的累计和
    int64_t valueSum = 0;    // 分母：强度累计和

    for (int i = 0; i < 8; ++i) {
        weightedSum += static_cast<int64_t>(weight[i]) * sensorValue[i];
        valueSum += sensorValue[i];
    }

    if (valueSum == 0) {
        // 所有传感器都看不到线：返回 0 只是占位，真正处理要交给“丢线状态机”
        return 0.0f;
    }

    // 线中心位置 = 加权平均值
    float linePosition = static_cast<float>(weightedSum) / static_cast<float>(valueSum);
    return linePosition;
}
```

### 6.2 PID 巡线控制

```cpp
struct PidState {
    float kp;            // 比例系数：当前误差越大，修正越大
    float ki;            // 积分系数：长期偏差累积修正
    float kd;            // 微分系数：误差变化过快时进行抑制

    float integral;      // 误差积分累计值
    float prevError;     // 上一周期误差

    float integralMin;   // 积分下限，防止积分过度累积
    float integralMax;   // 积分上限，防止积分过度累积
};

struct MotorCmd {
    float leftSpeed;     // 左轮目标速度
    float rightSpeed;    // 右轮目标速度
};

MotorCmd UpdateLinePid(
    float linePosition,      // 当前线位置（来自传感器计算）
    float targetPosition,    // 目标线位置，通常取 0（车身压在线中心）
    float baseSpeed,         // 基准速度（由当前路段状态决定）
    float dt,                // 控制周期（秒），例如 0.01 表示 10ms
    PidState& pid            // PID 状态（会在函数内更新）
) {
    // error：当前偏差，正值表示线在右，需要向右修正
    float error = targetPosition - linePosition;

    // integral：误差随时间累积，用于消除长期偏差
    pid.integral += error * dt;

    // 积分限幅：防止积分项过大造成过冲（常见于圆形路线）
    if (pid.integral > pid.integralMax) pid.integral = pid.integralMax;
    if (pid.integral < pid.integralMin) pid.integral = pid.integralMin;

    // derivative：误差变化率，反映“偏差变化速度”
    float derivative = (error - pid.prevError) / dt;

    // controlOutput：总修正量，决定左右轮速度差
    float controlOutput = pid.kp * error + pid.ki * pid.integral + pid.kd * derivative;

    pid.prevError = error;

    // 差速控制：左轮减修正、右轮加修正（符号可按车体安装方向调整）
    MotorCmd cmd;
    cmd.leftSpeed = baseSpeed - controlOutput;
    cmd.rightSpeed = baseSpeed + controlOutput;

    return cmd;
}
```

### 6.3 状态机主循环

```cpp
enum class RunState {
    StartAlign,      // 起步对线
    FollowLine,      // 常规巡线
    PassHairpin,     // U 型发卡弯
    PassSCurve,      // 连续 S 弯
    PassRectangle,   // 矩形角点段
    PassCircle,      // 圆形段
    RecoverLine,     // 丢线恢复
    FinishStop,      // 终点停车
    FaultStop        // 故障停车
};

void ControlLoopStep() {
    // state：当前状态，由全局任务管理模块维护
    extern RunState state;

    // lineLost：是否丢线（例如传感器强度总和过低）
    bool lineLost = IsLineLost();

    // 检测路段类型，通常由“传感器特征 + 行驶距离窗口 + 朝向变化”联合判断
    bool isHairpin = DetectHairpin();
    bool isSCurve = DetectSCurve();
    bool isRectangle = DetectRectangleSegment();
    bool isCircle = DetectCircleSegment();

    switch (state) {
        case RunState::StartAlign:
            if (IsStartAligned()) {
                state = RunState::FollowLine;
            }
            break;

        case RunState::FollowLine:
            if (lineLost) {
                state = RunState::RecoverLine;
            } else if (isHairpin) {
                state = RunState::PassHairpin;
            } else if (isSCurve) {
                state = RunState::PassSCurve;
            } else if (isRectangle) {
                state = RunState::PassRectangle;
            } else if (isCircle) {
                state = RunState::PassCircle;
            } else {
                FollowLineWithPid(); // 常规 PID 巡线
            }
            break;

        case RunState::PassHairpin:
            ExecuteHairpinTemplate(); // 发卡弯动作模板（降速 + 大角度转向 + 重捕线）
            if (HairpinDone()) state = RunState::FollowLine;
            break;

        case RunState::PassSCurve:
            ExecuteSCurveTemplate(); // S 弯动作模板（分段限速 + 变化率限制）
            if (SCurveDone()) state = RunState::FollowLine;
            break;

        case RunState::PassRectangle:
            ExecuteRectangleTemplate(); // 角点前减速、过角、角后确认主线
            if (RectangleSegmentDone()) state = RunState::FollowLine;
            break;

        case RunState::PassCircle:
            ExecuteCircleTemplate(); // 圆形段专用速度与积分保护
            if (CircleSegmentDone()) state = RunState::FollowLine;
            break;

        case RunState::RecoverLine:
            ExecuteLineRecovery(); // 按上次偏向方向低速搜索
            if (IsLineRecovered()) state = RunState::FollowLine;
            break;

        case RunState::FinishStop:
            StopMotors();
            break;

        case RunState::FaultStop:
            EmergencyStop();
            break;
    }
}
```

### 6.4 与 LoRa / 雷达模块的底盘控制权接口

```cpp
enum class ControlOwner {
    LineFollow,  // 巡线模块
    Obstacle,    // 避障模块
    Safety       // 安全模块（最高优先级）
};

struct ControlLease {
    ControlOwner owner;      // 当前控制权归属
    uint32_t expireMs;       // 控制权超时时间戳（毫秒）
    uint8_t priority;        // 优先级，数值越大优先级越高
};

// 全局控制权对象（仅示例）
ControlLease g_controlLease;

bool RequestChassisControl(
    ControlOwner requester,  // 请求者
    uint8_t priority,        // 请求优先级
    uint32_t nowMs,          // 当前时间
    uint32_t holdTimeMs      // 希望持有控制权的时长
) {
    bool leaseExpired = (nowMs > g_controlLease.expireMs); // 当前租约是否已过期

    if (leaseExpired || priority >= g_controlLease.priority) {
        g_controlLease.owner = requester;
        g_controlLease.priority = priority;
        g_controlLease.expireMs = nowMs + holdTimeMs;
        return true;
    }

    return false;
}
```

这段接口的意义是：任何模块都不能直接抢写电机输出，必须通过统一入口申请控制权。

## 7. 目录结构

```text
src/
  hal/                  // 硬件访问：ADC、PWM、UART、SPI、定时器
  perception/           // 传感器处理：滤波、线中心、路段识别
  control/              // PID 与动作模板
  behavior/             // 状态机与任务切换
  integration/          // 与 LoRa、雷达模块的接口和仲裁
  app/                  // 主循环与任务编排
```

#ifndef LF_APP_H
#define LF_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_control.h"
#include "lf_radar.h"
#include "lf_sensor.h"

typedef enum {
    LF_APP_STATE_BOOT = 0,
    LF_APP_STATE_WAIT_START,
    LF_APP_STATE_CALIBRATING,
    LF_APP_STATE_RUNNING,
    LF_APP_STATE_RECOVERING,
    LF_APP_STATE_AVOID_PREP,
    LF_APP_STATE_AVOID_TURN_OUT,
    LF_APP_STATE_AVOID_BYPASS,
    LF_APP_STATE_AVOID_TURN_IN,
    LF_APP_STATE_AVOID_REACQUIRE,
    LF_APP_STATE_STOPPED,
    LF_APP_STATE_FAULT
} LF_AppState;

typedef enum {
    LF_APP_REASON_NONE = 0,
    LF_APP_REASON_WAIT_START,
    LF_APP_REASON_CALIBRATION_STARTED,
    LF_APP_REASON_CALIBRATION_FAILED,
    LF_APP_REASON_CALIBRATION_DONE,
    LF_APP_REASON_LINE_LOST,
    LF_APP_REASON_LINE_RECOVERED,
    LF_APP_REASON_RECOVERY_TIMEOUT,
    LF_APP_REASON_RADAR_BLOCK,
    LF_APP_REASON_RADAR_CLEAR,
    LF_APP_REASON_AVOID_STARTED,
    LF_APP_REASON_AVOID_RETRY,
    LF_APP_REASON_AVOID_FAILED,
    LF_APP_REASON_AVOID_COMPLETED,
    LF_APP_REASON_FAULT_FALLBACK,
} LF_AppReason;

typedef struct {
    LF_AppState state;
    uint32_t boot_ms;
    uint32_t last_step_ms;
    uint32_t calib_start_ms;
    uint32_t recover_start_ms;
    uint32_t avoid_state_start_ms;
    int8_t last_seen_dir; /* -1 左侧，+1 右侧。 */
    int8_t avoid_dir;
    uint8_t avoid_attempts;
    LF_PIDState pid;
    LF_SensorFrame last_frame;
    LF_RadarObstacleState obstacle_state;
    uint16_t obstacle_distance_mm;
    uint32_t radar_parse_error_count;
    bool radar_has_target;
    bool radar_frame_valid;
    uint8_t radar_target_state;
    LF_RadarFrameType radar_frame_type;
    uint32_t radar_frame_count;
    uint32_t radar_last_update_ms;
    uint32_t radar_frame_age_ms;
    LF_AppReason reason;
    bool calibration_ok;
} LF_AppContext;

/* 应用层初始化。 */
void LF_App_Init(void);

/* 应用层周期执行（建议在 while(1) 中持续调用）。 */
void LF_App_RunStep(void);

/* 外部事件：通知经过检查点，内部转发到预留 hook。 */
void LF_App_NotifyCheckpoint(uint32_t checkpoint_id);

/* 外部查询当前状态。 */
const LF_AppContext *LF_App_GetContext(void);

#endif /* LF_APP_H */

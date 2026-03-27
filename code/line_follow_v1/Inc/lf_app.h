#ifndef LF_APP_H
#define LF_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_control.h"
#include "lf_sensor.h"

typedef enum {
    LF_APP_STATE_BOOT = 0,
    LF_APP_STATE_WAIT_START,
    LF_APP_STATE_CALIBRATING,
    LF_APP_STATE_RUNNING,
    LF_APP_STATE_RECOVERING,
    LF_APP_STATE_STOPPED,
    LF_APP_STATE_FAULT
} LF_AppState;

typedef struct {
    LF_AppState state;
    uint32_t boot_ms;
    uint32_t last_step_ms;
    uint32_t calib_start_ms;
    uint32_t recover_start_ms;
    int8_t last_seen_dir; /* -1 左侧，+1 右侧。 */
    LF_PIDState pid;
    LF_SensorFrame last_frame;
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

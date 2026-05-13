#ifndef LF_RADAR_H
#define LF_RADAR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LF_RADAR_OBSTACLE_CLEAR = 0,
    LF_RADAR_OBSTACLE_WARN,
    LF_RADAR_OBSTACLE_BLOCK,
} LF_RadarObstacleState;

typedef struct {
    bool has_target;
    bool frame_valid;
    uint16_t distance_mm;
    uint32_t frame_count;
    uint32_t parse_error_count;
    uint32_t last_update_ms;
    LF_RadarObstacleState obstacle_state;
} LF_RadarState;

/* 初始化雷达解析器状态。 */
void LF_Radar_Init(void);

/*
 * 非阻塞轮询解析雷达串口帧。
 * 支持默认帧格式: F4 F3 F2 F1 <dist_lo> <dist_hi> <target> <checksum>
 * checksum = dist_lo ^ dist_hi ^ target
 */
void LF_Radar_Tick(uint32_t now_ms);

/* 获取最近一次解析状态。 */
const LF_RadarState *LF_Radar_GetState(void);

#endif /* LF_RADAR_H */

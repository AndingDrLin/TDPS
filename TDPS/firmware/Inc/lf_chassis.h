/**
 * @file lf_chassis.h
 * @brief Differential-drive chassis motor command API.
 */
#ifndef LF_CHASSIS_H
#define LF_CHASSIS_H

#include <stdint.h>

/** @brief Initialize the chassis module. */
void LF_Chassis_Init(void);

/** @brief Set left/right wheel commands (-1000~1000). */
void LF_Chassis_SetCommand(int16_t left_cmd, int16_t right_cmd);

/**
 * @brief Clamp the left/right wheel differential to the specified range.
 *
 * When the differential exceeds the limit, both commands are shrunk
 * proportionally by the same amount.
 *
 * @param[out] left_cmd  Pointer to the left wheel command.
 * @param[out] right_cmd Pointer to the right wheel command.
 * @param      limit     Maximum allowed differential.
 */
void LF_Chassis_LimitMotorDelta(int16_t *left_cmd, int16_t *right_cmd, int16_t limit);

/** @brief Emergency stop. */
void LF_Chassis_Stop(void);

#endif /* LF_CHASSIS_H */

#include "wireless_hooks.h"
#include "wireless.h"
#include "wireless_app.h"
#include "lf_app.h"

static volatile uint32_t last_status_report_ms = 0;
static const uint32_t STATUS_REPORT_INTERVAL_MS = 1000;

void Wireless_Hooks_Init(void) {
    Wireless_App_Init();
    last_status_report_ms = 0;
}

void LF_Hook_OnCalibrationBegin(void) {
    uint8_t data[32];
    uint16_t len = sprintf((char*)data, "Calibration Started");
    Wireless_App_SendCommand(VEHICLE_CMD_TRACKING_DATA, data, len);
}

void LF_Hook_OnCalibrationComplete(bool success) {
    uint8_t data[32];
    uint16_t len = sprintf((char*)data, "Calibration %s", 
                           success ? "Success" : "Failed");
    Wireless_App_SendCommand(VEHICLE_CMD_TRACKING_DATA, data, len);
}

void LF_Hook_OnLineLost(void) {
    uint8_t data[32];
    uint16_t len = sprintf((char*)data, "Line Lost - Recovering");
    Wireless_App_SendCommand(VEHICLE_CMD_TRACKING_DATA, data, len);
}

void LF_Hook_OnLineRecovered(void) {
    uint8_t data[32];
    uint16_t len = sprintf((char*)data, "Line Recovered");
    Wireless_App_SendCommand(VEHICLE_CMD_TRACKING_DATA, data, len);
}

void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id) {
    uint8_t data[32];
    uint16_t len = sprintf((char*)data, "Checkpoint %lu reached", 
                           (unsigned long)checkpoint_id);
    Wireless_App_SendCommand(VEHICLE_CMD_TRACKING_DATA, data, len);
}

void LF_Hook_OnReservedObstacleWindow(void) {
    uint8_t data[32];
    uint16_t len = sprintf((char*)data, "Obstacle Detected");
    Wireless_App_SendCommand(VEHICLE_CMD_RADAR_DATA, data, len);
}

void Wireless_Hooks_Process(void) {
    uint32_t now_ms = 0;
    LF_Platform_GetMillisFunc get_millis = LF_Platform_GetMillis;
    
    if (get_millis != NULL) {
        now_ms = get_millis();
    }
    
    if (now_ms - last_status_report_ms >= STATUS_REPORT_INTERVAL_MS) {
        const LF_AppContext *ctx = LF_App_GetContext();
        uint8_t status_data[16];
        uint16_t len = 0;
        
        status_data[len++] = (uint8_t)ctx->state;
        status_data[len++] = (uint8_t)ctx->last_seen_dir;
        status_data[len++] = ctx->calibration_ok ? 1 : 0;
        status_data[len++] = (uint8_t)ctx->last_frame.line_detected;
        
        status_data[len++] = (uint8_t)(ctx->last_frame.position & 0xFF);
        status_data[len++] = (uint8_t)((ctx->last_frame.position >> 8) & 0xFF);
        
        status_data[len++] = (uint8_t)(ctx->last_frame.sum & 0xFF);
        status_data[len++] = (uint8_t)((ctx->last_frame.sum >> 8) & 0xFF);
        
        status_data[len++] = (uint8_t)(ctx->pid.integral & 0xFF);
        status_data[len++] = (uint8_t)((ctx->pid.integral >> 8) & 0xFF);
        
        Wireless_App_SendCommand(VEHICLE_CMD_GET_STATUS, status_data, len);
        
        last_status_report_ms = now_ms;
    }
}

void Wireless_Hooks_ProcessCommands(void) {
    Vehicle_MessageDef message;
    
    if (Wireless_App_ReceiveMessage(&message) == WIRELESS_OK) {
        switch (message.cmd) {
            case VEHICLE_CMD_STOP:
                LF_Chassis_Stop();
                break;
                
            case VEHICLE_CMD_FORWARD: {
                int16_t speed = 100;
                if (message.data_len >= 2) {
                    speed = message.data[0] | ((int16_t)message.data[1] << 8);
                    if (speed < 0) speed = -speed;
                }
                LF_Chassis_SetCommand(speed, speed);
                break;
            }
            
            case VEHICLE_CMD_BACKWARD: {
                int16_t speed = -100;
                if (message.data_len >= 2) {
                    speed = -(message.data[0] | ((int16_t)message.data[1] << 8));
                }
                LF_Chassis_SetCommand(speed, speed);
                break;
            }
            
            case VEHICLE_CMD_LEFT:
                LF_Chassis_SetCommand(-50, 50);
                break;
                
            case VEHICLE_CMD_RIGHT:
                LF_Chassis_SetCommand(50, -50);
                break;
                
            case VEHICLE_CMD_SPEED_SET: {
                if (message.data_len >= 2) {
                    int16_t left_speed = (int8_t)message.data[0];
                    int16_t right_speed = (int8_t)message.data[1];
                    LF_Chassis_SetCommand(left_speed, right_speed);
                }
                break;
            }
            
            case VEHICLE_CMD_GET_STATUS: {
                const LF_AppContext *ctx = LF_App_GetContext();
                uint8_t status_data[16];
                uint16_t len = 0;
                
                status_data[len++] = (uint8_t)ctx->state;
                status_data[len++] = (uint8_t)ctx->last_seen_dir;
                status_data[len++] = ctx->calibration_ok ? 1 : 0;
                status_data[len++] = (uint8_t)ctx->last_frame.line_detected;
                
                status_data[len++] = (uint8_t)(ctx->last_frame.position & 0xFF);
                status_data[len++] = (uint8_t)((ctx->last_frame.position >> 8) & 0xFF);
                
                status_data[len++] = (uint8_t)(ctx->last_frame.sum & 0xFF);
                status_data[len++] = (uint8_t)((ctx->last_frame.sum >> 8) & 0xFF);
                
                Wireless_App_SendCommand(VEHICLE_CMD_GET_STATUS, status_data, len);
                break;
            }
            
            default:
                break;
        }
    }
}

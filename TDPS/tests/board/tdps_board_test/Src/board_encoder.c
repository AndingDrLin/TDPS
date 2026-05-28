#include "board_encoder.h"
#include "board_cli.h"

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

static int32_t extend_count(uint16_t now, uint16_t zero)
{
    return (int16_t)(now - zero);
}

static uint16_t s_zero_a;
static uint16_t s_zero_b;

void BoardEncoder_Init(void)
{
    (void)HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    (void)HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    BoardEncoder_Zero();
}

void BoardEncoder_Zero(void)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    s_zero_a = 0;
    s_zero_b = 0;
}

int32_t BoardEncoder_ReadA(void)
{
    return extend_count((uint16_t)__HAL_TIM_GET_COUNTER(&htim3), s_zero_a);
}

int32_t BoardEncoder_ReadB(void)
{
    return extend_count((uint16_t)__HAL_TIM_GET_COUNTER(&htim4), s_zero_b);
}

void BoardEncoder_Print(void)
{
    Board_Printf("ENC A=%ld B=%ld\r\n", (long)BoardEncoder_ReadA(), (long)BoardEncoder_ReadB());
}

void BoardEncoder_Watch(uint32_t duration_ms)
{
    uint32_t start = HAL_GetTick();
    uint32_t last = 0;

    while ((HAL_GetTick() - start) < duration_ms)
    {
        uint32_t now = HAL_GetTick();
        if ((now - last) >= 100U)
        {
            last = now;
            BoardEncoder_Print();
        }
    }
}

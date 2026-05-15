#include "wl_app.h"
#include "wl_config.h"
#include "wl_lora.h"
#include "wl_platform.h"
#include "wl_stub_test.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char *message)
{
    if (!condition) {
        printf("FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int buffer_contains(const uint8_t *buf, uint16_t len, const char *needle)
{
    size_t needle_len = strlen(needle);
    uint16_t i;

    if (buf == NULL || needle == NULL || needle_len == 0U || needle_len > len) {
        return 0;
    }
    for (i = 0U; i <= (uint16_t)(len - needle_len); ++i) {
        if (memcmp(&buf[i], needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void tick_lora_for(uint32_t ms)
{
    uint32_t i;

    for (i = 0U; i < ms; ++i) {
        WL_LoRa_Tick();
        WL_Platform_DelayMs(1U);
    }
}

static int test_lora_queue_full(void)
{
    const WL_LoRa_LinkStatus *link;
    int failures = 0;
    uint16_t i;

    WL_Platform_Init();
    WL_LoRa_ServiceInit();

    for (i = 0U; i < g_wl_config.tx_queue_size; ++i) {
        failures += expect_true(WL_LoRa_EnqueueString("PING") == WL_LORA_OK, "queue accepts item before full");
    }

    failures += expect_true(WL_LoRa_EnqueueString("OVERFLOW") == WL_LORA_ERR_BUSY, "queue rejects overflow item");
    link = WL_LoRa_GetLinkStatus();
    failures += expect_true(link->queue_depth == g_wl_config.tx_queue_size, "queue depth reaches configured capacity");
    failures += expect_true(link->queue_dropped == 1U, "queue drop counter increments");
    return failures;
}

static int test_lora_ack_retry_success(void)
{
    const WL_LoRa_LinkStatus *link;
    const char ack[] = "ACK\n";
    int failures = 0;

    WL_Platform_Init();
    WL_LoRa_ServiceInit();
    WL_LoRa_SetAckEnabled(true);
    WL_Stub_ResetTxCount();

    failures += expect_true(WL_LoRa_EnqueueString("CP=21\n") == WL_LORA_OK, "ack test enqueue succeeds");
    WL_LoRa_Tick();
    link = WL_LoRa_GetLinkStatus();
    failures += expect_true(link->waiting_ack, "lora waits for ack after send");
    failures += expect_true(WL_Stub_GetTxCount() == 1U, "first ack-mode tx sent");

    tick_lora_for(g_wl_config.ack_timeout_ms + 1U);
    WL_LoRa_Tick();
    link = WL_LoRa_GetLinkStatus();
    failures += expect_true(link->retry_count == 1U, "ack timeout schedules one retry");
    failures += expect_true(WL_Stub_GetTxCount() == 2U, "retry tx sent after timeout");

    WL_Stub_AppendRxData((const uint8_t *)ack, (uint16_t)strlen(ack));
    WL_LoRa_Tick();
    link = WL_LoRa_GetLinkStatus();
    failures += expect_true(link->tx_success_count == 1U, "ack completes tx successfully");
    failures += expect_true(!link->waiting_ack, "ack clears waiting state");
    failures += expect_true(link->tx_fail_count == 0U, "ack retry path has no final failure");
    return failures;
}

static int test_app_checkpoint_payload(void)
{
    const WL_App_Diag *diag;
    uint8_t tx[128];
    uint16_t tx_len;
    int failures = 0;

    if (!WL_App_Init()) {
        return 1;
    }

    WL_App_StartRace();
    WL_Platform_DelayMs(5000U);
    WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_1);
    WL_App_Tick();

    diag = WL_App_GetDiag();
    tx_len = WL_Stub_GetLastTx(tx, (uint16_t)sizeof(tx));
    failures += expect_true(diag->checkpoint_enqueued_count == 1U, "checkpoint enqueue count is recorded");
    failures += expect_true(diag->checkpoint_enqueue_fail_count == 0U, "checkpoint enqueue failure count stays zero");
    failures += expect_true(diag->checkpoint_throttled_count == 0U, "checkpoint throttle count stays zero");
    failures += expect_true(tx_len > 0U, "wireless stub captured transmitted payload");
    failures += expect_true(buffer_contains(tx, tx_len, "CP=21"), "checkpoint payload contains CP=21");

    WL_App_StopRace();
    failures += expect_true(WL_App_GetState() == WL_APP_STATE_FINISHED, "wireless app reaches FINISHED");
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_lora_queue_full();
    failures += test_lora_ack_retry_success();
    failures += test_app_checkpoint_payload();

    if (failures != 0) {
        printf("test_wl failed: %d\n", failures);
        return 1;
    }

    printf("test_wl passed\n");
    return 0;
}

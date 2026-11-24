#include "board/oralb_scanner.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

namespace {

constexpr const char *TAG = "oralb";
// Interval == window: listen continuously. The brush advertises several times per second
// while brushing, so even a 50 % duty cycle would do; full duty keeps state changes snappy.
constexpr uint16_t SCAN_INTERVAL_MS = 100;
constexpr uint16_t SCAN_WINDOW_MS = 100;
constexpr int QUEUE_LEN = 16;

QueueHandle_t s_queue = nullptr;

void start_scan();

int gap_event(struct ble_gap_event *event, void *)
{
    if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        start_scan(); // scanning runs forever; restart if the stack ever stops it
        return 0;
    }
    if (event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0 || !fields.mfg_data) {
        return 0;
    }
    OralbAdvertisement item;
    // Manufacturer data starts with the little-endian company id, as brush_oralb_parse() expects.
    if (!brush_oralb_parse(fields.mfg_data, fields.mfg_data_len, &item.adv)) {
        return 0;
    }
    // NimBLE stores the address little-endian; the order only has to be consistent.
    memcpy(item.addr, event->disc.addr.val, 6);
    // Drop when full: a newer advertisement follows within milliseconds anyway.
    xQueueSend(s_queue, &item, 0);
    return 0;
}

void start_scan()
{
    struct ble_gap_disc_params params;
    memset(&params, 0, sizeof(params));
    params.itvl = BLE_GAP_SCAN_ITVL_MS(SCAN_INTERVAL_MS);
    params.window = BLE_GAP_SCAN_WIN_MS(SCAN_WINDOW_MS);
    params.passive = 1;           // never sends anything to the brush
    params.filter_duplicates = 0; // every advertisement matters, the state changes while brushing
    const int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, gap_event, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan start failed: %d", rc);
    }
}

void on_sync() { start_scan(); }

void host_task(void *)
{
    nimble_port_run(); // returns only when nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

} // namespace

void oralb_scanner_start()
{
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(OralbAdvertisement));
    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE init failed");
        return;
    }
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "passive BLE scan starting");
}

bool oralb_scanner_poll(OralbAdvertisement *out)
{
    return s_queue && xQueueReceive(s_queue, out, 0) == pdTRUE;
}

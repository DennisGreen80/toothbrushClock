#include "board/oralb_scanner.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

namespace {

// Interval == window: listen continuously. The brush advertises several times per second
// while brushing, so even a 50 % duty cycle would do; full duty keeps state changes snappy.
constexpr uint16_t SCAN_INTERVAL_MS = 100;
constexpr uint16_t SCAN_WINDOW_MS = 100;
constexpr int QUEUE_LEN = 16;

QueueHandle_t s_queue = nullptr;

class Callbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice *device) override
    {
        if (!device->haveManufacturerData()) {
            return;
        }
        const std::string data = device->getManufacturerData();
        OralbAdvertisement item;
        if (!brush_oralb_parse(reinterpret_cast<const uint8_t *>(data.data()), data.size(), &item.adv)) {
            return;
        }
        // NimBLE stores the address little-endian; the order only has to be consistent.
        memcpy(item.addr, device->getAddress().getNative(), 6);
        // Drop when full: a newer advertisement follows within milliseconds anyway.
        xQueueSend(s_queue, &item, 0);
    }
};

Callbacks s_callbacks;

} // namespace

void oralb_scanner_start()
{
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(OralbAdvertisement));
    NimBLEDevice::init("");
    NimBLEScan *scan = NimBLEDevice::getScan();
    // wantDuplicates: every advertisement matters, the state changes while brushing.
    scan->setAdvertisedDeviceCallbacks(&s_callbacks, true);
    scan->setDuplicateFilter(false);
    scan->setActiveScan(false); // passive: never sends anything to the brush
    scan->setInterval(SCAN_INTERVAL_MS);
    scan->setWindow(SCAN_WINDOW_MS);
    scan->setMaxResults(0); // don't keep a device list, just call back
    scan->start(0, nullptr, false);
    Serial.println("[oralb] passive BLE scan started");
}

bool oralb_scanner_poll(OralbAdvertisement *out)
{
    return s_queue && xQueueReceive(s_queue, out, 0) == pdTRUE;
}

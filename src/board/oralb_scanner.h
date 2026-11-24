// Passive BLE scanner for Oral-B toothbrush advertisements (ESP-IDF NimBLE host).
// Advertisements are parsed on the BLE host task and queued; drain them from the UI loop.
#pragma once

#include <stdint.h>

#include "brush_oralb.h"

struct OralbAdvertisement {
    uint8_t addr[6];
    oralb_adv_t adv;
};

void oralb_scanner_start();
// Returns true and fills `out` while there are queued advertisements.
bool oralb_scanner_poll(OralbAdvertisement *out);

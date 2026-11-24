// PCF85063 real-time clock. Times are stored as local time.
#pragma once

#include <time.h>

bool rtc_init();
// Reads the RTC. Returns false if the chip is missing or its oscillator stopped
// (e.g. after power loss without an RTC battery), meaning the time is not valid.
bool rtc_read(struct tm *out);
bool rtc_write(const struct tm *t);

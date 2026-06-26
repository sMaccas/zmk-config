/*
 * Copyright (c) 2026 Andrey A. Ugolnik
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

// All counters are persisted to flash through the Zephyr settings
// subsystem, so they survive soft resets, System OFF wake-ups, and
// cold boots. They reset only when the settings partition is wiped
// (re-flash, or an explicit settings_delete).

// Total number of boots ever observed by this firmware install.
uint8_t reset_reason_boot_count(void);

// Number of boots that were caused by a fatal fault (LOCKUP, watchdog,
// software-requested reset). A non-zero value means the firmware
// silently recovered from N hangs the user did not have to fix by
// pressing the reset button. Note that SREQ is also set by the
// Adafruit bootloader's DFU exit and any `&sys_reset`-style behaviour,
// so a single +1 right after a flash or a deliberate reset is normal;
// growth between sessions is the actual diagnostic signal.
uint8_t reset_reason_fatal_count(void);

// Raw NRF_POWER->RESETREAS bits captured at this boot, before the
// register was cleared. Useful for one-off debugging.
uint32_t reset_reason_last_raw(void);

/*
 * Copyright (c) 2026 Andrey A. Ugolnik
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

// All counters are reset to zero on a cold boot (power-on). They survive
// soft resets (LOCKUP, watchdog, software request, pin-reset) thanks to
// __noinit RAM, which keeps its contents while VDD is up.

// Total number of boots since the last cold boot.
uint8_t reset_reason_boot_count(void);

// Number of boots since the last cold boot that were caused by a fatal
// fault (LOCKUP via MPU/HW stack protection, watchdog timeout, or a
// software-requested reset). A non-zero value here means the firmware
// silently recovered from N hangs that the user did not have to fix by
// pressing the reset button.
uint8_t reset_reason_fatal_count(void);

// Raw NRF_POWER->RESETREAS bits captured from the most recent boot,
// before the register was cleared. Useful for one-off debugging.
uint32_t reset_reason_last_raw(void);

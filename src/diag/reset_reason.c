/*
 * Copyright (c) 2026 Andrey A. Ugolnik
 * SPDX-License-Identifier: MIT
 *
 * Reads NRF_POWER->RESETREAS very early in boot and tracks how many of
 * those boots were caused by a fatal fault (LOCKUP, watchdog, software
 * reset request). Counters live in __noinit RAM so they survive soft
 * resets but get reinitialised on the first boot after a real power-on.
 *
 * GPREGRET / GPREGRET2 are deliberately NOT used here: the Adafruit
 * nRF52 bootloader treats specific magic values in those registers as
 * "enter DFU" / "enter serial DFU" commands, so reusing them for an
 * unrelated counter risks accidentally rebooting into the bootloader.
 */

#include <diag/reset_reason.h>

#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>

#include <hal/nrf_power.h>

// Magic value used to tell "RAM was preserved across a soft reset" from
// "RAM was just powered up and contains garbage". After a real cold
// boot the magic word is whatever happened to be in SRAM, almost never
// the value below.
#define RESET_REASON_MAGIC 0xA55A5AA5UL

static __noinit uint32_t magic;
static __noinit uint8_t boots;
static __noinit uint8_t fatals;
static __noinit uint32_t last_raw;

// Bits that indicate the previous boot was a fault / programmatic
// reset, not a normal power-on or a user-pressed reset pin.
#define FATAL_BITS (POWER_RESETREAS_DOG_Msk    |  \
                    POWER_RESETREAS_LOCKUP_Msk |  \
                    POWER_RESETREAS_SREQ_Msk)

static int reset_reason_init(void)
{
    const uint32_t reasons = nrf_power_resetreas_get(NRF_POWER);
    nrf_power_resetreas_clear(NRF_POWER, reasons);

    if (magic != RESET_REASON_MAGIC) {
        magic = RESET_REASON_MAGIC;
        boots = 0;
        fatals = 0;
        last_raw = 0;
    }

    last_raw = reasons;
    if (boots < UINT8_MAX) {
        boots++;
    }
    if ((reasons & FATAL_BITS) != 0U && fatals < UINT8_MAX) {
        fatals++;
    }

    return 0;
}

SYS_INIT(reset_reason_init, PRE_KERNEL_1, 0);

uint8_t reset_reason_boot_count(void)  { return boots; }
uint8_t reset_reason_fatal_count(void) { return fatals; }
uint32_t reset_reason_last_raw(void)   { return last_raw; }

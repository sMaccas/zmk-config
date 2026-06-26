/*
 * Copyright (c) 2026 Andrey A. Ugolnik
 * SPDX-License-Identifier: MIT
 *
 * Captures NRF_POWER->RESETREAS very early in boot, then persists per-
 * boot counters to flash via the Zephyr settings subsystem so they
 * survive soft resets, System OFF wake-ups, and cold boots. Counters
 * are only cleared by re-flashing the firmware (which wipes the
 * settings partition) or by an explicit settings_delete.
 *
 * Earlier revisions kept the counters in __noinit RAM, which loses its
 * contents whenever the SoC enters System OFF (CONFIG_ZMK_PM_SOFT_OFF
 * powers down all RAM banks by default), so the counter reset to one
 * after every idle-sleep cycle and made the diagnostic close to
 * useless. Flash persistence avoids that entirely.
 */

#include <diag/reset_reason.h>

#include <errno.h>
#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <hal/nrf_power.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SETTINGS_TREE "diag"

// Bits that indicate the previous boot was a fault or a programmatic
// reset, not a normal power-on or a user-pressed reset pin. SREQ also
// fires for the Adafruit bootloader's DFU exit and any user-invoked
// `&sys_reset`, so a +1 right after a flash or a deliberate reset is
// expected; sustained growth between sessions is the real signal.
#define FATAL_BITS (POWER_RESETREAS_DOG_Msk    |  \
                    POWER_RESETREAS_LOCKUP_Msk |  \
                    POWER_RESETREAS_SREQ_Msk)

static uint32_t captured_reasons;
static uint8_t  boots;
static uint8_t  fatals;

// PRE_KERNEL_1 priority 0 is the earliest SYS_INIT slot Zephyr exposes,
// so we read the RESETREAS register before kernel init, nRF SoC init
// or ZMK has a chance to touch it. The clear is needed because the
// register accumulates bits sticky-style until written.
static int reset_reason_capture(void)
{
    captured_reasons = nrf_power_resetreas_get(NRF_POWER);
    nrf_power_resetreas_clear(NRF_POWER, captured_reasons);
    return 0;
}

SYS_INIT(reset_reason_capture, PRE_KERNEL_1, 0);

static int settings_set_cb(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    if (settings_name_steq(name, "b", NULL) && len == sizeof(boots)) {
        return read_cb(cb_arg, &boots, sizeof(boots)) < 0 ? -EIO : 0;
    }
    if (settings_name_steq(name, "f", NULL) && len == sizeof(fatals)) {
        return read_cb(cb_arg, &fatals, sizeof(fatals)) < 0 ? -EIO : 0;
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(diag_rr, SETTINGS_TREE, NULL, settings_set_cb, NULL, NULL);

// By APPLICATION level the kernel is up and the flash + NVS drivers are
// initialised, so settings_load_subtree() can populate boots/fatals
// from flash. After applying the captured RESETREAS we write the new
// values back. settings_subsys_init() is idempotent - returns -EALREADY
// if ZMK already initialised it earlier in the boot sequence.
static int reset_reason_finalize(void)
{
    const int rc_init = settings_subsys_init();
    const bool storage_ready = (rc_init == 0 || rc_init == -EALREADY);

    LOG_INF("diag/rr: subsys_init=%d storage_ready=%d resetreas=0x%08x",
            rc_init, (int)storage_ready, captured_reasons);

    int rc_load = 0;
    if (storage_ready) {
        rc_load = settings_load_subtree(SETTINGS_TREE);
        LOG_INF("diag/rr: load_subtree=%d -> boots=%u fatals=%u",
                rc_load, boots, fatals);
    }

    if (boots < UINT8_MAX) {
        boots++;
    }
    if ((captured_reasons & FATAL_BITS) != 0U && fatals < UINT8_MAX) {
        fatals++;
    }

    LOG_INF("diag/rr: after increment boots=%u fatals=%u", boots, fatals);

    if (storage_ready) {
        const int rc_b = settings_save_one(SETTINGS_TREE "/b",
                                           &boots, sizeof(boots));
        const int rc_f = settings_save_one(SETTINGS_TREE "/f",
                                           &fatals, sizeof(fatals));
        LOG_INF("diag/rr: save b=%d f=%d", rc_b, rc_f);
    }

    return 0;
}

SYS_INIT(reset_reason_finalize, APPLICATION, 90);

uint8_t reset_reason_boot_count(void)  { return boots; }
uint8_t reset_reason_fatal_count(void) { return fatals; }
uint32_t reset_reason_last_raw(void)   { return captured_reasons; }

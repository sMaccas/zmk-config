/*
 * Standalone momentary-layer-lock behavior.
 *
 * On press, locks the topmost currently-active non-base layer using upstream
 * ZMK's built-in layer-lock mechanism (zmk_keymap_layer_activate(layer, true)).
 * A subsequent press, when that layer is still active, deactivates and unlocks
 * it. If no non-base layer is active at press time, the configured `bindings`
 * phandle is invoked as a fallback (the standard usage is `<&to 0>` to return
 * to the base layer).
 */

#define DT_DRV_COMPAT zmk_behavior_momentary_layer_lock

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_molock_config {
    struct zmk_behavior_binding fallback_binding;
};

struct behavior_molock_data {
    bool fallback_pressed;
};

static int molock_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

static int molock_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_molock_config *config = dev->config;
    struct behavior_molock_data *data = dev->data;

    zmk_keymap_layer_index_t highest_idx = zmk_keymap_highest_layer_active();
    zmk_keymap_layer_id_t default_id = zmk_keymap_layer_default();
    zmk_keymap_layer_id_t highest_id = zmk_keymap_layer_index_to_id(highest_idx);

    LOG_DBG("molock pressed at position %d, highest active layer index %d (id %d), default id %d",
            event.position, highest_idx, highest_id, default_id);

    if (highest_id == default_id) {
        struct zmk_behavior_binding fallback = config->fallback_binding;
        data->fallback_pressed = true;
        LOG_DBG("no non-base layer active, invoking fallback binding");
        return behavior_keymap_binding_pressed(&fallback, event);
    }

    if (zmk_keymap_layer_locked(highest_id)) {
        LOG_DBG("layer %d already locked, deactivating and unlocking", highest_id);
        return zmk_keymap_layer_deactivate(highest_id, true);
    }

    LOG_DBG("locking layer %d", highest_id);
    return zmk_keymap_layer_activate(highest_id, true);
}

static int molock_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_molock_config *config = dev->config;
    struct behavior_molock_data *data = dev->data;

    if (data->fallback_pressed) {
        struct zmk_behavior_binding fallback = config->fallback_binding;
        data->fallback_pressed = false;
        return behavior_keymap_binding_released(&fallback, event);
    }

    return 0;
}

static const struct behavior_driver_api behavior_molock_driver_api = {
    .binding_pressed = molock_pressed,
    .binding_released = molock_released,
};

#define KP_INST(n)                                                                                 \
    static struct behavior_molock_config behavior_molock_config_##n = {                            \
        .fallback_binding =                                                                        \
            {                                                                                      \
                .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0)),            \
                .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, 0, param1), (0),    \
                                      (DT_INST_PHA_BY_IDX(n, bindings, 0, param1))),               \
                .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, 0, param2), (0),    \
                                      (DT_INST_PHA_BY_IDX(n, bindings, 0, param2))),               \
            },                                                                                     \
    };                                                                                             \
    static struct behavior_molock_data behavior_molock_data_##n = {.fallback_pressed = false};     \
    BEHAVIOR_DT_INST_DEFINE(n, molock_init, NULL, &behavior_molock_data_##n,                       \
                            &behavior_molock_config_##n, POST_KERNEL,                              \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_molock_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

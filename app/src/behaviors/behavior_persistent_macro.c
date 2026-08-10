/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_persistent_macro

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/settings/settings.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keys.h>

#include <dt-bindings/zmk/persistent_macro.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// リビルドなしで定型文をランタイム記録
// 記録開始/終了、再生キーでbehavior_queue経由のkp相当のタップ列として送出
#define PM_SLOT_COUNT 3
#define PM_MAX_KEYS 32

#define PM_KP_DEV DEVICE_DT_NAME(DT_NODELABEL(kp))

struct pm_slot {
    uint8_t length;
    uint32_t keys[PM_MAX_KEYS];
};

enum pm_state {
    PM_STATE_IDLE,
    PM_STATE_RECORDING,
};

static struct pm_slot pm_slots[PM_SLOT_COUNT];
static enum pm_state pm_state = PM_STATE_IDLE;
static uint8_t pm_recording_slot;
static uint32_t pm_record_buf[PM_MAX_KEYS];
static uint8_t pm_record_len;

static void pm_save_slot(uint8_t idx) {
    char key[16];
    snprintf(key, sizeof(key), "pmacro/slot/%d", idx);
    int err = settings_save_one(key, &pm_slots[idx], sizeof(pm_slots[idx]));
    if (err) {
        LOG_ERR("Failed to save persistent macro slot %d (err %d)", idx, err);
    } else {
        LOG_DBG("Saved persistent macro slot %d (%d keys)", idx, pm_slots[idx].length);
    }
}

static void pm_play_slot(uint8_t idx, struct zmk_behavior_binding_event *event) {
    for (int i = 0; i < pm_slots[idx].length; i++) {
        struct zmk_behavior_binding binding = {
            .behavior_dev = PM_KP_DEV,
            .param1 = pm_slots[idx].keys[i],
            .param2 = 0,
        };
        zmk_behavior_queue_add(event, binding, true, 1);
        zmk_behavior_queue_add(event, binding, false, 1);
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    uint8_t slot = (uint8_t)binding->param2;

    if (slot >= PM_SLOT_COUNT) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    switch (binding->param1) {
    case PM_TOGGLE:
        if (pm_state == PM_STATE_IDLE) {
            pm_state = PM_STATE_RECORDING;
            pm_recording_slot = slot;
            pm_record_len = 0;
        } else if (pm_state == PM_STATE_RECORDING && pm_recording_slot == slot) {
            pm_slots[slot].length = pm_record_len;
            memcpy(pm_slots[slot].keys, pm_record_buf, pm_record_len * sizeof(uint32_t));
            pm_save_slot(slot);
            pm_state = PM_STATE_IDLE;
        }
        // 別スロットの記録中に押された場合は誤操作防止のため無視する
        break;
    case PM_CANCEL:
        if (pm_state == PM_STATE_RECORDING) {
            pm_state = PM_STATE_IDLE;
            pm_record_len = 0;
        }
        break;
    case PM_PLAY:
        if (pm_state == PM_STATE_IDLE) {
            pm_play_slot(slot, &event);
        }
        break;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

// 記録中は、確定して送出されるキーコードをそのまま横取りしてバッファへ積む。
// レイヤーやコンボ経由でも最終的なキーコードを拾えるため、記録時と再生時で結果がぶれない。
// Shift等の修飾キー自体は別イベントとして飛んでくるため記録せず、通常キー側に
// zmk_hid_get_explicit_mods()で取得した「その瞬間の実際の修飾状態」を焼き込む。
static int pm_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev == NULL || !ev->state || pm_state != PM_STATE_RECORDING) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (is_mod(ev->usage_page, ev->keycode)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (pm_record_len < PM_MAX_KEYS) {
        uint32_t mods =
            ev->implicit_modifiers | ev->explicit_modifiers | zmk_hid_get_explicit_mods();
        pm_record_buf[pm_record_len++] = ZMK_HID_USAGE(ev->usage_page, ev->keycode) | (mods << 24);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_persistent_macro, pm_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_persistent_macro, zmk_keycode_state_changed);

#if IS_ENABLED(CONFIG_SETTINGS)

static int pm_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;

    if (settings_name_steq(name, "slot", &next) && next) {
        char *endptr;
        uint8_t idx = strtoul(next, &endptr, 10);
        if (*endptr != '\0' || idx >= PM_SLOT_COUNT || len != sizeof(struct pm_slot)) {
            LOG_WRN("Invalid persistent macro slot setting: %s", name);
            return -EINVAL;
        }

        int err = read_cb(cb_arg, &pm_slots[idx], sizeof(pm_slots[idx]));
        if (err <= 0) {
            LOG_ERR("Failed to load persistent macro slot %d (err %d)", idx, err);
            return err;
        }
        LOG_DBG("Loaded persistent macro slot %d (%d keys)", idx, pm_slots[idx].length);
    }

    return 0;
}

// 動的ハンドラ(settings_register)はmain()内のsettings_subsys_init()がリスト自体を
// リセットしてしまい、POST_KERNEL時点で登録した内容が失われるため、静的ハンドラで登録する。
SETTINGS_STATIC_HANDLER_DEFINE(pmacro, "pmacro", NULL, pm_settings_set, NULL, NULL);

#endif /* IS_ENABLED(CONFIG_SETTINGS) */

static int behavior_persistent_macro_init(const struct device *dev) { return 0; }

static const struct behavior_driver_api behavior_persistent_macro_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define PM_INST(n)                                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_persistent_macro_init, NULL, NULL, NULL, POST_KERNEL,    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_persistent_macro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PM_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

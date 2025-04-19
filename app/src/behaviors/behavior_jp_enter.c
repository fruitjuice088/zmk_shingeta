/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_jp_enter

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include "dt-bindings/zmk/keys.h"
#include "zephyr/devicetree.h"

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h> // zmk layer activate
#include <zmk/behavior.h>


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_jp_enter_config {
    zmk_keymap_layer_id_t je_entry_layer;
};


// HENK_ENT: Enterキーを押している間、かなとINT4が有効になる
static int64_t ent_pressed_timestamp;
static bool kana_key_pressed = false;
static bool enter_key_held = false;

static void tap_code32(uint32_t kc, int64_t timestamp) {
  raise_zmk_keycode_state_changed_from_encoded(kc, true, timestamp);
  raise_zmk_keycode_state_changed_from_encoded(kc, false, timestamp);
}

static int behavior_jp_enter_init(const struct device *dev) {
  LOG_DBG("JP Enter Init.");
  return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_jp_enter_config *cfg = dev->config;

  uint32_t keycode = binding->param1;

  switch (keycode) {
    case ENTER: {
      zmk_keymap_layer_activate(cfg->je_entry_layer);
      ent_pressed_timestamp = event.timestamp;
      kana_key_pressed = false;
      enter_key_held = true;
      break;
    }

    default:
    {
      if (!enter_key_held) {
        return ZMK_BEHAVIOR_OPAQUE;
      }

      // HENK_ENT後初めてかなキーを押した
      if (!kana_key_pressed) {
        tap_code32(INT4, event.timestamp);  // Henkan
        kana_key_pressed = true;
      }
      return raise_zmk_keycode_state_changed_from_encoded(keycode, true, event.timestamp);
    }
  }
  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_jp_enter_config *cfg = dev->config;

  uint32_t keycode = binding->param1;

  switch (keycode) {
    case ENTER: {
      zmk_keymap_layer_deactivate(cfg->je_entry_layer);
      if ((event.timestamp - ent_pressed_timestamp) < 200 && !kana_key_pressed) {
        tap_code32(ENTER, event.timestamp);
      }
      if (kana_key_pressed) {
        tap_code32(INT5, event.timestamp); // Muhenkan
      }
      enter_key_held = false;
      kana_key_pressed = false;
      break;
    }

    default:
    {
      if (!enter_key_held) {
        return ZMK_BEHAVIOR_OPAQUE;
      }

      return raise_zmk_keycode_state_changed_from_encoded(keycode, false, event.timestamp);
    }
  }
  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_jp_enter_driver_api = {
  .binding_pressed = on_keymap_binding_pressed,
  .binding_released = on_keymap_binding_released
};

#define KP_INST(n) \
  static const struct behavior_jp_enter_config behavior_jp_enter_config_##n = { \
      .je_entry_layer = DT_INST_PROP(n, je_entry_layer), \
  }; \
  BEHAVIOR_DT_INST_DEFINE(n, behavior_jp_enter_init, NULL, NULL, \
                          &behavior_jp_enter_config_##n, POST_KERNEL, \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_jp_enter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

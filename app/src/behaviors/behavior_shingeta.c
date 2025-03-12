/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_shingeta

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include "dt-bindings/zmk/keys.h"
#include "zephyr/devicetree.h"

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h> // zmk layer activate
#include <zmk/behavior.h>

#include <dt-bindings/zmk/shingeta.h>

// clang-format on
// Look-up table to convert an ASCII character to a keycode.
__attribute__((weak)) const uint32_t ascii_to_keycode_lut[128] = {
    ['a'] = A,        ['b'] = B,        ['c'] = C,       ['d'] = D,       ['e'] = E,
    ['f'] = F,        ['g'] = G,        ['h'] = H,       ['i'] = I,       ['j'] = J,
    ['k'] = K,        ['l'] = L,        ['m'] = M,       ['n'] = N,       ['o'] = O,
    ['p'] = P,        ['q'] = Q,        ['r'] = R,       ['s'] = S,       ['t'] = T,
    ['u'] = U,        ['v'] = V,        ['w'] = W,       ['x'] = X,       ['y'] = Y,
    ['z'] = Z,        ['1'] = N1,       ['2'] = N2,      ['3'] = N3,      ['4'] = N4,
    ['5'] = N5,       ['6'] = N6,       ['7'] = N7,      ['8'] = N8,      ['9'] = N9,
    ['0'] = N0,       ['['] = JP_LBKT,  [']'] = JP_RBKT, ['{'] = JP_LBRC, ['}'] = JP_RBRC,
    ['('] = JP_LPAR,  [')'] = JP_RPAR,

    ['-'] = JP_MINUS, [','] = JP_COMMA, ['.'] = JP_DOT,  ['/'] = JP_FSLH, ['\\'] = JP_BSLH,
    ['!'] = JP_EXCL,  ['?'] = JP_QMARK, ['L'] = LEFT,
};
// clang-format on

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_shingeta_config {
    zmk_keymap_layer_id_t sg_entry_layer;
};

// refer to
// https://github.com/funatsufumiya/qmk_firmware_k3pro/blob/main/keyboards/keychron/k3_pro/jis/white/keymaps/naginata_v15_and_shingeta/keymap.c
#define B_Q ((uint64_t)1 << 0)
#define B_W ((uint64_t)1 << 1)
#define B_E ((uint64_t)1 << 2)
#define B_R ((uint64_t)1 << 3)
#define B_T ((uint64_t)1 << 4)

#define B_Y ((uint64_t)1 << 5)
#define B_U ((uint64_t)1 << 6)
#define B_I ((uint64_t)1 << 7)
#define B_O ((uint64_t)1 << 8)
#define B_P ((uint64_t)1 << 9)

#define B_A ((uint64_t)1 << 10)
#define B_S ((uint64_t)1 << 11)
#define B_D ((uint64_t)1 << 12)
#define B_F ((uint64_t)1 << 13)
#define B_G ((uint64_t)1 << 14)

#define B_H ((uint64_t)1 << 15)
#define B_J ((uint64_t)1 << 16)
#define B_K ((uint64_t)1 << 17)
#define B_L ((uint64_t)1 << 18)
#define B_SEMI ((uint64_t)1 << 19)

#define B_Z ((uint64_t)1 << 20)
#define B_X ((uint64_t)1 << 21)
#define B_C ((uint64_t)1 << 22)
#define B_V ((uint64_t)1 << 23)
#define B_B ((uint64_t)1 << 24)

#define B_N ((uint64_t)1 << 25)
#define B_M ((uint64_t)1 << 26)
#define B_COMMA ((uint64_t)1 << 27)
#define B_DOT ((uint64_t)1 << 28)
#define B_SLASH ((uint64_t)1 << 29)

#define B_1 ((uint64_t)1 << 30)
#define B_2 ((uint64_t)1 << 31)
#define B_3 ((uint64_t)1 << 32)
#define B_4 ((uint64_t)1 << 33)
#define B_5 ((uint64_t)1 << 34)
#define B_6 ((uint64_t)1 << 35)
#define B_7 ((uint64_t)1 << 36)
#define B_8 ((uint64_t)1 << 37)
#define B_9 ((uint64_t)1 << 38)
#define B_0 ((uint64_t)1 << 39)
#define B_AT ((uint64_t)1 << 40)

#define SGBUFFER 5

// HENK_ENT: Enterキーを押している間、かなとINT4が有効になる
static int64_t ent_pressed_timestamp;
static bool kana_key_pressed = false;
static bool enter_key_held = false;

static uint8_t sg_chrcount = 0;
static uint64_t keycomb = (uint64_t)0;

static uint32_t ninputs[SGBUFFER];

// clang-format off
static const uint64_t ng_key[] = {
    [A - A] = B_A, [B - A] = B_B, [C - A] = B_C, [D - A] = B_D, [E - A] = B_E,
    [F - A] = B_F, [G - A] = B_G, [H - A] = B_H, [I - A] = B_I, [J - A] = B_J,
    [K - A] = B_K, [L - A] = B_L, [M - A] = B_M, [N - A] = B_N, [O - A] = B_O,
    [P - A] = B_P, [Q - A] = B_Q, [R - A] = B_R, [S - A] = B_S, [T - A] = B_T,
    [U - A] = B_U, [V - A] = B_V, [W - A] = B_W, [X - A] = B_X, [Y - A] = B_Y,
    [Z - A] = B_Z, [SEMI - A] = B_SEMI, [COMMA - A] = B_COMMA, [DOT - A] = B_DOT,
    [SLASH - A] = B_SLASH, [JP_AT - A] = B_AT,
    [N1 - A] = B_1, [N2 - A] = B_2, [N3 - A] = B_3, [N4 - A] = B_4, [N5 - A] = B_5,
    [N6 - A] = B_6, [N7 - A] = B_7, [N8 - A] = B_8, [N9 - A] = B_9, [N0 - A] = B_0
};
// clang-format on

typedef struct {
    uint64_t key;
    char kana[5];
} shingeta_keymap;

// clang-format off
static shingeta_keymap sgmap[] = {
    {.key = B_1, .kana = "1"},
    {.key = B_2, .kana = "2"},
    {.key = B_3, .kana = "3"},
    {.key = B_4, .kana = "4"},
    {.key = B_5, .kana = "5"},
    {.key = B_6, .kana = "6"},
    {.key = B_7, .kana = "7"},
    {.key = B_8, .kana = "8"},
    {.key = B_9, .kana = "9"},
    {.key = B_0, .kana = "0"},

    {.key = B_Q, .kana = "-"},
    {.key = B_W, .kana = "ni"},
    {.key = B_E, .kana = "ha"},
    {.key = B_R, .kana = ","},
    {.key = B_T, .kana = "ti"},
    {.key = B_Y, .kana = "gu"},
    {.key = B_U, .kana = "ba"},
    {.key = B_I, .kana = "ko"},
    {.key = B_O, .kana = "ga"},
    {.key = B_P, .kana = "hi"},
    {.key = B_AT, .kana = "ge"},

    {.key = B_A, .kana = "no"},
    {.key = B_S, .kana = "to"},
    {.key = B_D, .kana = "ka"},
    {.key = B_F, .kana = "nn"},
    {.key = B_G, .kana = "xtu"},
    {.key = B_H, .kana = "ku"},
    {.key = B_J, .kana = "u"},
    {.key = B_K, .kana = "i"},
    {.key = B_L, .kana = "si"},
    {.key = B_SEMI, .kana = "na"},

    {.key = B_Z, .kana = "su"},
    {.key = B_X, .kana = "ma"},
    {.key = B_C, .kana = "ki"},
    {.key = B_V, .kana = "ru"},
    {.key = B_B, .kana = "tu"},
    {.key = B_N, .kana = "te"},
    {.key = B_M, .kana = "ta"},
    {.key = B_COMMA, .kana = "de"},
    {.key = B_DOT, .kana = "."},
    {.key = B_SLASH, .kana = "bu"},

    // 中指シフト
    {.key = B_K | B_Q, .kana = "fa"},
    {.key = B_K | B_W, .kana = "go"},
    {.key = B_K | B_E, .kana = "hu"},
    {.key = B_K | B_R, .kana = "fi"},
    {.key = B_K | B_T, .kana = "fe"},
    {.key = B_D | B_Y, .kana = "wi"},
    {.key = B_D | B_U, .kana = "pa"},
    {.key = B_D | B_I, .kana = "yo"},
    {.key = B_D | B_O, .kana = "mi"},
    {.key = B_D | B_P, .kana = "we"},
    {.key = B_5 | B_AT, .kana = "who"},

    {.key = B_K | B_A, .kana = "ho"},
    {.key = B_K | B_S, .kana = "ji"},
    {.key = B_K | B_D, .kana = "re"},
    {.key = B_K | B_F, .kana = "mo"},
    {.key = B_K | B_G, .kana = "yu"},
    {.key = B_D | B_H, .kana = "he"},
    {.key = B_D | B_J, .kana = "a"},
    {.key = B_D | B_K, .kana = ""},
    {.key = B_D | B_L, .kana = "o"},
    {.key = B_D | B_SEMI, .kana = "e"},

    {.key = B_K | B_Z, .kana = "du"},
    {.key = B_K | B_X, .kana = "zo"},
    {.key = B_K | B_C, .kana = "bo"},
    {.key = B_K | B_V, .kana = "mu"},
    {.key = B_K | B_B, .kana = "fo"},
    {.key = B_D | B_N, .kana = "se"},
    {.key = B_D | B_M, .kana = "ne"},
    {.key = B_D | B_COMMA, .kana = "be"},
    {.key = B_D | B_DOT, .kana = "pu"},
    {.key = B_D | B_SLASH, .kana = "vu"},

    {.key = B_K | B_1, .kana = "xa"},
    {.key = B_K | B_2, .kana = "xi"},
    {.key = B_K | B_3, .kana = "xu"},
    {.key = B_K | B_4, .kana = "xe"},
    {.key = B_K | B_5, .kana = "xo"},

    // 薬指シフト
    {.key = B_L | B_Q, .kana = "di"},
    {.key = B_L | B_W, .kana = "me"},
    {.key = B_L | B_E, .kana = "ke"},
    {.key = B_L | B_R, .kana = "twi"},
    {.key = B_L | B_T, .kana = "dhi"},
    {.key = B_S | B_Y, .kana = "sye"},
    {.key = B_S | B_U, .kana = "pe"},
    {.key = B_S | B_I, .kana = "do"},
    {.key = B_S | B_O, .kana = "ya"},
    {.key = B_S | B_P, .kana = "je"},

    {.key = B_L | B_A, .kana = "wo"},
    {.key = B_L | B_S, .kana = "sa"},
    {.key = B_L | B_D, .kana = "o"},
    {.key = B_L | B_F, .kana = "ri"},
    {.key = B_L | B_G, .kana = "zu"},
    {.key = B_S | B_H, .kana = "bi"},
    {.key = B_S | B_J, .kana = "ra"},
    {.key = B_S | B_K, .kana = ""},
    {.key = B_S | B_L, .kana = ""},
    {.key = B_S | B_SEMI, .kana = "so"},

    {.key = B_L | B_Z, .kana = "ze"},
    {.key = B_L | B_X, .kana = "za"},
    {.key = B_L | B_C, .kana = "gi"},
    {.key = B_L | B_V, .kana = "ro"},
    {.key = B_L | B_B, .kana = "nu"},
    {.key = B_S | B_N, .kana = "wa"},
    {.key = B_S | B_M, .kana = "da"},
    {.key = B_S | B_COMMA, .kana = "pi"},
    {.key = B_S | B_DOT, .kana = "po"},
    {.key = B_S | B_SLASH, .kana = "che"},

    {.key = B_L | B_1, .kana = "xya"},
    {.key = B_L | B_2, .kana = "mya"},
    {.key = B_L | B_3, .kana = "myu"},
    {.key = B_L | B_4, .kana = "myo"},
    {.key = B_L | B_5, .kana = "xwa"},

    {.key = B_I | B_E, .kana = "sho"},
    {.key = B_I | B_W, .kana = "shu"},
    {.key = B_I | B_R, .kana = "kyu"},
    {.key = B_I | B_F, .kana = "kyo"},
    {.key = B_I | B_V, .kana = "kya"},
    {.key = B_I | B_C, .kana = "sha"},
    {.key = B_I | B_Q, .kana = "hyu"},
    {.key = B_I | B_A, .kana = "hyo"},
    {.key = B_I | B_Z, .kana = "hya"},
    {.key = B_I | B_T, .kana = "chu"},
    {.key = B_I | B_G, .kana = "cho"},
    {.key = B_I | B_B, .kana = "cha"},

    {.key = B_I | B_1, .kana = "xyu"},
    {.key = B_I | B_2, .kana = "bya"},
    {.key = B_I | B_3, .kana = "byu"},
    {.key = B_I | B_4, .kana = "byo"},

    {.key = B_O | B_E, .kana = "jo"},
    {.key = B_O | B_W, .kana = "ju"},
    {.key = B_O | B_R, .kana = "gyu"},
    {.key = B_O | B_F, .kana = "gyo"},
    {.key = B_O | B_V, .kana = "gya"},
    {.key = B_O | B_C, .kana = "ja"},
    {.key = B_O | B_Q, .kana = "ryu"},
    {.key = B_O | B_A, .kana = "ryo"},
    {.key = B_O | B_Z, .kana = "rya"},
    {.key = B_O | B_T, .kana = "nyu"},
    {.key = B_O | B_G, .kana = "nyo"},
    {.key = B_O | B_B, .kana = "nya"},

    {.key = B_O | B_1, .kana = "xyo"},
    {.key = B_O | B_2, .kana = "pya"},
    {.key = B_O | B_3, .kana = "pyu"},
    {.key = B_O | B_4, .kana = "pyo"},

    // others
    {.key = B_E | B_F, .kana = "/"},
    {.key = B_X | B_F, .kana = "\\"},
    {.key = B_F | B_B, .kana = "!"},
    {.key = B_N | B_J, .kana = "?"},

    {.key = B_T | B_Y, .kana = "[]L"},
    {.key = B_G | B_H, .kana = "()L"},
};
// clang-format off

void tap_code32(uint32_t kc, int64_t timestamp) {
  raise_zmk_keycode_state_changed_from_encoded(kc, true, timestamp);
  raise_zmk_keycode_state_changed_from_encoded(kc, false, timestamp);
}

void send_string(const char * s, int64_t timestamp) {
  for (int k = 0; k < 5; k++) {
    if (s == NULL) break;
    uint32_t kc = *(&ascii_to_keycode_lut[(uint8_t)(*s)]);
    tap_code32(kc, timestamp);
    s++;
  }
}

void shingeta_clear(void) {
    for (int i = 0; i < SGBUFFER; i++) {
        ninputs[i] = 0;
    }
    sg_chrcount = 0;
}


void shingeta_type(int64_t timestamp) {
  LOG_DBG("SHINGETA TYPE");

  if (sg_chrcount == 0) {
    return;
  }

  shingeta_keymap bsgmap;

  for (int i = 0; i < sizeof sgmap / sizeof bsgmap; i++) {
      // 一時バッファに定義配列を入れる
      memcpy(&bsgmap, &sgmap[i], sizeof(bsgmap));
      // 現在の入力が、今入れた定義と一致、つまり定義が見つかった
      if (keycomb == bsgmap.key) {
          send_string(bsgmap.kana, timestamp);
          shingeta_clear();
          return;
      }
  }

  // その他の入力
  uint32_t skey = 0;
  for (int j = 0; j < sg_chrcount; j++) {
      skey = ng_key[ninputs[j] - A];
      for (int i = 0; i < sizeof sgmap / sizeof bsgmap; i++) {
          memcpy(&bsgmap, &sgmap[i], sizeof(bsgmap));
          if (skey == bsgmap.key) {
            send_string(bsgmap.kana, timestamp);
            break;
          }
      }
  }

  shingeta_clear();
}


static int behavior_shingeta_init(const struct device *dev) {
  LOG_DBG("Shingeta Init.");
  return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_shingeta_config *cfg = dev->config;

  uint32_t keycode = binding->param1;

  switch (keycode) {
    case ENTER: {
      zmk_keymap_layer_activate(cfg->sg_entry_layer);
      keycomb = (uint64_t)0;
      ent_pressed_timestamp = event.timestamp;
      kana_key_pressed = false;
      enter_key_held = true;
      break;
    }

    case A ... N0:
    case LBKT ... FSLH:  // 458799 ... 458808 (LBKT ... FSLH (incl. BSLH, GRAV))
    {
      if (!enter_key_held) {
        return ZMK_BEHAVIOR_OPAQUE;
      }

      // HENK_ENT後初めてかなキーを押した
      if (!kana_key_pressed) {
        tap_code32(INT4, event.timestamp);  // Henkan
        shingeta_clear();
        kana_key_pressed = true;
      }
      ninputs[sg_chrcount++] = keycode;  // save to input buffer
      keycomb |= ng_key[keycode - A];

      // 2文字あれば処理
      if (sg_chrcount >= 2) {
        shingeta_type(event.timestamp);
      }
    }
  }
  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_shingeta_config *cfg = dev->config;

  uint32_t keycode = binding->param1;

  switch (keycode) {
    case ENTER: {
      keycomb = (uint64_t)0;
      shingeta_clear();
      zmk_keymap_layer_deactivate(cfg->sg_entry_layer);
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

    case A ... N0:
    case LBKT ... FSLH:  // 458799 ... 458808 (LBKT ... FSLH (incl. BSLH, GRAV))
    {
      if (!enter_key_held) {
        return ZMK_BEHAVIOR_OPAQUE;
      }

      // 何かしら入力が残っていれば処理
      if (sg_chrcount > 0) {
        shingeta_type(event.timestamp);
      }
      keycomb &= ~ng_key[keycode - A];
    }
  }
  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_shingeta_driver_api = {
  .binding_pressed = on_keymap_binding_pressed,
  .binding_released = on_keymap_binding_released
};

#define KP_INST(n) \
  static const struct behavior_shingeta_config behavior_shingeta_config_##n = { \
      .sg_entry_layer = DT_INST_PROP(n, sg_entry_layer), \
  }; \
  BEHAVIOR_DT_INST_DEFINE(n, behavior_shingeta_init, NULL, NULL, \
                          &behavior_shingeta_config_##n, POST_KERNEL, \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_shingeta_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

/*
 * Copyright (c) 2023 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <zephyr/dt-bindings/dt-util.h>
#include "dt-bindings/zmk/keys.h"

#define JP_ZKHK  GRAVE         // Zenkaku ↔ Hankaku ↔ Kanji
#define JP_MINUS MINUS         // -
#define JP_CARET EQUAL         // ^
#define JP_YEN   INT3          // ¥
#define JP_AT    LBKT          // @
#define JP_LBKT  RBKT          // [
#define JP_EISU  CAPS          // Eisū
#define JP_SEMI  SEMI          // ;
#define JP_COLN  SQT           // :
#define JP_RBKT  NUHS          // ]
#define JP_COMMA COMMA         // ,
#define JP_DOT   DOT           // .
#define JP_FSLH  SLASH         // /
#define JP_BSLH  INT1          // (backslash)
#define JP_MHEN  INT5          // Muhenkan
#define JP_HENK  INT4          // Henkan
#define JP_KANA  INT2          // Katakana ↔ Hiragana ↔ Rōmaji
#define JP_EXCL  LS(N1)        // !
#define JP_DQT   LS(N2)        // "
#define JP_HASH  LS(N3)        // #
#define JP_DLLR  LS(N4)        // $
#define JP_PRCNT LS(N5)        // %
#define JP_AMPS  LS(N6)        // &
#define JP_SQT   LS(N7)        // '
#define JP_LPAR  LS(N8)        // (
#define JP_RPAR  LS(N9)        // )
#define JP_EQUAL LS(JP_MINUS)  // =
#define JP_TILDE LS(JP_CARET)  // ~
#define JP_PIPE  LS(JP_YEN)    // |
#define JP_GRAVE LS(JP_AT)     // `
#define JP_LBRC  LS(JP_LBKT)   // {
#define JP_CAPS  LS(JP_EISU)   // Caps Lock
#define JP_PLUS  LS(JP_SEMI)   // +
#define JP_ASTRK LS(JP_COLN)   // *
#define JP_RBRC  LS(JP_RBKT)   // }
#define JP_LT    LS(JP_COMMA)  // <
#define JP_RT    LS(JP_DOT)    // >
#define JP_QMARK LS(JP_FSLH)   // ?
#define JP_UNDER LS(JP_BSLH)   // _

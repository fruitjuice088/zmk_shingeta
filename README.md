# Zephyr™ Mechanical Keyboard (ZMK) Firmware

[![Discord](https://img.shields.io/discord/719497620560543766)](https://zmk.dev/community/discord/invite)
[![Build](https://github.com/zmkfirmware/zmk/workflows/Build/badge.svg)](https://github.com/zmkfirmware/zmk/actions)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg)](CODE_OF_CONDUCT.md)

[ZMK Firmware](https://zmk.dev/) is an open source ([MIT](LICENSE)) keyboard firmware built on the [Zephyr™ Project](https://www.zephyrproject.org/) Real Time Operating System (RTOS). ZMK's goal is to provide a modern, wireless, and powerful firmware free of licensing issues.

Check out the website to learn more: https://zmk.dev/.

You can also come join our [ZMK Discord Server](https://zmk.dev/community/discord/invite).

To review features, check out the [feature overview](https://zmk.dev/docs/). ZMK is under active development, and new features are listed with the [enhancement label](https://github.com/zmkfirmware/zmk/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement) in GitHub. Please feel free to add 👍 to the issue description of any requests to upvote the feature.

---

ZMK フォーク。

- `app/boards/arm/nakid30/` — [nakid30](https://github.com/fruitjuice088/nakid30) の board 定義(MDBT50Q 直付け・ダイレクトスキャン 30キー分割)
- `app/boards/shields/k30_split/` — [k30_split](https://github.com/fruitjuice088/k30_split) の shield 定義(XIAO nRF52840・4×5 マトリクス)
- `app/boards/shields/revxlp30/` — revxlp を 30キーで使うための shield 定義
- `app/snippets/fj88/k30.keymap` — 上記すべてが include するキーマップ本体(大西配列ベース 30キー)
- `app/src/behaviors/behavior_jp_enter.c` — 日本語配列キーコードを送る behavior
- `app/src/behaviors/behavior_persistent_macro.c` — キー列を記録・再生する behavior

ビルド（Docker必須）
```bash
cd zmk_shingeta
./build.sh          # nakid30 左右をビルド（初回は自動で west init + update）
./build.sh split    # k30_split 左右をビルド
./build.sh revxlp   # revxlp30 をビルド
./build.sh clean    # ビルドキャッシュ削除
./build.sh update   # 依存の更新（west update）
./build.sh nuke     # volume全削除（完全リセット）
```
成果物: `.build/*.uf2`

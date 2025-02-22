# Zephyr™ Mechanical Keyboard (ZMK) Firmware

[![Discord](https://img.shields.io/discord/719497620560543766)](https://zmk.dev/community/discord/invite)
[![Build](https://github.com/zmkfirmware/zmk/workflows/Build/badge.svg)](https://github.com/zmkfirmware/zmk/actions)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg)](CODE_OF_CONDUCT.md)

[ZMK Firmware](https://zmk.dev/) is an open source ([MIT](LICENSE)) keyboard firmware built on the [Zephyr™ Project](https://www.zephyrproject.org/) Real Time Operating System (RTOS). ZMK's goal is to provide a modern, wireless, and powerful firmware free of licensing issues.

Check out the website to learn more: https://zmk.dev/.

You can also come join our [ZMK Discord Server](https://zmk.dev/community/discord/invite).

To review features, check out the [feature overview](https://zmk.dev/docs/). ZMK is under active development, and new features are listed with the [enhancement label](https://github.com/zmkfirmware/zmk/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement) in GitHub. Please feel free to add 👍 to the issue description of any requests to upvote the feature.

---

ZMKの`behavior`として新下駄配列を実装。  
また、親指でEnterキーを押しながらキーを入力することで、ひらがなONと新下駄レイヤをアクティブ化するようにしている。


セットアップ
```bash
cd zmk_shingeta

python3 -m venv .venv
source .venv/bin/activate
pip install west
unset ZEPHYR_BASE
west init -l app/
west update
west zephyr-export
pip install -r zephyr/scripts/requirements-base.txt
pip install -r zephyr/scripts/requirements-extras.txt

cd app
source ../zephyr/zephyr-env.sh
west build -b seeeduino_xiao_ble -- -DSHIELD=revxlp36 && ls -l build/zephyr/zmk.uf2
```

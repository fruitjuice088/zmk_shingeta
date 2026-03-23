# ZMK Shingeta - Project Instructions

## Build

```bash
./build.sh            # インクリメンタルビルド
./build.sh build -p   # クリーンビルド (pristine)
./build.sh clean      # ビルドキャッシュ削除
./build.sh nuke       # Docker volume全削除 + ビルドキャッシュ削除
./build.sh update     # west依存の更新
```

- 成果物: `.build/<shield>-<board>.uf2`
- Docker named volume (`zmk-zephyr`, `zmk-modules`, `zmk-tools`, `zmk-west`) にwest依存をキャッシュ
- ベースイメージ: `zmkfirmware/zmk-build-arm:stable`

## Project Structure

- `app/` — ZMKアプリケーション (自分のコード。git管理対象)
- `app/boards/shields/revxlp30/` — ボード定義
- `app/snippets/fj88/k30.keymap` — キーマップ本体
- `app/src/behaviors/behavior_shingeta.c` — 新下駄配列のカスタムbehavior
- `Dockerfile.build`, `build.sh` — ビルド環境
- `zephyr/`, `modules/`, `tools/`, `.west/` — west updateで生成 (gitignore済)

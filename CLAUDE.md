# zmk_shingeta - Project Instructions

## Build

```bash
./build.sh            # nakid30 左右をインクリメンタルビルド (デフォルト)
./build.sh build -p   # クリーンビルド (pristine)
./build.sh split      # k30_split 左右をビルド
./build.sh clean      # ビルドキャッシュ削除
./build.sh nuke       # Docker volume全削除 + ビルドキャッシュ削除
./build.sh update     # west依存の更新
```

- 成果物: `.build/キーボード名_{left,right}.uf2`
- Docker named volume (`zmk-zephyr`, `zmk-modules`, `zmk-tools`, `zmk-west`) にwest依存をキャッシュ
- ベースイメージ: `zmkfirmware/zmk-build-arm:stable`

## Project Structure

- `app/` — ZMKアプリケーション (自分のコード。git管理対象)
- `app/boards/arm/nakid30/` — nakid30 の board 定義
- `app/boards/shields/k30_split/` — k30_split の shield 定義
- `app/boards/shields/revxlp30/` — revxlp30 の shield 定義
- `app/snippets/fj88/k30.keymap` — キーマップ本体 (上記すべてが include する)
- `app/src/behaviors/behavior_jp_enter.c` — 日本語配列キーコードのカスタムbehavior
- `app/src/behaviors/behavior_persistent_macro.c` — キー列の記録・再生のカスタムbehavior
- `Dockerfile.build`, `build.sh` — ビルド環境
- `zephyr/`, `modules/`, `tools/`, `.west/` — west updateで生成 (gitignore)

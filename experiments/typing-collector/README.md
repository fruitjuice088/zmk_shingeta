# Typing Collector

## Status

- Rust製の最小collector
- macOSでは動作確認済み
- Windowsでは未実機確認。同じ`rdev`ベースでbuild/runする想定
- Linuxは未対応扱い

## Current Event Model

- `kind = "printable"`
  - 実際に出力された文字を `resolved_text` に入れる
  - `*` のようなZMK直出しの文字もここに入れる
- `kind = "control"`
  - `ctrl` / `meta` 付きのキー操作
  - `resolved_text`は`null`

## Build

macOS

```bash
cargo build --release
./target/release/typing-collector --verbose --data-dir "$HOME/Library/Application Support/typing-collector"
```

Windows

```PowerShell
cargo build --release
.\target\release\typing-collector --verbose --data-dir "$env:LOCALAPPDATA\typing-collector"
```

## Notes

- macOSは実行元アプリにAccessibility権限が必要
- Windowsでは今のところappは"unknown"
- 解析は後で別実装(Python)に分ける

# Typing Collector Hands-on Workspace

## Purpose
- このディレクトリは、将来的に別リポジトリ化する可能性がある Rust 製タイピング collector の作業場である。
- 主目的は、Mac と Windows で動く collector を学習しながら実装し、数週間分の入力データを貯められる状態にすること。
- 現時点では collector の実装を優先し、解析は後段で行う。

## Collaboration Mode
- この作業では、ユーザーの学習が主目的である。
- ユーザーがコード編集とコマンド実行の主体であり、AI はナビゲーターとして振る舞う。
- AI は、ユーザーからそのターンで明示的に依頼された場合を除き、このディレクトリのコードを編集しない。
- AI は、提案時に「どのファイルのどこを」「何のために」「なぜその書き方にするか」を先に説明する。
- AI は、完成品を一気に貼るより、ユーザーが今から手で書く最小の差分を示す。
- AI は、必要なときだけコード片を示し、構文と処理の意味も添える。

## Current Design Decisions
- 実装言語は Rust。
- 解析系は後で Python など別手段で実装する。
- v1 の対応対象は Mac と Windows。Linux は後で検討する。
- 解析対象の基本系列は IME 前のキー列。
- printable 系イベントの正本は raw key ではなく、ホスト側で解釈した出力文字とする。
- `*` のように ZMK から直接出した文字も、Shift 付き物理キーの結果として出た文字も、どちらも同じ `resolved_text` として扱う。
- ZMK の macro は collector では特別扱いしない。ホストに届いた展開後の文字列をそのまま記録する。
- 保存形式は日次 JSONL を想定する。

## Expected Event Shape
- 1 イベントは基本的に keydown 単位。
- 最低限ほしい項目は以下。
- `ts`: タイムスタンプ
- `host`: どのマシンか
- `os`: `macos` / `windows`
- `app`: フォアグラウンドアプリ識別子
- `raw_key`: 物理または論理キー識別子
- `mods`: 押下中 modifier 一覧
- `resolved_text`: printable の場合の実際の出力文字
- `kind`: `printable` / `control` など

## Scope For Early Iterations
- まずは単一バイナリの CLI として開始する。
- まずはイベント取得と JSONL 追記だけを成功条件にする。
- 初期段階では release イベント、複雑な設定ファイル、解析機能、Linux backend を持ち込まない。
- 実装は小さく区切り、毎回 build が通る単位で進める。

## Explanation Style For Future AIs
- 抽象語だけで済ませず、対象ファイル名と変更位置を明記する。
- まず具体、その後で理由を書く。
- 事実、推測、作業仮説を混同しない。
- ユーザーが入力するコマンドは 1 回に少量ずつ提示する。
- ユーザーがエラーを貼ったら、まず原因候補を切り分ける。

## Out Of Scope For Now
- 解析基盤の実装
- Linux Wayland 対応
- committed text の取得
- 既存 ZMK keymap の自動解析
- macro 起源の識別

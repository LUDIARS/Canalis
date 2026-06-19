# data: raw JSONL ストア (rawSave / replay の永続形)

①の出力 `RawRecord[]` を verbatim 保存する永続データ。再処理 (replay) の起点になる。
正本: `src/save/raw-writer.ts` (`JsonlRawSink` / `loadRawRecordsFromFile` / `loadRawRecordsFromDir`)。

## 保存形式

- 1 ファイル = 1 クロールバッチ。1 行 = 1 `RawRecord` の JSON (JSONL)。末尾改行あり (空配列なら空ファイル)。
- 文字コード UTF-8。

## 配置レイアウト

```
<dir>/<stamp>.jsonl
```

- `dir`: `JsonlRawSinkConfig.dir`。source ごとにサブフォルダを切る運用を想定。
- `stamp`: `JsonlRawSinkConfig.stamp` 省略時は書込時刻 (ISO8601、`:` → `-` 置換)。

CLI `notion-crawl` 経由の実配置 (実物):

```
data/raw/notion/<databaseId>/<stamp>.jsonl
```

例: `data/raw/notion/22839cbfbab9806d95c5fd8ce7f5977f/2026-06-10T06-47-49.968Z.jsonl`

> `data/` は `.gitignore` で無視されるランタイム出力先。`spec/data/` を巻き込まないよう
> `.gitignore` のパターンは `/data/` (anchored) にしてある。

## 読み戻し (replay)

- `loadRawRecordsFromFile(file)`: 1 本の JSONL を `RawRecord[]` に復元 (空行はスキップ)。
- `loadRawRecordsFromDir(dir)`: ディレクトリ内の全 `*.jsonl` を**ファイル名昇順**で連結。
- runner は `manifest.replayFrom` 指定 + `deps.replayLoader` 注入で、再クロールせず raw store から ②→③ を焼き直す。replay 時は raw を上書きしない。

関連: [data/raw-record.md](./raw-record.md) / [feature/replay.md](../feature/replay.md)

# Canalis — Claude 向けメモ

LUDIARS 共有データ取込パイプライン基盤。略称 **Ca** (暫定)。

## これは何か

外部データを ① crawl → ② clean/categorize → ③ save の 3 ステージで取り込む共有 lib。
契約 (型) で疎結合。Tr の Notion クローラ / Di の外部コメント収集を載せ替えるために作成。

## 鉄則 (踏み外し厳禁)

- **LLM をできる限り排除する。** 共有 lib に LLM 依存を足さない (SDK/API キー禁止)。
  整形は決定論 (セレクタ/正規表現/Lector/辞書/ルール) が第一。LLM はサービス ② の opt-in のみ。
- **②Transform を共有 lib に置かない。** ②はサービス固有 = 各サービスリポ。
  共有 lib は ①adapter / ③writer / 契約 / runner だけ (規約 §3)。
- **③writer に table-map/射影/サービス知識を入れない。** ②が最終形まで作り、③は verbatim 書込。
- **③writer に DB ドライバを直接依存させない。** `SqlExecutor`/`CypherExecutor` を注入。

## branch 運用

- substantive な編集は feat/ ブランチ + PR (main 直編集しない)。
- Ars 配下なので CI green ならオートマージ (squash+delete) 可。

## 関連

- `共通スクレイピングlib構想` (取得層の前身) / **Lector** (HTML→構造化、②で利用)。
- 移植元: Tirocinium `packages/notion` + `scripts/notion-crawl`。
- 設計詳細: `DESIGN.md`。

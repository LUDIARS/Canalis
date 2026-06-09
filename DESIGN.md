# Canalis — 設計 (DESIGN)

LUDIARS 共有データ取込パイプライン基盤。略称 **Ci**。

クロール/スクレイピングで集めた外部データを、各サービスの内部仕様へ整形し、保存先
(RDB / グラフ DB / ファイル) へ流し込む 3 ステージを、**契約 (型) で疎結合に接続する**。
Tirocinium の Notion クローラと Discutere の外部コメント収集を共通基盤に載せ替えるために起こした。

関連: `共通スクレイピングlib構想`(取得層の前身) / **Lector**(HTML→構造化 parse 層、本ツールの ②で利用可)。

---

## 0. 設計原則

### 0.1 LLM をできる限り排除する (最重要)

**Canalis は ①Crawl・②Transform を通じて LLM 利用を限りなく排除する。**

- 共有 lib (`@ludiars/canalis`) は **LLM への依存を一切持たない**。SDK も API キーも要求しない。
- 整形・分類・付与 (②) は **決定論的手段を第一**とする: セレクタ / 正規表現 / パーサ
  (Lector) / 辞書 / ルール / 構造化スキーマ。
- LLM が避けられない処理 (例: 自由文からの感情推定) は、**サービス側 ②の中で明示的な
  opt-in アダプタ**として実装する。Canalis core はそれを知らないし呼ばない。
- 理由: 決定論的処理は (1) 再現性が高く replay と相性が良い、(2) コスト/レイテンシ/
  レート制限から自由、(3) テスト可能。LLM は「最後の手段」であって既定の道具ではない。

> Tirocinium 旧クローラは企業情報抽出に LLM (+ heuristic fallback) を使っていたが、
> Canalis へ移すにあたり **heuristic/ルールを主、LLM を従** に逆転させる方針。

### 0.2 整形の知識は ②に集約し、③は verbatim 書込

②が「保存先のネイティブ最終形」まで作り切る。③は渡された形をそのまま書くだけで、
table-map も射影もサービス知識も持たない。→ ③writer は sink エンジン 1 種につき 1 個で全サービス共有。

### 0.3 共有 lib は 1 サービス専用ロジックを持たない (規約 §3)

① adapter (汎用) と ③ writer (汎用) と契約・runner だけを共有。**②Transform は各サービスリポに置く。**

---

## 1. 3 ステージ

```
① Crawl/Scrape ──RawRecord[]──┬──▶ raw sink (verbatim 保存)        [任意]
   汎用 (adapter)              │
                              └──▶ ② Clean/Categorize ──sink-native──▶ ③ Save/Import
                                    サービス固有 (plugin)              汎用 (writer)
```

| ステージ | 役割 | 汎用/固有 | 実体 |
|---|---|---|---|
| ① Crawl | 取得元から生データ取得 | 汎用 (adapter) | `Source` 実装。例: `NotionSource` |
| ② Clean | サービス内部仕様へ整形・分類・付与 (LLM 排除) | **サービス固有** | `Transform` 実装 (各リポ) |
| ③ Save | 保存先へ verbatim 書込 | 汎用 (writer) | `Sink` 実装。raw/postgres/kuzu |

新サービス追加コスト = **②plugin 1 本 + manifest 1 枚**。①は source が被れば流用、③は DB エンジンが被れば無改修。

---

## 2. 受け渡し契約

### 2.1 `RawRecord` (①→② / ①→raw)

source 非依存の正規化生レコード。`raw` に source ネイティブ原データを verbatim 保持。

```ts
type RawRecord = {
  source: string;        // "notion" | "youtube" | "reddit" | ...
  sourceId: string;      // source 内一意 (dedup)
  fetchedAt: string;     // ISO8601 UTC
  url?: string; title?: string; text?: string;  // text=正規化本文 (②の主入力)
  raw: unknown;          // source 原データ verbatim (replay/監査)
  meta?: Record<string, unknown>;
};
```

### 2.2 sink-native エンベロープ (②→③)

②が出す「最終形」。`kind` で ③へ振り分ける。

```ts
type RdbBatch = { kind: 'rdb'; upserts: { table; rows; conflictKey }[] };
type KgBatch  = { kind: 'kg'; nodes: GraphNode[]; edges: GraphEdge[] };
type FtBatch  = { kind: 'ft'; task: 'classification'|'causal-lm'; examples: FtExample[]; model?; dataset? };
type SinkBatch = RdbBatch | KgBatch | FtBatch;
```

raw は別系統で、raw sink は `RawRecord[]` を直接受ける (`accepts: 'raw'`)。
`FtBatch` は「学習的インポート」用 (§7)。

---

## 3. マニフェスト (サービスが渡す「フォーマット」)

```yaml
pipeline: di-game-sentiment
crawl:
  sources:
    - { adapter: youtube, config: { videoId: "..." } }
    - { adapter: reddit,  config: { subreddit: "..." } }
  rawSave: { adapter: jsonl, accepts: raw, config: { dir: "data/raw/di" } }
transform: di:comment-sentiment      # ② (サービス側登録。LLM opt-in はこの中)
save:
  - { adapter: kuzu, accepts: kg }

# Tr: 1 つの ② が RdbBatch + KgBatch を出し、2 sink へ振り分く例
pipeline: tr-companies
crawl:
  sources: [{ adapter: notion, config: { databaseId: "..." } }]
  rawSave: { adapter: jsonl, accepts: raw, config: { dir: "data/raw/tr" } }
transform: tr:company-normalize
save:
  - { adapter: postgres,    accepts: rdb }   # 企業 → ポスグレ
  - { adapter: reco-kuzu,   accepts: kg }    # 推薦タグ関係 → graph

# 再処理: crawl の代わりに raw store から ②→③ を焼き直す
replayFrom: { adapter: jsonl, accepts: raw, config: { dir: "data/raw/tr" } }
```

実体 (source/transform/sink) は `runPipeline(manifest, deps)` の `deps` に DI する。
adapter 名 → インスタンスの解決はサービス側が行う (core はグローバル registry を持たない)。

---

## 4. 再処理 (replay)

`①→②` が常に `RawRecord[]` なので、raw store はその同じ形のバッファ。②のロジックを
変えたら **再クロールせず raw store → ② → ③ で焼き直せる**。クロールは rate-limit /
元データ消失 / コスト高なので、これが実運用で効く。runner は `deps.replayLoader` を
渡せば `manifest.replayFrom` を起点にする (raw 保存は上書きしない)。

---

## 5. リポ構成

```
src/
  core/      契約: raw-record / envelope / pipeline(Source,Transform,Sink) / manifest
  crawl/
    notion/  ①adapter (Tr packages/notion 移植 + source.ts で RawRecord 化)
  save/      ③writer: raw-writer(jsonl) / postgres-writer / kuzu-writer / ft-writer
  runners/ft/  FT runner 契約 + 参照 Python (core 外、 tsc 対象外)
  runner/    manifest 実行オーケストレータ
  cli.ts     ①+raw だけの standalone (notion-crawl)。Tr scripts/notion-crawl 置換
test/        fake で ①→② 契約を固定
```

③ writer の DB ドライバ (`postgres` / `kuzu`) は **注入**する (`SqlExecutor` / `CypherExecutor`)。
共有 lib は DB ドライバにも LLM にも直接依存しない (§0.1 / §18)。

---

## FT — 学習的インポート (汎用、A/B 両対応)

収集・整形したデータを **ローカル LLM へ学習として取り込む**経路。③Save の sink の一種
(`FtSink`, `accepts: 'ft'`) として実装する。**core は学習を持たない** (§0.1)：`FtSink` は
データセットを materialize し、 core 外の **runner** を起動するだけ。

```
② ラベル付きデータ ──FtBatch──▶ ③ FtSink
   (task: classification | causal-lm)   ├─ <dir>/<dataset>/<stamp>/data.jsonl  (1行1例)
                                         ├─ 同/job.json  (task/model/count/path…)
                                         └─ runner(job) を起動 (任意)  → core 外で fine-tune
```

- **A=分類器** (`task:'classification'`, 例 `{input,label}`): 0+1 カスケードのラベルを教師に
  ローカル小型分類器を FT → 使うほど LLM 需要↓ の自己改善ループ。
- **B=生成LLM** (`task:'causal-lm'`, 例 `{messages}` / `{prompt,completion}`): ドメイン知識を
  LoRA でローカル LLM に注入。
- runner は注入 (`FtRunner`)。`commandRunner(cmd,args)` が `<cmd> --job <job.json>` で外部
  プロセス起動 (shell 非経由・exit 購読)。契約と参照実装は `runners/ft/`。
- core は `kuzu`/`postgres` 同様、**学習ランタイム (Python/PEFT 等) に直接依存しない**。

## 6. 未了 (scaffold 時点)

- ① adapter: youtube / reddit / website (Di 用)。Lector を parse に組み込む口。
- ③ writer: postgres/kuzu の **実 DB 結合テスト** (現状は SQL/Cypher 生成まで、未通電)。
- ② のリファレンス実装は各サービス側 (Tr=company-normalize / Di=comment-sentiment)。
  決定論実装を主、LLM opt-in を従で。
- manifest ローダ (YAML/JSON) と CLI の full-pipeline 実行 (現 CLI は crawl+raw のみ)。
- 実スキーマ突合: Di の Kuzu / Tr の companies・推薦 graph 定義とエンベロープの最終整合。
- FT runner の実体実装 (`runners/ft/train.py` は雛形)。classification=transformers+peft /
  causal-lm=unsloth 等。GPU/依存は環境次第。FtSink(TS) 側は実装済 + テスト済。
- ✅ PROJECT-CODES への 2 文字コード登録 = `Ci` (LUDIARS PR #29)。

# 段階 5: core の runner・manifest、save、llm

設計: `spec/plan/cpp-port.md` §9 の 5。

## 1. 機能の対応表

| TS | C++ | 備考 |
|---|---|---|
| `src/core/envelope.ts` | `native/src/canalis/core/envelope.hpp` | ② が組むエンベロープは JSON の object のまま受け渡す (`SinkBatch = Json`) |
| `src/core/pipeline.ts` | `native/src/canalis/core/pipeline.*` | `Source<Config>`・`JsonSource`・`Transform`・`Sink` (`write_raw` / `write_batch`) |
| `src/core/{manifest,manifest-loader}.ts` | `native/src/canalis/core/manifest.*` | D3: JSON だけを読む。.yaml / .yml は JSON への変換を案内して失敗にする |
| `src/runner/runner.ts` | `native/src/canalis/runner/runner.*` | 段ごとの失敗を errors に積む規則、manifest の誤りで止める規則、replay の規則は同じ |
| `src/save/raw-writer.ts` | `native/src/canalis/save/raw_writer.*` | `JsonlRawSink` が `Sink` (accepts raw) を実装するようにした |
| `src/save/postgres-writer.ts` | `native/src/canalis/save/postgres_writer.*` | `SqlExecutor` を注入する (ドライバを持たない) |
| `src/save/kuzu-writer.ts` | `native/src/canalis/save/kuzu_writer.*` | `CypherExecutor` を注入する |
| `src/save/ft-writer.ts` | `native/src/canalis/save/ft_writer.*` | `command_runner` は引数の配列で起動する (シェルを通さない) |
| `src/llm/types.ts` | `native/src/canalis/llm/types.hpp` | |
| `src/llm/cascade.ts` | `native/src/canalis/llm/cascade.*` | |
| `src/llm/lexicon.ts` | `native/src/canalis/llm/lexicon.*` | 辞書は JSON の object (キーの順が同点のときの順) |
| `src/llm/local-openai.ts` | `native/src/canalis/llm/local_openai.*` | |
| `src/llm/scorer.ts` | `native/src/canalis/llm/scorer.*` | |

`canalis::llm` は別ターゲットにして、core / crawl / save からは使わない (DESIGN §0.1 の「core は LLM を知らない」をリンクの形でも保つ)。

## 2. テストの対応表

| TS テスト | ctest |
|---|---|
| `test/core/manifest-loader.test.ts` | `runner.runner` (JSON の読み込み・pipeline 必須・無いファイル。YAML は D3 で失敗になることを確かめる) |
| — (runner) | `runner.runner` (crawl → raw 保存 → transform → kind ごとの振り分け・段ごとの失敗・manifest の誤り・replay) |
| `test/ft-writer.test.ts` | `save.writers` (classification・causal-lm・合わない例・危ない dataset 名・command_runner) |
| `test/save/{postgres,kuzu}-integration.test.ts` (本物の DB) | `save.writers` (実行関数を記録して、文とパラメタの形を確かめる。本物の DB には繋がない) |
| `test/cascade.test.ts`・`test/local-openai.test.ts` | `llm.llm` |

このセッションでの結果 (Windows・Visual Studio 2022・Release): 21 件すべて通過。

## 3. 安全の対応表

| 防御 | C++ |
|---|---|
| SQL の識別子 | TS と同じく `"` で引用し、普通の名前でなく `"` を含むものは拒否する。値は必ずパラメタ |
| Cypher の名前 | label に加え、プロパティ名と MERGE キーの名前も `[A-Za-z_][A-Za-z0-9_]*` で検査する (TS は label だけ。プロパティ名がクエリに直接入るため) |
| FT の出力先 | dataset 名は出力先の 1 区間だけ (`/`・`\`・`..` で外へ出さない)。TS は検査しない |
| FT の書きかけ | 例をすべて検査してから書く (合わない例があれば何も書かない) |
| FT runner | 引数の配列で起動 (シェルを通さない)。期限 (既定 24 時間)・出力の上限 (`proc::run` の既定) つき |
| ローカル LLM | 利用者が設定したローカルのサービスなので private network を許す。転送は追わない。API キーは Authorization ヘッダだけ |
| manifest の大きさ | 4 MiB まで、JSON は上限つきで読む |

## 4. 移していないもの・挙動の差

- manifest は JSON だけ (D3)。既存の YAML の manifest は JSON に変換する。
- manifest の読み込みで、`crawl.sources[]` の adapter・`save[]` の adapter と accepts を必須として検査する (TS は実行時に落ちる)。
- `Cascade::score_batch` は 1 件ずつ順に評価する (TS は並行)。
- FT runner の標準出力・標準エラーは親に流さず、失敗のときに標準エラーの末尾をエラー文に添える (TS は `stdio: 'inherit'`)。
- `JsonlRawSink::write` は書いたファイルのパスを返す (TS は void)。

## 5. 土台に欲しいもの / 人間の判断が要ること

- 段階 6 (利用者の切り替え) と段階 7 (TS 版の退役・配布) が残る。どちらも段階 1 からの PR がマージされてから。

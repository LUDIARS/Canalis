# Canalis native (C++)

TS 版 (`src/`) を C++20 に移したもの。設計と段階は `spec/plan/cpp-port.md`。移行中は TS 版と並べる。

- 依存と取り込みの検査: `native/docs/DEPENDENCIES.md`
- 書き方の規約: Discutere native の `native/docs/PORTING.md` に従う (名前空間は `canalis::<ディレクトリ>`、include は `canalis/<ディレクトリ>/<名前>.hpp`)
- 段階ごとの対応表・挙動の差: `native/docs/port-notes/*.md`

## 今あるもの (段階 1〜5)

| ターゲット | 中身 |
|---|---|
| `canalis::base` | JSON (上限つき)・文字列・時刻・ログ・環境変数・ファイル・URL (相対 URL の解決を含む)・子プロセス・暗号の基本 |
| `canalis::infra` | 外部への HTTP/HTTPS クライアント (期限・応答の上限・SSRF 対策)、導入済みの Edge / Chrome を CDP pipe で動かす Browser / Page |
| `canalis::config` | 暗号化 config (`config.json`、TS 版と同じ形式) |
| `canalis::core` | 受け渡しの契約 (RawRecord・Source・Transform・Sink・エンベロープ)、manifest (JSON) |
| `canalis::runner` | manifest を解釈して crawl → raw 保存 → transform → save を動かす `run_pipeline` |
| `canalis::crawl` | ①Crawl adapter: website・notion-public (`NotionPublicSource`・`crawl_public_pages`)・notion (公式 API: `NotionSource`・`crawl_database`・`crawl_page`・URL の変換)・youtube・reddit・discord (チャンネル / ギルド)・steam (新作一覧 / アプリ別レビュー / ユーザ別レビュー) |
| `canalis::save` | ③Save の writer: raw JSONL (`JsonlRawSink`・読み戻し)・Postgres・Kuzu (実行関数を注入)・FT (データセット + runner) |
| `canalis::llm` | opt-in のローカル LLM (OpenAI 互換)・推論カスケード・辞書のスコアラ。core / crawl / save からは使わない |
| 実行ファイル `canalis` | `notion-crawl`・`config` (TS 版と同じ) と `crawl`・`notion-public-crawl`・`website-parse` (C++ 版で追加)。引数は `spec/interface/cli.md` |

notion-public は導入済みの Edge / Chrome を使う (Chromium はダウンロードしない)。別のブラウザを使うときは env `CANALIS_BROWSER` に実行ファイルのパスを入れる。

## ビルドとテスト

```
cmake -S native -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure -j2
```

Windows (Visual Studio 2022) では `-G "Visual Studio 17 2022" -A x64` と、必要なら `-DOPENSSL_ROOT_DIR=<OpenSSL の場所>` を渡す。
取得済みの依存を使うときは `-DFETCHCONTENT_SOURCE_DIR_BOOST=<boost-1.87.0>`・`-DFETCHCONTENT_SOURCE_DIR_JSON=<json>`・`-DLORICA_SOURCE_DIR=<Lorica の clone>` を渡す (`-DFETCHCONTENT_FULLY_DISCONNECTED=ON` で通信しない)。

golden の fixtures は `native/tests/golden/*.gen.mjs` を node で実行して作り直す (Node の組み込みだけを使う)。

本物のブラウザを使う結合テスト (`infra.browser_e2e`) は `CANALIS_BROWSER_E2E=1` のときだけ動く (loopback の偽のページだけを取り、外への通信はプロキシ指定で塞ぐ)。

## 他のプロジェクトから組み込む (add_subdirectory)

Discutere native・Tirocinium native のように C++ のプロジェクトから使うときは、Canalis を submodule にして組み込む。

```cmake
# 親が同じ名前 (json / boost / lorica) で同じ版を宣言していれば、それが使われる (CanalisDependencies.cmake)。
# 親が Boost.Beast の分離コンパイルの単位をすでにコンパイルしているなら OFF にする (二重のリンクを避ける)。
set(CANALIS_COMPILE_BEAST OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/canalis/native ${CMAKE_BINARY_DIR}/_canalis EXCLUDE_FROM_ALL)
target_link_libraries(my_app PRIVATE canalis::crawl)   # canalis::core / canalis::save / canalis::runner / canalis::llm も同じ
```

組み込むときは、ctest のテストと `install` を作らない (Canalis 単独でビルドしたときだけ)。実行ファイル `canalis` は、ターゲットとして指定したときだけビルドされる (`EXCLUDE_FROM_ALL`)。

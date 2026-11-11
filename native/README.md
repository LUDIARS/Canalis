# Canalis native (C++)

TS 版 (`src/`) を C++20 に移したもの。設計と段階は `spec/plan/cpp-port.md`。移行中は TS 版と並べる。

- 依存と取り込みの検査: `native/docs/DEPENDENCIES.md`
- 書き方の規約: Discutere native の `native/docs/PORTING.md` に従う (名前空間は `canalis::<ディレクトリ>`、include は `canalis/<ディレクトリ>/<名前>.hpp`)
- 段階ごとの対応表・挙動の差: `native/docs/port-notes/*.md`

## 今あるもの (段階 1)

| ターゲット | 中身 |
|---|---|
| `canalis::base` | JSON (上限つき)・文字列・時刻・ログ・環境変数・ファイル・URL・子プロセス・暗号の基本 |
| `canalis::infra` | 外部への HTTP/HTTPS クライアント (期限・応答の上限・SSRF 対策) |
| `canalis::config` | 暗号化 config (`config.json`、TS 版と同じ形式) |

## ビルドとテスト

```
cmake -S native -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure -j2
```

Windows (Visual Studio 2022) では `-G "Visual Studio 17 2022" -A x64` と、必要なら `-DOPENSSL_ROOT_DIR=<OpenSSL の場所>` を渡す。
取得済みの依存を使うときは `-DFETCHCONTENT_SOURCE_DIR_BOOST=<boost-1.87.0>`・`-DFETCHCONTENT_SOURCE_DIR_JSON=<json>`・`-DLORICA_SOURCE_DIR=<Lorica の clone>` を渡す (`-DFETCHCONTENT_FULLY_DISCONNECTED=ON` で通信しない)。

golden の fixtures は `node native/tests/golden/config_crypto.gen.mjs` で作り直す (Node の組み込みだけを使う)。

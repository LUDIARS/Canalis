# 段階 1: 土台 (CMake・依存・base・infra・config)

設計: `spec/plan/cpp-port.md` §9 の 1。

## 1. 機能の対応表

| TS / Node | C++ | 備考 |
|---|---|---|
| (土台) JSON・文字列・時刻・ログ・環境変数・ファイル・URL・子プロセス・暗号の基本 | `native/src/canalis/base/*` | Discutere native `src/base` (Discutere `8dd73e7` 時点) から写した。名前空間を `di` → `canalis` に、include を `canalis/base/...` に変えた |
| `fetch` | `native/src/canalis/infra/http_client.*`・`http_attempt*`・`tls.*` | 同じく Discutere `src/infra` から写した。User-Agent の既定値だけを `canalis-native/0.1` に変えた |
| — | `native/src/canalis/infra/headers.*` | Discutere `src/infra/http.*` の `Headers` だけを切り出した (サーバ側の Request / Router は写していない) |
| `src/config/crypto.ts` | `native/src/canalis/config/crypto.*` | 新しく書いた。OpenSSL の `EVP_PBE_scrypt` (N=16384, r=8, p=1) と AES-256-GCM |
| `src/config/store.ts` | `native/src/canalis/config/store.*` | 新しく書いた |
| `os.hostname()` / `os.userInfo().username` | `canalis::env::hostname()` / `username()` | env に足した (master 鍵のマシン束縛値に使う) |

写さなかったもの (Canalis で使わない): Discutere の `base/sqlite`・`base/sidecar`・`base/executor`・`base/parallel`・`base/web_assets`・`infra/http_server`・`infra/ws_client`、`base/log` の Vestigium 書き出し。

## 2. テストの対応表

| TS テスト | ctest |
|---|---|
| `test/config-store.test.ts` (crypto の往復・鍵違い・暗号化保存・解決順・非シークレット・マスク) | `config.store` |
| — (TS 版との互換) | `config.store` の golden 2 件。`native/tests/golden/config_crypto.gen.mjs` (Node の組み込みだけ) で fixtures を作る |
| — (写した土台) | `base.json`・`base.strings`・`base.crypto_time`・`base.process` (Discutere のテストを写した)、`infra.http_client` (Asio の偽のサーバで書いた) |

## 3. 安全の対応表

| 防御 | C++ |
|---|---|
| token を平文で保存しない | AES-256-GCM で暗号化。テストで config.json と表示に平文が無いことを確かめる |
| 改竄・鍵違いを検出する | GCM のタグ検査で失敗させる。保存済み token は「無い」扱い (TS と同じ) |
| 壊れた・大きい config | 1 MiB を超える config.json・暗号文 64 KiB 超は読まない。JSON は `parse_json` の上限つき |
| 外部 HTTP の SSRF | `http_client` は既定で private network へ出ない。名前解決した IP を検査し、その IP に固定して接続する |
| 外部 HTTP の応答の大きさ | `max_response_bytes` (既定 16 MiB) を超えたら失敗 |

## 4. 移していないもの・挙動の差

- `applyNotionConfigToEnv` (process.env へ書き込む) は移さず、`notion_settings()` で値を返す。C++ の Notion クライアント (段階 3) は env を読まずに引数で受ける。解決順 (env が優先) は同じ。
- TS の `readConfig` は配列の JSON も config として受けるが、C++ は object 以外を空として扱う。
- TS の `decryptJson<string>` は中身が文字列でなくてもそのまま返すが、C++ の `get_stored_token` は文字列以外を「無い」扱いにする。
- `canalis config` サブコマンド (TTY の非表示入力を含む) は段階 3 で CLI と一緒に移す。

## 5. 土台に欲しいもの / 人間の判断が要ること

- Discutere native と Canalis native で base / infra が重複している。共有ライブラリへ切り出すかは別の判断 (今回は写して出自を書いた)。

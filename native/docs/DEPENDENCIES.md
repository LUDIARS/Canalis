# C++ 版の依存と取り込みの検査

設計: `spec/plan/cpp-port.md` §5。宣言は `native/cmake/CanalisDependencies.cmake` だけに置く。
版とハッシュは Discutere native (`Discutere/native/cmake/DiDependencies.cmake`、検査は `Discutere/native/docs/INVENTORY.md` §1) と同じにした。
LUDIARS の中で同じ版を使い、検査を一度で済ませるため。

## 版と固定

| 依存 | 版 | 取得元 | 固定 | 用途 |
|---|---|---|---|---|
| Boost (Asio / Beast、ヘッダのみ) | 1.87.0 | github boostorg `boost-1.87.0-b2-nodocs.zip` | SHA256 `40efbb1f38b8cf186fd7bb12809f561ae1c233bae0f75146862e9bfce77d0d42` | HTTPS クライアント |
| nlohmann_json | 3.11.3 | github nlohmann `json.tar.xz` | SHA256 `d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d` | JSON |
| Lorica | commit `cab4f12086e30484f249d1b4aa479bf7cdec13a9` | github LUDIARS/Lorica | git commit | 入力の上限 (`input_budget`)・hardening フラグ |
| OpenSSL | 3.x | システム (Windows: OpenSSL-Win64 3.2.1 で確認、CI: ubuntu `libssl-dev`) | — | TLS・SHA-256・乱数・scrypt・AES-256-GCM |

## 検査 (2026-11-11、external-import-review.md の手順)

1. 固定と実体: この PC の取得済みの物 (`E:/Document/Ars/.toolchains/odeum-native/boost.zip` と `json.tar.xz`) の SHA256 を測り、上の値と一致した。
   Lorica はローカルの clone (`E:/Document/Ars/Lorica`) の HEAD が `cab4f12` と一致した。
   このセッションのビルドは、これらを `FETCHCONTENT_SOURCE_DIR_*` / `LORICA_SOURCE_DIR` で渡し、`FETCHCONTENT_FULLY_DISCONNECTED=ON` にして通信せずに行った。
2. 履歴: Boost と nlohmann_json はハッシュ固定のリリース物なので、上流の後の変更は入らない。
   Lorica はローカルの clone で 2026-11-03 以降のコミットが無かった (`cab4f12` は 2026-10-28)。上流の main がこれより新しいかは、通信しないため確かめていない。
   受け入れ済みの版 (Discutere native) から版の差は無い。
3. 通信: どの依存もそれ自身では通信しない。外部への通信は Canalis のコード (`canalis/infra/http_client`) が、取得元 (Notion・Steam など) に対してだけ行う。
4. 不要な通信: 無い。
5. 依存の整理: TS 版の playwright・js-yaml は不要になる (spec/plan/cpp-port.md §8 D2・D3)。Discutere が使っている SQLite は Canalis では使わないので入れていない。
6. メモリ安全: 第三者のコードには警告を課さない (`CanalisWarnings.cmake`)。Canalis のコードは `/W4 /WX`・`-Wall -Wextra -Wpedantic -Werror` と `lorica_harden_target` を通す。
   外部入力 (HTTP 応答・JSON・config・子プロセス出力) は確保の前に上限をかける (PORTING の規則)。

導入時の処理: CMake の configure は nlohmann_json と Lorica の CMakeLists.txt を読むだけで、ほかのスクリプトは動かさない。Boost はビルドしない (`SOURCE_SUBDIR` に存在しない名前を渡す)。
CI (`.github/workflows/ci.yml` の native) は configure のときに上の 3 つをハッシュ・commit 固定で取得する。

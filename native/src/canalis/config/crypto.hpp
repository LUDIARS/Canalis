#pragma once
// secret の salt 付き可逆暗号 (src/config/crypto.ts の移植)。
// - scrypt(masterSecret, salt) で 32 バイトの鍵を導出 → AES-256-GCM。
//   scrypt の係数は Node の scryptSync の既定 (N=16384, r=8, p=1) と同じなので、TS 版が書いた config.json をそのまま読める。
// - 形式 (EncryptedBlob): v=1、salt (16 バイト)・iv (12 バイト)・tag (16 バイト)・data はすべて base64。salt はレコード毎にランダム。
// - 暗号の処理は OpenSSL 3 に任せる (自作しない)。
#include <optional>
#include <string>
#include <string_view>

#include "canalis/base/json.hpp"

namespace canalis::config {

struct EncryptedBlob {
    int v = 1;
    std::string salt;
    std::string iv;
    std::string tag;
    std::string data;
};

/** 任意の JSON 値を暗号化する (平文は JSON.stringify と同じ書式)。乱数・暗号の失敗は std::runtime_error。 */
EncryptedBlob encrypt_json(const Json& value, std::string_view master_secret);

/** encrypt_json の逆。改竄・鍵の不一致・壊れた形式は std::runtime_error。 */
Json decrypt_json(const EncryptedBlob& blob, std::string_view master_secret);

/** isEncryptedBlob と同じ判定 (v === 1、salt と data が文字列)。 */
bool is_encrypted_blob(const Json& value);

/** JSON の object から読む。is_encrypted_blob が偽なら nullopt。iv / tag が文字列でなければ空文字。 */
std::optional<EncryptedBlob> blob_from_json(const Json& value);

/** TS と同じキー順 (v, salt, iv, tag, data) の object にする。 */
Json to_json(const EncryptedBlob& blob);

} // namespace canalis::config

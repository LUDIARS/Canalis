#include "canalis/config/crypto.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>

#include <openssl/evp.h>

#include "canalis/base/crypto.hpp"

namespace canalis::config {
namespace {

constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kIvBytes = 12;
constexpr std::size_t kTagBytes = 16;
// Node の scryptSync の既定値。変えると既存の config.json を読めなくなる。
constexpr std::uint64_t kScryptN = 16384;
constexpr std::uint64_t kScryptR = 8;
constexpr std::uint64_t kScryptP = 1;
// 128 * N * r = 16 MiB を使う。上限は Node の既定 (32 MiB) に合わせる。
constexpr std::uint64_t kScryptMaxMem = 32ull << 20;
// 暗号化・復号する本文の上限 (config に入るのは token 程度)。壊れた config で大きな確保をしない。
constexpr std::size_t kMaxPayloadBytes = 64u << 10;

using CipherCtx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

unsigned char* bytes_of(crypto::Bytes& b) { return reinterpret_cast<unsigned char*>(b.data()); }
const unsigned char* bytes_of(const crypto::Bytes& b) { return reinterpret_cast<const unsigned char*>(b.data()); }

crypto::Bytes derive_key(std::string_view master_secret, const crypto::Bytes& salt) {
    crypto::Bytes key(kKeyBytes, '\0');
    if (EVP_PBE_scrypt(master_secret.data(), master_secret.size(), bytes_of(salt), salt.size(), kScryptN, kScryptR,
                       kScryptP, kScryptMaxMem, bytes_of(key), key.size()) != 1)
        throw std::runtime_error("scrypt failed");
    return key;
}

CipherCtx new_ctx() {
    CipherCtx ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx) throw std::runtime_error("cipher context allocation failed");
    return ctx;
}

int checked_len(std::size_t n) {
    if (n > kMaxPayloadBytes) throw std::runtime_error("encrypted value is too large");
    return static_cast<int>(n);
}

crypto::Bytes decode_field(const std::string& text, std::size_t expected, const char* name) {
    if (text.size() > kMaxPayloadBytes * 2) throw std::runtime_error(std::string("encrypted blob field is too large: ") + name);
    auto bytes = crypto::base64_decode(text);
    if (!bytes || (expected != 0 && bytes->size() != expected))
        throw std::runtime_error(std::string("invalid encrypted blob field: ") + name);
    return *bytes;
}

void init_gcm(EVP_CIPHER_CTX* ctx, bool encrypt, const crypto::Bytes& key, const crypto::Bytes& iv) {
    const auto init = encrypt ? EVP_EncryptInit_ex : EVP_DecryptInit_ex;
    if (init(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1 ||
        init(ctx, nullptr, nullptr, bytes_of(key), bytes_of(iv)) != 1)
        throw std::runtime_error("cipher init failed");
}

} // namespace

EncryptedBlob encrypt_json(const Json& value, std::string_view master_secret) {
    const crypto::Bytes salt = crypto::random_bytes(kSaltBytes);
    const crypto::Bytes iv = crypto::random_bytes(kIvBytes);
    const crypto::Bytes key = derive_key(master_secret, salt);
    const crypto::Bytes plaintext = json_stringify(value);
    const int plain_len = checked_len(plaintext.size());

    auto ctx = new_ctx();
    init_gcm(ctx.get(), true, key, iv);
    crypto::Bytes out(plaintext.size() + 16, '\0');  // GCM は平文と同じ長さ。余裕を持たせる
    int len = 0;
    if (EVP_EncryptUpdate(ctx.get(), bytes_of(out), &len, bytes_of(plaintext), plain_len) != 1)
        throw std::runtime_error("encrypt failed");
    int total = len;
    if (EVP_EncryptFinal_ex(ctx.get(), bytes_of(out) + total, &len) != 1) throw std::runtime_error("encrypt failed");
    total += len;
    out.resize(static_cast<std::size_t>(total));

    crypto::Bytes tag(kTagBytes, '\0');
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag.size()), tag.data()) != 1)
        throw std::runtime_error("encrypt failed");

    return EncryptedBlob{1, crypto::base64_encode(salt), crypto::base64_encode(iv), crypto::base64_encode(tag),
                         crypto::base64_encode(out)};
}

Json decrypt_json(const EncryptedBlob& blob, std::string_view master_secret) {
    if (blob.v != 1) throw std::runtime_error("unsupported encrypted blob version");
    // salt の長さは Node と同じく問わない (encryptJson が書くのは常に 16 バイト)。iv と tag は GCM の固定長。
    const crypto::Bytes salt = decode_field(blob.salt, 0, "salt");
    const crypto::Bytes iv = decode_field(blob.iv, kIvBytes, "iv");
    crypto::Bytes tag = decode_field(blob.tag, kTagBytes, "tag");
    const crypto::Bytes cipher = decode_field(blob.data, 0, "data");
    const int cipher_len = checked_len(cipher.size());
    const crypto::Bytes key = derive_key(master_secret, salt);

    auto ctx = new_ctx();
    init_gcm(ctx.get(), false, key, iv);
    crypto::Bytes plain(cipher.size() + 16, '\0');
    int len = 0;
    if (EVP_DecryptUpdate(ctx.get(), bytes_of(plain), &len, bytes_of(cipher), cipher_len) != 1)
        throw std::runtime_error("decrypt failed");
    int total = len;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()), tag.data()) != 1)
        throw std::runtime_error("decrypt failed");
    if (EVP_DecryptFinal_ex(ctx.get(), bytes_of(plain) + total, &len) != 1)
        throw std::runtime_error("decrypt failed (wrong key or tampered data)");
    total += len;
    plain.resize(static_cast<std::size_t>(total));

    JsonLimits limits;
    limits.max_bytes = kMaxPayloadBytes;
    return parse_json_or_throw(plain, limits);
}

bool is_encrypted_blob(const Json& value) {
    if (!value.is_object()) return false;
    const auto v = value.find("v");
    if (v == value.end() || !v->is_number() || v->get<double>() != 1) return false;
    const auto salt = value.find("salt");
    const auto data = value.find("data");
    return salt != value.end() && salt->is_string() && data != value.end() && data->is_string();
}

std::optional<EncryptedBlob> blob_from_json(const Json& value) {
    if (!is_encrypted_blob(value)) return std::nullopt;
    return EncryptedBlob{1, json_string(value, "salt"), json_string(value, "iv"), json_string(value, "tag"),
                         json_string(value, "data")};
}

Json to_json(const EncryptedBlob& blob) {
    Json out = Json::object();
    out["v"] = blob.v;
    out["salt"] = blob.salt;
    out["iv"] = blob.iv;
    out["tag"] = blob.tag;
    out["data"] = blob.data;
    return out;
}

} // namespace canalis::config

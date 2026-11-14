#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace canalis::url {

struct Url {
    std::string scheme;    // "http" / "https" / "ws" / "wss" (小文字)
    std::string host;      // IPv6 は角括弧なし
    std::uint16_t port = 0;  // 省略時は scheme の既定
    std::string target;    // パス + クエリ ("/" から始まる)
    std::string path;      // クエリを除いたパス
    std::string query;     // "?" を除いたクエリ
    std::string fragment;
    bool explicit_port = false;
    /** "https://host[:port]" (new URL(u).origin)。 */
    std::string origin() const;
    /** host が IPv6 なら角括弧つき。 */
    std::string host_for_header() const;
    /** new URL(u).href 相当 (origin + path + "?query" + "#fragment")。 */
    std::string href() const;
};

/** 絶対 URL を読む (http/https/ws/wss のみ)。userinfo つき・不正な形は nullopt。 */
std::optional<Url> parse(std::string_view text);

/**
 * new URL(ref, base).href 相当 (Canalis 追加)。base は http/https の絶対 URL。
 * ref が絶対 URL・"//host"・"/path"・"?query"・"#frag"・相対パスのどれでも解決し、"." / ".." を畳む。
 * 空白・制御文字・非 ASCII のバイトは %XX にする。http/https 以外になる ref (mailto: 等) や壊れた URL は nullopt。
 * WHATWG URL の完全な実装ではない (ホストの IDNA 変換・バックスラッシュの扱いはしない)。
 */
std::optional<std::string> resolve(std::string_view base, std::string_view ref);

/** 空白・制御文字・非 ASCII・URL に生で置けない記号 (" < > `) を %XX にする。既にある %XX はそのまま (Canalis 追加)。 */
std::string encode_unsafe(std::string_view s);

/** encodeURIComponent。 */
std::string encode_component(std::string_view s);
/** decodeURIComponent (不正な % は nullopt)。 */
std::optional<std::string> decode_component(std::string_view s);
/** application/x-www-form-urlencoded の値を戻す (+ → 空白)。不正な % はそのまま。 */
std::string decode_form(std::string_view s);
/** クエリ文字列 ("a=1&b=2") を読む。同名は出現順。 */
std::vector<std::pair<std::string, std::string>> parse_query(std::string_view query);
/** URLSearchParams#toString 相当。 */
std::string build_query(const std::vector<std::pair<std::string, std::string>>& params);

} // namespace canalis::url

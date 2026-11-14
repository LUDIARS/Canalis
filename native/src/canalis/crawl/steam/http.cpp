#include "canalis/crawl/steam/http.hpp"

#include <stdexcept>

#include "canalis/base/url.hpp"

namespace canalis::crawl::steam {
namespace {

constexpr int kMaxRedirects = 3;

std::string host_with_port(const url::Url& u) {
    return u.host_for_header() + (u.explicit_port ? ":" + std::to_string(u.port) : std::string());
}

/** 通信の失敗の種類だけを出す (URL も、URL を含み得る理由も出さない)。 */
const char* kind_name(http::FetchErrorKind kind) {
    switch (kind) {
    case http::FetchErrorKind::timeout: return "timeout";
    case http::FetchErrorKind::blocked: return "blocked";
    case http::FetchErrorKind::too_large: return "too large";
    case http::FetchErrorKind::busy: return "busy";
    case http::FetchErrorKind::dns: return "dns";
    case http::FetchErrorKind::invalid_url: return "invalid url";
    case http::FetchErrorKind::too_many_redirects: return "too many redirects";
    case http::FetchErrorKind::network: return "network";
    case http::FetchErrorKind::none: break;
    }
    return "error";
}

} // namespace

SteamHttp::SteamHttp(const SteamHttpOptions& options) : fetch_(options.fetch), sleep_(options.sleep) {
    delay_ms_ = options.polite_delay_ms.value_or(1200);
    if (delay_ms_ < 0) throw std::invalid_argument("steam politeDelayMs must be a non-negative integer");
    if (!fetch_) fetch_ = default_fetch;
    if (!sleep_) sleep_ = default_sleep;
}

http::ClientResponse SteamHttp::get(const std::string& start) const {
    std::string current = start;
    for (int hop = 0;; ++hop) {
        http::ClientRequest req;
        req.url = current;
        req.headers.set("User-Agent", std::string(kSteamUserAgent));
        req.max_redirects = 0;  // 転送は自分でたどる (同じホスト内だけ)
        http::ClientResponse res = fetch_(req);
        if (!res.transport_ok) throw std::runtime_error(std::string("steam fetch failed (") + kind_name(res.error_kind) + ")");
        if (res.status >= 300 && res.status < 400) {
            const auto location = res.headers.get("location");
            if (!location) throw std::runtime_error("steam fetch HTTP " + std::to_string(res.status) + " without location");
            const auto next = url::resolve(current, *location);
            const auto next_url = next ? url::parse(*next) : std::nullopt;
            const auto cur_url = url::parse(url::encode_unsafe(current));
            if (!next_url || !cur_url || next_url->scheme != "https" || host_with_port(*next_url) != host_with_port(*cur_url))
                throw std::runtime_error("steam fetch refused a redirect to another host");
            if (hop >= kMaxRedirects) throw std::runtime_error("steam fetch followed too many redirects");
            current = *next;
            continue;
        }
        if (!res.ok()) throw std::runtime_error("steam fetch HTTP " + std::to_string(res.status));
        return res;
    }
}

Json SteamHttp::get_json(const std::string& url) const { return parse_response_json(get(url), "steam fetch"); }

std::string SteamHttp::get_text(const std::string& url) const { return get(url).body; }

void SteamHttp::polite_wait() const { sleep_(std::chrono::milliseconds(delay_ms_)); }

} // namespace canalis::crawl::steam

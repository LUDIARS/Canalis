#include "canalis/infra/cdp_client.hpp"

#include <algorithm>

#include "canalis/base/time.hpp"

namespace canalis::cdp {
namespace {

std::chrono::milliseconds remaining(std::int64_t deadline_ms) {
    return std::chrono::milliseconds(std::max<std::int64_t>(0, deadline_ms - time::steady_ms()));
}

bool same_session(const Json& message, std::string_view session_id) {
    return json_string(message, "sessionId") == session_id;
}

} // namespace

Client::Client(Transport& transport, JsonLimits limits) : transport_(transport), limits_(limits) {}

std::optional<Json> Client::pump(std::chrono::milliseconds timeout) {
    const auto text = transport_.receive(timeout);
    if (!text) {
        if (transport_.closed()) throw CdpError("cdp: browser connection closed");
        return std::nullopt;
    }
    std::string error;
    auto message = parse_json(*text, limits_, &error);
    if (!message || !message->is_object()) throw CdpError("cdp: invalid message from browser: " + error);
    if (message->contains("id")) return message;
    if (events_.size() >= kMaxQueuedEvents) events_.pop_front();  // 使わないイベントで溜め込まない
    events_.push_back(std::move(*message));
    return std::nullopt;
}

Json Client::call(std::string_view method, const Json& params, std::chrono::milliseconds timeout,
                  std::string_view session_id) {
    const std::int64_t id = next_id_++;
    Json request = Json::object();
    request["id"] = id;
    request["method"] = std::string(method);
    request["params"] = params.is_object() ? params : Json::object();
    if (!session_id.empty()) request["sessionId"] = std::string(session_id);
    if (!transport_.send(json_stringify(request))) throw CdpError("cdp: failed to send " + std::string(method));

    const auto deadline = time::steady_ms() + timeout.count();
    while (true) {
        const auto left = remaining(deadline);
        if (left.count() == 0) throw CdpError("cdp: timed out waiting for " + std::string(method));
        auto message = pump(left);
        if (!message) continue;
        if (json_int(*message, "id", -1) != id) continue;  // 期限切れで捨てた古い要求への応答
        if (const auto err = message->find("error"); err != message->end()) {
            throw CdpError("cdp: " + std::string(method) + " failed: " + json_string(*err, "message", "unknown error"));
        }
        const auto result = message->find("result");
        return result != message->end() ? *result : Json::object();
    }
}

std::optional<Json> Client::wait_event(std::string_view method, std::string_view session_id,
                                       std::chrono::milliseconds timeout) {
    auto take = [&]() -> std::optional<Json> {
        for (auto it = events_.begin(); it != events_.end(); ++it) {
            if (json_string(*it, "method") == method && same_session(*it, session_id)) {
                Json params = json_object(*it, "params");
                events_.erase(it);
                return params;
            }
        }
        return std::nullopt;
    };
    if (auto found = take()) return found;
    const auto deadline = time::steady_ms() + timeout.count();
    while (true) {
        const auto left = remaining(deadline);
        if (left.count() == 0) return std::nullopt;
        pump(left);  // 応答 (id 付き) はここでは来ない想定。来ても読み捨てる
        if (auto found = take()) return found;
    }
}

void Client::drop_events(std::string_view session_id) {
    std::erase_if(events_, [&](const Json& e) { return same_session(e, session_id); });
}

} // namespace canalis::cdp

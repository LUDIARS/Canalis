#include "canalis/base/log.hpp"

#include <cstdio>
#include <map>
#include <mutex>

#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"

namespace canalis::log {
namespace {

struct Registry {
    std::mutex mutex;
    std::map<int, Sink> sinks;
    int next_id = 1;
    bool console = true;
};

Registry& registry() {
    static Registry r;
    return r;
}

thread_local bool in_sink = false;

constexpr std::size_t kMaxMsgBytes = 64 * 1024;

std::string truncate_msg(std::string_view s) {
    if (s.size() <= kMaxMsgBytes) return std::string(s);
    // vestigium: 先頭 (max-32) バイトを UTF-8 として読み、「…(truncated N)」を付ける。
    std::string cut = str::sanitize_utf8(s.substr(0, kMaxMsgBytes - 32));
    return cut + "\xE2\x80\xA6(truncated " + std::to_string(s.size() - (kMaxMsgBytes - 32)) + ")";
}

} // namespace

std::string_view level_name(Level level) {
    switch (level) {
    case Level::trace: return "trace";
    case Level::debug: return "debug";
    case Level::info: return "info";
    case Level::warn: return "warn";
    case Level::error: return "error";
    case Level::fatal: return "fatal";
    }
    return "info";
}

int add_sink(Sink sink) {
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    const int id = r.next_id++;
    r.sinks.emplace(id, std::move(sink));
    return id;
}

void remove_sink(int id) {
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    r.sinks.erase(id);
}

void set_console_enabled(bool enabled) {
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    r.console = enabled;
}

void write(Level level, std::string_view message) {
    auto& r = registry();
    Record rec{level, std::string(message), time::now_ms()};
    if (in_sink) {
        // sink の中から呼ばれた: 再入でロックを取り直さず、標準エラーにだけ出す。
        std::fwrite(message.data(), 1, message.size(), stderr);
        std::fputc('\n', stderr);
        return;
    }
    std::lock_guard lock(r.mutex);
    if (r.console) {
        FILE* out = (level == Level::warn || level == Level::error || level == Level::fatal) ? stderr : stdout;
        std::fwrite(message.data(), 1, message.size(), out);
        std::fputc('\n', out);
        std::fflush(out);
    }
    in_sink = true;
    for (auto& [id, sink] : r.sinks) {
        try {
            sink(rec);
        } catch (...) {
            // sink の失敗でログ呼び出し元を落とさない。
        }
    }
    in_sink = false;
}

} // namespace canalis::log

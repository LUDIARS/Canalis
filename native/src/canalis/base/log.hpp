#pragma once
// ログ。console.log / console.warn / console.error の代わり。
// 書き先: 標準出力・標準エラー (console と同じ) + 登録された sink
// (呼び出し側が足す。Canalis 自身は足さない)。
// 出自: Discutere native src/base/log.hpp。Vestigium 互換の JSONL 書き出しは Canalis に要らないので写していない。
// 秘密値 (bot token・API キー・Authorization ヘッダ) を渡さないこと。
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace canalis::log {

enum class Level { trace, debug, info, warn, error, fatal };
std::string_view level_name(Level level);

struct Record {
    Level level;
    std::string message;
    std::int64_t ts_ms;
};

using Sink = std::function<void(const Record&)>;
/** sink を足す。戻り値の id で外せる。sink の中で log を呼ばないこと。 */
int add_sink(Sink sink);
void remove_sink(int id);

void write(Level level, std::string_view message);
inline void debug(std::string_view m) { write(Level::debug, m); }
inline void info(std::string_view m) { write(Level::info, m); }
inline void warn(std::string_view m) { write(Level::warn, m); }
inline void error(std::string_view m) { write(Level::error, m); }

/** 標準出力への複写を止める (テスト用)。 */
void set_console_enabled(bool enabled);

} // namespace canalis::log

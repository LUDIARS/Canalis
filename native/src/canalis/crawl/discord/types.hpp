#pragma once
// Discord の取得の型と API の抽象 (src/crawl/discord/types.ts の移植)。メッセージ・チャンネル・スレッドは TS と同じく JSON のまま持つ。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/base/json.hpp"

namespace canalis::crawl::discord {

struct DiscordCrawlOptions {
    /** 取得する最大メッセージ数。既定 100。 */
    std::optional<int> max_messages;
    /** このメッセージ ID より古いものを取る。 */
    std::optional<std::string> before;
    /** このメッセージ ID より新しいものを取る。 */
    std::optional<std::string> after;
    /** true = 古い順 (after カーソル)。既定 false (新しい順 / before カーソル)。 */
    std::optional<bool> oldest_first;
};

struct MessagesQuery {
    int limit = 100;
    std::optional<std::string> before;
    std::optional<std::string> after;
};

struct ArchivedThreads {
    std::vector<Json> threads;
    bool has_more = false;
};

/** Discord API の抽象 (テストで差し替える)。失敗は例外。 */
class DiscordApi {
public:
    virtual ~DiscordApi() = default;
    virtual std::vector<Json> get_messages(const std::string& channel_id, const MessagesQuery& query) = 0;
    virtual std::vector<Json> get_guild_channels(const std::string& guild_id) = 0;
    virtual std::vector<Json> get_active_threads(const std::string& guild_id) = 0;
    virtual ArchivedThreads get_archived_threads(const std::string& channel_id, const std::optional<std::string>& before) = 0;
};


} // namespace canalis::crawl::discord

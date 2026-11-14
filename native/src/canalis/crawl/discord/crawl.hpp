#pragma once
// チャンネルのページ送りとギルド全体のクロール (src/crawl/discord/{crawl,guild}.ts の移植)。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/crawl/discord/types.hpp"

namespace canalis::crawl::discord {

struct ChannelCrawlResult {
    std::vector<Json> messages;
    /** max_messages に達して打ち切ったか。 */
    bool truncated = false;
};

/** チャンネルのメッセージを options に従ってページ送りで取る (src/crawl/discord/crawl.ts)。 */
ChannelCrawlResult crawl_channel(DiscordApi& api, const std::string& channel_id, const DiscordCrawlOptions& options = {});

/** ギルドクロールの 1 チャンネル / スレッド単位の中間結果。kind は 'text-channel' | 'announcement' | 'forum-thread'。 */
struct ChannelCrawlRecord {
    std::string id;
    std::string name;
    std::string kind;
    std::optional<std::string> forum_channel_name;
    std::vector<Json> messages;
};

struct GuildCrawlOptions {
    /** 既定 {0, 5, 15} (GUILD_TEXT / GUILD_ANNOUNCEMENT / GUILD_FORUM)。 */
    std::optional<std::vector<int>> channel_types;
    std::vector<std::string> exclude_channel_ids;
    DiscordCrawlOptions message_options;
};

/** ギルドの全チャンネルとフォーラムのスレッドを取る (src/crawl/discord/guild.ts)。 */
std::vector<ChannelCrawlRecord> crawl_guild(DiscordApi& api, const std::string& guild_id, const GuildCrawlOptions& options = {});


} // namespace canalis::crawl::discord

#include "canalis/crawl/discord/crawl.hpp"

#include <algorithm>
#include <unordered_set>

namespace canalis::crawl::discord {
namespace {

constexpr int kPageSize = 100;  // Discord API の 1 リクエストの上限
// ページ送りの安全弁 (カーソルが進まない応答でも止まる)。
constexpr int kMaxPages = 100'000;
const std::vector<int> kDefaultChannelTypes = {0, 5, 15};

std::vector<Json> collect_forum_threads(DiscordApi& api, const std::string& guild_id, const std::string& forum_id) {
    std::unordered_set<std::string> seen;
    std::vector<Json> threads;
    for (auto& t : api.get_active_threads(guild_id)) {
        const std::string id = json_string(t, "id");
        if (json_string(t, "parent_id") == forum_id && seen.insert(id).second) threads.push_back(std::move(t));
    }
    std::optional<std::string> before;
    for (int page = 0; page < kMaxPages; ++page) {
        auto archived = api.get_archived_threads(forum_id, before);
        for (const auto& archived_thread : archived.threads)
            if (seen.insert(json_string(archived_thread, "id")).second) threads.push_back(archived_thread);
        if (!archived.has_more || archived.threads.empty()) break;
        before = json_opt_string(json_object(archived.threads.back(), "thread_metadata"), "archive_timestamp");
        if (!before || before->empty()) break;
    }
    return threads;
}


} // namespace

ChannelCrawlResult crawl_channel(DiscordApi& api, const std::string& channel_id, const DiscordCrawlOptions& options) {
    const int max_messages = options.max_messages.value_or(100);
    const bool oldest_first = options.oldest_first.value_or(false);
    ChannelCrawlResult result;
    std::optional<std::string> cursor = oldest_first ? options.after : options.before;
    for (int page = 0; page < kMaxPages && static_cast<int>(result.messages.size()) < max_messages; ++page) {
        MessagesQuery q;
        q.limit = std::min(kPageSize, max_messages - static_cast<int>(result.messages.size()));
        if (cursor) (oldest_first ? q.after : q.before) = cursor;
        auto batch = api.get_messages(channel_id, q);
        if (batch.empty()) break;
        const std::size_t batch_size = batch.size();
        cursor = json_opt_string(batch.back(), "id");  // 次ページのカーソル (どちらの向きも末尾)
        result.messages.insert(result.messages.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
        if (static_cast<int>(result.messages.size()) >= max_messages) {
            result.truncated = true;
            break;
        }
        if (static_cast<int>(batch_size) < kPageSize) break;  // 100 件未満なら末尾
    }
    return result;
}

std::vector<ChannelCrawlRecord> crawl_guild(DiscordApi& api, const std::string& guild_id, const GuildCrawlOptions& options) {
    const std::vector<int>& types = options.channel_types ? *options.channel_types : kDefaultChannelTypes;
    const std::unordered_set<std::string> exclude(options.exclude_channel_ids.begin(), options.exclude_channel_ids.end());
    std::vector<ChannelCrawlRecord> records;
    for (const auto& ch : api.get_guild_channels(guild_id)) {
        const int type = static_cast<int>(json_int(ch, "type", -1));
        const std::string id = json_string(ch, "id");
        if (std::find(types.begin(), types.end(), type) == types.end() || exclude.count(id)) continue;
        if (type == 15) {
            // GUILD_FORUM: スレッドを全取得して、それぞれクロールする
            for (const auto& thread : collect_forum_threads(api, guild_id, id)) {
                const std::string thread_id = json_string(thread, "id");
                if (exclude.count(thread_id)) continue;
                auto r = crawl_channel(api, thread_id, options.message_options);
                records.push_back({thread_id, json_string(thread, "name"), "forum-thread", json_string(ch, "name"), std::move(r.messages)});
            }
        } else {
            auto r = crawl_channel(api, id, options.message_options);
            records.push_back({id, json_string(ch, "name"), type == 5 ? "announcement" : "text-channel", std::nullopt, std::move(r.messages)});
        }
    }
    return records;
}

} // namespace canalis::crawl::discord

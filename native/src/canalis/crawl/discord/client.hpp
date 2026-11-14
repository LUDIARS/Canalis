#pragma once
// Discord REST API v10 のクライアント (src/crawl/discord/client.ts の移植)。Bot token は Authorization ヘッダだけに入れ、ログ・エラー文に出さない。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <memory>

#include "canalis/crawl/discord/types.hpp"
#include "canalis/crawl/fetch.hpp"

namespace canalis::crawl::discord {

class DiscordApiClient final : public DiscordApi {
public:
    /** token が空なら std::invalid_argument。 */
    DiscordApiClient(std::string token, FetchFn fetch = {}, SleepFn sleep = {});
    std::vector<Json> get_messages(const std::string& channel_id, const MessagesQuery& query) override;
    std::vector<Json> get_guild_channels(const std::string& guild_id) override;
    std::vector<Json> get_active_threads(const std::string& guild_id) override;
    ArchivedThreads get_archived_threads(const std::string& channel_id, const std::optional<std::string>& before) override;

private:
    Json request(const std::string& url);
    std::string token_;
    FetchFn fetch_;
    SleepFn sleep_;
};

/** config.token → env DISCORD_BOT_TOKEN の順で client を作る。どちらも無ければ std::runtime_error。 */
std::unique_ptr<DiscordApi> create_discord_client(const std::optional<std::string>& token);


} // namespace canalis::crawl::discord

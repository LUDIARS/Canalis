#pragma once
// Discord の ①Crawl adapter (src/crawl/discord/{source,guild-source}.ts の移植)。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <functional>

#include "canalis/core/pipeline.hpp"
#include "canalis/crawl/discord/types.hpp"

namespace canalis::crawl::discord {

struct DiscordSourceConfig {
    std::string channel_id;
    std::optional<std::string> token;
    /** メッセージ URL に使う (省略時は @me)。 */
    std::optional<std::string> guild_id;
    DiscordCrawlOptions options;
};

struct DiscordGuildSourceConfig {
    std::string guild_id;
    std::optional<std::string> token;
    std::optional<std::vector<int>> channel_types;
    std::vector<std::string> exclude_channel_ids;
    DiscordCrawlOptions options;
};

struct DiscordSourceDeps {
    /** 注入する DiscordApi (呼び出し側の所有)。null なら token から作る。 */
    DiscordApi* api = nullptr;
    std::function<std::string()> now;
};

/** チャンネルのメッセージを 1 件 1 RawRecord にする ①Crawl adapter。 */
class DiscordSource final : public core::Source<DiscordSourceConfig> {
public:
    explicit DiscordSource(DiscordSourceDeps deps = {});
    std::string_view name() const override { return "discord"; }
    std::vector<core::RawRecord> crawl(const DiscordSourceConfig& config) override;

private:
    DiscordSourceDeps deps_;
};

/** ギルドの 1 チャンネル / フォーラム投稿を 1 RawRecord (会話をタイムラインのテキストに) にする ①Crawl adapter。 */
class DiscordGuildSource final : public core::Source<DiscordGuildSourceConfig> {
public:
    explicit DiscordGuildSource(DiscordSourceDeps deps = {});
    std::string_view name() const override { return "discord-guild"; }
    std::vector<core::RawRecord> crawl(const DiscordGuildSourceConfig& config) override;

private:
    DiscordSourceDeps deps_;
};

/** メッセージの列を「[YYYY-MM-DD HH:MM UTC] 名前: 内容」の行にする (timestamp の昇順)。 */
std::string messages_to_text(const std::vector<Json>& messages);


} // namespace canalis::crawl::discord

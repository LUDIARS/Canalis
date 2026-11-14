#include "canalis/crawl/discord/source.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"
#include "canalis/crawl/discord/client.hpp"
#include "canalis/crawl/discord/crawl.hpp"

namespace canalis::crawl::discord {
namespace {

std::string author_name(const Json& message) {
    const Json& author = json_object(message, "author");
    if (auto g = json_opt_string(author, "global_name")) return *g;
    return json_string(author, "username");
}

std::size_t array_size(const Json& obj, const char* key) {
    const auto it = obj.find(key);
    return it != obj.end() && it->is_array() ? it->size() : 0;
}

} // namespace

std::string messages_to_text(const std::vector<Json>& messages) {
    std::vector<const Json*> sorted;
    for (const auto& m : messages) sorted.push_back(&m);
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Json* a, const Json* b) { return json_string(*a, "timestamp") < json_string(*b, "timestamp"); });
    std::vector<std::string> lines;
    for (const Json* m : sorted) {
        std::string ts = str::js_slice(json_string(*m, "timestamp"), 0, 16);
        if (const auto t = ts.find('T'); t != std::string::npos) ts[t] = ' ';
        lines.push_back("[" + ts + " UTC] " + author_name(*m) + ": " + json_string(*m, "content"));
    }
    return str::join(lines, "\n");
}

DiscordSource::DiscordSource(DiscordSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

std::vector<core::RawRecord> DiscordSource::crawl(const DiscordSourceConfig& config) {
    if (config.channel_id.empty()) throw std::runtime_error("DiscordSource: channelId is required");
    std::unique_ptr<DiscordApi> owned;
    DiscordApi* api = deps_.api;
    if (!api) {
        owned = create_discord_client(config.token);
        api = owned.get();
    }
    const auto result = crawl_channel(*api, config.channel_id, config.options);
    const std::string fetched_at = deps_.now();
    const std::string guild = config.guild_id.value_or("@me");
    std::vector<core::RawRecord> out;
    for (const auto& msg : result.messages) {
        core::RawRecord r;
        r.source = "discord";
        r.source_id = json_string(msg, "id");
        r.fetched_at = fetched_at;
        r.url = "https://discord.com/channels/" + guild + "/" + json_string(msg, "channel_id") + "/" + r.source_id;
        const std::string content = json_string(msg, "content");
        if (!content.empty()) r.text = content;
        r.raw = msg;
        Json meta = Json::object();
        meta["channelId"] = config.channel_id;
        meta["authorId"] = json_string(json_object(msg, "author"), "id");
        meta["authorName"] = author_name(msg);
        meta["timestamp"] = json_string(msg, "timestamp");
        if (auto edited = json_opt_string(msg, "edited_timestamp")) meta["editedTimestamp"] = *edited;
        meta["hasAttachments"] = array_size(msg, "attachments") > 0;
        meta["hasEmbeds"] = array_size(msg, "embeds") > 0;
        const auto ref = msg.find("referenced_message");
        meta["isReply"] = ref != msg.end() && !ref->is_null();
        meta["truncated"] = result.truncated;
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

DiscordGuildSource::DiscordGuildSource(DiscordSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

std::vector<core::RawRecord> DiscordGuildSource::crawl(const DiscordGuildSourceConfig& config) {
    if (config.guild_id.empty()) throw std::runtime_error("DiscordGuildSource: guildId is required");
    std::unique_ptr<DiscordApi> owned;
    DiscordApi* api = deps_.api;
    if (!api) {
        owned = create_discord_client(config.token);
        api = owned.get();
    }
    GuildCrawlOptions options;
    options.channel_types = config.channel_types;
    options.exclude_channel_ids = config.exclude_channel_ids;
    options.message_options = config.options;
    const auto records = crawl_guild(*api, config.guild_id, options);
    const std::string fetched_at = deps_.now();
    std::vector<core::RawRecord> out;
    for (const auto& rec : records) {
        Json messages = Json::array();
        for (const auto& m : rec.messages) messages.push_back(m);
        core::RawRecord r;
        r.source = "discord-guild";
        r.source_id = rec.id;
        r.fetched_at = fetched_at;
        r.url = "https://discord.com/channels/" + config.guild_id + "/" + rec.id;
        r.title = rec.forum_channel_name && !rec.forum_channel_name->empty() ? *rec.forum_channel_name + " / " + rec.name : rec.name;
        r.text = messages_to_text(rec.messages);
        Json raw = Json::object();
        raw["id"] = rec.id;
        raw["name"] = rec.name;
        raw["kind"] = rec.kind;
        if (rec.forum_channel_name) raw["forumChannelName"] = *rec.forum_channel_name;
        raw["messages"] = messages;
        r.raw = std::move(raw);
        Json meta = Json::object();
        meta["guildId"] = config.guild_id;
        meta["channelId"] = rec.id;
        meta["channelName"] = rec.name;
        meta["kind"] = rec.kind;
        if (rec.forum_channel_name) meta["forumChannelName"] = *rec.forum_channel_name;
        meta["messageCount"] = rec.messages.size();
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

} // namespace canalis::crawl::discord

#include "canalis/crawl/steam/source.hpp"

#include <stdexcept>

#include "canalis/base/time.hpp"

namespace canalis::crawl::steam {
namespace {

std::function<std::string()> or_now(std::function<std::string()> now) {
    if (now) return now;
    return [] { return time::now_iso(); };
}

/** src[from] があれば dst[to] へ写す (TS の `x: r.y` は undefined なら JSON に出ない)。 */
void copy_if_present(Json& dst, const char* to, const Json& src, const char* from) {
    if (!src.is_object()) return;
    if (const auto it = src.find(from); it != src.end()) dst[to] = *it;
}

} // namespace

SteamNewReleasesSource::SteamNewReleasesSource(SteamSourceDeps deps) : deps_{or_now(std::move(deps.now))} {}

std::vector<core::RawRecord> SteamNewReleasesSource::crawl(const FetchNewReleasesOptions& config) {
    const auto entries = fetch_new_releases(config);
    const std::string fetched_at = deps_.now();
    std::vector<core::RawRecord> out;
    for (const auto& e : entries) {
        core::RawRecord r;
        r.source = "steam-new-releases";
        r.source_id = "steam-app:" + std::to_string(e.app_id);
        r.fetched_at = fetched_at;
        r.url = e.url;
        r.title = e.title;
        r.raw = to_json(e);
        Json meta = Json::object();
        meta["appId"] = e.app_id;
        if (e.release_text) meta["releaseText"] = *e.release_text;
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

SteamAppReviewsSource::SteamAppReviewsSource(SteamSourceDeps deps) : deps_{or_now(std::move(deps.now))} {}

std::vector<core::RawRecord> SteamAppReviewsSource::crawl(const FetchAppReviewsOptions& config) {
    if (config.app_id == 0) throw std::runtime_error("SteamAppReviewsSource: appId is required");
    const auto reviews = fetch_app_reviews(config);
    const std::string fetched_at = deps_.now();
    std::vector<core::RawRecord> out;
    for (const auto& review : reviews) {
        const Json& author = json_object(review, "author");
        const auto steam_id = json_opt_string(author, "steamid");
        core::RawRecord r;
        r.source = "steam-app-reviews";
        r.source_id = "steam-review:" + json_string(review, "recommendationid");
        r.fetched_at = fetched_at;
        if (steam_id && !steam_id->empty())
            r.url = "https://steamcommunity.com/profiles/" + *steam_id + "/recommended/" + std::to_string(config.app_id) + "/";
        const std::string text = json_string(review, "review");
        if (!text.empty()) r.text = text;
        r.raw = review;
        Json meta = Json::object();
        meta["appId"] = config.app_id;
        // SteamID64 = 公開・安定な同一性のアンカー (同じ人の横断の同定に使う)
        copy_if_present(meta, "steamId", author, "steamid");
        copy_if_present(meta, "votedUp", review, "voted_up");
        copy_if_present(meta, "votesUp", review, "votes_up");
        copy_if_present(meta, "language", review, "language");
        copy_if_present(meta, "timestampCreated", review, "timestamp_created");
        copy_if_present(meta, "playtimeForever", author, "playtime_forever");
        copy_if_present(meta, "numGamesOwned", author, "num_games_owned");
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

SteamUserReviewsSource::SteamUserReviewsSource(SteamSourceDeps deps) : deps_{or_now(std::move(deps.now))} {}

std::vector<core::RawRecord> SteamUserReviewsSource::crawl(const FetchUserReviewsOptions& config) {
    if (config.steam_id.empty()) throw std::runtime_error("SteamUserReviewsSource: steamId is required");
    const auto entries = fetch_user_reviews(config);
    const std::string fetched_at = deps_.now();
    std::vector<core::RawRecord> out;
    for (const auto& e : entries) {
        core::RawRecord r;
        r.source = "steam-user-reviews";
        r.source_id = "steam-user-review:" + e.steam_id + ":" + std::to_string(e.app_id);
        r.fetched_at = fetched_at;
        r.url = e.url;
        if (!e.text.empty()) r.text = e.text;
        r.raw = to_json(e);
        Json meta = Json::object();
        meta["appId"] = e.app_id;
        meta["steamId"] = e.steam_id;
        if (e.recommended) meta["recommended"] = *e.recommended;
        if (e.hours_text) meta["hoursText"] = *e.hours_text;
        if (e.posted_text) meta["postedText"] = *e.posted_text;
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

} // namespace canalis::crawl::steam

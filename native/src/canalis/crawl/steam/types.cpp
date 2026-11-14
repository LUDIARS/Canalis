#include "canalis/crawl/steam/types.hpp"

namespace canalis::crawl::steam {

Json to_json(const SteamSearchEntry& e) {
    Json out = Json::object();
    out["appId"] = e.app_id;
    out["title"] = e.title;
    out["url"] = e.url;
    if (e.release_text) out["releaseText"] = *e.release_text;
    return out;
}

Json to_json(const SteamUserReviewEntry& e) {
    Json out = Json::object();
    out["steamId"] = e.steam_id;
    out["appId"] = e.app_id;
    if (e.recommended) out["recommended"] = *e.recommended;
    out["text"] = e.text;
    if (e.hours_text) out["hoursText"] = *e.hours_text;
    if (e.posted_text) out["postedText"] = *e.posted_text;
    out["url"] = e.url;
    return out;
}

} // namespace canalis::crawl::steam

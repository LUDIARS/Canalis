#include "canalis/config/store.hpp"

#include <cmath>
#include <stdexcept>

#include "canalis/base/env.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/config/crypto.hpp"

namespace canalis::config {
namespace {

// config.json は手で書く小さなファイル。これを超えるものは壊れているとみなして読まない。
constexpr std::size_t kMaxConfigBytes = 1u << 20;

std::string master_secret() {
    if (auto key = env::get(std::string_view("CANALIS_MASTER_KEY")); key && !key->empty()) return *key;
    return "canalis:" + env::hostname() + ":" + env::username();
}

/** cfg.notion を object として返す (無い・object でなければ作り直す)。TS の `{ ...cfg.notion, ... }` に当たる。 */
Json& notion_of(Json& cfg) {
    auto it = cfg.find("notion");
    if (it == cfg.end() || !it->is_object()) cfg["notion"] = Json::object();
    return cfg["notion"];
}

} // namespace

std::string config_path() {
    if (auto p = env::get("CANALIS_CONFIG_PATH"); p && !p->empty()) return *p;
    return fs::join(env::cwd(), "config.json");
}

Json read_config() {
    const std::string path = config_path();
    if (!fs::exists(path)) return Json::object();
    const auto text = fs::read_file(path, kMaxConfigBytes);
    if (!text) return Json::object();
    JsonLimits limits;
    limits.max_bytes = kMaxConfigBytes;
    auto parsed = parse_json(*text, limits);
    if (!parsed || !parsed->is_object()) return Json::object();
    return *parsed;
}

void write_config(const Json& cfg) {
    const std::string path = config_path();
    if (!fs::ensure_dir(fs::dirname(path)) || !fs::write_file_atomic(path, json_stringify_pretty(cfg, 2) + "\n"))
        throw std::runtime_error("failed to write config: " + path);
}

void set_notion_token(const std::string& token) {
    if (token.empty()) throw std::invalid_argument("token is empty");
    Json cfg = read_config();
    notion_of(cfg)["tokenEnc"] = to_json(encrypt_json(Json(token), master_secret()));
    write_config(cfg);
}

void clear_notion_token() {
    Json cfg = read_config();
    if (auto it = cfg.find("notion"); it != cfg.end() && it->is_object()) it->erase("tokenEnc");
    write_config(cfg);
}

void set_notion_version(const std::string& version) {
    Json cfg = read_config();
    notion_of(cfg)["version"] = version;
    write_config(cfg);
}

void set_notion_min_interval(double ms) {
    if (!std::isfinite(ms) || ms < 0) throw std::invalid_argument("minIntervalMs must be a non-negative number");
    Json cfg = read_config();
    notion_of(cfg)["minIntervalMs"] = ms;
    write_config(cfg);
}

std::optional<std::string> get_stored_token() {
    const Json cfg = read_config();
    const Json& notion = json_object(cfg, "notion");
    const auto it = notion.find("tokenEnc");
    if (it == notion.end()) return std::nullopt;
    const auto blob = blob_from_json(*it);
    if (!blob) return std::nullopt;
    try {
        const Json value = decrypt_json(*blob, master_secret());
        if (!value.is_string()) return std::nullopt;
        return value.get<std::string>();
    } catch (const std::exception&) {
        return std::nullopt;  // master 鍵が変わった等
    }
}

std::optional<std::string> resolve_notion_token() {
    // TS は `process.env.NOTION_TOKEN ?? getStoredToken()`。空文字でも設定されていれば env を使う。
    if (auto token = env::get("NOTION_TOKEN")) return token;
    return get_stored_token();
}

NotionSettings notion_settings() {
    const Json cfg = read_config();
    const Json& notion = json_object(cfg, "notion");
    NotionSettings out;
    if (auto v = env::get_nonempty("NOTION_VERSION")) out.version = *v;
    else if (auto c = json_opt_string(notion, "version"); c && !c->empty()) out.version = *c;
    if (auto v = env::get_nonempty("NOTION_MIN_INTERVAL_MS")) out.min_interval_ms = str::js_number(*v);
    else out.min_interval_ms = json_opt_number(notion, "minIntervalMs");
    return out;
}

ConfigStatus get_config_status() {
    const Json cfg = read_config();
    const Json& notion = json_object(cfg, "notion");
    const auto env_token = env::get("NOTION_TOKEN");
    const auto stored = get_stored_token();
    const std::optional<std::string> effective = env_token ? env_token : stored;
    ConfigStatus s;
    s.path = config_path();
    // TS は truthy で判定する (空文字の env / token は「無い」扱い)。
    s.token_source = (env_token && !env_token->empty()) ? "env" : (stored && !stored->empty()) ? "config" : "none";
    if (effective && !effective->empty()) s.token_hint = "\xE2\x80\xA6" + str::js_slice(*effective, -4);
    s.version = json_opt_string(notion, "version");
    s.min_interval_ms = json_opt_number(notion, "minIntervalMs");
    return s;
}

Json to_json(const ConfigStatus& status) {
    Json out = Json::object();
    out["path"] = status.path;
    out["tokenSource"] = status.token_source;
    out["tokenHint"] = status.token_hint ? Json(*status.token_hint) : Json(nullptr);
    out["version"] = status.version ? Json(*status.version) : Json(nullptr);
    out["minIntervalMs"] = status.min_interval_ms ? Json(*status.min_interval_ms) : Json(nullptr);
    return out;
}

} // namespace canalis::config

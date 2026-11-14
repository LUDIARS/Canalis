#include "cli/commands.hpp"

#include <cmath>
#include <stdexcept>

#include "canalis/base/fsutil.hpp"
#include "canalis/base/json.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/config/store.hpp"
#include "canalis/crawl/notion/crawl.hpp"
#include "canalis/crawl/notion_public/source.hpp"
#include "canalis/crawl/website/source.hpp"
#include "canalis/save/raw_writer.hpp"
#include "cli/adapters.hpp"

namespace canalis::cli {
namespace {

constexpr std::string_view kCommands = "commands: notion-crawl, config, crawl, notion-public-crawl, website-parse\n";
constexpr std::string_view kConfigUsage = "usage:\n"
                                          "  canalis config set notion.token [value]\n"
                                          "  canalis config set notion.version <value>\n"
                                          "  canalis config set notion.minIntervalMs <ms>\n"
                                          "  canalis config unset notion.token\n"
                                          "  canalis config show\n"
                                          "  canalis config path\n";
constexpr std::size_t kMaxStdinBytes = 64u << 20;
constexpr std::size_t kMaxTokenBytes = 64u << 10;
constexpr std::size_t kMaxConfigJsonBytes = 1u << 20;

/** "--name value" の value を整数で読む。無い・整数でなければ nullopt。 */
std::optional<std::int64_t> int_arg(const std::vector<std::string>& args, std::size_t& i) {
    if (i + 1 >= args.size()) return std::nullopt;
    const auto n = str::js_number(args[++i]);
    if (!n || !std::isfinite(*n) || *n != std::floor(*n)) return std::nullopt;
    return static_cast<std::int64_t>(*n);
}

std::optional<int> small_int(std::optional<std::int64_t> v) {
    if (!v || *v < 0 || *v > 1'000'000) return std::nullopt;
    return static_cast<int>(*v);
}

int usage_error(const Io& io, std::string_view message, std::string_view usage) {
    io.err("error: " + std::string(message) + "\n\n" + std::string(usage));
    return 2;
}

/** --config の値: "@path" ならファイル、そうでなければ JSON の文字列そのもの。 */
Json read_config_arg(const std::string& value) {
    std::string text = value;
    if (str::starts_with(value, "@")) {
        auto file = fs::read_file(value.substr(1), kMaxConfigJsonBytes);
        if (!file) throw std::runtime_error("cannot read config file: " + value.substr(1));
        text = *file;
    }
    JsonLimits limits;
    limits.max_bytes = kMaxConfigJsonBytes;
    std::string error;
    auto json = parse_json(text, limits, &error);
    if (!json || !json->is_object()) throw std::runtime_error("--config must be a JSON object: " + error);
    return *json;
}


void write_records(const Io& io, const std::vector<core::RawRecord>& records) {
    for (const auto& r : records) io.out(json_stringify(core::to_json(r)) + "\n");
}

// ─── notion-crawl ──────────────────────────────────────────────

constexpr std::string_view kNotionCrawlUsage =
    "usage: canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]\n";

} // namespace

int notion_crawl(const std::vector<std::string>& args, const Io& io) {
    std::optional<std::string> database_id;
    std::string out_dir = "data/raw";
    crawl::notion::CrawlOptions options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--out") {
            if (i + 1 < args.size()) out_dir = args[++i];
        } else if (a == "--max-depth" || a == "--max-pages") {
            const auto n = small_int(int_arg(args, i));
            if (!n) {
                io.err(std::string(kNotionCrawlUsage));
                return 2;
            }
            (a == "--max-depth" ? options.max_depth : options.max_pages) = *n;
        } else if (!str::starts_with(a, "--")) {
            database_id = a;
        }
    }
    if (!database_id || database_id->empty()) {
        io.err(std::string(kNotionCrawlUsage));
        return 2;
    }
    const auto token = config::resolve_notion_token();
    if (!token || token->empty()) {
        io.err("error: Notion token is not set. Run `canalis config set notion.token` or set env NOTION_TOKEN.\n");
        return 2;
    }
    crawl::notion::NotionSource source;
    const auto records = source.crawl({*database_id, *token, options});
    const std::string dir = out_dir + "/notion/" + *database_id;
    save::JsonlRawSink({dir, std::nullopt}).write(records);
    io.out("crawled " + std::to_string(records.size()) + " records \xE2\x86\x92 " + dir + "/\n");
    return 0;
}

// ─── config ───────────────────────────────────────────────────

namespace {

std::string read_token(const Io& io) {
    if (io.stdin_is_tty && io.stdin_is_tty()) {
        return str::trim(io.prompt_hidden("Notion token (入力は表示されません): "));
    }
    // パイプ入力 (例: `echo $TOKEN | canalis config set notion.token`)。全体を 1 つの値として読む。
    const auto data = io.read_stdin ? io.read_stdin(kMaxTokenBytes) : std::nullopt;
    return data ? str::trim(*data) : std::string();
}

int config_set(const std::vector<std::string>& args, const Io& io) {
    const std::optional<std::string> key = args.size() > 0 ? std::optional(args[0]) : std::nullopt;
    const std::optional<std::string> value = args.size() > 1 ? std::optional(args[1]) : std::nullopt;
    if (key == "notion.token") {
        const std::string token = value ? *value : read_token(io);
        if (token.empty()) {
            io.err("error: token is empty\n");
            return 2;
        }
        config::set_notion_token(token);
        io.out("saved notion.token (encrypted) \xE2\x86\x92 " + config::config_path() + "\n");
        return 0;
    }
    if (key == "notion.version") {
        if (!value || value->empty()) return usage_error(io, "notion.version requires a value", kConfigUsage);
        config::set_notion_version(*value);
        io.out("saved notion.version=" + *value + "\n");
        return 0;
    }
    if (key == "notion.minIntervalMs") {
        if (!value || value->empty()) return usage_error(io, "notion.minIntervalMs requires a value", kConfigUsage);
        const auto ms = str::js_number(*value);
        if (!ms || !std::isfinite(*ms) || *ms < 0)
            return usage_error(io, "minIntervalMs must be a non-negative number", kConfigUsage);
        config::set_notion_min_interval(*ms);
        io.out("saved notion.minIntervalMs=" + js_number_to_string(*ms) + "\n");
        return 0;
    }
    return usage_error(io, "unknown key: " + key.value_or("(none)"), kConfigUsage);
}

/** `config show` の 1 行。ラベルを 14 文字に揃えて " : " で値とつなぐ (TS の表示と同じ並び)。 */
std::string show_row(std::string_view label, const std::string& value) {
    std::string out(label);
    if (out.size() < 14) out.append(14 - out.size(), ' ');
    return out + " : " + value + "\n";
}

int config_show(const Io& io) {
    const auto s = config::get_config_status();
    const std::string masked = s.token_hint.value_or("(unset)") + " [source: " + s.token_source + "]";
    const std::string interval = s.min_interval_ms ? js_number_to_string(*s.min_interval_ms) : std::string("(default 350)");
    io.out(show_row("path", s.path) + show_row("notion.token", masked) +
           show_row("notion.version", s.version.value_or("(default 2022-06-28)")) + show_row("notion.minIntervalMs", interval));
    return 0;
}

} // namespace

int config_command(const std::vector<std::string>& args, const Io& io) {
    const std::string sub = args.empty() ? "" : args[0];
    const std::vector<std::string> rest(args.begin() + (args.empty() ? 0 : 1), args.end());
    if (sub == "set") return config_set(rest, io);
    if (sub == "unset") {
        if (!rest.empty() && rest[0] == "notion.token") {
            config::clear_notion_token();
            io.out("removed notion.token\n");
            return 0;
        }
        return usage_error(io, "unknown key: " + (rest.empty() ? std::string("(none)") : rest[0]), kConfigUsage);
    }
    if (sub == "show") return config_show(io);
    if (sub == "path") {
        io.out(config::config_path() + "\n");
        return 0;
    }
    return usage_error(io, "unknown subcommand: " + (sub.empty() ? std::string("(none)") : sub), kConfigUsage);
}

// ─── crawl <adapter> ──────────────────────────────────────────

namespace {

std::string crawl_usage() {
    std::vector<std::string> names;
    for (const auto n : adapter_names()) names.emplace_back(n);
    return "usage: canalis crawl <adapter> --config <JSON | @file>\n  adapter: " + str::join(names, " | ") + "\n";
}

} // namespace

int crawl_command(const std::vector<std::string>& args, const Io& io) {
    std::optional<std::string> adapter;
    std::optional<std::string> config_value;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--config" && i + 1 < args.size()) config_value = args[++i];
        else if (!str::starts_with(args[i], "--") && !adapter) adapter = args[i];
    }
    if (!adapter || !config_value) {
        io.err(crawl_usage());
        return 2;
    }
    const Json config = read_config_arg(*config_value);
    const auto records = crawl_adapter(*adapter, config);
    if (!records) {
        io.err("error: unknown adapter: " + *adapter + "\n" + crawl_usage());
        return 2;
    }
    write_records(io, *records);
    return 0;
}

// ─── notion-public-crawl ───────────────────────────────────────

int notion_public_crawl(const std::vector<std::string>& args, const Io& io) {
    constexpr std::string_view usage =
        "usage: canalis notion-public-crawl <url> [--max-depth N] [--max-pages N] [--timeout ms] [--scroll-delay ms] "
        "[--max-scrolls N]\n";
    std::optional<std::string> url;
    crawl::notion_public::NotionPublicPageCrawlOptions options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--max-depth" || a == "--max-pages" || a == "--max-scrolls" || a == "--timeout" || a == "--scroll-delay") {
            const auto n = int_arg(args, i);
            const auto small = small_int(n);
            if (!small || (a == "--timeout" && *n <= 0)) {
                io.err(std::string(usage));
                return 2;
            }
            if (a == "--max-depth") options.max_depth = *small;
            else if (a == "--max-pages") options.max_pages = *small;
            else if (a == "--max-scrolls") options.page.max_scrolls = *small;
            else if (a == "--timeout") options.page.timeout = *n;
            else options.page.scroll_delay = *n;
        } else if (!str::starts_with(a, "--")) {
            url = a;
        }
    }
    if (!url || url->empty()) {
        io.err(std::string(usage));
        return 2;
    }
    const auto result = crawl::notion_public::crawl_public_pages(*url, options);
    io.out(json_stringify(crawl::notion_public::to_json(result)) + "\n");
    return 0;
}

// ─── website-parse ─────────────────────────────────────────────

int website_parse(const std::vector<std::string>& args, const Io& io) {
    constexpr std::string_view usage = "usage: canalis website-parse --url <url>  (HTML は標準入力)\n";
    std::optional<std::string> url;
    for (std::size_t i = 0; i < args.size(); ++i)
        if (args[i] == "--url" && i + 1 < args.size()) url = args[++i];
    if (!url || url->empty()) {
        io.err(std::string(usage));
        return 2;
    }
    const auto html = io.read_stdin ? io.read_stdin(kMaxStdinBytes) : std::nullopt;
    if (!html) throw std::runtime_error("failed to read HTML from stdin (limit 64 MiB)");
    const auto page = crawl::website::fallback_parse(*html, *url);
    Json out = Json::object();
    if (page.title) out["title"] = *page.title;
    out["text"] = page.text;
    Json links = Json::array();
    for (const auto& l : page.links) links.push_back(l);
    out["links"] = std::move(links);
    io.out(json_stringify(out) + "\n");
    return 0;
}

int run(const std::vector<std::string>& args, const Io& io) {
    const std::string command = args.empty() ? "" : args[0];
    const std::vector<std::string> rest(args.begin() + (args.empty() ? 0 : 1), args.end());
    if (command == "notion-crawl") return notion_crawl(rest, io);
    if (command == "config") return config_command(rest, io);
    if (command == "crawl") return crawl_command(rest, io);
    if (command == "notion-public-crawl") return notion_public_crawl(rest, io);
    if (command == "website-parse") return website_parse(rest, io);
    io.err(std::string(kCommands));
    return 2;
}

} // namespace canalis::cli

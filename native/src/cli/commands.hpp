#pragma once
// Canalis CLI (src/cli.ts と src/config/cli.ts の移植 + C++ 版で足したサブコマンド)。
// 引数・出力・終了コードは spec/interface/cli.md。入出力は Io で受け取り、テストから同じ関数を呼べるようにする。
//
//   canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]
//   canalis config set|unset|show|path ...
//   canalis crawl <adapter> --config <JSON | @file>          (C++ 版で追加: RawRecord を 1 行 1 件の JSON で標準出力へ)
//   canalis notion-public-crawl <url> [--max-depth N] ...    (C++ 版で追加: crawlPublicPages の結果を 1 行の JSON で)
//   canalis website-parse --url <url>                         (C++ 版で追加: 標準入力の HTML を fallback パーサで解析)
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canalis::cli {

struct Io {
    std::function<void(std::string_view)> out;
    std::function<void(std::string_view)> err;
    /** 標準入力を最後まで読む (max_bytes を超えたら nullopt)。 */
    std::function<std::optional<std::string>(std::size_t max_bytes)> read_stdin;
    /** 標準入力が端末か。 */
    std::function<bool()> stdin_is_tty;
    /** 端末で入力を表示せずに 1 行読む (label は標準エラーへ)。 */
    std::function<std::string(std::string_view label)> prompt_hidden;
};

/** argv[0] (プログラム名) を除いた引数で実行し、終了コードを返す。例外は呼び出し側 (main) で exit 1 にする。 */
int run(const std::vector<std::string>& args, const Io& io);

int notion_crawl(const std::vector<std::string>& args, const Io& io);
int config_command(const std::vector<std::string>& args, const Io& io);
int crawl_command(const std::vector<std::string>& args, const Io& io);
int notion_public_crawl(const std::vector<std::string>& args, const Io& io);
int website_parse(const std::vector<std::string>& args, const Io& io);

} // namespace canalis::cli

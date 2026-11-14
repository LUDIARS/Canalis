#pragma once
// Notion block → Markdown と、page の properties からタイトル・簡易 key→string を作る純粋関数
// (src/crawl/notion/{blocks,page}.ts の移植)。
#include <string>
#include <utility>
#include <vector>

#include "canalis/base/json.hpp"

namespace canalis::crawl::notion {

/** rich_text 配列を plain text に連結する (配列でなければ空文字)。 */
std::string rich_text_to_plain(const Json& rich);

/** 1 ブロックを Markdown にする。不明な type は rich_text (無ければ空文字)。children は呼び出し側で処理する。 */
std::string block_to_markdown(const Json& block);

struct IndentedBlock {
    Json block;
    int indent = 0;
};

/** 入れ子の深さ付きのブロック列を Markdown にする (各ブロックの 1 行目だけを字下げする)。 */
std::string blocks_to_markdown(const std::vector<IndentedBlock>& blocks);

/** properties から title 型のプロパティを探して plain text (前後の空白を除く) を返す。無ければ空文字。 */
std::string extract_title(const Json& page);

/** 1 プロパティを表示用の文字列にする。 */
std::string prop_to_string(const Json& value);

/** properties 全体を key → string の object にする (空の値は除く、キーの順は元のまま)。 */
Json simplify_properties(const Json& page);

} // namespace canalis::crawl::notion

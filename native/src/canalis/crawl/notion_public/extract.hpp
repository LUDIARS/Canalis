#pragma once
// ブラウザ内で動かす DOM 抽出の JS と、C++ 側の Markdown 変換 (src/crawl/notion-public/extract.ts の移植)。
#include <string>
#include <string_view>
#include <vector>

#include "canalis/crawl/notion_public/types.hpp"

namespace canalis::crawl::notion_public {

/**
 * ブラウザで評価する式 (extractPageContent を呼んだ結果の object を返す)。
 * extract.ts の extractPageContent をブラウザ用の JS のまま埋め込んだもの (型注釈だけを外した)。
 * extract.ts を変えたらここも同じに直す (TS 版を退役するまで両方を持つ)。
 */
std::string_view extract_page_content_expression();

/** ExtractedBlock の列を Markdown にする。 */
std::string blocks_to_markdown(const std::vector<ExtractedBlock>& blocks);

} // namespace canalis::crawl::notion_public

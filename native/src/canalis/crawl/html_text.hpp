#pragma once
// HTML から文字を取り出す小さな部品 (website と steam が共通で使う)。
// TS 版は正規表現で書いているが、C++ 版は std::regex を使わずに線形に走査して同じ結果にする (PORTING の規則)。
#include <cstddef>
#include <string>
#include <string_view>

namespace canalis::crawl::html {

/** haystack[from..] で needle を ASCII の大文字小文字を無視して探す。無ければ npos。 */
std::size_t ifind(std::string_view haystack, std::string_view needle, std::size_t from = 0);

/** `<[^>]+>` (1 文字以上の中身を持つタグ) を replacement に置き換える。`<>` と閉じの無い `<` は残す。 */
std::string replace_tags(std::string_view s, std::string_view replacement);

/** `<open[\s\S]*?close>` (大文字小文字を無視) を空白 1 つに置き換える。閉じが無ければそこから先は残す。 */
std::string replace_blocks(std::string_view html, std::string_view open, std::string_view close);

/** JS の \s が min_run 個以上続くところを空白 1 つにする (`\s+` なら 1、`\s{2,}` なら 2)。 */
std::string collapse_js_space(std::string_view s, std::size_t min_run);

/** &amp; &lt; &gt; &quot; &#39; をこの順で置き換える (with_nbsp なら最後に &nbsp; → 空白も)。 */
std::string decode_basic_entities(std::string_view s, bool with_nbsp);

/** ASCII の空白 (\t \n \v \f \r 空白) か。 */
bool is_ascii_space(char c);

} // namespace canalis::crawl::html

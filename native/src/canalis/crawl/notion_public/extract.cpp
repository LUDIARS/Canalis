#include "canalis/crawl/notion_public/extract.hpp"

#include "canalis/base/strings.hpp"

namespace canalis::crawl::notion_public {
namespace {

// extract.ts の extractPageContent と同じ処理 (ブラウザの中で動く)。
constexpr std::string_view kExtractExpression = R"JS((function extractPageContent() {
  const title = document.title.replace(/\s*[|–\-]\s*Notion\s*$/i, '').trim() || document.title;

  const content = document.querySelector('.notion-page-content');
  if (!content) {
    const bodyText = document.body.innerText.trim();
    return { title, blocks: bodyText ? [{ type: 'text', text: bodyText }] : [], links: [] };
  }

  const TYPE_MAP = [
    ['notion-header-block', 'h1'],
    ['notion-sub_header-block', 'h2'],
    ['notion-sub_sub_header-block', 'h3'],
    ['notion-bulleted_list-block', 'li'],
    ['notion-numbered_list-block', 'oli'],
    ['notion-quote-block', 'quote'],
    ['notion-code-block', 'code'],
    ['notion-callout-block', 'callout'],
    ['notion-divider-block', 'divider'],
  ];

  const blocks = [];
  const seen = new Set();

  for (const el of Array.from(content.querySelectorAll('[data-block-id]'))) {
    const id = el.getAttribute('data-block-id') ?? '';
    if (seen.has(id)) continue;
    seen.add(id);

    let type = 'text';
    for (const [cls, t] of TYPE_MAP) {
      if (el.querySelector('.' + cls) || el.classList.contains(cls)) {
        type = t;
        break;
      }
    }

    const text = el.innerText?.trim() ?? '';
    if (text || type === 'divider') blocks.push({ type, text });
  }

  if (!blocks.length) {
    const text = content.innerText?.trim() ?? '';
    if (text) blocks.push({ type: 'text', text });
  }

  const links = [];
  const self = location.origin + location.pathname;
  for (const a of Array.from(content.querySelectorAll('a[href]'))) {
    let u;
    try {
      u = new URL(a.href, location.href);
    } catch {
      continue;
    }
    const last = u.pathname.split('/').filter(Boolean).pop() ?? '';
    if (u.origin !== location.origin || !/[0-9a-f]{32}$/i.test(last)) continue;
    const abs = u.origin + u.pathname;
    if (abs !== self && !links.includes(abs)) links.push(abs);
  }

  return { title, blocks, links };
})())JS";

std::string prefix_lines(std::string_view text, std::string_view prefix) {
    const auto lines = str::split(text, "\n");
    std::vector<std::string> out;
    out.reserve(lines.size());
    for (const auto& l : lines) out.push_back(std::string(prefix) + l);
    return str::join(out, "\n");
}

} // namespace

std::string_view extract_page_content_expression() { return kExtractExpression; }

std::string blocks_to_markdown(const std::vector<ExtractedBlock>& blocks) {
    std::vector<std::string> parts;
    for (const auto& b : blocks) {
        std::string s;
        if (b.type == "h1") s = "# " + b.text;
        else if (b.type == "h2") s = "## " + b.text;
        else if (b.type == "h3") s = "### " + b.text;
        else if (b.type == "li") s = "- " + b.text;
        else if (b.type == "oli") s = "1. " + b.text;
        else if (b.type == "quote") s = prefix_lines(b.text, "> ");
        else if (b.type == "code") s = "```\n" + b.text + "\n```";
        else if (b.type == "callout") s = "> \xF0\x9F\x92\xA1 " + b.text;  // 💡
        else if (b.type == "divider") s = "---";
        else s = b.text;
        if (!str::trim(s).empty()) parts.push_back(std::move(s));
    }
    return str::join(parts, "\n\n");
}

} // namespace canalis::crawl::notion_public

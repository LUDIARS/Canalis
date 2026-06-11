// ブラウザ内 DOM 抽出ロジック と Node 側 Markdown 変換。
//
// extractPageContent はコンパイル後の JS が page.evaluate() でブラウザ内実行される。
// 外部 import を参照できないため完全自己完結で書く。

/// <reference lib="dom" />

import type { ExtractionResult, ExtractedBlock } from './types.js';

/**
 * ブラウザコンテキストで実行する DOM 抽出関数。
 * page.evaluate(extractPageContent) に渡すため、クロージャ外変数を参照しない。
 */
export function extractPageContent(): ExtractionResult {
  const title = document.title.replace(/\s*[|–\-]\s*Notion\s*$/i, '').trim() || document.title;

  const content = document.querySelector('.notion-page-content');
  if (!content) {
    const bodyText = (document.body as HTMLElement).innerText.trim();
    return { title, blocks: bodyText ? [{ type: 'text', text: bodyText }] : [] };
  }

  // ブロック種別マップ (className の部分一致で判定)
  const TYPE_MAP: [string, string][] = [
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

  const blocks: ExtractedBlock[] = [];
  const seen = new Set<string>();

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

    const text = (el as HTMLElement).innerText?.trim() ?? '';
    if (text || type === 'divider') blocks.push({ type, text });
  }

  if (!blocks.length) {
    const text = (content as HTMLElement).innerText?.trim() ?? '';
    if (text) blocks.push({ type: 'text', text });
  }

  return { title, blocks };
}

/** ExtractedBlock[] を Markdown 文字列へ変換する (Node 側で実行)。 */
export function blocksToMarkdown(blocks: ExtractedBlock[]): string {
  return blocks
    .map((b) => {
      switch (b.type) {
        case 'h1': return `# ${b.text}`;
        case 'h2': return `## ${b.text}`;
        case 'h3': return `### ${b.text}`;
        case 'li': return `- ${b.text}`;
        case 'oli': return `1. ${b.text}`;
        case 'quote': return b.text.split('\n').map((l) => `> ${l}`).join('\n');
        case 'code': return `\`\`\`\n${b.text}\n\`\`\``;
        case 'callout': return `> 💡 ${b.text}`;
        case 'divider': return '---';
        default: return b.text;
      }
    })
    .filter((s) => s.trim())
    .join('\n\n');
}

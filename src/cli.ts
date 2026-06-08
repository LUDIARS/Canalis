#!/usr/bin/env node
// Canalis CLI。 ②/③ のサービス固有コード無しで動く範囲 = ①Crawl + raw 保存 を提供する。
// (Tirocinium の scripts/notion-crawl を置き換える standalone エントリ)。
//
//   canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]
//
// token は env NOTION_TOKEN。 結果は <out>/notion/<databaseId>/<timestamp>.jsonl へ。

import { NotionSource } from './crawl/notion/index.js';
import { JsonlRawSink } from './save/index.js';

type Args = { databaseId?: string; out: string; maxDepth?: number; maxPages?: number };

function parseArgs(argv: string[]): Args {
  const out: Args = { out: 'data/raw' };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--out') out.out = argv[++i] ?? out.out;
    else if (a === '--max-depth') out.maxDepth = Number(argv[++i]);
    else if (a === '--max-pages') out.maxPages = Number(argv[++i]);
    else if (!a?.startsWith('--')) out.databaseId = a;
  }
  return out;
}

async function notionCrawl(argv: string[]): Promise<number> {
  const args = parseArgs(argv);
  if (!args.databaseId) {
    process.stderr.write('usage: canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]\n');
    return 2;
  }
  if (!process.env['NOTION_TOKEN']) {
    process.stderr.write('error: env NOTION_TOKEN is required\n');
    return 2;
  }

  const source = new NotionSource();
  const records = await source.crawl({
    databaseId: args.databaseId,
    crawl: { maxDepth: args.maxDepth, maxPages: args.maxPages },
  });

  const sink = new JsonlRawSink({ dir: `${args.out}/notion/${args.databaseId}` });
  await sink.write(records);

  process.stdout.write(`crawled ${records.length} records → ${args.out}/notion/${args.databaseId}/\n`);
  return 0;
}

async function main(): Promise<void> {
  const [, , command, ...rest] = process.argv;
  switch (command) {
    case 'notion-crawl':
      process.exitCode = await notionCrawl(rest);
      break;
    default:
      process.stderr.write('commands: notion-crawl\n');
      process.exitCode = 2;
  }
}

main().catch((err) => {
  process.stderr.write(`canalis: ${(err as Error).message}\n`);
  process.exitCode = 1;
});

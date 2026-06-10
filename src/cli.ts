#!/usr/bin/env node
// Canalis CLI。 ②/③ のサービス固有コード無しで動く範囲 = ①Crawl + raw 保存 を提供する。
// (Tirocinium の scripts/notion-crawl を置き換える standalone エントリ)。
//
//   canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]
//   canalis config ...   (設定 UI: token を暗号化保存 / version 等)
//
// token は env NOTION_TOKEN を優先し、 無ければ config.json の暗号化トークン (config set notion.token)。
// 結果は <out>/notion/<databaseId>/<timestamp>.jsonl へ。

import { NotionSource } from './crawl/notion/index.js';
import { JsonlRawSink } from './save/index.js';
import { runConfigCommand, resolveNotionToken, applyNotionConfigToEnv } from './config/index.js';

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
  // config の非シークレット設定 (version / minInterval) を env へ反映してから token を解決。
  applyNotionConfigToEnv();
  const token = resolveNotionToken();
  if (!token) {
    process.stderr.write(
      'error: Notion token is not set. Run `canalis config set notion.token` or set env NOTION_TOKEN.\n',
    );
    return 2;
  }

  const source = new NotionSource();
  const records = await source.crawl({
    databaseId: args.databaseId,
    token,
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
    case 'config':
      process.exitCode = await runConfigCommand(rest);
      break;
    default:
      process.stderr.write('commands: notion-crawl, config\n');
      process.exitCode = 2;
  }
}

main().catch((err) => {
  process.stderr.write(`canalis: ${(err as Error).message}\n`);
  process.exitCode = 1;
});

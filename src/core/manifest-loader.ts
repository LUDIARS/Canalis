// YAML / JSON ファイルから Manifest を読み込む。
// YAML は js-yaml を使用。 JSON は組み込み JSON.parse。

import { readFile } from 'node:fs/promises';
import { load as yamlLoad } from 'js-yaml';
import type { Manifest } from './manifest.js';

/**
 * 指定パスの YAML または JSON ファイルを Manifest として読み込む。
 * 拡張子 .yaml / .yml → YAML パース、それ以外 → JSON パース。
 */
export async function loadManifest(filePath: string): Promise<Manifest> {
  const src = await readFile(filePath, 'utf-8');
  const lower = filePath.toLowerCase();
  const raw = lower.endsWith('.yaml') || lower.endsWith('.yml') ? yamlLoad(src) : JSON.parse(src);
  assertManifest(raw, filePath);
  return raw;
}

function assertManifest(raw: unknown, filePath: string): asserts raw is Manifest {
  if (!raw || typeof raw !== 'object') throw new Error(`manifest-loader: ${filePath} is not an object`);
  const m = raw as Record<string, unknown>;
  if (typeof m['pipeline'] !== 'string') throw new Error(`manifest-loader: ${filePath}: pipeline (string) is required`);
  if (!m['crawl'] || typeof m['crawl'] !== 'object') throw new Error(`manifest-loader: ${filePath}: crawl is required`);
}

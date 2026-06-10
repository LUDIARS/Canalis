// Canalis 設定ストア (リポ内 config.json)。
//
// 方針 (LUDIARS 規約: 非シークレットは平文・secret は非平文):
//   - 保存先は リポジトリ直下 `config.json` (gitignore 済 → コミットされない)。
//     env override は `CANALIS_CONFIG_PATH`。
//   - 非シークレット (notion.version / minIntervalMs) は平文。
//   - notion.token のみ salt 付き AES-256-GCM で暗号化 (config/crypto.ts、 Excubitor と同方式)。
//     → config.json を読めても EncryptedBlob のみで、 鍵が無ければトークンは復元不能。
//   - master 鍵は env `CANALIS_MASTER_KEY` → 無ければマシン束縛値 (hostname + user)。
//
// 解決 (crawl 時): env `NOTION_TOKEN` を優先し、 無ければ config の暗号化トークンを復号して使う。

import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { hostname, userInfo } from 'node:os';
import { encryptJson, decryptJson, isEncryptedBlob, type EncryptedBlob } from './crypto.js';

interface NotionConfig {
  /** 暗号化済 integration token。 */
  tokenEnc?: EncryptedBlob;
  /** Notion-Version ヘッダ (非シークレット)。 */
  version?: string;
  /** API 呼び出し最小間隔 ms (非シークレット)。 */
  minIntervalMs?: number;
}

export interface CanalisConfig {
  notion?: NotionConfig;
}

/** 設定対象キー (CLI から触れる範囲)。 */
export type ConfigKey = 'notion.token' | 'notion.version' | 'notion.minIntervalMs';

/** 保存先: env override → リポジトリ直下 config.json。 */
export function configPath(): string {
  const override = process.env['CANALIS_CONFIG_PATH'];
  if (override && override.length > 0) return override;
  return join(process.cwd(), 'config.json');
}

/** master secret: env override → マシン束縛値 (hostname + user)。 */
function masterSecret(): string {
  const override = process.env['CANALIS_MASTER_KEY'];
  if (override && override.length > 0) return override;
  return `canalis:${hostname()}:${userInfo().username}`;
}

/** config.json を読む。 未存在 / 壊れていれば空 config 扱い。 */
export function readConfig(): CanalisConfig {
  const path = configPath();
  if (!existsSync(path)) return {};
  try {
    const parsed = JSON.parse(readFileSync(path, 'utf8')) as CanalisConfig;
    return parsed && typeof parsed === 'object' ? parsed : {};
  } catch {
    return {};
  }
}

/** config.json を書く (2-space JSON)。 */
export function writeConfig(cfg: CanalisConfig): void {
  const path = configPath();
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(cfg, null, 2)}\n`, 'utf8');
}

// ─────────────── 書き込み (CLI 設定 UI から呼ぶ) ───────────────

/** notion.token を暗号化して保存。 */
export function setNotionToken(token: string): void {
  if (!token) throw new Error('token is empty');
  const cfg = readConfig();
  cfg.notion = { ...cfg.notion, tokenEnc: encryptJson(token, masterSecret()) };
  writeConfig(cfg);
}

/** notion.token を削除。 */
export function clearNotionToken(): void {
  const cfg = readConfig();
  if (cfg.notion) delete cfg.notion.tokenEnc;
  writeConfig(cfg);
}

/** 非シークレット設定を保存。 */
export function setNotionVersion(version: string): void {
  const cfg = readConfig();
  cfg.notion = { ...cfg.notion, version };
  writeConfig(cfg);
}

export function setNotionMinInterval(ms: number): void {
  if (!Number.isFinite(ms) || ms < 0) throw new Error('minIntervalMs must be a non-negative number');
  const cfg = readConfig();
  cfg.notion = { ...cfg.notion, minIntervalMs: ms };
  writeConfig(cfg);
}

// ─────────────── 読み出し (crawl 解決) ───────────────

/** config の暗号化トークンを復号して返す。 未設定 / 復号失敗時は null。 */
export function getStoredToken(): string | null {
  const blob = readConfig().notion?.tokenEnc;
  if (!blob || !isEncryptedBlob(blob)) return null;
  try {
    return decryptJson<string>(blob, masterSecret());
  } catch {
    return null; // master 鍵が変わった等
  }
}

/** crawl が使うトークンを解決: env NOTION_TOKEN を優先、 無ければ config。 */
export function resolveNotionToken(): string | null {
  return process.env['NOTION_TOKEN'] ?? getStoredToken();
}

/**
 * config の非シークレット設定を process.env に注入 (既存 env は上書きしない)。
 * NotionApiClient は env から version / minInterval を読むため、 crawl 前に 1 回呼ぶ。
 */
export function applyNotionConfigToEnv(): void {
  const notion = readConfig().notion;
  if (!notion) return;
  if (notion.version && !process.env['NOTION_VERSION']) {
    process.env['NOTION_VERSION'] = notion.version;
  }
  if (notion.minIntervalMs !== undefined && !process.env['NOTION_MIN_INTERVAL_MS']) {
    process.env['NOTION_MIN_INTERVAL_MS'] = String(notion.minIntervalMs);
  }
}

// ─────────────── 表示 (config show) ───────────────

export interface ConfigStatus {
  path: string;
  /** トークンの出所。 */
  tokenSource: 'env' | 'config' | 'none';
  /** マスク表示 (末尾4文字のみ)。 平文は返さない。 */
  tokenHint: string | null;
  version: string | null;
  minIntervalMs: number | null;
}

export function getConfigStatus(): ConfigStatus {
  const notion = readConfig().notion;
  const envToken = process.env['NOTION_TOKEN'];
  const stored = getStoredToken();
  const effective = envToken ?? stored;
  const tokenSource: ConfigStatus['tokenSource'] = envToken ? 'env' : stored ? 'config' : 'none';
  return {
    path: configPath(),
    tokenSource,
    tokenHint: effective ? `…${effective.slice(-4)}` : null,
    version: notion?.version ?? null,
    minIntervalMs: notion?.minIntervalMs ?? null,
  };
}

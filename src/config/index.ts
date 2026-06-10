// config: リポ内 config.json による設定 (notion.token は暗号化)。
//   - store : 読み書き + crawl 解決 (resolveNotionToken / applyNotionConfigToEnv)
//   - cli   : `canalis config ...` サブコマンド (設定 UI)
//   - crypto: salt 付き AES-256-GCM (Excubitor と同方式)

export {
  configPath,
  readConfig,
  resolveNotionToken,
  applyNotionConfigToEnv,
  getStoredToken,
  getConfigStatus,
  type CanalisConfig,
  type ConfigStatus,
} from './store.js';
export { runConfigCommand } from './cli.js';
export { type EncryptedBlob } from './crypto.js';

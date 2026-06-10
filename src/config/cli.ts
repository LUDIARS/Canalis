// `canalis config ...` サブコマンド (設定 UI)。
//
//   canalis config set notion.token [value]     # value 省略時は (非表示) 入力を促す
//   canalis config set notion.version <value>
//   canalis config set notion.minIntervalMs <ms>
//   canalis config unset notion.token
//   canalis config show                          # token はマスク表示
//   canalis config path
//
// token は config.json に暗号化保存される (store.ts)。 非シークレットは平文。

import { createInterface } from 'node:readline';
import {
  setNotionToken,
  clearNotionToken,
  setNotionVersion,
  setNotionMinInterval,
  getConfigStatus,
  configPath,
} from './store.js';

const USAGE = `usage:
  canalis config set notion.token [value]
  canalis config set notion.version <value>
  canalis config set notion.minIntervalMs <ms>
  canalis config unset notion.token
  canalis config show
  canalis config path
`;

/** token を対話 (TTY: echo 非表示) または パイプ (非 TTY: stdin 全読み) で受け取る。 */
function readToken(): Promise<string> {
  if (process.stdin.isTTY) return promptHidden('Notion token (入力は表示されません): ');
  // パイプ入力 (例: `echo $TOKEN | canalis config set notion.token`)。 全体を 1 値として読む。
  return new Promise((resolve, reject) => {
    let data = '';
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', (c) => (data += c));
    process.stdin.on('end', () => resolve(data.trim()));
    process.stdin.on('error', reject);
  });
}

/** TTY 専用: echo を伏せて 1 行読む (パスワード入力の定石)。 */
function promptHidden(label: string): Promise<string> {
  return new Promise((resolve) => {
    const rl = createInterface({ input: process.stdin, output: process.stdout, terminal: true });
    process.stderr.write(label);
    (rl as unknown as { _writeToOutput: (s: string) => void })._writeToOutput = () => {};
    rl.question('', (answer) => {
      rl.close();
      process.stderr.write('\n');
      resolve(answer.trim());
    });
  });
}

async function handleSet(argv: string[]): Promise<number> {
  const key = argv[0];
  const value = argv[1];
  switch (key) {
    case 'notion.token': {
      const token = value ?? (await readToken());
      if (!token) {
        process.stderr.write('error: token is empty\n');
        return 2;
      }
      setNotionToken(token);
      process.stdout.write(`saved notion.token (encrypted) → ${configPath()}\n`);
      return 0;
    }
    case 'notion.version': {
      if (!value) return usageError('notion.version requires a value');
      setNotionVersion(value);
      process.stdout.write(`saved notion.version=${value}\n`);
      return 0;
    }
    case 'notion.minIntervalMs': {
      if (!value) return usageError('notion.minIntervalMs requires a value');
      const ms = Number(value);
      if (!Number.isFinite(ms) || ms < 0) return usageError('minIntervalMs must be a non-negative number');
      setNotionMinInterval(ms);
      process.stdout.write(`saved notion.minIntervalMs=${ms}\n`);
      return 0;
    }
    default:
      return usageError(`unknown key: ${key ?? '(none)'}`);
  }
}

function handleUnset(argv: string[]): number {
  const key = argv[0];
  if (key === 'notion.token') {
    clearNotionToken();
    process.stdout.write('removed notion.token\n');
    return 0;
  }
  return usageError(`unknown key: ${key ?? '(none)'}`);
}

function handleShow(): number {
  const s = getConfigStatus();
  process.stdout.write(
    [
      `path           : ${s.path}`,
      `notion.token   : ${s.tokenHint ?? '(unset)'} [source: ${s.tokenSource}]`,
      `notion.version : ${s.version ?? '(default 2022-06-28)'}`,
      `notion.minIntervalMs : ${s.minIntervalMs ?? '(default 350)'}`,
      '',
    ].join('\n'),
  );
  return 0;
}

function usageError(msg: string): number {
  process.stderr.write(`error: ${msg}\n\n${USAGE}`);
  return 2;
}

/** `config` サブコマンド本体。 cli.ts から呼ばれる。 */
export async function runConfigCommand(argv: string[]): Promise<number> {
  const [sub, ...rest] = argv;
  switch (sub) {
    case 'set':
      return handleSet(rest);
    case 'unset':
      return handleUnset(rest);
    case 'show':
      return handleShow();
    case 'path':
      process.stdout.write(`${configPath()}\n`);
      return 0;
    default:
      return usageError(`unknown subcommand: ${sub ?? '(none)'}`);
  }
}

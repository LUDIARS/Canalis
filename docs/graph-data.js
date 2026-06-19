/* Canalis ドメイン/機能グラフのデータ。 実コード (src/) の公開面から構成。
 * domain: 色分けキー。 kind: interface|type|class|fn|external。 */
window.GRAPH = {
  domains: {
    core:      { label: '契約 / Core',     color: '#f2cc60' },
    crawl:     { label: '① Crawl adapter',  color: '#5ec98a' },
    transform: { label: '② Transform',      color: '#c08bff' },
    save:      { label: '③ Save writer',    color: '#ff9d5c' },
    runner:    { label: 'Runner',           color: '#4ea1ff' },
    llm:       { label: 'LLM (opt-in)',     color: '#ff7a9c' },
    config:    { label: 'Config',           color: '#6fd0c5' },
    ext:       { label: '外部 (core 外)',    color: '#7d8893' },
  },

  nodes: [
    // --- core 契約 ---
    { id: 'Source',     domain: 'core', kind: 'interface', desc: '①取得元から RawRecord[] を得る抽象契約。adapter が実装する。' },
    { id: 'Transform',  domain: 'core', kind: 'interface', desc: '②RawRecord[] を sink ネイティブのエンベロープへ整形する契約。実体はサービス側。' },
    { id: 'Sink',       domain: 'core', kind: 'interface', desc: '③渡されたものを保存先へ verbatim 書込する契約。accepts でルーティング。' },
    { id: 'RawRecord',  domain: 'core', kind: 'type',      desc: '①→②/①→raw の受け渡し型。source 非依存の正規化生レコード（raw に原データ verbatim）。' },
    { id: 'SinkBatch',  domain: 'core', kind: 'type',      desc: '②→③ の sink ネイティブ最終形。RdbBatch | KgBatch | FtBatch。' },
    { id: 'Manifest',   domain: 'core', kind: 'type',      desc: 'パイプライン宣言（crawl/transform/save/replayFrom）。YAML/JSON を loadManifest で読む。' },

    // --- runner ---
    { id: 'runPipeline', domain: 'runner', kind: 'fn', desc: 'manifest を解釈し ①→[raw]→②→③ を実行するオーケストレータ。実体は deps に DI。' },

    // --- ① crawl adapters ---
    { id: 'NotionSource',        domain: 'crawl', kind: 'class', desc: 'Notion DB を再帰クロール（公式 API・token 必要）。Tirocinium から移植。' },
    { id: 'NotionPublicSource',  domain: 'crawl', kind: 'class', desc: '公開 Notion ページを Playwright でレンダリング取得（token 不要）。' },
    { id: 'YouTubeSource',       domain: 'crawl', kind: 'class', desc: 'YouTube Data API v3 でコメントスレッドを取得。' },
    { id: 'RedditSource',        domain: 'crawl', kind: 'class', desc: 'Reddit 公開 JSON API で投稿/コメントを取得（認証不要）。' },
    { id: 'WebsiteSource',       domain: 'crawl', kind: 'class', desc: 'URL から HTML を取得。HtmlParser（Lector 等）を注入可能。' },
    { id: 'DiscordSource',       domain: 'crawl', kind: 'class', desc: 'Discord 1 チャンネルのメッセージを取得（REST v10）。' },
    { id: 'DiscordGuildSource',  domain: 'crawl', kind: 'class', desc: 'ギルド全チャンネル＋フォーラムスレッドを取得。' },

    // --- ② transform (外部) ---
    { id: 'Service ②',  domain: 'transform', kind: 'external', desc: 'サービス固有の整形・分類。各サービスリポに置く（共有 lib は契約のみ）。LLM opt-in はここ。' },

    // --- ③ save writers ---
    { id: 'JsonlRawSink', domain: 'save', kind: 'class', desc: 'RawRecord[] を JSONL で verbatim 保存（accepts: raw）。replay の起点。' },
    { id: 'PostgresSink', domain: 'save', kind: 'class', desc: 'RdbBatch を Postgres へ upsert（accepts: rdb）。SqlExecutor を注入。' },
    { id: 'KuzuSink',     domain: 'save', kind: 'class', desc: 'KgBatch を Kuzu へ MERGE（accepts: kg）。CypherExecutor を注入。' },
    { id: 'FtSink',       domain: 'save', kind: 'class', desc: 'FtBatch を学習データセット＋job.json に materialize（accepts: ft）。FtRunner を注入。' },

    // --- LLM (opt-in, /llm サブパス) ---
    { id: 'Cascade',            domain: 'llm', kind: 'class', desc: '安価推論カスケード。安い段から評価し確信が出た段で打ち切る。LLM は最後の手段。' },
    { id: 'lexiconScorer',      domain: 'llm', kind: 'fn',    desc: 'Tier0: 辞書+信号の決定論スコアラ（LLM/外部依存なし・純関数）。' },
    { id: 'executorScorer',     domain: 'llm', kind: 'fn',    desc: 'Tier1/2: 注入 LlmExecutor を分類用 TierScorer に橋渡し。' },
    { id: 'localOpenAiExecutor', domain: 'llm', kind: 'fn',   desc: 'OpenAI 互換 /v1/chat/completions を叩く注入ヘルパ（Ollama/vLLM 等）。SDK/キー不要。' },

    // --- config ---
    { id: 'config',  domain: 'config', kind: 'fn', desc: 'config.json による設定。notion.token を AES-256-GCM 暗号化、非シークレットは平文。' },

    // --- 外部 runner ---
    { id: 'FT runner', domain: 'ext', kind: 'external', desc: 'core 外の学習プロセス（Python: PEFT/Unsloth 等）。job.json を受けて fine-tune。' },
  ],

  // from → to。 rel = 関係ラベル。 dashed = opt-in/外部境界。
  edges: [
    // adapters implement Source
    { from: 'NotionSource', to: 'Source', rel: 'implements' },
    { from: 'NotionPublicSource', to: 'Source', rel: 'implements' },
    { from: 'YouTubeSource', to: 'Source', rel: 'implements' },
    { from: 'RedditSource', to: 'Source', rel: 'implements' },
    { from: 'WebsiteSource', to: 'Source', rel: 'implements' },
    { from: 'DiscordSource', to: 'Source', rel: 'implements' },
    { from: 'DiscordGuildSource', to: 'Source', rel: 'implements' },

    // writers implement Sink
    { from: 'JsonlRawSink', to: 'Sink', rel: 'implements' },
    { from: 'PostgresSink', to: 'Sink', rel: 'implements' },
    { from: 'KuzuSink', to: 'Sink', rel: 'implements' },
    { from: 'FtSink', to: 'Sink', rel: 'implements' },

    // data flow
    { from: 'Source', to: 'RawRecord', rel: 'produces' },
    { from: 'RawRecord', to: 'Service ②', rel: '②入力' },
    { from: 'RawRecord', to: 'JsonlRawSink', rel: 'raw verbatim', dashed: true },
    { from: 'Service ②', to: 'SinkBatch', rel: 'produces' },
    { from: 'SinkBatch', to: 'PostgresSink', rel: 'kind=rdb' },
    { from: 'SinkBatch', to: 'KuzuSink', rel: 'kind=kg' },
    { from: 'SinkBatch', to: 'FtSink', rel: 'kind=ft' },
    { from: 'Service ②', to: 'Transform', rel: 'implements', dashed: true },

    // runner orchestration
    { from: 'runPipeline', to: 'Manifest', rel: 'reads' },
    { from: 'runPipeline', to: 'Source', rel: 'drives' },
    { from: 'runPipeline', to: 'Transform', rel: 'drives' },
    { from: 'runPipeline', to: 'Sink', rel: 'drives' },

    // FT launch (core 外境界)
    { from: 'FtSink', to: 'FT runner', rel: 'launches', dashed: true },

    // config
    { from: 'config', to: 'NotionSource', rel: 'token' },

    // LLM (opt-in)
    { from: 'Service ②', to: 'Cascade', rel: 'opt-in', dashed: true },
    { from: 'Cascade', to: 'lexiconScorer', rel: 'tier0' },
    { from: 'Cascade', to: 'executorScorer', rel: 'tier1/2' },
    { from: 'executorScorer', to: 'localOpenAiExecutor', rel: 'calls' },
    { from: 'Cascade', to: 'FtSink', rel: '教師→FT', dashed: true },
  ],
};

export type RedditSourceConfig = {
  /** subreddit 名 (r/ なし)。 postId と排他。 */
  subreddit?: string;
  /** 個別投稿 ID (t3_ プレフィックスなし)。 subreddit と排他。 */
  postId?: string;
  subredditForPost?: string;
  /** 最大取得件数 (デフォルト 100)。 */
  limit?: number;
  /** 並び順 (サブレ一覧のみ有効)。 */
  sort?: 'hot' | 'new' | 'top' | 'rising';
  /** 取得対象: 'posts' (一覧) | 'comments' (投稿のコメント)。 デフォルト 'posts'。 */
  mode?: 'posts' | 'comments';
};

export type RedditPost = {
  id: string;
  name: string;
  title: string;
  selftext: string;
  url: string;
  permalink: string;
  author: string;
  subreddit: string;
  score: number;
  upvote_ratio: number;
  num_comments: number;
  created_utc: number;
};

export type RedditComment = {
  id: string;
  name: string;
  body: string;
  author: string;
  permalink: string;
  subreddit: string;
  score: number;
  created_utc: number;
  parent_id: string;
  link_id: string;
};

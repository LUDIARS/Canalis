export type YouTubeSourceConfig = {
  videoId: string;
  apiKey: string;
  /** コメント最大取得数 (デフォルト 100)。 */
  maxResults?: number;
  /** textFormat: 'plainText' | 'html' (デフォルト 'plainText') */
  textFormat?: 'plainText' | 'html';
};

export type YouTubeCommentSnippet = {
  videoId: string;
  topLevelComment: {
    id: string;
    snippet: {
      videoId: string;
      textDisplay: string;
      authorDisplayName: string;
      authorChannelId?: { value: string };
      likeCount: number;
      publishedAt: string;
      updatedAt: string;
    };
  };
  replyCount: number;
  totalReplyCount: number;
};

export type YouTubeCommentThread = {
  kind: 'youtube#commentThread';
  etag: string;
  id: string;
  snippet: YouTubeCommentSnippet;
};

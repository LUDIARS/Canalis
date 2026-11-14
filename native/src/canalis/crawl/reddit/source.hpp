#pragma once
// ①Crawl adapter: Reddit の公開 JSON でポスト / コメントを取得し RawRecord へ変換する
// (src/crawl/reddit/{types,source}.ts の移植)。認証不要。User-Agent は必須 (Reddit の規約)。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/core/pipeline.hpp"
#include "canalis/crawl/fetch.hpp"

namespace canalis::crawl::reddit {

struct RedditSourceConfig {
    /** subreddit 名 (r/ なし)。mode=posts で使う。 */
    std::optional<std::string> subreddit;
    /** 個別投稿 ID (t3_ なし)。mode=comments で使う。 */
    std::optional<std::string> post_id;
    std::optional<std::string> subreddit_for_post;
    /** 最大取得件数 (posts は既定 100、comments は既定 500)。 */
    std::optional<int> limit;
    /** "hot" | "new" | "top" | "rising" (既定 "hot")。 */
    std::optional<std::string> sort;
    /** "posts" | "comments" (既定 "posts")。 */
    std::optional<std::string> mode;
};

struct RedditSourceDeps {
    FetchFn fetch;
    std::function<std::string()> now;
};

class RedditSource final : public core::Source<RedditSourceConfig> {
public:
    explicit RedditSource(RedditSourceDeps deps = {});
    std::string_view name() const override { return "reddit"; }
    std::vector<core::RawRecord> crawl(const RedditSourceConfig& config) override;

private:
    Json get(const std::string& url) const;
    RedditSourceDeps deps_;
};

} // namespace canalis::crawl::reddit

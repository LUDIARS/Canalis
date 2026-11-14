#pragma once
// ①Crawl adapter: Steam の 3 つの取得 (新作一覧 / アプリ別レビュー / ユーザ別レビュー) を Source 契約に合わせる薄いラッパ
// (src/crawl/steam/source.ts の移植)。取得の本体は new_releases / app_reviews / user_reviews。
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/core/pipeline.hpp"
#include "canalis/crawl/steam/app_reviews.hpp"
#include "canalis/crawl/steam/new_releases.hpp"
#include "canalis/crawl/steam/user_reviews.hpp"

namespace canalis::crawl::steam {

struct SteamSourceDeps {
    std::function<std::string()> now;
};

class SteamNewReleasesSource final : public core::Source<FetchNewReleasesOptions> {
public:
    explicit SteamNewReleasesSource(SteamSourceDeps deps = {});
    std::string_view name() const override { return "steam-new-releases"; }
    std::vector<core::RawRecord> crawl(const FetchNewReleasesOptions& config) override;

private:
    SteamSourceDeps deps_;
};

class SteamAppReviewsSource final : public core::Source<FetchAppReviewsOptions> {
public:
    explicit SteamAppReviewsSource(SteamSourceDeps deps = {});
    std::string_view name() const override { return "steam-app-reviews"; }
    std::vector<core::RawRecord> crawl(const FetchAppReviewsOptions& config) override;

private:
    SteamSourceDeps deps_;
};

class SteamUserReviewsSource final : public core::Source<FetchUserReviewsOptions> {
public:
    explicit SteamUserReviewsSource(SteamSourceDeps deps = {});
    std::string_view name() const override { return "steam-user-reviews"; }
    std::vector<core::RawRecord> crawl(const FetchUserReviewsOptions& config) override;

private:
    SteamSourceDeps deps_;
};

} // namespace canalis::crawl::steam

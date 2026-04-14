#include "ranking/ranking_board.h"

#include <algorithm>

namespace rankmatch::ranking {

RankingBoard::RankingBoard(storage::StorageRepository& repository) : repository_(repository) {}

void RankingBoard::set_cache(cache::RankingCache* cache) {
  cache_ = cache;
}

bool RankingBoard::rebuild_cache(std::string* error) const {
  if (cache_ == nullptr) {
    if (error != nullptr) {
      *error = "ranking cache is not configured";
    }
    return false;
  }
  std::string db_error;
  const auto players = repository_.list_players(&db_error);
  if (!db_error.empty()) {
    if (error != nullptr) {
      *error = db_error;
    }
    return false;
  }
  return cache_->rebuild(players, error);
}

bool RankingBoard::register_player(const std::string& player_id,
                                   const std::string& nickname,
                                   int score,
                                   std::string* error) {
  if (!repository_.upsert_player(player_id, nickname, score, error)) {
    return false;
  }
  refresh_player_cache(player_id);
  return true;
}

std::vector<model::PlayerInfo> RankingBoard::list_players(std::string* error) const {
  return repository_.list_players(error);
}

std::vector<model::RankEntry> RankingBoard::top_n(int n, std::string* error) const {
  if (cache_ != nullptr) {
    std::string cache_error;
    const auto cached_entries = cache_->top_n(n, &cache_error);
    if (cache_error.empty()) {
      std::vector<model::RankEntry> ranking;
      int rank = 1;
      for (const auto& [player_id, score] : cached_entries) {
        model::RankEntry entry;
        entry.rank = rank++;
        entry.player_id = player_id;
        entry.score = score;

        std::string player_error;
        const auto player = repository_.get_player(player_id, &player_error);
        if (player.has_value()) {
          entry.nickname = player->nickname;
        }
        ranking.push_back(entry);
      }
      if (!ranking.empty() || n == 0) {
        return ranking;
      }
    }
  }
  return repository_.get_top_n(n, error);
}

std::optional<int> RankingBoard::rank_of(const std::string& player_id, std::string* error) const {
  if (cache_ != nullptr) {
    std::string cache_error;
    const auto cached_rank = cache_->rank_of(player_id, &cache_error);
    if (cache_error.empty()) {
      return cached_rank;
    }
  }
  return repository_.get_rank(player_id, error);
}

std::optional<model::PlayerInfo> RankingBoard::find_player(const std::string& player_id,
                                                           std::string* error) const {
  return repository_.get_player(player_id, error);
}

model::ScoreDelta RankingBoard::calculate_score_delta(int winner_score, int loser_score) const {
  const int score_gap = loser_score - winner_score;
  const int bonus = score_gap > 0 ? std::min(20, score_gap / 20) : 0;

  model::ScoreDelta delta;
  delta.winner_gain = 30 + bonus;
  delta.loser_loss = -(15 + bonus / 2);
  return delta;
}

void RankingBoard::refresh_player_cache(const std::string& player_id) const {
  if (cache_ == nullptr) {
    return;
  }

  std::string error;
  const auto player = repository_.get_player(player_id, &error);
  if (!player.has_value()) {
    return;
  }
  cache_->upsert_score(player->player_id, player->score, &error);
}

}  // namespace rankmatch::ranking

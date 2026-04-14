#include "ranking/ranking_board.h"

#include <algorithm>

namespace rankmatch::ranking {

RankingBoard::RankingBoard(storage::StorageRepository& repository) : repository_(repository) {}

bool RankingBoard::register_player(const std::string& player_id,
                                   const std::string& nickname,
                                   int score,
                                   std::string* error) {
  return repository_.upsert_player(player_id, nickname, score, error);
}

std::vector<model::PlayerInfo> RankingBoard::list_players(std::string* error) const {
  return repository_.list_players(error);
}

std::vector<model::RankEntry> RankingBoard::top_n(int n, std::string* error) const {
  return repository_.get_top_n(n, error);
}

std::optional<int> RankingBoard::rank_of(const std::string& player_id, std::string* error) const {
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

}  // namespace rankmatch::ranking

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "model/types.h"
#include "storage/storage_repository.h"

namespace rankmatch::ranking {

class RankingBoard {
 public:
  explicit RankingBoard(storage::StorageRepository& repository);

  bool register_player(const std::string& player_id,
                       const std::string& nickname,
                       int score,
                       std::string* error);
  std::vector<model::PlayerInfo> list_players(std::string* error) const;
  std::vector<model::RankEntry> top_n(int n, std::string* error) const;
  std::optional<int> rank_of(const std::string& player_id, std::string* error) const;
  std::optional<model::PlayerInfo> find_player(const std::string& player_id,
                                               std::string* error) const;
  model::ScoreDelta calculate_score_delta(int winner_score, int loser_score) const;

 private:
  storage::StorageRepository& repository_;
};

}  // namespace rankmatch::ranking

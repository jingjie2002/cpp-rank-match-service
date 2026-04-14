#pragma once

#include <string>
#include <vector>

#include "model/types.h"
#include "ranking/ranking_board.h"
#include "storage/storage_repository.h"

namespace rankmatch::reward {

class RewardManager {
 public:
  RewardManager(storage::StorageRepository& repository, ranking::RankingBoard& ranking_board);

  bool settle_current_season(int* settled_season, int* reward_count, std::string* error);
  std::vector<model::RewardRecord> list_rewards(const std::string& player_id,
                                                std::string* error) const;
  storage::StorageRepository::ClaimStatus claim_reward(int reward_id,
                                                       const std::string& player_id,
                                                       model::RewardRecord* record,
                                                       std::string* error);

 private:
  storage::StorageRepository& repository_;
  ranking::RankingBoard& ranking_board_;
};

}  // namespace rankmatch::reward

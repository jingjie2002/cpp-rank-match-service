#include "reward/reward_manager.h"

namespace rankmatch::reward {

RewardManager::RewardManager(storage::StorageRepository& repository,
                             ranking::RankingBoard& ranking_board)
    : repository_(repository), ranking_board_(ranking_board) {}

bool RewardManager::settle_current_season(int* settled_season,
                                          int* reward_count,
                                          std::string* error) {
  const int season_id = repository_.current_season(error);
  if (season_id <= 0) {
    return false;
  }

  const auto ranking = ranking_board_.top_n(3, error);
  if (ranking.empty()) {
    if (error != nullptr) {
      *error = "no ranked players available for settlement";
    }
    return false;
  }

  if (!repository_.settle_season(season_id, ranking, error)) {
    return false;
  }

  if (settled_season != nullptr) {
    *settled_season = season_id;
  }
  if (reward_count != nullptr) {
    *reward_count = static_cast<int>(ranking.size());
  }
  return true;
}

std::vector<model::RewardRecord> RewardManager::list_rewards(const std::string& player_id,
                                                             std::string* error) const {
  return repository_.list_rewards(player_id, error);
}

storage::StorageRepository::ClaimStatus RewardManager::claim_reward(
    int reward_id,
    const std::string& player_id,
    model::RewardRecord* record,
    std::string* error) {
  return repository_.claim_reward(reward_id, player_id, record, error);
}

}  // namespace rankmatch::reward

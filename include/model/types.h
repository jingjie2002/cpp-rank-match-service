#pragma once

#include <chrono>
#include <string>

namespace rankmatch::model {

struct PlayerInfo {
  std::string player_id;
  std::string nickname;
  int score = 1000;
  std::string updated_at;
};

struct RankEntry {
  int rank = 0;
  std::string player_id;
  std::string nickname;
  int score = 0;
};

struct MatchTicket {
  std::string player_id;
  std::string nickname;
  int score = 0;
  std::chrono::steady_clock::time_point joined_at;
};

struct PendingMatch {
  int match_id = 0;
  std::string player_a;
  std::string player_b;
  int score_a_before = 0;
  int score_b_before = 0;
  std::string created_at;
};

struct RewardRecord {
  int reward_id = 0;
  int season_id = 0;
  std::string player_id;
  int rank = 0;
  int reward_coin = 0;
  bool claimed = false;
  std::string created_at;
  std::string claimed_at;
};

struct ScoreDelta {
  int winner_gain = 0;
  int loser_loss = 0;
};

}  // namespace rankmatch::model

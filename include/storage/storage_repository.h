#pragma once

#include <optional>
#include <string>
#include <vector>

#include "model/types.h"

struct sqlite3;
struct sqlite3_stmt;

namespace rankmatch::storage {

class StorageRepository {
 public:
  enum class ClaimStatus {
    kClaimed,
    kAlreadyClaimed,
    kNotFound,
    kOwnershipMismatch,
    kDbError
  };

  explicit StorageRepository(std::string db_path);
  ~StorageRepository();

  bool open(std::string* error);
  bool initialize_schema(std::string* error);

  bool upsert_player(const std::string& player_id,
                     const std::string& nickname,
                     int score,
                     std::string* error);
  std::optional<model::PlayerInfo> get_player(const std::string& player_id,
                                              std::string* error) const;
  std::vector<model::PlayerInfo> list_players(std::string* error) const;
  std::vector<model::RankEntry> get_top_n(int n, std::string* error) const;
  std::optional<int> get_rank(const std::string& player_id, std::string* error) const;

  int create_pending_match(const std::string& player_a,
                           const std::string& player_b,
                           int score_a_before,
                           int score_b_before,
                           std::string* error);
  std::vector<model::PendingMatch> list_pending_matches(std::string* error) const;
  bool complete_match(int match_id,
                      const std::string& winner_id,
                      const std::string& loser_id,
                      int winner_gain,
                      int loser_loss,
                      std::string* error);

  int current_season(std::string* error) const;
  bool settle_season(int season_id,
                     const std::vector<model::RankEntry>& ranking_snapshot,
                     std::string* error);
  std::vector<model::RewardRecord> list_rewards(const std::string& player_id,
                                                std::string* error) const;
  ClaimStatus claim_reward(int reward_id,
                           const std::string& player_id,
                           model::RewardRecord* record,
                           std::string* error);

 private:
  std::string db_path_;
  sqlite3* db_ = nullptr;

  bool exec_sql(const std::string& sql, std::string* error) const;
  sqlite3_stmt* prepare(const std::string& sql, std::string* error) const;
  static void finalize(sqlite3_stmt* stmt);
  static void bind_text(sqlite3_stmt* stmt, int index, const std::string& value);
};

}  // namespace rankmatch::storage

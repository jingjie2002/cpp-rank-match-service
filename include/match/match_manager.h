#pragma once

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "model/types.h"
#include "ranking/ranking_board.h"
#include "storage/storage_repository.h"

namespace rankmatch::match {

class MatchManager {
 public:
  MatchManager(storage::StorageRepository& repository, ranking::RankingBoard& ranking_board);

  bool load_pending_matches(std::string* error);
  bool join_queue(const std::string& player_id, std::string* error);
  bool cancel_queue(const std::string& player_id);
  std::optional<model::PendingMatch> run_once(std::string* error);
  std::vector<model::MatchTicket> queued_players() const;
  std::vector<model::PendingMatch> pending_matches() const;
  std::optional<model::ScoreDelta> finish_match(int match_id,
                                                const std::string& winner_id,
                                                std::string* error);

 private:
  storage::StorageRepository& repository_;
  ranking::RankingBoard& ranking_board_;
  std::deque<model::MatchTicket> queue_;
  std::unordered_set<std::string> queued_player_ids_;
  std::unordered_map<int, model::PendingMatch> pending_matches_;
  int base_score_window_ = 50;
  int score_window_growth_per_second_ = 80;

  int allowed_gap(const model::MatchTicket& ticket) const;
};

}  // namespace rankmatch::match

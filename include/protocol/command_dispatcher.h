#pragma once

#include <string>

#include "match/match_manager.h"
#include "ranking/ranking_board.h"
#include "reward/reward_manager.h"

namespace rankmatch::protocol {

class CommandDispatcher {
 public:
  CommandDispatcher(ranking::RankingBoard& ranking_board,
                    match::MatchManager& match_manager,
                    reward::RewardManager& reward_manager);

  std::string handle_command(const std::string& line);
  bool should_exit() const;

 private:
  ranking::RankingBoard& ranking_board_;
  match::MatchManager& match_manager_;
  reward::RewardManager& reward_manager_;
  bool should_exit_ = false;

  static std::string help_text();
};

}  // namespace rankmatch::protocol

#include "protocol/command_dispatcher.h"

#include <sstream>
#include <vector>

#include "common/time_utils.h"

namespace rankmatch::protocol {

namespace {

std::vector<std::string> split_tokens(const std::string& line) {
  std::string normalized = line;
  if (normalized.rfind("\xEF\xBB\xBF", 0) == 0) {
    normalized.erase(0, 3);
  }

  std::istringstream iss(normalized);
  std::vector<std::string> tokens;
  for (std::string token; iss >> token;) {
    tokens.push_back(token);
  }
  return tokens;
}

}  // namespace

CommandDispatcher::CommandDispatcher(ranking::RankingBoard& ranking_board,
                                     match::MatchManager& match_manager,
                                     reward::RewardManager& reward_manager)
    : ranking_board_(ranking_board),
      match_manager_(match_manager),
      reward_manager_(reward_manager) {}

std::string CommandDispatcher::handle_command(const std::string& line) {
  const auto tokens = split_tokens(line);
  if (tokens.empty()) {
    return {};
  }

  const std::string& cmd = tokens[0];
  std::ostringstream out;
  std::string error;

  if (cmd == "HELP") {
    return help_text();
  }

  if (cmd == "REGISTER_PLAYER") {
    if (tokens.size() != 4) {
      return "ERR usage: REGISTER_PLAYER <player_id> <nickname> <score>";
    }
    const int score = std::stoi(tokens[3]);
    if (!ranking_board_.register_player(tokens[1], tokens[2], score, &error)) {
      return "ERR " + error;
    }
    out << "OK REGISTER_PLAYER player=" << tokens[1] << " score=" << score;
    return out.str();
  }

  if (cmd == "SHOW_PLAYERS") {
    const auto players = ranking_board_.list_players(&error);
    if (!error.empty()) {
      return "ERR " + error;
    }
    if (players.empty()) {
      return "EMPTY players";
    }
    for (const auto& player : players) {
      out << player.player_id << "(" << player.nickname << ")"
          << " score=" << player.score << "\n";
    }
    return out.str();
  }

  if (cmd == "JOIN_MATCH") {
    if (tokens.size() != 2) {
      return "ERR usage: JOIN_MATCH <player_id>";
    }
    if (!match_manager_.join_queue(tokens[1], &error)) {
      return "ERR " + error;
    }
    return "OK JOIN_MATCH player=" + tokens[1];
  }

  if (cmd == "CANCEL_MATCH") {
    if (tokens.size() != 2) {
      return "ERR usage: CANCEL_MATCH <player_id>";
    }
    if (!match_manager_.cancel_queue(tokens[1])) {
      return "ERR player not in queue";
    }
    return "OK CANCEL_MATCH player=" + tokens[1];
  }

  if (cmd == "SHOW_QUEUE") {
    const auto queue = match_manager_.queued_players();
    if (queue.empty()) {
      return "EMPTY queue";
    }
    for (const auto& ticket : queue) {
      out << ticket.player_id << "(" << ticket.nickname << ")"
          << " score=" << ticket.score
          << " waited=" << common::wait_seconds(ticket.joined_at) << "s\n";
    }
    return out.str();
  }

  if (cmd == "RUN_MATCH") {
    const auto match = match_manager_.run_once(&error);
    if (!error.empty()) {
      return "ERR " + error;
    }
    if (!match.has_value()) {
      return "WAIT no compatible players yet";
    }
    out << "OK RUN_MATCH match_id=" << match->match_id
        << " players=" << match->player_a << "," << match->player_b;
    return out.str();
  }

  if (cmd == "SHOW_MATCHES") {
    const auto matches = match_manager_.pending_matches();
    if (matches.empty()) {
      return "EMPTY pending_matches";
    }
    for (const auto& match : matches) {
      out << "match_id=" << match.match_id
          << " players=" << match.player_a << "," << match.player_b
          << " scores=" << match.score_a_before << "," << match.score_b_before
          << "\n";
    }
    return out.str();
  }

  if (cmd == "FINISH_MATCH") {
    if (tokens.size() != 3) {
      return "ERR usage: FINISH_MATCH <match_id> <winner_id>";
    }
    const int match_id = std::stoi(tokens[1]);
    const auto delta = match_manager_.finish_match(match_id, tokens[2], &error);
    if (!delta.has_value()) {
      return "ERR " + error;
    }
    out << "OK FINISH_MATCH match_id=" << match_id
        << " winner=" << tokens[2]
        << " winner_gain=+" << delta->winner_gain
        << " loser_loss=" << delta->loser_loss;
    return out.str();
  }

  if (cmd == "TOP") {
    if (tokens.size() != 2) {
      return "ERR usage: TOP <n>";
    }
    const int n = std::stoi(tokens[1]);
    const auto ranking = ranking_board_.top_n(n, &error);
    if (!error.empty()) {
      return "ERR " + error;
    }
    if (ranking.empty()) {
      return "EMPTY ranking";
    }
    for (const auto& entry : ranking) {
      out << entry.rank << ". " << entry.player_id << "(" << entry.nickname
          << ") score=" << entry.score << "\n";
    }
    return out.str();
  }

  if (cmd == "RANK") {
    if (tokens.size() != 2) {
      return "ERR usage: RANK <player_id>";
    }
    const auto rank = ranking_board_.rank_of(tokens[1], &error);
    if (!error.empty()) {
      return "ERR " + error;
    }
    if (!rank.has_value()) {
      return "ERR player not found";
    }
    out << "OK RANK player=" << tokens[1] << " rank=" << *rank;
    return out.str();
  }

  if (cmd == "SETTLE_SEASON") {
    int season = 0;
    int reward_count = 0;
    if (!reward_manager_.settle_current_season(&season, &reward_count, &error)) {
      return "ERR " + error;
    }
    out << "OK SETTLE_SEASON season=" << season << " rewards=" << reward_count;
    return out.str();
  }

  if (cmd == "LIST_REWARDS") {
    if (tokens.size() != 2) {
      return "ERR usage: LIST_REWARDS <player_id>";
    }
    const auto rewards = reward_manager_.list_rewards(tokens[1], &error);
    if (!error.empty()) {
      return "ERR " + error;
    }
    if (rewards.empty()) {
      return "EMPTY rewards";
    }
    for (const auto& reward : rewards) {
      out << "reward_id=" << reward.reward_id
          << " season=" << reward.season_id
          << " rank=" << reward.rank
          << " coin=" << reward.reward_coin
          << " claimed=" << (reward.claimed ? "YES" : "NO") << "\n";
    }
    return out.str();
  }

  if (cmd == "CLAIM_REWARD") {
    if (tokens.size() != 3) {
      return "ERR usage: CLAIM_REWARD <player_id> <reward_id>";
    }
    model::RewardRecord record;
    const auto status = reward_manager_.claim_reward(
        std::stoi(tokens[2]), tokens[1], &record, &error);
    switch (status) {
      case storage::StorageRepository::ClaimStatus::kClaimed:
        out << "OK CLAIM_REWARD reward_id=" << record.reward_id << " coin=" << record.reward_coin;
        return out.str();
      case storage::StorageRepository::ClaimStatus::kAlreadyClaimed:
        return "ERR reward already claimed";
      case storage::StorageRepository::ClaimStatus::kNotFound:
        return "ERR reward not found";
      case storage::StorageRepository::ClaimStatus::kOwnershipMismatch:
        return "ERR reward does not belong to player";
      case storage::StorageRepository::ClaimStatus::kDbError:
        return "ERR " + error;
    }
  }

  if (cmd == "EXIT") {
    should_exit_ = true;
    return "BYE";
  }

  return "ERR unknown command, try HELP";
}

bool CommandDispatcher::should_exit() const {
  return should_exit_;
}

std::string CommandDispatcher::help_text() {
  return
      "Commands:\n"
      "  HELP\n"
      "  REGISTER_PLAYER <player_id> <nickname> <score>\n"
      "  SHOW_PLAYERS\n"
      "  JOIN_MATCH <player_id>\n"
      "  CANCEL_MATCH <player_id>\n"
      "  SHOW_QUEUE\n"
      "  RUN_MATCH\n"
      "  SHOW_MATCHES\n"
      "  FINISH_MATCH <match_id> <winner_id>\n"
      "  TOP <n>\n"
      "  RANK <player_id>\n"
      "  SETTLE_SEASON\n"
      "  LIST_REWARDS <player_id>\n"
      "  CLAIM_REWARD <player_id> <reward_id>\n"
      "  EXIT";
}

}  // namespace rankmatch::protocol

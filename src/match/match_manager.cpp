#include "match/match_manager.h"

#include <algorithm>
#include <cmath>

#include "common/time_utils.h"

namespace rankmatch::match {

MatchManager::MatchManager(storage::StorageRepository& repository,
                           ranking::RankingBoard& ranking_board)
    : repository_(repository), ranking_board_(ranking_board) {}

bool MatchManager::load_pending_matches(std::string* error) {
  pending_matches_.clear();
  const auto persisted = repository_.list_pending_matches(error);
  for (const auto& match : persisted) {
    pending_matches_.emplace(match.match_id, match);
  }
  return true;
}

bool MatchManager::join_queue(const std::string& player_id, std::string* error) {
  if (queued_player_ids_.count(player_id) != 0U) {
    if (error != nullptr) {
      *error = "player already in queue";
    }
    return false;
  }

  const auto player = ranking_board_.find_player(player_id, error);
  if (!player.has_value()) {
    if (error != nullptr && error->empty()) {
      *error = "player not found";
    }
    return false;
  }

  queue_.push_back(model::MatchTicket{
      player->player_id,
      player->nickname,
      player->score,
      std::chrono::steady_clock::now()});
  queued_player_ids_.insert(player->player_id);
  return true;
}

bool MatchManager::cancel_queue(const std::string& player_id) {
  auto it = std::find_if(queue_.begin(), queue_.end(),
                         [&](const model::MatchTicket& ticket) {
                           return ticket.player_id == player_id;
                         });
  if (it == queue_.end()) {
    return false;
  }
  queue_.erase(it);
  queued_player_ids_.erase(player_id);
  return true;
}

std::optional<model::PendingMatch> MatchManager::run_once(std::string* error) {
  if (queue_.size() < 2) {
    return std::nullopt;
  }

  for (std::size_t i = 0; i < queue_.size(); ++i) {
    for (std::size_t j = i + 1; j < queue_.size(); ++j) {
      const int score_gap = std::abs(queue_[i].score - queue_[j].score);
      const int allowed = std::max(allowed_gap(queue_[i]), allowed_gap(queue_[j]));
      if (score_gap > allowed) {
        continue;
      }

      const int match_id = repository_.create_pending_match(
          queue_[i].player_id, queue_[j].player_id, queue_[i].score, queue_[j].score, error);
      if (match_id == 0) {
        return std::nullopt;
      }

      model::PendingMatch match{
          match_id,
          queue_[i].player_id,
          queue_[j].player_id,
          queue_[i].score,
          queue_[j].score,
          common::now_string()};

      queued_player_ids_.erase(queue_[j].player_id);
      queued_player_ids_.erase(queue_[i].player_id);
      queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(j));
      queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(i));
      pending_matches_.emplace(match.match_id, match);
      return match;
    }
  }
  return std::nullopt;
}

std::vector<model::MatchTicket> MatchManager::queued_players() const {
  return std::vector<model::MatchTicket>(queue_.begin(), queue_.end());
}

std::vector<model::PendingMatch> MatchManager::pending_matches() const {
  std::vector<model::PendingMatch> matches;
  matches.reserve(pending_matches_.size());
  for (const auto& [_, match] : pending_matches_) {
    matches.push_back(match);
  }
  std::sort(matches.begin(), matches.end(),
            [](const model::PendingMatch& left, const model::PendingMatch& right) {
              return left.match_id < right.match_id;
            });
  return matches;
}

std::optional<model::ScoreDelta> MatchManager::finish_match(int match_id,
                                                            const std::string& winner_id,
                                                            std::string* error) {
  const auto pending_it = pending_matches_.find(match_id);
  if (pending_it == pending_matches_.end()) {
    if (error != nullptr) {
      *error = "pending match not found";
    }
    return std::nullopt;
  }

  const auto& match = pending_it->second;
  const bool winner_is_a = winner_id == match.player_a;
  const bool winner_is_b = winner_id == match.player_b;
  if (!winner_is_a && !winner_is_b) {
    if (error != nullptr) {
      *error = "winner must belong to matched players";
    }
    return std::nullopt;
  }

  const std::string loser_id = winner_is_a ? match.player_b : match.player_a;
  const int winner_score = winner_is_a ? match.score_a_before : match.score_b_before;
  const int loser_score = winner_is_a ? match.score_b_before : match.score_a_before;
  const model::ScoreDelta delta = ranking_board_.calculate_score_delta(winner_score, loser_score);

  if (!repository_.complete_match(match_id, winner_id, loser_id, delta.winner_gain,
                                  delta.loser_loss, error)) {
    return std::nullopt;
  }

  ranking_board_.refresh_player_cache(winner_id);
  ranking_board_.refresh_player_cache(loser_id);
  pending_matches_.erase(pending_it);
  return delta;
}

int MatchManager::allowed_gap(const model::MatchTicket& ticket) const {
  return base_score_window_ +
         static_cast<int>(common::wait_seconds(ticket.joined_at)) *
             score_window_growth_per_second_;
}

}  // namespace rankmatch::match

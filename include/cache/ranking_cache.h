#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "model/types.h"

namespace rankmatch::cache {

class RankingCache {
 public:
  virtual ~RankingCache() = default;

  virtual bool connect(std::string* error) = 0;
  virtual bool available() const = 0;
  virtual bool rebuild(const std::vector<model::PlayerInfo>& players, std::string* error) = 0;
  virtual bool upsert_score(const std::string& player_id, int score, std::string* error) = 0;
  virtual std::vector<std::pair<std::string, int>> top_n(int n, std::string* error) = 0;
  virtual std::optional<int> rank_of(const std::string& player_id, std::string* error) = 0;
};

}  // namespace rankmatch::cache

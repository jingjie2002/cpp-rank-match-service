#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cache/ranking_cache.h"

namespace rankmatch::cache {

class RedisRankingCache : public RankingCache {
 public:
  RedisRankingCache(std::string host, int port, std::string key);
  ~RedisRankingCache() override;

  bool connect(std::string* error) override;
  bool available() const override;
  bool rebuild(const std::vector<model::PlayerInfo>& players, std::string* error) override;
  bool upsert_score(const std::string& player_id, int score, std::string* error) override;
  std::vector<std::pair<std::string, int>> top_n(int n, std::string* error) override;
  std::optional<int> rank_of(const std::string& player_id, std::string* error) override;

 private:
  std::string host_;
  int port_ = 6379;
  std::string key_;

  using socket_type = long long;
  socket_type socket_ = -1;
  bool connected_ = false;

  struct RespValue {
    enum class Type { kSimpleString, kError, kInteger, kBulkString, kArray, kNil };

    Type type = Type::kNil;
    std::string string_value;
    long long integer_value = 0;
    std::vector<RespValue> array_values;
  };

  bool ensure_connected(std::string* error);
  void close_connection();
  RespValue execute(const std::vector<std::string>& parts, std::string* error);
  bool send_all(const std::string& request, std::string* error);
  bool read_exact(std::size_t size, std::string* error, std::string* out);
  bool read_line(std::string* error, std::string* out);
  RespValue read_resp_value(std::string* error);
  static std::string build_command(const std::vector<std::string>& parts);
};

}  // namespace rankmatch::cache

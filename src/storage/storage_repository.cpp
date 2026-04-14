#include "storage/storage_repository.h"

#include <array>

#include "common/time_utils.h"
#include "sqlite3.h"

namespace rankmatch::storage {

namespace {

std::string sqlite_error(sqlite3* db) {
  return db == nullptr ? "sqlite database handle is null" : sqlite3_errmsg(db);
}

}  // namespace

StorageRepository::StorageRepository(std::string db_path) : db_path_(std::move(db_path)) {}

StorageRepository::~StorageRepository() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool StorageRepository::open(std::string* error) {
  if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    return false;
  }
  sqlite3_busy_timeout(db_, 3000);
  return true;
}

bool StorageRepository::initialize_schema(std::string* error) {
  const char* schema = R"sql(
CREATE TABLE IF NOT EXISTS players(
  player_id TEXT PRIMARY KEY,
  nickname TEXT NOT NULL,
  score INTEGER NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS matches(
  match_id INTEGER PRIMARY KEY AUTOINCREMENT,
  player_a TEXT NOT NULL,
  player_b TEXT NOT NULL,
  score_a_before INTEGER NOT NULL,
  score_b_before INTEGER NOT NULL,
  winner_id TEXT,
  status TEXT NOT NULL,
  created_at TEXT NOT NULL,
  finished_at TEXT
);

CREATE TABLE IF NOT EXISTS seasons(
  season_id INTEGER PRIMARY KEY,
  settled_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS rewards(
  reward_id INTEGER PRIMARY KEY AUTOINCREMENT,
  season_id INTEGER NOT NULL,
  player_id TEXT NOT NULL,
  rank_no INTEGER NOT NULL,
  reward_coin INTEGER NOT NULL,
  claimed INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL,
  claimed_at TEXT,
  UNIQUE(season_id, player_id)
);

CREATE TABLE IF NOT EXISTS meta(
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

INSERT OR IGNORE INTO meta(key, value) VALUES('current_season', '1');
)sql";
  return exec_sql(schema, error);
}

bool StorageRepository::upsert_player(const std::string& player_id,
                                      const std::string& nickname,
                                      int score,
                                      std::string* error) {
  sqlite3_stmt* stmt = prepare(
      "INSERT INTO players(player_id, nickname, score, updated_at) "
      "VALUES(?, ?, ?, ?) "
      "ON CONFLICT(player_id) DO UPDATE SET "
      "nickname = excluded.nickname, "
      "score = excluded.score, "
      "updated_at = excluded.updated_at;",
      error);
  if (stmt == nullptr) {
    return false;
  }
  bind_text(stmt, 1, player_id);
  bind_text(stmt, 2, nickname);
  sqlite3_bind_int(stmt, 3, score);
  bind_text(stmt, 4, common::now_string());
  const int rc = sqlite3_step(stmt);
  finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    return false;
  }
  return true;
}

std::optional<model::PlayerInfo> StorageRepository::get_player(const std::string& player_id,
                                                               std::string* error) const {
  sqlite3_stmt* stmt = prepare(
      "SELECT player_id, nickname, score, updated_at FROM players WHERE player_id = ?;",
      error);
  if (stmt == nullptr) {
    return std::nullopt;
  }
  bind_text(stmt, 1, player_id);
  const int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    model::PlayerInfo player;
    player.player_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    player.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    player.score = sqlite3_column_int(stmt, 2);
    player.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    finalize(stmt);
    return player;
  }
  if (rc != SQLITE_DONE && error != nullptr) {
    *error = sqlite_error(db_);
  }
  finalize(stmt);
  return std::nullopt;
}

std::vector<model::PlayerInfo> StorageRepository::list_players(std::string* error) const {
  sqlite3_stmt* stmt = prepare(
      "SELECT player_id, nickname, score, updated_at "
      "FROM players ORDER BY score DESC, updated_at ASC, player_id ASC;",
      error);
  std::vector<model::PlayerInfo> players;
  if (stmt == nullptr) {
    return players;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    model::PlayerInfo player;
    player.player_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    player.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    player.score = sqlite3_column_int(stmt, 2);
    player.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    players.push_back(player);
  }
  finalize(stmt);
  return players;
}

std::vector<model::RankEntry> StorageRepository::get_top_n(int n, std::string* error) const {
  sqlite3_stmt* stmt = prepare(
      "SELECT player_id, nickname, score FROM players "
      "ORDER BY score DESC, updated_at ASC, player_id ASC LIMIT ?;",
      error);
  std::vector<model::RankEntry> ranking;
  if (stmt == nullptr) {
    return ranking;
  }
  sqlite3_bind_int(stmt, 1, n);
  int rank = 1;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    model::RankEntry entry;
    entry.rank = rank++;
    entry.player_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    entry.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    entry.score = sqlite3_column_int(stmt, 2);
    ranking.push_back(entry);
  }
  finalize(stmt);
  return ranking;
}

std::optional<int> StorageRepository::get_rank(const std::string& player_id,
                                               std::string* error) const {
  const auto players = list_players(error);
  for (std::size_t index = 0; index < players.size(); ++index) {
    if (players[index].player_id == player_id) {
      return static_cast<int>(index + 1);
    }
  }
  return std::nullopt;
}

int StorageRepository::create_pending_match(const std::string& player_a,
                                            const std::string& player_b,
                                            int score_a_before,
                                            int score_b_before,
                                            std::string* error) {
  sqlite3_stmt* stmt = prepare(
      "INSERT INTO matches(player_a, player_b, score_a_before, score_b_before, status, created_at) "
      "VALUES(?, ?, ?, ?, 'pending', ?);",
      error);
  if (stmt == nullptr) {
    return 0;
  }
  bind_text(stmt, 1, player_a);
  bind_text(stmt, 2, player_b);
  sqlite3_bind_int(stmt, 3, score_a_before);
  sqlite3_bind_int(stmt, 4, score_b_before);
  bind_text(stmt, 5, common::now_string());
  const int rc = sqlite3_step(stmt);
  finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    return 0;
  }
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

std::vector<model::PendingMatch> StorageRepository::list_pending_matches(std::string* error) const {
  sqlite3_stmt* stmt = prepare(
      "SELECT match_id, player_a, player_b, score_a_before, score_b_before, created_at "
      "FROM matches WHERE status = 'pending' ORDER BY match_id ASC;",
      error);
  std::vector<model::PendingMatch> matches;
  if (stmt == nullptr) {
    return matches;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    model::PendingMatch match;
    match.match_id = sqlite3_column_int(stmt, 0);
    match.player_a = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    match.player_b = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    match.score_a_before = sqlite3_column_int(stmt, 3);
    match.score_b_before = sqlite3_column_int(stmt, 4);
    match.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    matches.push_back(match);
  }
  finalize(stmt);
  return matches;
}

bool StorageRepository::complete_match(int match_id,
                                       const std::string& winner_id,
                                       const std::string& loser_id,
                                       int winner_gain,
                                       int loser_loss,
                                       std::string* error) {
  if (!exec_sql("BEGIN IMMEDIATE;", error)) {
    return false;
  }

  sqlite3_stmt* winner_stmt =
      prepare("UPDATE players SET score = score + ?, updated_at = ? WHERE player_id = ?;", error);
  if (winner_stmt == nullptr) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  sqlite3_bind_int(winner_stmt, 1, winner_gain);
  bind_text(winner_stmt, 2, common::now_string());
  bind_text(winner_stmt, 3, winner_id);
  if (sqlite3_step(winner_stmt) != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    finalize(winner_stmt);
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  finalize(winner_stmt);

  sqlite3_stmt* loser_stmt = prepare(
      "UPDATE players SET score = MAX(score + ?, 0), updated_at = ? WHERE player_id = ?;",
      error);
  if (loser_stmt == nullptr) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  sqlite3_bind_int(loser_stmt, 1, loser_loss);
  bind_text(loser_stmt, 2, common::now_string());
  bind_text(loser_stmt, 3, loser_id);
  if (sqlite3_step(loser_stmt) != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    finalize(loser_stmt);
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  finalize(loser_stmt);

  sqlite3_stmt* match_stmt = prepare(
      "UPDATE matches SET winner_id = ?, status = 'finished', finished_at = ? "
      "WHERE match_id = ? AND status = 'pending';",
      error);
  if (match_stmt == nullptr) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  bind_text(match_stmt, 1, winner_id);
  bind_text(match_stmt, 2, common::now_string());
  sqlite3_bind_int(match_stmt, 3, match_id);
  if (sqlite3_step(match_stmt) != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    finalize(match_stmt);
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  finalize(match_stmt);

  if (!exec_sql("COMMIT;", error)) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  return true;
}

int StorageRepository::current_season(std::string* error) const {
  sqlite3_stmt* stmt = prepare("SELECT value FROM meta WHERE key = 'current_season';", error);
  if (stmt == nullptr) {
    return 0;
  }
  const int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    const auto value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const int season = std::stoi(value);
    finalize(stmt);
    return season;
  }
  finalize(stmt);
  if (error != nullptr) {
    *error = "current season metadata missing";
  }
  return 0;
}

bool StorageRepository::settle_season(int season_id,
                                      const std::vector<model::RankEntry>& ranking_snapshot,
                                      std::string* error) {
  if (!exec_sql("BEGIN IMMEDIATE;", error)) {
    return false;
  }

  sqlite3_stmt* season_stmt =
      prepare("INSERT INTO seasons(season_id, settled_at) VALUES(?, ?);", error);
  if (season_stmt == nullptr) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  sqlite3_bind_int(season_stmt, 1, season_id);
  bind_text(season_stmt, 2, common::now_string());
  if (sqlite3_step(season_stmt) != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    finalize(season_stmt);
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  finalize(season_stmt);

  const std::array<int, 3> reward_schedule{300, 200, 100};
  sqlite3_stmt* reward_stmt = prepare(
      "INSERT INTO rewards(season_id, player_id, rank_no, reward_coin, claimed, created_at) "
      "VALUES(?, ?, ?, ?, 0, ?);",
      error);
  if (reward_stmt == nullptr) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }

  for (std::size_t index = 0; index < ranking_snapshot.size() && index < reward_schedule.size();
       ++index) {
    sqlite3_reset(reward_stmt);
    sqlite3_clear_bindings(reward_stmt);
    sqlite3_bind_int(reward_stmt, 1, season_id);
    bind_text(reward_stmt, 2, ranking_snapshot[index].player_id);
    sqlite3_bind_int(reward_stmt, 3, static_cast<int>(index + 1));
    sqlite3_bind_int(reward_stmt, 4, reward_schedule[index]);
    bind_text(reward_stmt, 5, common::now_string());
    if (sqlite3_step(reward_stmt) != SQLITE_DONE) {
      if (error != nullptr) {
        *error = sqlite_error(db_);
      }
      finalize(reward_stmt);
      exec_sql("ROLLBACK;", nullptr);
      return false;
    }
  }
  finalize(reward_stmt);

  sqlite3_stmt* meta_stmt =
      prepare("UPDATE meta SET value = ? WHERE key = 'current_season';", error);
  if (meta_stmt == nullptr) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  bind_text(meta_stmt, 1, std::to_string(season_id + 1));
  if (sqlite3_step(meta_stmt) != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    finalize(meta_stmt);
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  finalize(meta_stmt);

  if (!exec_sql("COMMIT;", error)) {
    exec_sql("ROLLBACK;", nullptr);
    return false;
  }
  return true;
}

std::vector<model::RewardRecord> StorageRepository::list_rewards(const std::string& player_id,
                                                                 std::string* error) const {
  sqlite3_stmt* stmt = prepare(
      "SELECT reward_id, season_id, player_id, rank_no, reward_coin, claimed, created_at, "
      "COALESCE(claimed_at, '') "
      "FROM rewards WHERE player_id = ? ORDER BY season_id DESC, reward_id ASC;",
      error);
  std::vector<model::RewardRecord> rewards;
  if (stmt == nullptr) {
    return rewards;
  }
  bind_text(stmt, 1, player_id);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    model::RewardRecord reward;
    reward.reward_id = sqlite3_column_int(stmt, 0);
    reward.season_id = sqlite3_column_int(stmt, 1);
    reward.player_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    reward.rank = sqlite3_column_int(stmt, 3);
    reward.reward_coin = sqlite3_column_int(stmt, 4);
    reward.claimed = sqlite3_column_int(stmt, 5) != 0;
    reward.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    reward.claimed_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    rewards.push_back(reward);
  }
  finalize(stmt);
  return rewards;
}

StorageRepository::ClaimStatus StorageRepository::claim_reward(int reward_id,
                                                               const std::string& player_id,
                                                               model::RewardRecord* record,
                                                               std::string* error) {
  sqlite3_stmt* query_stmt = prepare(
      "SELECT reward_id, season_id, player_id, rank_no, reward_coin, claimed, created_at, "
      "COALESCE(claimed_at, '') FROM rewards WHERE reward_id = ?;",
      error);
  if (query_stmt == nullptr) {
    return ClaimStatus::kDbError;
  }
  sqlite3_bind_int(query_stmt, 1, reward_id);
  if (sqlite3_step(query_stmt) != SQLITE_ROW) {
    finalize(query_stmt);
    return ClaimStatus::kNotFound;
  }

  model::RewardRecord loaded;
  loaded.reward_id = sqlite3_column_int(query_stmt, 0);
  loaded.season_id = sqlite3_column_int(query_stmt, 1);
  loaded.player_id = reinterpret_cast<const char*>(sqlite3_column_text(query_stmt, 2));
  loaded.rank = sqlite3_column_int(query_stmt, 3);
  loaded.reward_coin = sqlite3_column_int(query_stmt, 4);
  loaded.claimed = sqlite3_column_int(query_stmt, 5) != 0;
  loaded.created_at = reinterpret_cast<const char*>(sqlite3_column_text(query_stmt, 6));
  loaded.claimed_at = reinterpret_cast<const char*>(sqlite3_column_text(query_stmt, 7));
  finalize(query_stmt);

  if (loaded.player_id != player_id) {
    return ClaimStatus::kOwnershipMismatch;
  }
  if (loaded.claimed) {
    if (record != nullptr) {
      *record = loaded;
    }
    return ClaimStatus::kAlreadyClaimed;
  }

  sqlite3_stmt* update_stmt = prepare(
      "UPDATE rewards SET claimed = 1, claimed_at = ? WHERE reward_id = ? AND claimed = 0;",
      error);
  if (update_stmt == nullptr) {
    return ClaimStatus::kDbError;
  }
  bind_text(update_stmt, 1, common::now_string());
  sqlite3_bind_int(update_stmt, 2, reward_id);
  const int rc = sqlite3_step(update_stmt);
  finalize(update_stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    return ClaimStatus::kDbError;
  }

  loaded.claimed = true;
  loaded.claimed_at = common::now_string();
  if (record != nullptr) {
    *record = loaded;
  }
  return ClaimStatus::kClaimed;
}

bool StorageRepository::exec_sql(const std::string& sql, std::string* error) const {
  char* message = nullptr;
  const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &message);
  if (rc != SQLITE_OK) {
    if (error != nullptr) {
      *error = message != nullptr ? message : sqlite_error(db_);
    }
    if (message != nullptr) {
      sqlite3_free(message);
    }
    return false;
  }
  if (message != nullptr) {
    sqlite3_free(message);
  }
  return true;
}

sqlite3_stmt* StorageRepository::prepare(const std::string& sql, std::string* error) const {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite_error(db_);
    }
    return nullptr;
  }
  return stmt;
}

void StorageRepository::finalize(sqlite3_stmt* stmt) {
  if (stmt != nullptr) {
    sqlite3_finalize(stmt);
  }
}

void StorageRepository::bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
  sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

}  // namespace rankmatch::storage

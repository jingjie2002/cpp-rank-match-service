#include <filesystem>
#include <iostream>
#include <string>

#include "match/match_manager.h"
#include "protocol/command_dispatcher.h"
#include "ranking/ranking_board.h"
#include "reward/reward_manager.h"
#include "storage/storage_repository.h"

int main(int argc, char* argv[]) {
  namespace fs = std::filesystem;

  fs::path db_path = argc > 1 ? fs::path(argv[1]) : fs::path("data/rank_match.db");
  if (db_path.has_parent_path()) {
    fs::create_directories(db_path.parent_path());
  }

  rankmatch::storage::StorageRepository repository(db_path.string());
  std::string error;
  if (!repository.open(&error)) {
    std::cerr << "failed to open database: " << error << '\n';
    return 1;
  }
  if (!repository.initialize_schema(&error)) {
    std::cerr << "failed to initialize schema: " << error << '\n';
    return 1;
  }

  rankmatch::ranking::RankingBoard ranking_board(repository);
  rankmatch::match::MatchManager match_manager(repository, ranking_board);
  if (!match_manager.load_pending_matches(&error)) {
    std::cerr << "failed to load pending matches: " << error << '\n';
    return 1;
  }
  rankmatch::reward::RewardManager reward_manager(repository, ranking_board);
  rankmatch::protocol::CommandDispatcher dispatcher(ranking_board, match_manager, reward_manager);

  std::cout << "cpp-rank-match-service started" << '\n';
  std::cout << "db_path=" << db_path.string() << '\n';
  std::cout << "type HELP to show available commands" << '\n';

  std::string line;
  while (true) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }
    const std::string output = dispatcher.handle_command(line);
    if (!output.empty()) {
      std::cout << output << '\n';
    }
    if (dispatcher.should_exit()) {
      break;
    }
  }

  return 0;
}

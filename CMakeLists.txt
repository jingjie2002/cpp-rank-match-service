cmake_minimum_required(VERSION 3.20)

project(cpp-rank-match-service LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cpp-rank-match-service
  src/cache/redis_ranking_cache.cpp
  src/main.cpp
  src/match/match_manager.cpp
  src/protocol/command_dispatcher.cpp
  src/ranking/ranking_board.cpp
  src/reward/reward_manager.cpp
  src/storage/storage_repository.cpp
  third_party/sqlite/sqlite3.c
)

target_include_directories(cpp-rank-match-service PRIVATE
  include
  third_party/sqlite
)

target_compile_definitions(cpp-rank-match-service PRIVATE
  SQLITE_THREADSAFE=0
  SQLITE_OMIT_LOAD_EXTENSION
)

if (MSVC)
  target_compile_options(cpp-rank-match-service PRIVATE /W4 /permissive-)
  target_compile_definitions(cpp-rank-match-service PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(cpp-rank-match-service PRIVATE ws2_32)
else()
  target_compile_options(cpp-rank-match-service PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(cpp-rank-match-service PRIVATE dl pthread)
endif()

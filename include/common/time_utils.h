#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace rankmatch::common {

inline std::string now_string() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm tm_snapshot{};
#ifdef _WIN32
  localtime_s(&tm_snapshot, &now_time);
#else
  localtime_r(&now_time, &tm_snapshot);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm_snapshot, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

inline long long wait_seconds(const std::chrono::steady_clock::time_point& joined_at) {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now() - joined_at)
      .count();
}

}  // namespace rankmatch::common

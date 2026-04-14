#include "cache/redis_ranking_cache.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace rankmatch::cache {

namespace {

#ifdef _WIN32
using native_socket_type = SOCKET;
constexpr native_socket_type kInvalidSocket = INVALID_SOCKET;

bool ensure_wsa_started(std::string* error) {
  static bool initialized = false;
  if (initialized) {
    return true;
  }

  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    if (error != nullptr) {
      *error = "WSAStartup failed";
    }
    return false;
  }
  initialized = true;
  return true;
}

void close_native_socket(native_socket_type socket_handle) {
  if (socket_handle != kInvalidSocket) {
    closesocket(socket_handle);
  }
}

std::string socket_error_string() {
  return "winsock error code " + std::to_string(WSAGetLastError());
}
#else
using native_socket_type = int;
constexpr native_socket_type kInvalidSocket = -1;

bool ensure_wsa_started(std::string*) {
  return true;
}

void close_native_socket(native_socket_type socket_handle) {
  if (socket_handle != kInvalidSocket) {
    close(socket_handle);
  }
}

std::string socket_error_string() {
  return std::strerror(errno);
}
#endif

native_socket_type cast_socket(long long value) {
  return static_cast<native_socket_type>(value);
}

}  // namespace

RedisRankingCache::RedisRankingCache(std::string host, int port, std::string key)
    : host_(std::move(host)), port_(port), key_(std::move(key)) {}

RedisRankingCache::~RedisRankingCache() {
  close_connection();
}

bool RedisRankingCache::connect(std::string* error) {
  if (!ensure_wsa_started(error)) {
    return false;
  }
  if (connected_) {
    return true;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result = nullptr;
  const std::string port_string = std::to_string(port_);
  if (getaddrinfo(host_.c_str(), port_string.c_str(), &hints, &result) != 0) {
    if (error != nullptr) {
      *error = "getaddrinfo failed for redis host";
    }
    return false;
  }

  native_socket_type connected_socket = kInvalidSocket;
  for (addrinfo* cursor = result; cursor != nullptr; cursor = cursor->ai_next) {
    connected_socket = ::socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
    if (connected_socket == kInvalidSocket) {
      continue;
    }
    if (::connect(connected_socket, cursor->ai_addr, static_cast<int>(cursor->ai_addrlen)) == 0) {
      break;
    }
    close_native_socket(connected_socket);
    connected_socket = kInvalidSocket;
  }
  freeaddrinfo(result);

  if (connected_socket == kInvalidSocket) {
    if (error != nullptr) {
      *error = "failed to connect redis: " + socket_error_string();
    }
    return false;
  }

  socket_ = static_cast<long long>(connected_socket);
  connected_ = true;
  return true;
}

bool RedisRankingCache::available() const {
  return connected_;
}

bool RedisRankingCache::rebuild(const std::vector<model::PlayerInfo>& players, std::string* error) {
  auto response = execute({"DEL", key_}, error);
  if (response.type == RespValue::Type::kError) {
    if (error != nullptr && error->empty()) {
      *error = response.string_value;
    }
    return false;
  }

  for (const auto& player : players) {
    if (!upsert_score(player.player_id, player.score, error)) {
      return false;
    }
  }
  return true;
}

bool RedisRankingCache::upsert_score(const std::string& player_id, int score, std::string* error) {
  auto response = execute({"ZADD", key_, std::to_string(score), player_id}, error);
  if (response.type == RespValue::Type::kError) {
    if (error != nullptr && error->empty()) {
      *error = response.string_value;
    }
    return false;
  }
  return true;
}

std::vector<std::pair<std::string, int>> RedisRankingCache::top_n(int n, std::string* error) {
  std::vector<std::pair<std::string, int>> result;
  auto response =
      execute({"ZREVRANGE", key_, "0", std::to_string(std::max(0, n - 1)), "WITHSCORES"}, error);
  if (response.type == RespValue::Type::kError) {
    if (error != nullptr && error->empty()) {
      *error = response.string_value;
    }
    return result;
  }
  if (response.type != RespValue::Type::kArray) {
    if (error != nullptr) {
      *error = "unexpected redis response for ZREVRANGE";
    }
    return result;
  }

  for (std::size_t index = 0; index + 1 < response.array_values.size(); index += 2) {
    const auto& member = response.array_values[index];
    const auto& score = response.array_values[index + 1];
    if ((member.type == RespValue::Type::kBulkString ||
         member.type == RespValue::Type::kSimpleString) &&
        (score.type == RespValue::Type::kBulkString ||
         score.type == RespValue::Type::kSimpleString)) {
      result.emplace_back(member.string_value, std::stoi(score.string_value));
    }
  }
  return result;
}

std::optional<int> RedisRankingCache::rank_of(const std::string& player_id, std::string* error) {
  auto response = execute({"ZREVRANK", key_, player_id}, error);
  if (response.type == RespValue::Type::kNil) {
    return std::nullopt;
  }
  if (response.type == RespValue::Type::kError) {
    if (error != nullptr && error->empty()) {
      *error = response.string_value;
    }
    return std::nullopt;
  }
  if (response.type != RespValue::Type::kInteger) {
    if (error != nullptr) {
      *error = "unexpected redis response for ZREVRANK";
    }
    return std::nullopt;
  }
  return static_cast<int>(response.integer_value + 1);
}

bool RedisRankingCache::ensure_connected(std::string* error) {
  if (connected_) {
    return true;
  }
  return this->connect(error);
}

void RedisRankingCache::close_connection() {
  if (!connected_) {
    return;
  }
  close_native_socket(cast_socket(socket_));
  socket_ = -1;
  connected_ = false;
}

RedisRankingCache::RespValue RedisRankingCache::execute(const std::vector<std::string>& parts,
                                                        std::string* error) {
  if (!ensure_connected(error)) {
    return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis not connected"};
  }

  const std::string request = build_command(parts);
  if (!send_all(request, error)) {
    close_connection();
    return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis send failed"};
  }

  RespValue response = read_resp_value(error);
  if (response.type == RespValue::Type::kError) {
    if (error != nullptr && error->empty()) {
      *error = response.string_value;
    }
  }
  return response;
}

bool RedisRankingCache::send_all(const std::string& request, std::string* error) {
  std::size_t offset = 0;
  const char* buffer = request.data();
  const native_socket_type socket_handle = cast_socket(socket_);
  while (offset < request.size()) {
#ifdef _WIN32
    const int sent = ::send(socket_handle, buffer + offset,
                            static_cast<int>(request.size() - offset), 0);
#else
    const ssize_t sent =
        ::send(socket_handle, buffer + offset, request.size() - offset, 0);
#endif
    if (sent <= 0) {
      if (error != nullptr) {
        *error = "redis send failed: " + socket_error_string();
      }
      return false;
    }
    offset += static_cast<std::size_t>(sent);
  }
  return true;
}

bool RedisRankingCache::read_exact(std::size_t size, std::string* error, std::string* out) {
  out->clear();
  out->reserve(size);
  const native_socket_type socket_handle = cast_socket(socket_);
  while (out->size() < size) {
    char buffer[512];
    const std::size_t wanted = std::min<std::size_t>(sizeof(buffer), size - out->size());
#ifdef _WIN32
    const int received = ::recv(socket_handle, buffer, static_cast<int>(wanted), 0);
#else
    const ssize_t received = ::recv(socket_handle, buffer, wanted, 0);
#endif
    if (received <= 0) {
      if (error != nullptr) {
        *error = "redis recv failed: " + socket_error_string();
      }
      return false;
    }
    out->append(buffer, static_cast<std::size_t>(received));
  }
  return true;
}

bool RedisRankingCache::read_line(std::string* error, std::string* out) {
  out->clear();
  const native_socket_type socket_handle = cast_socket(socket_);
  while (true) {
    char c = '\0';
#ifdef _WIN32
    const int received = ::recv(socket_handle, &c, 1, 0);
#else
    const ssize_t received = ::recv(socket_handle, &c, 1, 0);
#endif
    if (received <= 0) {
      if (error != nullptr) {
        *error = "redis recv line failed: " + socket_error_string();
      }
      return false;
    }
    if (c == '\r') {
      char lf = '\0';
#ifdef _WIN32
      const int next_received = ::recv(socket_handle, &lf, 1, 0);
#else
      const ssize_t next_received = ::recv(socket_handle, &lf, 1, 0);
#endif
      if (next_received <= 0 || lf != '\n') {
        if (error != nullptr) {
          *error = "invalid redis line ending";
        }
        return false;
      }
      return true;
    }
    out->push_back(c);
  }
}

RedisRankingCache::RespValue RedisRankingCache::read_resp_value(std::string* error) {
  std::string prefix_buffer;
  if (!read_exact(1, error, &prefix_buffer)) {
    return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
  }

  const char prefix = prefix_buffer[0];
  std::string line;
  switch (prefix) {
    case '+':
      if (!read_line(error, &line)) {
        return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
      }
      return RespValue{RespValue::Type::kSimpleString, line};
    case '-':
      if (!read_line(error, &line)) {
        return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
      }
      return RespValue{RespValue::Type::kError, line};
    case ':':
      if (!read_line(error, &line)) {
        return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
      }
      return RespValue{RespValue::Type::kInteger, {}, std::stoll(line)};
    case '$': {
      if (!read_line(error, &line)) {
        return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
      }
      const long long length = std::stoll(line);
      if (length < 0) {
        return RespValue{RespValue::Type::kNil};
      }
      std::string payload;
      if (!read_exact(static_cast<std::size_t>(length) + 2, error, &payload)) {
        return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
      }
      payload.resize(static_cast<std::size_t>(length));
      return RespValue{RespValue::Type::kBulkString, payload};
    }
    case '*': {
      if (!read_line(error, &line)) {
        return RespValue{RespValue::Type::kError, error != nullptr ? *error : "redis read failed"};
      }
      const long long length = std::stoll(line);
      if (length < 0) {
        return RespValue{RespValue::Type::kNil};
      }
      RespValue value;
      value.type = RespValue::Type::kArray;
      for (long long index = 0; index < length; ++index) {
        value.array_values.push_back(read_resp_value(error));
      }
      return value;
    }
    default:
      if (error != nullptr) {
        *error = "unknown redis response prefix";
      }
      return RespValue{RespValue::Type::kError, error != nullptr ? *error : "unknown redis response"};
  }
}

std::string RedisRankingCache::build_command(const std::vector<std::string>& parts) {
  std::ostringstream oss;
  oss << "*" << parts.size() << "\r\n";
  for (const auto& part : parts) {
    oss << "$" << part.size() << "\r\n" << part << "\r\n";
  }
  return oss.str();
}

}  // namespace rankmatch::cache

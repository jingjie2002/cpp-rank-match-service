# cpp-rank-match-service

`cpp-rank-match-service` 是一个 C++17 命令行服务示例，聚焦匹配队列、排行榜、赛季结算和奖励领取状态。项目使用 SQLite 保存玩家、对局和奖励数据，可选使用 Redis 作为排行榜热数据缓存。

它不是完整游戏服务器，而是一个用于演示“玩家注册 -> 匹配 -> 对局结算 -> 排行更新 -> 赛季奖励 -> 领取奖励”这条基础链路的小型服务。

## 功能

- 玩家注册和基础积分记录。
- 加入、取消和查看匹配队列。
- 按分数窗口撮合玩家。
- 提交对局胜者并更新双方积分。
- 查询 TopN 排行榜和单个玩家排名。
- 按当前排行榜生成赛季奖励。
- 查询和领取奖励，已领取奖励不能重复领取。
- 按 SQLite 当前积分重建 Redis 榜单缓存。
- Redis 不可用时回退到 SQLite 查询。

## 技术栈

- C++17
- CMake
- SQLite
- Redis RESP 简易客户端
- Python demo script

## 目录结构

```text
include/                 头文件
src/                     C++ 源码
third_party/sqlite/      SQLite 源码
scripts/demo_flow.py     本地演示脚本
docs/architecture.md     架构说明
CMakeLists.txt           构建配置
```

核心模块：

| 模块 | 说明 |
|---|---|
| `MatchManager` | 维护等待队列、扩圈规则和待完成对局 |
| `RankingBoard` | 维护玩家积分、TopN 和个人排名 |
| `RewardManager` | 负责赛季结算、奖励生成和奖励领取 |
| `StorageRepository` | 负责 SQLite 落库 |
| `RedisRankingCache` | 负责排行榜热数据缓存 |
| `CommandDispatcher` | 负责命令解析和输出 |

## 构建

### Windows / Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

运行：

```powershell
.\build\Debug\cpp-rank-match-service.exe .\data\rank_match.db
```

如果本机安装了 Ninja，也可以使用：

```powershell
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja
.\build-ninja\cpp-rank-match-service.exe .\data\rank_match.db
```

### Linux / g++

```bash
cmake -S . -B build
cmake --build build
./build/cpp-rank-match-service ./data/rank_match.db
```

## 命令

```text
HELP
REGISTER_PLAYER <player_id> <nickname> <score>
SHOW_PLAYERS
JOIN_MATCH <player_id>
CANCEL_MATCH <player_id>
SHOW_QUEUE
RUN_MATCH
SHOW_MATCHES
FINISH_MATCH <match_id> <winner_id>
TOP <n>
RANK <player_id>
REBUILD_RANK_CACHE
SETTLE_SEASON
LIST_REWARDS <player_id>
CLAIM_REWARD <player_id> <reward_id>
EXIT
```

## Demo

Windows Debug 构建：

```powershell
python .\scripts\demo_flow.py .\build\Debug\cpp-rank-match-service.exe
```

Ninja 构建：

```powershell
python .\scripts\demo_flow.py .\build-ninja\cpp-rank-match-service.exe
```

演示脚本会自动完成：

1. 注册 4 个玩家。
2. 两组玩家进入匹配队列。
3. 执行匹配并提交对局结果。
4. 查询排行榜。
5. 生成赛季奖励。
6. 查询并领取奖励。
7. 再次领取同一奖励并验证失败。

更多验证说明见：[Verification](docs/verification.md)。

## 规则说明

匹配规则：

- 基础分差窗口：`50`
- 每等待 `1` 秒，窗口增加：`80`
- 以双方中更大的窗口判断是否能撮合

积分与奖励：

- 胜者基础加分：`30`
- 若胜者初始分低于负者，会获得额外补偿分
- 当前赛季奖励发给前 `3` 名
- 奖励档位：`300 / 200 / 100`
- 奖励领取状态保存在 SQLite 中

## 数据职责

SQLite 保存：

- 玩家积分
- 对局记录
- 赛季记录
- 奖励记录与领取状态

Redis 保存：

- 榜单热数据
- TopN 查询数据
- 个人排名查询数据

当前 Redis key：

```text
cpp-rank-match:rank:global
```

## 当前限制

- 不包含网关或前端页面。
- 不包含完整战斗逻辑。
- 不包含复杂二进制协议或 protobuf。
- 不包含多服务拆分。
- Redis 是可选缓存层，不是唯一数据源。
- 当前服务主要面向本地运行和业务流程演示。

## License

未指定。

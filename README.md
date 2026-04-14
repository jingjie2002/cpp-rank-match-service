# cpp-rank-match-service

`cpp-rank-match-service` 是一个面向 4399 C++ 游戏服务端方向补位的第三项目，当前聚焦：

- 匹配队列
- 排行榜维护
- 赛季结算
- 奖励领取防重复
- SQLite 持久化
- Redis 排行热数据层

它不是完整商业服，而是一个能把“匹配 -> 对局结果 -> 排行更新 -> 结算 -> 领奖励”这条链路跑通、讲清楚的 C++ 小型服务。

## 当前能力

- `REGISTER_PLAYER`：注册或更新玩家初始分数
- `JOIN_MATCH` / `CANCEL_MATCH`：进入或退出匹配队列
- `RUN_MATCH`：按分数窗口尝试撮合玩家
- `FINISH_MATCH`：提交对局胜者并回写积分
- `TOP` / `RANK`：查询榜单和个人排名，优先走 Redis
- `SETTLE_SEASON`：按当前榜单前 3 名生成赛季奖励
- `LIST_REWARDS` / `CLAIM_REWARD`：查看和领取奖励，已领取奖励不能重复领取
- `REBUILD_RANK_CACHE`：按 SQLite 当前积分重建 Redis 榜单缓存

## 设计边界

当前版本刻意不做这些内容：

- 网关
- 前端页面
- 微服务拆分
- protobuf 或复杂二进制协议
- 完整战斗逻辑

这版优先保证两件事：

1. C++ 状态组织和数据结构是清楚的
2. 整条业务链路可运行、可回归、可写进简历

## 核心模块

- `MatchManager`
  维护等待队列、扩圈规则和待完成对局
- `RankingBoard`
  维护玩家积分、TopN 和个人排名，并优先走 Redis 榜单缓存
- `RewardManager`
  负责赛季结算、奖励生成和奖励领取
- `StorageRepository`
  负责 SQLite 落库：玩家、对局、赛季、奖励
- `RedisRankingCache`
  负责排行榜热数据缓存与缓存重建
- `CommandDispatcher`
  负责命令解析和统一输出

## 匹配规则

当前版本使用简单的动态扩圈策略：

- 基础分差窗口：`50`
- 每等待 `1` 秒，窗口增加：`80`
- 以双方中更大的窗口为准决定是否能撮合

这样既能让分数接近的玩家快速匹配，也能让等待更久的玩家逐步扩大可匹配范围。

## 积分与奖励规则

- 默认按胜负结果更新积分
- 胜者基础加分：`30`
- 若胜者初始分低于负者，会获得额外补偿分
- 当前赛季奖励只发给前 `3` 名
- 奖励档位：`300 / 200 / 100`
- 奖励领取使用数据库状态控制，避免重复领取

## 数据职责拆分

- `SQLite`
  - 玩家积分真相源
  - 对局记录
  - 赛季记录
  - 奖励记录与领取状态
- `Redis`
  - 榜单热数据
  - TopN 查询
  - 个人排名查询

当前 Redis key 使用：

- `cpp-rank-match:rank:global`

启动时会优先尝试连接 Redis，并按 SQLite 当前积分重建榜单缓存；如果 Redis 不可用，项目仍可回退到 SQLite 正常运行。

## 目录结构

```text
cpp-rank-match-service/
  CMakeLists.txt
  README.md
  docs/
  include/
  scripts/
  src/
  third_party/sqlite/
```

## 构建方式

### Windows / Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

运行：

```powershell
.\build\Debug\cpp-rank-match-service.exe .\data\rank_match.db
```

### Linux / g++

```bash
cmake -S . -B build
cmake --build build
./build/cpp-rank-match-service ./data/rank_match.db
```

## 命令说明

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

## 演示脚本

```powershell
python .\scripts\demo_flow.py .\build\Debug\cpp-rank-match-service.exe
```

脚本会自动完成：

1. 注册 4 个玩家
2. 让两组玩家进入匹配队列
3. 提交两场对局结果并更新积分
4. 查询排行榜
5. 执行赛季结算
6. 查询并领取奖励

## 当前项目定位

这个项目后面最适合写进简历的中文名是：

`实时匹配、排行榜与奖励结算服务（C++）`

它和 `cpp-room-server` 的区别是：

- `cpp-room-server` 更强调连接、房间、广播、心跳和重连
- 这个项目更强调匹配、排行、Redis 热榜、结算、奖励和状态持久化

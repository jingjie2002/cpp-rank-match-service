# 架构说明

## 目标

这个项目只解决一条最小业务链路：

`玩家入队 -> 匹配成功 -> 记录对局 -> 更新积分 -> 查询排行榜 -> 赛季结算 -> 领取奖励`

它的重点不是高并发，而是把状态组织、数据落库和结果回查说明白。

## 模块拆分

### MatchManager

职责：

- 维护匹配队列
- 控制玩家入队和退队
- 根据等待时间动态扩圈
- 生成待完成对局

当前匹配状态放在内存里，待完成对局会同步写入 SQLite，避免进程退出后完全丢失上下文。

### RankingBoard

职责：

- 管理玩家积分
- 提供 TopN 查询
- 提供个人排名查询
- 根据胜负关系计算积分变化

当前积分规则刻意保持简单，方便在面试里把设计取舍讲清楚。

### RewardManager

职责：

- 读取当前赛季号
- 对当前排行榜做结算
- 为前 3 名生成奖励记录
- 处理奖励领取和重复领取拦截

### StorageRepository

职责：

- 初始化 SQLite 表结构
- 落库玩家、对局、赛季和奖励
- 提供原子化的对局完成更新
- 维护奖励领取状态

## 数据表

### players

- `player_id`
- `nickname`
- `score`
- `updated_at`

### matches

- `match_id`
- `player_a`
- `player_b`
- `score_a_before`
- `score_b_before`
- `winner_id`
- `status`
- `created_at`
- `finished_at`

### seasons

- `season_id`
- `settled_at`

### rewards

- `reward_id`
- `season_id`
- `player_id`
- `rank_no`
- `reward_coin`
- `claimed`
- `created_at`
- `claimed_at`

### meta

- `current_season`

## 当前取舍

### 为什么先用 SQLite

因为这版更看重：

- 先把状态组织清楚
- 先把结算和奖励链路跑通
- 让项目在本机和 GitHub 上都能快速复现

SQLite 足够承接当前版本的：

- 玩家积分
- 对局记录
- 奖励记录
- 领取状态

后续如果需要再扩成 Redis 热榜 + DB 落库，也更自然。

### 为什么先做命令驱动

这版没有先做 HTTP 或网关，因为今晚更重要的是：

- 让 C++ 主链路先成型
- 让数据结构和业务状态先能讲
- 让项目能稳定演示

命令驱动版更容易快速回归，也更适合把业务链路压实。

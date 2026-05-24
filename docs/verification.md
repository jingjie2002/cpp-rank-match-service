# Verification

This document records the local verification flow for `cpp-rank-match-service`.

## Environment

Verified on Windows with:

- Visual Studio 2022 C++ toolchain
- CMake
- Ninja
- Python 3

The project can also be built with the Visual Studio generator, but the local shell environment used for this verification had duplicated `PATH` / `Path` variables. Using Ninja avoided the MSBuild environment issue.

## Build

Open a Visual Studio developer command prompt or initialize the VS build environment, then run:

```powershell
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja
```

Expected result:

```text
cpp-rank-match-service.exe
```

is generated under:

```text
build-ninja/
```

## Demo

Run:

```powershell
python scripts\demo_flow.py .\build-ninja\cpp-rank-match-service.exe
```

The script uses a fresh SQLite database:

```text
data/demo.db
```

The database path is passed as a relative path from the repository root. This keeps the demo working when the repository path contains non-ASCII characters.

## Covered Flow

The demo verifies:

1. Register four players.
2. Put two pairs of players into the match queue.
3. Run matching twice.
4. Finish two matches and update scores.
5. Query the leaderboard.
6. Settle season rewards.
7. List rewards for the top player.
8. Claim the first reward.
9. Try to claim the same reward again.
10. Confirm the reward state is marked as claimed.

## Expected Output Fragments

The script checks these fragments:

```text
OK RUN_MATCH match_id=1
OK FINISH_MATCH match_id=1 winner=p2
OK RUN_MATCH match_id=2
OK FINISH_MATCH match_id=2 winner=p4
OK SETTLE_SEASON season=1 rewards=3
OK CLAIM_REWARD reward_id=1 coin=300
ERR reward already claimed
claimed=YES
```

If any fragment is missing, `demo_flow.py` exits with a non-zero status.

## Current Boundaries

- The service is a command-line demo service, not a network gateway.
- Redis is used as an optional ranking cache. SQLite remains the data source for players, matches, seasons, and reward claim state.
- The match queue is intentionally simple and exists to produce match results for ranking and reward verification.
- The project does not implement full battle logic, account authentication, payment, inventory, or production deployment.

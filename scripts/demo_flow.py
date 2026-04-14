import pathlib
import subprocess
import sys
import time


def send(proc: subprocess.Popen, command: str, delay: float = 0.1) -> None:
    print(f">>> {command}")
    proc.stdin.write(command + "\n")
    proc.stdin.flush()
    time.sleep(delay)


def main() -> int:
    if len(sys.argv) > 1:
        exe = pathlib.Path(sys.argv[1]).resolve()
    else:
        exe = pathlib.Path(__file__).resolve().parents[1] / "build" / "Debug" / "cpp-rank-match-service.exe"

    db_path = pathlib.Path(__file__).resolve().parents[1] / "data" / "demo.db"
    db_path.parent.mkdir(parents=True, exist_ok=True)
    if db_path.exists():
        db_path.unlink()

    proc = subprocess.Popen(
        [str(exe), str(db_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    commands = [
        "REGISTER_PLAYER p1 Alice 1200",
        "REGISTER_PLAYER p2 Bob 1220",
        "REGISTER_PLAYER p3 Carol 1400",
        "REGISTER_PLAYER p4 Dave 1470",
        "JOIN_MATCH p1",
        "JOIN_MATCH p2",
        "RUN_MATCH",
        "FINISH_MATCH 1 p2",
        "TOP 5",
        "JOIN_MATCH p3",
        "JOIN_MATCH p4",
    ]

    for command in commands:
        send(proc, command)

    time.sleep(1.2)

    tail_commands = [
        "RUN_MATCH",
        "FINISH_MATCH 2 p4",
        "TOP 5",
        "SETTLE_SEASON",
        "LIST_REWARDS p4",
        "CLAIM_REWARD p4 1",
        "LIST_REWARDS p4",
        "EXIT",
    ]

    for command in tail_commands:
        send(proc, command)

    proc.stdin.close()
    output = proc.stdout.read()
    proc.wait()
    print(output)
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())

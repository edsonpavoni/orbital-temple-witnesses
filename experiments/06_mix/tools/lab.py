#!/usr/bin/env python3
"""
Serial bridge for the position-lab firmware.

The bridge process owns the serial port: it reads the firmware's compressed
delta log into a file, and forwards commands written to a FIFO to the motor.
This lets a higher-level orchestrator (Claude, a shell script, the user) send
commands AND watch the log without two processes fighting for the port.

Subcommands:
  start        Run the bridge in the foreground (use with run_in_background).
  send "cmd"   Write a command line to the bridge's FIFO.
  tail [N]     Print the last N lines of the bridge's log (default 80).
  log          Print the entire log.
  clear        Truncate the log file.
  status       Show whether the bridge is running.
  stop         SIGTERM the bridge.
  run FILE     Execute a test plan (one cmd per line; "wait <sec>" lines pause).

Defaults assume the M5Stack Roller485 + XIAO ESP32-S3 setup at 115200 baud.
"""

import argparse
import os
import signal
import sys
import threading
import time
from pathlib import Path

import serial


PORT_DEFAULT = "/dev/cu.usbmodem101"
BAUD = 115200

# Use /tmp so multiple folders / multiple labs can coexist via different prefixes.
PREFIX = "/tmp/lab_pos"
FIFO = f"{PREFIX}.fifo"
LOG = f"{PREFIX}.log"
PIDFILE = f"{PREFIX}.pid"


# ---------- bridge process ----------

def _bridge_main(port: str) -> int:
    # Refuse to start twice.
    if _bridge_pid_alive():
        print(f"bridge already running (pid {Path(PIDFILE).read_text().strip()})",
              file=sys.stderr)
        return 1

    # Try to make the FIFO. If a stale one is there, recreate.
    try:
        os.mkfifo(FIFO)
    except FileExistsError:
        os.remove(FIFO)
        os.mkfifo(FIFO)

    Path(PIDFILE).write_text(str(os.getpid()))

    try:
        ser = serial.Serial(port, BAUD, timeout=0.1)
    except serial.SerialException as e:
        print(f"could not open {port}: {e}", file=sys.stderr)
        _cleanup()
        return 2

    log = open(LOG, "a", buffering=1)  # line-buffered
    log.write(f"\n=== bridge start {time.strftime('%Y-%m-%d %H:%M:%S')} "
              f"port={port} pid={os.getpid()} ===\n")

    stop = threading.Event()

    def reader():
        while not stop.is_set():
            try:
                chunk = ser.readline()
            except Exception as e:
                log.write(f"[reader error] {e}\n")
                stop.set()
                return
            if chunk:
                try:
                    log.write(chunk.decode("utf-8", errors="replace"))
                except Exception as e:
                    log.write(f"[decode error] {e}\n")

    t = threading.Thread(target=reader, daemon=True)
    t.start()

    def shutdown(*_):
        stop.set()

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    try:
        while not stop.is_set():
            # Open FIFO blocking; closes when writer disconnects. Loop and reopen.
            try:
                with open(FIFO, "r") as f:
                    for line in f:
                        line = line.rstrip("\r\n")
                        if not line:
                            continue
                        log.write(f"[->] {line}\n")
                        ser.write((line + "\n").encode())
                        ser.flush()
                        if stop.is_set():
                            break
            except Exception as e:
                log.write(f"[fifo error] {e}\n")
                time.sleep(0.5)
    finally:
        stop.set()
        try:
            ser.close()
        except Exception:
            pass
        log.write(f"=== bridge stop {time.strftime('%Y-%m-%d %H:%M:%S')} ===\n")
        log.close()
        _cleanup()
    return 0


def _cleanup():
    for p in (FIFO, PIDFILE):
        try:
            os.remove(p)
        except FileNotFoundError:
            pass


def _bridge_pid_alive() -> bool:
    try:
        pid = int(Path(PIDFILE).read_text().strip())
    except (FileNotFoundError, ValueError):
        return False
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


# ---------- client subcommands ----------

def cmd_send(cmd: str) -> int:
    if not _bridge_pid_alive():
        print("bridge not running. start it first.", file=sys.stderr)
        return 1
    # Open FIFO non-blocking write would risk failure; the bridge is reading.
    with open(FIFO, "w") as f:
        f.write(cmd + "\n")
    return 0


def cmd_tail(n: int) -> int:
    if not Path(LOG).exists():
        print("(log empty)")
        return 0
    with open(LOG, "rb") as f:
        # Read last ~64KB and split — simple and fast for our log sizes.
        f.seek(0, os.SEEK_END)
        size = f.tell()
        chunk = 64 * 1024
        f.seek(max(0, size - chunk))
        data = f.read().decode("utf-8", errors="replace")
    lines = data.splitlines()
    for line in lines[-n:]:
        print(line)
    return 0


def cmd_log() -> int:
    if not Path(LOG).exists():
        return 0
    with open(LOG, "r", encoding="utf-8", errors="replace") as f:
        sys.stdout.write(f.read())
    return 0


def _read_session() -> str:
    """Return only the content from the most recent bridge session
    (everything after the last '=== bridge start ...' marker, or the whole
    file if no marker is present)."""
    if not Path(LOG).exists():
        return ""
    text = Path(LOG).read_text(encoding="utf-8", errors="replace")
    marker = "=== bridge start "
    idx = text.rfind(marker)
    if idx < 0:
        return text
    return text[idx:]


def cmd_session() -> int:
    """Print only the current bridge session's content. Saves tokens vs `log`."""
    sys.stdout.write(_read_session())
    return 0


def cmd_events(n: int) -> int:
    """Print the last N event lines (commands sent + firmware messages)
    from the current session. Skips delta-log lines. Cheapest way to see
    what happened in a ritual."""
    text = _read_session()
    events = [line for line in text.splitlines()
              if line.startswith(("[->]", ">>", "==="))]
    for line in events[-n:]:
        print(line)
    return 0


def cmd_clear() -> int:
    Path(LOG).write_text("")
    return 0


def cmd_status() -> int:
    alive = _bridge_pid_alive()
    pid = ""
    try:
        pid = Path(PIDFILE).read_text().strip()
    except FileNotFoundError:
        pass
    print(f"bridge: {'RUNNING' if alive else 'stopped'}"
          + (f" (pid {pid})" if alive else ""))
    print(f"fifo:   {FIFO} {'[exists]' if Path(FIFO).exists() else '[missing]'}")
    print(f"log:    {LOG}"
          + (f" ({Path(LOG).stat().st_size} bytes)" if Path(LOG).exists() else " [missing]"))
    return 0


def cmd_stop() -> int:
    try:
        pid = int(Path(PIDFILE).read_text().strip())
    except (FileNotFoundError, ValueError):
        print("no pid file; bridge probably not running")
        _cleanup()
        return 0

    # SIGTERM sets the stop flag in the bridge's signal handler.
    try:
        os.kill(pid, signal.SIGTERM)
    except OSError as e:
        print(f"could not signal {pid}: {e}", file=sys.stderr)
        _cleanup()
        return 1

    # Bridge is likely blocked inside `open(FIFO, "r")` waiting for a writer,
    # which won't release on a signal alone. Tickle the FIFO to unblock it.
    if Path(FIFO).exists():
        try:
            fd = os.open(FIFO, os.O_WRONLY | os.O_NONBLOCK)
            try:
                os.write(fd, b"\n")
            finally:
                os.close(fd)
        except OSError:
            pass

    # Wait up to 3 s for graceful shutdown.
    for _ in range(30):
        if not _bridge_pid_alive():
            break
        time.sleep(0.1)

    # Force kill if it's still hanging on something.
    if _bridge_pid_alive():
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
        time.sleep(0.2)

    _cleanup()
    return 0


def cmd_run(plan_path: str) -> int:
    """Execute a simple plan file.
    Each non-empty / non-comment line is either:
        wait <seconds>
        <serial command>     (forwarded verbatim to the motor)
    """
    if not _bridge_pid_alive():
        print("bridge not running. start it first.", file=sys.stderr)
        return 1
    with open(plan_path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("wait "):
                secs = float(line.split(None, 1)[1])
                time.sleep(secs)
                continue
            with open(FIFO, "w") as fifo:
                fifo.write(line + "\n")
    return 0


# ---------- argparse ----------

def main(argv):
    p = argparse.ArgumentParser(prog="lab.py",
                                description="Serial bridge for the position lab.")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("start", help="Run the bridge (use with run_in_background).")
    s.add_argument("--port", default=PORT_DEFAULT)

    s = sub.add_parser("send", help="Send one command line to the motor.")
    s.add_argument("text")

    s = sub.add_parser("tail", help="Show last N log lines.")
    s.add_argument("n", nargs="?", type=int, default=80)

    sub.add_parser("log", help="Print full log file (every session).")
    sub.add_parser("session", help="Print only current bridge session content.")

    s = sub.add_parser("events",
                       help="Last N event lines from current session "
                            "(commands + firmware msgs only, no delta-log).")
    s.add_argument("n", nargs="?", type=int, default=15)

    sub.add_parser("clear", help="Truncate the log file.")
    sub.add_parser("status", help="Bridge status.")
    sub.add_parser("stop", help="Stop the bridge.")

    s = sub.add_parser("run", help="Run a plan file (cmds + 'wait N' lines).")
    s.add_argument("plan")

    args = p.parse_args(argv)

    if args.cmd == "start":
        return _bridge_main(args.port)
    if args.cmd == "send":
        return cmd_send(args.text)
    if args.cmd == "tail":
        return cmd_tail(args.n)
    if args.cmd == "log":
        return cmd_log()
    if args.cmd == "session":
        return cmd_session()
    if args.cmd == "events":
        return cmd_events(args.n)
    if args.cmd == "clear":
        return cmd_clear()
    if args.cmd == "status":
        return cmd_status()
    if args.cmd == "stop":
        return cmd_stop()
    if args.cmd == "run":
        return cmd_run(args.plan)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

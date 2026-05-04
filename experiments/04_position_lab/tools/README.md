# tools/lab.py — serial bridge for automated tests

A small Python tool that owns the serial port to the Roller485 motor while
the position-lab firmware is running. It lets a user (or Claude) **send
commands and watch the log at the same time**, and run scripted test plans.

## Why this exists

The serial port can only be open in one process at a time. PlatformIO's
device monitor (the VS Code task you've been using) holds the port for
interactive use. If we want to automate `0` / `r` / `kp...` sequences and
capture the resulting log, we need a single process that:

- holds the port,
- streams every line from the firmware to a log file in real time, and
- accepts commands from elsewhere (a FIFO) and forwards them to the motor.

`lab.py` is that process plus a few helpers to drive it.

## Files it creates (in `/tmp`)

| Path | Purpose |
|------|---------|
| `/tmp/lab_pos.fifo` | Named pipe; commands written here get sent to the motor |
| `/tmp/lab_pos.log`  | Append-only log of everything the firmware printed plus an `[->] cmd` line for each command sent |
| `/tmp/lab_pos.pid`  | PID of the running bridge |

Cleaned up automatically on `stop`.

## Workflow

### 0. Release the VS Code monitor

If you have **PlatformIO Monitor** open in VS Code, close it (Ctrl+C in that
terminal, or kill the task). Otherwise the bridge can't open the port.

### 1. Start the bridge

Run this once. Keep it running in a terminal (or have Claude run it as a
background process):

```bash
~/.platformio/penv/bin/python tools/lab.py start
```

Bridge prints nothing on stdout; everything goes to `/tmp/lab_pos.log`.

> **Note for ESP32-S3 USB-CDC:** opening the port may or may not reset the
> board depending on the host driver. If the firmware is mid-ritual when the
> bridge opens, expect the motor to drop to its boot state (RELEASED, output
> off). Plan accordingly — start the bridge first, then `0`, then `r`.

### 2. Send commands

```bash
~/.platformio/penv/bin/python tools/lab.py send "0"      # zero at current pos
~/.platformio/penv/bin/python tools/lab.py send "r"      # start ritual
~/.platformio/penv/bin/python tools/lab.py send "kp15000000"
~/.platformio/penv/bin/python tools/lab.py send "goto90"
~/.platformio/penv/bin/python tools/lab.py send "x"      # release
```

Each command is forwarded verbatim with a newline appended.

### 3. Watch the log

```bash
~/.platformio/penv/bin/python tools/lab.py tail 50
~/.platformio/penv/bin/python tools/lab.py log     # full log
```

Or just `tail -f /tmp/lab_pos.log` in another terminal.

### 4. Run a test plan

A plan is a plain text file with one command per line, plus `wait <seconds>`
lines that pause the runner. Lines starting with `#` are comments.

Example: `plans/kp_sweep.txt`

```
# Establish zero (you must have the pointer at visual zero by hand first)
0
wait 1

# Step-response baseline at default PID
eoff
goto90
wait 4
goto0
wait 4

# Try a higher P
kp30000000
goto90
wait 4
goto0
wait 4

# Restore default and release
kp15000000
x
```

Run it:

```bash
~/.platformio/penv/bin/python tools/lab.py run plans/kp_sweep.txt
```

The plan runs in real time; when it finishes, `tools/lab.py log` (or
`tail`) shows the full captured timeline. Filter by command markers
(`grep -nE "^\[->\]|^t=" /tmp/lab_pos.log`) to align segments to commands.

### 5. Stop the bridge

```bash
~/.platformio/penv/bin/python tools/lab.py stop
```

Cleans up the FIFO and PID file.

## Subcommand reference

| Subcommand | Action |
|------------|--------|
| `start [--port /dev/cu.usbmodem101]` | Run the bridge in the foreground (use with `run_in_background`) |
| `send "<text>"` | Forward one line to the motor |
| `tail [N]` | Print last N lines of the log (default 80) |
| `log` | Print the entire log |
| `clear` | Truncate the log |
| `status` | Show whether the bridge is running |
| `stop` | SIGTERM the bridge |
| `run <plan>` | Execute a plan file |

## Limitations

- Single-port at a time. Only one bridge at once.
- macOS/Linux only (uses `os.mkfifo`).
- Plan format is intentionally minimal — no loops, no captures, no
  conditionals. For richer experiments, write a Python script that calls
  `lab.py send` between Python `time.sleep()` calls.
- Pyserial is required (`pip install pyserial` if not present;
  PlatformIO's `~/.platformio/penv/` already has it).

## Tips for tuning

When the bridge is running and I'm watching the log, a productive loop is:

1. `clear` before each experiment (so the log only contains that run).
2. `send "0"` and confirm `st=H,Tg=0` appears.
3. Set the PID / current / easing / mt parameters as one batch.
4. `send "goto90"` (or `r` for the ritual) and wait.
5. `tail 200` and look for: tracking error during the move (`er`),
   overshoot at the end (`er` flipping sign past `tr=-`), settling time
   (lines after `tr=-` until `er` < tolerance), steady-state offset.

That's the cycle.

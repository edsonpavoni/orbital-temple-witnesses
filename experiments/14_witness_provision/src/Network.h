// Network.h — Wi-Fi connect + NTP time sync + offline virtual clock.
//
// Three time regimes:
//
//   1. NTP synced. nowUTC() returns real UTC from the system clock.
//   2. No NTP yet, but a virtual clock has been anchored. nowUTC() returns
//      virtualBaseUTC + (millis() - virtualBaseMS)/1000. This is the
//      "replay a moment from history" mode — sculpture follows a cached
//      day's worth of motion, in real-time, looping forever.
//   3. No NTP and no virtual anchor. nowUTC() returns 0; tracker stays parked.
//
// SNTP runs in the background (configTime sets a 1 h auto-poll). Once it
// succeeds at any point, time(nullptr) returns a valid UTC — nowUTC()
// notices this on its next call and silently switches from virtual to real.
//
// Wi-Fi reconnect is throttled to once per hour when the link is down,
// per the artwork's "no excessive polling" policy.
//
// Credentials live in secrets.h (gitignored).
#pragma once

#include <Arduino.h>

namespace Network {

// Try to connect to WiFi using the given SSID + password for at most
// timeoutMs. Credentials come from the caller (typically read from NVS
// via WifiCreds). Returns true on success.
bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 20000);

// Sync time from pool.ntp.org. Returns true when system time looks valid.
// Schedules SNTP auto-poll (default 1 h) for the rest of the session.
bool syncTime(uint32_t timeoutMs = 15000);

bool isConnected();
bool isTimeSynced();          // true when time(nullptr) returns a sane UTC

// Seconds since UNIX epoch. Real UTC if NTP has ever synced this session;
// else virtual time if anchored; else 0.
uint32_t nowUTC();

// Replay-mode anchor: pretend "now" is utcSec, and let it advance in
// real-time from the moment of this call. Only takes effect while
// isTimeSynced() is false; once NTP succeeds, real UTC overrides.
void anchorVirtualUTC(uint32_t utcSec);

// True iff we're currently using the virtual clock (no real NTP yet).
bool isVirtualClock();

// Best-effort reconnect; called from the main loop. Throttled to once
// per hour to keep the radio quiet.
void tick();

}  // namespace Network

// Network.h — Wi-Fi connect + NTP time sync. Boot blocks here briefly to
// establish a connection (so the schedule fetch has time + Wi-Fi available
// when the calibration starts running). Reconnect attempts continue in
// the background if the link drops.
//
// Credentials live in secrets.h (gitignored).
#pragma once

#include <Arduino.h>

namespace Network {

// Try to connect to WiFi for at most timeoutMs. Returns true on success.
bool connect(uint32_t timeoutMs = 20000);

// Sync time from pool.ntp.org. Returns true when system time looks valid.
bool syncTime(uint32_t timeoutMs = 15000);

bool isConnected();
bool isTimeSynced();

// Seconds since UNIX epoch, or 0 if time has never been synced.
uint32_t nowUTC();

// Best-effort reconnect / re-sync; called from main loop occasionally.
void tick();

}  // namespace Network

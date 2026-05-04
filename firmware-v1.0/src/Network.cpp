#include "Network.h"

#include <WiFi.h>
#include <time.h>

namespace Network {

// Anchored once at boot when no NTP is available. virtualBaseUTC is the
// "pretend now" we want at virtualBaseMS (millis() at the anchor moment).
// Real time, when it later arrives, transparently overrides this.
static uint32_t s_virtualBaseUTC = 0;
static uint32_t s_virtualBaseMS  = 0;

// Try several rounds with the radio at max TX power. Weak signal at the
// sculpture's location sometimes needs more than one attempt — the chip's
// scan window may miss the AP on the first pass.
bool connect(const char* ssid, const char* password, uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!ssid || ssid[0] == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);                                // don't write SSID/PW to NVS each connect
  WiFi.setSleep(false);                                  // disable power save
  WiFi.setTxPower(WIFI_POWER_19_5dBm);                   // max for ESP32-S3

  const int kAttempts        = 4;
  const uint32_t kPerAttempt = timeoutMs;                // each try gets full timeoutMs

  for (int attempt = 1; attempt <= kAttempts; attempt++) {
    Serial.print(">> WiFi: attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.print(kAttempts);
    Serial.print(" — connecting to '");
    Serial.print(ssid);
    Serial.println("'...");

    WiFi.disconnect(true, true);                         // clean state + drop any stored creds
    delay(200);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - start > kPerAttempt) break;
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(">> WiFi: connected, IP=");
      Serial.print(WiFi.localIP());
      Serial.print(", RSSI=");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      return true;
    }

    wl_status_t st = WiFi.status();
    Serial.print(">> WiFi: attempt ");
    Serial.print(attempt);
    Serial.print(" failed (status=");
    Serial.print((int)st);
    Serial.println(")");
    delay(1500);
  }

  Serial.println(">> WiFi: all attempts failed. Scanning visible networks:");
  int nFound = WiFi.scanNetworks(false, true);            // sync, show hidden
  if (nFound <= 0) {
    Serial.println(">> WiFi: scan returned no networks");
  } else {
    for (int i = 0; i < nFound; i++) {
      Serial.print("    ");
      Serial.print(WiFi.SSID(i));
      Serial.print("  RSSI=");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm  ch=");
      Serial.println(WiFi.channel(i));
    }
  }
  WiFi.scanDelete();
  return false;
}

bool syncTime(uint32_t timeoutMs) {
  // Configure NTP. UTC only (no TZ offset, no DST) — we work in UTC end-to-end
  // and the schedule itself is UTC-keyed. configTime also schedules background
  // SNTP polls (default 1 h), so even if this initial sync times out the
  // system can pick up time later — nowUTC() detects that automatically.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(">> NTP: syncing...");
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (isTimeSynced()) {
      Serial.print(">> NTP: synced, UTC=");
      Serial.println((uint32_t)time(nullptr));
      return true;
    }
    delay(250);
  }
  Serial.println(">> NTP: sync timeout (will keep trying in background)");
  return false;
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

// Ground truth: time(nullptr) is sane (post-2023). True the moment SNTP
// succeeds, regardless of whether it was the boot-time sync or a later
// background poll.
bool isTimeSynced() {
  time_t t = time(nullptr);
  return t > 1700000000;
}

uint32_t nowUTC() {
  // Real UTC if NTP has set the system clock at any point this session.
  if (isTimeSynced()) {
    return (uint32_t)time(nullptr);
  }
  // Virtual clock fallback: replay-mode. Advances from the anchor point
  // using the internal millis() oscillator.
  if (s_virtualBaseUTC != 0) {
    uint32_t elapsedSec = (millis() - s_virtualBaseMS) / 1000;
    return s_virtualBaseUTC + elapsedSec;
  }
  return 0;
}

void anchorVirtualUTC(uint32_t utcSec) {
  s_virtualBaseUTC = utcSec;
  s_virtualBaseMS  = millis();
  Serial.print(">> Virtual UTC anchored at ");
  Serial.print(utcSec);
  Serial.println(" — replaying cached schedule from this point.");
}

bool isVirtualClock() {
  return !isTimeSynced() && s_virtualBaseUTC != 0;
}

void tick() {
  // Reconnect once per hour when Wi-Fi is down (per Edson: "no excessive
  // polling"). configTime's background SNTP handles re-syncing time once
  // the link is back, so we don't have to.
  static const uint32_t RETRY_PERIOD_MS = 60UL * 60UL * 1000UL;   // 1 h
  static uint32_t lastTryMS = 0;
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastTryMS > RETRY_PERIOD_MS) {
    lastTryMS = millis();
    Serial.println(">> WiFi: dropped, hourly reconnect attempt...");
    WiFi.reconnect();
  }
}

}  // namespace Network

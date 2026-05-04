#include "Network.h"

#include <WiFi.h>
#include <time.h>

#include "secrets.h"

namespace Network {

static bool s_timeSynced = false;

// Try several rounds with the radio at max TX power. Weak signal at the
// sculpture's location sometimes needs more than one attempt — the chip's
// scan window may miss the AP on the first pass.
bool connect(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
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
    Serial.print(WIFI_SSID);
    Serial.println("'...");

    WiFi.disconnect(true, true);                         // clean state + drop any stored creds
    delay(200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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
  // and the schedule itself is UTC-keyed.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(">> NTP: syncing...");
  uint32_t start = millis();
  time_t t = 0;
  while (millis() - start < timeoutMs) {
    t = time(nullptr);
    if (t > 1700000000) {     // sanity: any time after 2023-11-14
      s_timeSynced = true;
      Serial.print(">> NTP: synced, UTC=");
      Serial.println((uint32_t)t);
      return true;
    }
    delay(250);
  }
  Serial.println(">> NTP: sync timeout");
  return false;
}

bool isConnected()    { return WiFi.status() == WL_CONNECTED; }
bool isTimeSynced()   { return s_timeSynced; }

uint32_t nowUTC() {
  if (!s_timeSynced) return 0;
  time_t t = time(nullptr);
  return (uint32_t)t;
}

void tick() {
  // Reconnect best-effort if Wi-Fi drops. Don't block.
  static uint32_t lastTryMS = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastTryMS > 10000) {
    lastTryMS = millis();
    Serial.println(">> WiFi: dropped, reconnecting...");
    WiFi.reconnect();
  }
}

}  // namespace Network

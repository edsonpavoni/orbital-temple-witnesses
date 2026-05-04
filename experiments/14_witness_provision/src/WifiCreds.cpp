#include "WifiCreds.h"

#include <nvs.h>
#include <nvs_flash.h>

namespace {
// "wifi" / "nvs.net80211" are used by ESP-IDF's own Wi-Fi stack — using
// either name will sometimes silently interfere with our keys. Pick a
// project-unique namespace.
const char* NS       = "fwcreds";
const char* KEY_SSID = "ssid";
const char* KEY_PW   = "pw";
}

namespace WifiCreds {

bool loadFromNVS(String& ssid, String& pw) {
  nvs_handle_t h;
  esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
  if (err != ESP_OK) return false;

  size_t lenS = 0, lenP = 0;
  err = nvs_get_str(h, KEY_SSID, nullptr, &lenS);
  if (err != ESP_OK || lenS == 0 || lenS > 64) { nvs_close(h); return false; }
  err = nvs_get_str(h, KEY_PW, nullptr, &lenP);
  if (err != ESP_OK || lenP > 96)              { nvs_close(h); return false; }

  char bufS[65] = {0};
  char bufP[97] = {0};
  err = nvs_get_str(h, KEY_SSID, bufS, &lenS);
  if (err != ESP_OK)                           { nvs_close(h); return false; }
  err = nvs_get_str(h, KEY_PW,   bufP, &lenP);
  if (err != ESP_OK)                           { nvs_close(h); return false; }
  nvs_close(h);

  ssid = String(bufS);
  pw   = String(bufP);
  return ssid.length() > 0;
}

bool save(const String& ssid, const String& pw) {
  Serial.print(">> WifiCreds::save(): ssid_len=");
  Serial.print(ssid.length());
  Serial.print(" pw_len=");
  Serial.println(pw.length());

  nvs_handle_t h;
  esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
  if (err != ESP_OK) {
    Serial.print(">> WifiCreds::save: nvs_open err=");
    Serial.println(esp_err_to_name(err));
    return false;
  }

  err = nvs_set_str(h, KEY_SSID, ssid.c_str());
  if (err != ESP_OK) {
    Serial.print(">> WifiCreds::save: nvs_set_str(ssid) err=");
    Serial.println(esp_err_to_name(err));
    nvs_close(h);
    return false;
  }
  err = nvs_set_str(h, KEY_PW, pw.c_str());
  if (err != ESP_OK) {
    Serial.print(">> WifiCreds::save: nvs_set_str(pw) err=");
    Serial.println(esp_err_to_name(err));
    nvs_close(h);
    return false;
  }
  err = nvs_commit(h);
  nvs_close(h);
  if (err != ESP_OK) {
    Serial.print(">> WifiCreds::save: nvs_commit err=");
    Serial.println(esp_err_to_name(err));
    return false;
  }

  // Read-back sanity check.
  String chkS, chkP;
  if (!loadFromNVS(chkS, chkP) || chkS != ssid) {
    Serial.println(">> WifiCreds::save: read-back FAILED");
    return false;
  }
  Serial.print(">> WifiCreds::save: verified persisted, ssid='");
  Serial.print(chkS);
  Serial.println("'");
  return true;
}

void forget() {
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_erase_all(h);
  nvs_commit(h);
  nvs_close(h);
  Serial.println(">> WifiCreds: NVS cleared.");
}

}  // namespace WifiCreds

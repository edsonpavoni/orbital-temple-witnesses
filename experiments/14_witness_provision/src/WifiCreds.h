// WifiCreds.h — NVS-persisted Wi-Fi SSID + password, set by the
// captive-portal provisioning flow. Once a user enters their network
// credentials through the SoftAP setup page on first boot, they live
// here forever — no re-flash needed to change networks (the user can
// run the `forget_wifi` serial command to wipe NVS and re-provision).
//
// The hardcoded WIFI_SSID / WIFI_PASSWORD in secrets.h still serves as
// a developer-convenience fallback when NVS is empty AND the macros
// hold something other than the "PROVISION_ME" placeholder. In a
// production deployment, secrets.h has WIFI_SSID="PROVISION_ME" and
// the portal is the only way creds enter NVS.
#pragma once

#include <Arduino.h>

namespace WifiCreds {

// Read SSID + password from NVS into the out-params. True if a previously
// stored pair was found, false otherwise (out-params untouched on false).
bool loadFromNVS(String& ssid, String& pw);

// Persist SSID + password to NVS. Replaces any previous values.
bool save(const String& ssid, const String& pw);

// Wipe NVS — next boot will re-enter the provisioning portal (assuming
// secrets.h is the PROVISION_ME placeholder).
void forget();

}  // namespace WifiCreds

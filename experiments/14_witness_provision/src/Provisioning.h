// Provisioning.h — captive-portal Wi-Fi onboarding.
//
// When invoked, the sculpture raises a Wi-Fi SoftAP named after the
// per-unit SCULPTURE_NAME (e.g. "Orbital Witness 1/12"), runs a DNS
// hijack that redirects every hostname to its own IP, and serves a
// small HTML form on port 80 letting the user pick their home network
// and enter the password. Submitted credentials are written
// to NVS via WifiCreds::save(), and the sculpture reboots into normal
// station-mode operation.
//
// This blocks `setup()` until credentials are entered. The pointer
// stays disengaged (motor output off) the entire time — physically
// safe to handle.
#pragma once

#include <Arduino.h>

namespace Provisioning {

// Run the captive portal. Blocks until the user submits valid SSID +
// password, persists them to NVS, then triggers an ESP.restart().
// Caller never returns.
[[noreturn]] void runPortal();

}  // namespace Provisioning

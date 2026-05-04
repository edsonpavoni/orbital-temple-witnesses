// Witness foundation · Watchdog
// Stateless wrapper around esp_task_wdt. Namespace, not a class.

#pragma once

#include <stdint.h>

namespace Watchdog {
  // Arm WDT and add the current task. Call first thing in setup.
  void arm(uint32_t timeout_seconds);
  // Reset the WDT for the current task. Call from loop and any long path.
  void pet();
  // Soft restart. Wraps ESP.restart() so callers don't include esp_system.
  void rebootChip();
}

// Witness foundation · Watchdog implementation

#include "Watchdog.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

namespace Watchdog {

void arm(uint32_t timeout_seconds) {
  // Panic-on-timeout (true) so a wedge resets the chip rather than hanging.
  esp_task_wdt_init(timeout_seconds, true);
  esp_task_wdt_add(NULL);
}

void pet() {
  esp_task_wdt_reset();
}

void rebootChip() {
  ESP.restart();
}

}  // namespace Watchdog

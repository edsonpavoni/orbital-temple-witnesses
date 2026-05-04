// ScheduleClient.h — fetch a fresh schedule from /api/schedule and load it
// into a ScheduleStore. Streams the JSON response to keep RAM usage low
// (50 KB JSON would blow the heap if fully buffered).
#pragma once

#include <Arduino.h>

#include "ScheduleStore.h"

class ScheduleClient {
 public:
  // GET <SCHEDULE_URL_BASE>?lat=<lat>&lon=<lon>&hours=24
  // Parses JSON, calls store.replace() on success. Updates fetched_at_utc.
  // Returns true on success.
  bool fetch(ScheduleStore& store, float lat, float lon, uint32_t nowUTC);
};

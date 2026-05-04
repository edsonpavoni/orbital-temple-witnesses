#include "ScheduleClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

namespace {

// Parse an ISO 8601 UTC timestamp like "2026-04-30T00:00:00.000Z" into a
// uint32_t epoch second. Tolerates the optional fractional seconds.
bool parseIsoUTC(const char* s, uint32_t& out) {
  if (!s) return false;
  int Y, M, D, h, m, sec;
  // Accept both "...Z" and "...000Z" forms.
  int n = sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec);
  if (n != 6) return false;
  struct tm t = {};
  t.tm_year = Y - 1900;
  t.tm_mon  = M - 1;
  t.tm_mday = D;
  t.tm_hour = h;
  t.tm_min  = m;
  t.tm_sec  = sec;
  // mktime applies local TZ; we keep system TZ at UTC (Network::syncTime
  // calls configTime(0,0,...)) so this is effectively timegm.
  time_t e = mktime(&t);
  if (e == (time_t)-1) return false;
  out = (uint32_t)e;
  return true;
}

}  // namespace

bool ScheduleClient::fetch(ScheduleStore& store, float lat, float lon, uint32_t nowUTC) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(">> ScheduleClient: WiFi not connected");
    return false;
  }

  // Build URL. The webservice is on Firebase hosting (HTTPS only).
  String url = String(SCHEDULE_URL_BASE) +
               "?lat=" + String(lat, 4) +
               "&lon=" + String(lon, 4) +
               "&hours=24";
  Serial.print(">> ScheduleClient: GET ");
  Serial.println(url);

  WiFiClientSecure client;
  client.setInsecure();    // skip cert validation; ESP32 doesn't carry root CAs

  HTTPClient http;
  http.setTimeout(20000);
  http.setReuse(false);
  http.setConnectTimeout(15000);
  if (!http.begin(client, url)) {
    Serial.println(">> ScheduleClient: http.begin() failed");
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    Serial.print(">> ScheduleClient: HTTP ");
    Serial.println(code);
    http.end();
    return false;
  }
  int contentLen = http.getSize();
  Serial.print(">> ScheduleClient: HTTP 200, content-length=");
  Serial.println(contentLen);

  // Buffer the full body before parsing. Streaming parse was unreliable on
  // this Wi-Fi (TLS chunks would arrive slower than the JSON parser pulled,
  // surfacing as IncompleteInput). The body is ~51 KB; we have plenty of
  // heap.
  String body = http.getString();
  http.end();
  Serial.print(">> ScheduleClient: body bytes=");
  Serial.println(body.length());
  if (contentLen > 0 && (int)body.length() < contentLen) {
    Serial.println(">> ScheduleClient: body shorter than content-length, aborting");
    return false;
  }

  // Filter to keep only the fields we use; saves heap for the samples array.
  JsonDocument filter;
  filter["valid_from_utc"]       = true;
  filter["valid_until_utc"]      = true;
  filter["samples_interval_sec"] = true;
  filter["samples_count"]        = true;
  JsonObject sampleFilter = filter["samples"][0].to<JsonObject>();
  sampleFilter["az"] = true;
  sampleFilter["el"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));

  if (err) {
    Serial.print(">> ScheduleClient: parse error ");
    Serial.println(err.c_str());
    return false;
  }

  ScheduleStore::Header h{};
  const char* vfrom  = doc["valid_from_utc"]  | (const char*)nullptr;
  const char* vuntil = doc["valid_until_utc"] | (const char*)nullptr;
  if (!parseIsoUTC(vfrom,  h.valid_from_utc) ||
      !parseIsoUTC(vuntil, h.valid_until_utc)) {
    Serial.println(">> ScheduleClient: bad valid_from/until timestamps");
    return false;
  }
  h.samples_interval_sec = doc["samples_interval_sec"] | 60;
  h.fetched_at_utc       = nowUTC;

  JsonArray samples = doc["samples"].as<JsonArray>();
  size_t n = samples.size();
  if (n == 0 || n > ScheduleStore::MAX_SAMPLES) {
    Serial.print(">> ScheduleClient: bad sample count ");
    Serial.println((uint32_t)n);
    return false;
  }
  h.samples_count = (uint16_t)n;

  // Project samples into a dense Sample array. Heavy stack? ~12 KB for
  // 1440 samples — push to a stack-allocated buffer to avoid an extra
  // copy in the store.
  static ScheduleStore::Sample buf[ScheduleStore::MAX_SAMPLES];
  size_t i = 0;
  for (JsonObject s : samples) {
    if (i >= ScheduleStore::MAX_SAMPLES) break;
    buf[i].az = s["az"] | 0.0f;
    buf[i].el = s["el"] | 0.0f;
    i++;
  }

  if (!store.replace(h, buf, h.samples_count)) {
    Serial.println(">> ScheduleClient: store.replace() failed");
    return false;
  }

  Serial.print(">> ScheduleClient: stored ");
  Serial.print(h.samples_count);
  Serial.print(" samples, valid ");
  Serial.print(h.valid_from_utc);
  Serial.print(" .. ");
  Serial.println(h.valid_until_utc);
  return true;
}

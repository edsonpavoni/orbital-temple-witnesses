#include "Geolocation.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {
const char* NS         = "geo";
const char* KEY_LAT    = "lat";
const char* KEY_LON    = "lon";
const char* KEY_FETCHED = "fetched";

const char* GEO_URL = "https://ipapi.co/json/";
}

namespace Geolocation {

bool loadFromNVS(float& lat, float& lon) {
  Preferences p;
  if (!p.begin(NS, true)) return false;
  if (!p.isKey(KEY_LAT) || !p.isKey(KEY_LON)) {
    p.end();
    return false;
  }
  float storedLat = p.getFloat(KEY_LAT, NAN);
  float storedLon = p.getFloat(KEY_LON, NAN);
  p.end();
  if (isnan(storedLat) || isnan(storedLon)) return false;
  if (fabsf(storedLat) > 90.0f || fabsf(storedLon) > 180.0f) return false;
  lat = storedLat;
  lon = storedLon;
  return true;
}

bool fetchAndStore(float& lat, float& lon, uint32_t nowUTC) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(">> Geolocation: WiFi not connected, skipping");
    return false;
  }

  Serial.print(">> Geolocation: GET ");
  Serial.println(GEO_URL);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(15000);
  http.setConnectTimeout(10000);
  http.setReuse(false);
  if (!http.begin(client, GEO_URL)) {
    Serial.println(">> Geolocation: http.begin() failed");
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    Serial.print(">> Geolocation: HTTP ");
    Serial.println(code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();
  Serial.print(">> Geolocation: body bytes=");
  Serial.println(body.length());

  // ipapi.co returns ~700 bytes of JSON; we only need latitude + longitude.
  // Filter to those two fields to keep memory minimal.
  JsonDocument filter;
  filter["latitude"]  = true;
  filter["longitude"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  if (err) {
    Serial.print(">> Geolocation: parse error ");
    Serial.println(err.c_str());
    return false;
  }

  float fetchedLat = doc["latitude"]  | NAN;
  float fetchedLon = doc["longitude"] | NAN;
  if (isnan(fetchedLat) || isnan(fetchedLon) ||
      fabsf(fetchedLat) > 90.0f || fabsf(fetchedLon) > 180.0f) {
    Serial.println(">> Geolocation: missing or invalid latitude/longitude");
    return false;
  }

  Preferences p;
  if (!p.begin(NS, false)) {
    Serial.println(">> Geolocation: NVS begin failed");
    return false;
  }
  p.clear();
  p.putFloat(KEY_LAT,    fetchedLat);
  p.putFloat(KEY_LON,    fetchedLon);
  p.putUInt(KEY_FETCHED, nowUTC);
  p.end();

  lat = fetchedLat;
  lon = fetchedLon;
  Serial.print(">> Geolocation: persisted lat=");
  Serial.print(lat, 4);
  Serial.print(" lon=");
  Serial.println(lon, 4);
  return true;
}

void forget() {
  Preferences p;
  if (!p.begin(NS, false)) return;
  p.clear();
  p.end();
  Serial.println(">> Geolocation: NVS cleared.");
}

}  // namespace Geolocation

#include "Provisioning.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "WifiCreds.h"
#include "secrets.h"

namespace {

constexpr uint8_t  DNS_PORT = 53;
constexpr uint8_t  HTTP_PORT = 80;
const     IPAddress AP_IP(192, 168, 4, 1);
const     IPAddress AP_NETMASK(255, 255, 255, 0);

DNSServer dnsServer;
WebServer httpServer(HTTP_PORT);

// Deferred-save state. handleSave() can't tear down the HTTP server +
// AP from inside its own handler (the WebServer is mid-execution and
// stopping it crashes the chip). Instead: stash the creds, set a flag,
// let the main loop notice, finish the response, then do the teardown
// + NVS write + reboot.
String   pendingSSID;
String   pendingPW;
bool     savePending  = false;
uint32_t saveAtMS     = 0;       // millis() when the save was requested

// Cached scan results. Filled once before the main loop starts so the
// very first page load doesn't block 3–5 seconds on a fresh scan.
struct ScanEntry { String ssid; int rssi; };
ScanEntry scanCache[32];
int       scanCount = 0;
uint32_t  scanAtMS  = 0;

void refreshScan() {
  int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  scanCount = (n > 32) ? 32 : (n > 0 ? n : 0);
  for (int i = 0; i < scanCount; i++) {
    scanCache[i].ssid = WiFi.SSID(i);
    scanCache[i].rssi = WiFi.RSSI(i);
  }
  // Sort by descending RSSI.
  for (int i = 0; i < scanCount - 1; i++) {
    for (int j = i + 1; j < scanCount; j++) {
      if (scanCache[j].rssi > scanCache[i].rssi) {
        ScanEntry tmp = scanCache[i];
        scanCache[i]  = scanCache[j];
        scanCache[j]  = tmp;
      }
    }
  }
  WiFi.scanDelete();
  scanAtMS = millis();
}

// Form HTML. Single self-contained page — we don't pull in any
// external assets (the user's phone is on our SoftAP and has no
// internet route here). JS is inline; CSS is inline.
const char* PAGE_HEAD =
"<!doctype html>"
"<html><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>" SCULPTURE_NAME " — Wi-Fi Setup</title>"
"<style>"
"  html,body{margin:0;padding:0;background:#0c0c0e;color:#f4f1ea;"
"            font:16px/1.5 -apple-system,'Helvetica Neue',sans-serif}"
"  .wrap{max-width:420px;margin:0 auto;padding:32px 24px}"
"  h1{font-weight:600;font-size:22px;margin:0 0 6px}"
"  p.sub{color:#9a9a94;margin:0 0 28px;font-size:14px}"
"  label{display:block;margin:18px 0 6px;color:#c0bcb4;font-size:13px;"
"        letter-spacing:0.04em;text-transform:uppercase}"
"  select,input{width:100%;box-sizing:border-box;padding:12px 14px;"
"               background:#1a1a1c;border:1px solid #2a2a2e;color:#f4f1ea;"
"               border-radius:8px;font:inherit;outline:none}"
"  select:focus,input:focus{border-color:#b89062}"
"  button{width:100%;margin-top:24px;padding:14px;background:#b89062;"
"         color:#0c0c0e;border:0;border-radius:8px;font:600 16px inherit;"
"         letter-spacing:0.04em;text-transform:uppercase;cursor:pointer}"
"  button:disabled{opacity:0.5}"
"  .scan{margin-top:8px;font-size:12px;color:#7a7874;cursor:pointer}"
"  .err{margin-top:14px;color:#c24a2c;font-size:14px}"
"</style></head><body><div class='wrap'>"
"<h1>" SCULPTURE_NAME "</h1>"
"<p class='sub'>Connect this sculpture to your Wi-Fi.</p>"
"<form method='POST' action='/save'>"
"  <label for='ssid'>Network</label>"
"  <select id='ssid' name='ssid'>";

const char* PAGE_TAIL =
"  </select>"
"  <a class='scan' href='/rescan' style='text-decoration:none'>↻ Re-scan networks</a>"
"  <label for='pw'>Password</label>"
"  <input id='pw' name='pw' type='password' autocomplete='off'>"
"  <button type='submit'>Connect</button>"
"</form>"
"</div></body></html>";

const char* DONE_PAGE =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Saved</title>"
"<style>html,body{margin:0;background:#0c0c0e;color:#f4f1ea;"
"font:16px/1.5 -apple-system,sans-serif}"
".wrap{max-width:420px;margin:0 auto;padding:48px 24px;text-align:center}"
"h1{font-weight:600}p{color:#9a9a94}</style></head>"
"<body><div class='wrap'>"
"<h1>Saved</h1>"
"<p>The sculpture will restart and join your network.<br>"
"You can disconnect from this Wi-Fi now.</p>"
"</div></body></html>";

String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '&':  out += "&amp;";  break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:   out += c;
    }
  }
  return out;
}

void handleRoot() {
  // Use the cached scan filled at portal startup — no per-request blocking.
  // The "↻ Re-scan" link in the form hits /rescan to refresh.
  String body = PAGE_HEAD;
  if (scanCount == 0) {
    body += "<option value=''>(no networks found — re-scan)</option>";
  } else {
    for (int i = 0; i < scanCount; i++) {
      const String& ssid = scanCache[i].ssid;
      if (ssid.length() == 0) continue;
      body += "<option value='" + htmlEscape(ssid) + "'>";
      body += htmlEscape(ssid);
      body += "  (" + String(scanCache[i].rssi) + " dBm)";
      body += "</option>";
    }
  }
  body += PAGE_TAIL;
  httpServer.sendHeader("Cache-Control", "no-store");
  httpServer.send(200, "text/html", body);
}

void handleRescan() {
  refreshScan();
  // 303 See Other → browser does a GET to /, refreshing with new list.
  httpServer.sendHeader("Location", "/", true);
  httpServer.send(303, "text/plain", "");
}

void handleSave() {
  String ssid = httpServer.arg("ssid");
  String pw   = httpServer.arg("pw");
  ssid.trim();
  if (ssid.length() == 0) {
    httpServer.send(400, "text/plain", "Missing SSID");
    return;
  }

  Serial.print(">> Provisioning: received SSID '");
  Serial.print(ssid);
  Serial.println("'");

  // Send the "Saved" response IMMEDIATELY. The actual teardown + NVS save
  // happens after this handler returns, in the main loop — we can't
  // dismantle the HTTP server from within one of its own callbacks
  // without crashing the chip.
  httpServer.send(200, "text/html", DONE_PAGE);
  pendingSSID = ssid;
  pendingPW   = pw;
  saveAtMS    = millis();
  savePending = true;
}

// iOS / Android probe specific URLs to detect a captive portal. The
// trick is: if we return *any* response that *isn't* the expected
// "Success" / 204 No Content, the OS knows we're a captive portal and
// pops the setup UI. Returning the form HTML directly (HTTP 200 with
// our setup page) means the popup shows our content immediately —
// faster than chaining a 302 redirect to a separate /.
void handleCaptiveProbe() {
  handleRoot();
}

void handleNotFound() {
  handleRoot();
}

String apSSID() {
  // The sculpture's per-unit name (e.g. "Orbital Witness 1/12") is the
  // SoftAP SSID. It's already unique per unit, so no MAC suffix is needed.
  return String(SCULPTURE_NAME);
}

}  // namespace

namespace Provisioning {

[[noreturn]] void runPortal() {
  // Make sure the motor is disengaged. The PowerGate may not have run
  // (it doesn't run until later in boot), so we are conservative here
  // and skip touching the motor — it stays in whatever state it was.
  Serial.println();
  Serial.println(">> ============================================");
  Serial.println(">> Provisioning portal active — no Wi-Fi creds.");
  Serial.println(">> ============================================");

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);                       // keep radio awake
  WiFi.setTxPower(WIFI_POWER_19_5dBm);        // max — first-time pairing
                                              // can be at the edge of range

  WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
  String ssid = apSSID();
  // Channel 1 is the most universally supported; ssid_hidden=0 means
  // broadcast normally; max_connection=1 (we only ever expect one phone).
  WiFi.softAP(ssid.c_str(), nullptr, /*channel=*/1, /*ssid_hidden=*/0,
              /*max_connection=*/1);
  delay(500);
  // Re-assert TX power once the AP is up — some chips reset it on softAP().
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.print(">> SoftAP: ");
  Serial.print(ssid);
  Serial.print("  IP=");
  Serial.println(WiFi.softAPIP());

  // DNS hijack: every hostname returns our AP IP.
  dnsServer.start(DNS_PORT, "*", AP_IP);

  // Pre-scan the visible networks once before serving any requests. The
  // first page load is then instant; otherwise it would block 3–5 s.
  Serial.println(">> Scanning visible networks for the form...");
  refreshScan();
  Serial.print(">> Scan: ");
  Serial.print(scanCount);
  Serial.println(" networks visible");

  httpServer.on("/",                       HTTP_GET,  handleRoot);
  httpServer.on("/rescan",                 HTTP_GET,  handleRescan);
  httpServer.on("/save",                   HTTP_POST, handleSave);
  // Captive-portal probe URLs the major OSes use:
  httpServer.on("/hotspot-detect.html",    HTTP_GET,  handleCaptiveProbe);  // Apple
  httpServer.on("/library/test/success.html", HTTP_GET, handleCaptiveProbe);
  httpServer.on("/generate_204",           HTTP_GET,  handleCaptiveProbe);  // Android
  httpServer.on("/gen_204",                HTTP_GET,  handleCaptiveProbe);
  httpServer.on("/connecttest.txt",        HTTP_GET,  handleCaptiveProbe);  // Windows
  httpServer.on("/ncsi.txt",               HTTP_GET,  handleCaptiveProbe);
  httpServer.on("/redirect",               HTTP_GET,  handleCaptiveProbe);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();

  Serial.println(">> Open any URL on a phone joined to the SoftAP — the");
  Serial.println(">> setup page will appear via captive-portal detection.");
  Serial.print(">>   or browse to http://");
  Serial.println(WiFi.softAPIP());

  // Service requests forever. handleSave() sets `savePending` instead of
  // tearing things down inline (which crashes the chip). When that flag
  // appears, give the response a beat to finish leaving the wire, then
  // shut the radio down cleanly and write to NVS with no contention.
  for (;;) {
    dnsServer.processNextRequest();
    httpServer.handleClient();

    if (savePending && (millis() - saveAtMS > 800)) {
      // Just save and reboot. We tried tearing down the AP first to avoid
      // a suspected NVS-vs-WiFi contention but that crashed the chip.
      // The renamed NVS namespace ("fwcreds" vs the old "wifi") was
      // probably the actual fix. Reboot itself shuts down WiFi cleanly.
      savePending = false;
      Serial.println(">> Provisioning: writing creds to NVS...");
      bool ok = WifiCreds::save(pendingSSID, pendingPW);
      Serial.print(">> Provisioning: save ");
      Serial.println(ok ? "OK — rebooting" : "FAILED — rebooting to retry portal");
      delay(800);
      ESP.restart();
    }

    delay(2);
  }
}

}  // namespace Provisioning

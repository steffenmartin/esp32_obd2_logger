#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_wifi.h>

namespace {
WifiState state = WifiState::Disconnected;
const char *storedSsid = nullptr;
const char *storedPassword = nullptr;
unsigned long lastAttemptMs = 0;

// How long to wait between connection attempts while Disconnected, and
// how long to wait for a Connecting attempt to resolve before giving up
// and retrying - both carried over unchanged from the sandbox repo's
// empirically-arrived-at values (see wifi_manager.h's header comment).
constexpr unsigned long RETRY_INTERVAL_MS = 5000;
constexpr unsigned long CONNECT_TIMEOUT_MS = 10000;

// Below this RSSI (dBm), the connection is treated as effectively
// useless and a reconnect is forced proactively rather than waiting for
// a hard drop - carried over from the sandbox repo's finding.
constexpr int32_t RSSI_RECONNECT_THRESHOLD = -85;

void handleWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] Connected. IP: %s, RSSI: %d dBm\n",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
      state = WifiState::Connected;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] Lost connection. Reason: %d\n", info.wifi_sta_disconnected.reason);
      state = WifiState::Disconnected;
      break;
    default:
      break;
  }
}
}  // namespace

void wifiManagerBegin(const char *ssid, const char *password) {
  storedSsid = ssid;
  storedPassword = password;

  WiFi.onEvent(handleWifiEvent);
  WiFi.mode(WIFI_STA);
  // ESP-IDF's own auto-reconnect would otherwise race this state
  // machine's own reconnect attempts below - see the header comment.
  WiFi.setAutoReconnect(false);

  // This board overheats and drops connections at the default TX
  // power; the sandbox repo found passing 8 here fixed both. IMPORTANT
  // UNITS CORRECTION versus that repo's own README/comments, which
  // call this "8dBm": per ESP-IDF's documented mapping table for
  // esp_wifi_set_max_tx_power(), this parameter is in units of
  // 0.25dBm, and a raw value of 8 actually configures ~2dBm, not 8dBm.
  // The empirical finding (this value fixed the overheating/drops on
  // this board) still stands regardless of that mislabeling - carrying
  // over the literal value 8 that was actually tested, not a
  // recomputed "true 8dBm" value that was never validated on this
  // hardware.
  esp_wifi_set_max_tx_power(8);
  int8_t power;
  esp_wifi_get_max_tx_power(&power);
  Serial.printf("[WiFi] Manager initialized. Max TX power: %d (%.2f dBm).\n", power, power * 0.25);
}

void wifiManagerTick() {
  if (state == WifiState::Connected) {
    int32_t rssi = WiFi.RSSI();
    if (rssi < RSSI_RECONNECT_THRESHOLD) {
      Serial.printf("[WiFi] RSSI too low (%d dBm) - forcing reconnect.\n", rssi);
      WiFi.disconnect();
      state = WifiState::Disconnected;
      lastAttemptMs = millis();
    }
  }

  switch (state) {
    case WifiState::Disconnected:
      if (millis() - lastAttemptMs > RETRY_INTERVAL_MS) {
        Serial.println("[WiFi] Attempting connection...");
        WiFi.begin(storedSsid, storedPassword);
        state = WifiState::Connecting;
        lastAttemptMs = millis();
      }
      break;
    case WifiState::Connecting:
      if (millis() - lastAttemptMs > CONNECT_TIMEOUT_MS) {
        Serial.println("[WiFi] Connection attempt timed out, retrying...");
        WiFi.disconnect();
        state = WifiState::Disconnected;
        lastAttemptMs = millis();
      }
      break;
    case WifiState::Connected:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost unexpectedly.");
        state = WifiState::Disconnected;
        lastAttemptMs = millis();
      }
      break;
  }
}

WifiState wifiManagerState() { return state; }

int32_t wifiManagerRssi() { return state == WifiState::Connected ? WiFi.RSSI() : 0; }

#include "wifi_store.h"

#include <Preferences.h>
#include <cstring>

#include "secrets.h"

static const char *NS = "edgepair";

static bool secretsHasWifi() {
  return WIFI_SSID[0] != '\0' && strcmp(WIFI_SSID, "sua-rede") != 0;
}

bool wifiStoreLoad(String &ssid, String &pass, String &serverUrl) {
  if (secretsHasWifi()) {
    ssid = WIFI_SSID;
    pass = WIFI_PASS;
    serverUrl = SERVER_URL;
    return ssid.length() > 0;
  }

  Preferences prefs;
  if (!prefs.begin(NS, true)) {
    return false;
  }
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  serverUrl = prefs.getString("server", "");
  prefs.end();
  wifiStoreApplySecretsFallback(ssid, pass, serverUrl);
  return ssid.length() > 0;
}

bool wifiStoreSave(const String &ssid, const String &pass, const String &serverUrl) {
  Preferences prefs;
  if (!prefs.begin(NS, false)) {
    return false;
  }
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("server", serverUrl);
  prefs.end();
  return true;
}

bool wifiStoreHasCredentials() {
  String ssid;
  String pass;
  String serverUrl;
  wifiStoreLoad(ssid, pass, serverUrl);
  return ssid.length() > 0;
}

void wifiStoreApplySecretsFallback(String &ssid, String &pass, String &serverUrl) {
  if (ssid.length() == 0 && WIFI_SSID[0] != '\0') {
    ssid = WIFI_SSID;
    pass = WIFI_PASS;
  }
  if (serverUrl.length() == 0 && SERVER_URL[0] != '\0') {
    serverUrl = SERVER_URL;
  }
}

#include "SerialConsole.h"
#include <M5Unified.h>
#include <WiFi.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../clock/ClockManager.h"
#include "../wifi/WiFiLink.h"
#include "../display/DisplayManager.h"
#include "../input/InputManager.h"
#include "../power/BatteryManager.h"
#include "../settings/SettingsManager.h"
#include "../ui/UiManager.h"
#include "NightMode.h"
#include "ConsoleExtras.h"

namespace console {

static char line[96];
static size_t len = 0;

void begin() {
  Serial.println();
  Serial.println(F("Type 'help' for console commands."));
}

static void help() {
  Serial.println(F(
      "Commands:\n"
      "  status                 time, Wi-Fi, battery, memory\n"
      "  settime YYYY-MM-DD H:MM AM|PM   set clock (IST)\n"
      "  sync                   NTP sync now\n"
      "  wifi on|off            enable / disable Wi-Fi\n"
      "  wifi SSID PASSWORD     save main network (no spaces in SSID)\n"
      "  wifi2 SSID PASSWORD    save backup network\n"
      "  key a|b|c [long]       simulate a button press\n"
      "  shot                   dump the screen (see tools/screenshot.py)\n"
      "  night on|off|skip      force / cancel night mode\n"
      "  bright N               display brightness 1..10\n"
      "  portal [off]           open / close the parent setup page\n"
      "  name NAME              child's name for greetings (empty = none)\n"
      "  reboot                 restart"));
  extras::help();
}

static void status() {
  Clock.printStatus(Serial);
  Serial.printf("Wi-Fi  : %s", Net.stateName());
  if (Net.connected()) Serial.printf(" %s %d dBm", Net.ip().toString().c_str(), Net.rssi());
  Serial.println();
  Serial.printf("Battery: %d%% charging=%d %d mV\n", Battery.level(), Battery.charging(), Battery.millivolts());
  Serial.printf("Night  : scheduled=%d active=%d peeking=%d skipped=%d\n", NightMode.scheduled(), NightMode.active(),
                NightMode.peeking(), NightMode.skipped());
  Serial.printf("Memory : heap %u (min %u), PSRAM free %u, render %lu us, push %lu us\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned long)UI.lastRenderUs(),
                (unsigned long)Gfx.lastPushUs());
  extras::status();
}

static void exec(char* cmd) {
  char* save = nullptr;
  char* w = strtok_r(cmd, " ", &save);
  if (!w) return;
  if (!strcmp(w, "help")) help();
  else if (!strcmp(w, "status")) status();
  else if (!strcmp(w, "settime")) {
    int y, mo, d, h, mi;
    char ap[4] = {0};
    const char* rest = save ? save : "";
    if (sscanf(rest, "%d-%d-%d %d:%d %3s", &y, &mo, &d, &h, &mi, ap) == 6 && (ap[0] == 'A' || ap[0] == 'P' || ap[0] == 'a' || ap[0] == 'p')) {
      const bool pm = ap[0] == 'P' || ap[0] == 'p';
      if (Clock.setManual(y, mo, d, tl::to24h(h, pm), mi)) Serial.println("OK");
      else Serial.println("ERR invalid date/time");
    } else {
      Serial.println("usage: settime 2026-11-17 11:59 PM");
    }
  } else if (!strcmp(w, "sync")) {
    Clock.requestSync();
    Serial.println(Net.connected() ? "OK syncing" : "ERR offline");
  } else if (!strcmp(w, "wifi")) {
    char* a = strtok_r(nullptr, " ", &save);
    char* b = strtok_r(nullptr, "", &save);
    if (a && !strcmp(a, "on")) { AppCfg.get().wifiEnabled = 1; AppCfg.save(); Net.applyEnabled(); Serial.println("OK"); }
    else if (a && !strcmp(a, "off")) { AppCfg.get().wifiEnabled = 0; AppCfg.save(); Net.applyEnabled(); Serial.println("OK"); }
    else if (a) {
      strlcpy(AppCfg.wifi().ssid, a, sizeof AppCfg.wifi().ssid);
      strlcpy(AppCfg.wifi().pass, b ? b : "", sizeof AppCfg.wifi().pass);
      AppCfg.saveWifi();
      AppCfg.get().wifiEnabled = 1;
      AppCfg.save();
      Net.reconnectNow();
      Serial.println("OK saved");
    } else Serial.printf("Wi-Fi %s\n", Net.stateName());
  } else if (!strcmp(w, "scan")) {
    const int n = WiFi.scanNetworks(false, true);  // blocking ~2 s, debug only
    for (int i = 0; i < n; ++i)
      Serial.printf("  %-32s %4d dBm ch%2d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    Serial.printf("%d networks\n", n);
    WiFi.scanDelete();
  } else if (!strcmp(w, "wifi2")) {
    char* a = strtok_r(nullptr, " ", &save);
    char* b = strtok_r(nullptr, "", &save);
    if (!a) { Serial.println("usage: wifi2 SSID PASSWORD"); return; }
    strlcpy(AppCfg.wifi().ssid2, a, sizeof AppCfg.wifi().ssid2);
    strlcpy(AppCfg.wifi().pass2, b ? b : "", sizeof AppCfg.wifi().pass2);
    AppCfg.saveWifi();
    Serial.println("OK backup saved");
  } else if (!strcmp(w, "key")) {
    char* k = strtok_r(nullptr, " ", &save);
    char* l = strtok_r(nullptr, " ", &save);
    if (!k) { Serial.println("usage: key a|b|c [long]"); return; }
    const Btn b = (*k == 'a' || *k == 'A') ? Btn::A : (*k == 'b' || *k == 'B') ? Btn::B : Btn::C;
    Input.inject(b, (l && !strcmp(l, "long")) ? Press::Long : Press::Short);
    Serial.println("OK");
  } else if (!strcmp(w, "shot")) {
    Gfx.dumpScreenshot(Serial);
  } else if (!strcmp(w, "night")) {
    char* a = strtok_r(nullptr, " ", &save);
    if (a && !strcmp(a, "on")) NightMode.forceNow(true);
    else if (a && !strcmp(a, "off")) NightMode.forceNow(false);
    else if (a && !strcmp(a, "skip")) NightMode.skipTonight();
    UI.invalidate();
    Serial.println("OK");
  } else if (!strcmp(w, "bright")) {
    char* a = strtok_r(nullptr, " ", &save);
    if (a) { AppCfg.get().brightness = (uint8_t)constrain(atoi(a), 1, 10); AppCfg.markDirty(); }
    Serial.println("OK");
  } else if (!strcmp(w, "rot")) {
    char* a = strtok_r(nullptr, " ", &save);
    if (a) M5.Display.setRotation(atoi(a) & 7);
    UI.invalidate();
    Serial.printf("rotation %d, panel %dx%d\n", M5.Display.getRotation(), (int)M5.Display.width(), (int)M5.Display.height());
  } else if (!strcmp(w, "portal")) {
    char* a = strtok_r(nullptr, " ", &save);
    if (a && !strcmp(a, "off")) {
      Net.stopPortal();
      UI.home();
    } else {
      UI.go(ScreenId::WifiSetup);  // starts the setup AP + web page
    }
    Serial.printf("OK portal %s, AP %s, home IP %s\n", Net.portalActive() ? "on" : "off", Net.apIp().toString().c_str(),
                  Net.ip().toString().c_str());
  } else if (!strcmp(w, "name")) {
    char* a = strtok_r(nullptr, "", &save);
    strlcpy(AppCfg.get().childName, a ? a : "", sizeof AppCfg.get().childName);
    AppCfg.save();
    UI.invalidate();
    Serial.println("OK");
  } else if (!strcmp(w, "reboot")) {
    Serial.println("OK rebooting");
    Serial.flush();
    ESP.restart();
  } else if (!extras::exec(w, save)) {
    Serial.printf("ERR unknown command '%s' (try help)\n", w);
  }
}

void loop() {
  while (Serial.available()) {
    const int ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      line[len] = '\0';
      if (len) exec(line);
      len = 0;
    } else if (len < sizeof(line) - 1) {
      line[len++] = (char)ch;
    }
  }
}

}  // namespace console

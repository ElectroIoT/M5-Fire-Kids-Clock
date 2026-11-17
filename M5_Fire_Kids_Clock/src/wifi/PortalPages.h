// Shared helpers for the parent setup web pages, plus the hooks that let the
// alarm / routine modules add their own pages.
#pragma once
#include <Arduino.h>

class WebServer;

namespace web {
void begin(const char* title);
void end();
void send(const char* s);
void send(const String& s);
void sendf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void sendEscaped(const char* s);
String arg(const char* name);
bool has(const char* name);
int argInt(const char* name, int def);
void redirect(const char* to);
void timePicker(const char* prefix, int hour24, int minute);  // 12-hour + AM/PM
int readTime(const char* prefix);                              // -> minute of day, or -1
void markSaved();
}  // namespace web

namespace PortalPages {
void registerRoutes(WebServer& server);
void homeExtras();
void settingsExtras();
void settingsSave();
}  // namespace PortalPages

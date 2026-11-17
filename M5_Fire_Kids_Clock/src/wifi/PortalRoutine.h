// Routine pages of the parent setup site (implemented with the routine module).
#pragma once
class WebServer;

namespace PortalPages {
namespace routine {
void registerRoutes(WebServer& server);
void homeExtras();
void settingsExtras();
void settingsSave();
}  // namespace routine
}  // namespace PortalPages

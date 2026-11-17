// Procedural drawing helpers: rounded cards, pills, icons and the big
// "bubble" clock digits. Everything is vector-drawn (anti-aliased where M5GFX
// supports it), so no bitmap assets are needed in flash.
#pragma once
#include <M5Unified.h>

using Canvas = LGFX_Sprite;

namespace draw {

// ---- Text ----------------------------------------------------------------
// Fonts used across the UI (all built into M5GFX).
extern const lgfx::IFont* const FONT_SMALL;   // FreeSans 9pt
extern const lgfx::IFont* const FONT_BODY;    // FreeSansBold 9pt
extern const lgfx::IFont* const FONT_TITLE;   // FreeSansBold 12pt
extern const lgfx::IFont* const FONT_BIG;     // FreeSansBold 18pt
extern const lgfx::IFont* const FONT_HUGE;    // FreeSansBold 24pt

void text(Canvas& c, const char* s, int x, int y, const lgfx::IFont* f, uint32_t color,
          lgfx::textdatum_t datum = lgfx::textdatum_t::top_left);
inline void textC(Canvas& c, const char* s, int cx, int cy, const lgfx::IFont* f, uint32_t color) {
  text(c, s, cx, cy, f, color, lgfx::textdatum_t::middle_center);
}
int textW(Canvas& c, const char* s, const lgfx::IFont* f);
// Copies s into out, shortened with ".." so it fits maxW pixels.
void fitText(Canvas& c, const char* s, const lgfx::IFont* f, int maxW, char* out, size_t n);

// ---- Shapes -----------------------------------------------------------------
void vgradient(Canvas& c, int x, int y, int w, int h, uint32_t top, uint32_t bottom);
// Rows [offset, offset+h) of a gradient that spans fullH rows (for cached layers).
void vgradientPart(Canvas& c, int w, int h, uint32_t top, uint32_t bottom, int fullH, int offset);
void card(Canvas& c, int x, int y, int w, int h, int r, uint32_t fill, uint32_t shadow, int shadowDy = 3);
void pill(Canvas& c, int x, int y, int w, int h, uint32_t fill, const char* label, const lgfx::IFont* f,
          uint32_t textColor, uint32_t shadow = 0);
void star(Canvas& c, int cx, int cy, int r, uint32_t color, float rot = 0.f);
void heart(Canvas& c, int cx, int cy, int r, uint32_t color);
void sparkle(Canvas& c, int cx, int cy, int r, uint32_t color);
void cloud(Canvas& c, int cx, int cy, int s, uint32_t color);
void hill(Canvas& c, int y, uint32_t color, uint32_t dark);

// ---- Icons (about 16-20 px) --------------------------------------------------
void iconWifi(Canvas& c, int x, int y, int bars, uint32_t on, uint32_t off, bool cross);
void iconBattery(Canvas& c, int x, int y, int level, bool charging, uint32_t color, uint32_t fillColor);
void iconBell(Canvas& c, int cx, int cy, int s, uint32_t color);
void iconSync(Canvas& c, int cx, int cy, int r, uint32_t color, bool ok);
void iconGear(Canvas& c, int cx, int cy, int r, uint32_t color, uint32_t bg);
void iconCheck(Canvas& c, int cx, int cy, int s, uint32_t color);
void iconList(Canvas& c, int cx, int cy, int s, uint32_t color);
void iconMoon(Canvas& c, int cx, int cy, int r, uint32_t color, uint32_t bg);
void iconSun(Canvas& c, int cx, int cy, int r, uint32_t color);
void iconTrophy(Canvas& c, int cx, int cy, int s, uint32_t color);
void iconArrow(Canvas& c, int cx, int cy, int s, int dir, uint32_t color);  // dir: 0 up,1 right,2 down,3 left
void iconSpeaker(Canvas& c, int cx, int cy, int s, uint32_t color, bool muted);

// Weather icon by kind (see WeatherManager::Kind). size ~ 20..40
void iconWeather(Canvas& c, int cx, int cy, int s, uint8_t kind, bool day);

// ---- Bubble digits -----------------------------------------------------------
// Rounded seven-segment digit, w x h box, stroke radius r.
// inset < 0 means "use r" (segment end points sit r px inside the box).
void bubbleDigit(Canvas& c, int x, int y, int w, int h, float r, int digit, uint32_t color, float inset = -1.f);
// Big time "07:30" in 12-hour format (+ optional shadow). Returns drawn width.
struct BigTimeStyle {
  int digitW, digitH, gap, colonW;
  float stroke;
  uint32_t color, shadow;
  int shadowDx, shadowDy;
  uint32_t outline;   // sticker outline colour
  float outlineW;     // extra stroke radius for the outline (0 = none)
};
int bigTimeWidth(const BigTimeStyle& st);
void bigTime(Canvas& c, int x, int y, int hour24, int minute, bool colonOn, const BigTimeStyle& st);
// Stacked AM / PM chips; the active one is filled.
void amPmChips(Canvas& c, int x, int y, int w, int h, bool pm, uint32_t onFill, uint32_t onText,
               uint32_t offFill, uint32_t offText);

// ---- Bottom hint bar over the three physical buttons ------------------------
// Pass nullptr to leave a slot empty. Labels may start with "<", ">", "^", "v"
// to draw an arrow icon instead of text.
void hintBar(Canvas& c, const char* a, const char* b, const char* cLabel, bool night = false);

// Rounded header bar with a title.
void header(Canvas& c, const char* title, uint32_t fill, uint32_t textColor);

}  // namespace draw

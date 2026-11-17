#include "Draw.h"
#include <math.h>
#include "Theme.h"
#include "../input/InputManager.h"

namespace draw {

const lgfx::IFont* const FONT_SMALL = &fonts::FreeSans9pt7b;
const lgfx::IFont* const FONT_BODY  = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* const FONT_TITLE = &fonts::FreeSansBold12pt7b;
const lgfx::IFont* const FONT_BIG   = &fonts::FreeSansBold18pt7b;
const lgfx::IFont* const FONT_HUGE  = &fonts::FreeSansBold24pt7b;

static uint32_t lerp888(uint32_t a, uint32_t b, int t, int n) {
  const int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
  const int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
  const int r = ar + (br - ar) * t / n, g = ag + (bg - ag) * t / n, bl = ab + (bb - ab) * t / n;
  return (uint32_t)((r << 16) | (g << 8) | bl);
}

void text(Canvas& c, const char* s, int x, int y, const lgfx::IFont* f, uint32_t color, lgfx::textdatum_t datum) {
  c.setFont(f);
  c.setTextColor(color);
  c.setTextDatum(datum);
  c.setTextSize(1);
  c.drawString(s, x, y);
}

int textW(Canvas& c, const char* s, const lgfx::IFont* f) {
  c.setTextSize(1);
  return c.textWidth(s, f);
}

void fitText(Canvas& c, const char* s, const lgfx::IFont* f, int maxW, char* out, size_t n) {
  strlcpy(out, s, n);
  if (textW(c, out, f) <= maxW) return;
  size_t len = strlen(out);
  while (len > 1) {
    out[--len] = '\0';
    while (len && out[len - 1] == ' ') out[--len] = '\0';
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%s..", out);
    if (textW(c, tmp, f) <= maxW) { strlcpy(out, tmp, n); return; }
  }
}

void vgradient(Canvas& c, int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
  for (int i = 0; i < h; ++i) c.drawFastHLine(x, y + i, w, lerp888(top, bottom, i, h > 1 ? h - 1 : 1));
}

void vgradientPart(Canvas& c, int w, int h, uint32_t top, uint32_t bottom, int fullH, int offset) {
  for (int i = 0; i < h; ++i) c.drawFastHLine(0, i, w, lerp888(top, bottom, offset + i, fullH > 1 ? fullH - 1 : 1));
}

void card(Canvas& c, int x, int y, int w, int h, int r, uint32_t fill, uint32_t shadow, int shadowDy) {
  if (shadowDy) c.fillSmoothRoundRect(x, y + shadowDy, w, h, r, shadow);
  c.fillSmoothRoundRect(x, y, w, h, r, fill);
}

void pill(Canvas& c, int x, int y, int w, int h, uint32_t fill, const char* label, const lgfx::IFont* f,
          uint32_t textColor, uint32_t shadow) {
  if (shadow) c.fillSmoothRoundRect(x, y + 2, w, h, h / 2, shadow);
  c.fillSmoothRoundRect(x, y, w, h, h / 2, fill);
  if (label && *label) textC(c, label, x + w / 2, y + h / 2 + 1, f, textColor);
}

void star(Canvas& c, int cx, int cy, int r, uint32_t color, float rot) {
  float px[10], py[10];
  for (int i = 0; i < 10; ++i) {
    const float a = rot + (float)M_PI * (-0.5f + i * 0.2f);
    const float rr = (i & 1) ? r * 0.45f : (float)r;
    px[i] = cx + cosf(a) * rr;
    py[i] = cy + sinf(a) * rr;
  }
  for (int i = 0; i < 10; ++i) {
    const int j = (i + 1) % 10;
    c.fillTriangle(cx, cy, (int)lroundf(px[i]), (int)lroundf(py[i]), (int)lroundf(px[j]), (int)lroundf(py[j]), color);
  }
}

void heart(Canvas& c, int cx, int cy, int r, uint32_t color) {
  const int h = r / 2;
  c.fillSmoothCircle(cx - h, cy - h / 2, h + 1, color);
  c.fillSmoothCircle(cx + h, cy - h / 2, h + 1, color);
  c.fillTriangle(cx - r - 1, cy - h / 3, cx + r + 1, cy - h / 3, cx, cy + r, color);
}

void sparkle(Canvas& c, int cx, int cy, int r, uint32_t color) {
  c.fillTriangle(cx, cy - r, cx - r / 4, cy, cx + r / 4, cy, color);
  c.fillTriangle(cx, cy + r, cx - r / 4, cy, cx + r / 4, cy, color);
  c.fillTriangle(cx - r, cy, cx, cy - r / 4, cx, cy + r / 4, color);
  c.fillTriangle(cx + r, cy, cx, cy - r / 4, cx, cy + r / 4, color);
}

void cloud(Canvas& c, int cx, int cy, int s, uint32_t color) {
  c.fillSmoothCircle(cx - s * 6 / 10, cy + s / 10, s * 4 / 10, color);
  c.fillSmoothCircle(cx, cy - s * 2 / 10, s * 55 / 100, color);
  c.fillSmoothCircle(cx + s * 6 / 10, cy + s / 10, s * 4 / 10, color);
  c.fillSmoothRoundRect(cx - s * 6 / 10, cy, s * 12 / 10, s * 5 / 10, s / 4, color);
}

void hill(Canvas& c, int y, uint32_t color, uint32_t dark) {
  c.fillEllipse(70, y + 46, 150, 56, dark);
  c.fillEllipse(260, y + 52, 170, 58, color);
  c.fillEllipse(60, y + 52, 140, 52, color);
}

// ---------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------
void iconWifi(Canvas& c, int x, int y, int bars, uint32_t on, uint32_t off, bool cross) {
  // x,y = bottom centre of the fan
  for (int i = 0; i < 3; ++i) {
    const int r1 = 5 + i * 5;
    c.fillArc(x, y, r1, r1 - 2, 225, 315, (i < bars) ? on : off);
  }
  c.fillSmoothCircle(x, y - 1, 2, bars > 0 ? on : off);
  if (cross) {
    c.drawWideLine(x + 5, y - 12, x + 11, y - 6, 1.2f, theme::RED_DEEP);
    c.drawWideLine(x + 11, y - 12, x + 5, y - 6, 1.2f, theme::RED_DEEP);
  }
}

void iconBattery(Canvas& c, int x, int y, int level, bool charging, uint32_t color, uint32_t fillColor) {
  // 24x12 body at (x,y)
  c.fillSmoothRoundRect(x, y, 24, 13, 3, color);
  c.fillSmoothRoundRect(x + 2, y + 2, 20, 9, 2, theme::SNOW);
  c.fillRect(x + 24, y + 4, 2, 5, color);
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  const int w = (18 * level + 50) / 100;
  if (w > 0) c.fillRect(x + 3, y + 3, w, 7, level <= 20 ? theme::RED_SOFT : fillColor);
  if (charging) {
    c.fillTriangle(x + 13, y + 1, x + 8, y + 7, x + 12, y + 7, theme::YELLOW_DEEP);
    c.fillTriangle(x + 11, y + 12, x + 16, y + 6, x + 12, y + 6, theme::YELLOW_DEEP);
  }
}

void iconBell(Canvas& c, int cx, int cy, int s, uint32_t color) {
  const int r = s / 2;
  c.fillSmoothCircle(cx, cy - r / 3, r * 2 / 3, color);
  c.fillRect(cx - r * 2 / 3, cy - r / 3, r * 4 / 3, r * 2 / 3, color);
  c.fillSmoothRoundRect(cx - r, cy + r / 4, r * 2, r / 3 + 1, 2, color);
  c.fillSmoothCircle(cx, cy + r * 2 / 3 + 1, r / 4 + 1, color);
  c.fillSmoothCircle(cx, cy - r - 1, 2, color);
}

void iconSync(Canvas& c, int cx, int cy, int r, uint32_t color, bool ok) {
  c.fillArc(cx, cy, r, r - 2, 200, 340, color);
  c.fillArc(cx, cy, r, r - 2, 20, 160, color);
  c.fillTriangle(cx + r + 2, cy - 1, cx + r - 4, cy - 1, cx + r - 1, cy - 5, color);
  c.fillTriangle(cx - r - 2, cy + 1, cx - r + 4, cy + 1, cx - r + 1, cy + 5, color);
  if (!ok) c.fillSmoothCircle(cx, cy, 2, theme::RED_DEEP);
}

void iconGear(Canvas& c, int cx, int cy, int r, uint32_t color, uint32_t bg) {
  for (int i = 0; i < 8; ++i) {
    const float a = i * (float)M_PI / 4;
    c.drawWideLine(cx, cy, cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), r * 0.22f, color);
  }
  c.fillSmoothCircle(cx, cy, r * 3 / 4, color);
  c.fillSmoothCircle(cx, cy, r / 3, bg);
}

void iconCheck(Canvas& c, int cx, int cy, int s, uint32_t color) {
  const float w = s * 0.12f + 1;
  c.drawWideLine(cx - s / 2, cy, cx - s / 8, cy + s * 3 / 8, w, color);
  c.drawWideLine(cx - s / 8, cy + s * 3 / 8, cx + s / 2, cy - s * 3 / 8, w, color);
}

void iconList(Canvas& c, int cx, int cy, int s, uint32_t color) {
  for (int i = -1; i <= 1; ++i) {
    c.fillSmoothCircle(cx - s / 2, cy + i * s / 3, 2, color);
    c.drawWideLine(cx - s / 4, cy + i * s / 3, cx + s / 2, cy + i * s / 3, 1.5f, color);
  }
}

void iconMoon(Canvas& c, int cx, int cy, int r, uint32_t color, uint32_t bg) {
  c.fillSmoothCircle(cx, cy, r, color);
  c.fillSmoothCircle(cx + r * 45 / 100, cy - r * 3 / 10, r * 85 / 100, bg);
}

void iconSun(Canvas& c, int cx, int cy, int r, uint32_t color) {
  for (int i = 0; i < 8; ++i) {
    const float a = i * (float)M_PI / 4;
    c.drawWideLine(cx + (int)(cosf(a) * r * 1.3f), cy + (int)(sinf(a) * r * 1.3f), cx + (int)(cosf(a) * r * 1.75f),
                   cy + (int)(sinf(a) * r * 1.75f), r * 0.14f + 0.6f, color);
  }
  c.fillSmoothCircle(cx, cy, r, color);
}

void iconTrophy(Canvas& c, int cx, int cy, int s, uint32_t color) {
  const int r = s / 2;
  c.fillArc(cx, cy - r / 3, r * 3 / 4, 0, 0, 180, color);
  c.fillRect(cx - r * 3 / 4, cy - r, r * 3 / 2, r * 2 / 3, color);
  c.fillArc(cx - r * 3 / 4, cy - r / 2, r / 3, r / 3 - 2, 90, 270, color);
  c.fillArc(cx + r * 3 / 4, cy - r / 2, r / 3, r / 3 - 2, 270, 450, color);
  c.fillRect(cx - 2, cy + r / 3, 4, r / 3, color);
  c.fillSmoothRoundRect(cx - r / 2, cy + r * 2 / 3, r, r / 4 + 2, 2, color);
}

void iconArrow(Canvas& c, int cx, int cy, int s, int dir, uint32_t color) {
  const int h = s / 2;
  switch (dir & 3) {
    case 0: c.fillTriangle(cx, cy - h, cx - h, cy + h / 2, cx + h, cy + h / 2, color); break;
    case 1: c.fillTriangle(cx + h, cy, cx - h / 2, cy - h, cx - h / 2, cy + h, color); break;
    case 2: c.fillTriangle(cx, cy + h, cx - h, cy - h / 2, cx + h, cy - h / 2, color); break;
    default: c.fillTriangle(cx - h, cy, cx + h / 2, cy - h, cx + h / 2, cy + h, color); break;
  }
}

void iconSpeaker(Canvas& c, int cx, int cy, int s, uint32_t color, bool muted) {
  const int h = s / 2;
  c.fillRect(cx - h, cy - h / 3, h / 2, h * 2 / 3, color);
  c.fillTriangle(cx - h / 2, cy - h / 3, cx, cy - h, cx, cy + h, color);
  c.fillTriangle(cx - h / 2, cy - h / 3, cx - h / 2, cy + h / 3, cx, cy + h, color);
  if (muted) {
    c.drawWideLine(cx + 3, cy - h / 2, cx + h, cy + h / 2, 1.2f, theme::RED_DEEP);
    c.drawWideLine(cx + h, cy - h / 2, cx + 3, cy + h / 2, 1.2f, theme::RED_DEEP);
  } else {
    c.fillArc(cx, cy, h * 2 / 3, h * 2 / 3 - 2, -45, 45, color);
    c.fillArc(cx, cy, h, h - 2, -45, 45, color);
  }
}

void iconWeather(Canvas& c, int cx, int cy, int s, uint8_t kind, bool day) {
  const uint32_t sun = theme::YELLOW_DEEP, cl = 0xFFFFFFu, clDark = 0xC9D3E6u, rain = theme::BLUE_DEEP;
  const int r = s / 2;
  switch (kind) {
    case 1:  // clear
      if (day) iconSun(c, cx, cy, r * 55 / 100, sun);
      else iconMoon(c, cx, cy, r * 7 / 10, theme::NIGHT_MOON, 0x2D3166u);
      break;
    case 2:  // partly cloudy
      if (day) iconSun(c, cx - r / 3, cy - r / 4, r * 4 / 10, sun);
      else iconMoon(c, cx - r / 3, cy - r / 4, r / 2, theme::NIGHT_MOON, 0x2D3166u);
      cloud(c, cx + r / 6, cy + r / 4, r * 8 / 10, cl);
      break;
    case 3:  // cloudy
      cloud(c, cx + r / 4, cy - r / 4, r * 6 / 10, clDark);
      cloud(c, cx - r / 8, cy + r / 6, r * 8 / 10, cl);
      break;
    case 4:  // fog
      cloud(c, cx, cy - r / 4, r * 7 / 10, clDark);
      for (int i = 0; i < 2; ++i) c.drawWideLine(cx - r * 3 / 4, cy + r / 3 + i * 5, cx + r * 3 / 4, cy + r / 3 + i * 5, 1.4f, clDark);
      break;
    case 5:  // drizzle
    case 6:  // rain
      cloud(c, cx, cy - r / 4, r * 8 / 10, kind == 6 ? clDark : cl);
      for (int i = -1; i <= 1; ++i) {
        const int x = cx + i * r / 2;
        c.drawWideLine(x, cy + r / 3, x - 3, cy + r * 3 / 4, kind == 6 ? 1.6f : 1.1f, rain);
      }
      break;
    case 7:  // snow
      cloud(c, cx, cy - r / 4, r * 8 / 10, cl);
      for (int i = -1; i <= 1; ++i) sparkle(c, cx + i * r / 2, cy + r / 2 + (i & 1) * 4, 4, 0xA9C9F0u);
      break;
    case 8:  // thunder
      cloud(c, cx, cy - r / 4, r * 8 / 10, clDark);
      c.fillTriangle(cx + 2, cy, cx - 5, cy + r / 2, cx, cy + r / 2, sun);
      c.fillTriangle(cx - 2, cy + r, cx + 5, cy + r / 3, cx, cy + r / 3, sun);
      break;
    default:
      cloud(c, cx, cy, r * 7 / 10, clDark);
      break;
  }
}

// ---------------------------------------------------------------------------
// Bubble digits
// ---------------------------------------------------------------------------
//   a
// f   b
//   g
// e   c
//   d
static const uint8_t kSegs[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111,
};

void bubbleDigit(Canvas& c, int x, int y, int w, int h, float r, int digit, uint32_t color, float inset) {
  if (digit < 0 || digit > 9) return;
  const uint8_t m = kSegs[digit];
  const int ri = (int)ceilf(inset < 0 ? r : inset);
  const int x0 = x + ri, x1 = x + w - ri, y0 = y + ri, ym = y + h / 2, y1 = y + h - ri;
  if (m & 0x01) c.drawWideLine(x0, y0, x1, y0, r, color);  // a
  if (m & 0x02) c.drawWideLine(x1, y0, x1, ym, r, color);  // b
  if (m & 0x04) c.drawWideLine(x1, ym, x1, y1, r, color);  // c
  if (m & 0x08) c.drawWideLine(x0, y1, x1, y1, r, color);  // d
  if (m & 0x10) c.drawWideLine(x0, ym, x0, y1, r, color);  // e
  if (m & 0x20) c.drawWideLine(x0, y0, x0, ym, r, color);  // f
  if (m & 0x40) c.drawWideLine(x0, ym, x1, ym, r, color);  // g
}

int bigTimeWidth(const BigTimeStyle& st) { return st.digitW * 4 + st.gap * 2 + st.colonW; }

static void bigTimePass(Canvas& c, int x, int y, int hour24, int minute, bool colonOn, const BigTimeStyle& st,
                        uint32_t color, float grow) {
  int h12 = hour24 % 12;
  if (h12 == 0) h12 = 12;
  const int d[4] = {h12 / 10, h12 % 10, minute / 10, minute % 10};
  int cx = x;
  for (int i = 0; i < 4; ++i) {
    bubbleDigit(c, cx, y, st.digitW, st.digitH, st.stroke + grow, d[i], color, st.stroke);
    cx += st.digitW;
    if (i == 0 || i == 2) cx += st.gap;
    if (i == 1) {
      if (colonOn) {
        const int ccx = cx + st.colonW / 2;
        const int dr = (int)(st.stroke * 1.05f + grow);
        c.fillSmoothCircle(ccx, y + st.digitH * 3 / 10, dr, color);
        c.fillSmoothCircle(ccx, y + st.digitH * 7 / 10, dr, color);
      }
      cx += st.colonW;
    }
  }
}

void bigTime(Canvas& c, int x, int y, int hour24, int minute, bool colonOn, const BigTimeStyle& st) {
  if (st.shadowDx || st.shadowDy)
    bigTimePass(c, x + st.shadowDx, y + st.shadowDy, hour24, minute, colonOn, st, st.shadow, st.outlineW);
  if (st.outlineW > 0) bigTimePass(c, x, y, hour24, minute, colonOn, st, st.outline, st.outlineW);
  bigTimePass(c, x, y, hour24, minute, colonOn, st, st.color, 0);
}

void amPmChips(Canvas& c, int x, int y, int w, int h, bool pm, uint32_t onFill, uint32_t onText, uint32_t offFill,
               uint32_t offText) {
  const int gap = 6;
  c.fillSmoothRoundRect(x, y, w, h, h / 2, pm ? offFill : onFill);
  textC(c, "AM", x + w / 2, y + h / 2 + 1, FONT_BODY, pm ? offText : onText);
  c.fillSmoothRoundRect(x, y + h + gap, w, h, h / 2, pm ? onFill : offFill);
  textC(c, "PM", x + w / 2, y + h + gap + h / 2 + 1, FONT_BODY, pm ? onText : offText);
}

// ---------------------------------------------------------------------------
// Bars
// ---------------------------------------------------------------------------
static void hintSlot(Canvas& c, int cx, const char* label, bool down, bool night) {
  if (!label) return;
  const int w = 86, h = 24, y = 213;
  const uint32_t fill = night ? theme::NIGHT_BG2 : (down ? theme::PINK_DEEP : theme::SNOW);
  const uint32_t ink = night ? theme::NIGHT_INK : (down ? theme::SNOW : theme::INK);
  if (!night) c.fillSmoothRoundRect(cx - w / 2, y + 2, w, h, h / 2, theme::SHADOW);
  c.fillSmoothRoundRect(cx - w / 2, y + (down ? 1 : 0), w, h, h / 2, fill);
  const int ty = y + h / 2 + (down ? 2 : 1);
  if (label[0] == '<' && label[1] == 0) iconArrow(c, cx, ty, 12, 3, ink);
  else if (label[0] == '>' && label[1] == 0) iconArrow(c, cx, ty, 12, 1, ink);
  else if (label[0] == '^' && label[1] == 0) iconArrow(c, cx, ty, 12, 0, ink);
  else if (label[0] == 'v' && label[1] == 0) iconArrow(c, cx, ty, 12, 2, ink);
  else textC(c, label, cx, ty, FONT_BODY, ink);
}

void hintBar(Canvas& c, const char* a, const char* b, const char* cLabel, bool night) {
  hintSlot(c, theme::BTN_X_A, a, Input.isDown(Btn::A), night);
  hintSlot(c, theme::BTN_X_B, b, Input.isDown(Btn::B), night);
  hintSlot(c, theme::BTN_X_C, cLabel, Input.isDown(Btn::C), night);
}

void header(Canvas& c, const char* title, uint32_t fill, uint32_t textColor) {
  c.fillSmoothRoundRect(8, 6, 304, 30, 15, fill);
  textC(c, title, 160, 22, FONT_TITLE, textColor);
}

}  // namespace draw

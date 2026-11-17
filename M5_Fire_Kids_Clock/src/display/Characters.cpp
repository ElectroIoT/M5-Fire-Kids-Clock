#include "Characters.h"
#include <math.h>
#include "Theme.h"
#include <stdio.h>

namespace chars {

using namespace theme;

static bool blinking(uint32_t t, uint32_t seed) {
  const uint32_t p = (t + seed * 977) % 4200;
  return p < 130;
}

// Eyes: open = round pupils with a shine; closed = little smiling arcs.
static void eyes(Canvas& c, int cx, int cy, int s, bool closed, uint32_t ink, int lookX = 0) {
  const int dx = s * 36 / 100, er = s * 11 / 100 + 1;
  for (int side = -1; side <= 1; side += 2) {
    const int ex = cx + side * dx + lookX;
    if (closed) {
      c.fillArc(ex, cy, er + 1, er - 1, 200, 340, ink);
    } else {
      c.fillSmoothCircle(ex, cy, er, ink);
      c.fillSmoothCircle(ex + er / 3, cy - er / 3, er / 3 + 1, SNOW);
    }
  }
}

static void cheeks(Canvas& c, int cx, int cy, int s) {
  const int dx = s * 55 / 100, r = s * 12 / 100 + 1;
  c.fillSmoothCircle(cx - dx, cy, r, CHEEK);
  c.fillSmoothCircle(cx + dx, cy, r, CHEEK);
}

static void mouth(Canvas& c, int cx, int cy, int s, Mood mood, uint32_t ink, uint32_t t) {
  const int r = s * 20 / 100 + 1;
  switch (mood) {
    case Mood::Excited: {
      const int open = s * 14 / 100 + 2 + (int)(2 * sinf(t / 90.0f));
      c.fillEllipse(cx, cy + 2, r, open, ink);
      c.fillEllipse(cx, cy + 2 + open / 2, r * 6 / 10, open / 2, PINK_DEEP);
      break;
    }
    case Mood::Sleepy:
      c.fillSmoothCircle(cx, cy + 2, s * 7 / 100 + 1, ink);
      break;
    case Mood::Proud:
      c.fillArc(cx, cy - r / 2, r + 2, 0, 20, 160, ink);
      c.fillArc(cx, cy - r / 2 + 2, r * 7 / 10, 0, 40, 140, PINK_DEEP);
      break;
    default:
      c.fillArc(cx, cy - r / 2, r + 1, r - 2, 25, 155, ink);
      break;
  }
}

static void drawSun(Canvas& c, int cx, int cy, int s, uint32_t t, Mood mood) {
  const float rot = t / 2600.0f;
  const int rays = 12;
  for (int i = 0; i < rays; ++i) {
    const float a = rot + i * 2 * (float)M_PI / rays;
    const float len = (i & 1) ? 1.42f : 1.58f;
    const int x0 = cx + (int)(cosf(a) * s * 1.12f), y0 = cy + (int)(sinf(a) * s * 1.12f);
    const int x1 = cx + (int)(cosf(a) * s * len), y1 = cy + (int)(sinf(a) * s * len);
    c.drawWedgeLine(x0, y0, x1, y1, s * 0.13f, s * 0.05f, (i & 1) ? APRICOT : YELLOW_DEEP);
  }
  c.fillSmoothCircle(cx, cy, s + 2, APRICOT);
  c.fillSmoothCircle(cx, cy, s, 0xFFD966u);
  c.fillSmoothCircle(cx - s / 3, cy - s / 3, s / 4, 0xFFE89Au);  // highlight
  const bool closed = mood == Mood::Sleepy || blinking(t, 1);
  eyes(c, cx, cy - s / 8, s, closed, INK);
  cheeks(c, cx, cy + s / 5, s);
  mouth(c, cx, cy + s * 3 / 10, s, mood, INK, t);
}

static void drawBunny(Canvas& c, int cx, int cy, int s, uint32_t t, Mood mood) {
  const uint32_t fur = 0xFFFFFFu, inner = BLUSH, outline = 0xEBDDE8u;
  const float wig = sinf(t / 380.0f) * (mood == Mood::Excited ? 0.25f : 0.08f);
  for (int side = -1; side <= 1; side += 2) {
    const float a = -1.5708f + side * (0.28f + (side < 0 ? wig : -wig));
    const int ex = cx + (int)(cosf(a) * s * 1.2f), ey = cy + (int)(sinf(a) * s * 1.2f);
    c.drawWideLine(cx + side * s / 3, cy - s / 2, ex, ey, s * 0.27f, outline);
    c.drawWideLine(cx + side * s / 3, cy - s / 2, ex, ey, s * 0.23f, fur);
    c.drawWideLine(cx + side * s / 3, cy - s / 2, ex, ey + s / 8, s * 0.11f, inner);
  }
  c.fillSmoothCircle(cx, cy, s + 2, outline);
  c.fillSmoothCircle(cx, cy, s, fur);
  const bool closed = mood == Mood::Sleepy || blinking(t, 2);
  eyes(c, cx, cy - s / 10, s, closed, INK);
  cheeks(c, cx, cy + s / 5, s);
  c.fillSmoothCircle(cx, cy + s / 8, s / 10 + 1, PINK_DEEP);  // nose
  mouth(c, cx, cy + s * 3 / 10, s, mood, INK, t);
}

static void drawKitty(Canvas& c, int cx, int cy, int s, uint32_t t, Mood mood) {
  const uint32_t fur = 0xFFCFA3u, dark = 0xF2A86Fu;
  for (int side = -1; side <= 1; side += 2) {
    const int bx = cx + side * s * 7 / 10;
    c.fillTriangle(bx - s / 3, cy - s / 2, bx + s / 3, cy - s / 2, bx + side * s / 6, cy - s * 13 / 10, dark);
    c.fillTriangle(bx - s / 5, cy - s / 2, bx + s / 5, cy - s / 2, bx + side * s / 8, cy - s * 11 / 10, BLUSH);
  }
  c.fillSmoothCircle(cx, cy, s + 2, dark);
  c.fillSmoothCircle(cx, cy, s, fur);
  c.drawWideLine(cx, cy - s, cx, cy - s * 6 / 10, s * 0.07f, dark);  // forehead stripes
  c.drawWideLine(cx - s / 5, cy - s * 95 / 100, cx - s / 5, cy - s * 7 / 10, s * 0.06f, dark);
  c.drawWideLine(cx + s / 5, cy - s * 95 / 100, cx + s / 5, cy - s * 7 / 10, s * 0.06f, dark);
  // Relaxed: half-closed happy eyes in the evening.
  const bool closed = mood != Mood::Excited || blinking(t, 3);
  eyes(c, cx, cy - s / 10, s, closed, INK);
  cheeks(c, cx, cy + s / 5, s);
  c.fillTriangle(cx - s / 9, cy + s / 10, cx + s / 9, cy + s / 10, cx, cy + s / 5, PINK_DEEP);
  for (int side = -1; side <= 1; side += 2) {  // whiskers
    c.drawLine(cx + side * s / 3, cy + s / 4, cx + side * s * 95 / 100, cy + s / 6, INK_SOFT);
    c.drawLine(cx + side * s / 3, cy + s * 3 / 10, cx + side * s * 95 / 100, cy + s * 35 / 100, INK_SOFT);
  }
  mouth(c, cx, cy + s * 35 / 100, s, mood == Mood::Excited ? Mood::Excited : Mood::Happy, INK, t);
}

static void drawMoon(Canvas& c, int cx, int cy, int s, uint32_t t, Mood mood, uint32_t bg) {
  const uint32_t moon = 0xFFEFA8u, rim = 0xF2D37Au;
  c.fillSmoothCircle(cx, cy, s + 2, rim);
  c.fillSmoothCircle(cx, cy, s, moon);
  // Craters
  c.fillSmoothCircle(cx + s / 3, cy - s / 2, s / 8, 0xF7E08Fu);
  c.fillSmoothCircle(cx - s / 2, cy + s / 3, s / 10, 0xF7E08Fu);
  const bool awake = mood == Mood::Excited || mood == Mood::Proud;
  eyes(c, cx, cy - s / 12, s, !awake, 0x5B4E7Au);
  cheeks(c, cx, cy + s / 5, s);
  mouth(c, cx, cy + s * 3 / 10, s, awake ? Mood::Happy : Mood::Sleepy, 0x5B4E7Au, t);
  // Night cap
  const int capX = cx - s / 5, capY = cy - s * 8 / 10;
  c.fillTriangle(capX - s * 7 / 10, capY + s / 6, capX + s * 6 / 10, capY - s / 8, capX + s * 9 / 10, capY - s * 7 / 10, LAV_DEEP);
  c.fillSmoothRoundRect(capX - s * 75 / 100, capY + s / 12, s * 13 / 10, s / 5, s / 10, LAVENDER);
  c.fillSmoothCircle(capX + s * 9 / 10, capY - s * 7 / 10, s / 7 + 1, SNOW);
  if (!awake) {
    // Floating "z z Z"
    const uint32_t ph = t % 3000;
    for (int i = 0; i < 3; ++i) {
      const int k = (int)((ph + i * 1000) % 3000);
      const int zx = cx + s + 4 + k * s / 3000;
      const int zy = cy - s / 3 - k * s / 1500;
      if (k < 2600) draw::text(c, i == 2 ? "Z" : "z", zx, zy, i == 2 ? draw::FONT_TITLE : draw::FONT_BODY,
                               bg == NIGHT_BG ? NIGHT_INK : LAV_DEEP, lgfx::textdatum_t::middle_center);
    }
  }
}

void draw(Canvas& c, Kind k, int cx, int cy, int s, uint32_t t, Mood mood, uint32_t bg) {
  // Gentle idle bob; bigger bounce when excited (alarm) or proud (reward).
  float amp = s * 0.05f, speed = 700.0f;
  if (mood == Mood::Excited) { amp = s * 0.18f; speed = 160.0f; }
  if (mood == Mood::Proud) { amp = s * 0.12f; speed = 260.0f; }
  if (mood == Mood::Sleepy) { amp = s * 0.04f; speed = 1400.0f; }
  const int bob = (int)(sinf(t / speed) * amp);
  switch (k) {
    case Kind::Sun: drawSun(c, cx, cy + bob, s, t, mood); break;
    case Kind::Bunny: drawBunny(c, cx, cy + bob, s, t, mood); break;
    case Kind::Kitty: drawKitty(c, cx, cy + bob, s, t, mood); break;
    case Kind::Moon: drawMoon(c, cx, cy + bob, s, t, mood, bg); break;
  }
}

Kind forHour(int h) {
  if (h >= 5 && h < 12) return Kind::Sun;
  if (h >= 12 && h < 17) return Kind::Bunny;
  if (h >= 17 && h < 21) return Kind::Kitty;
  return Kind::Moon;
}

const char* name(Kind k) {
  switch (k) {
    case Kind::Sun: return "Sunny";
    case Kind::Bunny: return "Bun-Bun";
    case Kind::Kitty: return "Miko";
    default: return "Luna";
  }
}

void greetingFor(int h, const char* name, char* out, size_t n) {
  if (!name || !name[0]) {
    snprintf(out, n, "%s", greeting(h));
    return;
  }
  const char* g = (h >= 5 && h < 12) ? "Good morning" : (h >= 12 && h < 17) ? "Good afternoon"
                : (h >= 17 && h < 21) ? "Good evening" : "Sweet dreams";
  snprintf(out, n, "%s, %s!", g, name);
}

const char* greeting(int h) {
  if (h >= 5 && h < 12) return "Good morning!";
  if (h >= 12 && h < 17) return "Good afternoon!";
  if (h >= 17 && h < 21) return "Good evening!";
  return "Sweet dreams!";
}

}  // namespace chars

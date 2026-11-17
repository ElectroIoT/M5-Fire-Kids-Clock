// Cute pastel cartoon palette (RGB888; M5GFX converts to RGB565).
// Always pass colours as uint32_t so M5GFX treats them as RGB888.
#pragma once
#include <stdint.h>

namespace theme {

constexpr uint32_t CREAM = 0xFFF8EEu;
constexpr uint32_t CREAM_DARK = 0xF3E6D3u;
constexpr uint32_t SNOW = 0xFFFFFFu;
constexpr uint32_t BLUSH = 0xFFC8DDu;
constexpr uint32_t PINK_DEEP = 0xF27BA5u;
constexpr uint32_t MINT = 0xBDF0DAu;
constexpr uint32_t MINT_DEEP = 0x52C49Eu;
constexpr uint32_t POWDER = 0xBFE3FAu;
constexpr uint32_t BLUE_DEEP = 0x5AA9E6u;
constexpr uint32_t BUTTER = 0xFFEDA6u;
constexpr uint32_t YELLOW_DEEP = 0xFFC94Du;
constexpr uint32_t APRICOT = 0xFFAE6Bu;
constexpr uint32_t LAVENDER = 0xE3D6F7u;
constexpr uint32_t LAV_DEEP = 0x9B7FD1u;
constexpr uint32_t PEACH = 0xFFD9C2u;
constexpr uint32_t RED_SOFT = 0xFF7B7Bu;
constexpr uint32_t RED_DEEP = 0xE5484Du;
constexpr uint32_t GRASS = 0xA6E3A1u;
constexpr uint32_t GRASS_DARK = 0x7FCF8Au;

constexpr uint32_t INK = 0x4A3F55u;  // main text (soft plum, never pure black)
constexpr uint32_t INK_SOFT = 0x8A7F96u;
constexpr uint32_t SHADOW = 0xD9CBB8u;
constexpr uint32_t CHEEK = 0xFF9DB8u;

// Night palette (very low glare)
constexpr uint32_t NIGHT_BG = 0x0B0D22u;
constexpr uint32_t NIGHT_BG2   = 0x161A3Cu;
constexpr uint32_t NIGHT_INK = 0x6E6AA8u;
constexpr uint32_t NIGHT_DIM = 0x3A3870u;
constexpr uint32_t NIGHT_MOON = 0xC9C08Au;

// Sky gradients per time of day: {top, bottom}
struct Sky { uint32_t top, bottom, digit, digitShadow; };
constexpr Sky SKY_MORNING   = {0xFFE5C4u, 0xD3EEFFu, 0xF27BA5u, 0xFFFFFFu};
constexpr Sky SKY_AFTERNOON = {0xBFE6FFu, 0xE8FFF4u, 0x3D8FD6u, 0xFFFFFFu};
constexpr Sky SKY_EVENING   = {0xE6D6FFu, 0xFFD9E6u, 0x8E6CCBu, 0xFFFFFFu};
constexpr Sky SKY_NIGHT     = {0x2D3166u, 0x4B4785u, 0xFFF1B8u, 0x1E2148u};

// Physical button centres on the M5Stack Fire (below the screen).
constexpr int BTN_X_A = 68;
constexpr int BTN_X_B = 160;
constexpr int BTN_X_C = 252;

}  // namespace theme

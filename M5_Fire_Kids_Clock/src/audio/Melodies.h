// Built-in synthesised tunes. {frequency Hz, duration ms}; frequency 0 = rest.
// Traditional / public-domain tunes: Frere Jacques, Twinkle Twinkle Little
// Star, Brahms' Lullaby. Everything else is original.
#pragma once
#include <stdint.h>

namespace melodies {

struct Note {
  uint16_t hz;
  uint16_t ms;
};

// Note frequencies
constexpr uint16_t G3 = 196, A3 = 220, B3 = 247;
constexpr uint16_t C4 = 262, D4 = 294, E4 = 330, F4 = 349, G4 = 392, A4 = 440, B4 = 494;
constexpr uint16_t C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880, B5 = 988;
constexpr uint16_t C6 = 1047, D6 = 1175, E6 = 1319, G6 = 1568;
constexpr uint16_t R = 0;

// ---- Alarm tunes (loop) ----------------------------------------------------
static const Note GENTLE_WAKE[] = {
    {C5, 260}, {E5, 260}, {G5, 260}, {C6, 520}, {R, 200}, {G5, 260}, {E5, 260}, {G5, 260}, {C6, 520}, {R, 200},
    {A5, 260}, {G5, 260}, {E5, 260}, {D5, 260}, {C5, 520}, {R, 900},
};
static const Note CARTOON_ALARM[] = {
    {G5, 90}, {C6, 90}, {G5, 90}, {C6, 90}, {G5, 90}, {C6, 90}, {R, 80},
    {C5, 40}, {D5, 40}, {E5, 40}, {F5, 40}, {G5, 40}, {A5, 40}, {B5, 40}, {C6, 40}, {D6, 40}, {E6, 80}, {R, 120},
    {E6, 70}, {R, 50}, {E6, 70}, {R, 50}, {G6, 160}, {R, 500},
};
// Frere Jacques ("Are you sleeping?") - traditional
static const Note MORNING_SONG[] = {
    {C5, 300}, {D5, 300}, {E5, 300}, {C5, 300}, {C5, 300}, {D5, 300}, {E5, 300}, {C5, 300},
    {E5, 300}, {F5, 300}, {G5, 600}, {E5, 300}, {F5, 300}, {G5, 600},
    {G5, 150}, {A5, 150}, {G5, 150}, {F5, 150}, {E5, 300}, {C5, 300},
    {G5, 150}, {A5, 150}, {G5, 150}, {F5, 150}, {E5, 300}, {C5, 300},
    {C5, 300}, {G4, 300}, {C5, 600}, {C5, 300}, {G4, 300}, {C5, 600}, {R, 700},
};
// Twinkle Twinkle Little Star - traditional
static const Note TWINKLE[] = {
    {C5, 300}, {C5, 300}, {G5, 300}, {G5, 300}, {A5, 300}, {A5, 300}, {G5, 600},
    {F5, 300}, {F5, 300}, {E5, 300}, {E5, 300}, {D5, 300}, {D5, 300}, {C5, 600},
    {G5, 300}, {G5, 300}, {F5, 300}, {F5, 300}, {E5, 300}, {E5, 300}, {D5, 600},
    {G5, 300}, {G5, 300}, {F5, 300}, {F5, 300}, {E5, 300}, {E5, 300}, {D5, 600},
    {C5, 300}, {C5, 300}, {G5, 300}, {G5, 300}, {A5, 300}, {A5, 300}, {G5, 600},
    {F5, 300}, {F5, 300}, {E5, 300}, {E5, 300}, {D5, 300}, {D5, 300}, {C5, 600}, {R, 800},
};
static const Note BIRDS[] = {
    {2794, 45}, {3136, 45}, {3520, 60}, {R, 90}, {2960, 35}, {3322, 35}, {R, 380},
    {3520, 40}, {3136, 40}, {2794, 60}, {R, 140}, {3729, 30}, {R, 40}, {3729, 30}, {R, 40}, {3951, 50}, {R, 700},
};

// ---- Effects (play once) ------------------------------------------------------
static const Note FX_CLICK[] = {{1760, 18}};
static const Note FX_TASK_DONE[] = {{C5, 80}, {E5, 80}, {G5, 80}, {C6, 200}};
static const Note FX_REWARD[] = {{G5, 100}, {C6, 100}, {E6, 100}, {G6, 220}, {R, 60}, {E6, 90}, {G6, 400}};
static const Note FX_NOTIFY[] = {{E5, 120}, {R, 40}, {C6, 220}};
static const Note FX_MORNING[] = {{G4, 160}, {C5, 160}, {E5, 160}, {G5, 360}};
// Brahms' Lullaby (Wiegenlied) - public domain
static const Note FX_BEDTIME[] = {
    {E4, 260}, {E4, 260}, {G4, 700}, {E4, 260}, {E4, 260}, {G4, 700},
    {E4, 260}, {G4, 260}, {C5, 520}, {B4, 520}, {A4, 520}, {A4, 260}, {G4, 700},
    {D4, 260}, {E4, 260}, {F4, 520}, {D4, 520}, {D4, 260}, {E4, 260}, {F4, 900},
    {D4, 260}, {F4, 260}, {B4, 260}, {A4, 260}, {G4, 520}, {B4, 520}, {C5, 1000},
};

#define KC_MELODY(x) x, (uint16_t)(sizeof(x) / sizeof((x)[0]))

}  // namespace melodies

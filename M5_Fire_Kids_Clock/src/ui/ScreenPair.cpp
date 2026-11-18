// "Pair phone app": shows the 6-digit code the companion app needs once.
#include "Screens.h"
#include "../api/Pairing.h"
#include "../api/ApiServer.h"
#include "../display/Theme.h"
#include "../display/Characters.h"
#include "../wifi/WiFiLink.h"

using namespace theme;

void PairScreen::onEnter() {
  if (Net.connected()) Pair.startWindow();
}

void PairScreen::onExit() { Pair.cancelWindow(); }

void PairScreen::onEvent(const InputEvent& e) {
  if (e.btn == Btn::A && (e.type == Press::Short || e.type == Press::Long)) {
    UI.click();
    UI.back();
  } else if (e.btn == Btn::B && e.type == Press::Short && Net.connected()) {
    UI.click();
    Pair.startWindow();
  } else if (e.btn == Btn::C && e.type == Press::Short && Pair.count() > 0) {
    UI.click();
    ConfirmDlg.ask("Remove phones?", "All paired phones lose\naccess to this clock.", [] {
      Pair.revokeAll();
      UI.toast("Phones removed");
    });
  }
}

void PairScreen::render(Canvas& c, uint32_t now) {
  c.fillScreen(CREAM);
  draw::header(c, "Pair Phone App", MINT, INK);
  if (!Net.connected()) {
    chars::draw(c, chars::Kind::Bunny, 70, 120, 28, now, chars::Mood::Happy, CREAM);
    draw::text(c, "Connect Wi-Fi first:", 120, 92, draw::FONT_BODY, INK);
    draw::text(c, "Settings > Parent setup", 120, 116, draw::FONT_SMALL, INK_SOFT);
    draw::hintBar(c, "Back", nullptr, nullptr);
    return;
  }
  draw::card(c, 16, 44, 288, 96, 20, SNOW, SHADOW, 3);
  if (Pair.lockedOut()) {
    draw::textC(c, "Too many wrong codes", 160, 80, draw::FONT_BODY, RED_DEEP);
    draw::textC(c, "wait a minute, then New code", 160, 106, draw::FONT_SMALL, INK_SOFT);
  } else if (Pair.windowOpen()) {
    char shown[8];
    snprintf(shown, sizeof shown, "%.3s %.3s", Pair.code(), Pair.code() + 3);
    draw::textC(c, shown, 160, 86, draw::FONT_HUGE, PINK_DEEP);
    char left[32];
    const uint32_t s = Pair.secondsLeft();
    snprintf(left, sizeof left, "code works for %lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    draw::textC(c, left, 160, 124, draw::FONT_SMALL, INK_SOFT);
  } else {
    draw::textC(c, "Code expired", 160, 80, draw::FONT_TITLE, INK_SOFT);
    draw::textC(c, "press New code", 160, 110, draw::FONT_SMALL, INK_SOFT);
  }
  draw::text(c, "Type this code in the Kids Clock app.", 16, 150, draw::FONT_SMALL, INK);
  char line[48];
  snprintf(line, sizeof line, "Clock: %s.local  (%s)", "kidsclock", Net.ip().toString().c_str());
  draw::text(c, line, 16, 170, draw::FONT_SMALL, INK_SOFT);
  snprintf(line, sizeof line, "Paired phones: %d of %d", Pair.count(), Pairing::kMaxPhones);
  draw::text(c, line, 16, 190, draw::FONT_SMALL, INK_SOFT);
  draw::hintBar(c, "Back", "New code", Pair.count() ? "Remove" : nullptr);
}

# Roadmap

## Next

- Verify and, if needed, fix the build on Arduino-ESP32 core 3.x (current Arduino IDE default).
- Test the parent web pages end-to-end from a phone; add a "test alarm" button there.
- Battery-only and multi-day stability tests; measure clock drift without Wi-Fi.
- Bedtime story / white-noise mode using microSD WAV loops with a sleep timer.

## Ideas

- **Ok-to-wake light:** the LED bar turns green when it is OK to get out of bed (common
  in children's sleep clocks), red before that.
- Shake-to-snooze or flip-to-silence using the MPU6886 IMU.
- Countdown timer for tooth brushing (2 minutes, with a song).
- Weekly star chart and parent-set rewards ("10 stars = park trip").
- Multiple children / profiles.
- Voice: record the parent's own "wake up" message to microSD from a phone.
- Hindi and other Indian-language packs for labels and greetings.
- Holiday mode: skip school-day alarms on chosen dates.
- OTA firmware updates from the parent page.
- External RTC unit (BM8563, driver already included) for time after power loss without Wi-Fi.
- Optional MP3 decoding if a bigger external speaker is used.

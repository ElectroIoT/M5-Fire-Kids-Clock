// M5 Fire Kids Clock - hardware pin reference for the ORIGINAL M5Stack Fire (K007).
//
// The display, buttons, speaker, IMU and power IC are configured by M5Unified,
// which auto-detects the Fire as board_M5Stack. These constants exist so the
// rest of the firmware never hard-codes a pin, and as documentation.
//
// Source: docs.m5stack.com/en/core/Fire (PinMap, rev. 2026-11-06) and
// M5Unified 0.3.0 pin tables (board_M5Stack).
#pragma once

namespace pins {

// ILI9342C 320x240 IPS (handled by M5GFX)
constexpr int LCD_MOSI = 23;
constexpr int LCD_MISO = 19;
constexpr int LCD_SCK  = 18;
constexpr int LCD_CS   = 14;
constexpr int LCD_DC   = 27;
constexpr int LCD_RST  = 33;
constexpr int LCD_BL   = 32;

// microSD (shares the LCD SPI bus)
constexpr int SD_CS    = 4;

// Front buttons (handled by M5Unified)
constexpr int BTN_A    = 39;
constexpr int BTN_B    = 38;
constexpr int BTN_C    = 37;

// Speaker: internal DAC1 on GPIO25 (handled by M5Unified Speaker, I2S DAC mode)
constexpr int SPEAKER  = 25;

// M5GO base: 10 x SK6812 3535 RGB LEDs on one data line
constexpr int RGB_LED  = 15;
constexpr int RGB_LED_COUNT = 10;

// M5GO base analog MEMS microphone (ADC1). Not used by this firmware.
constexpr int MIC      = 34;

// Internal I2C (also Grove Port A): IP5306 0x75, MPU6886 0x68 / SH200Q 0x6C, BMM150 0x10
constexpr int I2C_SDA  = 21;
constexpr int I2C_SCL  = 22;

// Grove Port B: G26 (DAC/GPIO) + G36 (input only)
constexpr int PORT_B_OUT = 26;
constexpr int PORT_B_IN  = 36;

// GPIO16 / GPIO17 are wired to the PSRAM on the Fire. NEVER use them
// (that also makes Grove Port C unusable while PSRAM is enabled).
constexpr int PSRAM_RESERVED_1 = 16;
constexpr int PSRAM_RESERVED_2 = 17;

}  // namespace pins

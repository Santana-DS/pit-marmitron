// =============================================================================
// User_Setup.h  —  TFT_eSPI library compile-time configuration
//
// TARGET : ST7789V  240×320  2.0" TFT (SPI)
// MCU    : ESP32-WROOM-32 (38-pin devkit)
//
// PLACEMENT:
//   Copy this file to:
//     .pio/libdeps/<env>/TFT_eSPI/User_Setup.h
//   OR place it in your project's include/ directory and set in platformio.ini:
//     build_flags = -DUSER_SETUP_LOADED -include User_Setup.h
//
// PIN RATIONALE (see display_manager.h for full conflict analysis):
//   MOSI=23, SCK=18  → VSPI hardware SPI, DMA-capable, no strapping conflict
//   CS=5             → VSPI default CS, internal pull-up safe at boot
//   DC=27             → Safe on boards without LED on GPIO2; swap to 27 if needed
//   RST=26            → Fully free pin
//   BL=32            → ADC1 ch4, LEDC PWM-capable for brightness control
// =============================================================================

// ── Step 1: Select driver ─────────────────────────────────────────────────────
// Only one driver define must be active. ST7789V is a superset of ST7789;
// using ST7789_DRIVER covers both silicon revisions.
// Hardware real: ST7789V. For Wokwi/ILI9341 simulation, swap these defines
// locally; do not enable both drivers in the same build.
#define ST7789_DRIVER
//#define ILI9341_DRIVER

// ── Step 2: Display geometry ──────────────────────────────────────────────────
// Physical pixel dimensions — independent of rotation.
// 240 wide × 320 tall = portrait native orientation.
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ── Step 3: Pin assignments ───────────────────────────────────────────────────
//#define TFT_MOSI  23   // SDA on display silkscreen
//#define TFT_SCLK  18   // SCL on display silkscreen
//#define TFT_CS     5   // Chip select (active LOW)
//#define TFT_DC    27   // Data/Command select
//#define TFT_RST   26   // Hardware reset (active LOW)
//#define TFT_BL    32   // Backlight enable — controlled via LEDC PWM in firmware

#define TFT_MOSI  23   // (Nativo VSPI) Backups: 13, 16, 17, 19.
#define TFT_SCLK  18   // (Nativo VSPI) Backups: 16, 17, 19, 21.
#define TFT_CS    5    // (Nativo VSPI) Backups: 13, 15, 21, 22.
#define TFT_DC    21   // Backups: 2, 13, 16, 17, 19.
#define TFT_RST   22   // Backups: 2, 13, 16, 17, 19.
#define TFT_BL    15   // Available in the current display wiring; validate boot behavior on hardware.


// ── Step 4: SPI speed ─────────────────────────────────────────────────────────
// ST7789V write cycle minimum: 16 ns → theoretical max ~62.5 MHz.
// ESP32 VSPI tops out at 80 MHz but signal integrity degrades above 40 MHz
// on unshielded jumper wires. 40 MHz is the practical safe ceiling for
// breadboard/devkit setups; drop to 27 MHz if you see display corruption.
#define SPI_FREQUENCY        40000000   // 40 MHz write
#define SPI_READ_FREQUENCY    6000000   // 6 MHz read (register reads only)

// ── Step 5: Colour order ──────────────────────────────────────────────────────
// Most ST7789V panels ship with BGR byte order internally.
// If colours appear inverted (red↔blue swapped), flip this define.
#define TFT_RGB_ORDER TFT_BGR

// ── Step 6: Font and feature inclusions ───────────────────────────────────────
// Load only what we use — keeps flash footprint minimal.
// Smooth (anti-aliased) fonts require SPIFFS and are not needed for QR rendering.
#define LOAD_GLCD    // Font 1 — Adafruit-style 5×7 pixel font (status overlays)
#define LOAD_FONT2   // Font 2 — 16px (labels)
#define LOAD_FONT4   // Font 4 — 26px (OTP digit overlay, "ABERTO!" text)
#define LOAD_FONT6   // Font 6 — 48px large digits (used for OTP display)
#define LOAD_FONT7   // Font 7 — 7-segment style, no chips, no borders. Just four huge digits with breathing room between them — legible at a glance, which is the entire point of a fallback code.

// ── Step 7: DMA ───────────────────────────────────────────────────────────────
// Enable DMA transfers so pushImage/pushRect run in background, freeing
// the CPU for JSON parsing or MQTT keep-alive during large pixel fills.
#define ESP32_DMA

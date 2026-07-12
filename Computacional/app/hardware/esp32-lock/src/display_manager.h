// =============================================================================
// display_manager.h
// MARMITRON 3000 — Display Subsystem
// ST7789V · 320×240 LANDSCAPE · TFT_eSPI
// =============================================================================

#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <qrcode.h>

// ── Logo asset ────────────────────────────────────────────────────────────────
static constexpr uint16_t LOGO_W = 200;
static constexpr uint16_t LOGO_H = 200;
extern const uint16_t marmitron_logo[40000];

// ── Panel ─────────────────────────────────────────────────────────────────────
static constexpr uint16_t PANEL_W = 320;
static constexpr uint16_t PANEL_H = 240;

// ── Backlight ─────────────────────────────────────────────────────────────────
// Must match TFT_BL in include/User_Setup.h. GPIO 32 belongs to the
// controller ESP's left encoder, so it cannot drive the display backlight.
static constexpr uint8_t  PIN_BL        = 15;
static constexpr uint8_t  BL_CHANNEL    = 0;
static constexpr uint8_t  BL_RESOLUTION = 8;
static constexpr uint32_t BL_FREQ_HZ    = 5000;

// ── Palette ───────────────────────────────────────────────────────────────────
static constexpr uint16_t C_BLACK   = 0x0000;
static constexpr uint16_t C_WHITE   = 0xFFFF;
static constexpr uint16_t C_GREEN   = 0x07E0;
static constexpr uint16_t C_RED     = 0xF800;
static constexpr uint16_t C_CYAN    = 0x07FF;
static constexpr uint16_t C_AMBER   = 0xFDC0;
static constexpr uint16_t C_GREY    = 0x4208;
static constexpr uint16_t C_DKGREY  = 0x2104;
static constexpr uint16_t C_MIDGREY = 0x8410;  // subtitle text

// UnB brand gradient colours (RGB565, computed from official hex values)
// UnB Green #007934 → 0x03C6    White → 0xFFFF    UnB Blue #0033A0 → 0x0194
static constexpr uint16_t C_UNB_GREEN = 0x03C6;
static constexpr uint16_t C_UNB_BLUE  = 0x0194;

// ── QR — landscape split layout ───────────────────────────────────────────────
// Left zone (0..219): 10px quiet margin + 200px QR + 10px quiet margin = 220px
// Right zone (220..319): 100px — four stacked Font7 OTP digits
static constexpr uint8_t  QR_VERSION = 2;
static constexpr uint8_t  QR_ECC     = 1;
static constexpr uint8_t  QR_MODULES = 25;
static constexpr uint8_t  QR_MOD_PX  = 8;
static constexpr uint16_t QR_SIDE    = QR_MODULES * QR_MOD_PX;  // 200
static constexpr uint16_t QR_QUIET   = 10;
static constexpr uint16_t QR_LEFT    = QR_QUIET;
static constexpr uint16_t QR_TOP     = (PANEL_H - QR_SIDE) / 2;  // 20
static constexpr uint16_t OTP_COL_X  = QR_SIDE + QR_QUIET * 2;   // 220
static constexpr uint16_t OTP_COL_W  = PANEL_W - OTP_COL_X;      // 100

// ── Cart animation ────────────────────────────────────────────────────────────
static constexpr uint16_t CART_W     = 70;
static constexpr uint16_t CART_H     = 50;
// CART_Y chosen so cart bottom (y = CART_Y + CART_H) = TRACK_Y exactly.
// TRACK_Y = 178, so CART_Y = 178 - 50 = 128.
static constexpr uint16_t TRACK_Y    = 178;
static constexpr uint16_t TRACK_H    = 8;
static constexpr int16_t  CART_Y     = (int16_t)(TRACK_Y - CART_H);  // 128
static constexpr int16_t  CART_SPEED = 4;   // px per tick at 30ms → ~133 px/s

// ── Unlock sprite — centralizado no topo ──────────────────────────────────────
static constexpr uint16_t SPR_UNL_W   = 120;
static constexpr uint16_t SPR_UNL_H   = 120;
static constexpr int16_t  SPR_UNL_X   = (PANEL_W - SPR_UNL_W) / 2;  // 100 (Centro exato)
static constexpr int16_t  SPR_UNL_Y   = 5;                          // Colado no topo
static constexpr int16_t  UNL_CIRC_CX = SPR_UNL_W / 2;              // 60
static constexpr int16_t  UNL_CIRC_CY = SPR_UNL_H / 2;              // 60
static constexpr int16_t  UNL_CIRC_R  = 45;                         // Bola um pouco menor
static constexpr uint16_t RING_TARGET = 55;                         // Limite do anel
static constexpr uint16_t RING_STEP   = 4;
static constexpr uint16_t ABERTO_CX   = PANEL_W / 2;                // 160

// ── Boot sprite — covers centre 160×160 around the logo ──────────────────────
// Logo is 150×150; we give it 5px breathing room each side → 160×160.
// Pushed to (80, 45) so logo TFT coords = (85, 50).
// Using the sprite for boot lets the halo draw behind the logo cleanly.
// 160×160×2 = 51 200 bytes.
static constexpr uint16_t SPR_BOOT_W = 180;
static constexpr uint16_t SPR_BOOT_H = 180;
static constexpr int16_t  SPR_BOOT_X = (PANEL_W - SPR_BOOT_W) / 2;
static constexpr int16_t  SPR_BOOT_Y = (PANEL_H - SPR_BOOT_H) / 2 - 15; // Subimos um pouco para dar espaço ao texto

// ── Screen modes ──────────────────────────────────────────────────────────────
enum class ScreenMode : uint8_t {
    NONE,
    BOOTING,
    IDLE,
    OFFLINE,
    QR_CODE,
    UNLOCK_SUCCESS,
    ERROR_FAULT,
};

// =============================================================================
class DisplayManager {
public:
    bool begin();

    // ── Screen entry points ───────────────────────────────────────────────────
    void showBooting();
    void showIdle();
    void showOffline();
    void showQrCode(const char* otp);
    void showUnlockSuccess(const char* orderId);
    void showError();

    // ── Per-frame ticks ───────────────────────────────────────────────────────
    // tickBooting()       call every 20 ms  — backlight breath
    // tickIdle()          call every 30 ms  — cart animation
    // tickUnlockSuccess() call every 16 ms  — ring expansion (self-terminates)
    void tickBooting();
    void tickIdle();
    void tickUnlockSuccess();

    void setBrightness(uint8_t v);
    bool       isReady() const { return _ready; }
    ScreenMode mode()    const { return _mode;  }

private:
    bool       _ready = false;
    ScreenMode _mode  = ScreenMode::NONE;

    TFT_eSPI    _tft;

    // _sprMain is allocated to whichever size is needed on screen entry;
    // createSprite() is called in each showX() — size changes are explicit.
    // Max: SPR_UNL_W*H = 180×200 = 72 000 bytes.
    // Boot sprite: 160×160 = 51 200 bytes.
    // Only one is live at a time — we delete + recreate on mode change.
    TFT_eSprite _sprMain = TFT_eSprite(&_tft);

    // Cart sprite — always 70×50 = 7 000 bytes, alive from begin() onwards.
    TFT_eSprite _sprCart = TFT_eSprite(&_tft);

    // ── Boot breath ───────────────────────────────────────────────────────────
    uint8_t  _breathIdx = 0;
    bool     _breathUp  = true;
    uint8_t  _breathLUT[32];

    // ── Unlock ring ───────────────────────────────────────────────────────────
    uint16_t _ringRadius = 0;

    // ── Cart ──────────────────────────────────────────────────────────────────
    int16_t _cartX = 0;

    // ── Unlock Text Toggle ────────────────────────────────────────────────────
    uint32_t _lastSubToggle = 0;
    bool     _subAlt        = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void _fullBlack();
    void _drawTrack();
    void _drawCart();
    void _drawCheckmark(TFT_eSprite& spr, int16_t cx, int16_t cy, int16_t r);
    void _pushCartClipped();
};

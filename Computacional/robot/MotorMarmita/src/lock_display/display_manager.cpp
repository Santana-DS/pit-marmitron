// =============================================================================
// display_manager.cpp
// MARMITRON 3000 — Display Subsystem  (Landscape 320×240)
// =============================================================================

#include "display_manager.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

namespace {
void setupBacklight() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_BL, BL_FREQ_HZ, BL_RESOLUTION);
#else
    ledcSetup(BL_CHANNEL, BL_FREQ_HZ, BL_RESOLUTION);
    ledcAttachPin(PIN_BL, BL_CHANNEL);
#endif
}

void writeBacklight(uint8_t value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_BL, value);
#else
    ledcWrite(BL_CHANNEL, value);
#endif
}
}  // namespace

// =============================================================================
// begin()
// =============================================================================
bool DisplayManager::begin() {

    // ── Backlight ─────────────────────────────────────────────────────────────
    setupBacklight();
    writeBacklight(0);

    // ── Panel ─────────────────────────────────────────────────────────────────
    _tft.init();
    _tft.setRotation(1);          // Landscape, USB at right
    _tft.setTextWrap(false);
    _tft.fillScreen(C_BLACK);

    // ── Cart sprite — allocated once, lives forever ────────────────────────────
    _sprCart.setColorDepth(16);
    if (_sprCart.createSprite(CART_W, CART_H) == nullptr) {
        return false;
    }
    _sprCart.setTextWrap(false);

    // ── Breath LUT (32 steps, 80-255, cosine-approximated, no float in loop) ──
    static const uint8_t cosLUT[17] = {
        255, 253, 245, 233, 215, 194,
        170, 145, 120,  96,  75,  57,
         43,  34,  28,  26,  26
    };
    for (uint8_t i = 0; i < 32; i++) {
        uint8_t c = (i < 16) ? cosLUT[i] : cosLUT[32 - i];
        _breathLUT[i] = (uint8_t)(80 + ((uint32_t)(c - 26) * 175) / 229);
    }

    _ready = true;

    // ── Backlight fade-in ─────────────────────────────────────────────────────
    for (uint16_t v = 0; v <= 255; v += 5) {
        writeBacklight(v);
        delay(6);
    }

    return true;
}

void DisplayManager::setBrightness(uint8_t v) { writeBacklight(v); }

void DisplayManager::_fullBlack() {
    writeBacklight(255);
    _tft.fillScreen(C_BLACK);
}

// =============================================================================
// =============================================================================
//  BOOT  (Landscape 320×240)
// -----------------------------------------------------------------------------
//  The logo breathes via backlight PWM — no per-frame SPI writes.
//  "Iniciando..." at bottom-centre, static, Font 2 (appropriate for a caption
//  in a minimalist boot screen — the logo is the hero, not the text).
// =============================================================================
// =============================================================================

void DisplayManager::showBooting() {
    if (!_ready) return;
    _mode      = ScreenMode::BOOTING;
    _breathIdx = 0;
    _breathUp  = true;

    _sprMain.deleteSprite();
    _tft.fillScreen(C_BLACK);

    // Renderiza o novo logo de 180x180
    _tft.setSwapBytes(true);
    // Puxado para o eixo Y = 5
    _tft.pushImage((PANEL_W - LOGO_W) / 2, 5, LOGO_W, LOGO_H, marmitron_logo);
    _tft.setSwapBytes(false);

    // "Iniciando..." — Mudou para C_WHITE e Font 4 (Maior e mais legível)
    _tft.setTextDatum(BC_DATUM);
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.drawString("Iniciando...", PANEL_W / 2, PANEL_H - 5, 4);

    writeBacklight(_breathLUT[0]);
}

void DisplayManager::tickBooting() {
    if (!_ready || _mode != ScreenMode::BOOTING) return;

    writeBacklight(_breathLUT[_breathIdx]);

    if (_breathUp) {
        if (++_breathIdx >= 32) { _breathIdx = 30; _breathUp = false; }
    } else {
        if (_breathIdx == 0)   { _breathIdx =  1; _breathUp = true;  }
        else                   { --_breathIdx; }
    }
}

// =============================================================================
// =============================================================================
//  IDLE  (Landscape 320×240)
// -----------------------------------------------------------------------------
//  Static layer (drawn once in showIdle):
//    "MARMITRON" Font 4 white + "3000" Font 4 cyan, paired and centred.
//    Subtitle "Aguardando pedido" Font 4, muted, below the track.
//    UnB gradient track at y=TRACK_Y, 8px tall, full width.
//
//  Animated layer (tickIdle, every 30ms):
//    Delivery bot sprite travels left→right.
//    Erase covers only y=CART_Y..TRACK_Y-1 (above the track) so the
//    gradient road is never overwritten.
// =============================================================================
// =============================================================================

void DisplayManager::showIdle() {
    if (!_ready) return;
    _mode  = ScreenMode::IDLE;
    _cartX = 0;

    _sprMain.deleteSprite();

    _fullBlack();

    // ── Título GIGANTE com multiplicador de tamanho ────────────────────────
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextSize(2); // Multiplica o tamanho da fonte por 2 (Fica com 52px!)
    
    _tft.setTextColor(C_UNB_GREEN, C_BLACK);
    _tft.drawString("MARMITRON", PANEL_W / 2, 20, 4); // Usa a Font 4 (com letras)
    
    _tft.setTextColor(C_UNB_BLUE, C_BLACK);
    _tft.drawString("3000", PANEL_W / 2, 75, 4);

    _tft.setTextSize(1); // OBRIGATÓRIO: Volta ao tamanho normal para não bugar o resto!

    // ── Subtítulo ─────────────────────────────────────────────────────────────
    const uint16_t subY = TRACK_Y + TRACK_H + 10;
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.drawString("Aguardando pedido", PANEL_W / 2, subY, 4);

    _drawTrack();
    tickIdle();
}

void DisplayManager::showOffline() {
    if (!_ready) return;
    _mode = ScreenMode::OFFLINE;
    _sprMain.deleteSprite();
    _fullBlack();

    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(C_UNB_GREEN, C_BLACK);
    _tft.drawString("MARMITRON", PANEL_W / 2, 28, 4);
    _tft.setTextColor(C_UNB_BLUE, C_BLACK);
    _tft.drawString("3000", PANEL_W / 2, 70, 4);

    _tft.setTextColor(C_AMBER, C_BLACK);
    _tft.drawString("SEM CONEXAO", PANEL_W / 2, 125, 4);
    _tft.setTextColor(C_MIDGREY, C_BLACK);
    _tft.drawString("Display operacional", PANEL_W / 2, 170, 2);
    _tft.drawString("Aguardando Wi-Fi e MQTT", PANEL_W / 2, 195, 2);
}

// =============================================================================
// _drawTrack() — UnB gradient road, drawn ONCE in showIdle()
// -----------------------------------------------------------------------------
// fillRectHGradient(x, y, w, h, color1, color2) interpolates horizontally.
// Left half:  UnB green → white
// Right half: white → UnB blue
// A second hairline (1px) of pure black above the track gives the road a
// clean "kerb" separation from the black background above it.
// =============================================================================
void DisplayManager::_drawTrack() {
    const uint16_t halfW = PANEL_W / 2;  // 160

    // Kerb line
    _tft.drawFastHLine(0, TRACK_Y - 1, PANEL_W, C_DKGREY);

    // Left half: green → white
    _tft.fillRectHGradient(0, TRACK_Y, halfW, TRACK_H, C_UNB_GREEN, C_WHITE);

    // Right half: white → blue
    _tft.fillRectHGradient(halfW, TRACK_Y, halfW, TRACK_H, C_WHITE, C_UNB_BLUE);
}

// =============================================================================
// _drawCart() — delivery bot drawn into _sprCart (70×50)
// -----------------------------------------------------------------------------
// Bot anatomy (sprite-local coords, bot faces RIGHT):
//
//   ┌──────────────────────────────────────────────────────┐ y=0
//   │  [●] antenna tip (cyan dot, x=22, y=0)              │
//   │   |  antenna stem (white vline x=22, y=1..5)        │
//   │  ┌────────────────┐  cargo pod (dark+amber glow)     │ y=4
//   │  │ ░░░░░░░░░░░░░░ │  amber rect = payload           │ y=7..13
//   │  └────────────────┘                                  │ y=16
//   │  ┌──────────────────────────────────────┐            │ y=16
//   │  │           CHASSIS (grey)             │◣ nose      │
//   │  │  ●  thruster glow (amber dots, x=4)  │  wedge     │ y=16..38
//   │  └──────────────────────────────────────┘            │ y=38
//   │      ⊙ rear wheel (x=16)   ⊙ front wheel (x=52)    │ y=32..45
//   └──────────────────────────────────────────────────────┘ y=50
//
// Wheels are tangent to TRACK_Y (sprite bottom = TRACK_Y, wheels at y=38
// + r=7 = y=45 which is inside the 50px sprite). The track gradient starts
// at y=45 in TFT coords — wheels appear to rest ON the road.
// =============================================================================
void DisplayManager::_drawCart() {
    _sprCart.fillSprite(C_BLACK);

    // ── Matemática de Animação baseada na posição (X) do carrinho ───────────
    // A cada avanço de X, as partes do carrinho reagem:
    int8_t bob = (_cartX % 12 < 6) ? 0 : 1;    // Suspensão: chassi pula 1 pixel
    int8_t thrust = (_cartX % 8 < 4) ? 2 : 5;  // Fogo pulsa em comprimento

    // ── Fogo do propulsor (Animado) ───────────────────────────────────────────
    _sprCart.fillTriangle(0, 24 + bob, thrust, 22 + bob, thrust, 26 + bob, C_AMBER);
    _sprCart.fillTriangle(0, 30 + bob, thrust, 28 + bob, thrust, 32 + bob, C_AMBER);

    // ── Chassis body (Reage à suspensão) ──────────────────────────────────────
    _sprCart.fillRoundRect(thrust + 2, 16 + bob, 52, 22, 4, C_GREY);
    _sprCart.fillTriangle(thrust + 50, 16 + bob, thrust + 50, 38 + bob, thrust + 62, 27 + bob, C_GREY);

    // ── Cargo pod (Baú iluminado) ─────────────────────────────────────────────
    _sprCart.fillRoundRect(12, 4 + bob, 34, 13, 3, C_DKGREY);
    _sprCart.drawRoundRect(12, 4 + bob, 34, 13, 3, C_CYAN);
    _sprCart.fillRect(16, 7 + bob, 26, 7, C_AMBER); // Produto quente dentro!

    // ── Antenna (Piscando e balançando) ───────────────────────────────────────
    _sprCart.drawFastVLine(24, 1 + bob, 5, C_WHITE);
    uint16_t antColor = (_cartX % 20 < 10) ? C_CYAN : C_RED; // Pisca
    _sprCart.fillCircle(24, bob, 2, antColor);

    // ── Wheels (Falsificando a rotação) ───────────────────────────────────────
    // As rodas não sofrem o 'bob' da suspensão (ficam coladas no chão)
    int16_t w1x = 18, w2x = 52, wy = 39;
    
    // Pneu e calota
    _sprCart.fillCircle(w1x, wy, 8, C_DKGREY);
    _sprCart.drawCircle(w1x, wy, 8, C_GREY);
    _sprCart.fillCircle(w2x, wy, 8, C_DKGREY);
    _sprCart.drawCircle(w2x, wy, 8, C_GREY);

    // Simulação do aro girando (alterna a linha dependendo da posição X)
    int8_t spin = (_cartX / 4) % 3;
    if (spin == 0) {
        _sprCart.drawFastHLine(w1x - 4, wy, 9, C_WHITE);
        _sprCart.drawFastHLine(w2x - 4, wy, 9, C_WHITE);
    } else if (spin == 1) {
        _sprCart.drawLine(w1x - 3, wy - 3, w1x + 3, wy + 3, C_WHITE);
        _sprCart.drawLine(w2x - 3, wy - 3, w2x + 3, wy + 3, C_WHITE);
    } else {
        _sprCart.drawLine(w1x - 3, wy + 3, w1x + 3, wy - 3, C_WHITE);
        _sprCart.drawLine(w2x - 3, wy + 3, w2x + 3, wy - 3, C_WHITE);
    }
}

// =============================================================================
// _pushCartClipped() — safe push handling negative X (left-edge entry)
// =============================================================================
void DisplayManager::_pushCartClipped() {
    if (_cartX >= (int16_t)PANEL_W || _cartX <= -(int16_t)CART_W) return;

    if (_cartX >= 0) {
        // Fully on-screen or clipped by right edge (TFT_eSPI handles right clip)
        _sprCart.pushSprite(_cartX, CART_Y);
    } else {
        // Partially off left edge — blit only the visible portion.
        // TFT_eSPI does NOT guarantee safe behaviour with negative x in
        // pushSprite(), so we manually blit row-by-row using pushImage().
        int16_t  clipX = -_cartX;              // first visible column in sprite
        uint16_t visW  = CART_W - clipX;
        uint16_t* buf  = (uint16_t*)_sprCart.getPointer();

        for (uint8_t row = 0; row < CART_H; row++) {
            _tft.pushImage(0, CART_Y + row, visW, 1,
                           buf + row * CART_W + clipX);
        }
    }
}

// =============================================================================
// tickIdle() — call every 30ms
// =============================================================================
void DisplayManager::tickIdle() {
    if (!_ready || _mode != ScreenMode::IDLE) return;

    // ── Erase previous cart position ──────────────────────────────────────────
    // Only erase the region ABOVE the track (y = CART_Y to TRACK_Y-1).
    // Height = TRACK_Y - CART_Y = 178 - 128 = 50 = CART_H exactly.
    // This means we never touch the gradient road, which stays permanent.
    if (_cartX > -(int16_t)CART_W && _cartX < (int16_t)PANEL_W) {
        int16_t  ex = _cartX;
        uint16_t ew = CART_W;

        if (ex < 0) { ew += ex; ex = 0; }                        // left clip
        if ((uint16_t)(ex + ew) > PANEL_W) { ew = PANEL_W - ex; } // right clip

        if (ew > 0) {
            // Erase from CART_Y, height = TRACK_Y - CART_Y (stops at track top)
            _tft.fillRect(ex, CART_Y, ew, TRACK_Y - CART_Y, C_BLACK);
        }
    }

    // ── Advance X ─────────────────────────────────────────────────────────────
    _cartX += CART_SPEED;
    if (_cartX >= (int16_t)PANEL_W) {
        _cartX = -(int16_t)CART_W;   // wrap to just off left edge
    }

    // ── Draw and push ─────────────────────────────────────────────────────────
    _drawCart();
    _pushCartClipped();
}

// =============================================================================
// =============================================================================
//  QR CODE  (Landscape 320×240)
// -----------------------------------------------------------------------------
//  Left zone (0..219): white quiet zone + 200×200 QR matrix.
//  Right zone (220..319, 100px): four Font 7 digits stacked vertically,
//  each centred in the column.
//  Layout: 4 × 48px + 3 × 8px gap = 216px, starts at y=(240-216)/2=12. ✓
// =============================================================================
// =============================================================================

void DisplayManager::showQrCode(const char* otp) {
    if (!_ready) return;
    _mode = ScreenMode::QR_CODE;

    _sprMain.deleteSprite();

    QRCode  qr;
    uint8_t qrBuf[qrcode_getBufferSize(QR_VERSION)];

    if (qrcode_initText(&qr, qrBuf, QR_VERSION, QR_ECC, otp) != 0) {
        showError();
        return;
    }

    _fullBlack();

    // ── Left zone: quiet zone + QR ─────────────────────────────────────────────
    _tft.fillRect(0, QR_TOP - QR_QUIET,
                  QR_SIDE + QR_QUIET * 2, QR_SIDE + QR_QUIET * 2,
                  C_WHITE);

    for (uint8_t row = 0; row < qr.size; row++) {
        for (uint8_t col = 0; col < qr.size; col++) {
            if (qrcode_getModule(&qr, col, row)) {
                _tft.fillRect(
                    QR_LEFT + col * QR_MOD_PX,
                    QR_TOP  + row * QR_MOD_PX,
                    QR_MOD_PX, QR_MOD_PX,
                    C_BLACK
                );
            }
        }
    }

    // ── Right zone: 4 Font 7 digits stacked ───────────────────────────────────
    // Font 7: 7-segment numerals, 48px tall, centred in the 100px column.
    static constexpr uint8_t  DIGIT_FONT = 7;
    static constexpr uint16_t DIGIT_H    = 48;
    static constexpr uint16_t DIGIT_GAP  = 8;
    const uint16_t totalH  = 4 * DIGIT_H + 3 * DIGIT_GAP;   // 216
    uint16_t       startY  = (PANEL_H - totalH) / 2;         // 12
    const uint16_t colCX   = OTP_COL_X + OTP_COL_W / 2;     // 270

    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(C_WHITE, C_BLACK);

    for (uint8_t i = 0; i < 4 && otp[i] != '\0'; i++) {
        char ch[2] = { otp[i], '\0' };
        _tft.drawString(ch, colCX, startY + i * (DIGIT_H + DIGIT_GAP), DIGIT_FONT);
    }

    // Thin vertical hairline separating zones — barely visible
    _tft.drawFastVLine(OTP_COL_X - 2, 20, PANEL_H - 40, C_DKGREY);
}

// =============================================================================
// =============================================================================
//  UNLOCK SUCCESS  (Landscape 320×240)
// -----------------------------------------------------------------------------
//  Split layout:
//    LEFT  (sprite 180×200 pushed at x=0, y=20):
//      Green filled circle r=70, centre at sprite-local (90,100) = TFT (90,120).
//      Expanding ring from circle edge to RING_TARGET=90, then holds.
//      White checkmark inside circle.
//    RIGHT (drawn once on TFT, static):
//      "ABERTO"           Font 4, white, centred at TFT (249, 108)
//      "Retire a marmita" Font 4, grey,  centred at TFT (249, 144)
//
//  "ABERTO" in Font 4: ~120px wide, centred at x=249 → x=189..309 ✓
// =============================================================================
// =============================================================================

// Não esqueça de adicionar o argumento na função
void DisplayManager::showUnlockSuccess(const char* orderId) {
    if (!_ready) return;
    _mode        = ScreenMode::UNLOCK_SUCCESS;
    _ringRadius  = 0;

    _sprMain.deleteSprite();
    _sprMain.setColorDepth(16);
    if (_sprMain.createSprite(SPR_UNL_W, SPR_UNL_H) == nullptr) {
        _fullBlack();
        _tft.fillCircle(SPR_UNL_X + UNL_CIRC_CX, SPR_UNL_Y + UNL_CIRC_CY, UNL_CIRC_R, C_GREEN);
    } else {
        _sprMain.setTextWrap(false);
        _fullBlack();
    }

    // ── Textos Centralizados em Tela Cheia (Sem sobreposição) ────────────────
    _tft.setTextDatum(MC_DATUM);
    
    // "ABERTO" gigante (usando todos os 320px de largura)
    _tft.setTextSize(2);
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.drawString("ABERTO", ABERTO_CX, 148, 4);
    _tft.setTextSize(1);

    // Cortador do número do pedido (Ciano)
    if (orderId && orderId[0] != '\0') {
        size_t len = strlen(orderId);
        const char* shortId = (len > 6) ? orderId + (len - 6) : orderId;
        
        char buf[32];
        snprintf(buf, sizeof(buf), "Pedido #%s", shortId);
        _tft.setTextColor(C_CYAN, C_BLACK);
        _tft.drawString(buf, ABERTO_CX, 187, 4);
    }

    // Prepara o cronômetro para a oscilação do texto
    _lastSubToggle = millis();
    _subAlt        = false;

    // Subtítulo desenhado com Padding para "limpar" o fundo automaticamente
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.setTextPadding(PANEL_W); // A largura do padding será a tela inteira (320px)
    _tft.drawString("Retire a marmita", ABERTO_CX, 222, 4);
    _tft.setTextPadding(0);       // Desliga o padding para não afetar futuros desenhos

    tickUnlockSuccess();
}

void DisplayManager::_drawCheckmark(TFT_eSprite& spr,
                                     int16_t cx, int16_t cy, int16_t r) {
    // Proportions tuned for r=70: left leg and right leg of the ✓
    int16_t pivX = cx - r / 8;
    int16_t pivY = cy + r / 3;
    int16_t lx   = cx - r * 6 / 10;
    int16_t ly   = cy;
    int16_t rx   = cx + r * 6 / 10;
    int16_t ry   = cy - r * 4 / 10;

    const int8_t stroke = 5;
    for (int8_t t = -stroke; t <= stroke; t++) {
        spr.drawLine(lx, ly + t, pivX, pivY + t, C_WHITE);
        spr.drawLine(pivX, pivY + t, rx, ry + t, C_WHITE);
    }
}

void DisplayManager::tickUnlockSuccess() {
    if (!_ready || _mode != ScreenMode::UNLOCK_SUCCESS) return;

    // If sprite allocation failed in showUnlockSuccess(), getPointer() is null.
    // Guard: skip animation, screen already has static content.
    if (_sprMain.getPointer() == nullptr) return;

    _sprMain.fillSprite(C_BLACK);

    // Green filled circle
    _sprMain.fillCircle(UNL_CIRC_CX, UNL_CIRC_CY, UNL_CIRC_R, C_GREEN);

    // White checkmark
    _drawCheckmark(_sprMain, UNL_CIRC_CX, UNL_CIRC_CY, UNL_CIRC_R);

    // Expanding ring — only while still growing, then holds at max
    if (_ringRadius > 0) {
        uint16_t rc = (_ringRadius < RING_TARGET / 2) ? C_GREEN : 0x0300;
        _sprMain.drawCircle(UNL_CIRC_CX, UNL_CIRC_CY, _ringRadius, rc);
        _sprMain.drawCircle(UNL_CIRC_CX, UNL_CIRC_CY, _ringRadius + 1, rc);
    }

    _sprMain.pushSprite(SPR_UNL_X, SPR_UNL_Y);

    // Advance ring — cap at RING_TARGET, then animation is effectively still
    if (_ringRadius < RING_TARGET) {
        _ringRadius += RING_STEP;
        if (_ringRadius > RING_TARGET) _ringRadius = RING_TARGET;
    }

    // ── Oscilação do Subtítulo (A cada 1.5 segundos) ────────────────────────
    uint32_t now = millis();
    if (now - _lastSubToggle > 1500) {
        _lastSubToggle = now;
        _subAlt = !_subAlt; // Inverte o estado (true/false)

        _tft.setTextDatum(MC_DATUM);
        _tft.setTextPadding(PANEL_W); // Limpa a linha toda ao escrever

        if (_subAlt) {
            _tft.setTextColor(C_CYAN, C_BLACK); // Chama atenção pro app na cor Ciano
            _tft.drawString("Avalie no app!", ABERTO_CX, 222, 4);
        } else {
            _tft.setTextColor(C_WHITE, C_BLACK); // Volta pro aviso principal em Branco
            _tft.drawString("Retire a marmita", ABERTO_CX, 222, 4);
        }
        
        _tft.setTextPadding(0); // Reseta o padding
    }
}

// =============================================================================
// =============================================================================
//  ERROR  (Landscape 320×240)
// -----------------------------------------------------------------------------
//  Giant red X centred at (160, 104).
//  Two-line text block beneath:
//    "FALHA"        Font 6, white, y=163
//    "NO SISTEMA"   Font 4, grey,  y=219
//  Block centred vertically: total h=86px, starts y=77. ✓
//  Static — no tick needed.
// =============================================================================
// =============================================================================

void DisplayManager::showError() {
    if (!_ready) return;
    _mode = ScreenMode::ERROR_FAULT;

    _sprMain.deleteSprite();
    _fullBlack();

    const int16_t cx  = PANEL_W / 2;   // 160
    const int16_t cy  = 95;            // X centre, slightly above midpoint
    const int16_t arm = 46;
    const int8_t  stk = 6;

    // Thick red X
    for (int8_t t = -stk; t <= stk; t++) {
        _tft.drawLine(cx - arm, cy - arm + t, cx + arm, cy + arm + t, C_RED);
        _tft.drawLine(cx + arm, cy - arm + t, cx - arm, cy + arm + t, C_RED);
    }

    // "FALHA" — Font 6, white
    // Font 6 height = 48px. Sits at y = cy + arm + 16 = 95 + 46 + 16 = 157
    const uint16_t falhaY     = cy + arm + 16;
    const uint16_t nosistemaY = falhaY + 48 + 8;   // 213

    // "FALHA" gigante
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextSize(2);
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.drawString("FALHA", cx, falhaY, 4);
    _tft.setTextSize(1);

    // "NO SISTEMA"
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.drawString("NO SISTEMA", cx, nosistemaY, 4);
}

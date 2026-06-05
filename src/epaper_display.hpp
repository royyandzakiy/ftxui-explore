#pragma once

// Abstraction layer that mirrors the GxEPD2_BW / Adafruit_GFX drawing API.
// Firmware code calls IEpaperDisplay* and never touches GxEPD2 directly, so
// swapping in the real display on-device is a one-line change.

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/pixel.hpp>

// Match firmware color constants (GxEPD2 uses uint16_t; we use bool for 1-bit)
static constexpr bool GxEPD_BLACK = true;
static constexpr bool GxEPD_WHITE = false;

// ─────────────────────────────────────────────────────────────────────────────
// IEpaperDisplay — abstract interface matching GxEPD2_BW + Adafruit_GFX subset
//
// Firmware drawing code only depends on this type. On-device you wrap GxEPD2
// with a thin adapter that forwards each call. In the desktop emulator the
// EpaperBuffer below is the implementation.
// ─────────────────────────────────────────────────────────────────────────────
class IEpaperDisplay {
public:
    static constexpr int WIDTH  = 200;
    static constexpr int HEIGHT = 200;

    virtual ~IEpaperDisplay() = default;

    // --- primitives ---
    virtual void fillScreen(bool black)                                    = 0;
    virtual void drawPixel(int x, int y, bool black)                       = 0;
    virtual void drawBitmap(int x, int y, const uint8_t* bmp,
                            int w, int h, bool black)                      = 0;

    // --- text (Adafruit_GFX style) ---
    virtual void setRotation(int rot)                                      = 0;
    virtual void setCursor(int x, int y)                                   = 0;
    virtual void setFont(const void* /*font*/) {}
    virtual void print(std::string_view text)                              = 0;
    virtual void printf(const char* fmt, ...)                              = 0;

    // --- composed helpers (match Adafruit_GFX / GxEPD2 API used in firmware) ---
    void drawHLine(int x, int y, int w, bool black) {
        for (int i = 0; i < w; ++i) drawPixel(x + i, y, black);
    }
    void drawVLine(int x, int y, int h, bool black) {
        for (int i = 0; i < h; ++i) drawPixel(x, y + i, black);
    }
    void drawLine(int x0, int y0, int x1, int y1, bool black) {
        int dx =  std::abs(x1-x0), sx = x0<x1 ? 1 : -1;
        int dy = -std::abs(y1-y0), sy = y0<y1 ? 1 : -1;
        int err = dx+dy;
        while (true) {
            drawPixel(x0, y0, black);
            if (x0==x1 && y0==y1) break;
            int e2 = 2*err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    void drawRect(int x, int y, int w, int h, bool black) {
        drawHLine(x,         y,         w, black);
        drawHLine(x,         y + h - 1, w, black);
        drawVLine(x,         y,         h, black);
        drawVLine(x + w - 1, y,         h, black);
    }
    void fillRect(int x, int y, int w, int h, bool black) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
                drawPixel(x + i, y + j, black);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// EpaperBuffer — FTXUI-backed emulator
//
// Pixels are stored in firmware (logical) coordinates. render() applies the
// active rotation to produce the correct physical canvas layout, matching
// exactly what GxEPD2's setRotation() does on hardware.
//
// Text is overlaid via FTXUI DrawText (not pixel-rendered) and is
// approximately positioned after rotation. This is an intentional emulator
// trade-off — bitmaps and pixel draws are exact, text position is close.
//
// Usage:
//   buf.setRotation(2);  // match firmware
//   draw_my_screen(buf, ...);
//   screen.render(color(Color::Black, bgcolor(Color::White, buf.render())));
// ─────────────────────────────────────────────────────────────────────────────
struct TextCmd { int x, y; std::string text; };

class EpaperBuffer : public IEpaperDisplay {
public:
    void fillScreen(bool black) override { pixels_.fill(black); }

    void drawPixel(int x, int y, bool black) override {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            pixels_[y * WIDTH + x] = black;
    }

    // Adafruit-style bitmap: MSB-first, row-major, each row padded to byte boundary.
    void drawBitmap(int x0, int y0, const uint8_t* bmp,
                    int w, int h, bool black) override {
        const int bpr = (w + 7) / 8;
        for (int row = 0; row < h; ++row)
            for (int col = 0; col < w; ++col)
                if ((bmp[row * bpr + col / 8] >> (7 - col % 8)) & 1)
                    drawPixel(x0 + col, y0 + row, black);
    }

    void setRotation(int rot) override { rotation_ = rot & 3; }
    void setCursor(int x, int y) override { cx_ = x; cy_ = y; }
    void setFont(const void*) override {}

    void print(std::string_view text) override {
        text_cmds_.push_back({cx_, cy_, std::string(text)});
        cx_ += static_cast<int>(text.size() * 6); // ~6px/char — default GFX font
    }

    void printf(const char* fmt, ...) override {  // NOLINT — member, not ::printf
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        print(buf);
    }

    // Produce an ftxui canvas Element. The pixel buffer is transformed by the
    // active rotation so the canvas matches the physical display orientation.
    // Wrap the result:  color(Color::Black, bgcolor(Color::White, render()))
    [[nodiscard]] ftxui::Element render() const {
        auto c = ftxui::Canvas(WIDTH, HEIGHT);

        // Pixel buffer → braille dots, rotated to physical coordinates
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x)
                if (pixels_[y * WIDTH + x]) {
                    auto [px, py] = toPhysical(x, y);
                    c.DrawPoint(px, py, true);
                }

        // Text overlays — transform cursor then snap to braille grid.
        // For rotation=2, text is right-aligned at the physical cursor so it
        // matches the firmware's right-to-left text direction after the flip.
        for (const auto& t : text_cmds_) {
            auto [px, py] = toPhysical(t.x, t.y);
            // In rotation=2 text goes left from the physical cursor position;
            // subtract text width (each char = 2 braille units in canvas).
            int effective_x = px;
            if (rotation_ == 2)
                effective_x = px - static_cast<int>(t.text.size() * 2);
            effective_x = std::max(0, effective_x & ~1);
            c.DrawText(effective_x, (py / 4) * 4, t.text,
                       [](ftxui::Pixel& p) {
                           p.foreground_color = ftxui::Color::Black;
                           p.background_color = ftxui::Color::White;
                       });
        }

        return ftxui::canvas(std::move(c));
    }

    void reset() {
        pixels_.fill(false);
        text_cmds_.clear();
        cx_ = cy_ = 0;
        rotation_ = 0;
    }

private:
    // Transform logical (firmware) coords to physical (canvas) coords.
    [[nodiscard]] std::pair<int,int> toPhysical(int x, int y) const {
        switch (rotation_) {
            case 1: return {HEIGHT - 1 - y, x};
            case 2: return {WIDTH  - 1 - x, HEIGHT - 1 - y};
            case 3: return {y,               WIDTH  - 1 - x};
            default: return {x, y};
        }
    }

    std::array<bool, WIDTH * HEIGHT> pixels_{};
    std::vector<TextCmd>             text_cmds_;
    int cx_ = 0, cy_ = 0;
    int rotation_ = 0;
};

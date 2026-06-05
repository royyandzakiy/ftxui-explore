#pragma once

// Abstraction layer that mirrors the GxEPD2_BW / Adafruit_GFX drawing API.
// Firmware code calls IEpaperDisplay* and never touches GxEPD2 directly, so
// swapping in the real display on-device is a one-line change.

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/pixel.hpp>

// Match firmware color constants (GxEPD2 uses uint16_t; we use bool for 1-bit)
inline constexpr bool GxEPD_BLACK = true;
inline constexpr bool GxEPD_WHITE = false;

// ─────────────────────────────────────────────────────────────────────────────
// IEpaperDisplay — abstract interface matching GxEPD2_BW + Adafruit_GFX subset
//
// Firmware drawing code only depends on this type. On-device you wrap GxEPD2
// with a thin adapter that forwards each call. In the desktop emulator the
// EpaperBuffer below is the implementation.
// ─────────────────────────────────────────────────────────────────────────────
class IEpaperDisplay {
public:
    static constexpr int16_t WIDTH  = 200;
    static constexpr int16_t HEIGHT = 200;

    virtual ~IEpaperDisplay() = default;

    // --- primitives ---
    virtual void fillScreen(bool black)                                       = 0;
    virtual void drawPixel(int16_t x, int16_t y, bool black)                 = 0;
    virtual void drawBitmap(int16_t x, int16_t y, const uint8_t* bmp,
                            int16_t w, int16_t h, bool black)                = 0;

    // --- text (Adafruit_GFX style) ---
    virtual void setCursor(int16_t x, int16_t y)                             = 0;
    virtual void setFont(const void* /*font*/) {}                             // no-op default
    virtual void print(std::string_view text)                                 = 0;
    virtual void printf(const char* fmt, ...)                                 = 0;

    // --- composed helpers (match Adafruit_GFX free functions used in firmware) ---
    void drawHLine(int16_t x, int16_t y, int16_t w, bool black) {
        for (int16_t i = 0; i < w; ++i) drawPixel(x + i, y, black);
    }
    void drawVLine(int16_t x, int16_t y, int16_t h, bool black) {
        for (int16_t i = 0; i < h; ++i) drawPixel(x, y + i, black);
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black) {
        drawHLine(x,         y,         w, black);
        drawHLine(x,         y + h - 1, w, black);
        drawVLine(x,         y,         h, black);
        drawVLine(x + w - 1, y,         h, black);
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black) {
        for (int16_t j = 0; j < h; ++j)
            for (int16_t i = 0; i < w; ++i)
                drawPixel(x + i, y + j, black);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// EpaperBuffer — FTXUI-backed emulator
//
// Stores a 200×200 1-bit pixel array and deferred text commands. Call render()
// inside an ftxui Renderer to obtain a canvas Element. Wrap the result with
//   color(Color::Black, bgcolor(Color::White, buf.render()))
// to emulate the e-paper's white background / black ink appearance.
// ─────────────────────────────────────────────────────────────────────────────
struct TextCmd { int16_t x, y; std::string text; };

class EpaperBuffer final : public IEpaperDisplay {
public:
    void fillScreen(bool black) override { pixels_.fill(black); }

    void drawPixel(int16_t x, int16_t y, bool black) override {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            pixels_[y * WIDTH + x] = black;
    }

    // Adafruit-style bitmap: MSB-first, row-major, each row padded to byte boundary.
    void drawBitmap(int16_t x0, int16_t y0, const uint8_t* bmp,
                    int16_t w, int16_t h, bool black) override {
        const int16_t bpr = (w + 7) / 8;
        for (int16_t row = 0; row < h; ++row)
            for (int16_t col = 0; col < w; ++col)
                if ((bmp[row * bpr + col / 8] >> (7 - col % 8)) & 1)
                    drawPixel(x0 + col, y0 + row, black);
    }

    void setCursor(int16_t x, int16_t y) override { cx_ = x; cy_ = y; }
    void setFont(const void*) override {}

    void print(std::string_view text) override {
        text_cmds_.push_back({cx_, cy_, std::string(text)});
        cx_ += static_cast<int16_t>(text.size() * 6); // ~6px/char — default GFX font
    }

    void printf(const char* fmt, ...) override {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        print(buf);
    }

    // Produce an ftxui Element. Wrap with color+bgcolor for e-paper look.
    [[nodiscard]] ftxui::Element render() const {
        auto c = ftxui::Canvas(WIDTH, HEIGHT);

        // Pixel buffer → braille dots (DrawPoint uses foreground color for dots)
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x)
                if (pixels_[y * WIDTH + x])
                    c.DrawPoint(x, y, true);

        // Text overlays — snap to braille grid (x even, y multiple of 4)
        for (const auto& t : text_cmds_)
            c.DrawText(t.x & ~1, (t.y / 4) * 4, t.text,
                       [](ftxui::Pixel& p) {
                           p.foreground_color = ftxui::Color::Black;
                           p.background_color = ftxui::Color::White;
                       });

        return ftxui::canvas(std::move(c));
    }

    void reset() { pixels_.fill(false); text_cmds_.clear(); cx_ = cy_ = 0; }

private:
    std::array<bool, WIDTH * HEIGHT> pixels_{};
    std::vector<TextCmd>             text_cmds_;
    int16_t cx_ = 0, cy_ = 0;
};

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "epaper_display.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Sample bitmaps (MSB-first, row-major, Adafruit-style)
// ─────────────────────────────────────────────────────────────────────────────

// 8×16 thermometer icon
static constexpr uint8_t BMP_THERMOMETER[] = {
    0b00011000,  // row  0   ##
    0b00100100,  // row  1  #  #
    0b00100100,  // row  2  #  #
    0b00100100,  // row  3  #  #
    0b00100100,  // row  4  #  #
    0b00111100,  // row  5  ####
    0b01111110,  // row  6 ######
    0b01111110,  // row  7 ######
    0b01111110,  // row  8 ######
    0b01111110,  // row  9 ######
    0b01111110,  // row 10 ######
    0b00111100,  // row 11  ####
    0b00011000,  // row 12   ##
    0b00000000,  // row 13
    0b00000000,  // row 14
    0b00000000,  // row 15
};

// 8×16 humidity / droplet icon
static constexpr uint8_t BMP_DROPLET[] = {
    0b00011000,  // row  0
    0b00011000,  // row  1
    0b00111100,  // row  2
    0b01111110,  // row  3
    0b11111111,  // row  4
    0b11111111,  // row  5
    0b11111111,  // row  6
    0b11111111,  // row  7
    0b01111110,  // row  8
    0b00111100,  // row  9
    0b00011000,  // row 10
    0b00000000,  // row 11
    0b00000000,  // row 12
    0b00000000,  // row 13
    0b00000000,  // row 14
    0b00000000,  // row 15
};

// ─────────────────────────────────────────────────────────────────────────────
// Climate screen — draws to any IEpaperDisplay* (firmware or emulator)
// ─────────────────────────────────────────────────────────────────────────────
struct ClimateData {
    float temp;
    float hum;
    std::vector<float> temp_history; // 48 readings, oldest first
    std::string timestamp;
    int battery_pct;
};

static void draw_climate_screen(IEpaperDisplay& d, const ClimateData& data) {
    d.fillScreen(GxEPD_WHITE);

    // ── header bar (inverted) ────────────────────────────────────────────────
    d.fillRect(0, 0, 200, 20, GxEPD_BLACK);
    d.setCursor(6, 6);
    d.print("  Climate Logger");

    // battery indicator top-right (small rect + fill)
    const int bat_x = 170, bat_y = 4;
    d.drawRect(bat_x, bat_y, 22, 12, GxEPD_WHITE);
    d.fillRect(bat_x + 22, bat_y + 3, 3, 6, GxEPD_WHITE); // terminal nub
    const int fill_w = static_cast<int>(data.battery_pct * 20 / 100);
    d.fillRect(bat_x + 1, bat_y + 1, fill_w, 10, GxEPD_WHITE);

    d.drawHLine(0, 20, 200, GxEPD_BLACK);

    // ── temperature block (left half, rows 20–90) ────────────────────────────
    d.drawBitmap(6, 28, BMP_THERMOMETER, 8, 16, GxEPD_BLACK);
    d.setCursor(20, 28);
    d.print("TEMP");

    d.setCursor(14, 48);
    d.printf("%.1f", static_cast<double>(data.temp));
    d.setCursor(68, 44);
    d.print("o");  // degree approximation
    d.setCursor(74, 48);
    d.print("C");

    // vertical divider
    d.drawVLine(100, 20, 72, GxEPD_BLACK);

    // ── humidity block (right half, rows 20–90) ──────────────────────────────
    d.drawBitmap(106, 28, BMP_DROPLET, 8, 16, GxEPD_BLACK);
    d.setCursor(120, 28);
    d.print("HUM");

    d.setCursor(114, 48);
    d.printf("%.1f%%", static_cast<double>(data.hum));

    d.drawHLine(0, 92, 200, GxEPD_BLACK);

    // ── graph section (rows 92–188) ───────────────────────────────────────────
    d.setCursor(4, 95);
    d.print("24h Temperature");
    d.drawHLine(0, 106, 200, GxEPD_BLACK);

    // graph area
    constexpr int GX = 8, GY = 110, GW = 186, GH = 72;

    d.drawVLine(GX, GY, GH, GxEPD_BLACK);
    d.drawHLine(GX, GY + GH, GW, GxEPD_BLACK);

    // dashed horizontal grid lines
    for (int gy = GY + 18; gy < GY + GH; gy += 18)
        for (int gx = GX + 2; gx < GX + GW; gx += 6)
            d.drawPixel(static_cast<int16_t>(gx), static_cast<int16_t>(gy), GxEPD_BLACK);

    // plot history
    const auto& hist = data.temp_history;
    if (hist.size() >= 2) {
        const float lo = *std::min_element(hist.begin(), hist.end()) - 0.5f;
        const float hi = *std::max_element(hist.begin(), hist.end()) + 0.5f;
        const float range = hi - lo;
        const auto n = static_cast<int>(hist.size());

        auto px_for = [&](int i) -> std::pair<int, int> {
            int px = GX + 1 + (i * (GW - 2)) / (n - 1);
            int py = GY + GH - 1 - static_cast<int>((hist[i] - lo) / range * (GH - 2));
            return {px, py};
        };

        for (int i = 1; i < n; ++i) {
            auto [x0, y0] = px_for(i - 1);
            auto [x1, y1] = px_for(i);
            // thick line: draw 2px height
            int steps = std::abs(x1 - x0) + 1;
            for (int s = 0; s < steps; ++s) {
                int px = x0 + s;
                int py = y0 + (y1 - y0) * s / steps;
                d.drawPixel(static_cast<int16_t>(px), static_cast<int16_t>(py),     GxEPD_BLACK);
                d.drawPixel(static_cast<int16_t>(px), static_cast<int16_t>(py + 1), GxEPD_BLACK);
            }
        }

        // min / max labels on y-axis
        d.setCursor(0, static_cast<int16_t>(GY + GH - 6));
        d.printf("%.0f", static_cast<double>(lo + 0.5f));
        d.setCursor(0, static_cast<int16_t>(GY));
        d.printf("%.0f", static_cast<double>(hi - 0.5f));
    }

    d.drawHLine(0, 188, 200, GxEPD_BLACK);

    // ── footer ────────────────────────────────────────────────────────────────
    d.setCursor(4, 191);
    d.printf("Updated: %s", data.timestamp.c_str());
    d.setCursor(148, 191);
    d.printf("Bat:%d%%", data.battery_pct);
}

// ─────────────────────────────────────────────────────────────────────────────
// Generate plausible fake history data
// ─────────────────────────────────────────────────────────────────────────────
static ClimateData make_sample_data(float temp_offset = 0.0f, float hum_offset = 0.0f) {
    ClimateData d;
    d.battery_pct = 87;
    d.timestamp   = "14:32";

    for (int i = 0; i < 48; ++i) {
        float t = 22.5f + temp_offset
                + 2.5f * std::sin(static_cast<float>(i) * 0.18f)
                + 0.8f * std::sin(static_cast<float>(i) * 0.9f);
        d.temp_history.push_back(t);
    }
    d.temp = d.temp_history.back();
    d.hum  = 63.0f + hum_offset;
    return d;
}

// ─────────────────────────────────────────────────────────────────────────────
// main — FTXUI shell around the emulated e-paper
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    EpaperBuffer epaper;
    auto data = make_sample_data();

    // initial draw
    draw_climate_screen(epaper, data);

    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen();

    // Wrap canvas in e-paper colors (white bg, black ink)
    auto epaper_element = [&]() -> Element {
        return color(Color::Black,
               bgcolor(Color::White,
               epaper.render()));
    };

    auto renderer = Renderer([&] {
        return vbox({
            text("E-Paper Emulator  —  GxEPD2_154_D67  (200×200 px)") | bold | hcenter,
            separator(),
            hbox({ filler(), epaper_element(), filler() }),
            separator(),
            hbox({
                filler(),
                text("[R] refresh data   [Q] quit") | dim,
                filler(),
            }),
        });
    });

    float offset = 0.0f;
    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            offset += 1.5f;
            data = make_sample_data(offset, static_cast<float>(std::rand() % 20) - 10.0f);
            epaper.reset();
            draw_climate_screen(epaper, data);
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return 0;
}

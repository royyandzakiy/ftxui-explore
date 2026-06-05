#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "bitmaps.h" // firmware bitmaps — exact same arrays
#include "epaper_display.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string current_time_str() {
	std::time_t t = std::time(nullptr);
	std::tm tm{};
#if defined(_MSC_VER)
	localtime_s(&tm, &t);
#else
	tm = *std::localtime(&t);
#endif
	char buf[8];
	std::snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
	return buf;
}

// Exact emoji selection from firmware app.cpp
static const uint8_t *select_emoji(float temp_c) {
	if (temp_c < -10)
		return bitmaps::face_scary_2;
	else if (temp_c < 0)
		return bitmaps::face_scary_2;
	else if (temp_c < 2)
		return bitmaps::face_scary_1;
	else if (temp_c < 4)
		return bitmaps::face_death_1;
	else if (temp_c < 10)
		return bitmaps::face_cold_1;
	else if (temp_c < 12)
		return bitmaps::face_sad_1;
	else if (temp_c < 14)
		return bitmaps::face_neutral_1;
	else if (temp_c < 16)
		return bitmaps::face_neutral_2;
	else if (temp_c < 18)
		return bitmaps::face_neutral_3;
	else if (temp_c < 20)
		return bitmaps::face_happy_4;
	else if (temp_c < 24)
		return bitmaps::face_happy_2;
	else if (temp_c < 26)
		return bitmaps::face_happy_3;
	else if (temp_c < 27)
		return bitmaps::face_hot_1;
	else if (temp_c < 28)
		return bitmaps::face_hot_2;
	else if (temp_c < 31)
		return bitmaps::face_hot_3;
	else if (temp_c < 33)
		return bitmaps::face_hot_4;
	else if (temp_c < 35)
		return bitmaps::face_scary_1;
	else
		return bitmaps::face_empty_1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Graph drawing — port of firmware App::drawGraph()
//
// Coordinates are in GxEPD2 firmware space (d->setRotation(2) already called).
// left/right/bottom/top follow the firmware convention: bottom and top are
// measured in pixels FROM THE PHYSICAL BOTTOM (i.e. y=disph-v in screen coords).
// ─────────────────────────────────────────────────────────────────────────────
static void draw_graph(IEpaperDisplay &d, int left, int right, int bottom, int top, const std::vector<float> &data) {
	constexpr int disph = 199; // matches firmware: constexpr int disph = 199

	if (left >= right || bottom >= top || data.empty())
		return;

	const int width = right - left;
	const int height = top - bottom;

	// Convert graph box to firmware screen coordinates
	const int invt = disph - top;	 // firmware screen y for graph top edge
	const int invb = disph - bottom; // firmware screen y for graph bottom edge

	// Outline (exact firmware calls)
	d.drawLine(left, invt, right, invt, GxEPD_BLACK);
	d.drawLine(left, invb, right, invb, GxEPD_BLACK);
	d.drawLine(left, invt, left, invb, GxEPD_BLACK);
	d.drawLine(right, invt, right, invb, GxEPD_BLACK);

	// Grid (dashed, alternating pixels — exact firmware pattern)
	for (int i = 1; i < width - 1; i++) {
		if (i % 2 != 0) {
			d.drawPixel(left + i, invt + height / 2, GxEPD_BLACK);
			d.drawPixel(left + i, invt + height / 4, GxEPD_BLACK);
			d.drawPixel(left + i, invb - height / 4, GxEPD_BLACK);
		}
	}
	for (int i = 1; i < height - 1; i++) {
		if (i % 2 != 0) {
			d.drawPixel(left + width / 2, invt + i, GxEPD_BLACK);
			d.drawPixel(left + width / 4, invt + i, GxEPD_BLACK);
			d.drawPixel(right - width / 4, invt + i, GxEPD_BLACK);
		}
	}

	if (data.size() < 2)
		return;

	const int n = static_cast<int>(data.size());

	// Min / max (firmware uses raw integer temperature × 100; we use float)
	float fmin = *std::min_element(data.begin(), data.end());
	float fmax = *std::max_element(data.begin(), data.end());
	if (fmax - fmin < 0.5f) {
		fmax = fmin + 0.5f;
	} // prevent flat-line divide-by-zero

	// Map a temperature value to firmware screen Y
	auto mapY = [&](float v) -> int {
		const float ratio = (v - fmin) / (fmax - fmin);
		const int t_px = static_cast<int>(ratio * static_cast<float>(height - 1) + 0.5f);
		return invb - t_px; // invb = low screen y, subtract → higher screen y for higher temp
	};

	// Polyline (firmware fast path: samples ≤ pixels)
	const int64_t xStepQ16 = (n > 1) ? (((int64_t)width << 16) / (n - 1)) : 0;
	int64_t fx = 0;
	int prevX = left;
	int prevY = mapY(data[0]);

	for (int i = 1; i < n; ++i) {
		fx += xStepQ16;
		const int x = left + static_cast<int>((fx + (1 << 15)) >> 16);
		const int y = mapY(data[i]);
		d.drawLine(prevX, prevY, x, y, GxEPD_BLACK);
		// second pass offset by 1px for a slightly thicker line (firmware uses 1.5px)
		d.drawLine(prevX, prevY + 1, x, y + 1, GxEPD_BLACK);
		prevX = x;
		prevY = y;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Standard display — exact layout mirroring firmware App::StandardDisplay()
//
// Coordinates are firmware coordinates with setRotation(2) active.
// Bitmap calls are verbatim from the firmware source.
// ─────────────────────────────────────────────────────────────────────────────
static void draw_standard_display(IEpaperDisplay &d, float temp, float hum, const std::vector<float> &history,
								  float prev_temp) {
	d.setRotation(2); // exact firmware setting
	d.fillScreen(GxEPD_WHITE);

	// ── thermometer icon (exact firmware: drawBitmap(2, 0, thermometer32, 12, 32, black)) ──
	d.drawBitmap(2, 0, bitmaps::thermometer32, 12, 32, GxEPD_BLACK);

	// ── temperature value ────────────────────────────────────────────────────
	// Firmware uses FreeMonoBold24pt7b and getTextBounds to position parts.
	// We approximate with fixed offsets; the integers are placed at the same
	// cursor origin (18, 29) as the firmware.
	{
		const int iv = static_cast<int>(temp);
		const int dv = std::abs(static_cast<int>((temp - static_cast<float>(iv)) * 10));
		d.setCursor(18, 29);
		d.printf("%d", iv);
		d.setCursor(38, 29); // decimal point
		d.print(".");
		d.setCursor(44, 29); // decimal digit
		d.printf("%d", dv);
		// degree symbol — exact firmware: drawBitmap(x1+w+40, 2, degree_symbol, 11, 11, black)
		d.drawBitmap(60, 2, bitmaps::degree_symbol, 11, 11, GxEPD_BLACK);
	}

	// ── trend arrow (exact firmware: drawBitmap(128, 0, up/down_arrow, 15, 24, black)) ──
	if (history.size() >= 2) {
		if (temp > prev_temp)
			d.drawBitmap(128, 0, bitmaps::up_arrow, 15, 24, GxEPD_BLACK);
		else if (temp < prev_temp)
			d.drawBitmap(128, 0, bitmaps::down_arrow, 15, 24, GxEPD_BLACK);
	}

	// ── emoji face (exact firmware: drawBitmap(151, 0, emoji, 48, 48, black)) ──
	d.drawBitmap(151, 0, select_emoji(temp), 48, 48, GxEPD_BLACK);

	// ── humidity ─────────────────────────────────────────────────────────────
	// Exact firmware: drawBitmap(0, 37, water_drop, 17, 17, black)
	d.drawBitmap(0, 37, bitmaps::water_drop, 17, 17, GxEPD_BLACK);
	d.setCursor(20, 52);
	d.printf("%d", static_cast<int>(std::roundf(hum)));
	// Exact firmware: drawBitmap(x1+w+3, 38, percent_symbol_16, 16, 16, black)
	// 2-digit hum text ≈ 12px wide → percent symbol at (20+12+3=35, 38)
	d.drawBitmap(35, 38, bitmaps::percent_symbol_16, 16, 16, GxEPD_BLACK);

	// ── clock + time ─────────────────────────────────────────────────────────
	// Exact firmware: drawBitmap(0, 58, clock, 17, 17, black)
	d.drawBitmap(0, 58, bitmaps::clock, 17, 17, GxEPD_BLACK);
	d.setCursor(20, 73);
	d.print(current_time_str());

	// ── graph (exact firmware: drawGraph(0, 199, 13, 118, data, count, ...)) ──
	const int n = static_cast<int>(history.size());
	if (n >= 2) {
		draw_graph(d, 0, 199, 13, 118, history);

		const float fmin = *std::min_element(history.begin(), history.end());
		const float fmax = *std::max_element(history.begin(), history.end());

		// Graph axis labels — exact firmware cursor positions
		d.setCursor(0, 199);
		d.printf("T-%dm", n);

		d.setCursor(140, 199);
		d.printf("%.1f", static_cast<double>(fmin));

		d.setCursor(140, 79);
		d.printf("%.1f", static_cast<double>(fmax));
	} else {
		d.setCursor(0, 199);
		d.print("No data");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared simulation state (written by updater thread, read by renderer)
// ─────────────────────────────────────────────────────────────────────────────
struct SimState {
	std::deque<float> history; // temperature readings, oldest first
	float temp = 0.0f;
	float hum = 60.0f;
	float prev_temp = 0.0f;
	bool dirty = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
	EpaperBuffer epaper;
	draw_standard_display(epaper, 0, 0, {}, 0); // initial: "No data" state

	std::mutex state_mtx;
	SimState state;
	std::atomic<bool> quit{false};

	using namespace ftxui;

	auto screen = ScreenInteractive::Fullscreen();

	// ── background temperature simulator ─────────────────────────────────────
	std::thread updater([&] {
		std::mt19937 rng(42);
		std::normal_distribution<float> drift(0.0f, 0.12f);
		std::normal_distribution<float> hum_drift(0.0f, 0.3f);
		float t = 21.5f, h = 62.0f;

		while (!quit) {
			std::this_thread::sleep_for(std::chrono::milliseconds(400));

			const float prev = t;
			t += drift(rng);
			t = std::clamp(t, 5.0f, 38.0f);
			h += hum_drift(rng);
			h = std::clamp(h, 20.0f, 95.0f);

			{
				std::lock_guard lock(state_mtx);
				state.prev_temp = prev;
				state.temp = t;
				state.hum = h;
				state.history.push_back(t);
				if (state.history.size() > 190) // keep ≤ one reading per graph pixel
					state.history.pop_front();
				state.dirty = true;
			}
			screen.PostEvent(Event::Custom);
		}
	});

	// ── FTXUI component ───────────────────────────────────────────────────────
	auto component = Renderer([&] {
		// Re-draw the epaper buffer only when new data arrived
		{
			std::lock_guard lock(state_mtx);
			if (state.dirty) {
				const std::vector<float> hist(state.history.begin(), state.history.end());
				epaper.reset();
				draw_standard_display(epaper, state.temp, state.hum, hist, state.prev_temp);
				state.dirty = false;
			}
		}

		return vbox({
			text(" E-Paper Emulator — GxEPD2_154_D67  (200×200 px)") | bold | hcenter,
			separator(),
			hbox({filler(), color(Color::Black, bgcolor(Color::White, epaper.render())), filler()}),
			separator(),
			text(" [Q] quit") | dim | hcenter,
		});
	});

	component = CatchEvent(component, [&](Event event) {
		if (event == Event::Character('q') || event == Event::Character('Q')) {
			screen.ExitLoopClosure()();
			return true;
		}
		return false;
	});

	screen.Loop(component);
	quit = true;
	updater.join();
	return 0;
}

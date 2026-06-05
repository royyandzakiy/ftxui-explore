#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>

int main() {
	using namespace ftxui;

	int counter = 0;

	// A simple interactive component: a button that increments a counter
	auto button = Button("Click me", [&] { counter++; });

	auto container = Container::Vertical({
		button,
		Renderer([&] { return text("Clicks: " + std::to_string(counter)) | center; }),
	});

	auto screen = ScreenInteractive::Fullscreen();
	screen.Loop(container);

	return 0;
}
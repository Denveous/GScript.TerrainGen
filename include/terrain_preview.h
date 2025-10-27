#pragma once
#include <cstdint>
namespace terrain {
  struct PreviewColor { std::uint8_t red; std::uint8_t green; std::uint8_t blue; };
  PreviewColor preview_color(double top_left, double top_right, double bottom_left, double bottom_right, bool slope_lighting);
}

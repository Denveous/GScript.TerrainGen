#include "terrain_preview.h"
#include <algorithm>
#include <cmath>
namespace terrain {
  namespace {
    int legacy_floor(double value) {
      // A tiny positive epsilon avoids a
      // floating-point representation just below an intended whole-number height.
      return static_cast<int>(std::floor(value + 0.0001));
    }
    std::uint8_t byte(int value) { return static_cast<std::uint8_t>(value); }
    std::uint8_t channel(int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); }
  }
  PreviewColor preview_color(double top_left, double top_right, double bottom_left, double bottom_right, bool slope_lighting) {
    // Shade one preview quad from four clamped 9x9 samples.
    const double average = (top_left + top_right + bottom_left + bottom_right) / 4.0;
    const int height = legacy_floor(average);
    PreviewColor color;
    if (average < -8.0) color = {0, 80, 111};
    else if (average < -2.0) color = {0, 127, 158};
    else if (average < 3.0) color = {102, 102, 0};
    else if (average < 25.0) color = {0, 224, 0};
    else if (average < 55.0) color = {160, 0, 0};
    else color = {224, 240, 240};
    if (!slope_lighting) return color;
    // The legacy preview uses the north/south difference, not a normal vector.
    // Deep water receives only the bright half of the slope shade, preserving its base blue-green.
    const int light = std::clamp(static_cast<int>(std::llround(((bottom_left + bottom_right) - (top_left + top_right)) * 16.0)), average < -8.0 ? 0 : -96, 95);
    return {channel(color.red + light), channel(color.green + light), channel(color.blue + light)};
  }
}

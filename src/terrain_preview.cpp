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
    int ramp(double value, double low, double high, int range, int offset) { return legacy_floor((value-low)*range/(high-low))+offset; }
  }
  PreviewColor preview_color(double top_left, double top_right, double bottom_left, double bottom_right, bool slope_lighting) {
    // Shade one preview quad from four clamped 9x9 samples.
    const double average = (top_left + top_right + bottom_left + bottom_right) / 4.0;
    PreviewColor color;
    if (average < -8.0) { const int depth=average < -32.0?144:legacy_floor(96.0-(average+8.0)*2.0); color={0,byte(224-depth),byte(255-depth)}; }
    else if (average < -2.0) { const int depth=legacy_floor(-average*12.0); color={0,byte(224-depth),byte(255-depth)}; }
    else if (average < 3.0) { const auto value=byte(ramp(average,-2.0,3.0,63,96)); color={value,value,0}; }
    else if (average < 25.0) color={0,byte(ramp(average,3.0,25.0,63,192)),0};
    else if (average < 55.0) color={0,byte(ramp(average,25.0,55.0,63,128)),0};
    else if (average < 65.0) color={byte(ramp(average,55.0,65.0,95,96)),0,0};
    else { const int value=(std::min)(31,legacy_floor(average-65.0)); color={byte(value+192),byte(value+224),byte(value+224)}; }
    if (!slope_lighting) return color;
    // The preview shades every terrain band from the north/south height difference.
    const int light = std::clamp(static_cast<int>(std::nearbyint(((bottom_left + bottom_right) - (top_left + top_right)) * 16.0)), -96, 95);
    return {channel(color.red + light), channel(color.green + light), channel(color.blue + light)};
  }
}

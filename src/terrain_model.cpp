#include "terrain_model.h"
#include <algorithm>
#include <stdexcept>
namespace terrain {
  double& Level::at(std::size_t x, std::size_t y) { return samples.at(y * 9 + x); }
  const double& Level::at(std::size_t x, std::size_t y) const { return samples.at(y * 9 + x); }
  Terrain::Terrain(std::size_t width, std::size_t height) : width_(width), height_(height), levels_(width * height) {}
  std::size_t Terrain::width() const { return width_; }
  std::size_t Terrain::height() const { return height_; }
  Level& Terrain::level(std::size_t x, std::size_t y) { if (x >= width_ || y >= height_) throw std::out_of_range("terrain level"); return levels_[y * width_ + x]; }
  const Level& Terrain::level(std::size_t x, std::size_t y) const { if (x >= width_ || y >= height_) throw std::out_of_range("terrain level"); return levels_[y * width_ + x]; }
  double Terrain::sample_clamped(std::ptrdiff_t level_x, std::ptrdiff_t level_y, std::size_t sample_x, std::size_t sample_y) const {
    if (!width_ || !height_) throw std::out_of_range("empty terrain");
    const auto x = static_cast<std::size_t>(std::clamp(level_x, std::ptrdiff_t{}, static_cast<std::ptrdiff_t>(width_ - 1)));
    const auto y = static_cast<std::size_t>(std::clamp(level_y, std::ptrdiff_t{}, static_cast<std::ptrdiff_t>(height_ - 1)));
    return level(x, y).at(sample_x, sample_y);
  }
  void Terrain::edit_level_sample(std::size_t level_x, std::size_t level_y, std::ptrdiff_t sample_x, std::ptrdiff_t sample_y, double value) {
    auto& target = level(level_x, level_y);
    const auto x = static_cast<std::size_t>(std::clamp(sample_x, std::ptrdiff_t{}, std::ptrdiff_t{8}));
    const auto y = static_cast<std::size_t>(std::clamp(sample_y, std::ptrdiff_t{}, std::ptrdiff_t{8}));
    target.at(x, y) = value;
    target.dirty = true;
  }
  void Terrain::apply_control_delta(std::size_t control_x, std::size_t control_y, double delta) {
    if (control_x > width_ || control_y > height_) throw std::out_of_range("terrain control");
    const auto first_x = control_x == 0 ? 0 : control_x - 1;
    const auto first_y = control_y == 0 ? 0 : control_y - 1;
    const auto last_x = (std::min)(control_x, width_ - 1);
    const auto last_y = (std::min)(control_y, height_ - 1);
    for (std::size_t level_y = first_y; level_y <= last_y; ++level_y) for (std::size_t level_x = first_x; level_x <= last_x; ++level_x) {
      auto& target = level(level_x, level_y);
      const int corner_x = control_x == level_x ? 0 : 8;
      const int corner_y = control_y == level_y ? 0 : 8;
      // A control point is one corner of every adjacent level. Interpolating its
      // delta over the entire patch preserves the shared edge on every neighbor.
      for (int y = 0; y < 9; ++y) for (int x = 0; x < 9; ++x) {
        const double x_weight = static_cast<double>(corner_x == 0 ? 8 - x : x) / 8.0;
        const double y_weight = static_cast<double>(corner_y == 0 ? 8 - y : y) / 8.0;
        target.at(x, y) += delta * x_weight * y_weight;
      }
    }
  }
}

#pragma once
#include <array>
#include <cstddef>
#include <vector>
namespace terrain {
  struct Level {
    bool dirty{};
    std::array<double, 81> samples{};
    double& at(std::size_t x, std::size_t y);
    const double& at(std::size_t x, std::size_t y) const;
  };
  class Terrain {
  public:
    Terrain(std::size_t width, std::size_t height);
    std::size_t width() const;
    std::size_t height() const;
    Level& level(std::size_t x, std::size_t y);
    const Level& level(std::size_t x, std::size_t y) const;
    double sample_clamped(std::ptrdiff_t level_x, std::ptrdiff_t level_y, std::size_t sample_x, std::size_t sample_y) const;
    void edit_level_sample(std::size_t level_x, std::size_t level_y, std::ptrdiff_t sample_x, std::ptrdiff_t sample_y, double value);
    void apply_control_delta(std::size_t control_x, std::size_t control_y, double delta);
  private:
    std::size_t width_;
    std::size_t height_;
    std::vector<Level> levels_;
  };
}

#include "terrain_generator.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace terrain {
  std::uint32_t TerrainRandom::drandseed() const { return drandseed_; }
  double TerrainRandom::drand() {
    // Delphi's Random uses the shared 32-bit state and scales its unsigned result by 2^-32.
    drandseed_ = drandseed_ * 0x08088405u + 1u;
    return std::ldexp(static_cast<double>(drandseed_), -32);
  }
  std::int32_t TerrainRandom::drandi() {
    // drandi advances the identical stream but exposes the signed 32-bit state directly.
    drandseed_ = drandseed_ * 0x08088405u + 1u;
    return static_cast<std::int32_t>(drandseed_);
  }
  void generate_midpoint(std::span<double> heights, int stride, TerrainRandom& random, double chaos_scale, double chaos, int bottom, int right, int top, int left) {
    if (stride <= 0 || left < 0 || top < 0 || right < 0 || bottom < 0 || left >= stride || right >= stride || static_cast<std::size_t>(std::max(top, bottom) * stride + std::max(left, right)) >= heights.size()) throw std::out_of_range("height bounds");
    if (right - left <= 1 && bottom - top <= 1) return;
    // This is the legacy recursive displacement pass. It fills the midpoint of a
    // diagonal, then recurses into the two resulting diagonals with damped chaos.
    const int middle_x = (right + left) / 2;
    const int middle_y = (bottom + top) / 2;
    // Average opposite corners, add signed chaos, then damp it recursively.
    heights[middle_y * stride + middle_x] = (heights[top * stride + left] + heights[bottom * stride + right]) / 2.0 + (random.drand() - 0.5) * 2.0 * chaos;
    generate_midpoint(heights, stride, random, chaos_scale, chaos * chaos_scale, middle_y, middle_x, top, left);
    generate_midpoint(heights, stride, random, chaos_scale, chaos * chaos_scale, bottom, right, middle_y, middle_x);
  }
  namespace {
    void generate_quadrant(std::span<double> heights, int stride, TerrainRandom& random, double chaos_scale, double chaos, int bottom, int right, int top, int left) {
      if (right - left <= 1 && bottom - top <= 1) return;
      const int middle_x = (left + right) / 2;
      const int middle_y = (top + bottom) / 2;
      generate_midpoint(heights, stride, random, chaos_scale, chaos, middle_y, right, middle_y, left);
      generate_midpoint(heights, stride, random, chaos_scale, chaos * chaos_scale, middle_y, middle_x, top, middle_x);
      generate_midpoint(heights, stride, random, chaos_scale, chaos * chaos_scale, bottom, middle_x, middle_y, middle_x);
      generate_quadrant(heights, stride, random, chaos_scale, chaos * chaos_scale, middle_y, middle_x, top, left);
      generate_quadrant(heights, stride, random, chaos_scale, chaos * chaos_scale, middle_y, right, top, middle_x);
      generate_quadrant(heights, stride, random, chaos_scale, chaos * chaos_scale, bottom, middle_x, middle_y, left);
      generate_quadrant(heights, stride, random, chaos_scale, chaos * chaos_scale, bottom, right, middle_y, middle_x);
    }
    void apply_override_row(std::span<double> heights, double delta_height, int origin_x, int y) {
      const double delta_step = delta_height / 8.0;
      if (origin_x >= 8) for (int x = 1; x < 8; ++x) heights[y * 65 + origin_x - 8 + x] += delta_step * x;
      heights[y * 65 + origin_x] += delta_height;
      if (origin_x < 64) for (int x = 1; x < 8; ++x) heights[y * 65 + origin_x + x] += delta_step * (8 - x);
    }
    void apply_overrides(std::span<double> heights, std::span<const double, 81> overrides) {
      for (int y = 0; y < 9; ++y) for (int x = 0; x < 9; ++x) {
        const int origin_x = x * 8;
        const int origin_y = y * 8;
        const double delta_step = (overrides[y * 9 + x] - heights[origin_y * 65 + origin_x]) / 8.0;
        const int top = origin_y >= 8 ? origin_y - 7 : origin_y;
        const int bottom = origin_y <= 56 ? origin_y + 7 : origin_y;
        for (int row = top; row <= bottom; ++row) apply_override_row(heights, delta_step * (8 - std::abs(row - origin_y)), origin_x, row);
      }
    }
  }
  void generate_map_terrain(int map_width, int map_height, std::uint32_t map_seed, double base_height, bool even_borders, double map_height_deviation, double map_chaos, std::vector<double>& controls, std::vector<std::uint32_t>& level_seeds) {
    if (map_width <= 0 || map_height <= 0) throw std::out_of_range("terrain map dimensions");
    controls.assign(static_cast<std::size_t>(map_width + 1) * (map_height + 1), base_height);
    TerrainRandom random(map_seed);
    const int stride = map_width + 1;
    if (!even_borders) {
      // The four perimeter calls precede the map quadrant and consume one shared
      // Delphi stream. Their order is top, bottom, left, then right in the EXE.
      generate_midpoint(controls, stride, random, map_chaos, map_height_deviation, 0, map_width, 0, 0);
      generate_midpoint(controls, stride, random, map_chaos, map_height_deviation, map_height, map_width, map_height, 0);
      generate_midpoint(controls, stride, random, map_chaos, map_height_deviation, map_height, 0, 0, 0);
      generate_midpoint(controls, stride, random, map_chaos, map_height_deviation, map_height, map_width, 0, map_width);
    }
    generate_quadrant(controls, stride, random, map_chaos, map_height_deviation, map_height, map_width, 0, 0);
    for (auto& control : controls) control = std::floor(control + 0.0001);
    level_seeds.resize(static_cast<std::size_t>(map_width) * map_height);
    // The legacy nested loop is x-major although the saved array is y * width + x.
    for (int x = 0; x < map_width; ++x) for (int y = 0; y < map_height; ++y) level_seeds[y * map_width + x] = static_cast<std::uint32_t>(random.drand() * 2147483647.0);
  }
  TerrainBorders generate_map_borders(std::span<const double> map_controls, int map_width, int map_height, std::uint32_t map_seed, double map_height_deviation, double map_chaos) {
    if (map_width <= 0 || map_height <= 0 || map_controls.size() != static_cast<std::size_t>(map_width + 1) * (map_height + 1)) throw std::out_of_range("terrain map bounds");
    TerrainBorders borders{.horizontal = std::vector<double>(static_cast<std::size_t>(map_width * 64 + 1) * (map_height + 1)), .vertical = std::vector<double>(static_cast<std::size_t>(map_height * 64 + 1) * (map_width + 1))};
    const auto control = [&](int x, int y) { return map_controls[y * (map_width + 1) + x]; };
    // The map PRNG is shared across every border segment in legacy column-major order.
    // Re-seeding it per level would generate seams at every level boundary.
    TerrainRandom map_random(map_seed);
    for (int x = 0; x <= map_width; ++x) for (int y = 0; y <= map_height; ++y) {
      borders.horizontal[y * (map_width * 64 + 1) + x * 64] = control(x, y);
      borders.vertical[y * 64 * (map_width + 1) + x] = control(x, y);
    }
    for (int x = 0; x <= map_width; ++x) for (int y = 0; y <= map_height; ++y) {
      if (x < map_width) generate_midpoint(borders.horizontal, map_width * 64 + 1, map_random, map_chaos, map_height_deviation, y, (x + 1) * 64, y, x * 64);
      if (y < map_height) generate_midpoint(borders.vertical, map_width + 1, map_random, map_chaos, map_height_deviation, (y + 1) * 64, x, y * 64, x);
    }
    return borders;
  }
  void generate_level_terrain(const TerrainBorders& borders, int map_width, int map_height, int level_x, int level_y, std::uint32_t level_seed, double level_height, double level_chaos, std::span<const double, 81> overrides, bool apply_height_overrides, std::vector<double>& output) {
    if (map_width <= 0 || map_height <= 0 || level_x < 0 || level_y < 0 || level_x >= map_width || level_y >= map_height) throw std::out_of_range("terrain level bounds");
    output.assign(65 * 65, 0.0);
    // A level copies its four generated borders, then fills its interior from its own seed.
    for (int tile = 0; tile <= 64; ++tile) {
      output[tile] = borders.horizontal[level_y * (map_width * 64 + 1) + level_x * 64 + tile];
      output[64 * 65 + tile] = borders.horizontal[(level_y + 1) * (map_width * 64 + 1) + level_x * 64 + tile];
      output[tile * 65] = borders.vertical[(level_y * 64 + tile) * (map_width + 1) + level_x];
      output[tile * 65 + 64] = borders.vertical[(level_y * 64 + tile) * (map_width + 1) + level_x + 1];
    }
    TerrainRandom random(level_seed);
    generate_quadrant(output, 65, random, level_chaos, level_height * level_chaos, 64, 64, 0, 0);
    if (apply_height_overrides) apply_overrides(output, overrides);
  }
  void generate_level_terrain(std::span<const double> map_controls, int map_width, int map_height, int level_x, int level_y, std::uint32_t map_seed, std::uint32_t level_seed, double map_height_deviation, double map_chaos, double level_height, double level_chaos, std::span<const double, 81> overrides, bool apply_height_overrides, std::vector<double>& output) {
    generate_level_terrain(generate_map_borders(map_controls, map_width, map_height, map_seed, map_height_deviation, map_chaos), map_width, map_height, level_x, level_y, level_seed, level_height, level_chaos, overrides, apply_height_overrides, output);
  }
}

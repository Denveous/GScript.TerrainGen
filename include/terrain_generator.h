#pragma once
#include <cstdint>
#include <span>
#include <array>
#include <vector>
namespace terrain {
  class TerrainRandom {
  public:
    // Legacy Delphi stream: drandseed = drandseed * 0x08088405 + 1 (mod 2^32).
    explicit TerrainRandom(std::uint32_t drandseed) : drandseed_(drandseed) {}
    std::uint32_t drandseed() const;
    // Returns uint32(drandseed) * 2^-32 after advancing the state.
    double drand();
    // Returns int32(drandseed) after advancing the same state.
    std::int32_t drandi();
  private:
    std::uint32_t drandseed_;
  };
  struct TerrainBorders {
    std::vector<double> horizontal;
    std::vector<double> vertical;
  };
  void generate_midpoint(std::span<double> heights, int stride, TerrainRandom& random, double chaos_scale, double chaos, int bottom, int right, int top, int left);
  void generate_map_terrain(int map_width, int map_height, std::uint32_t map_seed, double base_height, bool even_borders, double map_height_deviation, double map_chaos, std::vector<double>& controls, std::vector<std::uint32_t>& level_seeds);
  TerrainBorders generate_map_borders(std::span<const double> map_controls, int map_width, int map_height, std::uint32_t map_seed, double level_height, double level_chaos);
  void generate_level_terrain(const TerrainBorders& borders, int map_width, int map_height, int level_x, int level_y, std::uint32_t level_seed, double level_height, double level_chaos, std::span<const double, 81> overrides, bool apply_height_overrides, std::vector<double>& output);
  void generate_level_terrain(std::span<const double> map_controls, int map_width, int map_height, int level_x, int level_y, std::uint32_t map_seed, std::uint32_t level_seed, double map_height_deviation, double map_chaos, double level_height, double level_chaos, std::span<const double, 81> overrides, bool apply_height_overrides, std::vector<double>& output);
}

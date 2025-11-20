#pragma once
#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <vector>
namespace terrain {
  struct TerrainLevelOverride {
    std::string level_name;
    std::array<double, 81> samples{};
  };
  struct TerrainMapDocument {
    int width{};
    int height{};
    std::string generated;
    std::string map_image;
    // GENSEED drives map-border generation; RANDOMSEEDS is row-major level detail state.
    std::uint32_t map_seed{};
    double base_height{};
    bool even_borders{};
    double map_height{};
    double map_chaos{};
    double level_height{};
    double level_chaos{};
    std::vector<double> heightmap;
    std::vector<std::uint32_t> random_seeds;
    std::vector<TerrainLevelOverride> height_overrides;
  };
  TerrainMapDocument parse_terrain_gmap(const std::string& text);
  void regenerate_terrain_map(TerrainMapDocument& document);
  void edit_control_height(TerrainMapDocument& document, int control_x, int control_y, double value);
  std::string generated_level_name(const TerrainMapDocument& document, int level_x, int level_y);
  std::string serialize_terrain_gmap(const TerrainMapDocument& document, bool include_height_overrides = true);
  std::string serialize_terrain_level(std::span<const double, 81> heights, bool include_heights);
}

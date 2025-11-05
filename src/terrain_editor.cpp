#include "terrain_format.h"
#include "terrain_generator.h"
#include "terrain_preview.h"
#include <array>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
  std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open " + path);
    return {std::istreambuf_iterator<char>{input}, {}};
  }
  void write_file(const std::string& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("could not write " + path);
    output << text;
    if (!output) throw std::runtime_error("could not finish " + path);
  }
  void write_preview(const terrain::TerrainMapDocument& document, const std::string& path) {
    const int width = document.width * 8;
    const int height = document.height * 8;
    std::vector<terrain::PreviewColor> pixels(static_cast<std::size_t>(width) * height);
    std::array<double, 81> no_override{};
    std::vector<double> level;
    const auto borders = terrain::generate_map_borders(document.heightmap, document.width, document.height, document.map_seed, document.map_height, document.map_chaos);
    for (int y = 0; y < document.height; ++y) for (int x = 0; x < document.width; ++x) {
      const auto level_name = terrain::generated_level_name(document, x, y);
      const auto override = std::find_if(document.height_overrides.begin(), document.height_overrides.end(), [&level_name](const terrain::TerrainLevelOverride& item) {
        return item.level_name.size() == level_name.size() && std::equal(item.level_name.begin(), item.level_name.end(), level_name.begin(), level_name.end(), [](char left, char right) { return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right)); });
      });
      const auto& samples = override == document.height_overrides.end() ? no_override : override->samples;
      terrain::generate_level_terrain(borders, document.width, document.height, x, y, document.random_seeds[y * document.width + x], document.level_height, document.level_chaos, samples, override != document.height_overrides.end(), level);
      for (int cell_y = 0; cell_y < 8; ++cell_y) for (int cell_x = 0; cell_x < 8; ++cell_x) {
        const int sample_x = cell_x * 8;
        const int sample_y = cell_y * 8;
        pixels[(y * 8 + cell_y) * width + x * 8 + cell_x] = terrain::preview_color(level[sample_y * 65 + sample_x], level[sample_y * 65 + sample_x + 8], level[(sample_y + 8) * 65 + sample_x], level[(sample_y + 8) * 65 + sample_x + 8], true);
      }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("could not write " + path);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (const auto pixel : pixels) output.write(reinterpret_cast<const char*>(&pixel), 3);
    if (!output) throw std::runtime_error("could not finish " + path);
  }
  void usage() {
    std::cerr << "usage: terrain_editor <input.gmap> <output.gmap> [--generate] [--set-control x y value] [--preview output.ppm]\n";
  }
}
int terrain_cli_main(int argument_count, char** arguments) {
  if (argument_count < 3) { usage(); return 2; }
  try {
    auto document = terrain::parse_terrain_gmap(read_file(arguments[1]));
    for (int index = 3; index < argument_count; ++index) {
      const std::string option = arguments[index];
      if (option == "--generate") terrain::regenerate_terrain_map(document);
      else if (option == "--set-control" && index + 3 < argument_count) {
        const int control_x = std::stoi(arguments[++index]);
        const int control_y = std::stoi(arguments[++index]);
        const double value = std::stod(arguments[++index]);
        terrain::edit_control_height(document, control_x, control_y, value);
      }
      else if (option == "--preview" && index + 1 < argument_count) write_preview(document, arguments[++index]);
      else throw std::runtime_error("unknown option " + option);
    }
    write_file(arguments[2], terrain::serialize_terrain_gmap(document));
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "terrain_editor: " << error.what() << '\n';
    return 1;
  }
}

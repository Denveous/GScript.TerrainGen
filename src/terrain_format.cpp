#include "terrain_format.h"
#include "terrain_generator.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace terrain {
  namespace {
    std::string trim(std::string value) {
      const auto first = value.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) return {};
      return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    }
    template<typename T> T number(const std::string& value) {
      T result{};
      const auto begin = value.data();
      const auto end = begin + value.size();
      const auto [ptr, error] = std::from_chars(begin, end, result);
      if (error != std::errc{} || ptr != end) throw std::runtime_error("invalid terrain number");
      return result;
    }
    double decimal(const std::string& value) {
      std::size_t parsed{};
      const auto result = std::stod(value, &parsed);
      if (parsed != value.size()) throw std::runtime_error("invalid terrain decimal");
      return result;
    }
    template<typename T, typename Parse> void values(std::vector<T>& output, const std::string& line, Parse parse) {
      std::size_t start{};
      while (start <= line.size()) {
        const auto end = line.find(',', start);
        const auto value = trim(line.substr(start, end == std::string::npos ? end : end - start));
        if (!value.empty()) output.push_back(parse(value));
        if (end == std::string::npos) return;
        start = end + 1;
      }
    }
    std::vector<std::string> split_values(const std::string& line) {
      std::vector<std::string> output;
      std::size_t start{};
      while (start <= line.size()) {
        const auto end = line.find(',', start);
        output.push_back(trim(line.substr(start, end == std::string::npos ? end : end - start)));
        if (end == std::string::npos) return output;
        start = end + 1;
      }
      return output;
    }
    std::string delphi_decimal(double value) {
      std::ostringstream output;
      output << std::setprecision(15) << value;
      return output.str();
    }
    template<typename T, typename Format> void write_grid(std::ostringstream& output, const std::vector<T>& grid, int rows, int columns, Format format) {
      if (grid.size() != static_cast<std::size_t>(rows) * columns) throw std::runtime_error("terrain grid dimensions do not match map dimensions");
      for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
          if (column) output << ',';
          output << format(grid[static_cast<std::size_t>(row) * columns + column]);
        }
        output << '\n';
      }
    }
  }
  TerrainMapDocument parse_terrain_gmap(const std::string& text) {
    std::istringstream input(text);
    std::string line;
    if (!std::getline(input, line) || trim(line) != "GRMAP001") throw std::runtime_error("not a GRMAP001 file");
    TerrainMapDocument document;
    // TerrainGenerator stores map controls, then a (width + 1) by (height + 1)
    // control-height grid, then one row-major random seed per level.
    while (std::getline(input, line)) {
      line = trim(line);
      if (line.empty()) continue;
      const auto split = line.find_first_of(" \t");
      const auto key = line.substr(0, split);
      const auto value = split == std::string::npos ? std::string{} : trim(line.substr(split + 1));
      if (key == "WIDTH") document.width = number<int>(value);
      else if (key == "HEIGHT") document.height = number<int>(value);
      else if (key == "GENERATED") document.generated = value;
      else if (key == "MAPIMG") document.map_image = value;
      // GENSEED controls map-border generation; RANDOMSEEDS controls one local generation stream per level.
      else if (key == "GENSEED") document.map_seed = number<std::uint32_t>(value);
      else if (key == "GENBASE") document.base_height = decimal(value);
      else if (key == "GENEVENBORDERS") document.even_borders = value == "true";
      else if (key == "GENHEIGHT") document.map_height = decimal(value);
      else if (key == "GENCHAOS") document.map_chaos = decimal(value);
      else if (key == "LEVHEIGHT") document.level_height = decimal(value);
      else if (key == "LEVCHAOS") document.level_chaos = decimal(value);
      // HEIGHTMAP holds shared level-border control points; RANDOMSEEDS regenerates local detail.
      else if (key == "HEIGHTMAP") {
        while (std::getline(input, line) && trim(line) != "HEIGHTMAPEND") values(document.heightmap, trim(line), decimal);
      } else if (key == "RANDOMSEEDS") {
        while (std::getline(input, line) && trim(line) != "RANDOMSEEDSEND") values(document.random_seeds, trim(line), number<std::uint32_t>);
      } else if (key == "HEIGHTS") {
        TerrainLevelOverride override;
        override.level_name = value;
        int row{};
        while (std::getline(input, line) && trim(line) != "HEIGHTSEND") {
          const auto row_values = split_values(trim(line));
          if (row >= 9 || row_values.size() > 9) throw std::runtime_error("invalid terrain height override dimensions");
          for (std::size_t column = 0; column < row_values.size(); ++column) {
            if (!row_values[column].empty()) override.samples[static_cast<std::size_t>(row) * 9 + column] = decimal(row_values[column]);
          }
          ++row;
        }
        if (row != 9) throw std::runtime_error("invalid terrain height override row count");
        document.height_overrides.push_back(std::move(override));
      }
    }
    return document;
  }
  void regenerate_terrain_map(TerrainMapDocument& document) {
    // This is the same state transition as Generate World: controls and local
    // seeds are replaced together so every generated level remains deterministic.
    generate_map_terrain(document.width, document.height, document.map_seed, document.base_height, document.even_borders, document.map_height, document.map_chaos, document.heightmap, document.random_seeds);
    document.height_overrides.clear();
  }
  void edit_control_height(TerrainMapDocument& document, int control_x, int control_y, double value) {
    if (control_x < 0 || control_y < 0 || control_x > document.width || control_y > document.height || document.heightmap.size() != static_cast<std::size_t>(document.width + 1) * (document.height + 1)) throw std::out_of_range("terrain control");
    // Controls are the persistent shared terrain state. Generated level samples
    // are derived from them unless that level has an explicit HEIGHTS override.
    document.heightmap[control_y * (document.width + 1) + control_x] = value;
  }
  std::string generated_level_name(const TerrainMapDocument& document, int level_x, int level_y) {
    if (level_x < 0 || level_y < 0 || level_x >= document.width || level_y >= document.height || document.generated.size() < 4 || document.generated.substr(document.generated.size() - 3) != ".nw") return {};
    int column_digits = 1;
    for (int capacity = 26; capacity < document.width; capacity *= 26) ++column_digits;
    const int row_digits = static_cast<int>(std::floor(std::log10(static_cast<double>(document.height)))) + 1;
    const auto base = document.generated.substr(0, document.generated.size() - 3);
    if (static_cast<int>(base.size()) < column_digits + row_digits) return {};
    const std::size_t row_start = base.size() - row_digits;
    std::size_t letters_end = row_start;
    while (letters_end > 0 && !std::isalpha(static_cast<unsigned char>(base[letters_end - 1]))) --letters_end;
    std::size_t letters_start = letters_end;
    while (letters_start > 0 && std::isalpha(static_cast<unsigned char>(base[letters_start - 1]))) --letters_start;
    if (static_cast<int>(letters_end - letters_start) < column_digits) return {};
    const std::string prefix_and_separator = base.substr(0, letters_end - column_digits);
    const std::string row_separator = base.substr(letters_end, row_start - letters_end);
    std::string column(column_digits, 'a');
    int value = level_x;
    for (auto it = column.rbegin(); value > 0 && it != column.rend(); ++it) { *it = static_cast<char>('a' + value % 26); value /= 26; }
    std::ostringstream row;
    row << std::setw(row_digits) << std::setfill('0') << level_y + 1;
    return prefix_and_separator + column + row_separator + row.str() + ".nw";
  }
  std::string serialize_terrain_gmap(const TerrainMapDocument& document, bool include_height_overrides) {
    if (document.width < 0 || document.height < 0) throw std::runtime_error("negative terrain map dimensions");
    std::ostringstream output;
    output << "GRMAP001\n";
    output << "WIDTH " << document.width << '\n';
    output << "HEIGHT " << document.height << '\n';
    output << "GENERATED " << document.generated << '\n';
    output << "GENSEED " << document.map_seed << '\n';
    output << "GENBASE " << delphi_decimal(document.base_height) << '\n';
    output << "GENEVENBORDERS " << (document.even_borders ? "true" : "false") << '\n';
    output << "GENHEIGHT " << delphi_decimal(document.map_height) << '\n';
    output << "GENCHAOS " << delphi_decimal(document.map_chaos) << '\n';
    output << "LEVHEIGHT " << delphi_decimal(document.level_height) << '\n';
    output << "LEVCHAOS " << delphi_decimal(document.level_chaos) << '\n';
    output << "MAPIMG " << document.map_image << "\n\n";
    output << "HEIGHTMAP\n";
    write_grid(output, document.heightmap, document.height + 1, document.width + 1, delphi_decimal);
    output << "HEIGHTMAPEND\n\nRANDOMSEEDS\n";
    write_grid(output, document.random_seeds, document.height, document.width, [](std::uint32_t value) { return std::to_string(value); });
    output << "RANDOMSEEDSEND\n\n";
    if (!include_height_overrides) return output.str();
    for (const auto& override : document.height_overrides) {
      output << "HEIGHTS " << override.level_name << '\n';
      for (int row = 0; row < 9; ++row) {
        for (int column = 0; column < 9; ++column) {
          if (column) output << ',';
          output << delphi_decimal(override.samples[static_cast<std::size_t>(row) * 9 + column]);
        }
        output << '\n';
      }
      output << "HEIGHTSEND\n";
    }
    return output.str();
  }
  std::string serialize_terrain_level(std::span<const double, 81> heights, bool include_heights) {
    std::ostringstream output;
    output << "GLEVNW01\n";
    if (!include_heights) return output.str();
    output << "HEIGHTS\n";
    for (int row = 0; row < 9; ++row) {
      for (int column = 0; column < 9; ++column) {
        if (column) output << ',';
        output << delphi_decimal(heights[static_cast<std::size_t>(row) * 9 + column]);
      }
      output << '\n';
    }
    output << "HEIGHTSEND\n";
    return output.str();
  }
}

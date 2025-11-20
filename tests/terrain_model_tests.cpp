#include "terrain_model.h"
#include "terrain_generator.h"
#include "terrain_format.h"
#include "terrain_preview.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <array>
#undef assert
#define assert(expression) do { if (!(expression)) return 1; } while (false)
int terrain_self_test() {
  terrain::Terrain map(2, 2);
  map.level(1, 1).at(8, 8) = 42.5;
  assert(map.sample_clamped(99, 99, 8, 8) == 42.5);
  map.edit_level_sample(0, 0, -1, 99, 7.0);
  assert(map.level(0, 0).at(0, 8) == 7.0);
  assert(map.level(0, 0).dirty);
  terrain::Terrain control_map(2, 2);
  control_map.apply_control_delta(1, 1, 8.0);
  assert(control_map.level(0, 0).at(8, 8) == 8.0);
  assert(control_map.level(1, 0).at(0, 8) == 8.0);
  assert(control_map.level(0, 1).at(8, 0) == 8.0);
  assert(control_map.level(1, 1).at(0, 0) == 8.0);
  assert(control_map.level(1, 1).at(4, 4) == 2.0);
  assert(!control_map.level(1, 1).dirty);
  terrain::TerrainRandom random(0);
  assert(random.drandseed() == 0);
  assert(random.drand() == 1.0 / 4294967296.0);
  assert(random.drandseed() == 0x00000001u);
  assert(random.drand() == static_cast<double>(0x08088406u) / 4294967296.0);
  assert(random.drandseed() == 0x08088406u);
  assert(random.drandi() == static_cast<std::int32_t>(0xdc6dac1fu));
  assert(random.drandseed() == 0xdc6dac1fu);
  std::ifstream file{TERRAIN_SOURCE_DIR "/original/myworld.gmap"};
  const std::string fixture{std::istreambuf_iterator<char>{file}, {}};
  const auto document = terrain::parse_terrain_gmap(fixture);
  assert(document.width == 32 && document.height == 32);
  assert(document.map_seed == 67387906u);
  assert(document.map_image == "myworld.png");
  assert(document.heightmap.size() == 33 * 33);
  assert(document.random_seeds.size() == 32 * 32);
  assert(document.random_seeds[32 * 31 + 31] == 1293905230u);
  const auto serialized = terrain::serialize_terrain_gmap(document, false);
  const auto normalized_fixture = [&fixture] {
    std::string normalized;
    for (std::size_t index = 0; index < fixture.size(); ++index) if (fixture[index] != '\r') normalized += fixture[index];
    return normalized;
  }();
  assert(serialized == normalized_fixture);
  const auto round_trip = terrain::parse_terrain_gmap(serialized);
  assert(round_trip.width == document.width && round_trip.height == document.height);
  assert(round_trip.map_image == document.map_image);
  assert(round_trip.heightmap == document.heightmap);
  assert(round_trip.random_seeds == document.random_seeds);
  const auto override_document = terrain::parse_terrain_gmap(
    "GRMAP001\nWIDTH 1\nHEIGHT 1\nHEIGHTMAP\n0,0\n0,0\nHEIGHTMAPEND\nRANDOMSEEDS\n7\nRANDOMSEEDSEND\nHEIGHTS one_aa.nw\n"
    "1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\n"
    "1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\n1,2,3,4,5,6,7,8,9\nHEIGHTSEND\n");
  assert(override_document.height_overrides.size() == 1);
  assert(override_document.height_overrides[0].level_name == "one_aa.nw");
  assert(override_document.height_overrides[0].samples[80] == 9.0);
  const std::array<double, 81> empty_level{};
  assert(terrain::serialize_terrain_level(empty_level, false) == "GLEVNW01\n");
  const auto level_text = terrain::serialize_terrain_level(override_document.height_overrides[0].samples, true);
  assert(level_text.starts_with("GLEVNW01\nHEIGHTS\n1,2,3,4,5,6,7,8,9\n"));
  assert(level_text.ends_with("1,2,3,4,5,6,7,8,9\nHEIGHTSEND\n"));
  std::array<double, 4> controls{0.0, 64.0, 128.0, 192.0};
  std::array<double, 81> overrides{};
  std::vector<double> generated_level;
  terrain::generate_level_terrain(controls, 1, 1, 0, 0, 7, 11, 5.0, 0.6, 5.0544, 0.6, overrides, false, generated_level);
  assert(generated_level.size() == 65 * 65);
  assert(generated_level[0] == 0.0 && generated_level[64] == 64.0);
  assert(generated_level[64 * 65] == 128.0 && generated_level[64 * 65 + 64] == 192.0);
  assert(generated_level[32 * 65 + 32] != 0.0);
  const auto grass = terrain::preview_color(10.0, 10.0, 10.0, 10.0, false);
  assert(grass.red == 0 && grass.green == 224 && grass.blue == 0);
  const auto lit_grass = terrain::preview_color(10.0, 10.0, 11.0, 11.0, true);
  assert(lit_grass.green == 255);
  std::vector<double> generated_controls;
  std::vector<std::uint32_t> generated_seeds;
  terrain::generate_map_terrain(2, 2, 67387906u, 0.0, false, 65.0, 0.6, generated_controls, generated_seeds);
  assert(generated_controls.size() == 9 && generated_seeds.size() == 4);
  assert(generated_controls[0] == 0.0 && generated_controls[2] == 0.0 && generated_controls[6] == 0.0 && generated_controls[8] == 0.0);
  assert(generated_seeds[0] != generated_seeds[1]);
  terrain::generate_map_terrain(document.width, document.height, document.map_seed, document.base_height, document.even_borders, document.map_height, document.map_chaos, generated_controls, generated_seeds);
  assert(generated_controls == document.heightmap);
  assert(generated_seeds == document.random_seeds);
  auto regenerated_document = document;
  terrain::regenerate_terrain_map(regenerated_document);
  assert(regenerated_document.heightmap == document.heightmap);
  assert(regenerated_document.random_seeds == document.random_seeds);
  terrain::edit_control_height(regenerated_document, 1, 1, 123.0);
  assert(regenerated_document.heightmap[1 * 33 + 1] == 123.0);
  assert(terrain::generated_level_name(document, 31, 31) == "myworld_bf-32.nw");
  assert(terrain::generated_level_name(document, 0, 0) == "myworld_aa-01.nw");
  return 0;
}

#include <chrono>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "bandit_live_world.h"
#include "basecamp.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "clzones.h"
#include "coordinates.h"
#include "faction.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "item_components.h"
#include "itype.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "messages.h"
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "pocket_type.h"
#include "point.h"
#include "stomach.h"
#include "type_id.h"
#include "units.h"
#include "value_ptr.h"
#include "weather.h"

static const itype_id itype_9mm("9mm");
static const itype_id itype_38_special("38_special");
static const itype_id itype_38_speedloader("38_speedloader");
static const itype_id itype_abaya("abaya");
static const itype_id itype_adhesive_bandages("adhesive_bandages");
static const itype_id itype_armor_lc_plate("armor_lc_plate");
static const itype_id itype_attachable_ear_muffs("attachable_ear_muffs");
static const itype_id itype_bandages_makeshift("bandages_makeshift");
static const itype_id itype_bandages_makeshift_bleached("bandages_makeshift_bleached");
static const itype_id itype_bandages_makeshift_boiled("bandages_makeshift_boiled");
static const itype_id itype_aspirin("aspirin");
static const itype_id itype_ballistic_vest_esapi("ballistic_vest_esapi");
static const itype_id itype_belt223("belt223");
static const itype_id itype_esapi_plate("esapi_plate");
static const itype_id itype_bandages("bandages");
static const itype_id itype_backpack("backpack");
static const itype_id itype_bholster("bholster");
static const itype_id itype_bio_blade_weapon("bio_blade_weapon");
static const itype_id itype_boots("boots");
static const itype_id itype_briefs("briefs");
static const itype_id itype_coat_rain("coat_rain");
static const itype_id itype_coat_winter("coat_winter");
static const itype_id itype_crude_plastic_vest("crude_plastic_vest");
static const itype_id itype_crossbow("crossbow");
static const itype_id itype_daypack("daypack");
static const itype_id itype_duffelbag("duffelbag");
static const itype_id itype_fishing_waders("fishing_waders");
static const itype_id itype_glasses_eye("glasses_eye");
static const itype_id itype_gloves_leather("gloves_leather");
static const itype_id itype_glock_19("glock_19");
static const itype_id itype_glockmag("glockmag");
static const itype_id itype_glockbigmag("glockbigmag");
static const itype_id itype_hakama("hakama");
static const itype_id itype_medical_gauze("medical_gauze");
static const itype_id itype_hazmat_suit("hazmat_suit");
static const itype_id itype_hakama_gi("hakama_gi");
static const itype_id itype_hat_ball("hat_ball");
static const itype_id itype_waist_apron_long("waist_apron_long");
static const itype_id itype_holster("holster");
static const itype_id itype_helmet_army("helmet_army");
static const itype_id itype_helmet_bike("helmet_bike");
static const itype_id itype_helmet_plate("helmet_plate");
static const itype_id itype_jacket_light("jacket_light");
static const itype_id itype_jeans("jeans");
static const itype_id itype_knee_pads("knee_pads");
static const itype_id itype_knife_combat("knife_combat");
static const itype_id itype_leg_small_bag("leg_small_bag");
static const itype_id itype_legguard_hard("legguard_hard");
static const itype_id itype_legguard_lc_brigandine("legguard_lc_brigandine");
static const itype_id itype_legguard_metal_sheets_hip("legguard_metal_sheets_hip");
static const itype_id itype_leggings("leggings");
static const itype_id itype_long_dress("long_dress");
static const itype_id itype_long_dress_sleeved("long_dress_sleeved");
static const itype_id itype_mask_dust("mask_dust");
static const itype_id itype_m249("m249");
static const itype_id itype_cheongsam("cheongsam");
static const itype_id itype_pants_cargo("pants_cargo");
static const itype_id itype_rock("rock");
static const itype_id itype_short_dress("short_dress");
static const itype_id itype_shorts_cargo("shorts_cargo");
static const itype_id itype_sneakers("sneakers");
static const itype_id itype_socks("socks");
static const itype_id itype_suit("suit");
static const itype_id itype_sw_619("sw_619");
static const itype_id itype_tool_belt("tool_belt");
static const itype_id itype_survivor_suit("survivor_suit");
static const itype_id itype_test_100_kcal("test_100_kcal");
static const itype_id itype_test_200_kcal("test_200_kcal");
static const itype_id itype_test_500_kcal("test_500_kcal");
static const itype_id itype_test_jumpsuit_cotton("test_jumpsuit_cotton");
static const itype_id itype_tshirt("tshirt");
static const itype_id itype_tux("tux");
static const itype_id itype_union_suit("union_suit");
static const itype_id itype_vest("vest");
static const itype_id itype_556("556");
static const itype_id itype_wetsuit("wetsuit");

static const vitamin_id vitamin_mutagen("mutagen");
static const vitamin_id vitamin_mutant_toxin("mutant_toxin");

static const weather_type_id weather_drizzle( "drizzle" );

static const zone_type_id zone_type_CAMP_FOOD("CAMP_FOOD");
static const zone_type_id zone_type_CAMP_STORAGE("CAMP_STORAGE");
static const zone_type_id zone_type_CAMP_LOCKER("CAMP_LOCKER");
static const zone_type_id zone_type_CAMP_PATROL("CAMP_PATROL");
static const zone_type_id zone_type_NO_NPC_PICKUP("NO_NPC_PICKUP");

static std::string find_snapshot_line(const std::string &snapshot,
                                      const std::string &prefix) {
  const size_t start = snapshot.find(prefix);
  if (start == std::string::npos) {
    return std::string();
  }
  const size_t end = snapshot.find('\n', start);
  return snapshot.substr(start, end == std::string::npos ? std::string::npos
                                                         : end - start);
}

static std::string lowercase_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](const unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return text;
}

static void create_tile_zone(const std::string &name,
                             const zone_type_id &zone_type,
                             const tripoint_abs_ms &pos) {
  zone_manager::get_manager().add(name, zone_type, your_fac, false, true, pos,
                                  pos, nullptr, false);
}

static void create_rect_zone(const std::string &name,
                             const zone_type_id &zone_type,
                             const tripoint_abs_ms &start,
                             const tripoint_abs_ms &end) {
  zone_manager::get_manager().add(name, zone_type, your_fac, false, true,
                                  start, end, nullptr, false);
}

static int count_character_items(Character &who, const itype_id &type) {
  int count = 0;
  who.visit_items([&](item *node, item *) {
    if (node != nullptr && node->typeId() == type) {
      count++;
    }
    return VisitResponse::NEXT;
  });
  return count;
}

static int count_tile_items(map &here, const tripoint_bub_ms &pos,
                            const itype_id &type) {
  int count = 0;
  for (const item &it : here.i_at(pos)) {
    if (it.typeId() == type) {
      count++;
    }
  }
  return count;
}

static int count_top_level_items(map &here,
                                 const std::vector<tripoint_abs_ms> &tiles) {
  int total = 0;
  for (const tripoint_abs_ms &tile : tiles) {
    total += static_cast<int>(here.i_at(here.get_bub(tile)).size());
  }
  return total;
}

static void set_map_temperature(units::temperature new_temperature) {
  get_weather().temperature = new_temperature;
  get_weather().clear_temp_cache();
}

static camp_patrol_cluster make_patrol_cluster(
    map &here, std::initializer_list<tripoint_bub_ms> local_tiles) {
  camp_patrol_cluster cluster;
  cluster.reserve(local_tiles.size());
  for (const tripoint_bub_ms &tile : local_tiles) {
    cluster.push_back(here.get_abs(tile));
  }
  std::sort(cluster.begin(), cluster.end(), [](const tripoint_abs_ms &lhs,
                                               const tripoint_abs_ms &rhs) {
    if (lhs.z() != rhs.z()) {
      return lhs.z() < rhs.z();
    }
    if (lhs.y() != rhs.y()) {
      return lhs.y() < rhs.y();
    }
    return lhs.x() < rhs.x();
  });
  return cluster;
}

static item make_loaded_glock_magazine(int rounds = 15) {
  item magazine(itype_glockmag);
  const ret_val<void> loaded =
      magazine.put_in(item(itype_9mm, calendar::turn_zero, rounds),
                      pocket_type::MAGAZINE);
  if (!loaded.success()) {
    throw std::runtime_error(loaded.str());
  }
  return magazine;
}

static item make_filled_daypack(int rocks = 3) {
  item bag(itype_daypack);
  for (int i = 0; i < rocks; ++i) {
    const ret_val<void> inserted =
        bag.put_in(item(itype_rock), pocket_type::CONTAINER);
    if (!inserted.success()) {
      throw std::runtime_error(inserted.str());
    }
  }
  return bag;
}

struct locker_threshold_timing_sample {
  camp_locker_service_probe probe;
  long long median_us = 0;
  long long p95_us = 0;
};

static long long median_duration_us(std::vector<long long> samples) {
  if (samples.empty()) {
    return 0;
  }
  std::sort(samples.begin(), samples.end());
  const size_t middle = samples.size() / 2;
  if (samples.size() % 2 == 0) {
    return (samples[middle - 1] + samples[middle]) / 2;
  }
  return samples[middle];
}

static long long p95_duration_us(std::vector<long long> samples) {
  if (samples.empty()) {
    return 0;
  }
  std::sort(samples.begin(), samples.end());
  const size_t index = (samples.size() - 1) * 95 / 100;
  return samples[index];
}

template <typename ProbeFn>
static locker_threshold_timing_sample measure_locker_threshold_timing(
    ProbeFn &&probe_fn, int repeats) {
  std::vector<long long> samples;
  samples.reserve(repeats);

  camp_locker_service_probe last_probe = probe_fn();
  for (int i = 0; i < repeats; ++i) {
    const auto started_at = std::chrono::steady_clock::now();
    last_probe = probe_fn();
    const auto finished_at = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                          finished_at - started_at)
                          .count());
  }

  return {last_probe, median_duration_us(samples), p95_duration_us(samples)};
}

TEST_CASE("camp_locker_policy_serialization", "[camp][locker]") {
  REQUIRE(zone_manager::get_manager().has_type(zone_type_CAMP_LOCKER));

  basecamp test_camp("Locker Camp", tripoint_abs_omt(8, 8, 0));
  CHECK(test_camp.get_locker_policy().enabled_count() ==
        static_cast<int>(all_camp_locker_slots().size()));

  test_camp.set_locker_slot_enabled(camp_locker_slot::socks, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::gloves, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::bag, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::holster, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::ranged_weapon, false);
  test_camp.set_locker_prefers_bulletproof( true );

  std::ostringstream os;
  JsonOut jsout(os);
  test_camp.serialize(jsout);

  JsonValue jsin = json_loader::from_string(os.str());
  JsonObject jo = jsin.get_object();
  basecamp loaded;
  loaded.deserialize(jo);

  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::socks));
  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::gloves));
  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::bag));
  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::holster));
  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::ranged_weapon));
  CHECK(loaded.is_locker_slot_enabled(camp_locker_slot::pants));
  CHECK(loaded.locker_prefers_bulletproof());
  CHECK(loaded.get_locker_policy().enabled_count() ==
        static_cast<int>(all_camp_locker_slots().size()) - 5);
}

TEST_CASE("camp_locker_item_classification", "[camp][locker]") {
  CHECK(classify_camp_locker_item(item(itype_briefs)) ==
        camp_locker_slot::underwear);
  CHECK(classify_camp_locker_item(item(itype_socks)) ==
        camp_locker_slot::socks);
  CHECK(classify_camp_locker_item(item(itype_gloves_leather)) ==
        camp_locker_slot::gloves);
  CHECK(classify_camp_locker_item(item(itype_sneakers)) ==
        camp_locker_slot::shoes);
  CHECK(classify_camp_locker_item(item(itype_jeans)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_leggings)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_fishing_waders)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_test_jumpsuit_cotton)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_cheongsam)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_abaya)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_long_dress)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_long_dress_sleeved)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_short_dress)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_hazmat_suit)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_survivor_suit)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_suit)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_tux)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_union_suit)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_wetsuit)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_mask_dust)) ==
        camp_locker_slot::mask);
  CHECK(classify_camp_locker_item(item(itype_tool_belt)) ==
        camp_locker_slot::belt);
  CHECK(classify_camp_locker_item(item(itype_bholster)) ==
        camp_locker_slot::holster);
  CHECK(classify_camp_locker_item(item(itype_holster)) ==
        camp_locker_slot::holster);
  CHECK_FALSE(classify_camp_locker_item(item(itype_knee_pads)).has_value());
  CHECK_FALSE(classify_camp_locker_item(item(itype_leg_small_bag)).has_value());
  CHECK_FALSE(classify_camp_locker_item(item(itype_legguard_hard)).has_value());
  CHECK_FALSE(classify_camp_locker_item(item(itype_legguard_lc_brigandine)).has_value());
  CHECK_FALSE(classify_camp_locker_item(item(itype_legguard_metal_sheets_hip)).has_value());
  CHECK_FALSE(classify_camp_locker_item(item(itype_hakama)).has_value());
  CHECK_FALSE(classify_camp_locker_item(item(itype_hakama_gi)).has_value());
  CHECK_FALSE(
      classify_camp_locker_item(item(itype_waist_apron_long)).has_value());
  CHECK(classify_camp_locker_item(item(itype_tshirt)) ==
        camp_locker_slot::shirt);
  CHECK(classify_camp_locker_item(item(itype_vest)) == camp_locker_slot::vest);
  CHECK(classify_camp_locker_item(item(itype_ballistic_vest_esapi)) ==
        camp_locker_slot::body_armor);
  CHECK(classify_camp_locker_item(item(itype_armor_lc_plate)) ==
        camp_locker_slot::body_armor);
  CHECK(classify_camp_locker_item(item(itype_hat_ball)) ==
        camp_locker_slot::helmet);
  CHECK(classify_camp_locker_item(item(itype_helmet_army)) ==
        camp_locker_slot::helmet);
  CHECK(classify_camp_locker_item(item(itype_helmet_bike)) ==
        camp_locker_slot::helmet);
  CHECK(classify_camp_locker_item(item(itype_helmet_plate)) ==
        camp_locker_slot::helmet);
  CHECK(classify_camp_locker_item(item(itype_glasses_eye)) ==
        camp_locker_slot::glasses);
  CHECK(classify_camp_locker_item(item(itype_backpack)) ==
        camp_locker_slot::bag);
  CHECK(classify_camp_locker_item(item(itype_knife_combat)) ==
        camp_locker_slot::melee_weapon);
  CHECK(classify_camp_locker_item(item(itype_bio_blade_weapon)) ==
        camp_locker_slot::melee_weapon);
  CHECK(classify_camp_locker_item(item(itype_glock_19)) ==
        camp_locker_slot::ranged_weapon);
  CHECK(classify_camp_locker_item(item(itype_crossbow)) ==
        camp_locker_slot::ranged_weapon);
}

TEST_CASE("camp_locker_zone_candidate_gathering", "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{5, 5, 0});
  const tripoint_abs_ms offzone_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_abs_ms blocked_abs = here.get_abs(tripoint_bub_ms{7, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);
  const tripoint_bub_ms offzone_local = here.get_bub(offzone_abs);
  const tripoint_bub_ms blocked_local = here.get_bub(blocked_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  create_tile_zone("Blocked locker", zone_type_CAMP_LOCKER, blocked_abs);
  create_tile_zone("No NPC pickup", zone_type_NO_NPC_PICKUP, blocked_abs);
  here.i_clear(locker_local);
  here.i_clear(offzone_local);
  here.i_clear(blocked_local);
  here.add_item_or_charges(locker_local, item(itype_sneakers));
  here.add_item_or_charges(locker_local, item(itype_gloves_leather));
  here.add_item_or_charges(locker_local, item(itype_mask_dust));
  here.add_item_or_charges(locker_local, item(itype_tool_belt));
  here.add_item_or_charges(locker_local, item(itype_holster));
  here.add_item_or_charges(locker_local, item(itype_backpack));
  here.add_item_or_charges(locker_local, item(itype_glock_19));
  here.add_item_or_charges(offzone_local, item(itype_helmet_bike));
  here.add_item_or_charges(blocked_local, item(itype_helmet_army));

  basecamp test_camp("Locker Camp", project_to<coords::omt>(locker_abs));
  test_camp.set_locker_slot_enabled(camp_locker_slot::belt, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::bag, false);

  const camp_locker_candidate_map candidates =
      collect_camp_locker_zone_candidates(locker_abs, your_fac,
                                          test_camp.get_locker_policy());

  REQUIRE(candidates.count(camp_locker_slot::shoes) == 1);
  REQUIRE(candidates.at(camp_locker_slot::shoes).size() == 1);
  CHECK(candidates.at(camp_locker_slot::shoes).front()->typeId() ==
        itype_sneakers);

  REQUIRE(candidates.count(camp_locker_slot::gloves) == 1);
  REQUIRE(candidates.at(camp_locker_slot::gloves).size() == 1);
  CHECK(candidates.at(camp_locker_slot::gloves).front()->typeId() ==
        itype_gloves_leather);

  REQUIRE(candidates.count(camp_locker_slot::mask) == 1);
  REQUIRE(candidates.at(camp_locker_slot::mask).size() == 1);
  CHECK(candidates.at(camp_locker_slot::mask).front()->typeId() ==
        itype_mask_dust);

  REQUIRE(candidates.count(camp_locker_slot::holster) == 1);
  REQUIRE(candidates.at(camp_locker_slot::holster).size() == 1);
  CHECK(candidates.at(camp_locker_slot::holster).front()->typeId() ==
        itype_holster);

  REQUIRE(candidates.count(camp_locker_slot::ranged_weapon) == 1);
  REQUIRE(candidates.at(camp_locker_slot::ranged_weapon).size() == 1);
  CHECK(candidates.at(camp_locker_slot::ranged_weapon).front()->typeId() ==
        itype_glock_19);

  CHECK(candidates.count(camp_locker_slot::belt) == 0);
  CHECK(candidates.count(camp_locker_slot::bag) == 0);
  CHECK(candidates.count(camp_locker_slot::helmet) == 0);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_zone_surface_and_sorted_tiles", "[camp][patrol]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  REQUIRE(zone_manager::get_manager().has_type(zone_type_CAMP_PATROL));

  map &here = get_map();
  const tripoint_abs_ms tile_a = here.get_abs(tripoint_bub_ms{9, 7, 0});
  const tripoint_abs_ms tile_b = here.get_abs(tripoint_bub_ms{5, 7, 0});
  const tripoint_abs_ms tile_c = here.get_abs(tripoint_bub_ms{5, 5, 0});

  create_tile_zone("Patrol C", zone_type_CAMP_PATROL, tile_c);
  create_tile_zone("Patrol A", zone_type_CAMP_PATROL, tile_a);
  create_tile_zone("Patrol B", zone_type_CAMP_PATROL, tile_b);

  const std::vector<tripoint_abs_ms> sorted_tiles =
      collect_sorted_camp_patrol_tiles(tile_a, your_fac);

  REQUIRE(sorted_tiles.size() == 3);
  CHECK(sorted_tiles[0] == tile_c);
  CHECK(sorted_tiles[1] == tile_b);
  CHECK(sorted_tiles[2] == tile_a);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_zone_clusters_use_4_way_connectivity",
          "[camp][patrol]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms a0 = here.get_abs(tripoint_bub_ms{5, 5, 0});
  const tripoint_abs_ms a1 = here.get_abs(tripoint_bub_ms{5, 6, 0});
  const tripoint_abs_ms a2 = here.get_abs(tripoint_bub_ms{6, 6, 0});
  const tripoint_abs_ms b0 = here.get_abs(tripoint_bub_ms{8, 8, 0});
  const tripoint_abs_ms c0 = here.get_abs(tripoint_bub_ms{9, 6, 0});
  const tripoint_abs_ms c1 = here.get_abs(tripoint_bub_ms{10, 6, 0});

  create_tile_zone("Patrol A0", zone_type_CAMP_PATROL, a0);
  create_tile_zone("Patrol A1", zone_type_CAMP_PATROL, a1);
  create_tile_zone("Patrol A2", zone_type_CAMP_PATROL, a2);
  create_tile_zone("Patrol B0", zone_type_CAMP_PATROL, b0);
  create_tile_zone("Patrol C0", zone_type_CAMP_PATROL, c0);
  create_tile_zone("Patrol C1", zone_type_CAMP_PATROL, c1);

  const std::vector<camp_patrol_cluster> clusters =
      collect_camp_patrol_clusters(a0, your_fac);

  REQUIRE(clusters.size() == 3);
  REQUIRE(clusters[0].size() == 3);
  CHECK(clusters[0][0] == a0);
  CHECK(clusters[0][1] == a1);
  CHECK(clusters[0][2] == a2);

  REQUIRE(clusters[1].size() == 2);
  CHECK(clusters[1][0] == c0);
  CHECK(clusters[1][1] == c1);

  REQUIRE(clusters[2].size() == 1);
  CHECK(clusters[2][0] == b0);

  zone_manager::get_manager().clear();
}

TEST_CASE("legacy_job_data_load_adds_missing_patrol_priority_surface",
          "[camp][patrol]") {
  static const activity_id ACT_CAMP_PATROL("ACT_CAMP_PATROL");

  job_data loaded;
  JsonValue jsin = json_loader::from_string(
      R"({"task_priorities":{"ACT_MOVE_LOOT":6,"ACT_MULTIPLE_FARM":4},"fetch_history":{}})");
  loaded.deserialize(jsin);

  CHECK(loaded.get_priority_of_job(ACT_CAMP_PATROL) == 0);
  loaded.set_all_priorities(3);
  CHECK(loaded.get_priority_of_job(ACT_CAMP_PATROL) == 3);
}

TEST_CASE("job_data_serializes_activity_cooldowns", "[camp][jobs]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  static const activity_id ACT_MOVE_LOOT("ACT_MOVE_LOOT");

  job_data saved;
  REQUIRE(saved.set_task_priority(ACT_MOVE_LOOT, 6));
  saved.block_job_until(ACT_MOVE_LOOT, calendar::turn + 30_minutes);

  std::ostringstream os;
  JsonOut jsout(os);
  saved.serialize(jsout);

  JsonValue jsin = json_loader::from_string(os.str());
  job_data loaded;
  loaded.deserialize(jsin);

  CHECK(loaded.get_priority_of_job(ACT_MOVE_LOOT) == 6);
  CHECK(loaded.is_job_blocked(ACT_MOVE_LOOT));

  calendar::turn += 31_minutes;
  CHECK_FALSE(loaded.is_job_blocked(ACT_MOVE_LOOT));
}

TEST_CASE("camp_patrol_worker_pool_uses_patrol_priority_surface",
          "[camp][patrol]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  static const activity_id ACT_CAMP_PATROL("ACT_CAMP_PATROL");

  map &here = get_map();
  const tripoint_abs_ms camp_abs = here.get_abs(tripoint_bub_ms{5, 5, 0});
  basecamp test_camp("Patrol Camp", project_to<coords::omt>(camp_abs));

  npc &high_priority_guard = spawn_npc(tripoint_bub_ms{6, 5, 0}.xy(), "thug");
  npc &ignored_guard = spawn_npc(tripoint_bub_ms{7, 5, 0}.xy(), "thug");
  npc &mid_priority_guard = spawn_npc(tripoint_bub_ms{8, 5, 0}.xy(), "thug");

  REQUIRE(high_priority_guard.job.set_task_priority(ACT_CAMP_PATROL, 8));
  REQUIRE(ignored_guard.job.set_task_priority(ACT_CAMP_PATROL, 0));
  REQUIRE(mid_priority_guard.job.set_task_priority(ACT_CAMP_PATROL, 4));

  test_camp.add_assignee(high_priority_guard.getID());
  test_camp.add_assignee(ignored_guard.getID());
  test_camp.add_assignee(mid_priority_guard.getID());

  const std::vector<camp_patrol_worker> workers =
      collect_camp_patrol_workers(test_camp.get_npcs_assigned());

  REQUIRE(workers.size() == 2);
  CHECK(workers[0].worker_id == high_priority_guard.getID());
  CHECK(workers[0].priority == 8);
  CHECK(workers[1].worker_id == mid_priority_guard.getID());
  CHECK(workers[1].priority == 4);
}

TEST_CASE("camp_patrol_planner_contract", "[camp][patrol]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();

  SECTION("1 guard covers four disconnected posts on the day shift") {
    const std::vector<camp_patrol_worker> workers = {{character_id(101), 5}};
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here, {{10, 10, 0}}),
        make_patrol_cluster(here, {{12, 10, 0}}),
        make_patrol_cluster(here, {{14, 10, 0}}),
        make_patrol_cluster(here, {{16, 10, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.active_guards.size() == 1);
    CHECK(plan.day.reserve_guards.empty());
    CHECK(plan.day.active_guards[0].worker_id == character_id(101));
    CHECK(plan.day.active_guards[0].cluster_indices ==
          std::vector<size_t>({0, 1, 2, 3}));
    for (const camp_patrol_cluster_plan &cluster : plan.day.clusters) {
      REQUIRE(cluster.assigned_guards.size() == 1);
      CHECK(cluster.assigned_guards.front() == character_id(101));
    }

    CHECK(plan.night.active_guards.empty());
    CHECK(plan.night.reserve_guards.empty());
  }

  SECTION("4 guards split into two shifts across four disconnected posts") {
    const std::vector<camp_patrol_worker> workers = {
        {character_id(201), 5}, {character_id(202), 4},
        {character_id(203), 3}, {character_id(204), 2},
    };
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here, {{10, 12, 0}}),
        make_patrol_cluster(here, {{12, 12, 0}}),
        make_patrol_cluster(here, {{14, 12, 0}}),
        make_patrol_cluster(here, {{16, 12, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.active_guards.size() == 2);
    CHECK(plan.day.reserve_guards.empty());
    CHECK(plan.day.active_guards[0].worker_id == character_id(201));
    CHECK(plan.day.active_guards[1].worker_id == character_id(203));
    CHECK(plan.day.active_guards[0].cluster_indices == std::vector<size_t>({0, 2}));
    CHECK(plan.day.active_guards[1].cluster_indices == std::vector<size_t>({1, 3}));

    REQUIRE(plan.night.active_guards.size() == 2);
    CHECK(plan.night.reserve_guards.empty());
    CHECK(plan.night.active_guards[0].worker_id == character_id(202));
    CHECK(plan.night.active_guards[1].worker_id == character_id(204));
    CHECK(plan.night.active_guards[0].cluster_indices == std::vector<size_t>({0, 2}));
    CHECK(plan.night.active_guards[1].cluster_indices == std::vector<size_t>({1, 3}));
  }

  SECTION("mixed clusters get one guard first before extra coverage stacks") {
    const std::vector<camp_patrol_worker> workers = {
        {character_id(301), 9}, {character_id(302), 8}, {character_id(303), 7},
        {character_id(304), 6}, {character_id(305), 5}, {character_id(306), 4},
        {character_id(307), 3}, {character_id(308), 2}, {character_id(309), 1},
        {character_id(310), 1},
    };
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{10, 14, 0}, {11, 14, 0}, {12, 14, 0}, {13, 14, 0}}),
        make_patrol_cluster(here, {{10, 16, 0}, {11, 16, 0}}),
        make_patrol_cluster(here, {{10, 18, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.active_guards.size() == 5);
    CHECK(plan.day.clusters[0].assigned_guards.size() == 3);
    CHECK(plan.day.clusters[1].assigned_guards.size() == 1);
    CHECK(plan.day.clusters[2].assigned_guards.size() == 1);
    CHECK(plan.day.active_guards[0].cluster_indices == std::vector<size_t>({0}));
    CHECK(plan.day.active_guards[1].cluster_indices == std::vector<size_t>({1}));
    CHECK(plan.day.active_guards[2].cluster_indices == std::vector<size_t>({2}));
    CHECK(plan.day.active_guards[3].cluster_indices == std::vector<size_t>({0}));
    CHECK(plan.day.active_guards[4].cluster_indices == std::vector<size_t>({0}));
  }

  SECTION("connected four-tile cluster caps each shift at four active guards") {
    std::vector<camp_patrol_worker> workers;
    for (int index = 0; index < 16; ++index) {
      workers.push_back(
          camp_patrol_worker{character_id(400 + index), 16 - index});
    }
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{20, 10, 0}, {21, 10, 0}, {20, 11, 0}, {21, 11, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.active_guards.size() == 4);
    CHECK(plan.day.reserve_guards.size() == 4);
    CHECK(plan.day.clusters[0].assigned_guards.size() == 4);
    REQUIRE(plan.night.active_guards.size() == 4);
    CHECK(plan.night.reserve_guards.size() == 4);
    CHECK(plan.night.clusters[0].assigned_guards.size() == 4);
  }
}

TEST_CASE("camp_patrol_interrupt_contract", "[camp][patrol]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();

  SECTION("routine chores cannot steal an on-shift guard") {
    std::vector<camp_patrol_worker> workers;
    for (int index = 0; index < 10; ++index) {
      workers.push_back(
          camp_patrol_worker{character_id(500 + index), 10 - index});
    }
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{20, 20, 0}, {21, 20, 0}, {20, 21, 0}, {21, 21, 0}}),
    };

    camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.active_guards.size() == 4);
    REQUIRE(plan.day.reserve_guards == std::vector<character_id>({character_id(508)}));
    CHECK_FALSE(camp_patrol_interrupt_is_whitelisted(
        camp_patrol_interrupt_reason::routine_chore));
    CHECK_FALSE(apply_camp_patrol_guard_interrupt(
        plan.day, character_id(500),
        camp_patrol_interrupt_reason::routine_chore));
    CHECK(plan.day.active_guards[0].worker_id == character_id(500));
    CHECK(plan.day.reserve_guards == std::vector<character_id>({character_id(508)}));
    CHECK(plan.day.clusters[0].assigned_guards ==
          std::vector<character_id>({character_id(500), character_id(502),
                                     character_id(504), character_id(506)}));
  }

  SECTION("urgent loss backfills from reserve without reshuffling other guards") {
    std::vector<camp_patrol_worker> workers;
    for (int index = 0; index < 10; ++index) {
      workers.push_back(
          camp_patrol_worker{character_id(600 + index), 10 - index});
    }
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{24, 20, 0}, {25, 20, 0}, {24, 21, 0}, {25, 21, 0}}),
    };

    camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.reserve_guards == std::vector<character_id>({character_id(608)}));
    CHECK(camp_patrol_interrupt_is_whitelisted(
        camp_patrol_interrupt_reason::combat_threat));
    CHECK(apply_camp_patrol_guard_interrupt(
        plan.day, character_id(600),
        camp_patrol_interrupt_reason::combat_threat));
    REQUIRE(plan.day.active_guards.size() == 4);
    CHECK(plan.day.active_guards[0].worker_id == character_id(608));
    CHECK(plan.day.active_guards[0].cluster_indices == std::vector<size_t>({0}));
    CHECK(plan.day.active_guards[1].worker_id == character_id(602));
    CHECK(plan.day.active_guards[2].worker_id == character_id(604));
    CHECK(plan.day.active_guards[3].worker_id == character_id(606));
    CHECK(plan.day.reserve_guards.empty());
    CHECK(plan.day.clusters[0].assigned_guards ==
          std::vector<character_id>({character_id(602), character_id(604),
                                     character_id(606), character_id(608)}));
  }

  SECTION("urgent loss without reserve leaves a gap instead of reshuffling") {
    std::vector<camp_patrol_worker> workers;
    for (int index = 0; index < 8; ++index) {
      workers.push_back(camp_patrol_worker{character_id(700 + index), 8 - index});
    }
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{28, 20, 0}, {29, 20, 0}, {28, 21, 0}, {29, 21, 0}}),
    };

    camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.reserve_guards.empty());
    CHECK(apply_camp_patrol_guard_interrupt(
        plan.day, character_id(700),
        camp_patrol_interrupt_reason::explicit_reassignment));
    REQUIRE(plan.day.active_guards.size() == 3);
    CHECK(plan.day.active_guards[0].worker_id == character_id(702));
    CHECK(plan.day.active_guards[1].worker_id == character_id(704));
    CHECK(plan.day.active_guards[2].worker_id == character_id(706));
    CHECK(plan.day.clusters[0].assigned_guards ==
          std::vector<character_id>({character_id(702), character_id(704),
                                     character_id(706)}));
  }
}

TEST_CASE("camp_patrol_shift_roster_latches_until_boundary",
          "[camp][patrol]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms patrol_abs = here.get_abs(tripoint_bub_ms{10, 10, 0});
  create_tile_zone("Patrol Post", zone_type_CAMP_PATROL, patrol_abs);

  basecamp test_camp("Patrol Camp", project_to<coords::omt>(patrol_abs));
  test_camp.set_owner(your_fac);
  test_camp.set_bb_pos(patrol_abs);

  npc &day_guard = spawn_npc(tripoint_bub_ms{6, 5, 0}.xy(), "thug");
  npc &night_guard = spawn_npc(tripoint_bub_ms{7, 5, 0}.xy(), "thug");
  npc &replacement_guard = spawn_npc(tripoint_bub_ms{8, 5, 0}.xy(), "thug");

  static const activity_id ACT_CAMP_PATROL("ACT_CAMP_PATROL");
  REQUIRE(day_guard.job.set_task_priority(ACT_CAMP_PATROL, 9));
  REQUIRE(night_guard.job.set_task_priority(ACT_CAMP_PATROL, 8));
  REQUIRE(replacement_guard.job.set_task_priority(ACT_CAMP_PATROL, 0));

  test_camp.add_assignee(day_guard.getID());
  test_camp.add_assignee(night_guard.getID());
  test_camp.add_assignee(replacement_guard.getID());

  calendar::turn = sunrise(calendar::turn_zero) + 2_hours;
  const camp_patrol_shift_plan *day_plan = test_camp.get_current_patrol_shift_plan();
  REQUIRE(day_plan != nullptr);
  REQUIRE(day_plan->shift == camp_patrol_shift::day);
  CHECK(day_plan->roster == std::vector<character_id>({day_guard.getID()}));
  CHECK(test_camp.is_worker_on_patrol_shift(day_guard));
  CHECK_FALSE(test_camp.is_worker_on_patrol_shift(replacement_guard));

  REQUIRE(night_guard.job.set_task_priority(ACT_CAMP_PATROL, 0));
  REQUIRE(replacement_guard.job.set_task_priority(ACT_CAMP_PATROL, 7));

  const camp_patrol_shift_plan *still_day_plan =
      test_camp.get_current_patrol_shift_plan();
  REQUIRE(still_day_plan != nullptr);
  CHECK(still_day_plan->roster == std::vector<character_id>({day_guard.getID()}));
  CHECK_FALSE(test_camp.is_worker_on_patrol_shift(replacement_guard));

  calendar::turn = sunset(calendar::turn_zero) + 2_hours;
  const camp_patrol_shift_plan *night_plan =
      test_camp.get_current_patrol_shift_plan();
  REQUIRE(night_plan != nullptr);
  REQUIRE(night_plan->shift == camp_patrol_shift::night);
  CHECK(night_plan->roster == std::vector<character_id>({replacement_guard.getID()}));
  CHECK(test_camp.is_worker_on_patrol_shift(replacement_guard));
  CHECK_FALSE(test_camp.is_worker_on_patrol_shift(day_guard));

  calendar::turn = sunrise(calendar::turn_zero) + 3_hours;
  REQUIRE(test_camp.raise_patrol_alarm(day_guard.getID(), 10_minutes));
  CHECK(test_camp.is_patrol_alarm_active());
  const camp_patrol_shift_plan *alarm_plan =
      test_camp.get_current_patrol_shift_plan();
  REQUIRE(alarm_plan != nullptr);
  REQUIRE(alarm_plan->shift == camp_patrol_shift::day);
  CHECK(alarm_plan->roster == std::vector<character_id>({day_guard.getID(),
                                                         replacement_guard.getID()}));

  calendar::turn += 11_minutes;
  CHECK_FALSE(test_camp.is_patrol_alarm_active());
  CHECK_FALSE(test_camp.is_worker_on_patrol_shift(replacement_guard));

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_assessment_engages_hostile_bandits_without_neutral_false_positive",
          "[camp][patrol]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map();
  clear_creatures();
  zone_manager::get_manager().clear();

  map &here = get_map();
  get_player_character().setpos( here, tripoint_bub_ms{50, 49, 0} );
  const tripoint_bub_ms guard_local{50, 50, 0};
  const tripoint_abs_ms patrol_abs = here.get_abs(tripoint_bub_ms{52, 50, 0});
  create_tile_zone("Patrol Post", zone_type_CAMP_PATROL, patrol_abs);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(patrol_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);
  test_camp->set_bb_pos(patrol_abs);

  npc &guard = spawn_npc( guard_local.xy(), "thug" );
  clear_character( guard, true );
  guard.setpos( here, guard_local );
  static const faction_id faction_free_merchants( "free_merchants" );
  guard.set_fac( faction_free_merchants );
  static const activity_id ACT_CAMP_PATROL( "ACT_CAMP_PATROL" );
  REQUIRE( guard.job.set_task_priority( ACT_CAMP_PATROL, 9 ) );
  test_camp->add_assignee( guard.getID() );

  const tripoint_bub_ms neutral_local{50, 51, 0};
  npc &neutral = spawn_npc( neutral_local.xy(), "thug" );
  clear_character( neutral, true );
  neutral.setpos( here, neutral_local );
  static const faction_id faction_no_faction( "no_faction" );
  neutral.set_fac( faction_no_faction );

  calendar::turn = sunrise(calendar::turn_zero) + 2_hours;
  guard.regen_ai_cache();
  CHECK( guard.current_target() == nullptr );
  CHECK_FALSE( test_camp->is_patrol_alarm_active() );
  CHECK( guard.attitude_to( neutral ) != Creature::Attitude::HOSTILE );

  const tripoint_bub_ms raider_local{52, 50, 0};
  npc &raider = spawn_npc( raider_local.xy(), "bandit" );
  clear_character( raider, true );
  raider.setpos( here, raider_local );
  static const faction_id faction_hells_raiders( "hells_raiders" );
  raider.set_fac( faction_hells_raiders );
  CHECK( guard.attitude_to( raider ) == Creature::Attitude::HOSTILE );
  CHECK( here.has_potential_los( guard.pos_bub(), raider.pos_bub() ) );
  CHECK( guard.sees( here, raider.pos_bub( here ) ) );
  guard.regen_ai_cache();
  CHECK( guard.current_target() == &raider );
  CHECK( test_camp->is_patrol_alarm_active() );

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_alarm_watches_active_shakedown_contact_without_combat_escalation",
          "[camp][patrol][bandit][live_world]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  restore_on_out_of_scope restore_bandit_world( overmap_buffer.global_state.bandit_live_world );
  clear_avatar();
  clear_map();
  clear_creatures();
  zone_manager::get_manager().clear();
  overmap_buffer.global_state.bandit_live_world.clear();

  map &here = get_map();
  get_player_character().setpos( here, tripoint_bub_ms{50, 49, 0} );
  const tripoint_bub_ms guard_local{50, 50, 0};
  const tripoint_abs_ms patrol_abs = here.get_abs( tripoint_bub_ms{52, 50, 0} );
  create_tile_zone( "Patrol Post", zone_type_CAMP_PATROL, patrol_abs );

  const tripoint_abs_omt camp_omt = project_to<coords::omt>( patrol_abs );
  here.add_camp( camp_omt, "faction_camp" );
  std::optional<basecamp *> bcp = overmap_buffer.find_camp( camp_omt.xy() );
  REQUIRE( !!bcp );
  basecamp *test_camp = *bcp;
  test_camp->set_owner( your_fac );
  test_camp->set_bb_pos( patrol_abs );

  npc &guard = spawn_npc( guard_local.xy(), "thug" );
  clear_character( guard, true );
  guard.setpos( here, guard_local );
  static const faction_id faction_free_merchants( "free_merchants" );
  guard.set_fac( faction_free_merchants );
  static const activity_id ACT_CAMP_PATROL( "ACT_CAMP_PATROL" );
  REQUIRE( guard.job.set_task_priority( ACT_CAMP_PATROL, 9 ) );
  test_camp->add_assignee( guard.getID() );

  const tripoint_bub_ms raider_local{52, 50, 0};
  npc &raider = spawn_npc( raider_local.xy(), "bandit" );
  clear_character( raider, true );
  raider.setpos( here, raider_local );
  static const faction_id faction_hells_raiders( "hells_raiders" );
  raider.set_fac( faction_hells_raiders );

  bandit_live_world::site_record site;
  site.site_id = "test:active_toll_parley";
  site.site_kind = bandit_live_world::owned_site_kind::bandit_camp;
  site.profile = bandit_live_world::hostile_site_profile::camp_style;
  site.anchor = raider.pos_abs_omt();
  site.active_group_id = "test_active_toll_group";
  site.active_job_type = "toll";
  site.active_target_id = "player_basecamp_nearby";
  site.active_member_ids.push_back( raider.getID() );
  bandit_live_world::member_record member;
  member.npc_id = raider.getID();
  member.state = bandit_live_world::member_state::local_contact;
  site.members.push_back( member );
  overmap_buffer.global_state.bandit_live_world.sites.push_back( site );

  CHECK( guard.attitude_to( raider ) == Creature::Attitude::HOSTILE );
  CHECK( here.has_potential_los( guard.pos_bub(), raider.pos_bub() ) );
  CHECK( guard.sees( here, raider.pos_bub( here ) ) );
  calendar::turn = sunrise( calendar::turn_zero ) + 2_hours;
  guard.regen_ai_cache();
  CHECK( guard.current_target() == nullptr );
  CHECK( test_camp->is_patrol_alarm_active() );

  site.last_shakedown_outcome = "fight_unresolved";
  overmap_buffer.global_state.bandit_live_world.sites.front() = site;
  guard.regen_ai_cache();
  CHECK( guard.current_target() == &raider );

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_assessment_engages_zombies", "[camp][patrol]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map();
  clear_creatures();
  zone_manager::get_manager().clear();

  map &here = get_map();
  get_player_character().setpos( here, tripoint_bub_ms{50, 49, 0} );
  const tripoint_bub_ms guard_local{50, 50, 0};
  const tripoint_abs_ms patrol_abs = here.get_abs(tripoint_bub_ms{52, 50, 0});
  create_tile_zone("Patrol Post", zone_type_CAMP_PATROL, patrol_abs);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(patrol_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);
  test_camp->set_bb_pos(patrol_abs);

  npc &guard = spawn_npc( guard_local.xy(), "thug" );
  clear_character( guard, true );
  guard.setpos( here, guard_local );
  static const faction_id faction_free_merchants( "free_merchants" );
  guard.set_fac( faction_free_merchants );
  static const activity_id ACT_CAMP_PATROL( "ACT_CAMP_PATROL" );
  REQUIRE( guard.job.set_task_priority( ACT_CAMP_PATROL, 9 ) );
  test_camp->add_assignee( guard.getID() );

  calendar::turn = sunrise(calendar::turn_zero) + 2_hours;
  guard.assess_danger();
  CHECK( guard.current_target() == nullptr );
  CHECK_FALSE( test_camp->is_patrol_alarm_active() );

  monster &zombie = spawn_test_monster( "mon_zombie", tripoint_bub_ms{52, 50, 0} );
  guard.assess_danger();
  CHECK( guard.current_target() == &zombie );
  CHECK( test_camp->is_patrol_alarm_active() );

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_worker_downtime_hides_routine_route_reports",
          "[camp][patrol]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  clear_creatures();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms worker_local{10, 10, 0};
  const tripoint_abs_ms patrol_abs = here.get_abs(tripoint_bub_ms{12, 10, 0});
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{13, 10, 0});
  create_tile_zone("Patrol Post", zone_type_CAMP_PATROL, patrol_abs);
  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(patrol_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);
  test_camp->set_bb_pos(patrol_abs);

  npc &worker = spawn_npc(worker_local.xy(), "thug");
  worker.set_fac( your_fac );
  static const activity_id ACT_CAMP_PATROL("ACT_CAMP_PATROL");
  REQUIRE(worker.job.set_task_priority(ACT_CAMP_PATROL, 9));
  test_camp->add_assignee(worker.getID());

  calendar::turn = sunrise(calendar::turn_zero) + 2_hours;
  Messages::clear_messages();
  worker.worker_downtime();
  CHECK(Messages::recent_messages(0).empty());

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_patrol_runtime_contract", "[camp][patrol]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();

  SECTION("one guard loops disconnected patrol posts in fixed order") {
    const std::vector<camp_patrol_worker> workers = {{character_id(801), 5}};
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here, {{10, 22, 0}}),
        make_patrol_cluster(here, {{12, 22, 0}}),
        make_patrol_cluster(here, {{14, 22, 0}}),
        make_patrol_cluster(here, {{16, 22, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    const std::optional<camp_patrol_guard_runtime> start_runtime =
        describe_camp_patrol_guard_runtime(plan.day, character_id(801),
                                           calendar::turn_zero);
    REQUIRE(start_runtime);
    CHECK(start_runtime->behavior == camp_patrol_guard_behavior::loop);
    CHECK(start_runtime->route ==
          std::vector<tripoint_abs_ms>({clusters[0][0], clusters[1][0],
                                        clusters[2][0], clusters[3][0]}));
    CHECK(start_runtime->target == clusters[0][0]);

    const std::optional<camp_patrol_guard_runtime> second_runtime =
        describe_camp_patrol_guard_runtime(plan.day, character_id(801),
                                           calendar::turn_zero +
                                               camp_patrol_loop_dwell());
    REQUIRE(second_runtime);
    CHECK(second_runtime->target == clusters[1][0]);
  }

  SECTION("one guard loops an understaffed connected cluster") {
    const std::vector<camp_patrol_worker> workers = {{character_id(811), 5}};
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{20, 22, 0}, {21, 22, 0}, {20, 23, 0}, {21, 23, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    const std::optional<camp_patrol_guard_runtime> start_runtime =
        describe_camp_patrol_guard_runtime(plan.day, character_id(811),
                                           calendar::turn_zero);
    REQUIRE(start_runtime);
    CHECK(start_runtime->behavior == camp_patrol_guard_behavior::loop);
    CHECK(start_runtime->route == clusters[0]);
    CHECK(start_runtime->target == clusters[0][0]);

    const std::optional<camp_patrol_guard_runtime> third_runtime =
        describe_camp_patrol_guard_runtime(plan.day, character_id(811),
                                           calendar::turn_zero +
                                               2 * camp_patrol_loop_dwell());
    REQUIRE(third_runtime);
    CHECK(third_runtime->target == clusters[0][2]);
  }

  SECTION("fully staffed connected cluster holds distinct squares") {
    std::vector<camp_patrol_worker> workers;
    for (int index = 0; index < 16; ++index) {
      workers.push_back(
          camp_patrol_worker{character_id(820 + index), 16 - index});
    }
    const std::vector<camp_patrol_cluster> clusters = {
        make_patrol_cluster(here,
                            {{30, 22, 0}, {31, 22, 0}, {30, 23, 0}, {31, 23, 0}}),
    };

    const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

    REQUIRE(plan.day.active_guards.size() == 4);
    REQUIRE(plan.day.reserve_guards.size() == 4);
    for (size_t index = 0; index < plan.day.active_guards.size(); ++index) {
      const character_id worker_id = plan.day.active_guards[index].worker_id;
      const std::optional<camp_patrol_guard_runtime> runtime =
          describe_camp_patrol_guard_runtime(plan.day, worker_id,
                                             calendar::turn_zero +
                                                 3 * camp_patrol_loop_dwell());
      REQUIRE(runtime);
      CHECK(runtime->behavior == camp_patrol_guard_behavior::hold);
      CHECK(runtime->route == std::vector<tripoint_abs_ms>({clusters[0][index]}));
      CHECK(runtime->target == clusters[0][index]);
    }

    REQUIRE(plan.night.active_guards.size() == 4);
    REQUIRE(plan.night.reserve_guards.size() == 4);
    for (size_t index = 0; index < plan.night.active_guards.size(); ++index) {
      const character_id worker_id = plan.night.active_guards[index].worker_id;
      const std::optional<camp_patrol_guard_runtime> runtime =
          describe_camp_patrol_guard_runtime(plan.night, worker_id,
                                             calendar::turn_zero +
                                                 3 * camp_patrol_loop_dwell());
      REQUIRE(runtime);
      CHECK(runtime->behavior == camp_patrol_guard_behavior::hold);
      CHECK(runtime->route == std::vector<tripoint_abs_ms>({clusters[0][index]}));
      CHECK(runtime->target == clusters[0][index]);
    }
  }
}

TEST_CASE("camp_locker_zone_candidate_gathering_respects_reservations",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{5, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_duffelbag));
  here.add_item_or_charges(locker_local, item(itype_daypack));

  const npc &reserving_worker =
      spawn_npc(tripoint_bub_ms{6, 5, 0}.xy(), "thug");
  const npc &requesting_worker =
      spawn_npc(tripoint_bub_ms{7, 5, 0}.xy(), "thug");

  const std::vector<camp_locker_reservation> reservations = {{
      reserving_worker.getID(),
      camp_locker_slot::bag,
      locker_abs,
      itype_duffelbag,
      calendar::turn + 10_minutes,
  }};

  const camp_locker_candidate_map other_worker_candidates =
      collect_camp_locker_zone_candidates(
          locker_abs, your_fac, camp_locker_policy(), reservations,
          requesting_worker.getID());
  REQUIRE(other_worker_candidates.count(camp_locker_slot::bag) == 1);
  REQUIRE(other_worker_candidates.at(camp_locker_slot::bag).size() == 1);
  CHECK(other_worker_candidates.at(camp_locker_slot::bag).front()->typeId() ==
        itype_daypack);

  const camp_locker_candidate_map reserving_worker_candidates =
      collect_camp_locker_zone_candidates(
          locker_abs, your_fac, camp_locker_policy(), reservations,
          reserving_worker.getID());
  REQUIRE(reserving_worker_candidates.count(camp_locker_slot::bag) == 1);
  REQUIRE(reserving_worker_candidates.at(camp_locker_slot::bag).size() == 2);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_loadout_planning", "[camp][locker]") {
  basecamp test_camp("Locker Camp", tripoint_abs_omt(8, 8, 0));

  SECTION("keeps the best duplicate and equips missing categories") {
    item jeans(itype_jeans);
    item cargo_pants(itype_pants_cargo);
    item backpack(itype_backpack);

    const std::vector<const item *> current_items = {&jeans, &cargo_pants};
    const std::vector<const item *> locker_items = {&backpack};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.duplicate_current.size() == 1);
    CHECK(pants_plan.duplicate_current.front()->typeId() == itype_jeans);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK(pants_plan.has_changes());

    REQUIRE(plan.count(camp_locker_slot::bag) == 1);
    const camp_locker_slot_plan &bag_plan = plan.at(camp_locker_slot::bag);
    CHECK(bag_plan.missing_current);
    REQUIRE(bag_plan.selected_candidate != nullptr);
    CHECK(bag_plan.selected_candidate->typeId() == itype_backpack);
    CHECK_FALSE(bag_plan.upgrade_selected);
    CHECK(bag_plan.has_changes());
  }

  SECTION("obvious upgrades beat weaker current gear") {
    item daypack(itype_daypack);
    item duffelbag(itype_duffelbag);
    item boots(itype_boots);
    item sneakers(itype_sneakers);

    const std::vector<const item *> current_items = {&daypack, &boots};
    const std::vector<const item *> locker_items = {&duffelbag, &sneakers};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::bag) == 1);
    const camp_locker_slot_plan &bag_plan = plan.at(camp_locker_slot::bag);
    REQUIRE(bag_plan.kept_current != nullptr);
    CHECK(bag_plan.kept_current->typeId() == itype_daypack);
    REQUIRE(bag_plan.selected_candidate != nullptr);
    CHECK(bag_plan.selected_candidate->typeId() == itype_duffelbag);
    CHECK(bag_plan.upgrade_selected);
    CHECK(bag_plan.has_changes());

    REQUIRE(plan.count(camp_locker_slot::shoes) == 1);
    const camp_locker_slot_plan &shoe_plan = plan.at(camp_locker_slot::shoes);
    REQUIRE(shoe_plan.kept_current != nullptr);
    CHECK(shoe_plan.kept_current->typeId() == itype_boots);
    CHECK(shoe_plan.selected_candidate == nullptr);
    CHECK_FALSE(shoe_plan.has_changes());
  }

  SECTION("disabled categories stay out of the plan") {
    test_camp.set_locker_slot_enabled(camp_locker_slot::bag, false);

    item daypack(itype_daypack);
    item backpack(itype_backpack);

    const std::vector<const item *> current_items = {&daypack};
    const std::vector<const item *> locker_items = {&backpack};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    CHECK(plan.count(camp_locker_slot::bag) == 0);
  }

  SECTION("cold weather prefers warmer outerwear upgrades") {
    item jacket_light(itype_jacket_light);
    item winter_coat(itype_coat_winter);

    REQUIRE(classify_camp_locker_item(jacket_light) == camp_locker_slot::vest);
    REQUIRE(classify_camp_locker_item(winter_coat) == camp_locker_slot::vest);

    const std::vector<const item *> current_items = {&jacket_light};
    const std::vector<const item *> locker_items = {&winter_coat};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(35));

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    REQUIRE(vest_plan.kept_current != nullptr);
    CHECK(vest_plan.kept_current->typeId() == itype_jacket_light);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_coat_winter);
    CHECK(vest_plan.upgrade_selected);
    CHECK(vest_plan.has_changes());
  }

  SECTION("hot weather prefers lighter outerwear upgrades") {
    item jacket_light(itype_jacket_light);
    item winter_coat(itype_coat_winter);

    const std::vector<const item *> current_items = {&winter_coat};
    const std::vector<const item *> locker_items = {&jacket_light};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    REQUIRE(vest_plan.kept_current != nullptr);
    CHECK(vest_plan.kept_current->typeId() == itype_coat_winter);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_jacket_light);
    CHECK(vest_plan.upgrade_selected);
    CHECK(vest_plan.has_changes());
  }

  SECTION("winter season still prefers warmer outerwear at moderate temperatures") {
    restore_on_out_of_scope restore_calendar_turn(calendar::turn);
    calendar::turn = calendar::turn_zero + 3 * calendar::season_length() + 1_days;

    item jacket_light(itype_jacket_light);
    item winter_coat(itype_coat_winter);

    const std::vector<const item *> current_items = {&jacket_light};
    const std::vector<const item *> locker_items = {&winter_coat};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(65));

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    REQUIRE(vest_plan.kept_current != nullptr);
    CHECK(vest_plan.kept_current->typeId() == itype_jacket_light);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_coat_winter);
    CHECK(vest_plan.upgrade_selected);
    CHECK(vest_plan.has_changes());
  }

  SECTION("summer season still prefers lighter outerwear at moderate temperatures") {
    restore_on_out_of_scope restore_calendar_turn(calendar::turn);
    calendar::turn = calendar::turn_zero + calendar::season_length() + 1_days;

    item jacket_light(itype_jacket_light);
    item winter_coat(itype_coat_winter);

    const std::vector<const item *> current_items = {&winter_coat};
    const std::vector<const item *> locker_items = {&jacket_light};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(65));

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    REQUIRE(vest_plan.kept_current != nullptr);
    CHECK(vest_plan.kept_current->typeId() == itype_coat_winter);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_jacket_light);
    CHECK(vest_plan.upgrade_selected);
    CHECK(vest_plan.has_changes());
  }

  SECTION("rainy weather prefers rainproof outerwear at moderate temperatures") {
    item rain_coat(itype_coat_rain);
    item winter_coat(itype_coat_winter);

    REQUIRE(classify_camp_locker_item(rain_coat) == camp_locker_slot::vest);
    REQUIRE(classify_camp_locker_item(winter_coat) == camp_locker_slot::vest);

    const std::vector<const item *> current_items = {&winter_coat};
    const std::vector<const item *> locker_items = {&rain_coat};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(65), true);

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    REQUIRE(vest_plan.kept_current != nullptr);
    CHECK(vest_plan.kept_current->typeId() == itype_coat_winter);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_coat_rain);
    CHECK(vest_plan.upgrade_selected);
    CHECK(vest_plan.has_changes());
  }

  SECTION("cold weather prefers full-length legwear upgrades") {
    item cargo_shorts(itype_shorts_cargo);
    item cargo_pants(itype_pants_cargo);

    REQUIRE(classify_camp_locker_item(cargo_shorts) ==
            camp_locker_slot::pants);
    REQUIRE(classify_camp_locker_item(cargo_pants) ==
            camp_locker_slot::pants);

    const std::vector<const item *> current_items = {&cargo_shorts};
    const std::vector<const item *> locker_items = {&cargo_pants};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(35));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_shorts_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_pants_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION("hot weather prefers short legwear upgrades") {
    item cargo_shorts(itype_shorts_cargo);
    item cargo_pants(itype_pants_cargo);

    const std::vector<const item *> current_items = {&cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION("winter season still prefers full-length legwear at moderate temperatures") {
    restore_on_out_of_scope restore_calendar_turn(calendar::turn);
    calendar::turn = calendar::turn_zero + 3 * calendar::season_length() + 1_days;

    item cargo_shorts(itype_shorts_cargo);
    item cargo_pants(itype_pants_cargo);

    const std::vector<const item *> current_items = {&cargo_shorts};
    const std::vector<const item *> locker_items = {&cargo_pants};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(65));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_shorts_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_pants_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION("summer season still prefers short legwear at moderate temperatures") {
    restore_on_out_of_scope restore_calendar_turn(calendar::turn);
    calendar::turn = calendar::turn_zero + calendar::season_length() + 1_days;

    item cargo_shorts(itype_shorts_cargo);
    item cargo_pants(itype_pants_cargo);

    const std::vector<const item *> current_items = {&cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(65));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION(
      "hot weather keeps the best current full-length legwear and drops extra "
      "duplicates") {
    item antarvasa( itype_id( "antarvasa" ) );
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);

    REQUIRE(classify_camp_locker_item(antarvasa) == camp_locker_slot::pants);

    const std::vector<const item *> current_items = {&antarvasa, &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.duplicate_current.size() == 1);
    CHECK(pants_plan.duplicate_current.front()->typeId() ==
          itype_id( "antarvasa" ));
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION("hot weather treats leggings as pants-slot duplicates") {
    item leggings(itype_leggings);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&leggings, &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.duplicate_current.size() == 1);
    CHECK(pants_plan.duplicate_current.front()->typeId() == itype_leggings);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION("hot weather keeps shorts over duplicate jeans") {
    item jeans(itype_jeans);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&jeans, &cargo_shorts};
    const camp_locker_candidate_map locker_candidates;

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_shorts_cargo);
    REQUIRE(pants_plan.duplicate_current.size() == 1);
    CHECK(pants_plan.duplicate_current.front()->typeId() == itype_jeans);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK(pants_plan.has_changes());
  }

  SECTION("better-condition equivalent bags replace damaged current bags") {
    item damaged_daypack(itype_daypack);
    damaged_daypack.set_damage(std::max(1, damaged_daypack.max_damage() / 2));
    item fresh_daypack(itype_daypack);

    const std::vector<const item *> current_items = {&damaged_daypack};
    const std::vector<const item *> locker_items = {&fresh_daypack};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::bag) == 1);
    const camp_locker_slot_plan &bag_plan = plan.at(camp_locker_slot::bag);
    CHECK(bag_plan.kept_current == &damaged_daypack);
    CHECK(bag_plan.selected_candidate == &fresh_daypack);
    CHECK(bag_plan.upgrade_selected);
    CHECK(bag_plan.has_changes());
  }

  SECTION("army helmets replace weaker caps") {
    item baseball_cap(itype_hat_ball);
    item army_helmet(itype_helmet_army);

    const std::vector<const item *> current_items = {&baseball_cap};
    const std::vector<const item *> locker_items = {&army_helmet};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::helmet) == 1);
    const camp_locker_slot_plan &helmet_plan =
        plan.at(camp_locker_slot::helmet);
    CHECK(helmet_plan.kept_current == &baseball_cap);
    CHECK(helmet_plan.selected_candidate == &army_helmet);
    CHECK(helmet_plan.upgrade_selected);
    CHECK(helmet_plan.has_changes());
  }

  SECTION(
      "ballistic helmets are not swapped out for melee-skewed great helms on flat score alone") {
    item army_helmet(itype_helmet_army);
    item great_helm(itype_helmet_plate);

    const std::vector<const item *> current_items = {&army_helmet};
    const std::vector<const item *> locker_items = {&great_helm};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::helmet) == 1);
    const camp_locker_slot_plan &helmet_plan =
        plan.at(camp_locker_slot::helmet);
    CHECK(helmet_plan.kept_current == &army_helmet);
    CHECK(helmet_plan.selected_candidate == nullptr);
    CHECK_FALSE(helmet_plan.upgrade_selected);
    CHECK_FALSE(helmet_plan.has_changes());
  }

  SECTION("ballistic helmets can still replace melee-skewed great helms") {
    item great_helm(itype_helmet_plate);
    item army_helmet(itype_helmet_army);

    const std::vector<const item *> current_items = {&great_helm};
    const std::vector<const item *> locker_items = {&army_helmet};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::helmet) == 1);
    const camp_locker_slot_plan &helmet_plan =
        plan.at(camp_locker_slot::helmet);
    CHECK(helmet_plan.kept_current == &great_helm);
    CHECK(helmet_plan.selected_candidate == &army_helmet);
    CHECK(helmet_plan.upgrade_selected);
    CHECK(helmet_plan.has_changes());
  }

  SECTION(
      "ballistic body armor is not swapped out for melee-skewed plate on flat score alone") {
    item ballistic_vest(itype_ballistic_vest_esapi);
    item plate_armor(itype_armor_lc_plate);

    const std::vector<const item *> current_items = {&ballistic_vest};
    const std::vector<const item *> locker_items = {&plate_armor};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::body_armor) == 1);
    const camp_locker_slot_plan &armor_plan =
        plan.at(camp_locker_slot::body_armor);
    CHECK(armor_plan.kept_current == &ballistic_vest);
    CHECK(armor_plan.selected_candidate == nullptr);
    CHECK_FALSE(armor_plan.upgrade_selected);
    CHECK_FALSE(armor_plan.has_changes());
  }

  SECTION("ballistic body armor can still replace melee-skewed plate") {
    item plate_armor(itype_armor_lc_plate);
    item ballistic_vest(itype_ballistic_vest_esapi);

    const std::vector<const item *> current_items = {&plate_armor};
    const std::vector<const item *> locker_items = {&ballistic_vest};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::body_armor) == 1);
    const camp_locker_slot_plan &armor_plan =
        plan.at(camp_locker_slot::body_armor);
    CHECK(armor_plan.kept_current == &plate_armor);
    CHECK(armor_plan.selected_candidate == &ballistic_vest);
    CHECK(armor_plan.upgrade_selected);
    CHECK(armor_plan.has_changes());
  }

  SECTION("bulletproof preference boosts ballistic armor and helmet scoring") {
    item plate_armor(itype_armor_lc_plate);
    item ballistic_vest(itype_ballistic_vest_esapi);
    item great_helm(itype_helmet_plate);
    item army_helmet(itype_helmet_army);

    REQUIRE(ballistic_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());
    REQUIRE(ballistic_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());

    camp_locker_policy bulletproof_policy = test_camp.get_locker_policy();
    bulletproof_policy.set_prefers_bulletproof( true );

    const int default_body_gap =
        score_camp_locker_item(camp_locker_slot::body_armor,
                               ballistic_vest, test_camp.get_locker_policy()) -
        score_camp_locker_item(camp_locker_slot::body_armor, plate_armor,
                               test_camp.get_locker_policy());
    const int bulletproof_body_gap =
        score_camp_locker_item(camp_locker_slot::body_armor, ballistic_vest,
                               bulletproof_policy) -
        score_camp_locker_item(camp_locker_slot::body_armor, plate_armor,
                               bulletproof_policy);
    CHECK(bulletproof_body_gap > default_body_gap);

    const int default_helmet_gap =
        score_camp_locker_item(camp_locker_slot::helmet, army_helmet,
                               test_camp.get_locker_policy()) -
        score_camp_locker_item(camp_locker_slot::helmet, great_helm,
                               test_camp.get_locker_policy());
    const int bulletproof_helmet_gap =
        score_camp_locker_item(camp_locker_slot::helmet, army_helmet,
                               bulletproof_policy) -
        score_camp_locker_item(camp_locker_slot::helmet, great_helm,
                               bulletproof_policy);
    CHECK(bulletproof_helmet_gap > default_helmet_gap);
  }

  SECTION("worker fit context uses item encumbrance scoring") {
    clear_avatar();
    Character &worker = get_player_character();
    item loose_jeans(itype_jeans);
    item fitted_jeans(itype_jeans);
    fitted_jeans.set_flag(flag_FIT);

    CHECK(score_camp_locker_item(camp_locker_slot::pants, fitted_jeans,
                                 test_camp.get_locker_policy(), std::nullopt,
                                 false, &worker) >
          score_camp_locker_item(camp_locker_slot::pants, loose_jeans,
                                 test_camp.get_locker_policy(), std::nullopt,
                                 false, &worker));
  }

  SECTION("ballistic vest scoring notices loaded plates and damaged inserts") {
    item empty_vest(itype_ballistic_vest_esapi);
    item loaded_vest(itype_ballistic_vest_esapi);
    item damaged_vest(itype_ballistic_vest_esapi);

    REQUIRE(loaded_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());
    REQUIRE(loaded_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());

    item damaged_front_plate(itype_esapi_plate);
    damaged_front_plate.set_damage(damaged_front_plate.max_damage() / 2);
    item damaged_back_plate(itype_esapi_plate);
    damaged_back_plate.set_damage(damaged_back_plate.max_damage() / 2);
    REQUIRE(damaged_vest.put_in(damaged_front_plate, pocket_type::CONTAINER)
                .success());
    REQUIRE(damaged_vest.put_in(damaged_back_plate, pocket_type::CONTAINER)
                .success());

    CHECK(score_camp_locker_item(camp_locker_slot::body_armor, loaded_vest,
                                 test_camp.get_locker_policy()) >
          score_camp_locker_item(camp_locker_slot::body_armor, empty_vest,
                                 test_camp.get_locker_policy()));
    CHECK(score_camp_locker_item(camp_locker_slot::body_armor, loaded_vest,
                                 test_camp.get_locker_policy()) >
          score_camp_locker_item(camp_locker_slot::body_armor, damaged_vest,
                                 test_camp.get_locker_policy()));
  }

  SECTION("same ballistic vest type upgrades when locker plates are healthier") {
    item current_vest(itype_ballistic_vest_esapi);
    item locker_vest(itype_ballistic_vest_esapi);

    item damaged_front_plate(itype_esapi_plate);
    damaged_front_plate.set_damage(damaged_front_plate.max_damage() / 2);
    item damaged_back_plate(itype_esapi_plate);
    damaged_back_plate.set_damage(damaged_back_plate.max_damage() / 2);
    REQUIRE(current_vest.put_in(damaged_front_plate, pocket_type::CONTAINER)
                .success());
    REQUIRE(current_vest.put_in(damaged_back_plate, pocket_type::CONTAINER)
                .success());

    REQUIRE(locker_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());
    REQUIRE(locker_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());

    const std::vector<const item *> current_items = {&current_vest};
    const std::vector<const item *> locker_items = {&locker_vest};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::body_armor) == 1);
    const camp_locker_slot_plan &armor_plan =
        plan.at(camp_locker_slot::body_armor);
    CHECK(armor_plan.kept_current == &current_vest);
    CHECK(armor_plan.selected_candidate == &locker_vest);
    CHECK(armor_plan.upgrade_selected);
    CHECK(armor_plan.has_changes());
  }

  SECTION("full-body body armor does not invite filler pants underneath it") {
    item plate_armor(itype_armor_lc_plate);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&plate_armor};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::body_armor) == 1);
    const camp_locker_slot_plan &armor_plan =
        plan.at(camp_locker_slot::body_armor);
    CHECK(armor_plan.kept_current == &plate_armor);
    CHECK_FALSE(armor_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::pants) == 0);
  }

  SECTION("one-piece jumpsuits satisfy the pants slot instead of shoes") {
    item cotton_jumpsuit(itype_test_jumpsuit_cotton);
    const std::vector<const item *> current_items = {&cotton_jumpsuit};
    const camp_locker_candidate_map locker_candidates;
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cotton_jumpsuit);
    CHECK_FALSE(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::shoes) == 0);
  }

  SECTION("jumpsuit-like outer suits satisfy the pants slot instead of vest") {
    item suit(itype_suit);
    const std::vector<const item *> current_items = {&suit};
    const camp_locker_candidate_map locker_candidates;
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &suit);
    CHECK_FALSE(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::vest) == 0);
  }

  SECTION(
      "indirect jumpsuit-like outer suits satisfy the pants slot instead of vest") {
    item tux(itype_tux);
    const std::vector<const item *> current_items = {&tux};
    const camp_locker_candidate_map locker_candidates;
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &tux);
    CHECK_FALSE(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::vest) == 0);
  }

  SECTION(
      "skintight full-body suits satisfy the pants slot instead of underwear") {
    item wetsuit(itype_wetsuit);
    const std::vector<const item *> current_items = {&wetsuit};
    const camp_locker_candidate_map locker_candidates;
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &wetsuit);
    CHECK_FALSE(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::underwear) == 0);
  }

  SECTION(
      "draped-only overgarments stay out of pants-slot duplicate cleanup") {
    item hakama(itype_hakama);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&hakama, &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cargo_pants);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.duplicate_current.empty());
  }

  SECTION(
      "draped waist aprons stay out of pants-slot duplicate cleanup") {
    item waist_apron_long(itype_waist_apron_long);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&waist_apron_long,
                                                     &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cargo_pants);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.duplicate_current.empty());
    CHECK(plan.count(camp_locker_slot::shirt) == 0);
    CHECK(plan.count(camp_locker_slot::vest) == 0);
  }

  SECTION(
      "lower-body upgrades do not strip torso coverage from one-piece suits") {
    item suit(itype_suit);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&suit};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &suit);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK_FALSE(pants_plan.has_changes());
  }

  SECTION(
      "full-body head-covering suits stay in the pants lane for shorts-only upgrades") {
    item hazmat_suit(itype_hazmat_suit);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&hazmat_suit};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &hazmat_suit);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK_FALSE(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::helmet) == 0);
  }

  SECTION(
      "one-piece suits can split into shirt and pants when both replacements exist") {
    item suit(itype_suit);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&suit};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &suit);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
    CHECK(shirt_plan.missing_current);
    CHECK(shirt_plan.has_changes());
  }

  SECTION(
      "footed jumpsuits keep their built-in footwear unless replacements exist") {
    item cotton_jumpsuit(itype_test_jumpsuit_cotton);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&cotton_jumpsuit};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cotton_jumpsuit);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
  }

  SECTION(
      "footed jumpsuits can split when shirt and shoes replacements exist") {
    item cotton_jumpsuit(itype_test_jumpsuit_cotton);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);
    item hiking_boots(itype_boots);

    const std::vector<const item *> current_items = {&cotton_jumpsuit};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt,
                                                    &hiking_boots};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cotton_jumpsuit);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);

    REQUIRE(plan.count(camp_locker_slot::shoes) == 1);
    const camp_locker_slot_plan &shoes_plan = plan.at(camp_locker_slot::shoes);
    CHECK(shoes_plan.kept_current == nullptr);
    REQUIRE(shoes_plan.selected_candidate != nullptr);
    CHECK(shoes_plan.selected_candidate->typeId() == itype_boots);
  }

  SECTION(
      "footed lower-body gear keeps built-in torso and footwear unless replacements exist") {
    item fishing_waders(itype_fishing_waders);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&fishing_waders};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &fishing_waders);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK_FALSE(pants_plan.has_changes());
  }

  SECTION(
      "footed lower-body gear can split when torso and shoes replacements exist") {
    item fishing_waders(itype_fishing_waders);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);
    item hiking_boots(itype_boots);

    const std::vector<const item *> current_items = {&fishing_waders};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt,
                                                    &hiking_boots};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &fishing_waders);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);

    REQUIRE(plan.count(camp_locker_slot::shoes) == 1);
    const camp_locker_slot_plan &shoes_plan = plan.at(camp_locker_slot::shoes);
    CHECK(shoes_plan.kept_current == nullptr);
    REQUIRE(shoes_plan.selected_candidate != nullptr);
    CHECK(shoes_plan.selected_candidate->typeId() == itype_boots);
  }

  SECTION(
      "head-covering full-body suits do not split unless the extra coverage is preserved") {
    item hazmat_suit(itype_hazmat_suit);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&hazmat_suit};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &hazmat_suit);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
  }

  SECTION(
      "armored full-body utility suits do not split into ordinary hot-weather clothes") {
    item survivor_suit(itype_survivor_suit);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);
    const std::vector<const item *> current_items = {&survivor_suit};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};

    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &survivor_suit);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK_FALSE(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::shirt) == 0);
  }

  SECTION(
      "armored full-body utility suits suppress missing shirt filler when chosen from the locker") {
    item survivor_suit(itype_survivor_suit);
    item tshirt(itype_tshirt);
    const std::vector<const item *> current_items;
    const std::vector<const item *> locker_items = {&survivor_suit, &tshirt};

    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == nullptr);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_survivor_suit);
    CHECK(pants_plan.missing_current);
    CHECK(pants_plan.has_changes());
    CHECK(plan.count(camp_locker_slot::shirt) == 0);
  }

  SECTION(
      "armored full-body utility suits can displace weaker current shirts") {
    item survivor_suit(itype_survivor_suit);
    item pants_cargo(itype_pants_cargo);
    item tshirt(itype_tshirt);
    const std::vector<const item *> current_items = {&pants_cargo, &tshirt};
    const std::vector<const item *> locker_items = {&survivor_suit};

    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_survivor_suit);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    CHECK(shirt_plan.selected_candidate == nullptr);
    REQUIRE(shirt_plan.duplicate_current.size() == 1);
    CHECK(shirt_plan.duplicate_current.front() == &tshirt);
    CHECK(shirt_plan.has_changes());
  }

  SECTION(
      "armored full-body utility suits can displace weaker current vests") {
    item survivor_suit(itype_survivor_suit);
    item pants_cargo(itype_pants_cargo);
    item vest(itype_vest);
    const std::vector<const item *> current_items = {&pants_cargo, &vest};
    const std::vector<const item *> locker_items = {&survivor_suit};

    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_survivor_suit);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    CHECK(vest_plan.kept_current == nullptr);
    CHECK(vest_plan.selected_candidate == nullptr);
    REQUIRE(vest_plan.duplicate_current.size() == 1);
    CHECK(vest_plan.duplicate_current.front() == &vest);
    CHECK(vest_plan.has_changes());
  }

  SECTION(
      "armored full-body utility suits can displace weaker current body armor") {
    item survivor_suit(itype_survivor_suit);
    item pants_cargo(itype_pants_cargo);
    item crude_plastic_vest(itype_crude_plastic_vest);
    const std::vector<const item *> current_items = {&pants_cargo,
                                                     &crude_plastic_vest};
    const std::vector<const item *> locker_items = {&survivor_suit};

    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_survivor_suit);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::body_armor) == 1);
    const camp_locker_slot_plan &armor_plan =
        plan.at(camp_locker_slot::body_armor);
    CHECK(armor_plan.kept_current == nullptr);
    CHECK(armor_plan.selected_candidate == nullptr);
    REQUIRE(armor_plan.duplicate_current.size() == 1);
    CHECK(armor_plan.duplicate_current.front() == &crude_plastic_vest);
    CHECK(armor_plan.has_changes());
  }

  SECTION(
      "armored full-body utility suits do not displace stronger current ballistic body armor") {
    item survivor_suit(itype_survivor_suit);
    item pants_cargo(itype_pants_cargo);
    item ballistic_vest(itype_ballistic_vest_esapi);
    REQUIRE(ballistic_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());
    REQUIRE(ballistic_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
                .success());
    const std::vector<const item *> current_items = {&pants_cargo,
                                                     &ballistic_vest};
    const std::vector<const item *> locker_items = {&survivor_suit};

    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_survivor_suit);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::body_armor) == 1);
    const camp_locker_slot_plan &armor_plan =
        plan.at(camp_locker_slot::body_armor);
    CHECK(armor_plan.kept_current == &ballistic_vest);
    CHECK(armor_plan.selected_candidate == nullptr);
    CHECK(armor_plan.duplicate_current.empty());
    CHECK_FALSE(armor_plan.has_changes());
  }

  SECTION(
      "lower-body upgrades do not strip torso coverage from short dresses") {
    item short_dress(itype_short_dress);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&short_dress};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &short_dress);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK_FALSE(pants_plan.has_changes());
  }

  SECTION(
      "short dresses can split into shirt and pants when both replacements exist") {
    item short_dress(itype_short_dress);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&short_dress};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &short_dress);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
    CHECK(shirt_plan.missing_current);
    CHECK(shirt_plan.has_changes());
  }

  SECTION(
      "full-length dresses do not strip torso coverage for shorts-only upgrades") {
    item long_dress(itype_long_dress);
    item cargo_shorts(itype_shorts_cargo);

    const std::vector<const item *> current_items = {&long_dress};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &long_dress);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);
    CHECK_FALSE(pants_plan.has_changes());
  }

  SECTION(
      "full-length dresses can split into shirt and pants when both replacements exist") {
    item long_dress(itype_long_dress);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&long_dress};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());

    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &long_dress);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
    CHECK(shirt_plan.missing_current);
    CHECK(shirt_plan.has_changes());
  }

  SECTION(
      "sleeved dresses do not split when the only upper-body replacement drops arm coverage") {
    item long_dress_sleeved(itype_long_dress_sleeved);
    item cargo_shorts(itype_shorts_cargo);
    item vest(itype_vest);
    const std::vector<const item *> current_items = {&long_dress_sleeved};
    const std::vector<const item *> locker_items = {&cargo_shorts, &vest};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy());

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &long_dress_sleeved);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    CHECK(vest_plan.kept_current == nullptr);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_vest);
  }

  SECTION(
      "sleeved dresses can split when an arm-covering shirt replacement exists") {
    item long_dress_sleeved(itype_long_dress_sleeved);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);
    const std::vector<const item *> current_items = {&long_dress_sleeved};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &long_dress_sleeved);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
    CHECK(shirt_plan.missing_current);
    CHECK(shirt_plan.has_changes());
  }

  SECTION(
      "cheongsams do not split when the only upper-body replacement drops shoulder coverage") {
    item cheongsam(itype_cheongsam);
    item cargo_shorts(itype_shorts_cargo);
    item vest(itype_vest);

    const std::vector<const item *> current_items = {&cheongsam};
    const std::vector<const item *> locker_items = {&cargo_shorts, &vest};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items, camp_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, camp_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cheongsam);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    CHECK(vest_plan.kept_current == nullptr);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_vest);
  }

  SECTION(
      "cheongsams can split when an arm-covering shirt replacement exists") {
    item cheongsam(itype_cheongsam);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&cheongsam};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items, camp_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, camp_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &cheongsam);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
    CHECK(shirt_plan.missing_current);
    CHECK(shirt_plan.has_changes());
  }

  SECTION(
      "outer one-piece garments do not split when the only upper-body replacement drops arm coverage") {
    item abaya(itype_abaya);
    item cargo_shorts(itype_shorts_cargo);
    item vest(itype_vest);

    const std::vector<const item *> current_items = {&abaya};
    const std::vector<const item *> locker_items = {&cargo_shorts, &vest};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items, camp_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, camp_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &abaya);
    CHECK(pants_plan.selected_candidate == nullptr);
    CHECK_FALSE(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::vest) == 1);
    const camp_locker_slot_plan &vest_plan = plan.at(camp_locker_slot::vest);
    CHECK(vest_plan.kept_current == nullptr);
    REQUIRE(vest_plan.selected_candidate != nullptr);
    CHECK(vest_plan.selected_candidate->typeId() == itype_vest);
  }

  SECTION(
      "outer one-piece garments can split when an arm-covering shirt replacement exists") {
    item abaya(itype_abaya);
    item cargo_shorts(itype_shorts_cargo);
    item tshirt(itype_tshirt);

    const std::vector<const item *> current_items = {&abaya};
    const std::vector<const item *> locker_items = {&cargo_shorts, &tshirt};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items, camp_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, camp_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    CHECK(pants_plan.kept_current == &abaya);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);

    REQUIRE(plan.count(camp_locker_slot::shirt) == 1);
    const camp_locker_slot_plan &shirt_plan = plan.at(camp_locker_slot::shirt);
    CHECK(shirt_plan.kept_current == nullptr);
    REQUIRE(shirt_plan.selected_candidate != nullptr);
    CHECK(shirt_plan.selected_candidate->typeId() == itype_tshirt);
    CHECK(shirt_plan.missing_current);
    CHECK(shirt_plan.has_changes());
  }

  SECTION("leg holsters stay out of pants-slot duplicate cleanup") {
    item holster(itype_holster);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&holster, &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.duplicate_current.empty());
  }

  SECTION("knee pads stay out of pants-slot duplicate cleanup") {
    item knee_pads(itype_knee_pads);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&knee_pads, &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.duplicate_current.empty());
  }

  SECTION("full-leg hard guards stay out of pants-slot duplicate cleanup") {
    item hard_legguards(itype_legguard_hard);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&hard_legguards,
                                                     &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.duplicate_current.empty());
  }

  SECTION(
      "full-leg outer greaves without hip coverage stay out of pants-slot duplicate cleanup") {
    item brigandine_greaves(itype_legguard_lc_brigandine);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&brigandine_greaves,
                                                     &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.duplicate_current.empty());
  }

  SECTION("hip-only armored skirts stay out of pants-slot duplicate cleanup") {
    item armored_skirt(itype_legguard_metal_sheets_hip);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {&armored_skirt,
                                                     &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.upgrade_selected);
    CHECK(pants_plan.duplicate_current.empty());
  }

  SECTION("partial leg support storage stays out of underwear and pants cleanup") {
    item deep_holster(itype_bholster);
    item leg_bag(itype_leg_small_bag);
    item cargo_pants(itype_pants_cargo);
    item cargo_shorts(itype_shorts_cargo);
    const std::vector<const item *> current_items = {
        &deep_holster, &leg_bag, &cargo_pants};
    const std::vector<const item *> locker_items = {&cargo_shorts};
    const camp_locker_candidate_map locker_candidates =
        collect_camp_locker_candidates(locker_items,
                                       test_camp.get_locker_policy());
    const camp_locker_plan plan = plan_camp_locker_loadout(
        current_items, locker_candidates, test_camp.get_locker_policy(),
        units::from_fahrenheit(85));

    REQUIRE(plan.count(camp_locker_slot::pants) == 1);
    const camp_locker_slot_plan &pants_plan = plan.at(camp_locker_slot::pants);
    REQUIRE(pants_plan.kept_current != nullptr);
    CHECK(pants_plan.kept_current->typeId() == itype_pants_cargo);
    REQUIRE(pants_plan.selected_candidate != nullptr);
    CHECK(pants_plan.selected_candidate->typeId() == itype_shorts_cargo);
    CHECK(pants_plan.duplicate_current.empty());
    CHECK(plan.count(camp_locker_slot::underwear) == 0);
  }

}

TEST_CASE("camp_locker_weapon_scoring_uses_character_weapon_value",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();

  npc &worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(worker, true);

  const camp_locker_policy policy;
  const item combat_knife(itype_knife_combat);
  const item glock(itype_glock_19);

  CHECK(score_camp_locker_item(camp_locker_slot::melee_weapon, combat_knife,
                               policy, std::nullopt, false, &worker) ==
        static_cast<int>(worker.evaluate_weapon(combat_knife, true) * 100.0));
  CHECK(score_camp_locker_item(camp_locker_slot::ranged_weapon, glock, policy,
                               std::nullopt, false, &worker) ==
        static_cast<int>(worker.evaluate_weapon(glock, true) * 100.0));
}

TEST_CASE("camp_locker_service_equips_upgrades_and_returns_replaced_gear",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_duffelbag));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_duffelbag));
  CHECK_FALSE(worker.is_wearing(itype_daypack));

  bool locker_has_daypack = false;
  bool locker_has_duffelbag = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_daypack = locker_has_daypack || it.typeId() == itype_daypack;
    locker_has_duffelbag =
        locker_has_duffelbag || it.typeId() == itype_duffelbag;
  }
  CHECK(locker_has_daypack);
  CHECK_FALSE(locker_has_duffelbag);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_filters_unwearable_armor_candidates",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_esapi_plate));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  test_camp->add_assignee(worker.getID());

  const camp_locker_service_probe probe =
      test_camp->measure_camp_locker_service(worker);
  CHECK_FALSE(probe.applied_changes);
  CHECK(probe.locker_item_count == 1);
  CHECK(probe.candidate_item_count == 0);
  CHECK(probe.changed_slot_count == 0);
  CHECK(count_character_items(worker, itype_esapi_plate) == 0);

  bool locker_has_plate = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_plate = locker_has_plate || it.typeId() == itype_esapi_plate;
  }
  CHECK(locker_has_plate);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_filters_unwieldable_weapon_candidates",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_bio_blade_weapon));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  test_camp->add_assignee(worker.getID());

  const camp_locker_service_probe probe =
      test_camp->measure_camp_locker_service(worker);
  CHECK_FALSE(probe.applied_changes);
  CHECK(probe.locker_item_count == 1);
  CHECK(probe.candidate_item_count == 0);
  CHECK(probe.changed_slot_count == 0);
  CHECK(count_character_items(worker, itype_bio_blade_weapon) == 0);

  bool locker_has_bio_blade = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_bio_blade = locker_has_bio_blade ||
                            it.typeId() == itype_bio_blade_weapon;
  }
  CHECK(locker_has_bio_blade);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_prefers_winter_outerwear_in_moderate_winter_weather",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  calendar::turn = calendar::turn_zero + 3 * calendar::season_length() + 1_days;

  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(65));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_coat_winter));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_jacket_light), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_coat_winter));
  CHECK_FALSE(worker.is_wearing(itype_jacket_light));

  bool locker_has_winter_coat = false;
  bool locker_has_jacket_light = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_winter_coat = locker_has_winter_coat ||
                             it.typeId() == itype_coat_winter;
    locker_has_jacket_light = locker_has_jacket_light ||
                              it.typeId() == itype_jacket_light;
  }
  CHECK_FALSE(locker_has_winter_coat);
  CHECK(locker_has_jacket_light);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_prefers_rainproof_outerwear_in_rainy_weather",
          "[camp][locker]") {
  scoped_weather_override rainy_weather( weather_drizzle );

  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(65));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_coat_rain));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_coat_winter), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_coat_rain));
  CHECK_FALSE(worker.is_wearing(itype_coat_winter));

  bool locker_has_rain_coat = false;
  bool locker_has_winter_coat = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_rain_coat = locker_has_rain_coat ||
                           it.typeId() == itype_coat_rain;
    locker_has_winter_coat = locker_has_winter_coat ||
                             it.typeId() == itype_coat_winter;
  }
  CHECK_FALSE(locker_has_rain_coat);
  CHECK(locker_has_winter_coat);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_prefers_summer_legwear_in_moderate_summer_weather",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  calendar::turn = calendar::turn_zero + calendar::season_length() + 1_days;

  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(65));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));

  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_equips_common_combat_support_slots",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_gloves_leather));
  here.add_item_or_charges(locker_local, item(itype_mask_dust));
  here.add_item_or_charges(locker_local, item(itype_tool_belt));
  here.add_item_or_charges(locker_local, item(itype_holster));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_gloves_leather));
  CHECK(worker.is_wearing(itype_mask_dust));
  CHECK(worker.is_wearing(itype_tool_belt));
  CHECK(worker.is_wearing(itype_holster));

  bool locker_has_gloves = false;
  bool locker_has_mask = false;
  bool locker_has_belt = false;
  bool locker_has_holster = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_gloves = locker_has_gloves || it.typeId() == itype_gloves_leather;
    locker_has_mask = locker_has_mask || it.typeId() == itype_mask_dust;
    locker_has_belt = locker_has_belt || it.typeId() == itype_tool_belt;
    locker_has_holster = locker_has_holster || it.typeId() == itype_holster;
  }
  CHECK_FALSE(locker_has_gloves);
  CHECK_FALSE(locker_has_mask);
  CHECK_FALSE(locker_has_belt);
  CHECK_FALSE(locker_has_holster);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_swaps_in_better_condition_equivalent_bags",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_daypack));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  item damaged_daypack(itype_daypack);
  damaged_daypack.set_damage(std::max(1, damaged_daypack.max_damage() / 2));
  REQUIRE(worker.wear_item(damaged_daypack, false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_daypack));

  item *worn_daypack = worker.item_worn_with_id(itype_daypack);
  REQUIRE(worn_daypack != nullptr);
  CHECK(worn_daypack->damage() == 0);

  int damaged_daypacks_in_locker = 0;
  int fresh_daypacks_in_locker = 0;
  for (const item &it : here.i_at(locker_local)) {
    if (it.typeId() != itype_daypack) {
      continue;
    }
    if (it.damage() == 0) {
      fresh_daypacks_in_locker++;
    } else {
      damaged_daypacks_in_locker++;
    }
  }
  CHECK(damaged_daypacks_in_locker == 1);
  CHECK(fresh_daypacks_in_locker == 0);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_swaps_caps_for_army_helmets",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_helmet_army));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_hat_ball), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_helmet_army));
  CHECK_FALSE(worker.is_wearing(itype_hat_ball));

  bool locker_has_baseball_cap = false;
  bool locker_has_army_helmet = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_baseball_cap = locker_has_baseball_cap ||
                              it.typeId() == itype_hat_ball;
    locker_has_army_helmet = locker_has_army_helmet ||
                             it.typeId() == itype_helmet_army;
  }
  CHECK(locker_has_baseball_cap);
  CHECK_FALSE(locker_has_army_helmet);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_keeps_ballistic_helmets_over_melee_skewed_great_helms",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_helmet_plate));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_helmet_army), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_helmet_army));
  CHECK_FALSE(worker.is_wearing(itype_helmet_plate));

  bool locker_has_army_helmet = false;
  bool locker_has_great_helm = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_army_helmet = locker_has_army_helmet ||
                             it.typeId() == itype_helmet_army;
    locker_has_great_helm = locker_has_great_helm ||
                            it.typeId() == itype_helmet_plate;
  }
  CHECK_FALSE(locker_has_army_helmet);
  CHECK(locker_has_great_helm);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_swaps_great_helms_for_ballistic_army_helmets",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_helmet_army));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_helmet_plate), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_helmet_army));
  CHECK_FALSE(worker.is_wearing(itype_helmet_plate));

  bool locker_has_army_helmet = false;
  bool locker_has_great_helm = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_army_helmet = locker_has_army_helmet ||
                             it.typeId() == itype_helmet_army;
    locker_has_great_helm = locker_has_great_helm ||
                            it.typeId() == itype_helmet_plate;
  }
  CHECK_FALSE(locker_has_army_helmet);
  CHECK(locker_has_great_helm);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_ballistic_body_armor_over_melee_skewed_plate",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_armor_lc_plate));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(
      worker.wear_item(item(itype_ballistic_vest_esapi), false).has_value());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_ballistic_vest_esapi));
  CHECK_FALSE(worker.is_wearing(itype_armor_lc_plate));

  bool locker_has_ballistic_vest = false;
  bool locker_has_plate_armor = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_ballistic_vest = locker_has_ballistic_vest ||
                                it.typeId() == itype_ballistic_vest_esapi;
    locker_has_plate_armor = locker_has_plate_armor ||
                             it.typeId() == itype_armor_lc_plate;
  }
  CHECK_FALSE(locker_has_ballistic_vest);
  CHECK(locker_has_plate_armor);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_upgrades_plate_to_ballistic_body_armor",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_ballistic_vest_esapi));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_armor_lc_plate), false).has_value());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_ballistic_vest_esapi));
  CHECK_FALSE(worker.is_wearing(itype_armor_lc_plate));

  bool locker_has_ballistic_vest = false;
  bool locker_has_plate_armor = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_ballistic_vest = locker_has_ballistic_vest ||
                                it.typeId() == itype_ballistic_vest_esapi;
    locker_has_plate_armor = locker_has_plate_armor ||
                             it.typeId() == itype_armor_lc_plate;
  }
  CHECK_FALSE(locker_has_ballistic_vest);
  CHECK(locker_has_plate_armor);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_equips_full_body_candidate_after_clearing_weaker_blockers",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_survivor_suit));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  REQUIRE(worker.wear_item(item(itype_tshirt), false).has_value());
  REQUIRE(worker.wear_item(item(itype_crude_plastic_vest), false).has_value());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_survivor_suit));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK_FALSE(worker.is_wearing(itype_tshirt));
  CHECK_FALSE(worker.is_wearing(itype_crude_plastic_vest));

  bool locker_has_survivor_suit = false;
  bool locker_has_pants = false;
  bool locker_has_tshirt = false;
  bool locker_has_vest = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_survivor_suit = locker_has_survivor_suit ||
                               it.typeId() == itype_survivor_suit;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_vest = locker_has_vest ||
                      it.typeId() == itype_crude_plastic_vest;
  }
  CHECK_FALSE(locker_has_survivor_suit);
  CHECK(locker_has_pants);
  CHECK(locker_has_tshirt);
  CHECK(locker_has_vest);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_preserves_ballistic_armor_while_adding_full_body_candidate",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_survivor_suit));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  item ballistic_vest(itype_ballistic_vest_esapi);
  REQUIRE(ballistic_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
              .success());
  REQUIRE(ballistic_vest.put_in(item(itype_esapi_plate), pocket_type::CONTAINER)
              .success());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  REQUIRE(worker.wear_item(ballistic_vest, false).has_value());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK(worker.is_wearing(itype_ballistic_vest_esapi));
  CHECK(worker.is_wearing(itype_survivor_suit));
  CHECK(count_character_items(worker, itype_esapi_plate) == 2);

  bool locker_has_survivor_suit = false;
  bool locker_has_pants = false;
  bool locker_has_ballistic_vest = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_survivor_suit = locker_has_survivor_suit ||
                               it.typeId() == itype_survivor_suit;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
    locker_has_ballistic_vest = locker_has_ballistic_vest ||
                                it.typeId() == itype_ballistic_vest_esapi;
  }
  CHECK_FALSE(locker_has_survivor_suit);
  CHECK(locker_has_pants);
  CHECK_FALSE(locker_has_ballistic_vest);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_rejects_damaged_full_body_candidate",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  item damaged_survivor_suit(itype_survivor_suit);
  damaged_survivor_suit.set_damage(damaged_survivor_suit.max_damage() / 2);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, damaged_survivor_suit);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_survivor_suit), false).has_value());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_survivor_suit));

  bool locker_has_damaged_survivor_suit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_damaged_survivor_suit = locker_has_damaged_survivor_suit ||
                                       it.typeId() == itype_survivor_suit;
  }
  CHECK(locker_has_damaged_survivor_suit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_does_not_add_filler_shorts_under_full-body_plate_armor",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_armor_lc_plate), false).has_value());

  set_map_temperature(units::from_fahrenheit(85));

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_armor_lc_plate));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
  }
  CHECK(locker_has_shorts);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_clears_conflicting_legwear_before_hot_weather_upgrade",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_id("antarvasa")), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK_FALSE(worker.is_wearing(itype_id("antarvasa")));

  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  bool locker_has_antarvasa = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants ||
                       it.typeId() == itype_pants_cargo;
    locker_has_antarvasa = locker_has_antarvasa ||
                           it.typeId() == itype_id("antarvasa");
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);
  CHECK(locker_has_antarvasa);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_drops_duplicate_jeans_when_shorts_are_already_best",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_jeans), false).has_value());
  REQUIRE(worker.wear_item(item(itype_shorts_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_jeans));

  bool locker_has_jeans = false;
  bool locker_has_shorts = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_jeans = locker_has_jeans || it.typeId() == itype_jeans;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
  }
  CHECK(locker_has_jeans);
  CHECK_FALSE(locker_has_shorts);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_clears_full-leg_underlayers_before_hot_weather_upgrade",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_leggings), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK_FALSE(worker.is_wearing(itype_leggings));

  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  bool locker_has_leggings = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants ||
                       it.typeId() == itype_pants_cargo;
    locker_has_leggings = locker_has_leggings ||
                          it.typeId() == itype_leggings;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);
  CHECK(locker_has_leggings);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_leg_holsters_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_holster), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK(worker.is_wearing(itype_holster));

  bool locker_has_holster = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_holster = locker_has_holster || it.typeId() == itype_holster;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_holster);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_knee_pads_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_knee_pads), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK(worker.is_wearing(itype_knee_pads));

  bool locker_has_knee_pads = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_knee_pads = locker_has_knee_pads || it.typeId() == itype_knee_pads;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_knee_pads);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_full-leg_hard_guards_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "test_talker");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  REQUIRE(worker.wear_item(item(itype_legguard_hard), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK(worker.is_wearing(itype_legguard_hard));

  bool locker_has_legguards = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_legguards = locker_has_legguards ||
                           it.typeId() == itype_legguard_hard;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_legguards);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_full-leg_outer_greaves_without_hip_coverage_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "test_talker");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_legguard_lc_brigandine), false)
              .has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_legguard_lc_brigandine));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));

  bool locker_has_legguards = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_legguards =
        locker_has_legguards || it.typeId() == itype_legguard_lc_brigandine;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_legguards);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_hip_only_armored_skirts_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  test_camp->add_assignee(worker.getID());

  REQUIRE(worker.wear_item(item(itype_legguard_metal_sheets_hip), false)
              .has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());

  REQUIRE(test_camp->service_camp_locker(worker));

  CHECK(worker.is_wearing(itype_legguard_metal_sheets_hip));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));

  bool locker_has_armored_skirt = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_armored_skirt =
        locker_has_armored_skirt || it.typeId() == itype_legguard_metal_sheets_hip;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_armored_skirt);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_draped_hakama_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_hakama), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_hakama));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));

  bool locker_has_hakama = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_hakama = locker_has_hakama || it.typeId() == itype_hakama;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_hakama);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_draped_waist_aprons_while_upgrading_actual_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_waist_apron_long), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_waist_apron_long));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));

  bool locker_has_apron = false;
  bool locker_has_shorts = false;
  bool locker_has_pants = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_apron = locker_has_apron || it.typeId() == itype_waist_apron_long;
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
  }
  CHECK_FALSE(locker_has_apron);
  CHECK_FALSE(locker_has_shorts);
  CHECK(locker_has_pants);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_one-piece_suits_when_pants_upgrade_would_strip_torso",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_suit), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_suit));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_suit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_suit = locker_has_suit || it.typeId() == itype_suit;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_suit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_head-covering_full-body_suits_when_shorts_upgrade_would_split_them",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_hazmat_suit), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_hazmat_suit));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_hazmat_suit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_hazmat_suit = locker_has_hazmat_suit ||
                             it.typeId() == itype_hazmat_suit;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_hazmat_suit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_armored_full-body_utility_suits_without_filling_missing_shirts_from_junk",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_survivor_suit), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_survivor_suit));
  CHECK_FALSE(worker.is_wearing(itype_tshirt));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_survivor_suit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_survivor_suit = locker_has_survivor_suit ||
                               it.typeId() == itype_survivor_suit;
  }
  CHECK(locker_has_shorts);
  CHECK(locker_has_tshirt);
  CHECK_FALSE(locker_has_survivor_suit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_uses_armored_full-body_utility_suits_over_weaker_current_shirts",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(65));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_survivor_suit));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  REQUIRE(worker.wear_item(item(itype_tshirt), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_survivor_suit));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK_FALSE(worker.is_wearing(itype_tshirt));

  bool locker_has_survivor_suit = false;
  bool locker_has_pants = false;
  bool locker_has_tshirt = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_survivor_suit = locker_has_survivor_suit ||
                               it.typeId() == itype_survivor_suit;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
  }
  CHECK_FALSE(locker_has_survivor_suit);
  CHECK(locker_has_pants);
  CHECK(locker_has_tshirt);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_uses_armored_full-body_utility_suits_over_weaker_current_vests",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(65));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_survivor_suit));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  REQUIRE(worker.wear_item(item(itype_vest), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_survivor_suit));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK_FALSE(worker.is_wearing(itype_vest));

  bool locker_has_survivor_suit = false;
  bool locker_has_pants = false;
  bool locker_has_vest = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_survivor_suit = locker_has_survivor_suit ||
                               it.typeId() == itype_survivor_suit;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
    locker_has_vest = locker_has_vest || it.typeId() == itype_vest;
  }
  CHECK_FALSE(locker_has_survivor_suit);
  CHECK(locker_has_pants);
  CHECK(locker_has_vest);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_uses_armored_full-body_utility_suits_over_weaker_current_body_armor",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(65));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_survivor_suit));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  REQUIRE(
      worker.wear_item(item(itype_crude_plastic_vest), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_survivor_suit));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));
  CHECK_FALSE(worker.is_wearing(itype_crude_plastic_vest));

  bool locker_has_survivor_suit = false;
  bool locker_has_pants = false;
  bool locker_has_crude_plastic_vest = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_survivor_suit = locker_has_survivor_suit ||
                               it.typeId() == itype_survivor_suit;
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
    locker_has_crude_plastic_vest =
        locker_has_crude_plastic_vest ||
        it.typeId() == itype_crude_plastic_vest;
  }
  CHECK_FALSE(locker_has_survivor_suit);
  CHECK(locker_has_pants);
  CHECK(locker_has_crude_plastic_vest);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_indirect_one-piece_suits_when_pants_upgrade_would_strip_torso",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_tux), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_tux));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_tux = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_tux = locker_has_tux || it.typeId() == itype_tux;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_tux);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_does_not_strip_short_dresses_for_shorts_only_upgrades",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_short_dress), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_short_dress));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_short_dress = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_short_dress = locker_has_short_dress ||
                             it.typeId() == itype_short_dress;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_short_dress);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_short_dresses_when_torso_replacement_exists",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_short_dress), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_short_dress));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_short_dress = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_short_dress = locker_has_short_dress ||
                             it.typeId() == itype_short_dress;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK(locker_has_short_dress);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_does_not_strip_full-length_dresses_for_shorts_only_upgrades",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_long_dress), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_long_dress));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_long_dress = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_long_dress = locker_has_long_dress ||
                            it.typeId() == itype_long_dress;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_long_dress);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_full-length_dresses_when_torso_replacement_exists",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_long_dress), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_long_dress));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_long_dress = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_long_dress = locker_has_long_dress ||
                            it.typeId() == itype_long_dress;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK(locker_has_long_dress);

  zone_manager::get_manager().clear();
}


TEST_CASE(
    "camp_locker_service_keeps_sleeved_dresses_when_torso-only_replacements_would_strip_arms",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_vest));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_long_dress_sleeved), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_long_dress_sleeved));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_sleeved_dress = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_sleeved_dress =
        locker_has_sleeved_dress || it.typeId() == itype_long_dress_sleeved;
  }

  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_sleeved_dress);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_sleeved_dresses_when_arm_covering_shirt_exists",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_long_dress_sleeved), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_long_dress_sleeved));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_sleeved_dress = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_sleeved_dress =
        locker_has_sleeved_dress || it.typeId() == itype_long_dress_sleeved;
  }

  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK(locker_has_sleeved_dress);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_cheongsams_when_torso-only_replacements_would_strip_shoulders",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_vest));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_cheongsam), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_cheongsam));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_cheongsam = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_cheongsam = locker_has_cheongsam || it.typeId() == itype_cheongsam;
  }

  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_cheongsam);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_cheongsams_when_arm_covering_shirt_exists",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_cheongsam), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_cheongsam));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_cheongsam = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_cheongsam = locker_has_cheongsam || it.typeId() == itype_cheongsam;
  }

  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK(locker_has_cheongsam);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_outer_one-piece_garments_when_torso-only_replacements_would_strip_arms",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_vest));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_abaya), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_abaya));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_abaya = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_abaya = locker_has_abaya || it.typeId() == itype_abaya;
  }

  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_abaya);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_outer_one-piece_garments_when_arm_covering_shirt_exists",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_abaya), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_abaya));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_abaya = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_abaya = locker_has_abaya || it.typeId() == itype_abaya;
  }

  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK(locker_has_abaya);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_one-piece_suits_when_torso_replacement_exists",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_suit), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_suit));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_suit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_suit = locker_has_suit || it.typeId() == itype_suit;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK(locker_has_suit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_footed_jumpsuits_when_split_would_strip_built-in_footwear",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_test_jumpsuit_cotton), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_test_jumpsuit_cotton));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_boots));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_jumpsuit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_jumpsuit =
        locker_has_jumpsuit || it.typeId() == itype_test_jumpsuit_cotton;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK_FALSE(locker_has_jumpsuit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_footed_jumpsuits_when_shirt_and_shoes_exist",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));
  here.add_item_or_charges(locker_local, item(itype_boots));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_test_jumpsuit_cotton), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_test_jumpsuit_cotton));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));
  CHECK(worker.is_wearing(itype_boots));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_boots = false;
  bool locker_has_jumpsuit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_boots = locker_has_boots || it.typeId() == itype_boots;
    locker_has_jumpsuit =
        locker_has_jumpsuit || it.typeId() == itype_test_jumpsuit_cotton;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK_FALSE(locker_has_boots);
  CHECK(locker_has_jumpsuit);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_footed_lower-body_gear_when_split_would_strip_torso_or_footwear",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_fishing_waders), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_fishing_waders));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_boots));

  bool locker_has_shorts = false;
  bool locker_has_waders = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_waders = locker_has_waders || it.typeId() == itype_fishing_waders;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_waders);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_can_split_footed_lower-body_gear_when_torso_and_shoes_exist",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));
  here.add_item_or_charges(locker_local, item(itype_tshirt));
  here.add_item_or_charges(locker_local, item(itype_boots));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_fishing_waders), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK_FALSE(worker.is_wearing(itype_fishing_waders));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK(worker.is_wearing(itype_tshirt));
  CHECK(worker.is_wearing(itype_boots));

  bool locker_has_shorts = false;
  bool locker_has_tshirt = false;
  bool locker_has_boots = false;
  bool locker_has_waders = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts || it.typeId() == itype_shorts_cargo;
    locker_has_tshirt = locker_has_tshirt || it.typeId() == itype_tshirt;
    locker_has_boots = locker_has_boots || it.typeId() == itype_boots;
    locker_has_waders = locker_has_waders || it.typeId() == itype_fishing_waders;
  }
  CHECK_FALSE(locker_has_shorts);
  CHECK_FALSE(locker_has_tshirt);
  CHECK_FALSE(locker_has_boots);
  CHECK(locker_has_waders);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_partial_leg_support_storage_when_hot-weather_cleanup_swaps_pants",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_leg_small_bag), false).has_value());
  REQUIRE(worker.wear_item(item(itype_pants_cargo), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_leg_small_bag));
  CHECK(worker.is_wearing(itype_shorts_cargo));
  CHECK_FALSE(worker.is_wearing(itype_pants_cargo));

  bool locker_has_pants = false;
  bool locker_has_leg_bag = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_pants = locker_has_pants || it.typeId() == itype_pants_cargo;
    locker_has_leg_bag = locker_has_leg_bag || it.typeId() == itype_leg_small_bag;
  }
  CHECK(locker_has_pants);
  CHECK_FALSE(locker_has_leg_bag);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_keeps_skintight_one-piece_suits_when_pants_upgrade_would_strip_torso",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  set_map_temperature(units::from_fahrenheit(85));

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_shorts_cargo));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(bcp.has_value());
  basecamp *test_camp = *bcp;
  REQUIRE(test_camp != nullptr);
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_wetsuit), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_wetsuit));
  CHECK_FALSE(worker.is_wearing(itype_shorts_cargo));

  bool locker_has_shorts = false;
  bool locker_has_wetsuit = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_shorts = locker_has_shorts ||
                        it.typeId() == itype_shorts_cargo;
    locker_has_wetsuit = locker_has_wetsuit || it.typeId() == itype_wetsuit;
  }
  CHECK(locker_has_shorts);
  CHECK_FALSE(locker_has_wetsuit);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_probe_scales_with_top_level_clutter",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  auto run_probe = [](int clutter_items) {
    clear_map_without_vision();
    zone_manager::get_manager().clear();

    map &here = get_map();
    const tripoint_bub_ms npc_local{5, 5, 0};
    const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
    const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

    create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
    here.i_clear(locker_local);
    here.add_item_or_charges(locker_local, item(itype_duffelbag));
    for (int i = 0; i < clutter_items; ++i) {
      here.add_item_or_charges(locker_local, item(itype_aspirin));
    }

    const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
    here.add_camp(camp_omt, "faction_camp");
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
    REQUIRE(!!bcp);
    basecamp *test_camp = *bcp;
    test_camp->set_owner(your_fac);

    npc &worker = spawn_npc(npc_local.xy(), "thug");
    clear_character(worker, true);
    REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
    test_camp->add_assignee(worker.getID());

    const camp_locker_service_probe probe =
        test_camp->measure_camp_locker_service(worker);
    CHECK(probe.applied_changes);
    CHECK(worker.is_wearing(itype_duffelbag));
    CHECK_FALSE(worker.is_wearing(itype_daypack));
    CHECK(probe.locker_item_count == clutter_items + 1);
    CHECK(probe.metrics.candidate_item_checks >= clutter_items + 1);
    return probe;
  };

  const camp_locker_service_probe small_probe = run_probe(50);
  const camp_locker_service_probe hundred_probe = run_probe(100);
  const camp_locker_service_probe medium_probe = run_probe(200);
  const camp_locker_service_probe five_hundred_probe = run_probe(500);
  const camp_locker_service_probe large_probe = run_probe(1000);

  CHECK(small_probe.metrics.zone_top_level_items_seen == 2 * 51);
  CHECK(hundred_probe.metrics.zone_top_level_items_seen == 2 * 101);
  CHECK(medium_probe.metrics.zone_top_level_items_seen == 2 * 201);
  CHECK(five_hundred_probe.metrics.zone_top_level_items_seen == 2 * 501);
  CHECK(large_probe.metrics.zone_top_level_items_seen == 2 * 1001);
  CHECK(small_probe.metrics.candidate_item_checks == 2 * 51);
  CHECK(hundred_probe.metrics.candidate_item_checks == 2 * 101);
  CHECK(medium_probe.metrics.candidate_item_checks == 2 * 201);
  CHECK(five_hundred_probe.metrics.candidate_item_checks == 2 * 501);
  CHECK(large_probe.metrics.candidate_item_checks == 2 * 1001);
  CHECK(render_camp_locker_service_probe(large_probe).find("top_level=2002") !=
        std::string::npos);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_probe_scales_linearly_with_worker_count",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  auto run_probe = [](int worker_count) {
    clear_map_without_vision();
    zone_manager::get_manager().clear();

    map &here = get_map();
    const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
    const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

    create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
    here.i_clear(locker_local);
    here.add_item_or_charges(locker_local, item(itype_daypack));
    for (int i = 0; i < 100; ++i) {
      here.add_item_or_charges(locker_local, item(itype_aspirin));
    }

    const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
    here.add_camp(camp_omt, "faction_camp");
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
    REQUIRE(!!bcp);
    basecamp *test_camp = *bcp;
    test_camp->set_owner(your_fac);

    int total_top_level_scans = 0;
    int total_candidate_checks = 0;
    for (int i = 0; i < worker_count; ++i) {
      npc &worker = spawn_npc(tripoint_bub_ms{5, 5 + i, 0}.xy(), "thug");
      clear_character(worker, true);
      REQUIRE(worker.wear_item(item(itype_duffelbag), false).has_value());
      test_camp->add_assignee(worker.getID());
      const camp_locker_service_probe probe =
          test_camp->measure_camp_locker_service(worker);
      CHECK_FALSE(probe.applied_changes);
      CHECK(probe.locker_item_count == 101);
      total_top_level_scans += probe.metrics.zone_top_level_items_seen;
      total_candidate_checks += probe.metrics.candidate_item_checks;
    }
    return std::pair<int, int>(total_top_level_scans, total_candidate_checks);
  };

  const auto one_worker = run_probe(1);
  const auto five_workers = run_probe(5);
  const auto ten_workers = run_probe(10);

  CHECK(one_worker.first == 2 * 101);
  CHECK(five_workers.first == 5 * one_worker.first);
  CHECK(ten_workers.first == 10 * one_worker.first);
  CHECK(one_worker.second == 2 * 101);
  CHECK(five_workers.second == 5 * one_worker.second);
  CHECK(ten_workers.second == 10 * one_worker.second);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_probe_threshold_packet_for_top_level_clutter",
          "[.][camp][locker][threshold]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  auto build_fixture = [](int clutter_items) {
    clear_map_without_vision();
    zone_manager::get_manager().clear();

    map &here = get_map();
    constexpr int per_tile_clutter_cap = 1000;
    const int locker_tile_count = std::max(1, (clutter_items + per_tile_clutter_cap - 1) /
                                                  per_tile_clutter_cap);
    const int locker_width = std::min(5, locker_tile_count);
    const int locker_height = (locker_tile_count + locker_width - 1) / locker_width;
    const tripoint_bub_ms zone_start_local{6, 5, 0};
    const tripoint_abs_ms zone_start_abs = here.get_abs(zone_start_local);
    const tripoint_abs_ms zone_end_abs =
        here.get_abs(tripoint_bub_ms{6 + locker_width - 1, 5 + locker_height - 1, 0});

    create_rect_zone("Locker", zone_type_CAMP_LOCKER, zone_start_abs, zone_end_abs);

    std::vector<tripoint_abs_ms> locker_tiles;
    locker_tiles.reserve(locker_tile_count);
    for (int dy = 0; dy < locker_height; ++dy) {
      for (int dx = 0; dx < locker_width; ++dx) {
        if (static_cast<int>(locker_tiles.size()) >= locker_tile_count) {
          break;
        }
        const tripoint_abs_ms tile_abs =
            here.get_abs(tripoint_bub_ms{6 + dx, 5 + dy, 0});
        locker_tiles.push_back(tile_abs);
        here.i_clear(here.get_bub(tile_abs));
      }
    }

    here.add_item_or_charges(here.get_bub(locker_tiles.front()), item(itype_duffelbag));
    int remaining_clutter = clutter_items;
    for (const tripoint_abs_ms &tile_abs : locker_tiles) {
      const int items_here = std::min(per_tile_clutter_cap, remaining_clutter);
      for (int i = 0; i < items_here; ++i) {
        here.add_item_or_charges(here.get_bub(tile_abs), item(itype_aspirin));
      }
      remaining_clutter -= items_here;
    }

    const int expected_top_level_items = clutter_items + 1;
    CHECK(count_top_level_items(here, locker_tiles) == expected_top_level_items);

    const tripoint_abs_omt camp_omt = project_to<coords::omt>(zone_start_abs);
    here.add_camp(camp_omt, "faction_camp");
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
    REQUIRE(!!bcp);
    basecamp *test_camp = *bcp;
    test_camp->set_owner(your_fac);

    npc &worker = spawn_npc(tripoint_bub_ms{4, 4, 0}.xy(), "thug");
    clear_character(worker, true);
    REQUIRE(worker.wear_item(item(itype_duffelbag), false).has_value());
    test_camp->add_assignee(worker.getID());

    return std::tuple<basecamp *, npc *, int>(test_camp, &worker,
                                              expected_top_level_items);
  };

  for (const int clutter_items : {1000, 2000, 5000, 10000, 20000}) {
    const auto fixture = build_fixture(clutter_items);
    basecamp &test_camp = *std::get<0>(fixture);
    npc &worker = *std::get<1>(fixture);
    const int expected_top_level_items = std::get<2>(fixture);

    const camp_locker_service_probe baseline_probe =
        test_camp.measure_camp_locker_service(worker);
    const camp_locker_service_probe repeat_probe =
        test_camp.measure_camp_locker_service(worker);

    CHECK_FALSE(baseline_probe.applied_changes);
    CHECK_FALSE(repeat_probe.applied_changes);
    CHECK(baseline_probe.locker_item_count == expected_top_level_items);
    CHECK(baseline_probe.metrics.zone_top_level_items_seen ==
          2 * expected_top_level_items);
    CHECK(baseline_probe.metrics.candidate_item_checks ==
          2 * expected_top_level_items);
    CHECK(render_camp_locker_service_probe(repeat_probe) ==
          render_camp_locker_service_probe(baseline_probe));

    const int repeats = clutter_items >= 20000 ? 3 : clutter_items >= 10000 ? 5 :
                        clutter_items >= 5000 ? 8 : 12;
    const locker_threshold_timing_sample timing =
        measure_locker_threshold_timing(
            [&]() { return test_camp.measure_camp_locker_service(worker); },
            repeats);
    std::cout << "locker-threshold top_level clutter=" << clutter_items
              << " workers=1 median_us=" << timing.median_us
              << " p95_us=" << timing.p95_us << ' '
              << render_camp_locker_service_probe(timing.probe) << '\n';
  }

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_probe_threshold_packet_for_worker_count",
          "[.][camp][locker][threshold]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  auto build_fixture = [](int clutter_items, int worker_count) {
    clear_map_without_vision();
    zone_manager::get_manager().clear();

    map &here = get_map();
    constexpr int per_tile_clutter_cap = 1000;
    const int locker_tile_count = std::max(1, (clutter_items + per_tile_clutter_cap - 1) /
                                                  per_tile_clutter_cap);
    const int locker_width = std::min(5, locker_tile_count);
    const int locker_height = (locker_tile_count + locker_width - 1) / locker_width;
    const tripoint_bub_ms zone_start_local{6, 5, 0};
    const tripoint_abs_ms zone_start_abs = here.get_abs(zone_start_local);
    const tripoint_abs_ms zone_end_abs =
        here.get_abs(tripoint_bub_ms{6 + locker_width - 1, 5 + locker_height - 1, 0});

    create_rect_zone("Locker", zone_type_CAMP_LOCKER, zone_start_abs, zone_end_abs);

    std::vector<tripoint_abs_ms> locker_tiles;
    locker_tiles.reserve(locker_tile_count);
    for (int dy = 0; dy < locker_height; ++dy) {
      for (int dx = 0; dx < locker_width; ++dx) {
        if (static_cast<int>(locker_tiles.size()) >= locker_tile_count) {
          break;
        }
        const tripoint_abs_ms tile_abs =
            here.get_abs(tripoint_bub_ms{6 + dx, 5 + dy, 0});
        locker_tiles.push_back(tile_abs);
        here.i_clear(here.get_bub(tile_abs));
      }
    }

    here.add_item_or_charges(here.get_bub(locker_tiles.front()), item(itype_duffelbag));
    int remaining_clutter = clutter_items;
    for (const tripoint_abs_ms &tile_abs : locker_tiles) {
      const int items_here = std::min(per_tile_clutter_cap, remaining_clutter);
      for (int i = 0; i < items_here; ++i) {
        here.add_item_or_charges(here.get_bub(tile_abs), item(itype_aspirin));
      }
      remaining_clutter -= items_here;
    }

    const int expected_top_level_items = clutter_items + 1;
    CHECK(count_top_level_items(here, locker_tiles) == expected_top_level_items);

    const tripoint_abs_omt camp_omt = project_to<coords::omt>(zone_start_abs);
    here.add_camp(camp_omt, "faction_camp");
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
    REQUIRE(!!bcp);
    basecamp *test_camp = *bcp;
    test_camp->set_owner(your_fac);

    std::vector<npc *> workers;
    workers.reserve(worker_count);
    for (int i = 0; i < worker_count; ++i) {
      npc &worker = spawn_npc(tripoint_bub_ms{4, 4 + i, 0}.xy(), "thug");
      clear_character(worker, true);
      REQUIRE(worker.wear_item(item(itype_duffelbag), false).has_value());
      test_camp->add_assignee(worker.getID());
      workers.push_back(&worker);
    }

    return std::tuple<basecamp *, std::vector<npc *>, int>(test_camp,
                                                            std::move(workers),
                                                            expected_top_level_items);
  };

  constexpr int clutter_items = 5000;
  for (const int worker_count : {1, 5, 10}) {
    const auto fixture = build_fixture(clutter_items, worker_count);
    basecamp &test_camp = *std::get<0>(fixture);
    const std::vector<npc *> &workers = std::get<1>(fixture);
    const int expected_top_level_items = std::get<2>(fixture);

    auto run_all_workers = [&]() {
      camp_locker_service_probe last_probe;
      for (npc *worker : workers) {
        last_probe = test_camp.measure_camp_locker_service(*worker);
        CHECK_FALSE(last_probe.applied_changes);
        CHECK(last_probe.locker_item_count == expected_top_level_items);
        CHECK(last_probe.metrics.zone_top_level_items_seen ==
              2 * expected_top_level_items);
        CHECK(last_probe.metrics.candidate_item_checks ==
              2 * expected_top_level_items);
      }
      return last_probe;
    };

    const camp_locker_service_probe baseline_probe = run_all_workers();
    const camp_locker_service_probe repeat_probe = run_all_workers();
    CHECK(render_camp_locker_service_probe(repeat_probe) ==
          render_camp_locker_service_probe(baseline_probe));

    const locker_threshold_timing_sample timing =
        measure_locker_threshold_timing(run_all_workers,
                                        worker_count == 10 ? 4 : 6);
    std::cout << "locker-threshold workers clutter=" << clutter_items
              << " workers=" << worker_count
              << " median_total_us=" << timing.median_us
              << " median_per_worker_us=" << (timing.median_us / worker_count)
              << " p95_total_us=" << timing.p95_us << ' '
              << render_camp_locker_service_probe(timing.probe) << '\n';
  }

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_probe_treats_nested_magazines_and_bags_as_top_level_items",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  auto run_probe = [](const std::vector<item> &extra_items) {
    clear_map_without_vision();
    zone_manager::get_manager().clear();

    map &here = get_map();
    const tripoint_bub_ms npc_local{5, 5, 0};
    const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
    const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

    create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
    here.i_clear(locker_local);
    here.add_item_or_charges(locker_local, item(itype_duffelbag));
    for (const item &extra : extra_items) {
      here.add_item_or_charges(locker_local, extra);
    }

    const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
    here.add_camp(camp_omt, "faction_camp");
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
    REQUIRE(!!bcp);
    basecamp *test_camp = *bcp;
    test_camp->set_owner(your_fac);

    npc &worker = spawn_npc(npc_local.xy(), "thug");
    clear_character(worker, true);
    REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
    test_camp->add_assignee(worker.getID());

    return test_camp->measure_camp_locker_service(worker);
  };

  std::vector<item> junk_items(25, item(itype_aspirin));
  std::vector<item> empty_magazines(25, item(itype_glockmag));
  std::vector<item> loaded_magazines;
  loaded_magazines.reserve(25);
  for (int i = 0; i < 25; ++i) {
    loaded_magazines.emplace_back(make_loaded_glock_magazine());
  }
  std::vector<item> empty_daypacks(25, item(itype_daypack));
  std::vector<item> filled_daypacks;
  filled_daypacks.reserve(25);
  for (int i = 0; i < 25; ++i) {
    filled_daypacks.emplace_back(make_filled_daypack());
  }

  const camp_locker_service_probe junk_probe = run_probe(junk_items);
  const camp_locker_service_probe empty_mag_probe = run_probe(empty_magazines);
  const camp_locker_service_probe loaded_mag_probe = run_probe(loaded_magazines);
  const camp_locker_service_probe empty_bag_probe = run_probe(empty_daypacks);
  const camp_locker_service_probe filled_bag_probe = run_probe(filled_daypacks);

  CHECK(junk_probe.metrics.zone_top_level_items_seen ==
        empty_mag_probe.metrics.zone_top_level_items_seen);
  CHECK(junk_probe.metrics.zone_top_level_items_seen ==
        empty_bag_probe.metrics.zone_top_level_items_seen);
  CHECK(empty_mag_probe.metrics.zone_top_level_items_seen ==
        loaded_mag_probe.metrics.zone_top_level_items_seen);
  CHECK(empty_bag_probe.metrics.zone_top_level_items_seen ==
        filled_bag_probe.metrics.zone_top_level_items_seen);

  CHECK(junk_probe.metrics.candidate_item_checks ==
        empty_mag_probe.metrics.candidate_item_checks);
  CHECK(junk_probe.metrics.candidate_item_checks ==
        empty_bag_probe.metrics.candidate_item_checks);
  CHECK(empty_mag_probe.metrics.candidate_item_checks ==
        loaded_mag_probe.metrics.candidate_item_checks);
  CHECK(empty_bag_probe.metrics.candidate_item_checks ==
        filled_bag_probe.metrics.candidate_item_checks);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_probe_distinguishes_junk_candidate_and_ranged_stock_shapes",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  auto run_probe = [](const std::vector<item> &locker_items) {
    clear_map_without_vision();
    zone_manager::get_manager().clear();

    map &here = get_map();
    const tripoint_bub_ms npc_local{5, 5, 0};
    const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
    const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

    create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
    here.i_clear(locker_local);
    for (const item &locker_item : locker_items) {
      here.add_item_or_charges(locker_local, locker_item);
    }

    const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
    here.add_camp(camp_omt, "faction_camp");
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
    REQUIRE(!!bcp);
    basecamp *test_camp = *bcp;
    test_camp->set_owner(your_fac);

    npc &worker = spawn_npc(npc_local.xy(), "thug");
    clear_character(worker, true);
    REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
    item empty_glock(itype_glock_19);
    REQUIRE(worker.wield(empty_glock));
    test_camp->add_assignee(worker.getID());

    const camp_locker_service_probe probe =
        test_camp->measure_camp_locker_service(worker);
    CHECK(probe.applied_changes);
    CHECK(worker.is_wearing(itype_duffelbag));
    return probe;
  };

  std::vector<item> junk_items;
  junk_items.reserve(25);
  junk_items.emplace_back(itype_duffelbag);
  for (int i = 0; i < 24; ++i) {
    junk_items.emplace_back(itype_aspirin);
  }

  std::vector<item> candidate_items(25, item(itype_duffelbag));

  std::vector<item> ranged_items;
  ranged_items.reserve(25);
  ranged_items.emplace_back(itype_duffelbag);
  ranged_items.emplace_back(item(itype_9mm, calendar::turn_zero,
                                 item(itype_glockmag).remaining_ammo_capacity() * 2));
  for (int i = 0; i < 11; ++i) {
    ranged_items.emplace_back(itype_daypack);
  }
  for (int i = 0; i < 12; ++i) {
    ranged_items.emplace_back(itype_glockmag);
  }

  const camp_locker_service_probe junk_probe = run_probe(junk_items);
  const camp_locker_service_probe candidate_probe = run_probe(candidate_items);
  const camp_locker_service_probe ranged_probe = run_probe(ranged_items);

  CHECK(junk_probe.metrics.candidate_items_accepted <
        candidate_probe.metrics.candidate_items_accepted);
  CHECK(junk_probe.metrics.candidate_items_accepted <
        ranged_probe.metrics.candidate_items_accepted);
  CHECK(ranged_probe.metrics.candidate_items_accepted <
        candidate_probe.metrics.candidate_items_accepted);

  CHECK(junk_probe.magazines_to_take == 0);
  CHECK(candidate_probe.magazines_to_take == 0);
  CHECK(ranged_probe.magazines_to_take == 2);
  CHECK(ranged_probe.magazines_to_reload == 2);
  CHECK(junk_probe.metrics.compatible_ammo_item_checks == 0);
  CHECK(candidate_probe.metrics.compatible_ammo_item_checks == 0);
  CHECK(ranged_probe.metrics.compatible_ammo_item_checks > 0);
  CHECK(render_camp_locker_service_probe(ranged_probe).find("ammo_checks=") !=
        std::string::npos);

  zone_manager::get_manager().clear();
}

TEST_CASE(
    "camp_locker_service_dumps_carried_junk_outside_curated_locker_stock",
    "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms storage_abs = here.get_abs(tripoint_bub_ms{4, 5, 0});
  const tripoint_bub_ms storage_local = here.get_bub(storage_abs);
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Storage", zone_type_CAMP_STORAGE, storage_abs);
  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(storage_local);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_duffelbag));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
  REQUIRE(worker.i_add(item(itype_bandages)));
  REQUIRE(worker.i_add(item(itype_glockmag)));
  REQUIRE(worker.i_add(item(itype_9mm, calendar::turn_zero, 10)));
  REQUIRE(worker.i_add(item(itype_esapi_plate)));
  REQUIRE(worker.i_add(item(itype_attachable_ear_muffs)));
  REQUIRE(worker.i_add(item(itype_helmet_army)));
  REQUIRE(worker.i_add(item(itype_knife_combat)));
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));
  CHECK(worker.is_wearing(itype_duffelbag));
  CHECK_FALSE(worker.is_wearing(itype_daypack));

  bool worker_has_bandages = false;
  bool worker_has_magazine = false;
  bool worker_has_ammo = false;
  bool worker_has_knife = false;
  bool worker_has_plate = false;
  bool worker_has_ear_muffs = false;
  bool worker_has_helmet = false;
  worker.visit_items([&](item *node, item *) {
    if (node == nullptr) {
      return VisitResponse::NEXT;
    }
    worker_has_bandages = worker_has_bandages || node->typeId() == itype_bandages;
    worker_has_magazine = worker_has_magazine || node->typeId() == itype_glockmag;
    worker_has_ammo = worker_has_ammo || node->typeId() == itype_9mm;
    worker_has_plate = worker_has_plate || node->typeId() == itype_esapi_plate;
    worker_has_ear_muffs = worker_has_ear_muffs ||
                            node->typeId() == itype_attachable_ear_muffs;
    worker_has_helmet = worker_has_helmet || node->typeId() == itype_helmet_army;
    worker_has_knife = worker_has_knife || node->typeId() == itype_knife_combat;
    return VisitResponse::NEXT;
  });
  CHECK(worker_has_bandages);
  CHECK(worker_has_magazine);
  CHECK(worker_has_ammo);
  CHECK(worker_has_plate);
  CHECK(worker_has_ear_muffs);
  CHECK_FALSE(worker_has_helmet);
  CHECK_FALSE(worker_has_knife);

  bool locker_has_daypack = false;
  bool locker_has_knife = false;
  bool locker_has_bandages = false;
  bool locker_recursive_has_bandages = false;
  bool locker_recursive_has_magazine = false;
  bool locker_recursive_has_ammo = false;
  bool locker_recursive_has_knife = false;
  for (const item &it : here.i_at(locker_local)) {
    locker_has_daypack = locker_has_daypack || it.typeId() == itype_daypack;
    locker_has_knife = locker_has_knife || it.typeId() == itype_knife_combat;
    locker_has_bandages = locker_has_bandages || it.typeId() == itype_bandages;
    for (const item *nested : it.all_items_top()) {
      locker_recursive_has_bandages = locker_recursive_has_bandages || nested->typeId() == itype_bandages;
      locker_recursive_has_magazine = locker_recursive_has_magazine || nested->typeId() == itype_glockmag;
      locker_recursive_has_ammo = locker_recursive_has_ammo || nested->typeId() == itype_9mm;
      locker_recursive_has_knife = locker_recursive_has_knife || nested->typeId() == itype_knife_combat;
    }
  }
  INFO("locker recursive bandages=" << locker_recursive_has_bandages
                                     << " mag=" << locker_recursive_has_magazine
                                     << " ammo=" << locker_recursive_has_ammo
                                     << " knife=" << locker_recursive_has_knife);
  CHECK(locker_has_daypack);
  CHECK_FALSE(locker_has_knife);
  CHECK_FALSE(locker_has_bandages);

  bool storage_has_knife = false;
  bool storage_has_ear_muffs = false;
  bool storage_has_helmet = false;
  bool storage_has_bandages = false;
  bool storage_has_magazine = false;
  bool storage_has_ammo = false;
  for (const item &it : here.i_at(storage_local)) {
    storage_has_knife = storage_has_knife || it.typeId() == itype_knife_combat;
    storage_has_ear_muffs = storage_has_ear_muffs ||
                             it.typeId() == itype_attachable_ear_muffs;
    storage_has_helmet = storage_has_helmet || it.typeId() == itype_helmet_army;
    storage_has_bandages = storage_has_bandages || it.typeId() == itype_bandages;
    storage_has_magazine = storage_has_magazine || it.typeId() == itype_glockmag;
    storage_has_ammo = storage_has_ammo || it.typeId() == itype_9mm;
  }
  const tripoint_bub_ms worker_floor_local = here.get_bub(worker.pos_abs());
  bool worker_floor_has_knife = false;
  bool worker_floor_has_bandages = false;
  bool worker_floor_has_magazine = false;
  bool worker_floor_has_ammo = false;
  for (const item &it : here.i_at(worker_floor_local)) {
    worker_floor_has_knife = worker_floor_has_knife || it.typeId() == itype_knife_combat;
    worker_floor_has_bandages = worker_floor_has_bandages || it.typeId() == itype_bandages;
    worker_floor_has_magazine = worker_floor_has_magazine || it.typeId() == itype_glockmag;
    worker_floor_has_ammo = worker_floor_has_ammo || it.typeId() == itype_9mm;
  }
  INFO("storage knife=" << storage_has_knife
                         << " ear_muffs=" << storage_has_ear_muffs
                         << " helmet=" << storage_has_helmet
                         << " bandages=" << storage_has_bandages
                         << " mag=" << storage_has_magazine << " ammo=" << storage_has_ammo);
  INFO("worker floor knife=" << worker_floor_has_knife
                              << " bandages=" << worker_floor_has_bandages
                              << " mag=" << worker_floor_has_magazine
                              << " ammo=" << worker_floor_has_ammo);
  CHECK(storage_has_knife);
  CHECK_FALSE(storage_has_ear_muffs);
  CHECK(storage_has_helmet);
  CHECK_FALSE(storage_has_bandages);
  CHECK_FALSE(storage_has_magazine);
  CHECK_FALSE(storage_has_ammo);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_readies_medical_consumables_from_locker_supply",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_bandages));
  here.add_item_or_charges(locker_local, item(itype_adhesive_bandages));
  here.add_item_or_charges(locker_local, item(itype_medical_gauze));
  here.add_item_or_charges(locker_local, item(itype_bandages_makeshift));
  here.add_item_or_charges(locker_local, item(itype_bandages_makeshift_bleached));
  here.add_item_or_charges(locker_local, item(itype_bandages_makeshift_boiled));
  here.add_item_or_charges(locker_local, item(itype_aspirin));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));

  CHECK(count_character_items(worker, itype_bandages) == 1);
  CHECK(count_character_items(worker, itype_adhesive_bandages) == 1);
  CHECK(count_character_items(worker, itype_medical_gauze) == 1);
  CHECK(count_character_items(worker, itype_bandages_makeshift) == 1);
  CHECK(count_character_items(worker, itype_bandages_makeshift_bleached) == 1);
  CHECK(count_character_items(worker, itype_bandages_makeshift_boiled) == 1);
  CHECK(count_character_items(worker, itype_aspirin) == 0);
  CHECK(count_tile_items(here, locker_local, itype_bandages) == 0);
  CHECK(count_tile_items(here, locker_local, itype_adhesive_bandages) == 0);
  CHECK(count_tile_items(here, locker_local, itype_medical_gauze) == 0);
  CHECK(count_tile_items(here, locker_local, itype_bandages_makeshift) == 0);
  CHECK(count_tile_items(here, locker_local, itype_bandages_makeshift_bleached) == 0);
  CHECK(count_tile_items(here, locker_local, itype_bandages_makeshift_boiled) == 0);
  CHECK(count_tile_items(here, locker_local, itype_aspirin) == 1);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_preserves_carried_medical_and_caps_reserve",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  for (int i = 0; i < 12; ++i) {
    here.add_item_or_charges(locker_local, item(itype_bandages));
  }
  here.add_item_or_charges(locker_local, item(itype_aspirin));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  REQUIRE(worker.i_add(item(itype_adhesive_bandages)));
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));

  CHECK(count_character_items(worker, itype_adhesive_bandages) == 1);
  CHECK(count_character_items(worker, itype_bandages) == 9);
  CHECK(count_character_items(worker, itype_aspirin) == 0);
  CHECK(count_tile_items(here, locker_local, itype_bandages) == 3);
  CHECK(count_tile_items(here, locker_local, itype_aspirin) == 1);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_ignores_unrelated_medical_consumables",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_aspirin));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->service_camp_locker(worker));

  CHECK(count_character_items(worker, itype_aspirin) == 0);
  CHECK(count_tile_items(here, locker_local, itype_aspirin) == 1);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_service_readies_ranged_loadouts_from_locker_supply",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_glockmag));
  here.add_item_or_charges(locker_local, item(itype_glockmag));
  here.add_item_or_charges(locker_local, item(itype_glockmag));
  const int ammo_total = item(itype_glockmag).remaining_ammo_capacity() * 2;
  here.add_item_or_charges(locker_local,
                           item(itype_9mm, calendar::turn_zero, ammo_total));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  item empty_glock(itype_glock_19);
  REQUIRE(worker.wield(empty_glock));
  REQUIRE(worker.get_wielded_item());
  CHECK(worker.get_wielded_item()->ammo_remaining() == 0);

  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->process_camp_locker_downtime(worker));

  const item_location weapon = worker.get_wielded_item();
  REQUIRE(weapon);
  CHECK(weapon->typeId() == itype_glock_19);
  REQUIRE(weapon->magazine_current() != nullptr);
  CHECK(weapon->ammo_remaining() > 0);

  int compatible_magazines = 1;
  int loaded_compatible_magazines = weapon->magazine_current()->ammo_remaining() > 0 ? 1 : 0;
  int compatible_ammo_total = weapon->magazine_current()->ammo_remaining();
  worker.visit_items([&weapon, &compatible_magazines,
                      &loaded_compatible_magazines,
                      &compatible_ammo_total](item *node, item *) {
    if (node != nullptr && node->is_magazine() &&
        weapon->can_reload_with(*node, false)) {
      compatible_magazines++;
      compatible_ammo_total += node->ammo_remaining();
      if (node->ammo_remaining() > 0) {
        loaded_compatible_magazines++;
      }
    }
    return VisitResponse::NEXT;
  });

  CHECK(compatible_magazines == 2);
  CHECK(loaded_compatible_magazines == 2);
  CHECK(compatible_ammo_total == ammo_total);

  int locker_magazines = 0;
  int locker_ammo_remaining = 0;
  for (const item &it : here.i_at(locker_local)) {
    if (it.typeId() == itype_glockmag) {
      locker_magazines++;
    }
    if (it.typeId() == itype_9mm) {
      locker_ammo_remaining += it.charges;
    }
  }
  CHECK(locker_magazines == 1);
  CHECK(locker_ammo_remaining == 0);

  calendar::turn += 10_minutes;
  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_ranged_readiness_prefers_existing_magazine_capacity_api",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_glockmag));
  here.add_item_or_charges(locker_local, item(itype_glockbigmag));
  const int ammo_total = item(itype_glockmag).remaining_ammo_capacity() +
                         item(itype_glockbigmag).remaining_ammo_capacity();
  here.add_item_or_charges(locker_local,
                           item(itype_9mm, calendar::turn_zero, ammo_total));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  REQUIRE(worker.i_add(item(itype_glockmag)));
  item empty_glock(itype_glock_19);
  REQUIRE(worker.wield(empty_glock));
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));

  const item_location weapon = worker.get_wielded_item();
  REQUIRE(weapon);
  REQUIRE(weapon->magazine_current() != nullptr);
  CHECK(weapon->magazine_current()->typeId() == itype_glockbigmag);
  CHECK(weapon->ammo_remaining() ==
        item(itype_glockbigmag).remaining_ammo_capacity());
  CHECK(count_character_items(worker, itype_glockmag) == 1);
  CHECK(count_tile_items(here, locker_local, itype_glockbigmag) == 0);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_ranged_readiness_uses_existing_reload_supply_api",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);

  item loaded_speedloader(itype_38_speedloader, calendar::turn_zero, 0);
  const ammotype &speedloader_ammo_type =
      item::find_type(loaded_speedloader.ammo_default())->ammo->type;
  item speedloader_ammo(
      itype_38_special, calendar::turn_zero,
      loaded_speedloader.ammo_capacity(speedloader_ammo_type));
  REQUIRE(loaded_speedloader.put_in(speedloader_ammo,
                                    pocket_type::MAGAZINE).success());
  const int speedloader_rounds = loaded_speedloader.ammo_remaining();
  REQUIRE(speedloader_rounds > 0);
  REQUIRE(loaded_speedloader.has_ammo());
  here.add_item_or_charges(locker_local, loaded_speedloader);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  item empty_revolver(itype_sw_619, calendar::turn_zero, 0);
  REQUIRE(worker.wield(empty_revolver));
  REQUIRE(worker.get_wielded_item());
  CHECK(worker.get_wielded_item()->ammo_remaining() == 0);
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->service_camp_locker(worker));

  const item_location weapon = worker.get_wielded_item();
  REQUIRE(weapon);
  CHECK(weapon->ammo_remaining() == speedloader_rounds);

  bool locker_has_empty_speedloader = false;
  for (const item &it : here.i_at(locker_local)) {
    if (it.typeId() == itype_38_speedloader) {
      locker_has_empty_speedloader = locker_has_empty_speedloader ||
                                     (!it.has_ammo() && it.ammo_remaining() == 0);
    }
  }
  CHECK(locker_has_empty_speedloader);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_ranged_readiness_defers_worker_reload_rules",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_belt223));
  here.add_item_or_charges(locker_local, item(itype_556, calendar::turn_zero, 500));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  item m249(itype_m249);
  REQUIRE(worker.wield(m249));
  test_camp->add_assignee(worker.getID());

  const camp_locker_service_probe probe =
      test_camp->measure_camp_locker_service(worker);
  CHECK(probe.magazines_to_take == 1);
  CHECK(probe.magazines_to_reload == 0);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_ranged_readiness_ignores_magazines_installed_in_carried_guns",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_bub_ms npc_local{5, 5, 0};
  const tripoint_abs_ms storage_abs = here.get_abs(tripoint_bub_ms{4, 5, 0});
  const tripoint_bub_ms storage_local = here.get_bub(storage_abs);
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Storage", zone_type_CAMP_STORAGE, storage_abs);
  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(storage_local);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_glockmag));
  here.add_item_or_charges(locker_local, item(itype_glockmag));
  const int ammo_total = item(itype_glockmag).remaining_ammo_capacity() * 2;
  here.add_item_or_charges(locker_local,
                           item(itype_9mm, calendar::turn_zero, ammo_total));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(npc_local.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_backpack), false).has_value());
  item empty_glock(itype_glock_19);
  REQUIRE(worker.wield(empty_glock));

  item carried_spare_gun(itype_glock_19);
  REQUIRE(carried_spare_gun.put_in(make_loaded_glock_magazine(),
                                   pocket_type::MAGAZINE_WELL).success());
  REQUIRE(carried_spare_gun.magazine_current() != nullptr);
  REQUIRE(worker.i_add(carried_spare_gun));
  test_camp->add_assignee(worker.getID());

  const camp_locker_service_probe probe =
      test_camp->measure_camp_locker_service(worker);
  CHECK(probe.magazines_to_take == 2);
  CHECK(probe.magazines_to_reload == 2);

  const item_location weapon = worker.get_wielded_item();
  REQUIRE(weapon);
  REQUIRE(weapon->magazine_current() != nullptr);
  CHECK(weapon->ammo_remaining() > 0);
  CHECK(count_character_items(worker, itype_glockmag) == 1);
  CHECK(count_tile_items(here, storage_local, itype_glock_19) == 1);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_downtime_queue_processes_one_worker_at_a_time",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_duffelbag));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &first_worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(first_worker, true);
  REQUIRE(first_worker.wear_item(item(itype_daypack), false).has_value());

  npc &second_worker = spawn_npc(tripoint_bub_ms{5, 6, 0}.xy(), "thug");
  clear_character(second_worker, true);
  REQUIRE(second_worker.wear_item(item(itype_daypack), false).has_value());

  test_camp->add_assignee(first_worker.getID());
  test_camp->add_assignee(second_worker.getID());

  CHECK(test_camp->process_camp_locker_downtime(second_worker));
  CHECK(second_worker.is_wearing(itype_duffelbag));
  CHECK_FALSE(second_worker.is_wearing(itype_daypack));

  CHECK_FALSE(test_camp->process_camp_locker_downtime(first_worker));
  CHECK(first_worker.is_wearing(itype_daypack));

  calendar::turn += 10_minutes;
  CHECK_FALSE(test_camp->process_camp_locker_downtime(first_worker));
  CHECK(first_worker.is_wearing(itype_daypack));
  CHECK_FALSE(first_worker.is_wearing(itype_duffelbag));

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_new_zone_gear_requeues_worker_after_baseline_noop",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));
  CHECK(worker.is_wearing(itype_daypack));
  CHECK_FALSE(worker.is_wearing(itype_duffelbag));

  here.add_item_or_charges(locker_local, item(itype_duffelbag));

  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));
  CHECK(worker.is_wearing(itype_daypack));
  CHECK_FALSE(worker.is_wearing(itype_duffelbag));

  calendar::turn += 2_minutes;
  CHECK(test_camp->process_camp_locker_downtime(worker));
  CHECK(worker.is_wearing(itype_duffelbag));
  CHECK_FALSE(worker.is_wearing(itype_daypack));

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_missing_zone_reports_typed_exception_once",
          "[camp][locker][messages]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();
  Messages::clear_messages();
  basecamp_ai::reset_camp_job_report_debounce();

  map &here = get_map();
  const tripoint_abs_omt camp_omt = project_to<coords::omt>(here.get_abs(tripoint_bub_ms{6, 5, 0}));
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(worker, true);
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));
  std::vector<std::pair<std::string, std::string>> messages = Messages::recent_messages(0);
  REQUIRE_FALSE(messages.empty());
  CHECK(messages.back().second.find("[camp][locker]") != std::string::npos);
  CHECK(messages.back().second.find("no Basecamp: Locker zone") != std::string::npos);

  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));
  CHECK(Messages::recent_messages(0).size() == messages.size());

  calendar::turn += 31_minutes;
  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));
  messages = Messages::recent_messages(0);
  REQUIRE_FALSE(messages.empty());
  CHECK(messages.back().second.find("[camp][locker]") != std::string::npos);

  Messages::clear_messages();
  basecamp_ai::reset_camp_job_report_debounce();
  zone_manager::get_manager().clear();
  overmap_buffer.clear_camps(camp_omt.xy());
}

TEST_CASE("camp_locker_skip_probe_handles_numeric_last_service_turn",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(worker, true);
  test_camp->add_assignee(worker.getID());

  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));

  bool processed = true;
  CHECK_NOTHROW(processed = test_camp->process_camp_locker_downtime(worker));
  CHECK_FALSE(processed);

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_losing_managed_gear_requeues_worker",
          "[camp][locker]") {
  restore_on_out_of_scope restore_calendar_turn(calendar::turn);
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_duffelbag));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  here.add_camp(camp_omt, "faction_camp");
  std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_omt.xy());
  REQUIRE(!!bcp);
  basecamp *test_camp = *bcp;
  test_camp->set_owner(your_fac);

  npc &worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
  test_camp->add_assignee(worker.getID());

  REQUIRE(test_camp->process_camp_locker_downtime(worker));
  REQUIRE(worker.is_wearing(itype_duffelbag));
  CHECK_FALSE(worker.is_wearing(itype_daypack));

  std::list<item> removed = worker.remove_worn_items_with([](item &it) {
    return it.typeId() == itype_duffelbag;
  });
  REQUIRE(removed.size() == 1);
  CHECK_FALSE(worker.is_wearing(itype_duffelbag));

  CHECK_FALSE(test_camp->process_camp_locker_downtime(worker));
  CHECK_FALSE(worker.is_wearing(itype_daypack));

  calendar::turn += 10_minutes;
  CHECK(test_camp->process_camp_locker_downtime(worker));
  CHECK(worker.is_wearing(itype_daypack));
  CHECK_FALSE(worker.is_wearing(itype_duffelbag));

  zone_manager::get_manager().clear();
}

TEST_CASE("camp_locker_downtime_rehydrates_transient_assignees",
          "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.add_item_or_charges(locker_local, item(itype_duffelbag));

  const tripoint_abs_omt camp_omt = project_to<coords::omt>(locker_abs);
  basecamp loaded("Locker Camp", camp_omt);
  loaded.set_owner(your_fac);

  npc &worker = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
  clear_character(worker, true);
  REQUIRE(worker.wear_item(item(itype_daypack), false).has_value());
  worker.assigned_camp = camp_omt;
  g->add_npc_follower(worker.getID());
  CHECK(g->get_follower_list().count(worker.getID()) == 1);
  CHECK(loaded.get_npcs_assigned().size() == 1);

  CHECK(loaded.process_camp_locker_downtime(worker));
  CHECK(worker.is_wearing(itype_duffelbag));
  CHECK_FALSE(worker.is_wearing(itype_daypack));

  g->remove_npc_follower(worker.getID());
  zone_manager::get_manager().clear();
}

TEST_CASE("camp_calorie_counting", "[camp]") {
  clear_avatar();
  clear_map_without_vision();
  map &m = get_map();
    const tripoint_abs_ms zone_loc = m.get_abs( tripoint_bub_ms{ 5, 5, 0 } );
    REQUIRE( m.inbounds( zone_loc ) );
    mapgen_place_zone( zone_loc, zone_loc, zone_type_CAMP_FOOD, your_fac, {},
                       "food" );
    mapgen_place_zone( zone_loc, zone_loc, zone_type_CAMP_STORAGE, your_fac, {},
                       "storage" );
    faction *camp_faction = get_player_character().get_faction();
    const tripoint_abs_omt this_omt = project_to<coords::omt>( zone_loc );
    m.add_camp( this_omt, "faction_camp" );
    std::optional<basecamp *> bcp = overmap_buffer.find_camp( this_omt.xy() );
    REQUIRE( !!bcp );
    basecamp *test_camp = *bcp;
    test_camp->set_owner( your_fac );
    WHEN( "a base item is added to larder" ) {
        camp_faction->empty_food_supply();
        item test_100_kcal( itype_test_100_kcal );
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
        m.add_item_or_charges( zone_local, test_100_kcal );
        REQUIRE( m.has_items( zone_local ) );
        test_camp->distribute_food();
        CHECK( camp_faction->food_supply().kcal() == 100 );
    }

    WHEN( "an item with inherited components is added to larder" ) {
        camp_faction->empty_food_supply();
        item test_100_kcal( itype_test_100_kcal );
        item test_200_kcal( itype_test_200_kcal );
    item_components made_of;
    made_of.add(test_100_kcal);
    made_of.add(test_100_kcal);
    // Setting the actual components. This will return 185 unless it's actually
    // made up of two 100kcal components!
    test_200_kcal.components = made_of;
    tripoint_bub_ms zone_local = m.get_bub(zone_loc);
    m.i_clear(zone_local);
        m.add_item_or_charges( zone_local, test_200_kcal );
        test_camp->distribute_food();
        CHECK( camp_faction->food_supply().kcal() == 200 );
    }

    WHEN( "an item with vitamins is added to larder" ) {
        camp_faction->empty_food_supply();
        item test_500_kcal( itype_test_500_kcal );
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
    m.add_item_or_charges(zone_local, test_500_kcal);
    test_camp->distribute_food();
    REQUIRE(camp_faction->food_supply().kcal() == 500);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutant_toxin) ==
            100);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutagen) == 200);
  }

    WHEN( "a larder with stored calories and vitamins has food withdrawn" ) {
        camp_faction->empty_food_supply();
        std::map<time_point, nutrients> added_food;
        added_food[calendar::turn_zero].calories = 100000;
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutant_toxin, 100 );
    added_food[calendar::turn_zero].set_vitamin(vitamin_mutagen, 200);
    camp_faction->add_to_food_supply(added_food);
    REQUIRE(camp_faction->food_supply().kcal() == 100);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutant_toxin) ==
            100);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutagen) == 200);
    // Now withdraw 15% of the total calories, this should also draw out 15% of
    // the stored vitamins.
    test_camp->camp_food_supply(-15);
    CHECK(camp_faction->food_supply().kcal() == 85);
    CHECK(camp_faction->food_supply().get_vitamin(vitamin_mutant_toxin) == 85);
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 170 );
    }

    WHEN( "a larder with perishable food passes the expiry date" ) {
        restore_on_out_of_scope restore_calendar_turn( calendar::turn );
        camp_faction->empty_food_supply();
        // non-perishable food
        std::map<time_point, nutrients> added_food;
        added_food[calendar::turn_zero].calories = 100000;
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutant_toxin, 100 );
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutagen, 200 );

    camp_faction->add_to_food_supply(added_food);
    REQUIRE(camp_faction->food_supply().kcal() == 100);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutant_toxin) ==
            100);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutagen) == 200);

    // remove non-perishable from added
        added_food.erase( added_food.begin() );
        // perishable food
        added_food[calendar::turn + 7_days].calories = 150000;
        added_food[calendar::turn + 7_days].set_vitamin( vitamin_mutant_toxin, 200 );
        added_food[calendar::turn + 7_days].set_vitamin( vitamin_mutagen, 100 );

    camp_faction->add_to_food_supply(added_food);
    REQUIRE(camp_faction->food_supply().kcal() == 250);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutant_toxin) ==
            300);
    REQUIRE(camp_faction->food_supply().get_vitamin(vitamin_mutagen) == 300);

    // advance time
        calendar::turn += 14_days;

        CHECK( camp_faction->food_supply().kcal() == 100 );
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 100 );
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 200 );
    }

    WHEN( "an item that expires is added to larder" ) {
        restore_on_out_of_scope restore_calendar_turn( calendar::turn );
        camp_faction->empty_food_supply();
        item test_100_kcal( itype_test_100_kcal );
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
        m.add_item_or_charges( zone_local, test_100_kcal );
        test_camp->distribute_food();
        REQUIRE( camp_faction->food_supply().kcal() == 100 );
        REQUIRE( test_100_kcal.type->comestible != nullptr );
        REQUIRE( test_100_kcal.type->comestible->spoils == 360_days );

        calendar::turn += 365_days;

        CHECK( camp_faction->food_supply().kcal() == 0 );
  }
  overmap_buffer.clear_camps(this_omt.xy());
}
TEST_CASE("camp_request_speech_parsing", "[camp][basecamp_ai]") {
  using basecamp_ai::camp_board_handoff_snapshot;
  using basecamp_ai::camp_board_status_bark;
  using basecamp_ai::camp_craft_resolution_outcome;
  using basecamp_ai::camp_job_token_kind;
  using basecamp_ai::camp_request_handoff_snapshot;
  using basecamp_ai::camp_request_spoken_status_bark;
  using basecamp_ai::camp_request_subject_for_display;
  using basecamp_ai::collect_archived_camp_request_ids;
  using basecamp_ai::collect_blocked_camp_request_ids;
  using basecamp_ai::collect_ready_camp_request_ids;
    using basecamp_ai::format_overmap_movement_token;
    using basecamp_ai::format_overmap_snapshot;
    using basecamp_ai::match_camp_craft_query;
    using basecamp_ai::match_camp_request_reference;
    using basecamp_ai::matches_assigned_camp_request_worker;
    using basecamp_ai::parse_heard_camp_approval_query;
    using basecamp_ai::parse_heard_camp_cancel_query;
    using basecamp_ai::parse_heard_camp_clear_query;
    using basecamp_ai::parse_heard_camp_craft_order;
    using basecamp_ai::parse_heard_camp_status_query;
    using basecamp_ai::parse_overmap_movement_token;
    using basecamp_ai::parse_relative_omt_delta;
    using basecamp_ai::parse_structured_camp_craft_order;
    using basecamp_ai::parse_structured_camp_job_token;
    using basecamp_ai::parsed_overmap_movement_intent;
    using basecamp_ai::resolve_camp_craft_query;
    using basecamp_ai::resolve_overmap_movement_target;
    using basecamp_ai::score_camp_recipe_query;

    SECTION( "craft orders parse quantity words and direct object" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_heard_camp_craft_order( "please craft me five bandages" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 5 );
        CHECK( parsed->item_query == "bandages" );
    }

    SECTION( "spoken craft intake requires the exact standalone word craft" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_heard_camp_craft_order( "craft knife" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 1 );
    CHECK(parsed->item_query == "knife");
  }

  SECTION(
      "spoken craft intake rejects non craft verbs and partial-word matches") {
    CHECK_FALSE(parse_heard_camp_craft_order("make knife").has_value());
    CHECK_FALSE(parse_heard_camp_craft_order("build knife").has_value());
    CHECK_FALSE(parse_heard_camp_craft_order("witchcraft knife").has_value());
    }

    SECTION( "spoken craft intake tolerates polite prompt lead-ins" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_heard_camp_craft_order( "could you please craft me five bandages" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 5 );
        CHECK( parsed->item_query == "bandages" );
    }

    SECTION( "structured craft tokens parse quantity and direct object" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_structured_camp_craft_order( " craft=the five bandages please " );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 5 );
    CHECK(parsed->item_query == "bandages");
  }

  SECTION("structured craft tokens reject malformed payloads and unknown "
          "prefixes") {
    CHECK_FALSE(parse_structured_camp_craft_order("craft=").has_value());
    CHECK_FALSE(
        parse_structured_camp_craft_order("craft = bandages").has_value());
    CHECK_FALSE(parse_structured_camp_craft_order("job=12").has_value());
  }

  SECTION("structured camp job tokens parse board inspection, exact request "
          "ids, and batch board actions") {
    const std::optional<basecamp_ai::parsed_camp_job_token> board =
        parse_structured_camp_job_token(" SHOW_BOARD ");
    REQUIRE(board.has_value());
        CHECK( board->kind == camp_job_token_kind::show_board );
        CHECK( board->request_id == 0 );

        const std::optional<basecamp_ai::parsed_camp_job_token> details =
            parse_structured_camp_job_token( " show_job=12 " );
        REQUIRE( details.has_value() );
        CHECK( details->kind == camp_job_token_kind::show_job );
        CHECK( details->request_id == 12 );

        const std::optional<basecamp_ai::parsed_camp_job_token> action =
            parse_structured_camp_job_token( " job=12 " );
        REQUIRE( action.has_value() );
        CHECK( action->kind == camp_job_token_kind::act_on_job );
        CHECK( action->request_id == 12 );

        const std::optional<basecamp_ai::parsed_camp_job_token> deletion =
            parse_structured_camp_job_token( " DELETE_JOB=7 " );
        REQUIRE( deletion.has_value() );
        CHECK( deletion->kind == camp_job_token_kind::delete_job );
        CHECK( deletion->request_id == 7 );

        const std::optional<basecamp_ai::parsed_camp_job_token> launch_ready =
            parse_structured_camp_job_token( " launch_ready_jobs " );
        REQUIRE( launch_ready.has_value() );
        CHECK( launch_ready->kind == camp_job_token_kind::launch_ready_jobs );
        CHECK( launch_ready->request_id == 0 );

        const std::optional<basecamp_ai::parsed_camp_job_token> retry_blocked =
            parse_structured_camp_job_token( " RETRY_BLOCKED_JOBS " );
        REQUIRE( retry_blocked.has_value() );
        CHECK( retry_blocked->kind == camp_job_token_kind::retry_blocked_jobs );
        CHECK( retry_blocked->request_id == 0 );

        const std::optional<basecamp_ai::parsed_camp_job_token> clear_archived =
            parse_structured_camp_job_token( "clear_archived_jobs" );
        REQUIRE( clear_archived.has_value() );
        CHECK( clear_archived->kind == camp_job_token_kind::clear_archived_jobs );
    CHECK(clear_archived->request_id == 0);
  }

  SECTION(
      "structured camp job tokens reject malformed ids and unknown prefixes") {
    CHECK_FALSE(parse_structured_camp_job_token("show_job=0").has_value());
    CHECK_FALSE(parse_structured_camp_job_token("job=0").has_value());
    CHECK_FALSE(parse_structured_camp_job_token("job=-2").has_value());
    CHECK_FALSE(parse_structured_camp_job_token("job=twelve").has_value());
    CHECK_FALSE(
        parse_structured_camp_job_token("show_job=12 please").has_value());
    CHECK_FALSE(parse_structured_camp_job_token("job=12 please").has_value());
    CHECK_FALSE(parse_structured_camp_job_token("launch_ready_jobs please")
                    .has_value());
    CHECK_FALSE(
        parse_structured_camp_job_token("retry blocked jobs").has_value());
    CHECK_FALSE(parse_structured_camp_job_token("work=12").has_value());
  }

    SECTION( "approval commands parse explicit request numbers" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_approval_query( "approve request #12 please" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->has_request_id );
        CHECK( parsed->request_id == 12 );
        CHECK_FALSE( parsed->all_requests );
    }

    SECTION( "approval commands can target all ready work orders" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_approval_query( "launch all ready work orders" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "approval commands tolerate polite prompt lead-ins" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_approval_query( "would you please approve request #12" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->has_request_id );
        CHECK( parsed->request_id == 12 );
    }

    SECTION( "approval commands accept ready-request synonyms" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> requests =
            parse_heard_camp_approval_query( "approve all ready requests" );
        REQUIRE( requests.has_value() );
        CHECK( requests->all_requests );
        CHECK_FALSE( requests->all_blocked_requests );
        CHECK_FALSE( requests->has_request_id );

        const std::optional<basecamp_ai::parsed_camp_request_reference> jobs =
            parse_heard_camp_approval_query( "launch all ready jobs" );
        REQUIRE( jobs.has_value() );
        CHECK( jobs->all_requests );
        CHECK_FALSE( jobs->all_blocked_requests );
        CHECK_FALSE( jobs->has_request_id );
    }

    SECTION( "approval commands can target all blocked work orders for retry" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> requests =
            parse_heard_camp_approval_query( "retry all blocked requests" );
        REQUIRE( requests.has_value() );
        CHECK_FALSE( requests->all_requests );
        CHECK( requests->all_blocked_requests );
        CHECK_FALSE( requests->has_request_id );

        const std::optional<basecamp_ai::parsed_camp_request_reference> jobs =
            parse_heard_camp_approval_query( "resume all blocked jobs" );
        REQUIRE( jobs.has_value() );
        CHECK_FALSE( jobs->all_requests );
        CHECK( jobs->all_blocked_requests );
        CHECK_FALSE( jobs->has_request_id );
    }

    SECTION( "ready-request collector excludes blocked and archived entries" ) {
        const std::vector<camp_llm_request> requests = {
            ([]() { camp_llm_request request; request.request_id = 2; request.status = "awaiting_approval"; return request; }()),
            ([]() { camp_llm_request request; request.request_id = 4; request.status = "blocked"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 5; request.status = "in_progress"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 7; request.status = "awaiting_approval"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.status = "completed"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 11; request.status = "cancelled"; return request; }())};

    CHECK(collect_ready_camp_request_ids(requests) == std::vector<int>{2, 7});
  }

    SECTION( "blocked-request collector excludes ready live work and archives" ) {
        const std::vector<camp_llm_request> requests = {
            ([]() { camp_llm_request request; request.request_id = 2; request.status = "awaiting_approval"; return request; }()),
            ([]() { camp_llm_request request; request.request_id = 4; request.status = "blocked"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 5; request.status = "in_progress"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 7; request.status = "blocked"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.status = "completed"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 11; request.status = "cancelled"; return request; }())};

    CHECK(collect_blocked_camp_request_ids(requests) == std::vector<int>{4, 7});
  }

  SECTION(
      "archived-request collector keeps completed and cancelled work only") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 2; request.status = "awaiting_approval"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 4; request.status = "blocked"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 5; request.status = "in_progress"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.status = "completed"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 11; request.status = "cancelled"; return request; }())};

    CHECK(collect_archived_camp_request_ids(requests) ==
          std::vector<int>{9, 11});
  }

  SECTION("status queries can ask for the whole board") {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_status_query( "what's on the board" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "status queries accept organic board phrasing" ) {
        for( const std::string_view utterance : {
                 std::string_view( "what needs making" ),
                 std::string_view( "what needs doing" ),
                 std::string_view( "got any craft work" ),
                 std::string_view( "show me what needs doing" ) } ) {
            const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
                parse_heard_camp_status_query( utterance );
            CAPTURE( utterance );
            REQUIRE( parsed.has_value() );
            CHECK( parsed->all_requests );
            CHECK_FALSE( parsed->has_request_id );
        }
    }

    SECTION( "status queries tolerate polite prompt lead-ins" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_status_query( "can you please show me the board" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "clear commands can target archived paperwork in bulk" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_clear_query( "clear archived requests" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "clear commands tolerate polite prompt lead-ins" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_clear_query( "can you please clear archived requests" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

  SECTION("clear commands keep the archived work-order subject") {
    const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
        parse_heard_camp_clear_query(
            "clear the work order for long string please");
    REQUIRE(parsed.has_value());
    CHECK_FALSE(parsed->has_request_id);
    CHECK(parsed->query == "long string");
    }

    SECTION( "clear commands parse explicit request numbers" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_clear_query( "clear request #12 please" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->has_request_id );
        CHECK( parsed->request_id == 12 );
        CHECK_FALSE( parsed->all_requests );
    }

  SECTION("cancel commands keep the work-order subject") {
    const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
        parse_heard_camp_cancel_query(
            "cancel the work order for long string please");
    REQUIRE(parsed.has_value());
    CHECK_FALSE(parsed->has_request_id);
    CHECK(parsed->query == "long string");
    }

    SECTION( "cancel commands tolerate polite prompt lead-ins" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_cancel_query( "could you please cancel request #12" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->has_request_id );
    CHECK(parsed->request_id == 12);
  }

  SECTION("request matcher prefers explicit ids and respects the active "
          "predicate") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "awaiting_approval"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 12; request.requested_item_query = "knife"; request.chosen_recipe_name = "knife"; request.status = "cancelled"; return request; }())};
    basecamp_ai::parsed_camp_request_reference reference;
    reference.request_id = 12;
    reference.has_request_id = true;

    const basecamp_ai::camp_request_match_result archived =
        match_camp_request_reference(requests, reference,
                                     [](const camp_llm_request &request) {
                                       return request.status != "completed" &&
                                              request.status != "cancelled";
                                     });
    CHECK_FALSE(archived.found);
    CHECK(archived.request_id == 0);

    reference.request_id = 7;
    const basecamp_ai::camp_request_match_result active =
        match_camp_request_reference(requests, reference,
                                     [](const camp_llm_request &request) {
                                       return request.status != "completed" &&
                                              request.status != "cancelled";
                                     });
    CHECK(active.found);
    CHECK(active.request_id == 7);
    CHECK(active.score == 1000);
  }

  SECTION("request matcher reports ambiguous live board matches instead of "
          "guessing") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 3; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "awaiting_approval"; request.assigned_worker_name = "Alice"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 4; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "blocked"; request.assigned_worker_name = "Bob"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 5; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "cancelled"; request.assigned_worker_name = "Cara"; return request; }())};
    basecamp_ai::parsed_camp_request_reference reference;
    reference.query = "bandages";

    const basecamp_ai::camp_request_match_result match =
        match_camp_request_reference(requests, reference,
                                     [](const camp_llm_request &request) {
                                       return request.status != "completed" &&
                                              request.status != "cancelled";
                                     });
    CHECK_FALSE(match.found);
    CHECK(match.score >= 650);
    CHECK(match.ambiguous_matches.size() == 2);
    CHECK(std::any_of(match.ambiguous_matches.begin(),
                      match.ambiguous_matches.end(),
                      [](const std::string &summary) {
                        return summary.find("#3") != std::string::npos;
                      }));
    CHECK(std::any_of(match.ambiguous_matches.begin(),
                      match.ambiguous_matches.end(),
                      [](const std::string &summary) {
                        return summary.find("#4") != std::string::npos;
                      }));
  }

  SECTION(
      "request matcher can target archived work orders for clear commands") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 3; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "awaiting_approval"; request.assigned_worker_name = "Alice"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 4; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "cancelled"; request.assigned_worker_name = "Bob"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 5; request.requested_item_query = "knife"; request.chosen_recipe_name = "knife"; request.status = "completed"; request.assigned_worker_name = "Cara"; return request; }())};
    basecamp_ai::parsed_camp_request_reference reference;
    reference.query = "bob bandages";

    const basecamp_ai::camp_request_match_result match =
        match_camp_request_reference(requests, reference,
                                     [](const camp_llm_request &request) {
                                       return request.status == "completed" ||
                                              request.status == "cancelled";
                                     });
    CHECK(match.found);
    CHECK(match.request_id == 4);
        CHECK( match.score > 1000 );
    CHECK(match.ambiguous_matches.empty());
  }

  SECTION(
      "request matcher can disambiguate duplicate work orders by worker name") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 3; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "awaiting_approval"; request.assigned_worker_name = "Alice"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 4; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "awaiting_approval"; request.assigned_worker_name = "Bob"; return request; }())};
    basecamp_ai::parsed_camp_request_reference reference;
    reference.query = "bob bandages";

    const basecamp_ai::camp_request_match_result match =
        match_camp_request_reference(requests, reference,
                                     [](const camp_llm_request &request) {
                                       return request.status != "completed" &&
                                              request.status != "cancelled";
                                     });
    CHECK(match.found);
    CHECK(match.request_id == 4);
        CHECK( match.score > 1000 );
    CHECK(match.ambiguous_matches.empty());
  }

  SECTION("request matcher can disambiguate duplicate work orders by status "
          "words") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 3; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "awaiting_approval"; request.approval_state = "waiting_player"; request.assigned_worker_name = "Alice"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 4; request.requested_item_query = "bandages"; request.chosen_recipe_name = "bandages"; request.status = "blocked"; request.assigned_worker_name = "Bob"; return request; }())};
    basecamp_ai::parsed_camp_request_reference reference;
    reference.query = "blocked bandages";

    const basecamp_ai::camp_request_match_result match =
        match_camp_request_reference(requests, reference,
                                     [](const camp_llm_request &request) {
                                       return request.status != "completed" &&
                                              request.status != "cancelled";
                                     });
    CHECK(match.found);
    CHECK(match.request_id == 4);
        CHECK( match.score > 1000 );
    CHECK(match.ambiguous_matches.empty());
  }

  SECTION("request matcher can resolve spoken subject text through the source "
          "utterance") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 8; request.requested_item_query = "heavy cable"; request.chosen_recipe_name = "heavy cable"; request.source_utterance = "please craft me a long string"; request.status = "awaiting_approval"; return request; }())};
    basecamp_ai::parsed_camp_request_reference reference;
    reference.query = "long string";

    const basecamp_ai::camp_request_match_result match =
        match_camp_request_reference(
            requests, reference, [](const camp_llm_request &) { return true; });
    CHECK(match.found);
    CHECK(match.request_id == 8);
    CHECK(match.score >= 650);
    CHECK(match.ambiguous_matches.empty());
  }

  SECTION("request display subject prefers the heard query over the resolved "
          "recipe") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; return request; }());

    CHECK(camp_request_subject_for_display(request) == "5 × bandages");
  }

  SECTION(
      "request display subject falls back to the resolved recipe when needed") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.requested_count = 2; request.chosen_recipe_name = "boiled makeshift bandage"; return request; }());

    CHECK(camp_request_subject_for_display(request) ==
          "2 × boiled makeshift bandage");
  }

  SECTION("request summary subject can append the resolved recipe when it "
          "differs") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; return request; }());

    CHECK(camp_request_subject_for_display(request, true) ==
          "5 × bandages (matched sterile bandage)");
  }

  SECTION("request summary subject does not duplicate the resolved recipe when "
          "it matches") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "bandages"; return request; }());

    CHECK(camp_request_subject_for_display(request, true) == "5 × bandages");
  }

    SECTION( "request display subject falls back to a generic label when empty" ) {
        const camp_llm_request request;

    CHECK(camp_request_subject_for_display(request) == "crafting request");
  }

  SECTION("request display subject can keep the request number as trailing "
          "detail") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; return request; }());

    CHECK(camp_request_subject_for_display(request, false, true) ==
          "5 × bandages (#7)");
    CHECK(camp_request_subject_for_display(request, true, true) ==
          "5 × bandages (matched sterile bandage) (#7)");
  }

  SECTION("request display subject falls back to a generic numbered label when "
          "empty") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_id = 9; return request; }());

    CHECK(camp_request_subject_for_display(request, false, true) ==
          "crafting request (#9)");
  }

  SECTION("spoken status bark keeps ordinary speech free of request ids") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.status = "awaiting_approval"; request.approval_state = "waiting_player"; return request; }());

    CHECK(camp_request_spoken_status_bark(request) ==
          "5 × bandages is pinned, waiting on your go-ahead.");
  }

  SECTION(
      "spoken status bark keeps matched recipe detail on blocked requests") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; request.status = "blocked"; request.blockers = {
            "Camp storage could not supply the needed ingredients or tools."}; return request; }());

    CHECK(camp_request_spoken_status_bark(request) ==
          "5 × bandages (matched sterile bandage) is blocked — Camp "
          "storage could not supply the needed ingredients or tools.");
  }

  SECTION("board status bark stays concise and human-facing") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.status = "blocked"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 8; request.requested_item_query = "knife"; request.status = "in_progress"; request.assigned_worker_name = "Bruna"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.requested_item_query = "bandages"; request.requested_count = 2; request.status = "completed"; return request; }())};

    CHECK(camp_board_status_bark(requests) ==
          "Board's got 2 live and 1 old — 5 × bandages, knife.");
  }

  SECTION("craft request handoff snapshot keeps stable request facts plus "
          "board/detail/next tokens") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_id = 7; request.source_utterance = "craft 5 bandages"; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; request.status = "blocked"; request.approval_state = "not_needed"; request.blockers = {"No stationed worker can take this recipe right now."}; return request; }());

    CHECK(camp_request_handoff_snapshot(request) ==
          "board=show_board\n"
               "details=show_job=7\n"
               "id=7\n"
               "query=bandages\n"
               "count=5\n"
               "source=craft 5 bandages\n"
               "request=5 × bandages\n"
               "recipe=sterile bandage\n"
               "status=blocked\n"
               "approval=not_needed\n"
               "worker=none\n"
               "blockers=No stationed worker can take this recipe right now.\n"
          "next=job=7\n");
  }

  SECTION(
      "craft request handoff snapshot flips archived jobs to delete tokens") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_id = 9; request.requested_item_query = "bandages"; request.requested_count = 2; request.chosen_recipe_name = "bandages"; request.status = "completed"; request.approval_state = "approved"; request.assigned_worker_name = "Bruna"; return request; }());

    CHECK(camp_request_handoff_snapshot(request) == "board=show_board\n"
                                                    "details=show_job=9\n"
                                                    "id=9\n"
                                                    "query=bandages\n"
               "count=2\n"
               "source=none\n"
               "request=2 × bandages\n"
               "recipe=bandages\n"
               "status=completed\n"
               "approval=approved\n"
               "worker=Bruna\n"
               "blockers=none\n"
                                                    "next=delete_job=9\n");
  }

  SECTION("board handoff snapshot summarizes active and archived jobs with "
          "detail tokens") {
    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; request.status = "blocked"; request.approval_state = "not_needed"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 8; request.requested_item_query = "knife"; request.chosen_recipe_name = "knife"; request.status = "in_progress"; request.approval_state = "approved"; request.assigned_worker_name = "Bruna"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.requested_item_query = "bandages"; request.requested_count = 2; request.chosen_recipe_name = "bandages"; request.status = "completed"; request.approval_state = "approved"; request.assigned_worker_name = "Cara"; return request; }())};

    CHECK(camp_board_handoff_snapshot(requests) ==
          "board=show_board\n"
          "active=2\n"
          "archived=1\n"
          "job=7 subject=5 × bandages (matched sterile bandage) status=blocked "
          "approval=not_needed worker=none details=show_job=7 next=job=7\n"
          "job=8 subject=knife status=in_progress approval=approved "
          "worker=Bruna details=show_job=8 next=job=8\n"
          "job=9 subject=2 × bandages status=completed approval=approved "
          "worker=Cara details=show_job=9 next=delete_job=9\n");
  }

  SECTION("board handoff snapshot can include live overmap planner context "
          "when origin is provided") {
    static const mongroup_id GROUP_ZOMBIE_HORDE("GROUP_ZOMBIE_HORDE");
    const tripoint_abs_omt origin(1600, 1600, 0);

    overmap_buffer.clear_mongroups();
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        overmap_buffer.ter_set(
            tripoint_abs_omt(origin.x() + dx, origin.y() + dy, origin.z()),
            oter_id("field"));
      }
    }

    get_map().add_camp(origin, "faction_camp", false);
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x() + 1, origin.y(), origin.z()),
        oter_id("road_nesw"));
    overmap_buffer.spawn_mongroup(
        project_to<coords::sm>(tripoint_abs_omt(origin.x() + 1, origin.y(), 0)),
        GROUP_ZOMBIE_HORDE, 3);

    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; request.status = "blocked"; request.approval_state = "not_needed"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.requested_item_query = "bandages"; request.requested_count = 2; request.chosen_recipe_name = "bandages"; request.status = "completed"; request.approval_state = "approved"; request.assigned_worker_name = "Cara"; return request; }())};

    const std::string snapshot = camp_board_handoff_snapshot(origin, requests);
    CAPTURE(snapshot);
    CHECK(snapshot.find(
              "board=show_board\nplanner_move=stay | move_omt dx=<signed_int> "
              "dy=<signed_int>\novermap:\novermap snapshot centered on dx=0 "
              "dy=0 (+x east/right, +y north/up)\n") != std::string::npos);
    const std::string dy0_line = find_snapshot_line(snapshot, "dy=+0");
    CHECK_FALSE(dy0_line.empty());
    CHECK(lowercase_ascii(dy0_line).find(" c  r ") != std::string::npos);
    CHECK(snapshot.find("  c camp\n") != std::string::npos);
    CHECK(snapshot.find("  f field\n") != std::string::npos);
    CHECK(snapshot.find("  r road\n") != std::string::npos);
    CHECK(snapshot.find("  uppercase = horde present\n") != std::string::npos);
    CHECK(snapshot.find("active=1\narchived=1\n") != std::string::npos);
    CHECK(snapshot.find("job=7 subject=5 × bandages (matched sterile bandage) "
                        "status=blocked approval=not_needed worker=none "
                        "details=show_job=7 next=job=7\n") !=
          std::string::npos);
    CHECK(snapshot.find(
              "job=9 subject=2 × bandages status=completed approval=approved "
              "worker=Cara details=show_job=9 next=delete_job=9\n") !=
          std::string::npos);

    overmap_buffer.clear_camps(origin.xy());
    overmap_buffer.clear_mongroups();
  }

  SECTION("board handoff keeps planner context as a prefixed layer over the deterministic board body") {
    const tripoint_abs_omt origin(1605, 1605, 0);

    overmap_buffer.clear_mongroups();
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        overmap_buffer.ter_set(
            tripoint_abs_omt(origin.x() + dx, origin.y() + dy, origin.z()),
            oter_id("field"));
      }
    }

    get_map().add_camp(origin, "faction_camp", false);

    const std::vector<camp_llm_request> requests = {
        ([]() { camp_llm_request request; request.request_id = 7; request.requested_item_query = "bandages"; request.requested_count = 5; request.chosen_recipe_name = "sterile bandage"; request.status = "blocked"; request.approval_state = "not_needed"; return request; }()),
        ([]() { camp_llm_request request; request.request_id = 9; request.requested_item_query = "bandages"; request.requested_count = 2; request.chosen_recipe_name = "bandages"; request.status = "completed"; request.approval_state = "approved"; request.assigned_worker_name = "Cara"; return request; }())};

    const std::string board_snapshot = camp_board_handoff_snapshot(requests);
    const std::string board_prefix = "board=show_board\n";
    REQUIRE(board_snapshot.find(board_prefix) == 0);
    const std::string board_body = board_snapshot.substr(board_prefix.size());
    REQUIRE_FALSE(board_body.empty());

    const std::string planner_snapshot = camp_board_handoff_snapshot(origin, requests);
    CAPTURE(planner_snapshot);
    CHECK(planner_snapshot.find(
              "board=show_board\nplanner_move=stay | move_omt dx=<signed_int> "
              "dy=<signed_int>\n") == 0);
    REQUIRE(planner_snapshot.size() >= board_body.size());
    CHECK(planner_snapshot.rfind(board_body) == planner_snapshot.size() - board_body.size());

    overmap_buffer.clear_camps(origin.xy());
    overmap_buffer.clear_mongroups();
  }

  SECTION("camp request router keeps structured board handoff separate from spoken board bark") {
    static const mongroup_id GROUP_ZOMBIE_HORDE("GROUP_ZOMBIE_HORDE");
    clear_avatar();
    clear_map_without_vision();
    Messages::clear_messages();

    const tripoint_abs_omt origin(1700, 1700, 0);
    overmap_buffer.clear_mongroups();
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        overmap_buffer.ter_set(
            tripoint_abs_omt(origin.x() + dx, origin.y() + dy, origin.z()),
            oter_id("field"));
      }
    }

    get_map().add_camp(origin, "faction_camp", false);
    std::optional<basecamp *> found_camp = overmap_buffer.find_camp(origin.xy());
    REQUIRE(found_camp.has_value());
    REQUIRE(*found_camp != nullptr);
    basecamp *camp = *found_camp;

    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x() + 1, origin.y(), origin.z()),
        oter_id("road_nesw"));
    overmap_buffer.spawn_mongroup(
        project_to<coords::sm>(tripoint_abs_omt(origin.x() + 1, origin.y(), 0)),
        GROUP_ZOMBIE_HORDE, 3);

    npc &listener = spawn_npc(tripoint_bub_ms{5, 5, 0}.xy(), "thug");
    clear_character(listener, true);
    listener.assigned_camp = origin;

    const recipe &bandages = recipe_id("bandages").obj();
    camp->queue_crafting_request(bandages, 5, listener, "craft 5 bandages",
                                 "bandages", listener.getID());

    REQUIRE(camp->handle_heard_camp_request(listener, "show_board"));
    std::vector<std::pair<std::string, std::string>> messages =
        Messages::recent_messages(0);
    REQUIRE_FALSE(messages.empty());
    const std::string structured_reply = messages.back().second;
    CAPTURE( structured_reply );
    CHECK( structured_reply.find( "Board's got" ) != std::string::npos );
    CHECK( structured_reply.find( "bandages" ) != std::string::npos );
    CHECK( structured_reply.find( "board=show_board" ) == std::string::npos );
    CHECK( structured_reply.find( "status=awaiting_approval" ) == std::string::npos );
    CHECK( structured_reply.find( "approval=waiting_player" ) == std::string::npos );
    CHECK( structured_reply.find( "details=show_job=1" ) == std::string::npos );
    CHECK( structured_reply.find( "next=job=1" ) == std::string::npos );

    Messages::clear_messages();
    REQUIRE(camp->handle_heard_camp_request(listener, "what needs making"));
    messages = Messages::recent_messages(0);
    REQUIRE_FALSE(messages.empty());
    const std::string spoken_reply = messages.back().second;
    CAPTURE(spoken_reply);
    CHECK(spoken_reply.find("Board's got 1 live and 0 old — 5 × bandages.") !=
          std::string::npos);
    CHECK(spoken_reply.find("board=show_board") == std::string::npos);
    CHECK(spoken_reply.find("planner_move=") == std::string::npos);
    CHECK(spoken_reply.find("overmap:") == std::string::npos);

    Messages::clear_messages();
    overmap_buffer.clear_camps(origin.xy());
    overmap_buffer.clear_mongroups();
  }

  SECTION( "camp request routing accepts stationed camp assignees, but keeps follower states out" ) {
    clear_avatar();
    clear_map_without_vision();

    const tripoint_abs_omt origin( 1704, 1704, 0 );
    npc &listener = spawn_npc( tripoint_bub_ms{ 6, 6, 0 }.xy(), "thug" );
    clear_character( listener, true );
    listener.assigned_camp = origin;
    listener.mission = NPC_MISSION_NULL;
    listener.companion_mission_role_id.clear();
    listener.set_attitude( NPCATT_NULL );

    CHECK( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.companion_mission_role_id = "SCAVENGER";
    CHECK( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.set_attitude( NPCATT_FOLLOW );
    CHECK_FALSE( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.set_attitude( NPCATT_WAIT );
    CHECK_FALSE( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.set_attitude( NPCATT_LEAD );
    CHECK_FALSE( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.set_attitude( NPCATT_NULL );
    listener.mission = NPC_MISSION_GUARD_ALLY;
    CHECK_FALSE( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.mission = NPC_MISSION_GUARD;
    CHECK( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.mission = NPC_MISSION_GUARD_PATROL;
    CHECK( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.mission = NPC_MISSION_NULL;
    listener.companion_mission_role_id = "FACTION_CAMP";
    CHECK( basecamp_ai::uses_basecamp_request_routing( listener ) );

    listener.assigned_camp.reset();
    CHECK_FALSE( basecamp_ai::uses_basecamp_request_routing( listener ) );
  }

  SECTION("camp request router keeps structured next=job follow-through on the job snapshot path") {
    clear_avatar();
    clear_map_without_vision();
    Messages::clear_messages();

    const tripoint_abs_omt origin(1705, 1705, 0);
    overmap_buffer.clear_mongroups();
    for( int dy = -2; dy <= 2; ++dy ) {
        for( int dx = -2; dx <= 2; ++dx ) {
            overmap_buffer.ter_set(
                tripoint_abs_omt( origin.x() + dx, origin.y() + dy, origin.z() ),
                oter_id( "field" ) );
        }
    }

    get_map().add_camp( origin, "faction_camp", false );
    std::optional<basecamp *> found_camp = overmap_buffer.find_camp( origin.xy() );
    REQUIRE( found_camp.has_value() );
    REQUIRE( *found_camp != nullptr );
    basecamp *camp = *found_camp;

    npc &listener = spawn_npc( tripoint_bub_ms{ 6, 6, 0 }.xy(), "thug" );
    clear_character( listener, true );
    listener.assigned_camp = origin;

    const recipe &bandages = recipe_id( "bandages" ).obj();
    camp->queue_crafting_request( bandages, 5, listener, "craft 5 bandages",
                                  "bandages", listener.getID() );

    REQUIRE( camp->handle_heard_camp_request( listener, "job=1" ) );
    const std::vector<std::pair<std::string, std::string>> messages =
        Messages::recent_messages( 0 );
    REQUIRE_FALSE( messages.empty() );
    const std::string followup_reply = messages.back().second;
    CAPTURE( followup_reply );
    CHECK( followup_reply.find( "bandages" ) != std::string::npos );
    CHECK( followup_reply.find( "board=show_board" ) == std::string::npos );
    CHECK( followup_reply.find( "details=show_job=1" ) == std::string::npos );
    CHECK( followup_reply.find( "next=job=1" ) == std::string::npos );
    CHECK( followup_reply.find( "status=blocked" ) == std::string::npos );
    CHECK( followup_reply.find( "(#1)" ) == std::string::npos );
    CHECK( followup_reply.find( "Got it." ) == std::string::npos );

    Messages::clear_messages();
    overmap_buffer.clear_camps( origin.xy() );
    overmap_buffer.clear_mongroups();
  }

  SECTION("camp request router keeps structured next=delete_job follow-through on archived clears") {
    clear_avatar();
    clear_map_without_vision();
    Messages::clear_messages();

    const tripoint_abs_omt origin(1710, 1710, 0);
    overmap_buffer.clear_mongroups();
    for( int dy = -2; dy <= 2; ++dy ) {
        for( int dx = -2; dx <= 2; ++dx ) {
            overmap_buffer.ter_set(
                tripoint_abs_omt( origin.x() + dx, origin.y() + dy, origin.z() ),
                oter_id( "field" ) );
        }
    }

    get_map().add_camp( origin, "faction_camp", false );
    std::optional<basecamp *> found_camp = overmap_buffer.find_camp( origin.xy() );
    REQUIRE( found_camp.has_value() );
    REQUIRE( *found_camp != nullptr );
    basecamp *camp = *found_camp;

    npc &listener = spawn_npc( tripoint_bub_ms{ 7, 7, 0 }.xy(), "thug" );
    clear_character( listener, true );
    listener.assigned_camp = origin;

    const recipe &bandages = recipe_id( "bandages" ).obj();
    const int request_id = camp->queue_crafting_request( bandages, 2, listener,
                           "craft 2 bandages", "bandages", listener.getID() );
    REQUIRE( request_id == 1 );

    REQUIRE( camp->handle_heard_camp_request( listener, "cancel request #1" ) );
    Messages::clear_messages();

    REQUIRE( camp->handle_heard_camp_request( listener, "delete_job=1" ) );
    const std::vector<std::pair<std::string, std::string>> messages =
        Messages::recent_messages( 0 );
    REQUIRE_FALSE( messages.empty() );
    const std::string clear_reply = messages.back().second;
    CAPTURE( clear_reply );
    CHECK( clear_reply.find( camp_board_status_bark( {} ) ) != std::string::npos );
    CHECK( clear_reply.find( "board=show_board" ) == std::string::npos );
    CHECK( clear_reply.find( "job=1" ) == std::string::npos );
    CHECK( clear_reply.find( "active=0" ) == std::string::npos );
    CHECK( clear_reply.find( "Cleared old" ) == std::string::npos );

    Messages::clear_messages();
    overmap_buffer.clear_camps( origin.xy() );
    overmap_buffer.clear_mongroups();
  }

  SECTION( "camp job report debounce preserves first, changed, and typed exception reports" ) {
    clear_avatar();
    clear_map_without_vision();
    Messages::clear_messages();
    basecamp_ai::reset_camp_job_report_debounce();
    const time_point old_turn = calendar::turn;

    npc &worker = spawn_npc( tripoint_bub_ms{ 4, 4, 0 }.xy(), "thug" );
    clear_character( worker, true );
    worker.assigned_camp = tripoint_abs_omt( 1720, 1720, 0 );

    npc &other_worker = spawn_npc( tripoint_bub_ms{ 5, 4, 0 }.xy(), "thug" );
    clear_character( other_worker, true );
    other_worker.assigned_camp = worker.assigned_camp;

    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::completion, "activity=ACT_MULTIPLE_FARM" ) );
    CHECK_FALSE( basecamp_ai::should_show_camp_job_report(
                     worker, basecamp_ai::camp_job_report_kind::completion,
                     "activity=ACT_MULTIPLE_FARM" ) );
    calendar::turn += 31_minutes;
    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::completion,
               "activity=ACT_MULTIPLE_FARM" ) );
    CHECK_FALSE( basecamp_ai::should_show_camp_job_report(
                     worker, basecamp_ai::camp_job_report_kind::completion,
                     "activity=ACT_MULTIPLE_FARM" ) );
    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::completion,
               "activity=ACT_MULTIPLE_CHOP_PLANKS" ) );
    CHECK( basecamp_ai::should_show_camp_job_report(
               other_worker, basecamp_ai::camp_job_report_kind::completion,
               "activity=ACT_MULTIPLE_CHOP_PLANKS" ) );

    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::missing_tool,
               "request=7|blocked=missing hammer" ) );
    CHECK_FALSE( basecamp_ai::should_show_camp_job_report(
                     worker, basecamp_ai::camp_job_report_kind::missing_tool,
                     "request=7|blocked=missing hammer" ) );
    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::missing_tool,
               "request=7|blocked=missing wrench" ) );

    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::locker_exception,
               "no-locker-zone" ) );
    CHECK_FALSE( basecamp_ai::should_show_camp_job_report(
                     worker, basecamp_ai::camp_job_report_kind::locker_exception,
                     "no-locker-zone" ) );
    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::patrol_exception,
               "reserve-or-inactive" ) );
    CHECK_FALSE( basecamp_ai::should_show_camp_job_report(
                     worker, basecamp_ai::camp_job_report_kind::patrol_exception,
                     "reserve-or-inactive" ) );

    Messages::clear_messages();
    CHECK( basecamp_ai::should_show_camp_job_report(
               worker, basecamp_ai::camp_job_report_kind::no_progress,
               "launch-ready-empty" ) );
    add_msg( m_info, "camp repeat source" );
    CHECK_FALSE( basecamp_ai::should_show_camp_job_report(
                     worker, basecamp_ai::camp_job_report_kind::no_progress,
                     "launch-ready-empty" ) );
    add_msg( m_bad, "unrelated danger stays visible" );
    const std::vector<std::pair<std::string, std::string>> messages =
        Messages::recent_messages( 0 );
    REQUIRE( messages.size() >= 2 );
    CHECK( messages[messages.size() - 2].second == "camp repeat source" );
    CHECK( messages.back().second == "unrelated danger stays visible" );

    Messages::clear_messages();
    basecamp_ai::reset_camp_job_report_debounce();
    calendar::turn = old_turn;
  }

  SECTION("board handoff snapshot stays explicit when the board is empty") {
    CHECK(camp_board_handoff_snapshot({}) == "board=show_board\n"
                                             "active=0\n"
                                             "archived=0\n"
                                             "jobs=none\n");
  }

  SECTION("camp reply log packet fences the structured payload") {
    CHECK(basecamp_ai::format_camp_reply_log_packet("board", "show_board", "Ricky Broughton", 3,
                                                     "board=show_board\nactive=0\narchived=0\njobs=none\n") ==
          "camp board reply Ricky Broughton (3)\n"
          "heard=show_board\n"
          "reply_begin\n"
          "board=show_board\n"
          "active=0\n"
          "archived=0\n"
          "jobs=none\n"
          "reply_end");
    CHECK(basecamp_ai::format_camp_reply_log_packet("job", "show_job=7", "Ricky Broughton", 3,
                                                     "request_id=7") ==
          "camp job reply Ricky Broughton (3)\n"
          "heard=show_job=7\n"
          "reply_begin\n"
          "request_id=7\n"
          "reply_end");
  }

  SECTION("non craft requests do not emit craft handoff snapshots") {
    const camp_llm_request request = ([]() { camp_llm_request request; request.request_kind = "camp_upgrade"; request.request_id = 3; request.status = "awaiting_approval"; return request; }());

    CHECK(camp_request_handoff_snapshot(request).empty());
  }

  SECTION(
      "craft recall matching prefers the assigned worker id over the name") {
    camp_llm_request request;
    request.assigned_worker_npc_id = character_id(7);
    request.assigned_worker_name = "Alice";

    CHECK(matches_assigned_camp_request_worker(request, character_id(7),
                                               "Not Alice"));
    CHECK_FALSE(matches_assigned_camp_request_worker(request, character_id(8),
                                                     "Alice"));
  }

  SECTION("craft recall matching falls back to the worker name when legacy ids "
          "are missing") {
    camp_llm_request request;
    request.assigned_worker_name = "Alice";

    CHECK(
        matches_assigned_camp_request_worker(request, character_id(), "Alice"));
    CHECK_FALSE(
        matches_assigned_camp_request_worker(request, character_id(), "Bob"));
  }

  SECTION("overmap delta parser keeps signed dx and dy values") {
        point_rel_omt delta = point_rel_omt::zero;
        std::string error;

        CHECK( parse_relative_omt_delta( "4", "-2", delta, error ) );
        CHECK( delta == point_rel_omt( 4, -2 ) );
        CHECK( error.empty() );

        CHECK( parse_relative_omt_delta( " -1 ", " +3 ", delta, error ) );
        CHECK( delta == point_rel_omt( -1, 3 ) );
        CHECK( error.empty() );
    }

    SECTION( "overmap delta parser rejects malformed fields" ) {
        point_rel_omt delta = point_rel_omt( 9, 9 );
        std::string error;

        CHECK_FALSE( parse_relative_omt_delta( "east", "2", delta, error ) );
        CHECK( error == "Overmap delta dx is invalid." );
        CHECK( delta == point_rel_omt::zero );

        CHECK_FALSE( parse_relative_omt_delta( "2", "north", delta, error ) );
        CHECK( error == "Overmap delta dy is invalid." );
        CHECK( delta == point_rel_omt::zero );
    }

    SECTION( "overmap movement parser accepts stay and signed delta intents" ) {
        parsed_overmap_movement_intent intent;
        std::string error;

        CHECK( parse_overmap_movement_token( " stay ", intent, error ) );
        CHECK( intent.stay );
        CHECK( intent.delta == point_rel_omt::zero );
        CHECK( error.empty() );

        CHECK( parse_overmap_movement_token( "move_omt dx=4 dy=-2", intent, error ) );
        CHECK_FALSE( intent.stay );
    CHECK(intent.delta == point_rel_omt(4, -2));
    CHECK(error.empty());

    CHECK(parse_overmap_movement_token("move_omt   dx=-1    dy=+3", intent,
                                       error));
    CHECK_FALSE(intent.stay);
    CHECK(intent.delta == point_rel_omt(-1, 3));
    CHECK(error.empty());

        CHECK( parse_overmap_movement_token( "move_omt dy=+3 dx=-1", intent, error ) );
        CHECK_FALSE( intent.stay );
        CHECK( intent.delta == point_rel_omt( -1, 3 ) );
    CHECK(error.empty());
  }

  SECTION("overmap movement parser rejects malformed tokens with safe fallback "
          "state") {
    parsed_overmap_movement_intent intent;
    std::string error;

    CHECK_FALSE(parse_overmap_movement_token("move_omt 4 -2", intent, error));
    CHECK(error == "Overmap move token must use 'stay' or 'move_omt "
                   "dx=<signed_int> dy=<signed_int>'.");
    CHECK(intent.stay);
    CHECK(intent.delta == point_rel_omt::zero);

    CHECK_FALSE(
        parse_overmap_movement_token("move_omt dx=east dy=2", intent, error));
    CHECK(error == "Overmap delta dx is invalid.");
    CHECK(intent.stay);
    CHECK(intent.delta == point_rel_omt::zero);

    CHECK_FALSE(parse_overmap_movement_token("move_omt dx=2", intent, error));
    CHECK(error == "Overmap move token must use 'stay' or 'move_omt "
                   "dx=<signed_int> dy=<signed_int>'.");
    CHECK(intent.stay);
    CHECK(intent.delta == point_rel_omt::zero);

    CHECK_FALSE(
        parse_overmap_movement_token("move_omt dx=2 dy=3 dz=1", intent, error));
    CHECK(error == "Overmap move token must use 'stay' or 'move_omt "
                   "dx=<signed_int> dy=<signed_int>'.");
    CHECK(intent.stay);
    CHECK(intent.delta == point_rel_omt::zero);

    CHECK_FALSE(
        parse_overmap_movement_token("move_omt dx=2 dx=3 dy=1", intent, error));
    CHECK(error == "Overmap move token must use 'stay' or 'move_omt "
                   "dx=<signed_int> dy=<signed_int>'.");
    CHECK(intent.stay);
    CHECK(intent.delta == point_rel_omt::zero);
  }

  SECTION("overmap movement resolver keeps stay in place and uses the shared "
          "signed axis convention") {
    const tripoint_abs_omt origin(10, 20, 0);

    CHECK(resolve_overmap_movement_target(
              origin, parsed_overmap_movement_intent{}) == origin);

    parsed_overmap_movement_intent southeast;
    southeast.stay = false;
    southeast.delta = point_rel_omt(4, -2);
    CHECK(resolve_overmap_movement_target(origin, southeast) ==
          tripoint_abs_omt(14, 22, 0));

    parsed_overmap_movement_intent northwest;
    northwest.stay = false;
    northwest.delta = point_rel_omt(-1, 3);
    CHECK(resolve_overmap_movement_target(origin, northwest) ==
          tripoint_abs_omt(9, 17, 0));
  }

  SECTION(
      "overmap movement formatter emits shared stay and signed delta tokens") {
    const tripoint_abs_omt origin(10, 20, 0);
    std::string token;
    std::string error;

        CHECK( format_overmap_movement_token( origin, origin, token, error ) );
    CHECK(token == "stay");
    CHECK(error.empty());

    CHECK(format_overmap_movement_token(origin, tripoint_abs_omt(14, 22, 0),
                                        token, error));
    CHECK(token == "move_omt dx=+4 dy=-2");
    CHECK(error.empty());

    CHECK(format_overmap_movement_token(origin, tripoint_abs_omt(9, 17, 0),
                                        token, error));
    CHECK(token == "move_omt dx=-1 dy=+3");
    CHECK(error.empty());
  }

  SECTION("overmap movement formatter round-trips through the shared parser "
          "and resolver") {
    const tripoint_abs_omt origin(10, 20, 0);
    const tripoint_abs_omt target(14, 22, 0);
    std::string token;
        std::string error;
        parsed_overmap_movement_intent intent;

        REQUIRE( format_overmap_movement_token( origin, target, token, error ) );
        REQUIRE( parse_overmap_movement_token( token, intent, error ) );
    CHECK(resolve_overmap_movement_target(origin, intent) == target);
  }

  SECTION(
      "overmap movement formatter rejects z-level changes instead of lying") {
    std::string token = "move_omt dx=+9 dy=+9";
    std::string error;

    CHECK_FALSE(format_overmap_movement_token(tripoint_abs_omt(10, 20, 0),
                                              tripoint_abs_omt(10, 20, 1),
                                              token, error));
    CHECK(token.empty());
    CHECK(error == "Overmap move token cannot encode z-level changes.");
  }

  SECTION("overmap snapshot formatter builds a present-only legend and marks "
          "horde tiles in uppercase") {
    static const mongroup_id GROUP_ZOMBIE_HORDE("GROUP_ZOMBIE_HORDE");
    const tripoint_abs_omt origin(1500, 1500, 0);
    std::string snapshot;
        std::string error;

    overmap_buffer.clear_mongroups();
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        overmap_buffer.ter_set(
            tripoint_abs_omt(origin.x() + dx, origin.y() + dy, origin.z()),
            oter_id("field"));
      }
    }

    get_map().add_camp(origin, "faction_camp", false);
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x(), origin.y() - 1, origin.z()),
        oter_id("forest"));
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x() + 1, origin.y() - 1, origin.z()),
        oter_id("bridge_north"));
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x() - 1, origin.y(), origin.z()),
        oter_id("house_09_north"));
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x() + 1, origin.y(), origin.z()),
        oter_id("road_nesw"));
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x(), origin.y() + 1, origin.z()),
        oter_id("river_center"));
    overmap_buffer.ter_set(
        tripoint_abs_omt(origin.x() + 1, origin.y() + 1, origin.z()),
        oter_id("lake_shore"));
    overmap_buffer.spawn_mongroup(
        project_to<coords::sm>(tripoint_abs_omt(origin.x() + 1, origin.y(), 0)),
        GROUP_ZOMBIE_HORDE, 3);

    REQUIRE(format_overmap_snapshot(origin, 2, snapshot, error));
    CAPTURE(snapshot);
    CHECK(error.empty());
    CHECK(overmap_buffer.get_horde_size(
              tripoint_abs_omt(origin.x() + 1, origin.y(), 0)) > 0);
    CHECK(snapshot.find("      dx -2 -1 +0 +1 +2") != std::string::npos);
    const std::string dy_plus_1_line = find_snapshot_line(snapshot, "dy=+1");
    const std::string dy0_line = find_snapshot_line(snapshot, "dy=+0");
        const std::string dy_minus_1_line = find_snapshot_line( snapshot, "dy=-1" );
        CHECK_FALSE( dy_plus_1_line.empty() );
        CHECK_FALSE( dy0_line.empty() );
        CHECK_FALSE( dy_minus_1_line.empty() );
        CHECK( dy_plus_1_line.find( " t  b " ) != std::string::npos );
        CHECK( dy0_line.find( " h  c  R " ) != std::string::npos );
        CHECK( dy_minus_1_line.find( " w  n " ) != std::string::npos );
        CHECK( snapshot.find( "  b bridge\n" ) != std::string::npos );
        CHECK( snapshot.find( "  c camp\n" ) != std::string::npos );
        CHECK( snapshot.find( "  f field\n" ) != std::string::npos );
        CHECK( snapshot.find( "  h house\n" ) != std::string::npos );
        CHECK( snapshot.find( "  n riverbank/shore\n" ) != std::string::npos );
        CHECK( snapshot.find( "  r road\n" ) != std::string::npos );
        CHECK( snapshot.find( "  t forest\n" ) != std::string::npos );
        CHECK( snapshot.find( "  w water\n" ) != std::string::npos );
        CHECK( snapshot.find( "  uppercase = horde present\n" ) != std::string::npos );
        CHECK( snapshot.find( "  p point of interest\n" ) == std::string::npos );
        CHECK( snapshot.find( "  u urban\n" ) == std::string::npos );

        overmap_buffer.clear_camps( origin.xy() );
    overmap_buffer.clear_mongroups();
  }

  SECTION("overmap snapshot formatter rejects out-of-contract radii with safe "
          "fallback output") {
    std::string snapshot = "already here";
    std::string error;

    CHECK_FALSE(format_overmap_snapshot(tripoint_abs_omt(10, 20, 0), 0,
                                        snapshot, error));
    CHECK(snapshot.empty());
    CHECK(error == "Overmap snapshot radius must stay between 1 and 2 tiles.");
  }

  SECTION("craft router prefers specific phrase matches over generic noun "
          "matches") {
    const recipe &plain_bandages = recipe_id("bandages").obj();
    const recipe &boiled_bandages =
        recipe_id("bandages_makeshift_boiled").obj();

    CHECK(score_camp_recipe_query(boiled_bandages, "boiled bandages") >
          score_camp_recipe_query(plain_bandages, "boiled bandages"));
        CHECK( score_camp_recipe_query( plain_bandages, "bandages" ) >
          score_camp_recipe_query(boiled_bandages, "bandages"));
  }

  SECTION("craft router uses exact words instead of partial-word substring "
          "matches") {
    const recipe &plain_bandages = recipe_id("bandages").obj();
    CHECK(score_camp_recipe_query(plain_bandages, "band") == 0);
    CHECK(score_camp_recipe_query(plain_bandages, "bandages") > 0);
  }

  SECTION("craft router can singularize the final plural query word as a "
          "fallback") {
    const recipe &hunting_knife = recipe_id("knife_hunting").obj();
    CHECK(score_camp_recipe_query(hunting_knife, "hunting knifes") >= 650);

    const std::unordered_set<recipe_id> recipes = {recipe_id("knife_hunting")};
    const basecamp_ai::camp_craft_recipe_match match =
        match_camp_craft_query(recipes, "hunting knifes");
    CHECK(match.score >= 650);
    CHECK(match.fallback_query == "hunting knife");
    CHECK_FALSE(match.score_notes.empty());
  }

  SECTION("craft router reports ambiguous top subject matches instead of "
          "guessing") {
    const std::unordered_set<recipe_id> recipes = {
        recipe_id("bandages_makeshift_boiled"), recipe_id("potato_boiled")};

    const basecamp_ai::camp_craft_recipe_match match =
        match_camp_craft_query(recipes, "boiled");
    CHECK(match.score >= 650);
    CHECK(match.recipe_ids.size() == 2);
    CHECK(match.subjects == std::vector<std::string>{"boiled makeshift bandage",
                                                     "boiled potato"});

    const basecamp_ai::camp_craft_resolution resolution =
        resolve_camp_craft_query(recipes, "boiled", [](const recipe &) {
          return basecamp_ai::camp_craft_recipe_candidate{};
        });
    CHECK(resolution.outcome == camp_craft_resolution_outcome::MATCH_AMBIGUOUS);
    }

  SECTION("shared craft resolver keeps blocked matches and their blockers") {
    const std::unordered_set<recipe_id> recipes = {recipe_id("bandages")};

    const basecamp_ai::camp_craft_resolution resolution =
        resolve_camp_craft_query(recipes, "bandages", [](const recipe &) {
          basecamp_ai::camp_craft_recipe_candidate candidate;
          candidate.blockers = {
              "No stationed worker can take this recipe right now."};
          return candidate;
        });

        CHECK( resolution.match.score >= 650 );
        REQUIRE( resolution.choice.has_value() );
        CHECK( resolution.choice->recipe == recipe_id( "bandages" ) );
    CHECK(resolution.choice->candidate.worker == nullptr);
    CHECK(resolution.outcome == camp_craft_resolution_outcome::MATCH_BLOCKED);
    CHECK(resolution.choice->candidate.blockers ==
          std::vector<std::string>{
              "No stationed worker can take this recipe right now."});
  }

  SECTION("shared craft resolver reports no match explicitly") {
    const std::unordered_set<recipe_id> recipes = {recipe_id("bandages")};

    const basecamp_ai::camp_craft_resolution resolution =
        resolve_camp_craft_query(recipes, "quantum soup", [](const recipe &) {
          return basecamp_ai::camp_craft_recipe_candidate{};
        });

        CHECK( resolution.outcome == camp_craft_resolution_outcome::NO_MATCH );
        CHECK_FALSE( resolution.choice.has_value() );
        CHECK( resolution.match.recipe_ids.empty() );
  }
}

// TODO: Tests for: Check calorie display at various activity levels, camp
// crafting works as expected (consumes inputs, returns outputs+byproducts,
// costs calories)

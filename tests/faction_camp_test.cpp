#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

#include "basecamp.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "clzones.h"
#include "coordinates.h"
#include "faction.h"
#include "game.h"
#include "item.h"
#include "item_components.h"
#include "itype.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "messages.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "point.h"
#include "stomach.h"
#include "type_id.h"
#include "value_ptr.h"

static const itype_id itype_9mm("9mm");
static const itype_id itype_armor_lc_plate("armor_lc_plate");
static const itype_id itype_backpack("backpack");
static const itype_id itype_boots("boots");
static const itype_id itype_briefs("briefs");
static const itype_id itype_daypack("daypack");
static const itype_id itype_duffelbag("duffelbag");
static const itype_id itype_glasses_eye("glasses_eye");
static const itype_id itype_glock_19("glock_19");
static const itype_id itype_glockmag("glockmag");
static const itype_id itype_helmet_bike("helmet_bike");
static const itype_id itype_jeans("jeans");
static const itype_id itype_knife_combat("knife_combat");
static const itype_id itype_pants_cargo("pants_cargo");
static const itype_id itype_sneakers("sneakers");
static const itype_id itype_socks("socks");
static const itype_id itype_test_100_kcal("test_100_kcal");
static const itype_id itype_test_200_kcal("test_200_kcal");
static const itype_id itype_test_500_kcal("test_500_kcal");
static const itype_id itype_tshirt("tshirt");
static const itype_id itype_vest("vest");

static const vitamin_id vitamin_mutagen("mutagen");
static const vitamin_id vitamin_mutant_toxin("mutant_toxin");

static const zone_type_id zone_type_CAMP_FOOD("CAMP_FOOD");
static const zone_type_id zone_type_CAMP_STORAGE("CAMP_STORAGE");
static const zone_type_id zone_type_CAMP_LOCKER("CAMP_LOCKER");

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

TEST_CASE("camp_locker_policy_serialization", "[camp][locker]") {
  REQUIRE(zone_manager::get_manager().has_type(zone_type_CAMP_LOCKER));

  basecamp test_camp("Locker Camp", tripoint_abs_omt(8, 8, 0));
  CHECK(test_camp.get_locker_policy().enabled_count() ==
        static_cast<int>(all_camp_locker_slots().size()));

  test_camp.set_locker_slot_enabled(camp_locker_slot::socks, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::bag, false);
  test_camp.set_locker_slot_enabled(camp_locker_slot::ranged_weapon, false);

  std::ostringstream os;
  JsonOut jsout(os);
  test_camp.serialize(jsout);

  JsonValue jsin = json_loader::from_string(os.str());
  JsonObject jo = jsin.get_object();
  basecamp loaded;
  loaded.deserialize(jo);

  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::socks));
  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::bag));
  CHECK_FALSE(loaded.is_locker_slot_enabled(camp_locker_slot::ranged_weapon));
  CHECK(loaded.is_locker_slot_enabled(camp_locker_slot::pants));
  CHECK(loaded.get_locker_policy().enabled_count() ==
        static_cast<int>(all_camp_locker_slots().size()) - 3);
}

TEST_CASE("camp_locker_item_classification", "[camp][locker]") {
  CHECK(classify_camp_locker_item(item(itype_briefs)) ==
        camp_locker_slot::underwear);
  CHECK(classify_camp_locker_item(item(itype_socks)) ==
        camp_locker_slot::socks);
  CHECK(classify_camp_locker_item(item(itype_sneakers)) ==
        camp_locker_slot::shoes);
  CHECK(classify_camp_locker_item(item(itype_jeans)) ==
        camp_locker_slot::pants);
  CHECK(classify_camp_locker_item(item(itype_tshirt)) ==
        camp_locker_slot::shirt);
  CHECK(classify_camp_locker_item(item(itype_vest)) == camp_locker_slot::vest);
  CHECK(classify_camp_locker_item(item(itype_armor_lc_plate)) ==
        camp_locker_slot::body_armor);
  CHECK(classify_camp_locker_item(item(itype_helmet_bike)) ==
        camp_locker_slot::helmet);
  CHECK(classify_camp_locker_item(item(itype_glasses_eye)) ==
        camp_locker_slot::glasses);
  CHECK(classify_camp_locker_item(item(itype_backpack)) ==
        camp_locker_slot::bag);
  CHECK(classify_camp_locker_item(item(itype_knife_combat)) ==
        camp_locker_slot::melee_weapon);
  CHECK(classify_camp_locker_item(item(itype_glock_19)) ==
        camp_locker_slot::ranged_weapon);
}

TEST_CASE("camp_locker_zone_candidate_gathering", "[camp][locker]") {
  clear_avatar();
  clear_map_without_vision();
  zone_manager::get_manager().clear();

  map &here = get_map();
  const tripoint_abs_ms locker_abs = here.get_abs(tripoint_bub_ms{5, 5, 0});
  const tripoint_abs_ms offzone_abs = here.get_abs(tripoint_bub_ms{6, 5, 0});
  const tripoint_bub_ms locker_local = here.get_bub(locker_abs);
  const tripoint_bub_ms offzone_local = here.get_bub(offzone_abs);

  create_tile_zone("Locker", zone_type_CAMP_LOCKER, locker_abs);
  here.i_clear(locker_local);
  here.i_clear(offzone_local);
  here.add_item_or_charges(locker_local, item(itype_sneakers));
  here.add_item_or_charges(locker_local, item(itype_backpack));
  here.add_item_or_charges(locker_local, item(itype_glock_19));
  here.add_item_or_charges(offzone_local, item(itype_helmet_bike));

  basecamp test_camp("Locker Camp", project_to<coords::omt>(locker_abs));
  test_camp.set_locker_slot_enabled(camp_locker_slot::bag, false);

  const camp_locker_candidate_map candidates =
      collect_camp_locker_zone_candidates(locker_abs, your_fac,
                                          test_camp.get_locker_policy());

  REQUIRE(candidates.count(camp_locker_slot::shoes) == 1);
  REQUIRE(candidates.at(camp_locker_slot::shoes).size() == 1);
  CHECK(candidates.at(camp_locker_slot::shoes).front()->typeId() ==
        itype_sneakers);

  REQUIRE(candidates.count(camp_locker_slot::ranged_weapon) == 1);
  REQUIRE(candidates.at(camp_locker_slot::ranged_weapon).size() == 1);
  CHECK(candidates.at(camp_locker_slot::ranged_weapon).front()->typeId() ==
        itype_glock_19);

  CHECK(candidates.count(camp_locker_slot::bag) == 0);
  CHECK(candidates.count(camp_locker_slot::helmet) == 0);

  zone_manager::get_manager().clear();
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
            camp_llm_request{ .request_id = 2, .status = "awaiting_approval" },
            camp_llm_request{ .request_id = 4, .status = "blocked" },
        camp_llm_request{.request_id = 5, .status = "in_progress"},
        camp_llm_request{.request_id = 7, .status = "awaiting_approval"},
        camp_llm_request{.request_id = 9, .status = "completed"},
        camp_llm_request{.request_id = 11, .status = "cancelled"}};

    CHECK(collect_ready_camp_request_ids(requests) == std::vector<int>{2, 7});
  }

    SECTION( "blocked-request collector excludes ready live work and archives" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 2, .status = "awaiting_approval" },
            camp_llm_request{ .request_id = 4, .status = "blocked" },
        camp_llm_request{.request_id = 5, .status = "in_progress"},
        camp_llm_request{.request_id = 7, .status = "blocked"},
        camp_llm_request{.request_id = 9, .status = "completed"},
        camp_llm_request{.request_id = 11, .status = "cancelled"}};

    CHECK(collect_blocked_camp_request_ids(requests) == std::vector<int>{4, 7});
  }

  SECTION(
      "archived-request collector keeps completed and cancelled work only") {
    const std::vector<camp_llm_request> requests = {
        camp_llm_request{.request_id = 2, .status = "awaiting_approval"},
        camp_llm_request{.request_id = 4, .status = "blocked"},
        camp_llm_request{.request_id = 5, .status = "in_progress"},
        camp_llm_request{.request_id = 9, .status = "completed"},
        camp_llm_request{.request_id = 11, .status = "cancelled"}};

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
        camp_llm_request{.request_id = 7,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "awaiting_approval"},
        camp_llm_request{.request_id = 12,
                         .requested_item_query = "knife",
                         .chosen_recipe_name = "knife",
                         .status = "cancelled"}};
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
        camp_llm_request{.request_id = 3,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "awaiting_approval",
                         .assigned_worker_name = "Alice"},
        camp_llm_request{.request_id = 4,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "blocked",
                         .assigned_worker_name = "Bob"},
        camp_llm_request{.request_id = 5,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "cancelled",
                         .assigned_worker_name = "Cara"}};
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
        camp_llm_request{.request_id = 3,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "awaiting_approval",
                         .assigned_worker_name = "Alice"},
        camp_llm_request{.request_id = 4,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "cancelled",
                         .assigned_worker_name = "Bob"},
        camp_llm_request{.request_id = 5,
                         .requested_item_query = "knife",
                         .chosen_recipe_name = "knife",
                         .status = "completed",
                         .assigned_worker_name = "Cara"}};
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
        camp_llm_request{.request_id = 3,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "awaiting_approval",
                         .assigned_worker_name = "Alice"},
        camp_llm_request{.request_id = 4,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "awaiting_approval",
                         .assigned_worker_name = "Bob"}};
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
        camp_llm_request{.request_id = 3,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "awaiting_approval",
                         .approval_state = "waiting_player",
                         .assigned_worker_name = "Alice"},
        camp_llm_request{.request_id = 4,
                         .requested_item_query = "bandages",
                         .chosen_recipe_name = "bandages",
                         .status = "blocked",
                         .assigned_worker_name = "Bob"}};
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
        camp_llm_request{.request_id = 8,
                         .requested_item_query = "heavy cable",
                         .chosen_recipe_name = "heavy cable",
                         .source_utterance = "please craft me a long string",
                         .status = "awaiting_approval"}};
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
    const camp_llm_request request{.requested_item_query = "bandages",
                                   .requested_count = 5,
                                   .chosen_recipe_name = "sterile bandage"};

    CHECK(camp_request_subject_for_display(request) == "5 × bandages");
  }

  SECTION(
      "request display subject falls back to the resolved recipe when needed") {
    const camp_llm_request request{
        .requested_count = 2, .chosen_recipe_name = "boiled makeshift bandage"};

    CHECK(camp_request_subject_for_display(request) ==
          "2 × boiled makeshift bandage");
  }

  SECTION("request summary subject can append the resolved recipe when it "
          "differs") {
    const camp_llm_request request{.requested_item_query = "bandages",
                                   .requested_count = 5,
                                   .chosen_recipe_name = "sterile bandage"};

    CHECK(camp_request_subject_for_display(request, true) ==
          "5 × bandages (matched sterile bandage)");
  }

  SECTION("request summary subject does not duplicate the resolved recipe when "
          "it matches") {
    const camp_llm_request request{.requested_item_query = "bandages",
                                   .requested_count = 5,
                                   .chosen_recipe_name = "bandages"};

    CHECK(camp_request_subject_for_display(request, true) == "5 × bandages");
  }

    SECTION( "request display subject falls back to a generic label when empty" ) {
        const camp_llm_request request;

    CHECK(camp_request_subject_for_display(request) == "crafting request");
  }

  SECTION("request display subject can keep the request number as trailing "
          "detail") {
    const camp_llm_request request{.request_id = 7,
                                   .requested_item_query = "bandages",
                                   .requested_count = 5,
                                   .chosen_recipe_name = "sterile bandage"};

    CHECK(camp_request_subject_for_display(request, false, true) ==
          "5 × bandages (#7)");
    CHECK(camp_request_subject_for_display(request, true, true) ==
          "5 × bandages (matched sterile bandage) (#7)");
  }

  SECTION("request display subject falls back to a generic numbered label when "
          "empty") {
    const camp_llm_request request{.request_id = 9};

    CHECK(camp_request_subject_for_display(request, false, true) ==
          "crafting request (#9)");
  }

  SECTION("spoken status bark keeps the request id trailing instead of leading "
          "with it") {
    const camp_llm_request request{.request_id = 7,
                                   .requested_item_query = "bandages",
                                   .requested_count = 5,
                                   .status = "awaiting_approval",
                                   .approval_state = "waiting_player"};

    CHECK(camp_request_spoken_status_bark(request) ==
          "5 × bandages (#7) is pinned, waiting on your go-ahead.");
  }

  SECTION(
      "spoken status bark keeps matched recipe detail on blocked requests") {
    const camp_llm_request request{
        .request_id = 7,
        .requested_item_query = "bandages",
        .requested_count = 5,
        .chosen_recipe_name = "sterile bandage",
        .status = "blocked",
        .blockers = {
            "Camp storage could not supply the needed ingredients or tools."}};

    CHECK(camp_request_spoken_status_bark(request) ==
          "5 × bandages (matched sterile bandage) (#7) is blocked — Camp "
          "storage could not supply the needed ingredients or tools.");
  }

  SECTION("board status bark stays concise and human-facing") {
    const std::vector<camp_llm_request> requests = {
        camp_llm_request{.request_id = 7,
                         .requested_item_query = "bandages",
                         .requested_count = 5,
                         .status = "blocked"},
        camp_llm_request{.request_id = 8,
                         .requested_item_query = "knife",
                         .status = "in_progress",
                         .assigned_worker_name = "Bruna"},
        camp_llm_request{.request_id = 9,
                         .requested_item_query = "bandages",
                         .requested_count = 2,
                         .status = "completed"}};

    CHECK(camp_board_status_bark(requests) ==
          "Board's got 2 live and 1 old — 5 × bandages, knife.");
  }

  SECTION("craft request handoff snapshot keeps stable request facts plus "
          "board/detail/next tokens") {
    const camp_llm_request request{
        .request_id = 7,
        .source_utterance = "craft 5 bandages",
            .requested_item_query = "bandages",
            .requested_count = 5,
        .chosen_recipe_name = "sterile bandage",
        .status = "blocked",
        .approval_state = "not_needed",
        .blockers = {"No stationed worker can take this recipe right now."}};

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
    const camp_llm_request request{.request_id = 9,
                                   .requested_item_query = "bandages",
                                   .requested_count = 2,
                                   .chosen_recipe_name = "bandages",
                                   .status = "completed",
                                   .approval_state = "approved",
                                   .assigned_worker_name = "Bruna"};

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
        camp_llm_request{.request_id = 7,
                         .requested_item_query = "bandages",
                         .requested_count = 5,
                         .chosen_recipe_name = "sterile bandage",
                         .status = "blocked",
                         .approval_state = "not_needed"},
        camp_llm_request{.request_id = 8,
                         .requested_item_query = "knife",
                         .chosen_recipe_name = "knife",
                         .status = "in_progress",
                         .approval_state = "approved",
                         .assigned_worker_name = "Bruna"},
        camp_llm_request{.request_id = 9,
                         .requested_item_query = "bandages",
                         .requested_count = 2,
                         .chosen_recipe_name = "bandages",
                         .status = "completed",
                         .approval_state = "approved",
                         .assigned_worker_name = "Cara"}};

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
        camp_llm_request{.request_id = 7,
                         .requested_item_query = "bandages",
                         .requested_count = 5,
                         .chosen_recipe_name = "sterile bandage",
                         .status = "blocked",
                         .approval_state = "not_needed"},
        camp_llm_request{.request_id = 9,
                         .requested_item_query = "bandages",
                         .requested_count = 2,
                         .chosen_recipe_name = "bandages",
                         .status = "completed",
                         .approval_state = "approved",
                         .assigned_worker_name = "Cara"}};

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
        camp_llm_request{.request_id = 7,
                         .requested_item_query = "bandages",
                         .requested_count = 5,
                         .chosen_recipe_name = "sterile bandage",
                         .status = "blocked",
                         .approval_state = "not_needed"},
        camp_llm_request{.request_id = 9,
                         .requested_item_query = "bandages",
                         .requested_count = 2,
                         .chosen_recipe_name = "bandages",
                         .status = "completed",
                         .approval_state = "approved",
                         .assigned_worker_name = "Cara"}};

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
    CAPTURE(structured_reply);
    CHECK(structured_reply.find("board=show_board\nplanner_move=stay | move_omt dx=<signed_int> dy=<signed_int>\novermap:\n") !=
          std::string::npos);
    CHECK(structured_reply.find("  c camp\n") != std::string::npos);
    CHECK(structured_reply.find("  uppercase = horde present\n") !=
          std::string::npos);
    CHECK(structured_reply.find("active=1\narchived=0\n") !=
          std::string::npos);
    CHECK(structured_reply.find("job=1 subject=5 × bandages") !=
          std::string::npos);
    CHECK(structured_reply.find("status=awaiting_approval") !=
          std::string::npos);
    CHECK(structured_reply.find("approval=waiting_player worker=") !=
          std::string::npos);
    CHECK(structured_reply.find("details=show_job=1 next=job=1") !=
          std::string::npos);

    Messages::clear_messages();
    REQUIRE(camp->handle_heard_camp_request(listener, "show me the board"));
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
    CHECK( followup_reply.find( "board=show_board\ndetails=show_job=1\nid=1\n" ) != std::string::npos );
    CHECK( followup_reply.find( "query=bandages\n" ) != std::string::npos );
    CHECK( followup_reply.find( "source=craft 5 bandages\n" ) != std::string::npos );
    CHECK( followup_reply.find( "next=job=1\n" ) != std::string::npos );
    CHECK( followup_reply.find( "planner_move=" ) == std::string::npos );
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
    CHECK( clear_reply.find( "board=show_board\nplanner_move=stay | move_omt dx=<signed_int> dy=<signed_int>\novermap:\n" ) !=
           std::string::npos );
    CHECK( clear_reply.find( "active=0\narchived=0\njobs=none\n" ) != std::string::npos );
    CHECK( clear_reply.find( "job=1" ) == std::string::npos );
    CHECK( clear_reply.find( "Cleared old" ) == std::string::npos );

    Messages::clear_messages();
    overmap_buffer.clear_camps( origin.xy() );
    overmap_buffer.clear_mongroups();
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
    const camp_llm_request request{.request_kind = "camp_upgrade",
                                   .request_id = 3,
                                   .status = "awaiting_approval"};

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
        CHECK( resolution.choice->recipe_id == recipe_id( "bandages" ) );
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

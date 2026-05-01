#include "basecamp.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "avatar.h"
#include "build_reqs.h"
#include "calendar.h"
#include "cata_assert.h"
#include "cata_utility.h"
#include "character.h"
#include "character_id.h"
#include "clzones.h"
#include "color.h"
#include "crafting.h"
#include "debug.h"
#include "event.h"
#include "event_bus.h"
#include "faction.h"
#include "faction_camp.h"
#include "flag.h"
#include "game.h"
#include "input_popup.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"
#include "iuse_actor.h"
#include "llm_intent.h"
#include "map.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "mapdata.h"
#include "messages.h"
#include "npc.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "recipe_groups.h"
#include "requirements.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"

static const flag_id json_flag_PSEUDO("PSEUDO");

static const zone_type_id zone_type_CAMP_STORAGE("CAMP_STORAGE");
static const zone_type_id zone_type_CAMP_LOCKER("CAMP_LOCKER");
static const zone_type_id zone_type_CAMP_PATROL("CAMP_PATROL");
static const zone_type_id zone_type_NO_NPC_PICKUP("NO_NPC_PICKUP");

static const activity_id ACT_CAMP_PATROL("ACT_CAMP_PATROL");

static const damage_type_id damage_bash("bash");
static const damage_type_id damage_bullet("bullet");
static const damage_type_id damage_cut("cut");

namespace {

bool tripoint_abs_ms_zyx_less(const tripoint_abs_ms &lhs,
                              const tripoint_abs_ms &rhs) {
    if( lhs.z() != rhs.z() ) {
        return lhs.z() < rhs.z();
    }
    if( lhs.y() != rhs.y() ) {
        return lhs.y() < rhs.y();
    }
    return lhs.x() < rhs.x();
}

void sort_tripoints_zyx(std::vector<tripoint_abs_ms> &tiles) {
    std::sort(tiles.begin(), tiles.end(), tripoint_abs_ms_zyx_less);
}

std::string summarize_camp_patrol_cluster_tiles(
    const std::vector<camp_patrol_cluster> &clusters ) {
    std::ostringstream summary;
    for( size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index ) {
        if( cluster_index > 0 ) {
            summary << "; ";
        }
        summary << cluster_index << "=[";
        const camp_patrol_cluster &cluster = clusters[cluster_index];
        for( size_t tile_index = 0; tile_index < cluster.size(); ++tile_index ) {
            if( tile_index > 0 ) {
                summary << ", ";
            }
            summary << cluster[tile_index].to_string_writable();
        }
        summary << "]";
    }
    return summary.str();
}

const char *camp_patrol_shift_name( const camp_patrol_shift shift ) {
    switch( shift ) {
        case camp_patrol_shift::day:
            return "day";
        case camp_patrol_shift::night:
            return "night";
    }
    return "unknown";
}

} // namespace

camp_locker_policy::camp_locker_policy() { enabled_slots.fill(true); }

bool camp_locker_policy::is_enabled(camp_locker_slot slot) const {
  return enabled_slots[static_cast<size_t>(slot)];
}

void camp_locker_policy::set_enabled(camp_locker_slot slot, bool enabled) {
  enabled_slots[static_cast<size_t>(slot)] = enabled;
}

bool camp_locker_policy::prefers_bulletproof() const {
  return prefer_bulletproof;
}

void camp_locker_policy::set_prefers_bulletproof( bool enabled ) {
  prefer_bulletproof = enabled;
}

int camp_locker_policy::enabled_count() const {
  return static_cast<int>(
      std::count(enabled_slots.begin(), enabled_slots.end(), true));
}

std::vector<camp_locker_slot> all_camp_locker_slots() {
  return {
      camp_locker_slot::underwear,    camp_locker_slot::socks,
      camp_locker_slot::gloves,       camp_locker_slot::shoes,
      camp_locker_slot::pants,        camp_locker_slot::shirt,
      camp_locker_slot::vest,         camp_locker_slot::body_armor,
      camp_locker_slot::helmet,       camp_locker_slot::glasses,
      camp_locker_slot::mask,         camp_locker_slot::belt,
      camp_locker_slot::holster,      camp_locker_slot::bag,
      camp_locker_slot::melee_weapon, camp_locker_slot::ranged_weapon,
  };
}

std::string camp_locker_slot_id(camp_locker_slot slot) {
  switch (slot) {
  case camp_locker_slot::underwear:
    return "underwear";
  case camp_locker_slot::socks:
    return "socks";
  case camp_locker_slot::gloves:
    return "gloves";
  case camp_locker_slot::shoes:
    return "shoes";
  case camp_locker_slot::pants:
    return "pants";
  case camp_locker_slot::shirt:
    return "shirt";
  case camp_locker_slot::vest:
    return "vest";
  case camp_locker_slot::body_armor:
    return "body_armor";
  case camp_locker_slot::helmet:
    return "helmet";
  case camp_locker_slot::glasses:
    return "glasses";
  case camp_locker_slot::mask:
    return "mask";
  case camp_locker_slot::belt:
    return "belt";
  case camp_locker_slot::holster:
    return "holster";
  case camp_locker_slot::bag:
    return "bag";
  case camp_locker_slot::melee_weapon:
    return "melee_weapon";
  case camp_locker_slot::ranged_weapon:
    return "ranged_weapon";
  case camp_locker_slot::num_slots:
    break;
  }
  return "unknown";
}

translation camp_locker_slot_name(camp_locker_slot slot) {
  switch (slot) {
  case camp_locker_slot::underwear:
    return to_translation("Locker slot", "Underwear");
  case camp_locker_slot::socks:
    return to_translation("Locker slot", "Socks");
  case camp_locker_slot::gloves:
    return to_translation("Locker slot", "Gloves");
  case camp_locker_slot::shoes:
    return to_translation("Locker slot", "Shoes");
  case camp_locker_slot::pants:
    return to_translation("Locker slot", "Pants");
  case camp_locker_slot::shirt:
    return to_translation("Locker slot", "Shirt");
  case camp_locker_slot::vest:
    return to_translation("Locker slot", "Vest");
  case camp_locker_slot::body_armor:
    return to_translation("Locker slot", "Body armor");
  case camp_locker_slot::helmet:
    return to_translation("Locker slot", "Helmet");
  case camp_locker_slot::glasses:
    return to_translation("Locker slot", "Glasses");
  case camp_locker_slot::mask:
    return to_translation("Locker slot", "Mask");
  case camp_locker_slot::belt:
    return to_translation("Locker slot", "Belt");
  case camp_locker_slot::holster:
    return to_translation("Locker slot", "Holster");
  case camp_locker_slot::bag:
    return to_translation("Locker slot", "Bag");
  case camp_locker_slot::melee_weapon:
    return to_translation("Locker slot", "Melee weapon");
  case camp_locker_slot::ranged_weapon:
    return to_translation("Locker slot", "Ranged weapon");
  case camp_locker_slot::num_slots:
    break;
  }
  return translation();
}

std::optional<camp_locker_slot> camp_locker_slot_from_id(std::string_view id) {
  for (const camp_locker_slot slot : all_camp_locker_slots()) {
    if (camp_locker_slot_id(slot) == id) {
      return slot;
    }
  }
  return std::nullopt;
}

bool camp_locker_slot_plan::has_changes() const {
  return selected_candidate != nullptr || !duplicate_current.empty();
}

namespace {
bool armor_covers_any(const item &it,
                      const std::initializer_list<std::string_view> &parts) {
  for (const std::string_view part : parts) {
    const bodypart_str_id bodypart(part);
    if (bodypart.is_valid() && it.covers(bodypart.id(), false)) {
      return true;
    }
  }
  return false;
}

bool armor_specifically_covers_any(
    const item &it, const std::initializer_list<std::string_view> &parts) {
  for (const std::string_view part : parts) {
    const sub_bodypart_str_id subpart(part);
    if (subpart.is_valid() && it.covers(subpart.id(), false)) {
      return true;
    }
  }
  return false;
}
} // namespace

namespace {
int average_armor_portion_stat(const item &it,
                               int armor_portion_data::*member) {
  const islot_armor *armor = it.find_armor_data();
  if (armor == nullptr || armor->data.empty()) {
    return 0;
  }

  int total = 0;
  for (const armor_portion_data &portion : armor->data) {
    total += portion.*member;
  }
  return total / static_cast<int>(armor->data.size());
}

int average_armor_encumber(const item &it, const Character *fit_context) {
  if (fit_context != nullptr) {
    return it.get_avg_encumber(*fit_context, item::encumber_flags::assume_empty);
  }
  return average_armor_portion_stat(it, &armor_portion_data::encumber);
}

int average_armor_coverage(const item &it) {
  return it.get_avg_coverage();
}

units::volume utility_storage_capacity(const item &it) {
  return it.get_volume_capacity([](const item_pocket &pocket) {
    return item_pocket::ok_default_containers(pocket) && !pocket.is_ablative();
  });
}

int storage_score(const item &it, int milliliters_per_point) {
  if (milliliters_per_point <= 0) {
    return 0;
  }
  return units::to_milliliter(utility_storage_capacity(it)) /
         milliliters_per_point;
}

int protection_score(const item &it, int bash_weight, int cut_weight,
                     int bullet_weight) {
  return static_cast<int>(it.resist(damage_bash)) * bash_weight +
         static_cast<int>(it.resist(damage_cut)) * cut_weight +
         static_cast<int>(it.resist(damage_bullet)) * bullet_weight;
}

int generic_clothing_score(const item &it, int storage_divisor,
                           int encumbrance_weight,
                           const Character *fit_context) {
  return average_armor_coverage(it) * 3 + protection_score(it, 4, 6, 8) +
         it.get_env_resist() * 2 + it.get_warmth() +
         storage_score(it, storage_divisor) -
         average_armor_encumber(it, fit_context) * encumbrance_weight;
}

int bag_score(const item &it, const Character *fit_context) {
  return storage_score(it, 25) + protection_score(it, 2, 2, 2) +
         average_armor_coverage(it) + it.get_env_resist() -
         average_armor_encumber(it, fit_context) * 3;
}

int ablative_armor_score(const item &it, const Character *fit_context) {
  int score = 0;
  for (const item *ablative : it.all_ablative_armor()) {
    if (ablative == nullptr) {
      continue;
    }
    score += average_armor_coverage(*ablative) * 2 +
             protection_score(*ablative, 6, 8, 12) -
             average_armor_encumber(*ablative, fit_context) * 2;
  }
  return score;
}

int total_ballistic_resist_score(const item &it) {
  int score = static_cast<int>(it.resist(damage_bullet));
  for (const item *ablative : it.all_ablative_armor()) {
    if (ablative == nullptr) {
      continue;
    }
    score += static_cast<int>(ablative->resist(damage_bullet));
  }
  return score;
}

int body_armor_score(const item &it, const Character *fit_context) {
  return average_armor_coverage(it) * 4 + protection_score(it, 10, 12, 16) +
         ablative_armor_score(it, fit_context) + it.get_env_resist() * 2 +
         it.get_warmth() - average_armor_encumber(it, fit_context) * 6;
}

int helmet_score(const item &it, const Character *fit_context) {
  return average_armor_coverage(it) * 4 + protection_score(it, 8, 10, 14) +
         it.get_env_resist() * 2 + it.get_warmth() -
         average_armor_encumber(it, fit_context) * 4;
}

int glasses_score(const item &it, const Character *fit_context) {
  return average_armor_coverage(it) * 2 + protection_score(it, 1, 2, 2) +
         it.get_env_resist() * 8 - average_armor_encumber(it, fit_context) * 2;
}

int character_weapon_score(const item &it, const Character *fit_context) {
  return fit_context == nullptr ? 0 :
         static_cast<int>(fit_context->evaluate_weapon(it, true) * 100.0);
}

int melee_weapon_score(const item &it, const Character *fit_context) {
  if (fit_context != nullptr) {
    return character_weapon_score(it, fit_context);
  }
  return static_cast<int>(it.base_damage_melee().total_damage() * 20.0f) -
         units::to_gram(it.weight()) / 100 -
         units::to_milliliter(it.volume()) / 250;
}

int ranged_weapon_score(const item &it, const Character *fit_context) {
  if (fit_context != nullptr) {
    return character_weapon_score(it, fit_context);
  }
  return static_cast<int>(it.gun_damage().total_damage() * 20.0f) -
         units::to_gram(it.weight()) / 250 -
         units::to_milliliter(it.volume()) / 250;
}

int camp_locker_upgrade_threshold(camp_locker_slot slot) {
  switch (slot) {
  case camp_locker_slot::underwear:
  case camp_locker_slot::socks:
  case camp_locker_slot::gloves:
    return 20;
  case camp_locker_slot::shoes:
  case camp_locker_slot::pants:
  case camp_locker_slot::shirt:
  case camp_locker_slot::helmet:
  case camp_locker_slot::mask:
    return 25;
  case camp_locker_slot::vest:
  case camp_locker_slot::body_armor:
  case camp_locker_slot::belt:
  case camp_locker_slot::holster:
    return 35;
  case camp_locker_slot::glasses:
    return 15;
  case camp_locker_slot::bag:
    return 120;
  case camp_locker_slot::melee_weapon:
    return 20;
  case camp_locker_slot::ranged_weapon:
    return 30;
  case camp_locker_slot::num_slots:
    break;
  }
  return 25;
}

bool is_better_scored_locker_item(
    camp_locker_slot slot, const item &lhs, const item &rhs,
    const camp_locker_policy &policy,
    const std::optional<units::temperature> &local_temperature = std::nullopt,
    bool wet_weather = false, const Character *fit_context = nullptr) {
  const int lhs_score =
      score_camp_locker_item(slot, lhs, policy, local_temperature,
                             wet_weather, fit_context);
  const int rhs_score =
      score_camp_locker_item(slot, rhs, policy, local_temperature,
                             wet_weather, fit_context);
  if (lhs_score != rhs_score) {
    return lhs_score > rhs_score;
  }
  if (lhs.damage_level() != rhs.damage_level()) {
    return lhs.damage_level() < rhs.damage_level();
  }
  if (lhs.damage() != rhs.damage()) {
    return lhs.damage() < rhs.damage();
  }
  return lhs.typeId().str() < rhs.typeId().str();
}

const item *select_best_locker_item(
    camp_locker_slot slot, const std::vector<const item *> &items,
    const camp_locker_policy &policy,
    const std::optional<units::temperature> &local_temperature = std::nullopt,
    bool wet_weather = false, const Character *fit_context = nullptr) {
  const item *best = nullptr;
  for (const item *candidate : items) {
    if (candidate == nullptr) {
      continue;
    }
    if (best == nullptr ||
        is_better_scored_locker_item(slot, *candidate, *best, policy,
                                     local_temperature, wet_weather,
                                     fit_context)) {
      best = candidate;
    }
  }
  return best;
}

bool is_camp_locker_weather_sensitive_outerwear(
    camp_locker_slot slot, const item &it) {
  if (slot != camp_locker_slot::shirt && slot != camp_locker_slot::vest) {
    return false;
  }
  const bool outer =
      it.has_layer({layer_level::OUTER}) || it.has_flag(flag_OUTER);
  return outer && armor_covers_any(it, {"torso"}) &&
         armor_covers_any(it, {"arm_l", "arm_r"});
}

bool is_camp_locker_weather_sensitive_legwear(
    camp_locker_slot slot, const item &it) {
  return slot == camp_locker_slot::pants &&
         armor_covers_any(it, {"leg_l", "leg_r"});
}

bool is_camp_locker_weather_sensitive_rainwear(
    camp_locker_slot slot, const item &it) {
  return is_camp_locker_weather_sensitive_outerwear(slot, it) &&
         (it.has_flag(flag_RAINPROOF) || it.get_env_resist() > 0);
}

bool is_camp_locker_short_legwear(const item &it) {
  return armor_specifically_covers_any(
             it, {"leg_hip_l", "leg_hip_r", "leg_upper_l", "leg_upper_r"}) &&
         !armor_specifically_covers_any(
             it, {"leg_knee_l", "leg_knee_r", "leg_lower_l", "leg_lower_r"});
}

bool is_camp_locker_draped_only_legwear(const item &it) {
  return armor_specifically_covers_any(it, {"leg_draped_l", "leg_draped_r"}) &&
         !armor_specifically_covers_any(
             it, {"leg_hip_l", "leg_hip_r", "leg_upper_l", "leg_upper_r",
                  "leg_knee_l", "leg_knee_r", "leg_lower_l", "leg_lower_r",
                  "foot_l", "foot_r"});
}

bool is_camp_locker_draped_overlay_onepiece(const item &it) {
  return armor_specifically_covers_any(it, {"torso_waist"}) &&
         armor_specifically_covers_any(it, {"leg_draped_l", "leg_draped_r"}) &&
         !armor_specifically_covers_any(
             it, {"torso_upper", "torso_lower", "arm_upper_l", "arm_upper_r",
                  "arm_lower_l", "arm_lower_r", "hand_l", "hand_r",
                  "leg_hip_l", "leg_hip_r", "leg_upper_l", "leg_upper_r",
                  "leg_knee_l", "leg_knee_r", "leg_lower_l", "leg_lower_r",
                  "foot_l", "foot_r", "head", "eye_l", "eye_r", "mouth"});
}

bool is_camp_locker_leg_accessory(const item &it) {
  const bool covers_hips =
      armor_specifically_covers_any(it, {"leg_hip_l", "leg_hip_r"});
  const bool covers_upper_leg =
      armor_specifically_covers_any(it, {"leg_hip_l", "leg_hip_r",
                                         "leg_upper_l", "leg_upper_r"});
  const bool covers_only_upper_leg =
      armor_specifically_covers_any(it, {"leg_upper_l", "leg_upper_r"});
  const bool covers_lower_leg =
      armor_specifically_covers_any(it, {"leg_knee_l", "leg_knee_r",
                                         "leg_lower_l", "leg_lower_r"});
  const bool covers_feet =
      armor_specifically_covers_any(it, {"foot_l", "foot_r"});
  const bool covers_only_partial_upper_leg = covers_only_upper_leg != covers_hips;
  const bool full_leg_without_hips =
      covers_only_upper_leg && covers_lower_leg && !covers_hips;
  const bool support_storage = utility_storage_capacity(it) > 0_ml;
  const bool outer =
      it.has_layer({layer_level::OUTER}) || it.has_flag(flag_OUTER);

  if (covers_upper_leg && !covers_lower_leg && !covers_feet &&
      (it.has_flag(flag_BELTED) || it.has_flag(flag_BELT_CLIP))) {
    return true;
  }

  if (!covers_lower_leg && !covers_feet && covers_only_partial_upper_leg &&
      (support_storage || it.is_holster())) {
    return true;
  }

  const bool covers_non_leg_regions =
      armor_specifically_covers_any(it, {"torso_upper", "torso_lower",
                                         "arm_upper_l", "arm_upper_r",
                                         "arm_lower_l", "arm_lower_r",
                                         "hand_l", "hand_r", "head",
                                         "eye_l", "eye_r", "mouth"});

  if (covers_upper_leg && !covers_lower_leg && !covers_feet &&
      !support_storage && !it.is_holster() && !covers_non_leg_regions && outer &&
      it.weight() >= 1500_gram) {
    return true;
  }

  if (covers_lower_leg && covers_only_upper_leg && !covers_feet &&
      !support_storage && !it.is_holster() && !covers_non_leg_regions &&
      it.has_flag(flag_BLOCK_WHILE_WORN)) {
    return true;
  }

  if (full_leg_without_hips && !covers_feet && !support_storage &&
      !it.is_holster() && !covers_non_leg_regions && outer) {
    return true;
  }

  return covers_lower_leg && !covers_upper_leg && !covers_feet;
}

bool is_camp_locker_jumpsuit_like(const item &it) {
  static const itype_id itype_jumpsuit("jumpsuit");
  static const itype_id itype_suit("suit");
  return it.typeId() == itype_jumpsuit ||
         it.typeId()->looks_like == itype_jumpsuit ||
         it.typeId()->looks_like == itype_suit;
}

bool is_camp_locker_armored_full_body_suit(const item &it) {
  return utility_storage_capacity(it) >= 4_liter &&
         armor_covers_any(it, {"torso"}) &&
         armor_covers_any(it, {"arm_l", "arm_r"}) &&
         armor_covers_any(it, {"leg_l", "leg_r"}) &&
         !it.has_layer({layer_level::SKINTIGHT}) &&
         protection_score(it, 1, 2, 2) >= 80;
}

bool is_camp_locker_protective_full_body_suit(const item &it) {
  if (!armor_covers_any(it, {"torso"}) ||
      !armor_covers_any(it, {"arm_l", "arm_r"}) ||
      !armor_covers_any(it, {"leg_l", "leg_r"}) ||
      it.has_layer({layer_level::SKINTIGHT}) ||
      armor_covers_any(it, {"head", "eyes", "mouth", "hand_l",
                            "hand_r", "foot_l", "foot_r"})) {
    return false;
  }

  if (is_camp_locker_armored_full_body_suit(it)) {
    return true;
  }

  return total_ballistic_resist_score(it) >= 3 ||
         protection_score(it, 1, 2, 2) >= 40 ||
         it.get_env_resist() >= 8 ||
         (utility_storage_capacity(it) >= 4_liter &&
          protection_score(it, 1, 2, 2) >= 24);
}

bool is_camp_locker_outer_onepiece_garment(const item &it) {
  const bool outer =
      it.has_layer({layer_level::OUTER}) || it.has_flag(flag_OUTER);
  return outer && it.weight() < 1500_gram &&
         utility_storage_capacity(it) < 500_ml &&
         armor_covers_any(it, {"torso"}) &&
         armor_covers_any(it, {"arm_l", "arm_r"}) &&
         armor_covers_any(it, {"leg_l", "leg_r"}) &&
         !armor_covers_any(it, {"head", "eyes", "mouth", "hand_l",
                                "hand_r", "foot_l", "foot_r"});
}

bool camp_locker_plan_slot_retains_coverage(
    const camp_locker_plan &plan, camp_locker_slot slot,
    std::initializer_list<std::string_view> parts) {
  const auto found = plan.find(slot);
  if (found == plan.end()) {
    return false;
  }
  const camp_locker_slot_plan &slot_plan = found->second;
  const item *retained_item =
      slot_plan.selected_candidate != nullptr ? slot_plan.selected_candidate
                                  : slot_plan.kept_current;
  return retained_item != nullptr && armor_covers_any(*retained_item, parts);
}

bool camp_locker_plan_has_other_coverage(
    const camp_locker_plan &plan,
    std::initializer_list<std::string_view> parts,
    std::initializer_list<camp_locker_slot> slots) {
  return std::any_of(slots.begin(), slots.end(),
                     [&](const camp_locker_slot slot) {
                       return camp_locker_plan_slot_retains_coverage(
                           plan, slot, parts);
                     });
}

void prevent_upper_body_stripping_pants_upgrades(camp_locker_plan &plan) {
  const auto pants_it = plan.find(camp_locker_slot::pants);
  if (pants_it == plan.end()) {
    return;
  }

  camp_locker_slot_plan &pants_plan = pants_it->second;
  if (pants_plan.kept_current == nullptr ||
      pants_plan.selected_candidate == nullptr ||
      !pants_plan.upgrade_selected) {
    return;
  }

  if (is_camp_locker_armored_full_body_suit(*pants_plan.kept_current) &&
      !is_camp_locker_armored_full_body_suit(*pants_plan.selected_candidate)) {
    pants_plan.selected_candidate = nullptr;
    pants_plan.upgrade_selected = false;
    return;
  }

  auto coverage_preserved =
      [&](std::initializer_list<std::string_view> parts,
          std::initializer_list<camp_locker_slot> fallback_slots) {
        return !armor_covers_any(*pants_plan.kept_current, parts) ||
               armor_covers_any(*pants_plan.selected_candidate, parts) ||
               camp_locker_plan_has_other_coverage(plan, parts,
                                                   fallback_slots);
      };

  if (coverage_preserved({"torso"}, {camp_locker_slot::shirt,
                                      camp_locker_slot::vest,
                                      camp_locker_slot::body_armor}) &&
      coverage_preserved({"arm_l", "arm_r"},
                         {camp_locker_slot::shirt, camp_locker_slot::vest,
                          camp_locker_slot::body_armor}) &&
      coverage_preserved({"head"}, {camp_locker_slot::helmet}) &&
      coverage_preserved({"eyes"},
                         {camp_locker_slot::helmet,
                          camp_locker_slot::glasses}) &&
      coverage_preserved({"mouth"}, {camp_locker_slot::helmet}) &&
      coverage_preserved({"hand_l", "hand_r"}, {}) &&
      coverage_preserved({"foot_l", "foot_r"},
                         {camp_locker_slot::socks,
                          camp_locker_slot::shoes})) {
    return;
  }

  pants_plan.selected_candidate = nullptr;
  pants_plan.upgrade_selected = false;
}

void prevent_missing_pants_fill_under_full_body_body_armor(
    camp_locker_plan &plan) {
  if (!camp_locker_plan_slot_retains_coverage(
          plan, camp_locker_slot::body_armor, {"leg_l", "leg_r"})) {
    return;
  }

  const auto pants_it = plan.find(camp_locker_slot::pants);
  if (pants_it == plan.end()) {
    return;
  }

  camp_locker_slot_plan &pants_plan = pants_it->second;
  if (pants_plan.kept_current != nullptr || pants_plan.selected_candidate == nullptr ||
      !pants_plan.missing_current || !pants_plan.duplicate_current.empty()) {
    return;
  }

  plan.erase(pants_it);
}

void prevent_missing_shirt_fill_under_armored_full_body_suit(
    camp_locker_plan &plan) {
  const auto pants_it = plan.find(camp_locker_slot::pants);
  if (pants_it == plan.end()) {
    return;
  }

  const camp_locker_slot_plan &pants_plan = pants_it->second;
  const item *retained_pants =
      pants_plan.selected_candidate != nullptr ? pants_plan.selected_candidate
                                               : pants_plan.kept_current;
  if (retained_pants == nullptr ||
      !is_camp_locker_protective_full_body_suit(*retained_pants)) {
    return;
  }

  const auto shirt_it = plan.find(camp_locker_slot::shirt);
  if (shirt_it == plan.end()) {
    return;
  }

  const camp_locker_slot_plan &shirt_plan = shirt_it->second;
  if (shirt_plan.kept_current != nullptr ||
      shirt_plan.selected_candidate == nullptr) {
    return;
  }

  plan.erase(shirt_it);
}

void prefer_selected_full_body_suit_over_redundant_current_upper_body_layers(
    camp_locker_plan &plan, const camp_locker_policy &policy,
    const std::optional<units::temperature> &local_temperature,
    bool wet_weather, const Character *fit_context) {
  const auto pants_it = plan.find(camp_locker_slot::pants);
  if (pants_it == plan.end()) {
    return;
  }

  const camp_locker_slot_plan &pants_plan = pants_it->second;
  if (pants_plan.selected_candidate == nullptr || !pants_plan.upgrade_selected ||
      !is_camp_locker_protective_full_body_suit(*pants_plan.selected_candidate)) {
    return;
  }

  for (const camp_locker_slot slot :
       {camp_locker_slot::shirt, camp_locker_slot::vest,
        camp_locker_slot::body_armor}) {
    const auto slot_it = plan.find(slot);
    if (slot_it == plan.end()) {
      continue;
    }

    camp_locker_slot_plan &slot_plan = slot_it->second;
    if (slot_plan.kept_current == nullptr ||
        slot_plan.selected_candidate != nullptr ||
        !is_camp_locker_candidate_meaningfully_better(
            slot, *pants_plan.selected_candidate, *slot_plan.kept_current,
            policy, local_temperature, wet_weather, fit_context)) {
      continue;
    }

    slot_plan.duplicate_current.push_back(slot_plan.kept_current);
    slot_plan.kept_current = nullptr;
  }
}

int camp_locker_outerwear_temperature_adjustment(
    camp_locker_slot slot, const item &it,
    const std::optional<units::temperature> &local_temperature) {
  if (!local_temperature ||
      !is_camp_locker_weather_sensitive_outerwear(slot, it)) {
    return 0;
  }

  if (*local_temperature <= units::from_fahrenheit(50)) {
    return it.get_warmth() * 3;
  }
  if (*local_temperature >= units::from_fahrenheit(75)) {
    return -it.get_warmth() * 3;
  }

  switch (season_of_year(calendar::turn)) {
  case WINTER:
    return it.get_warmth() * 3;
  case SUMMER:
    return -it.get_warmth() * 3;
  default:
    return 0;
  }
}

int camp_locker_legwear_temperature_adjustment(
    camp_locker_slot slot, const item &it,
    const std::optional<units::temperature> &local_temperature) {
  if (!local_temperature || !is_camp_locker_weather_sensitive_legwear(slot, it)) {
    return 0;
  }

  const int coverage_bias = is_camp_locker_short_legwear(it) ? 80 : -80;
  if (*local_temperature <= units::from_fahrenheit(50)) {
    return -coverage_bias + it.get_warmth();
  }
  if (*local_temperature >= units::from_fahrenheit(75)) {
    return coverage_bias - it.get_warmth();
  }

  switch (season_of_year(calendar::turn)) {
  case WINTER:
    return -coverage_bias + it.get_warmth();
  case SUMMER:
    return coverage_bias - it.get_warmth();
  default:
    return 0;
  }
}

int camp_locker_outerwear_rain_adjustment(
    camp_locker_slot slot, const item &it,
    const std::optional<units::temperature> &local_temperature,
    bool wet_weather) {
  if (!wet_weather || !is_camp_locker_weather_sensitive_rainwear(slot, it)) {
    return 0;
  }
  if (local_temperature && *local_temperature <= units::from_fahrenheit(50)) {
    return 0;
  }

  return it.get_env_resist() * 30 +
         (it.has_flag(flag_RAINPROOF) ? 180 : 0);
}
} // namespace

std::optional<camp_locker_slot> classify_camp_locker_item(const item &it) {
  if (it.is_gun()) {
    return camp_locker_slot::ranged_weapon;
  }
  if (!it.is_armor()) {
    if (it.is_holster()) {
      return camp_locker_slot::holster;
    }
    if (it.is_melee()) {
      return camp_locker_slot::melee_weapon;
    }
    return std::nullopt;
  }

  const bool covers_eyes = armor_covers_any(it, {"eyes"});
  const bool covers_head = armor_covers_any(it, {"head"});
  const bool covers_hands = armor_covers_any(it, {"hand_l", "hand_r"});
  const bool covers_mouth = armor_covers_any(it, {"mouth", "muzzle"});
  const bool covers_feet = armor_covers_any(it, {"foot_l", "foot_r"});
  const bool covers_legs = armor_covers_any(it, {"leg_l", "leg_r"});
  const bool covers_lower_legs =
      armor_specifically_covers_any(it, {"leg_knee_l", "leg_knee_r",
                                        "leg_lower_l", "leg_lower_r"});
  const bool covers_torso = armor_covers_any(it, {"torso"});
  const bool covers_arms = armor_covers_any(it, {"arm_l", "arm_r"});
  const bool covers_waist = armor_specifically_covers_any(it, {"torso_waist"});
  const bool covers_hips =
      armor_specifically_covers_any(it, {"leg_hip_l", "leg_hip_r"});
  const bool skintight = it.has_layer({layer_level::SKINTIGHT});
  const bool outer =
      it.has_layer({layer_level::OUTER}) || it.has_flag(flag_OUTER);
  const units::volume storage = utility_storage_capacity(it);

  if (is_camp_locker_draped_overlay_onepiece(it)) {
    return std::nullopt;
  }
  if (covers_torso && covers_legs &&
      (covers_feet || !outer || is_camp_locker_jumpsuit_like(it) ||
       is_camp_locker_outer_onepiece_garment(it))) {
    return camp_locker_slot::pants;
  }
  if (covers_mouth && !covers_head) {
    return camp_locker_slot::mask;
  }
  if (covers_head) {
    return camp_locker_slot::helmet;
  }
  if (covers_eyes) {
    return camp_locker_slot::glasses;
  }
  if (covers_hands && !covers_head && !covers_eyes && !covers_torso &&
      !covers_arms && !covers_legs && !covers_feet) {
    return camp_locker_slot::gloves;
  }
  if (covers_feet && !covers_legs && !covers_torso && !covers_arms) {
    return skintight ? camp_locker_slot::socks : camp_locker_slot::shoes;
  }
  if (it.is_holster() && (covers_hips || !covers_waist)) {
    return camp_locker_slot::holster;
  }
  if (is_camp_locker_leg_accessory(it)) {
    return std::nullopt;
  }
  if (skintight &&
      (covers_torso || covers_arms || covers_feet ||
       (covers_legs && !covers_lower_legs))) {
    return camp_locker_slot::underwear;
  }
  if (covers_legs && !covers_head && !covers_eyes && !covers_torso &&
      !is_camp_locker_draped_only_legwear(it)) {
    return camp_locker_slot::pants;
  }
  if (covers_waist && !covers_head && !covers_eyes && !covers_mouth &&
      !covers_arms && !covers_hands && !covers_feet &&
      (!it.is_holster() || !covers_hips)) {
    return camp_locker_slot::belt;
  }
  if (covers_torso) {
    if (storage >= 6_liter) {
      return camp_locker_slot::bag;
    }
    if (storage >= 500_ml) {
      return camp_locker_slot::vest;
    }
    if (outer && it.weight() >= 1500_gram) {
      return camp_locker_slot::body_armor;
    }
    return camp_locker_slot::shirt;
  }

  return std::nullopt;
}

namespace {

bool camp_locker_slot_is_worn(camp_locker_slot slot) {
  return slot != camp_locker_slot::melee_weapon &&
         slot != camp_locker_slot::ranged_weapon;
}

bool camp_locker_candidate_is_usable_by_worker(
    camp_locker_slot slot, const item &it, const Character *worker) {
  if (worker == nullptr) {
    return true;
  }
  if (camp_locker_slot_is_worn(slot)) {
    return worker->can_wear(it, true).success();
  }
  return worker->can_wield(it).success();
}

camp_locker_candidate_map collect_camp_locker_candidates_impl(
    const std::vector<const item *> &items, const camp_locker_policy &policy,
    camp_locker_service_probe *probe, const Character *worker = nullptr) {
  camp_locker_candidate_map candidates;
  for (const item *it : items) {
    if (it == nullptr) {
      continue;
    }
    if (probe != nullptr) {
      probe->metrics.candidate_item_checks++;
    }
    const std::optional<camp_locker_slot> slot = classify_camp_locker_item(*it);
    if (!slot || !policy.is_enabled(*slot) ||
        !camp_locker_candidate_is_usable_by_worker(*slot, *it, worker)) {
      continue;
    }
    if (probe != nullptr) {
      probe->metrics.candidate_items_accepted++;
    }
    candidates[*slot].push_back(it);
  }
  return candidates;
}

int count_camp_locker_candidates(const camp_locker_candidate_map &candidates) {
  int count = 0;
  for (const auto &entry : candidates) {
    count += static_cast<int>(entry.second.size());
  }
  return count;
}

int count_changed_camp_locker_slots(const camp_locker_plan &plan) {
  int count = 0;
  for (const auto &entry : plan) {
    if (entry.second.has_changes()) {
      count++;
    }
  }
  return count;
}

} // namespace

camp_locker_candidate_map
collect_camp_locker_candidates(const std::vector<const item *> &items,
                               const camp_locker_policy &policy) {
  return collect_camp_locker_candidates_impl(items, policy, nullptr);
}

std::vector<tripoint_abs_ms>
collect_sorted_camp_patrol_tiles(const tripoint_abs_ms &origin,
                                 const faction_id &fac,
                                 int range) {
    std::unordered_set<tripoint_abs_ms> patrol_tiles =
        zone_manager::get_manager().get_near(zone_type_CAMP_PATROL, origin, range,
                                             nullptr, fac);
    std::vector<tripoint_abs_ms> sorted_tiles(patrol_tiles.begin(), patrol_tiles.end());
    sort_tripoints_zyx(sorted_tiles);
    return sorted_tiles;
}

std::vector<camp_patrol_cluster>
collect_camp_patrol_clusters(const tripoint_abs_ms &origin,
                             const faction_id &fac,
                             int range) {
    const std::vector<tripoint_abs_ms> patrol_tiles =
        collect_sorted_camp_patrol_tiles(origin, fac, range);
    std::unordered_set<tripoint_abs_ms> remaining_tiles(patrol_tiles.begin(),
                                                        patrol_tiles.end());
    std::vector<camp_patrol_cluster> clusters;

    for( const tripoint_abs_ms &seed : patrol_tiles ) {
        if( remaining_tiles.erase(seed) == 0 ) {
            continue;
        }

        camp_patrol_cluster cluster;
        std::vector<tripoint_abs_ms> frontier = { seed };

        while( !frontier.empty() ) {
            const tripoint_abs_ms current = frontier.back();
            frontier.pop_back();
            cluster.push_back(current);

            for( const tripoint &delta :
                 { tripoint::north, tripoint::east, tripoint::south, tripoint::west } ) {
                const tripoint_abs_ms neighbor = current + delta;
                if( remaining_tiles.erase(neighbor) > 0 ) {
                    frontier.push_back(neighbor);
                }
            }
        }

        sort_tripoints_zyx(cluster);
        clusters.push_back(std::move(cluster));
    }

    return clusters;
}

namespace {

bool camp_patrol_worker_sort_less(const camp_patrol_worker &lhs,
                                  const camp_patrol_worker &rhs) {
    if( lhs.priority != rhs.priority ) {
        return lhs.priority > rhs.priority;
    }
    return lhs.worker_id.get_value() < rhs.worker_id.get_value();
}

size_t count_camp_patrol_tiles(const std::vector<camp_patrol_cluster> &clusters) {
    size_t total_tiles = 0;
    for( const camp_patrol_cluster &cluster : clusters ) {
        total_tiles += cluster.size();
    }
    return total_tiles;
}

void assign_camp_patrol_guard(camp_patrol_guard_plan &guard_plan,
                              camp_patrol_cluster_plan &cluster_plan,
                              size_t cluster_index) {
    guard_plan.cluster_indices.push_back(cluster_index);
    cluster_plan.assigned_guards.push_back(guard_plan.worker_id);
}

size_t select_camp_patrol_extra_cluster(
    const std::vector<camp_patrol_cluster_plan> &clusters) {
    size_t best_index = 0;
    size_t best_remaining_capacity = 0;
    size_t best_cluster_size = 0;

    for( size_t index = 0; index < clusters.size(); ++index ) {
        const camp_patrol_cluster_plan &cluster = clusters[index];
        if( cluster.assigned_guards.size() >= cluster.tiles.size() ) {
            continue;
        }

        const size_t remaining_capacity =
            cluster.tiles.size() - cluster.assigned_guards.size();
        if( remaining_capacity > best_remaining_capacity ||
            ( remaining_capacity == best_remaining_capacity &&
              cluster.tiles.size() > best_cluster_size ) ) {
            best_index = index;
            best_remaining_capacity = remaining_capacity;
            best_cluster_size = cluster.tiles.size();
        }
    }

    return best_index;
}

camp_patrol_shift_plan build_camp_patrol_shift_plan(
    camp_patrol_shift shift, const std::vector<character_id> &roster,
    const std::vector<camp_patrol_cluster> &clusters) {
    camp_patrol_shift_plan plan;
    plan.shift = shift;
    plan.roster = roster;
    plan.clusters.reserve(clusters.size());
    for( const camp_patrol_cluster &cluster : clusters ) {
        plan.clusters.push_back(camp_patrol_cluster_plan{ cluster, {} });
    }

    const size_t active_count = std::min(roster.size(), count_camp_patrol_tiles(clusters));
    plan.active_guards.reserve(active_count);
    for( size_t index = 0; index < active_count; ++index ) {
        plan.active_guards.push_back(camp_patrol_guard_plan{ roster[index], {} });
    }
    plan.reserve_guards.assign(roster.begin() + active_count, roster.end());

    if( plan.active_guards.empty() || plan.clusters.empty() ) {
        return plan;
    }

    for( size_t cluster_index = 0; cluster_index < plan.clusters.size(); ++cluster_index ) {
        camp_patrol_guard_plan &guard_plan =
            plan.active_guards[cluster_index % plan.active_guards.size()];
        assign_camp_patrol_guard(guard_plan, plan.clusters[cluster_index], cluster_index);
    }

    for( size_t guard_index = plan.clusters.size(); guard_index < plan.active_guards.size();
         ++guard_index ) {
        const size_t cluster_index = select_camp_patrol_extra_cluster(plan.clusters);
        assign_camp_patrol_guard(plan.active_guards[guard_index],
                                 plan.clusters[cluster_index], cluster_index);
    }

    return plan;
}

} // namespace

std::vector<camp_patrol_worker>
collect_camp_patrol_workers(const std::vector<npc_ptr> &assigned_npcs) {
    std::vector<camp_patrol_worker> workers;

    for( const npc_ptr &guard : assigned_npcs ) {
        if( !guard ) {
            continue;
        }
        const int priority = guard->job.get_priority_of_job(ACT_CAMP_PATROL);
        if( priority <= 0 ) {
            continue;
        }
        workers.push_back(camp_patrol_worker{ guard->getID(), priority });
    }

    std::sort(workers.begin(), workers.end(), camp_patrol_worker_sort_less);
    return workers;
}

camp_patrol_plan
plan_camp_patrol(const std::vector<camp_patrol_worker> &workers,
                 const std::vector<camp_patrol_cluster> &clusters) {
    std::vector<character_id> day_roster;
    std::vector<character_id> night_roster;
    day_roster.reserve((workers.size() + 1) / 2);
    night_roster.reserve(workers.size() / 2);

    for( size_t index = 0; index < workers.size(); ++index ) {
        const character_id worker_id = workers[index].worker_id;
        if( index % 2 == 0 ) {
            day_roster.push_back(worker_id);
        } else {
            night_roster.push_back(worker_id);
        }
    }

    camp_patrol_plan plan;
    plan.day = build_camp_patrol_shift_plan(camp_patrol_shift::day, day_roster,
                                            clusters);
    plan.night = build_camp_patrol_shift_plan(camp_patrol_shift::night,
                                              night_roster, clusters);
    return plan;
}

namespace {

size_t camp_patrol_guard_cluster_slot(const camp_patrol_cluster_plan &cluster,
                                      const character_id &worker_id) {
    const auto assigned_it = std::find(cluster.assigned_guards.begin(),
                                       cluster.assigned_guards.end(), worker_id);
    if( assigned_it == cluster.assigned_guards.end() ) {
        return 0;
    }
    return static_cast<size_t>(std::distance(cluster.assigned_guards.begin(),
                                             assigned_it));
}

bool camp_patrol_guard_holds_single_cluster(
    const camp_patrol_shift_plan &plan, const camp_patrol_guard_plan &guard_plan) {
    if( guard_plan.cluster_indices.size() != 1 ) {
        return false;
    }
    const size_t cluster_index = guard_plan.cluster_indices.front();
    if( cluster_index >= plan.clusters.size() ) {
        return false;
    }
    const camp_patrol_cluster_plan &cluster = plan.clusters[cluster_index];
    return cluster.assigned_guards.size() >= cluster.tiles.size();
}

std::vector<tripoint_abs_ms>
collect_camp_patrol_guard_route(const camp_patrol_shift_plan &plan,
                                const camp_patrol_guard_plan &guard_plan) {
    std::vector<tripoint_abs_ms> route;
    for( const size_t cluster_index : guard_plan.cluster_indices ) {
        if( cluster_index >= plan.clusters.size() ) {
            continue;
        }
        const camp_patrol_cluster_plan &cluster = plan.clusters[cluster_index];
        route.insert(route.end(), cluster.tiles.begin(), cluster.tiles.end());
    }
    return route;
}

size_t camp_patrol_guard_loop_phase(const camp_patrol_shift_plan &plan,
                                    const camp_patrol_guard_plan &guard_plan) {
    if( guard_plan.cluster_indices.size() != 1 ) {
        return 0;
    }
    const size_t cluster_index = guard_plan.cluster_indices.front();
    if( cluster_index >= plan.clusters.size() ) {
        return 0;
    }
    return camp_patrol_guard_cluster_slot(plan.clusters[cluster_index],
                                          guard_plan.worker_id);
}

} // namespace

time_duration camp_patrol_loop_dwell() {
    return 10_minutes;
}

std::optional<camp_patrol_guard_runtime>
describe_camp_patrol_guard_runtime(const camp_patrol_shift_plan &plan,
                                   const character_id &worker_id,
                                   const time_point &turn) {
    const auto guard_it = std::find_if(
        plan.active_guards.begin(), plan.active_guards.end(),
        [&worker_id]( const camp_patrol_guard_plan &guard_plan ) {
            return guard_plan.worker_id == worker_id;
        } );
    if( guard_it == plan.active_guards.end() ) {
        return std::nullopt;
    }

    camp_patrol_guard_runtime runtime;
    runtime.worker_id = worker_id;

    if( camp_patrol_guard_holds_single_cluster(plan, *guard_it) ) {
        const size_t cluster_index = guard_it->cluster_indices.front();
        const camp_patrol_cluster_plan &cluster = plan.clusters[cluster_index];
        const size_t hold_index = camp_patrol_guard_cluster_slot(cluster, worker_id);
        if( cluster.tiles.empty() || hold_index >= cluster.tiles.size() ) {
            return std::nullopt;
        }
        runtime.behavior = camp_patrol_guard_behavior::hold;
        runtime.route = { cluster.tiles[hold_index] };
        runtime.target = cluster.tiles[hold_index];
        return runtime;
    }

    runtime.behavior = camp_patrol_guard_behavior::loop;
    runtime.route = collect_camp_patrol_guard_route(plan, *guard_it);
    if( runtime.route.empty() ) {
        return std::nullopt;
    }

    const int dwell_turns = to_turns<int>(camp_patrol_loop_dwell());
    const int absolute_turn = to_turns<int>(turn - calendar::turn_zero);
    const size_t loop_step = dwell_turns > 0 ? static_cast<size_t>(absolute_turn /
                             dwell_turns) : 0;
    const size_t loop_phase = camp_patrol_guard_loop_phase(plan, *guard_it);
    runtime.target = runtime.route[(loop_step + loop_phase) % runtime.route.size()];
    return runtime;
}

bool camp_patrol_interrupt_is_whitelisted(
    camp_patrol_interrupt_reason reason) {
    switch( reason ) {
        case camp_patrol_interrupt_reason::routine_chore:
            return false;
        case camp_patrol_interrupt_reason::combat_threat:
        case camp_patrol_interrupt_reason::severe_injury:
        case camp_patrol_interrupt_reason::collapse_need:
        case camp_patrol_interrupt_reason::explicit_reassignment:
            return true;
    }
    return false;
}

bool apply_camp_patrol_guard_interrupt(
    camp_patrol_shift_plan &plan, const character_id &worker_id,
    camp_patrol_interrupt_reason reason) {
    if( !camp_patrol_interrupt_is_whitelisted(reason) ) {
        return false;
    }

    const auto active_it = std::find_if(
        plan.active_guards.begin(), plan.active_guards.end(),
        [&worker_id]( const camp_patrol_guard_plan &guard_plan ) {
            return guard_plan.worker_id == worker_id;
        } );
    if( active_it != plan.active_guards.end() ) {
        const std::vector<size_t> cluster_indices = active_it->cluster_indices;
        for( const size_t cluster_index : cluster_indices ) {
            if( cluster_index >= plan.clusters.size() ) {
                continue;
            }
            std::vector<character_id> &assigned_guards =
                plan.clusters[cluster_index].assigned_guards;
            assigned_guards.erase(
                std::remove(assigned_guards.begin(), assigned_guards.end(), worker_id),
                assigned_guards.end());
        }

        if( !plan.reserve_guards.empty() ) {
            const character_id replacement_id = plan.reserve_guards.front();
            plan.reserve_guards.erase(plan.reserve_guards.begin());
            active_it->worker_id = replacement_id;
            for( const size_t cluster_index : cluster_indices ) {
                if( cluster_index >= plan.clusters.size() ) {
                    continue;
                }
                plan.clusters[cluster_index].assigned_guards.push_back(replacement_id);
            }
        } else {
            plan.active_guards.erase(active_it);
        }
        return true;
    }

    const auto reserve_it =
        std::find(plan.reserve_guards.begin(), plan.reserve_guards.end(), worker_id);
    if( reserve_it != plan.reserve_guards.end() ) {
        plan.reserve_guards.erase(reserve_it);
        return true;
    }

    return false;
}

namespace {

int camp_patrol_shift_cache_day(const time_point &turn) {
    return to_days<int>(turn - calendar::turn_zero);
}

camp_patrol_shift camp_patrol_shift_for_turn(const time_point &turn) {
    return is_night(turn) ? camp_patrol_shift::night : camp_patrol_shift::day;
}

} // namespace

std::vector<tripoint_abs_ms>
collect_sorted_camp_locker_tiles(const tripoint_abs_ms &origin,
                                 const faction_id &fac,
                                 int range = MAX_VIEW_DISTANCE);

namespace {

bool camp_locker_reservation_matches(const camp_locker_reservation &reservation,
                                     const tripoint_abs_ms &tile,
                                     const item &candidate,
                                     const character_id &requesting_worker) {
  return reservation.expires > calendar::turn &&
         reservation.worker_id != requesting_worker && reservation.tile == tile &&
         reservation.item_type == candidate.typeId();
}

} // namespace

camp_locker_candidate_map collect_camp_locker_zone_candidates(
    const tripoint_abs_ms &origin, const faction_id &fac,
    const camp_locker_policy &policy, int range) {
  return collect_camp_locker_zone_candidates(origin, fac, policy,
                                             std::vector<camp_locker_reservation>(),
                                             character_id(), range);
}

static std::vector<const item *> collect_camp_locker_zone_items(
    const std::vector<tripoint_abs_ms> &locker_tiles,
    const std::vector<camp_locker_reservation> &reservations,
    const character_id &requesting_worker,
    camp_locker_service_probe *probe = nullptr) {
  map &here = get_map();
  std::vector<const item *> items;

  if (probe != nullptr) {
    probe->metrics.zone_item_collection_calls++;
  }
  for (const tripoint_abs_ms &tile : locker_tiles) {
    const tripoint_bub_ms local = here.get_bub(tile);
    if (!here.inbounds(local)) {
      continue;
    }
    if (g->check_zone(zone_type_NO_NPC_PICKUP, local)) {
      continue;
    }
    if (probe != nullptr) {
      probe->metrics.zone_tile_visits++;
    }
    for (const item &it : here.i_at(local)) {
      if (probe != nullptr) {
        probe->metrics.zone_top_level_items_seen++;
      }
      if (std::any_of(reservations.begin(), reservations.end(),
                      [&tile, &it, &requesting_worker](
                          const camp_locker_reservation &reservation) {
                        return camp_locker_reservation_matches(
                            reservation, tile, it, requesting_worker);
                      })) {
        if (probe != nullptr) {
          probe->metrics.zone_reserved_items_skipped++;
        }
        continue;
      }
      items.push_back(&it);
      if (probe != nullptr) {
        probe->metrics.zone_items_returned++;
      }
    }
  }

  return items;
}

camp_locker_candidate_map collect_camp_locker_zone_candidates(
    const tripoint_abs_ms &origin, const faction_id &fac,
    const camp_locker_policy &policy,
    const std::vector<camp_locker_reservation> &reservations,
    const character_id &requesting_worker, int range) {
  const std::vector<tripoint_abs_ms> locker_tiles =
      collect_sorted_camp_locker_tiles(origin, fac, range);
  return collect_camp_locker_candidates_impl(
      collect_camp_locker_zone_items(locker_tiles, reservations,
                                     requesting_worker),
      policy, nullptr);
}

int score_camp_locker_item(
    camp_locker_slot slot, const item &it, const camp_locker_policy &policy,
    const std::optional<units::temperature> &local_temperature,
    bool wet_weather, const Character *fit_context) {
  int score = 0;
  switch (slot) {
  case camp_locker_slot::underwear:
  case camp_locker_slot::socks:
  case camp_locker_slot::gloves:
  case camp_locker_slot::shoes:
  case camp_locker_slot::pants:
  case camp_locker_slot::shirt:
    score = generic_clothing_score(it, 250, 4, fit_context);
    break;
  case camp_locker_slot::vest:
    score = generic_clothing_score(it, 100, 4, fit_context);
    break;
  case camp_locker_slot::body_armor:
    score = body_armor_score(it, fit_context) +
            ( policy.prefers_bulletproof() ?
                  total_ballistic_resist_score(it) * 12 : 0 );
    break;
  case camp_locker_slot::helmet:
    score = helmet_score(it, fit_context) +
            ( policy.prefers_bulletproof() ?
                  static_cast<int>( it.resist( damage_bullet ) ) * 10 :
                  0 );
    break;
  case camp_locker_slot::glasses:
  case camp_locker_slot::mask:
    score = glasses_score(it, fit_context);
    break;
  case camp_locker_slot::belt:
  case camp_locker_slot::holster:
  case camp_locker_slot::bag:
    score = bag_score(it, fit_context);
    break;
  case camp_locker_slot::melee_weapon:
    score = melee_weapon_score(it, fit_context);
    break;
  case camp_locker_slot::ranged_weapon:
    score = ranged_weapon_score(it, fit_context);
    break;
  case camp_locker_slot::num_slots:
    break;
  }
  return score +
         camp_locker_outerwear_temperature_adjustment(slot, it,
                                                      local_temperature) +
         camp_locker_legwear_temperature_adjustment(slot, it,
                                                    local_temperature) +
         camp_locker_outerwear_rain_adjustment(slot, it, local_temperature,
                                               wet_weather);
}

bool is_camp_locker_candidate_meaningfully_better(
    camp_locker_slot slot, const item &candidate, const item &current,
    const camp_locker_policy &policy,
    const std::optional<units::temperature> &local_temperature,
    bool wet_weather, const Character *fit_context) {
  if (candidate.typeId() == current.typeId()) {
    if (candidate.damage_level() != current.damage_level()) {
      return candidate.damage_level() < current.damage_level();
    }
    if (candidate.damage() != current.damage()) {
      return candidate.damage() < current.damage();
    }

    const int candidate_score =
        score_camp_locker_item(slot, candidate, policy, local_temperature,
                               wet_weather, fit_context);
    const int current_score =
        score_camp_locker_item(slot, current, policy, local_temperature,
                               wet_weather, fit_context);
    if (candidate_score != current_score) {
      return candidate_score > current_score;
    }
  }
  return score_camp_locker_item(slot, candidate, policy,
                                local_temperature, wet_weather,
                                fit_context) >=
         score_camp_locker_item(slot, current, policy, local_temperature,
                                wet_weather, fit_context) +
             camp_locker_upgrade_threshold(slot);
}

camp_locker_plan
plan_camp_locker_loadout(
    const std::vector<const item *> &current_items,
    const camp_locker_candidate_map &locker_candidates,
    const camp_locker_policy &policy,
    const std::optional<units::temperature> &local_temperature,
    bool wet_weather, const Character *fit_context) {
  camp_locker_plan plan;
  const camp_locker_candidate_map current_by_slot =
      collect_camp_locker_candidates(current_items, policy);

  for (const camp_locker_slot slot : all_camp_locker_slots()) {
    if (!policy.is_enabled(slot)) {
      continue;
    }

    camp_locker_slot_plan slot_plan;
    auto current_it = current_by_slot.find(slot);
    if (current_it != current_by_slot.end() && !current_it->second.empty()) {
      std::vector<const item *> sorted_current = current_it->second;
      std::stable_sort(sorted_current.begin(), sorted_current.end(),
                       [slot, &policy, &local_temperature, wet_weather,
                        fit_context](const item *lhs, const item *rhs) {
                         if (lhs == nullptr || rhs == nullptr) {
                           return rhs != nullptr;
                         }
                         return is_better_scored_locker_item(
                             slot, *lhs, *rhs, policy, local_temperature,
                             wet_weather, fit_context);
                       });
      slot_plan.kept_current = sorted_current.front();
      if (sorted_current.size() > 1) {
        slot_plan.duplicate_current.assign(sorted_current.begin() + 1,
                                           sorted_current.end());
      }
    }

    auto candidate_it = locker_candidates.find(slot);
    if (candidate_it != locker_candidates.end()) {
      const item *best_candidate = select_best_locker_item(
          slot, candidate_it->second, policy, local_temperature,
          wet_weather, fit_context);
      if (best_candidate != nullptr) {
        if (slot_plan.kept_current == nullptr) {
          slot_plan.missing_current = true;
          slot_plan.selected_candidate = best_candidate;
        } else if (is_camp_locker_candidate_meaningfully_better(
                       slot, *best_candidate, *slot_plan.kept_current,
                       policy, local_temperature, wet_weather, fit_context)) {
          slot_plan.selected_candidate = best_candidate;
          slot_plan.upgrade_selected = true;
        }
      }
    }

    if (slot_plan.kept_current != nullptr ||
        slot_plan.selected_candidate != nullptr ||
        !slot_plan.duplicate_current.empty()) {
      plan.emplace(slot, std::move(slot_plan));
    }
  }

  prevent_missing_pants_fill_under_full_body_body_armor(plan);
  prevent_upper_body_stripping_pants_upgrades(plan);
  prevent_missing_shirt_fill_under_armored_full_body_suit(plan);
  prefer_selected_full_body_suit_over_redundant_current_upper_body_layers(
      plan, policy, local_temperature, wet_weather, fit_context);

  return plan;
}

static std::vector<const item *> collect_camp_locker_current_items(
    const npc &worker) {
  return worker.items_with([&worker](const item &node) {
    return worker.is_worn(node) || worker.is_wielding(node);
  });
}

item take_camp_locker_candidate(
    const std::vector<tripoint_abs_ms> &locker_tiles, const item *target);
void store_camp_locker_item(const tripoint_abs_ms &locker_tile, item moved);
std::string camp_locker_item_debug_label(const item &it);
static std::string join_camp_locker_debug_parts(const std::vector<std::string> &parts);

static std::vector<const item *> collect_camp_locker_worker_items(
    const npc &worker) {
  return worker.items_with([](const item &) { return true; });
}

static const heal_actor *camp_locker_heal_actor(const item &it) {
  for (const auto &[use_id, use] : it.type->use_methods) {
    (void)use_id;
    if (const heal_actor *actor =
            dynamic_cast<const heal_actor *>(use.get_actor_ptr())) {
      return actor;
    }
  }
  return nullptr;
}

static bool is_camp_locker_medical_readiness_supply(const item &it) {
  const heal_actor *actor = camp_locker_heal_actor(it);
  return actor != nullptr && (actor->bandages_power > 0.0f || actor->bleed > 0);
}

static bool is_camp_locker_armor_insert(const item &it) {
  return it.is_armor() && it.has_flag(flag_CANT_WEAR);
}

static bool is_camp_locker_kept_carried_item(const item &it) {
  return is_camp_locker_medical_readiness_supply(it) || it.is_ammo() ||
         it.is_magazine() || is_camp_locker_armor_insert(it);
}

static bool is_camp_locker_dumpable_carried_item(const npc &worker,
                                                  const item &it) {
  return !worker.is_worn(it) && !worker.is_wielding(it) &&
         !is_camp_locker_kept_carried_item(it);
}

struct camp_locker_carried_cleanup_state {
  int items_to_dump = 0;
  std::vector<std::string> dump_items;
  std::vector<std::string> kept_items;

  bool has_changes() const {
    return items_to_dump > 0;
  }
};

static camp_locker_carried_cleanup_state
collect_camp_locker_carried_cleanup_state(const npc &worker) {
  camp_locker_carried_cleanup_state cleanup;
  const std::vector<const item *> worker_items = collect_camp_locker_worker_items(worker);
  for (const item *node : worker_items) {
    if (node == nullptr) {
      continue;
    }
    if (is_camp_locker_dumpable_carried_item(worker, *node)) {
      cleanup.items_to_dump++;
      cleanup.dump_items.push_back(camp_locker_item_debug_label(*node));
    } else if (!worker.is_worn(*node) && !worker.is_wielding(*node) &&
               is_camp_locker_kept_carried_item(*node)) {
      cleanup.kept_items.push_back(camp_locker_item_debug_label(*node));
    }
  }
  return cleanup;
}

static std::string camp_locker_carried_cleanup_debug_summary(
    const camp_locker_carried_cleanup_state &cleanup) {
  if (cleanup.items_to_dump <= 0 && cleanup.kept_items.empty()) {
    return "none";
  }
  std::vector<std::string> parts;
  if (cleanup.items_to_dump > 0) {
    parts.push_back(string_format("dump=%d items=[%s]", cleanup.items_to_dump,
                                  join_camp_locker_debug_parts(cleanup.dump_items)));
  } else {
    parts.push_back("dump=0 items=[none]");
  }
  if (!cleanup.kept_items.empty()) {
    parts.push_back(string_format("kept=[%s]",
                                  join_camp_locker_debug_parts(cleanup.kept_items)));
  }
  return join_camp_locker_debug_parts(parts);
}

struct camp_locker_extracted_contents {
  std::list<item> kept_items;
  std::list<item> dumped_items;

  bool has_changes() const {
    return !kept_items.empty() || !dumped_items.empty();
  }
};

static camp_locker_extracted_contents extract_camp_locker_item_contents(
    item &source, bool extract_dumped_items) {
  camp_locker_extracted_contents extracted;
  extracted.kept_items = source.remove_items_with(
      [](const item &it) { return is_camp_locker_kept_carried_item(it); });
  if (extract_dumped_items) {
    extracted.dumped_items = source.remove_items_with(
        [](const item &it) { return !is_camp_locker_kept_carried_item(it); });
  }
  return extracted;
}

static void restore_camp_locker_item_contents(
    item &source, camp_locker_extracted_contents &extracted) {
  for (item &kept : extracted.kept_items) {
    source.force_insert_item(kept, pocket_type::CONTAINER);
  }
  for (item &dumped : extracted.dumped_items) {
    source.force_insert_item(dumped, pocket_type::CONTAINER);
  }
}

static bool place_camp_locker_item_contents(
    npc &worker, camp_locker_extracted_contents &extracted,
    const tripoint_abs_ms &locker_drop_tile,
    const std::optional<tripoint_abs_ms> &cleanup_drop_tile) {
  bool changed = false;

  for (item &kept : extracted.kept_items) {
    item_location added =
        worker.i_add(kept, true, nullptr, nullptr, false, false);
    if (!added) {
      store_camp_locker_item(locker_drop_tile, std::move(kept));
      continue;
    }
    changed = true;
  }

  if (cleanup_drop_tile) {
    for (item &dumped : extracted.dumped_items) {
      changed = true;
      store_camp_locker_item(*cleanup_drop_tile, std::move(dumped));
    }
  }

  return changed;
}

static bool is_camp_locker_managed_ranged_weapon(const item &it,
                                          const camp_locker_policy &policy) {
  const std::optional<camp_locker_slot> slot = classify_camp_locker_item(it);
  return slot && *slot == camp_locker_slot::ranged_weapon &&
         policy.is_enabled(*slot);
}

static int camp_locker_magazine_total_capacity(const item &magazine) {
  const itype *capacity_ammo = magazine.ammo_data();
  if (capacity_ammo == nullptr) {
    const itype_id default_ammo = magazine.ammo_default();
    if (default_ammo.is_null()) {
      return 0;
    }
    capacity_ammo = item::find_type(default_ammo);
  }
  if (capacity_ammo == nullptr || !capacity_ammo->ammo) {
    return 0;
  }

  return magazine.ammo_capacity(capacity_ammo->ammo->type);
}

static bool is_better_camp_locker_magazine(const item &lhs, const item &rhs) {
  if (lhs.ammo_remaining() != rhs.ammo_remaining()) {
    return lhs.ammo_remaining() > rhs.ammo_remaining();
  }
  const int lhs_capacity = camp_locker_magazine_total_capacity(lhs);
  const int rhs_capacity = camp_locker_magazine_total_capacity(rhs);
  if (lhs_capacity != rhs_capacity) {
    return lhs_capacity > rhs_capacity;
  }
  if (lhs.weight() != rhs.weight()) {
    return lhs.weight() < rhs.weight();
  }
  return lhs.typeId().str() < rhs.typeId().str();
}

static void sort_camp_locker_magazines(std::vector<const item *> &magazines) {
  std::stable_sort(magazines.begin(), magazines.end(),
                   [](const item *lhs, const item *rhs) {
                     if (lhs == nullptr || rhs == nullptr) {
                       return rhs != nullptr;
                     }
                     return is_better_camp_locker_magazine(*lhs, *rhs);
                   });
}

static std::vector<const item *> collect_camp_locker_compatible_magazines(
    const std::vector<const item *> &items, const item &weapon,
    camp_locker_service_probe *probe = nullptr) {
  std::vector<const item *> magazines;
  if (!weapon.is_gun() || !weapon.uses_magazine()) {
    return magazines;
  }

  const item *current_magazine = weapon.magazine_current();
  if (current_magazine != nullptr) {
    magazines.push_back(current_magazine);
  }
  for (const item *it : items) {
    if (probe != nullptr) {
      probe->metrics.compatible_magazine_item_checks++;
    }
    if (it == nullptr || it == current_magazine || !it->is_magazine() ||
        !weapon.can_reload_with(*it, false)) {
      continue;
    }
    magazines.push_back(it);
  }

  sort_camp_locker_magazines(magazines);
  return magazines;
}

static std::vector<item_location> collect_camp_locker_compatible_magazine_locations(
    npc &worker, const item &weapon) {
  std::vector<item_location> magazines;
  if (!weapon.is_gun() || !weapon.uses_magazine()) {
    return magazines;
  }

  const item_location weapon_loc = worker.get_wielded_item();
  const item *current_magazine = weapon.magazine_current();
  if (weapon_loc && current_magazine != nullptr) {
    magazines.emplace_back(weapon_loc, const_cast<item *>(current_magazine));
  }

  for (const item_location &ammo_loc : worker.find_ammo(weapon, true, -1)) {
    if (ammo_loc && ammo_loc.get_item() != current_magazine &&
        ammo_loc->is_magazine()) {
      magazines.push_back(ammo_loc);
    }
  }

  std::stable_sort(magazines.begin(), magazines.end(),
                   [](const item_location &lhs, const item_location &rhs) {
                     if (!lhs || !rhs) {
                       return static_cast<bool>(rhs);
                     }
                     return is_better_camp_locker_magazine(*lhs, *rhs);
                   });
  return magazines;
}

static bool can_camp_locker_worker_reload_with(const Character *reloader,
                                               const item &target,
                                               const item &ammo) {
  if (!target.can_reload_with(ammo, true)) {
    return false;
  }
  return reloader == nullptr || reloader->can_reload(target, &ammo);
}

static const item *select_best_camp_locker_ammo_candidate(
    const std::vector<const item *> &items, const item &target,
    const Character *reloader = nullptr,
    camp_locker_service_probe *probe = nullptr) {
  std::vector<const item *> ammo_items;
  for (const item *it : items) {
    if (probe != nullptr) {
      probe->metrics.compatible_ammo_item_checks++;
    }
    if (it == nullptr || it->ammo_remaining() <= 0 ||
        !can_camp_locker_worker_reload_with(reloader, target, *it)) {
      continue;
    }
    ammo_items.push_back(it);
  }

  std::stable_sort(ammo_items.begin(), ammo_items.end(),
                   [](const item *lhs, const item *rhs) {
                     if (lhs == nullptr || rhs == nullptr) {
                       return rhs != nullptr;
                     }
                     if (lhs->ammo_remaining() != rhs->ammo_remaining()) {
                       return lhs->ammo_remaining() > rhs->ammo_remaining();
                     }
                     return lhs->typeId().str() < rhs->typeId().str();
                   });

  return ammo_items.empty() ? nullptr : ammo_items.front();
}

static bool reload_camp_locker_target_from_zone(npc &worker,
                                         const item_location &target,
                                         const std::vector<tripoint_abs_ms> &locker_tiles,
                                         const tripoint_abs_ms &locker_drop_tile) {
  if (!target) {
    return false;
  }

  const item *ammo_candidate = select_best_camp_locker_ammo_candidate(
      collect_camp_locker_zone_items(locker_tiles,
                                     std::vector<camp_locker_reservation>(),
                                     worker.getID()),
      *target, &worker, nullptr);
  if (ammo_candidate == nullptr) {
    return false;
  }

  item moved_ammo = take_camp_locker_candidate(locker_tiles, ammo_candidate);
  if (moved_ammo.is_null()) {
    return false;
  }

  item_location ammo_loc =
      worker.try_add(moved_ammo, nullptr, nullptr, false);
  if (!ammo_loc) {
    store_camp_locker_item(locker_drop_tile, std::move(moved_ammo));
    return false;
  }

  item::reload_option reload_opt(&worker, target, ammo_loc);
  item &reload_target = const_cast<item &>(*target);
  const bool reloaded =
      reload_target.reload(worker, ammo_loc, reload_opt.qty());
  if (ammo_loc) {
    store_camp_locker_item(locker_drop_tile, worker.i_rem(&*ammo_loc));
  }
  return reloaded;
}

static item_location select_best_loaded_camp_locker_magazine(
    const std::vector<item_location> &magazines) {
  for (const item_location &magazine_loc : magazines) {
    if (magazine_loc && magazine_loc->ammo_remaining() > 0) {
      return magazine_loc;
    }
  }
  return item_location();
}

struct camp_locker_ranged_readiness_state {
  std::string weapon;
  int current_compatible_magazines = 0;
  int magazines_to_take = 0;
  int magazines_to_reload = 0;
  bool weapon_needs_reload = false;

  bool has_changes() const {
    return magazines_to_take > 0 || magazines_to_reload > 0 ||
           weapon_needs_reload;
  }
};

static camp_locker_ranged_readiness_state collect_camp_locker_ranged_readiness_state(
    npc &worker, const camp_locker_policy &policy,
    const std::vector<const item *> &locker_items,
    camp_locker_service_probe *probe = nullptr) {
  camp_locker_ranged_readiness_state readiness;

  const item_location weapon_loc = worker.get_wielded_item();
  if (!weapon_loc || !is_camp_locker_managed_ranged_weapon(*weapon_loc, policy)) {
    return readiness;
  }

  readiness.weapon = camp_locker_item_debug_label(*weapon_loc);

  if (weapon_loc->uses_magazine()) {
    const std::vector<item_location> current_magazine_locs =
        collect_camp_locker_compatible_magazine_locations(worker, *weapon_loc);
    readiness.current_compatible_magazines =
        static_cast<int>(current_magazine_locs.size());

    std::vector<const item *> selected_magazines;
    selected_magazines.reserve(current_magazine_locs.size());
    for (const item_location &magazine_loc : current_magazine_locs) {
      if (magazine_loc) {
        selected_magazines.push_back(&*magazine_loc);
      }
    }
    const std::vector<const item *> locker_magazines =
        collect_camp_locker_compatible_magazines(locker_items, *weapon_loc,
                                                 probe);
    const int desired_magazines = std::max(0, 2 - readiness.current_compatible_magazines);
    readiness.magazines_to_take =
        std::min(desired_magazines, static_cast<int>(locker_magazines.size()));
    for (int i = 0; i < readiness.magazines_to_take; ++i) {
      if (locker_magazines[i] != nullptr) {
        selected_magazines.push_back(locker_magazines[i]);
      }
    }
    sort_camp_locker_magazines(selected_magazines);
    if (selected_magazines.size() > 2) {
      selected_magazines.resize(2);
    }

    for (const item *magazine : selected_magazines) {
      if (magazine == nullptr ||
          select_best_camp_locker_ammo_candidate(locker_items, *magazine,
                                                 &worker, probe) == nullptr) {
        continue;
      }
      readiness.magazines_to_reload++;
    }

    const bool has_ready_magazine = std::any_of(
        selected_magazines.begin(), selected_magazines.end(),
        [](const item *magazine) {
          return magazine != nullptr && magazine->ammo_remaining() > 0;
        });
    const bool could_ready_magazine = has_ready_magazine ||
                                      readiness.magazines_to_reload > 0;
    readiness.weapon_needs_reload =
        weapon_loc->ammo_remaining() <= 0 && could_ready_magazine;
  } else {
    readiness.weapon_needs_reload =
        select_best_camp_locker_ammo_candidate(locker_items, *weapon_loc,
                                               &worker, probe) != nullptr;
  }

  return readiness;
}

static std::string camp_locker_ranged_readiness_debug_summary(
    const camp_locker_ranged_readiness_state &readiness) {
  if (readiness.weapon.empty()) {
    return "none";
  }

  return string_format("weapon=%s mags=%d take=%d reload=%d ready=%s",
                       readiness.weapon, readiness.current_compatible_magazines,
                       readiness.magazines_to_take,
                       readiness.magazines_to_reload,
                       readiness.weapon_needs_reload ? "yes" : "no");
}

static bool service_camp_locker_ranged_readiness(
    npc &worker, const std::vector<tripoint_abs_ms> &locker_tiles,
    const tripoint_abs_ms &locker_drop_tile, const camp_locker_policy &policy) {
  const item_location weapon_loc = worker.get_wielded_item();
  if (!weapon_loc || !is_camp_locker_managed_ranged_weapon(*weapon_loc, policy)) {
    return false;
  }

  bool applied_changes = false;

  if (weapon_loc->uses_magazine()) {
    std::vector<item_location> compatible_magazines =
        collect_camp_locker_compatible_magazine_locations(worker, *weapon_loc);
    int magazines_needed =
        std::max(0, 2 - static_cast<int>(compatible_magazines.size()));
    while (magazines_needed-- > 0) {
      const item *locker_magazine = nullptr;
      const std::vector<const item *> locker_magazines =
          collect_camp_locker_compatible_magazines(
              collect_camp_locker_zone_items(
                  locker_tiles, std::vector<camp_locker_reservation>(),
                  worker.getID()),
              *weapon_loc);
      if (!locker_magazines.empty()) {
        locker_magazine = locker_magazines.front();
      }
      if (locker_magazine == nullptr) {
        break;
      }

      item moved_magazine =
          take_camp_locker_candidate(locker_tiles, locker_magazine);
      if (moved_magazine.is_null()) {
        continue;
      }

      item_location added_magazine =
          worker.try_add(moved_magazine, nullptr, nullptr, false);
      if (!added_magazine) {
        store_camp_locker_item(locker_drop_tile, std::move(moved_magazine));
        break;
      }
      applied_changes = true;
    }

    compatible_magazines =
        collect_camp_locker_compatible_magazine_locations(worker, *weapon_loc);
    if (compatible_magazines.size() > 2) {
      compatible_magazines.resize(2);
    }
    for (const item_location &magazine_loc : compatible_magazines) {
      applied_changes =
          reload_camp_locker_target_from_zone(worker, magazine_loc, locker_tiles,
                                              locker_drop_tile) ||
          applied_changes;
    }

    if (weapon_loc->ammo_remaining() <= 0) {
      const item_location loaded_magazine =
          select_best_loaded_camp_locker_magazine(
              collect_camp_locker_compatible_magazine_locations(worker,
                                                                *weapon_loc));
      item &reload_weapon = const_cast<item &>(*weapon_loc);
      if (loaded_magazine && reload_weapon.reload(worker, loaded_magazine, 1)) {
        applied_changes = true;
      }
    }
  } else {
    applied_changes =
        reload_camp_locker_target_from_zone(worker, weapon_loc, locker_tiles,
                                            locker_drop_tile) ||
        applied_changes;
  }

  return applied_changes;
}

static constexpr int camp_locker_medical_readiness_reserve_limit = 10;

struct camp_locker_medical_readiness_state {
  int current_supplies = 0;
  int supplies_to_take = 0;

  bool has_changes() const {
    return supplies_to_take > 0;
  }
};

static int count_camp_locker_medical_readiness_supplies(
    const std::vector<const item *> &items) {
  return static_cast<int>(std::count_if(
      items.begin(), items.end(), [](const item *it) {
        return it != nullptr && is_camp_locker_medical_readiness_supply(*it);
      }));
}

static camp_locker_medical_readiness_state
collect_camp_locker_medical_readiness_state(
    const std::vector<const item *> &worker_items,
    const std::vector<const item *> &locker_items) {
  camp_locker_medical_readiness_state readiness;
  readiness.current_supplies =
      count_camp_locker_medical_readiness_supplies(worker_items);
  const int desired_supplies = std::max(
      0, camp_locker_medical_readiness_reserve_limit - readiness.current_supplies);
  readiness.supplies_to_take = std::min(
      desired_supplies, count_camp_locker_medical_readiness_supplies(locker_items));
  return readiness;
}

static std::string camp_locker_medical_readiness_debug_summary(
    const camp_locker_medical_readiness_state &readiness) {
  if (!readiness.has_changes() && readiness.current_supplies <= 0) {
    return "none";
  }
  return string_format("current=%d take=%d", readiness.current_supplies,
                       readiness.supplies_to_take);
}

static bool is_better_camp_locker_medical_readiness_supply(const item &lhs,
                                                           const item &rhs) {
  const heal_actor *lhs_actor = camp_locker_heal_actor(lhs);
  const heal_actor *rhs_actor = camp_locker_heal_actor(rhs);
  if (lhs_actor == nullptr || rhs_actor == nullptr) {
    return rhs_actor != nullptr;
  }
  if (lhs_actor->bandages_power != rhs_actor->bandages_power) {
    return lhs_actor->bandages_power > rhs_actor->bandages_power;
  }
  if (lhs_actor->bleed != rhs_actor->bleed) {
    return lhs_actor->bleed > rhs_actor->bleed;
  }
  if (lhs_actor->move_cost != rhs_actor->move_cost) {
    return lhs_actor->move_cost < rhs_actor->move_cost;
  }
  return lhs.typeId().str() < rhs.typeId().str();
}

static const item *select_camp_locker_medical_readiness_supply(
    const std::vector<const item *> &items) {
  std::vector<const item *> supplies;
  for (const item *it : items) {
    if (it != nullptr && is_camp_locker_medical_readiness_supply(*it)) {
      supplies.push_back(it);
    }
  }

  std::stable_sort(supplies.begin(), supplies.end(),
                   [](const item *lhs, const item *rhs) {
                     if (lhs == nullptr || rhs == nullptr) {
                       return rhs != nullptr;
                     }
                     return is_better_camp_locker_medical_readiness_supply(
                         *lhs, *rhs);
                   });
  return supplies.empty() ? nullptr : supplies.front();
}

static bool service_camp_locker_medical_readiness(
    npc &worker, const std::vector<tripoint_abs_ms> &locker_tiles,
    const tripoint_abs_ms &locker_drop_tile) {
  bool applied_changes = false;
  int current_supplies = count_camp_locker_medical_readiness_supplies(
      collect_camp_locker_worker_items(worker));

  while (current_supplies < camp_locker_medical_readiness_reserve_limit) {
    const item *locker_supply = select_camp_locker_medical_readiness_supply(
        collect_camp_locker_zone_items(locker_tiles,
                                       std::vector<camp_locker_reservation>(),
                                       worker.getID()));
    if (locker_supply == nullptr) {
      break;
    }

    item moved_supply = take_camp_locker_candidate(locker_tiles, locker_supply);
    if (moved_supply.is_null()) {
      break;
    }

    item_location added_supply =
        worker.try_add(moved_supply, nullptr, nullptr, false);
    if (!added_supply) {
      store_camp_locker_item(locker_drop_tile, std::move(moved_supply));
      break;
    }

    current_supplies++;
    applied_changes = true;
  }

  return applied_changes;
}

static std::vector<tripoint_abs_ms>
collect_sorted_camp_storage_tiles(const tripoint_abs_ms &origin,
                                  const faction_id &fac,
                                  int range) {
  std::unordered_set<tripoint_abs_ms> storage_tiles =
      zone_manager::get_manager().get_near(zone_type_CAMP_STORAGE, origin, range,
                                           nullptr, fac);
  std::vector<tripoint_abs_ms> sorted_tiles(storage_tiles.begin(),
                                            storage_tiles.end());
  sort_tripoints_zyx(sorted_tiles);
  return sorted_tiles;
}

std::vector<tripoint_abs_ms>
collect_sorted_camp_locker_tiles(const tripoint_abs_ms &origin,
                                 const faction_id &fac,
                                 int range) {
  std::unordered_set<tripoint_abs_ms> locker_tiles =
      zone_manager::get_manager().get_near(zone_type_CAMP_LOCKER, origin, range,
                                           nullptr, fac);
  std::vector<tripoint_abs_ms> sorted_tiles(locker_tiles.begin(),
                                            locker_tiles.end());
  sort_tripoints_zyx(sorted_tiles);
  return sorted_tiles;
}

static std::optional<tripoint_abs_ms> find_camp_locker_cleanup_drop_tile(
    const npc &worker, const faction_id &fac,
    const std::vector<tripoint_abs_ms> &locker_tiles,
    const tripoint_abs_ms &preferred_tile, int range = MAX_VIEW_DISTANCE) {
  const std::unordered_set<tripoint_abs_ms> locker_tile_set(locker_tiles.begin(),
                                                            locker_tiles.end());
  map &here = get_map();
  const auto usable_drop_tile = [&here, &locker_tile_set](
                                  const tripoint_abs_ms &candidate) {
    return locker_tile_set.count(candidate) == 0 &&
           here.inbounds(here.get_bub(candidate));
  };

  if (preferred_tile != tripoint_abs_ms::zero && usable_drop_tile(preferred_tile)) {
    return preferred_tile;
  }

  const std::vector<tripoint_abs_ms> storage_tiles =
      collect_sorted_camp_storage_tiles(worker.pos_abs(), fac, range);
  for (const tripoint_abs_ms &storage_tile : storage_tiles) {
    if (usable_drop_tile(storage_tile)) {
      return storage_tile;
    }
  }

  if (usable_drop_tile(worker.pos_abs())) {
    return worker.pos_abs();
  }

  for (int radius = 1; radius <= range; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        const tripoint_abs_ms candidate =
            worker.pos_abs() + point(dx, dy);
        if (usable_drop_tile(candidate)) {
          return candidate;
        }
      }
    }
  }

  return std::nullopt;
}

static std::optional<tripoint_abs_ms> find_camp_locker_item_tile(
    const std::vector<tripoint_abs_ms> &locker_tiles, const item *target) {
  if (target == nullptr) {
    return std::nullopt;
  }

  map &here = get_map();
  for (const tripoint_abs_ms &tile : locker_tiles) {
    const tripoint_bub_ms local = here.get_bub(tile);
    if (!here.inbounds(local)) {
      continue;
    }
    for (const item &it : here.i_at(local)) {
      if (&it == target) {
        return tile;
      }
    }
  }

  return std::nullopt;
}

static item remove_worker_equipped_item(npc &worker, const item *target) {
  if (target == nullptr) {
    return item();
  }

  item_location wielded = worker.get_wielded_item();
  if (wielded && wielded.get_item() == target) {
    return worker.remove_weapon();
  }

  std::list<item> removed = worker.remove_worn_items_with(
      [target](item &it) { return &it == target; });
  if (removed.empty()) {
    return item();
  }

  item moved = std::move(removed.front());
  removed.pop_front();
  return moved;
}

item take_camp_locker_candidate(
    const std::vector<tripoint_abs_ms> &locker_tiles, const item *target) {
  if (target == nullptr) {
    return item();
  }

  map &here = get_map();
  for (const tripoint_abs_ms &tile : locker_tiles) {
    const tripoint_bub_ms local = here.get_bub(tile);
    if (!here.inbounds(local)) {
      continue;
    }
    for (item &it : here.i_at(local)) {
      if (&it != target) {
        continue;
      }
      item moved = it;
      here.i_rem(local, &it);
      return moved;
    }
  }

  return item();
}

static bool equip_camp_locker_item(npc &worker, camp_locker_slot slot, item &it) {
  if (it.is_null()) {
    return false;
  }

  if (slot == camp_locker_slot::melee_weapon ||
      slot == camp_locker_slot::ranged_weapon) {
    return worker.wield(it);
  }

  return worker.can_wear(it).success() &&
         worker.wear_item(it, false).has_value();
}

void store_camp_locker_item(const tripoint_abs_ms &locker_tile, item moved) {
  if (moved.is_null()) {
    return;
  }

  map &here = get_map();
  const tripoint_bub_ms local = here.get_bub(locker_tile);
  if (!here.inbounds(local)) {
    return;
  }
  here.add_item_or_charges(local, std::move(moved));
}

static std::string join_camp_locker_debug_parts(const std::vector<std::string> &parts) {
  if (parts.empty()) {
    return "none";
  }

  std::ostringstream out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      out << "; ";
    }
    out << parts[i];
  }
  return out.str();
}

std::string camp_locker_item_debug_label(const item &it) {
  return string_format("%s<%s>", it.tname(1, false), it.typeId().str());
}

static std::string camp_locker_candidate_debug_summary(
    const camp_locker_candidate_map &candidates) {
  std::vector<std::string> parts;
  for (const camp_locker_slot slot : all_camp_locker_slots()) {
    const auto found = candidates.find(slot);
    if (found == candidates.end() || found->second.empty()) {
      continue;
    }

    std::vector<std::string> labels;
    for (const item *candidate : found->second) {
      if (candidate != nullptr) {
        labels.push_back(camp_locker_item_debug_label(*candidate));
      }
    }
    if (!labels.empty()) {
      parts.push_back(string_format("%s=[%s]", camp_locker_slot_id(slot),
                                    join_camp_locker_debug_parts(labels)));
    }
  }

  return join_camp_locker_debug_parts(parts);
}

static std::string camp_locker_item_debug_summary(
    const std::vector<const item *> &items, const camp_locker_policy &policy) {
  return camp_locker_candidate_debug_summary(
      collect_camp_locker_candidates(items, policy));
}

static std::string camp_locker_plan_debug_summary(const camp_locker_plan &plan) {
  std::vector<std::string> parts;
  for (const camp_locker_slot slot : all_camp_locker_slots()) {
    const auto found = plan.find(slot);
    if (found == plan.end() || !found->second.has_changes()) {
      continue;
    }

    const camp_locker_slot_plan &slot_plan = found->second;
    if (slot_plan.selected_candidate != nullptr) {
      if (slot_plan.missing_current) {
        parts.push_back(string_format(
            "%s equip %s", camp_locker_slot_id(slot),
            camp_locker_item_debug_label(*slot_plan.selected_candidate)));
      } else if (slot_plan.upgrade_selected && slot_plan.kept_current != nullptr) {
        parts.push_back(string_format(
            "%s upgrade %s -> %s", camp_locker_slot_id(slot),
            camp_locker_item_debug_label(*slot_plan.kept_current),
            camp_locker_item_debug_label(*slot_plan.selected_candidate)));
      }
    }

    if (!slot_plan.duplicate_current.empty()) {
      std::vector<std::string> duplicates;
      for (const item *duplicate : slot_plan.duplicate_current) {
        if (duplicate != nullptr) {
          duplicates.push_back(camp_locker_item_debug_label(*duplicate));
        }
      }
      parts.push_back(string_format(
          "%s dedupe keep=%s drop=[%s]", camp_locker_slot_id(slot),
          slot_plan.kept_current != nullptr
              ? camp_locker_item_debug_label(*slot_plan.kept_current).c_str()
              : "none",
          join_camp_locker_debug_parts(duplicates)));
    }
  }

  return join_camp_locker_debug_parts(parts);
}

namespace {

constexpr const char *camp_locker_state_signature_key =
    "camp_locker_last_state_signature";
constexpr const char *camp_locker_probe_skip_reason_key =
    "camp_locker_probe_skip_reason";

struct camp_locker_live_state {
  std::vector<const item *> current_items;
  std::vector<const item *> worker_items;
  std::vector<const item *> locker_items;
  camp_locker_candidate_map locker_candidates;
  camp_locker_plan plan;
  camp_locker_carried_cleanup_state carried_cleanup;
  camp_locker_ranged_readiness_state ranged_readiness;
  camp_locker_medical_readiness_state medical_readiness;
};

bool camp_locker_plan_has_changes(const camp_locker_plan &plan) {
  return std::any_of(plan.begin(), plan.end(),
                     [](const camp_locker_plan::value_type &entry) {
                       return entry.second.has_changes();
                     });
}

camp_locker_live_state collect_camp_locker_live_state(
    npc &worker, const std::vector<tripoint_abs_ms> &locker_tiles,
    const camp_locker_policy &policy,
    const std::vector<camp_locker_reservation> &reservations,
    camp_locker_service_probe *probe = nullptr) {
  camp_locker_live_state live_state;
  live_state.current_items = collect_camp_locker_current_items(worker);
  live_state.worker_items = collect_camp_locker_worker_items(worker);
  live_state.locker_items = collect_camp_locker_zone_items(
      locker_tiles, reservations, worker.getID(), probe);
  live_state.locker_candidates = collect_camp_locker_candidates_impl(
      live_state.locker_items, policy, probe, &worker);
  live_state.plan = plan_camp_locker_loadout(
      live_state.current_items, live_state.locker_candidates, policy,
      get_weather().get_temperature(worker.pos_bub()),
      get_weather().weather_id->rains, &worker);
  live_state.carried_cleanup =
      collect_camp_locker_carried_cleanup_state(worker);
  live_state.ranged_readiness = collect_camp_locker_ranged_readiness_state(
      worker, policy, live_state.locker_items, probe);
  live_state.medical_readiness = collect_camp_locker_medical_readiness_state(
      live_state.worker_items, live_state.locker_items);
  return live_state;
}

camp_locker_live_state collect_camp_locker_live_state(
    npc &worker, const faction_id &fac, const camp_locker_policy &policy,
    const std::vector<camp_locker_reservation> &reservations) {
  return collect_camp_locker_live_state(
      worker, collect_sorted_camp_locker_tiles(worker.pos_abs(), fac), policy,
      reservations);
}

std::string camp_locker_live_state_signature(
    const camp_locker_live_state &live_state,
    const camp_locker_policy &policy) {
  return string_format(
      "worker=[%s]|locker=[%s]|plan=[%s]|cleanup=[%s]|ranged=[%s]|medical=[%s]",
      camp_locker_item_debug_summary(live_state.current_items, policy),
      camp_locker_candidate_debug_summary(live_state.locker_candidates),
      camp_locker_plan_debug_summary(live_state.plan),
      camp_locker_carried_cleanup_debug_summary(live_state.carried_cleanup),
      camp_locker_ranged_readiness_debug_summary(live_state.ranged_readiness),
      camp_locker_medical_readiness_debug_summary(live_state.medical_readiness));
}

} // namespace

const std::map<point_rel_omt, base_camps::direction_data>
    base_camps::all_directions = {
        // direction, direction id, tab order, direction abbreviation with
        // bracket, direction tab title
        {base_camps::base_dir,
         {"[B]", base_camps::TAB_MAIN, to_translation("base camp: base", "[B]"),
          to_translation("base camp: base", " MAIN ")}},
        {point_rel_omt::north,
         {"[N]", base_camps::TAB_N, to_translation("base camp: north", "[N]"),
          to_translation("base camp: north", "  [N] ")}},
        {point_rel_omt::north_east,
         {"[NE]", base_camps::TAB_NE,
          to_translation("base camp: northeast", "[NE]"),
          to_translation("base camp: northeast", " [NE] ")}},
        {point_rel_omt::east,
         {"[E]", base_camps::TAB_E, to_translation("base camp: east", "[E]"),
          to_translation("base camp: east", "  [E] ")}},
        {point_rel_omt::south_east,
         {"[SE]", base_camps::TAB_SE,
          to_translation("base camp: southeast", "[SE]"),
          to_translation("base camp: southeast", " [SE] ")}},
        {point_rel_omt::south,
         {"[S]", base_camps::TAB_S, to_translation("base camp: south", "[S]"),
          to_translation("base camp: south", "  [S] ")}},
        {point_rel_omt::south_west,
         {"[SW]", base_camps::TAB_SW,
          to_translation("base camp: southwest", "[SW]"),
          to_translation("base camp: southwest", " [SW] ")}},
        {point_rel_omt::west,
         {"[W]", base_camps::TAB_W, to_translation("base camp: west", "[W]"),
          to_translation("base camp: west", "  [W] ")}},
        {point_rel_omt::north_west,
         {"[NW]", base_camps::TAB_NW,
          to_translation("base camp: northwest", "[NW]"),
          to_translation("base camp: northwest", " [NW] ")}},
};

point_rel_omt base_camps::direction_from_id(const std::string &id) {
  for (const auto &dir : all_directions) {
    if (dir.second.id == id) {
      return dir.first;
        }
    }
  return base_dir;
}

std::string base_camps::faction_encode_short(const std::string &type) {
  return prefix + type + "_";
}

std::string base_camps::faction_encode_abs(const expansion_data &e,
                                           int number) {
  return faction_encode_short(e.type) + std::to_string(number);
}

std::string base_camps::faction_decode(std::string_view full_type) {
  if (full_type.size() < (prefix_len + 2)) {
    return "camp";
  }
  int last_bar = full_type.find_last_of('_');

  return std::string{
      full_type.substr(prefix_len, size_t(last_bar - prefix_len))};
}

static const time_duration work_day_hours_time = work_day_hours * 1_hours;

time_duration base_camps::to_workdays(const time_duration &work_time) {
  // logic here is duplicated in reverse in basecamp::time_to_food
  if (work_time < (work_day_hours + 1) * 1_hours) {
    return work_time;
    }
    int work_days = work_time / work_day_hours_time;
    time_duration excess_time = work_time - work_days * work_day_hours_time;
    return excess_time + 24_hours * work_days;
}

static std::map<std::string, int> max_upgrade_cache;

int base_camps::max_upgrade_by_type(const std::string &type) {
  if (max_upgrade_cache.find(type) == max_upgrade_cache.end()) {
    int max = -1;
    const std::string faction_base = faction_encode_short(type);
        while( recipe_id( faction_base + std::to_string( max + 1 ) ).is_valid() ) {
            max += 1;
        }
        max_upgrade_cache[type] = max;
    }
    return max_upgrade_cache[type];
}

basecamp::basecamp() = default;

basecamp_map::basecamp_map(const basecamp_map &) {}

basecamp_map &basecamp_map::operator=(const basecamp_map &) {
  map_.reset();
  return *this;
}

basecamp::basecamp(const std::string &name_, const tripoint_abs_omt &omt_pos_)
    : name(name_), omt_pos(omt_pos_) {
  parse_tags(name, get_player_character(), get_player_character());
}

basecamp::basecamp(const std::string &name_, const tripoint_abs_ms &bb_pos_,
                   const std::vector<point_rel_omt> &directions_,
                   const std::map<point_rel_omt, expansion_data> &expansions_)
    : directions(directions_), name(name_), bb_pos(bb_pos_),
      expansions(expansions_) {
  parse_tags(name, get_player_character(), get_player_character());
}

std::string basecamp::board_name() const {
  //~ Name of a basecamp
  return string_format(_("%s Board"), name);
}

void basecamp::set_by_radio(bool access_by_radio) {
  by_radio = access_by_radio;
}

// read an expansion's terrain ID of the form faction_base_$TYPE_$CURLEVEL
// find the last underbar, strip off the prefix of faction_base_ (which is 13
// chars), and the pull out the $TYPE and $CURLEVEL This is legacy support for
// existing camps; future camps don't use cur_level at all
expansion_data basecamp::parse_expansion(std::string_view terrain,
                                         const tripoint_abs_omt &new_pos) {
  expansion_data e;
  size_t last_bar = terrain.find_last_of('_');
  e.type =
      terrain.substr(base_camps::prefix_len, last_bar - base_camps::prefix_len);
  e.cur_level = std::stoi(str_cat("0", terrain.substr(last_bar + 1)));
  e.pos = new_pos;
  return e;
}

void basecamp::add_expansion(const std::string &terrain,
                             const tripoint_abs_omt &new_pos) {
  if (terrain.find(base_camps::prefix) == std::string::npos) {
    return;
  }

    const point_rel_omt dir = talk_function::om_simple_dir( omt_pos, new_pos );
    expansions[ dir ] = parse_expansion( terrain, new_pos );
    update_provides( terrain, expansions[ dir ] );
  directions.push_back(dir);
}

void basecamp::add_expansion(const std::string &bldg,
                             const tripoint_abs_omt &new_pos,
                             const point_rel_omt &dir) {
  expansion_data e;
  e.type = base_camps::faction_decode(bldg);
  e.cur_level = -1;
    e.pos = new_pos;
    expansions[ dir ] = e;
    directions.push_back( dir );
    update_provides( bldg, expansions[ dir ] );
  update_resources(bldg);
}

void basecamp::define_camp(const tripoint_abs_omt &p,
                           std::string_view camp_type, bool player_founded) {
  if (player_founded) {
    query_new_name(true);
  }
    omt_pos = p;
    const oter_id &omt_ref = overmap_buffer.ter( omt_pos );
    // purging the regions guarantees all entries will start with faction_base_
    for( const std::pair<std::string, tripoint_abs_omt> &expansion :
         talk_function::om_building_region( omt_pos, 1, true ) ) {
        add_expansion( expansion.first, expansion.second );
    }
    const std::string om_cur = omt_ref.id().c_str();
    if( om_cur.find( base_camps::prefix ) == std::string::npos ) {
        expansion_data e;
        e.type = base_camps::faction_decode( camp_type );
        e.cur_level = -1;
        e.pos = omt_pos;
    expansions[base_camps::base_dir] = e;
    const std::string direction = oter_get_rotation_string(omt_ref);
    if (player_founded) {
      const oter_id bcid(direction.empty()
                             ? "faction_base_camp_0"
                             : "faction_base_camp_new_0" + direction);
      overmap_buffer.ter_set(omt_pos, bcid);
    }
    update_provides(base_camps::faction_encode_abs(e, 0),
                         expansions[base_camps::base_dir] );
    } else {
        expansions[base_camps::base_dir] = parse_expansion( om_cur, omt_pos );
    }
}

/// Returns the description for the recipe of the next building @ref bldg
std::string basecamp::om_upgrade_description(const std::string &bldg,
                                             const mapgen_arguments &args,
                                             bool trunc) const {
  const recipe &making = recipe_id(bldg).obj();

  const requirement_data *reqs;
    time_duration base_time;
    const std::map<skill_id, int> *skills;

  if (making.is_blueprint()) {
    auto req_it = making.blueprint_build_reqs().reqs_by_parameters.find(args);
    cata_assert(req_it !=
                making.blueprint_build_reqs().reqs_by_parameters.end());
    const build_reqs &bld_reqs = req_it->second;
    reqs = &bld_reqs.consolidated_reqs;
    base_time = time_duration::from_moves(bld_reqs.time);
        skills = &bld_reqs.skills;
    } else {
        reqs = &making.simple_requirements();
        base_time = making.batch_duration( get_player_character() );
        skills = &making.required_skills;
    }

  std::vector<std::string> component_print_buffer;
  const int pane = FULL_SCREEN_WIDTH;
  const auto tools = reqs->get_folded_tools_list(pane, c_white, _inv, 1);
  const auto comps = reqs->get_folded_components_list(
      pane, c_white, _inv, making.get_component_filter(), 1);
  component_print_buffer.insert(component_print_buffer.end(), tools.begin(),
                                tools.end());
  component_print_buffer.insert(component_print_buffer.end(), comps.begin(),
                                comps.end());

  std::string comp;
  for (auto &elem : component_print_buffer) {
    str_append(comp, elem, "\n");
  }
  comp = string_format(_("Notes:\n%s\n\nSkills used: %s\n%s\n"),
                       making.description,
                       making.required_all_skills_string(*skills), comp);
  if (!trunc) {
    comp += string_format(_("Risk: None\nTime: %s\n"),
                          to_string(base_camps::to_workdays(base_time)));
    }
    return comp;
}

// upgrade levels
// legacy next upgrade
std::string basecamp::next_upgrade(const point_rel_omt &dir,
                                   const int offset) const {
  const auto &e = expansions.find(dir);
  if (e == expansions.end()) {
    return "null";
    }
    const expansion_data &e_data = e->second;

    int cur_level = -1;
    for( int i = 0; i < base_camps::max_upgrade_by_type( e_data.type ); i++ ) {
        const std::string candidate = base_camps::faction_encode_abs( e_data, i );
        if( e_data.provides.find( candidate ) == e_data.provides.end() ) {
            break;
        } else {
            cur_level = i;
        }
    }
    if( cur_level >= 0 ) {
        return base_camps::faction_encode_abs( e_data, cur_level + offset );
    }
  return "null";
}

bool basecamp::has_provides(const std::string &req,
                            const expansion_data &e_data, int level) const {
  for (const auto &provide : e_data.provides) {
    if (provide.first == req && provide.second > level) {
      return true;
        }
    }
  return false;
}

bool basecamp::has_provides(const std::string &req,
                            const std::optional<point_rel_omt> &dir,
                            int level) const {
  if (!dir) {
    for (const auto &e : expansions) {
      if (has_provides(req, e.second, level)) {
                return true;
            }
        }
    } else {
        const auto &e = expansions.find( *dir );
        if( e != expansions.end() ) {
            return has_provides( req, e->second, level );
        }
    }
  return false;
}

bool basecamp::has_water() const {
  // special case required for fbmh_well_north constructed between b9162 (Jun
  // 16, 2019) and b9644 (Sep 20, 2019)
  return has_provides("water_well") || has_provides("fbmh_well_north");
}

bool basecamp::allowed_access_by(Character &guy, bool water_request) const {
  // The owner can always access their own camp.
  if (fac() == guy.get_faction()) {
    return true;
  }
  // Sharing stuff also means sharing access.
  if (fac()->has_relationship(guy.get_faction()->id,
                              npc_factions::relationship::share_my_stuff)) {
    return true;
  }
  // Some factions will share access to infinite water sources, but not food
  if (water_request &&
      fac()->has_relationship(guy.get_faction()->id,
                              npc_factions::relationship::share_public_goods)) {
    return true;
  }
  return false;
}

std::vector<basecamp_upgrade>
basecamp::available_upgrades(const point_rel_omt &dir) {
  std::vector<basecamp_upgrade> ret_data;
  auto e = expansions.find(dir);
  if (e != expansions.end()) {
        expansion_data &e_data = e->second;
        for( const recipe *recp_p : recipe_dict.all_blueprints() ) {
            const recipe &recp = *recp_p;
            const std::string &bldg = recp.result().str();
            // skip buildings that are completed
            if( e_data.provides.find( bldg ) != e_data.provides.end() ) {
                continue;
            }
            // skip building that have unmet requirements
            size_t needed_requires = recp.blueprint_requires().size();
            size_t met_requires = 0;
            for( const auto &bp_require : recp.blueprint_requires() ) {
                if( e_data.provides.find( bp_require.first ) == e_data.provides.end() ) {
                    break;
                }
                if( e_data.provides[bp_require.first] < bp_require.second ) {
                    break;
                }
                met_requires += 1;
            }
            if( met_requires < needed_requires ) {
                continue;
            }
            bool should_display = true;
            bool in_progress = false;
            for( const auto &bp_exclude : recp.blueprint_excludes() ) {
                // skip buildings that are excluded by previous builds
                if( e_data.provides.find( bp_exclude.first ) != e_data.provides.end() ) {
                    if( e_data.provides[bp_exclude.first] >= bp_exclude.second ) {
                        should_display = false;
                        break;
          }
        }
        // track buildings that are currently being built
        if (e_data.in_progress.find(bp_exclude.first) !=
            e_data.in_progress.end()) {
          if (e_data.in_progress[bp_exclude.first] >= bp_exclude.second) {
            in_progress = true;
            break;
                    }
                }
            }
            if( !should_display ) {
        continue;
      }
      if (recp.blueprint_build_reqs().reqs_by_parameters.empty()) {
        debugmsg("blueprint recipe %s lacked any blueprint_build_reqs",
                 recp.result().str());
      }
      for (const std::pair<const mapgen_arguments, build_reqs> &args_and_reqs :
           recp.blueprint_build_reqs().reqs_by_parameters) {
        const mapgen_arguments &args = args_and_reqs.first;
        const requirement_data &reqs = args_and_reqs.second.consolidated_reqs;
        bool can_make = reqs.can_make_with_inventory(
            _inv, recp.get_component_filter(), 1, craft_flags::none, false);
        ret_data.push_back(
            {bldg, args, recp.blueprint_name(), can_make, in_progress});
      }
    }
  }
  return ret_data;
}

std::unordered_set<recipe_id> basecamp::recipe_deck_all() const {
  std::unordered_set<recipe_id> known_recipes;
  for (const npc_ptr &guy : assigned_npcs) {
    if (guy.get()) {
            for( const recipe *rec : guy->get_learned_recipes() ) {
                known_recipes.insert( rec->ident() );
            }
        }
    }

    for( const auto &exp_data_pair : expansions ) {
        for( const auto &provides : exp_data_pair.second.provides ) {
            const auto &test_s = recipe_group::get_recipes_by_id( provides.first );
            for( const std::pair<const recipe_id, translation> &rec_list : test_s ) {
                known_recipes.insert( rec_list.first );
            }
        }
    }

    return known_recipes;
}

// recipes and craft support functions
std::map<recipe_id, translation>
basecamp::recipe_deck(const point_rel_omt &dir) const {
  std::map<recipe_id, translation> recipes;

  const auto &e = expansions.find(dir);
    if( e == expansions.end() ) {
        return recipes;
    }
    for( const auto &provides : e->second.provides ) {
        const auto &test_s = recipe_group::get_recipes_by_id( provides.first );
        recipes.insert( test_s.cbegin(), test_s.cend() );
    }
  return recipes;
}

std::map<recipe_id, translation>
basecamp::recipe_deck(const std::string &bldg) const {
  return recipe_group::get_recipes_by_bldg(bldg);
}

void basecamp::add_resource(const itype_id &camp_resource) {
  basecamp_resource bcp_r;
  bcp_r.fake_id = camp_resource;
  item camp_item(bcp_r.fake_id, calendar::turn_zero);
    bcp_r.ammo_id = camp_item.ammo_default();
    resources.emplace_back( bcp_r );
  fuel_types.insert(bcp_r.ammo_id);
}

void basecamp::update_resources(const std::string &bldg) {
  if (!recipe_id(bldg).is_valid()) {
    return;
  }

    const recipe &making = recipe_id( bldg ).obj();
    for( const itype_id &bp_resource : making.blueprint_resources() ) {
        add_resource( bp_resource );
  }
}

void basecamp::update_provides(const std::string &bldg,
                               expansion_data &e_data) {
  if (!recipe_id(bldg).is_valid()) {
    debugmsg("Invalid basecamp recipe %s", bldg);
    return;
    }

    const recipe &making = recipe_id( bldg ).obj();
    for( const auto &bp_provides : making.blueprint_provides() ) {
        if( e_data.provides.find( bp_provides.first ) == e_data.provides.end() ) {
            e_data.provides[bp_provides.first] = 0;
        }
        e_data.provides[bp_provides.first] += bp_provides.second;
  }
}

void basecamp::update_in_progress(const std::string &bldg,
                                  const point_rel_omt &dir) {
  if (!recipe_id(bldg).is_valid()) {
    return;
  }
    auto e = expansions.find( dir );
    if( e == expansions.end() ) {
        return;
    }
    expansion_data &e_data = e->second;

  const recipe &making = recipe_id(bldg).obj();
  for (const auto &bp_provides : making.blueprint_provides()) {
    if (e_data.in_progress.find(bp_provides.first) ==
        e_data.in_progress.end()) {
      e_data.in_progress[bp_provides.first] = 0;
    }
    e_data.in_progress[bp_provides.first] += bp_provides.second;
  }
}

void basecamp::reset_camp_resources(map &here) {
  reset_camp_workers();
  for (auto &e : expansions) {
    expansion_data &e_data = e.second;
        for( int level = 0; level <= e_data.cur_level; level++ ) {
            const std::string &bldg = base_camps::faction_encode_abs( e_data, level );
            if( bldg == "null" ) {
                break;
            }
            update_provides( bldg, e_data );
        }
        for( const auto &bp_provides : e_data.provides ) {
            update_resources( bp_provides.first );
        }
        for( itype_id &it : e.second.available_pseudo_items ) {
            add_resource( it );
        }
    }
    form_crafting_inventory( here );
}

// available companion list manipulation
// get all the companions currently performing missions at this camp
void basecamp::reset_camp_workers() {
  camp_workers.clear();
  for (const auto &elem : overmap_buffer.get_companion_mission_npcs()) {
    npc_companion_mission c_mission = elem->get_companion_mission();
        if( c_mission.position == omt_pos && c_mission.role_id == "FACTION_CAMP" ) {
            camp_workers.push_back( elem );
        }
  }
}

void basecamp::add_assignee(character_id id) {
  npc_ptr npc_to_add = overmap_buffer.find_npc(id);
  if (!npc_to_add) {
    debugmsg("cant find npc to assign to basecamp, on the overmap_buffer");
        return;
    }
    npc_to_add->assigned_camp = omt_pos;
  assigned_npcs.push_back(npc_to_add);
  mark_camp_locker_dirty(*npc_to_add, true);
}

void basecamp::remove_assignee(character_id id) {
  npc_ptr npc_to_remove = overmap_buffer.find_npc(id);
  if (!npc_to_remove) {
    debugmsg("cant find npc to remove from basecamp, on the overmap_buffer");
    return;
  }
  interrupt_patrol_worker(id,
                          camp_patrol_interrupt_reason::explicit_reassignment);
  npc_to_remove->assigned_camp = std::nullopt;
  assigned_npcs.erase(
      std::remove(assigned_npcs.begin(), assigned_npcs.end(), npc_to_remove),
      assigned_npcs.end());
  locker_service_queue.erase(
      std::remove(locker_service_queue.begin(), locker_service_queue.end(), id),
      locker_service_queue.end());
  locker_reservations.erase(
      std::remove_if(locker_reservations.begin(), locker_reservations.end(),
                     [&id](const camp_locker_reservation &reservation) {
                       return reservation.worker_id == id;
                     }),
      locker_reservations.end());
}

void basecamp::validate_assignees() {
  std::vector<character_id> removed_workers;
  std::vector<npc_ptr>::iterator iter = assigned_npcs.begin();
  while (iter != assigned_npcs.end()) {
    if (!(*iter) || !(*iter)->assigned_camp ||
        *(*iter)->assigned_camp != omt_pos) {
      if (*iter) {
        removed_workers.push_back((*iter)->getID());
      }
      iter = assigned_npcs.erase(iter);
    } else {
      ++iter;
        }
    }
    for( character_id elem : g->get_follower_list() ) {
        npc_ptr npc_to_add = overmap_buffer.find_npc( elem );
    if (!npc_to_add) {
      continue;
    }
    if (std::find(assigned_npcs.begin(), assigned_npcs.end(), npc_to_add) !=
        assigned_npcs.end()) {
      continue;
    } else {
      if (npc_to_add->assigned_camp && *npc_to_add->assigned_camp == omt_pos) {
                assigned_npcs.push_back( npc_to_add );
            }
        }
    }
    // remove duplicates - for legacy handling.
    std::sort( assigned_npcs.begin(), assigned_npcs.end() );
    auto last = std::unique( assigned_npcs.begin(), assigned_npcs.end() );
  assigned_npcs.erase(last, assigned_npcs.end());
  if( patrol_shift_cache_valid ) {
    for( const character_id &worker_id : removed_workers ) {
      apply_camp_patrol_guard_interrupt(
          patrol_shift_cache, worker_id,
          camp_patrol_interrupt_reason::explicit_reassignment);
    }
  }
}

std::vector<npc_ptr> basecamp::get_npcs_assigned() {
  validate_assignees();
  return assigned_npcs;
}

void basecamp::hide_mission(ui_mission_id id) {
  const base_camps::direction_data &base_data =
      base_camps::all_directions.at(id.id.dir.value());
  for (ui_mission_id &miss_id : hidden_missions[size_t(base_data.tab_order)]) {
    if (is_equal(miss_id, id)) {
      return;
    } //  The UI shouldn't allow us to hide something already hidden, but check
      //  anyway.
  }
  hidden_missions[size_t(base_data.tab_order)].push_back(id);
}

void basecamp::reveal_mission(ui_mission_id id) {
  const base_camps::direction_data &base_data =
      base_camps::all_directions.at(id.id.dir.value());
  for (auto it = hidden_missions[size_t(base_data.tab_order)].begin();
       it != hidden_missions[size_t(base_data.tab_order)].end(); it++) {
    if (is_equal(id.id, it->id)) {
            hidden_missions[size_t( base_data.tab_order )].erase( it );
            return;
        }
    }
  debugmsg("Trying to reveal revealed mission.  Has no effect.");
}

bool basecamp::is_hidden(ui_mission_id id) {
  if (hidden_missions.empty()) {
    return false;
  }

    if( !id.id.dir ) {
    return false;
  }

  const base_camps::direction_data &base_data =
      base_camps::all_directions.at(id.id.dir.value());
  for (ui_mission_id &miss_id : hidden_missions[size_t(base_data.tab_order)]) {
    if (is_equal(miss_id, id)) {
      return true;
        }
    }
    return false;
}

// get the subset of companions working on a specific task
comp_list basecamp::get_mission_workers(const mission_id &miss_id,
                                        bool contains) {
  comp_list available;
  for (const auto &elem : camp_workers) {
    npc_companion_mission c_mission = elem->get_companion_mission();
        if( is_equal( c_mission.miss_id, miss_id ) ||
            ( contains && c_mission.miss_id.id == miss_id.id &&
              c_mission.miss_id.dir == miss_id.dir ) ) {
            available.push_back( elem );
        }
    }
  return available;
}

void basecamp::query_new_name(bool force) {
  string_input_popup_imgui input_popup(40);
  bool done = false;
  bool need_input = true;
    std::string text;
    do {
        input_popup.set_description( _( "Name this camp" ) );
        input_popup.set_max_input_length( 25 );
        text = input_popup.query();
        if( input_popup.cancelled() || text.empty() ) {
            if( name.empty() || force ) {
                popup( _( "You need to input the base camp name." ) );
            } else {
                need_input = false;
            }
        } else {
            done = true;
        }
    } while( !done && need_input );
    if( done ) {
        name = text;
  }
}

void basecamp::set_name(const std::string &new_name) {
  name = new_name;
  parse_tags(name, get_player_character(), get_player_character());
}

/*
 * we could put this logic in map::use_charges() the way the vehicle code does,
 * but I think that's sloppy
 */
std::list<item> basecamp::use_charges(const itype_id &fake_id, int &quantity) {
  std::list<item> ret;
  if (quantity <= 0) {
    return ret;
    }
    for( basecamp_resource &bcp_r : resources ) {
        if( bcp_r.fake_id == fake_id ) {
            item camp_item( bcp_r.fake_id, calendar::turn_zero );
            camp_item.charges = std::min( bcp_r.available, quantity );
            quantity -= camp_item.charges;
            bcp_r.available -= camp_item.charges;
            bcp_r.consumed += camp_item.charges;
            ret.push_back( camp_item );
            if( quantity <= 0 ) {
                break;
            }
        }
  }
  return ret;
}
void basecamp::form_storage_zones(map &here, const tripoint_abs_ms &abspos) {
  zone_manager &mgr = zone_manager::get_manager();
  if (here.check_vehicle_zones(here.get_abs_sub().z())) {
    mgr.cache_vzones();
    }
    // NPC camps may never have had bb_pos registered
    validate_bb_pos( project_to<coords::ms>( omt_pos ) );
  tripoint_bub_ms src_loc = here.get_bub(bb_pos) + point::north;
  std::vector<tripoint_abs_ms> possible_liquid_dumps;
  if (mgr.has_near(zone_type_CAMP_STORAGE, abspos, MAX_VIEW_DISTANCE)) {
    const std::vector<const zone_data *> zones = mgr.get_near_zones(
        zone_type_CAMP_STORAGE, abspos, MAX_VIEW_DISTANCE, get_owner());
    // Find the nearest unsorted zone to dump objects at
    if (!zones.empty()) {
      std::unordered_set<tripoint_abs_ms> src_set;
            for( const zone_data *zone : zones ) {
                for( const tripoint_abs_ms &p : tripoint_range<tripoint_abs_ms>(
                         zone->get_start_point(), zone->get_end_point() ) ) {
                    src_set.emplace( p );
                }
            }
            set_storage_tiles( src_set );
            src_loc = here.get_bub( zones.front()->get_center_point() );
        }
        map &here = get_map();
        for( const zone_data *zone : zones ) {
      if (zone->get_type() == zone_type_CAMP_STORAGE) {
        for (const tripoint_abs_ms &p : tripoint_range<tripoint_abs_ms>(
                 zone->get_start_point(), zone->get_end_point())) {
          if (here.has_flag_ter_or_furn(ter_furn_flag::TFLAG_LIQUIDCONT,
                                        here.get_bub(p))) {
            possible_liquid_dumps.emplace_back(p);
          }
        }
            }
        }
  }
  set_dumping_spot(here.get_abs(src_loc));
  set_liquid_dumping_spot(possible_liquid_dumps);
}
void basecamp::form_crafting_inventory(map &target_map) {
  _inv.clear();
  zone_manager &mgr = zone_manager::get_manager();
  map &here = get_map();
    if( here.check_vehicle_zones( here.get_abs_sub().z() ) ) {
        mgr.cache_vzones();
    }
    if( !src_set.empty() ) {
        _inv.form_from_zone( target_map, src_set, nullptr, false );
  }
  /*
   * something of a hack: add the resources we know the camp has
   * the hacky part is that we're adding resources based on the camp's flags,
   * which were generated based on upgrade missions, instead of having resources
   * added when the map changes
   */
  // make sure the array is empty
  fuels.clear();
    for( const itype_id &fuel_id : fuel_types ) {
        basecamp_fuel bcp_f;
        bcp_f.ammo_id = fuel_id;
        fuels.emplace_back( bcp_f );
    }

    // find available fuel

    for( const tripoint_abs_ms &abs_ms_pt : src_set ) {
        const tripoint_bub_ms &pt = target_map.get_bub( abs_ms_pt );
        if( target_map.accessible_items( pt ) ) {
            for( const item &i : target_map.i_at( pt ) ) {
                for( basecamp_fuel &bcp_f : fuels ) {
                    if( bcp_f.ammo_id == i.typeId() ) {
                        bcp_f.available += i.charges;
                        break;
                    }
        }
      }
    }
  }
  for (basecamp_resource &bcp_r : resources) {
    bcp_r.consumed = 0;
        item camp_item( bcp_r.fake_id, calendar::turn_zero );
        camp_item.set_flag( json_flag_PSEUDO );
        if( !bcp_r.ammo_id.is_null() ) {
            for( basecamp_fuel &bcp_f : fuels ) {
                if( bcp_f.ammo_id == bcp_r.ammo_id ) {
                    if( bcp_f.available > 0 ) {
                        bcp_r.available = bcp_f.available;
                        camp_item = camp_item.ammo_set( bcp_f.ammo_id, bcp_f.available );
                    }
                    break;
                }
            }
        }
    _inv.add_item(camp_item);
  }

  //  We're potentially adding the same item multiple times if present in
  //  multiple expansions, but we're already that with the resources above. The
  //  resources are stored in expansions rather than in a common pool to allow
  //  them to apply only to their respective expansion in the future.
  for (auto &expansion : expansions) {
    for (itype_id &it : expansion.second.available_pseudo_items) {
      item camp_item = item(it);
            if( camp_item.is_magazine() ) {
                for( basecamp_fuel &bcp_f : fuels ) {
                    if( camp_item.can_reload_with( item( bcp_f.ammo_id ), false ) ) {
                        if( bcp_f.available > 0 ) {
                            camp_item = camp_item.ammo_set( bcp_f.ammo_id, bcp_f.available );
                        }
                        break;
                    }
                }
            }

            _inv.add_item( camp_item );
        }
  }
}

map &basecamp::get_camp_map() {
  if (by_radio) {
    if (!camp_map.map_) {
      camp_map.map_ = std::make_unique<map>();
            camp_map.map_->load( project_to<coords::sm>( omt_pos ) - point( 5, 5 ), false );
        }
        return *camp_map.map_;
    }
  return get_map();
}

void basecamp::unload_camp_map() {
  if (camp_map.map_) {
    camp_map.map_.reset();
  }
}

void basecamp::set_owner(faction_id new_owner) {
  // Absolutely no safety checks, factions don't exist until you've encountered
  // them but we sometimes set the owner before that
  owner = new_owner;
  clear_patrol_shift_cache();
}

faction_id basecamp::get_owner() { return owner; }

bool basecamp::has_locker_zone() const {
  const faction_id fac_id = owner.is_valid() ? owner : your_fac;
  return zone_manager::get_manager().has_defined(zone_type_CAMP_LOCKER, fac_id);
}

bool basecamp::has_patrol_zone() const {
  const faction_id fac_id = owner.is_valid() ? owner : your_fac;
  return zone_manager::get_manager().has_defined(zone_type_CAMP_PATROL, fac_id);
}

tripoint_abs_ms basecamp::patrol_origin() const {
  tripoint_abs_ms origin = get_bb_pos_abs();
  if( origin.raw() == tripoint::zero ) {
    origin = project_to<coords::ms>(omt_pos);
  }
  return origin;
}

void basecamp::clear_patrol_shift_cache() {
  patrol_shift_cache_valid = false;
  patrol_shift_cache_day = -1;
  patrol_shift_cache_kind = camp_patrol_shift::day;
  patrol_shift_cache = camp_patrol_shift_plan();
}

bool basecamp::refresh_patrol_shift_cache() {
  validate_assignees();

  const int current_day = camp_patrol_shift_cache_day(calendar::turn);
  const camp_patrol_shift current_shift = camp_patrol_shift_for_turn(calendar::turn);
  if( patrol_shift_cache_valid && patrol_shift_cache_day == current_day &&
      patrol_shift_cache_kind == current_shift ) {
    return !patrol_shift_cache.clusters.empty() &&
           !patrol_shift_cache.roster.empty();
  }

  clear_patrol_shift_cache();
  if( !has_patrol_zone() ) {
    DebugLog( D_INFO, DC_ALL )
        << string_format( "camp patrol: cache camp=%s shift=%s reason=no_patrol_zone",
                          name, camp_patrol_shift_name( current_shift ) );
    return false;
  }

  const faction_id fac_id = owner.is_valid() ? owner : your_fac;
  const std::vector<camp_patrol_cluster> clusters =
      collect_camp_patrol_clusters(patrol_origin(), fac_id);
  if( clusters.empty() ) {
    DebugLog( D_INFO, DC_ALL )
        << string_format( "camp patrol: cache camp=%s shift=%s reason=no_clusters",
                          name, camp_patrol_shift_name( current_shift ) );
    return false;
  }

  const std::vector<camp_patrol_worker> workers =
      collect_camp_patrol_workers(get_npcs_assigned());
  const camp_patrol_plan plan = plan_camp_patrol(workers, clusters);

  patrol_shift_cache = current_shift == camp_patrol_shift::night ? plan.night : plan.day;
  patrol_shift_cache_valid = true;
  patrol_shift_cache_day = current_day;
  patrol_shift_cache_kind = current_shift;
  DebugLog( D_INFO, DC_ALL )
      << string_format(
             "camp patrol: cache camp=%s shift=%s workers=%zu roster=%zu active=%zu reserve=%zu clusters=%s",
             name, camp_patrol_shift_name( current_shift ), workers.size(),
             patrol_shift_cache.roster.size(), patrol_shift_cache.active_guards.size(),
             patrol_shift_cache.reserve_guards.size(),
             summarize_camp_patrol_cluster_tiles( clusters ) );
  return !patrol_shift_cache.roster.empty();
}

const camp_patrol_shift_plan *basecamp::get_current_patrol_shift_plan() {
  if( !refresh_patrol_shift_cache() ) {
    return nullptr;
  }
  return &patrol_shift_cache;
}

std::optional<camp_patrol_guard_runtime>
basecamp::get_current_patrol_runtime(const character_id &worker_id,
                                     const time_point &turn) {
  const camp_patrol_shift_plan *plan = get_current_patrol_shift_plan();
  if( plan == nullptr ) {
    return std::nullopt;
  }
  return describe_camp_patrol_guard_runtime(*plan, worker_id, turn);
}

bool basecamp::is_worker_on_patrol_shift(const npc &worker) {
  const camp_patrol_shift_plan *plan = get_current_patrol_shift_plan();
  if( plan == nullptr ) {
    return false;
  }

  return std::any_of(plan->active_guards.begin(), plan->active_guards.end(),
                     [&worker](const camp_patrol_guard_plan &guard_plan) {
                       return guard_plan.worker_id == worker.getID();
                     });
}

bool basecamp::interrupt_patrol_worker(
    const character_id &worker_id, camp_patrol_interrupt_reason reason) {
  const camp_patrol_shift_plan *plan = get_current_patrol_shift_plan();
  if( plan == nullptr ) {
    return false;
  }
  return apply_camp_patrol_guard_interrupt(patrol_shift_cache, worker_id, reason);
}

void basecamp::mark_camp_locker_dirty(npc &worker, bool high_priority) {
  const character_id worker_id = worker.getID();
  locker_service_queue.erase(
      std::remove(locker_service_queue.begin(), locker_service_queue.end(),
                  worker_id),
      locker_service_queue.end());
  if (high_priority) {
    locker_service_queue.insert(locker_service_queue.begin(), worker_id);
  } else {
    locker_service_queue.push_back(worker_id);
  }
}

bool basecamp::process_camp_locker_downtime(npc &worker) {
  if (!has_locker_zone()) {
    if( basecamp_ai::should_show_camp_job_report(
            worker, basecamp_ai::camp_job_report_kind::locker_exception, "no-locker-zone" ) ) {
      add_msg( m_info,
               _( "[camp][locker] %s cannot check camp locker gear: no Basecamp: Locker zone is defined." ),
               worker.disp_name() );
    }
    return false;
  }

  validate_assignees();

  locker_reservations.erase(
      std::remove_if(
          locker_reservations.begin(), locker_reservations.end(),
          [](const camp_locker_reservation &reservation) {
            return reservation.expires <= calendar::turn;
          }),
      locker_reservations.end());

  const diag_value &wake_dirty_value =
      worker.get_value("camp_locker_wake_dirty");
  const bool wake_dirty = wake_dirty_value.str() == "true";
  const diag_value &last_service_turn_value =
      worker.get_value("camp_locker_last_service_turn");
  const bool already_queued =
      std::find(locker_service_queue.begin(), locker_service_queue.end(),
                worker.getID()) != locker_service_queue.end();
  if ((last_service_turn_value.is_empty() && !already_queued) || wake_dirty) {
    mark_camp_locker_dirty(worker, wake_dirty);
    worker.set_value("camp_locker_wake_dirty", "false");
    DebugLog(D_INFO, DC_ALL)
        << string_format(
               "camp locker: queued %s wake_dirty=%s queue_size=%d next_turn=%d",
               worker.get_name(), wake_dirty ? "true" : "false",
               static_cast<int>(locker_service_queue.size()),
               to_turn<int>(locker_next_service_turn));
  }

  std::set<character_id> valid_workers;
  for (const npc_ptr &assignee : assigned_npcs) {
    if (assignee && assignee->assigned_camp && *assignee->assigned_camp == omt_pos) {
      valid_workers.insert(assignee->getID());
    }
  }
  const bool queued_before_cleanup =
      std::find(locker_service_queue.begin(), locker_service_queue.end(),
                worker.getID()) != locker_service_queue.end();
  locker_service_queue.erase(
      std::remove_if(locker_service_queue.begin(), locker_service_queue.end(),
                     [&valid_workers](const character_id &worker_id) {
                       return valid_workers.count(worker_id) == 0;
                     }),
      locker_service_queue.end());
  const bool queued_after_cleanup =
      std::find(locker_service_queue.begin(), locker_service_queue.end(),
                worker.getID()) != locker_service_queue.end();
  const bool worker_assigned_here =
      worker.assigned_camp && *worker.assigned_camp == omt_pos;
  if (queued_before_cleanup && !queued_after_cleanup) {
    DebugLog(D_INFO, DC_ALL)
        << string_format(
               "camp locker: dropped %s from queue during assignee cleanup assigned_here=%s valid_workers=%d assigned_npcs=%d",
               worker.get_name(), worker_assigned_here ? "true" : "false",
               static_cast<int>(valid_workers.size()),
               static_cast<int>(assigned_npcs.size()));
  }

  const faction_id fac_id = owner.is_valid() ? owner : your_fac;
  const camp_locker_live_state live_state = collect_camp_locker_live_state(
      worker, fac_id, locker_policy, locker_reservations);
  const std::string locker_state_signature =
      camp_locker_live_state_signature(live_state, locker_policy);
  const std::string previous_signature =
      worker.get_value(camp_locker_state_signature_key).str();
  const bool has_state_changes =
      camp_locker_plan_has_changes(live_state.plan) ||
      live_state.carried_cleanup.has_changes() ||
      live_state.ranged_readiness.has_changes() ||
      live_state.medical_readiness.has_changes();
  if (previous_signature != locker_state_signature && has_state_changes &&
      !queued_after_cleanup) {
    mark_camp_locker_dirty(worker);
    DebugLog(D_INFO, DC_ALL)
        << string_format(
               "camp locker: queued %s state-dirty queue_size=%d next_turn=%d plan=[%s] cleanup=[%s] ranged=[%s] medical=[%s]",
               worker.get_name(), static_cast<int>(locker_service_queue.size()),
               to_turn<int>(locker_next_service_turn),
               camp_locker_plan_debug_summary(live_state.plan),
               camp_locker_carried_cleanup_debug_summary(
                   live_state.carried_cleanup),
               camp_locker_ranged_readiness_debug_summary(
                   live_state.ranged_readiness),
               camp_locker_medical_readiness_debug_summary(
                   live_state.medical_readiness));
  }
  worker.set_value(camp_locker_state_signature_key, locker_state_signature);

  if (locker_service_queue.empty() ||
      locker_service_queue.front() != worker.getID() ||
      calendar::turn < locker_next_service_turn) {
    std::string skip_reason;
    if (locker_service_queue.empty()) {
      skip_reason = "queue-empty";
    } else if (locker_service_queue.front() != worker.getID()) {
      skip_reason = "waiting-behind-other-worker";
    } else {
      skip_reason = string_format("cooldown-until=%d",
                                  to_turn<int>(locker_next_service_turn));
    }
    if (worker.get_value(camp_locker_probe_skip_reason_key).str() != skip_reason) {
      const std::string last_service_turn_text = last_service_turn_value.is_empty()
                                                 ? "none"
                                                 : last_service_turn_value.to_string( false );
      DebugLog(D_INFO, DC_ALL)
          << string_format(
                 "camp locker: skip %s reason=%s queue_size=%d worker_in_queue=%s assigned_here=%s valid_workers=%d last_service=%s has_changes=%s now=%d next_turn=%d",
                 worker.get_name(), skip_reason,
                 static_cast<int>(locker_service_queue.size()),
                 queued_after_cleanup ? "true" : "false",
                 worker_assigned_here ? "true" : "false",
                 static_cast<int>(valid_workers.size()),
                 last_service_turn_text,
                 has_state_changes ? "true" : "false",
                 to_turn<int>(calendar::turn),
                 to_turn<int>(locker_next_service_turn));
      worker.set_value(camp_locker_probe_skip_reason_key, skip_reason);
    }
    return false;
  }
  worker.remove_value(camp_locker_probe_skip_reason_key);

  DebugLog(D_INFO, DC_ALL)
      << string_format("camp locker: servicing %s queue_size=%d now=%d",
                       worker.get_name(),
                       static_cast<int>(locker_service_queue.size()),
                       to_turn<int>(calendar::turn));
  const bool applied_changes = service_camp_locker(worker);
  locker_service_queue.erase(locker_service_queue.begin());
  worker.set_value("camp_locker_last_service_turn", to_turn<int>(calendar::turn));
  locker_next_service_turn =
      calendar::turn + (applied_changes ? 10_minutes : 2_minutes);

  const camp_locker_live_state post_service_state = collect_camp_locker_live_state(
      worker, fac_id, locker_policy, locker_reservations);
  worker.set_value(camp_locker_state_signature_key,
                   camp_locker_live_state_signature(post_service_state,
                                                    locker_policy));

  DebugLog(D_INFO, DC_ALL)
      << string_format(
             "camp locker: serviced %s applied=%s queue_remaining=%d next_turn=%d",
             worker.get_name(), applied_changes ? "true" : "false",
             static_cast<int>(locker_service_queue.size()),
             to_turn<int>(locker_next_service_turn));
  return applied_changes;
}

bool basecamp::service_camp_locker_impl(npc &worker,
                                       camp_locker_service_probe *probe,
                                       bool verbose_logging) {
  const faction_id fac_id = owner.is_valid() ? owner : your_fac;
  const std::vector<tripoint_abs_ms> locker_tiles =
      collect_sorted_camp_locker_tiles(worker.pos_abs(), fac_id);
  if (probe != nullptr) {
    probe->locker_tile_count = static_cast<int>(locker_tiles.size());
  }
  if (locker_tiles.empty()) {
    return false;
  }

  locker_reservations.erase(
      std::remove_if(
          locker_reservations.begin(), locker_reservations.end(),
          [&worker](const camp_locker_reservation &reservation) {
            return reservation.expires <= calendar::turn ||
                   reservation.worker_id == worker.getID();
          }),
      locker_reservations.end());

  const camp_locker_live_state live_state = collect_camp_locker_live_state(
      worker, locker_tiles, locker_policy, locker_reservations, probe);
  const int candidate_count =
      count_camp_locker_candidates(live_state.locker_candidates);
  if (probe != nullptr) {
    probe->current_item_count =
        static_cast<int>(live_state.current_items.size());
    probe->worker_item_count = static_cast<int>(live_state.worker_items.size());
    probe->locker_item_count = static_cast<int>(live_state.locker_items.size());
    probe->candidate_item_count = candidate_count;
    probe->changed_slot_count = count_changed_camp_locker_slots(live_state.plan);
    probe->cleanup_item_count = live_state.carried_cleanup.items_to_dump;
    probe->magazines_to_take = live_state.ranged_readiness.magazines_to_take;
    probe->magazines_to_reload = live_state.ranged_readiness.magazines_to_reload;
    probe->medical_supplies_to_take = live_state.medical_readiness.supplies_to_take;
  }
  if (live_state.plan.empty() && !live_state.carried_cleanup.has_changes() &&
      !live_state.ranged_readiness.has_changes() && !live_state.medical_readiness.has_changes()) {
    if (probe != nullptr) {
      probe->applied_changes = false;
    }
    if( verbose_logging ) {
        DebugLog(D_INFO, DC_ALL)
        << string_format(
               "camp locker: no-op for %s locker_tiles=%d current_items=%d candidates=%d reservations=%d cleanup=[%s] ranged=[%s] medical=[%s]",
               worker.get_name(), static_cast<int>(locker_tiles.size()),
               static_cast<int>(live_state.current_items.size()), candidate_count,
               static_cast<int>(locker_reservations.size()),
               camp_locker_carried_cleanup_debug_summary(live_state.carried_cleanup),
               camp_locker_ranged_readiness_debug_summary(live_state.ranged_readiness),
               camp_locker_medical_readiness_debug_summary(live_state.medical_readiness));
    }
    return false;
  }

  if( verbose_logging ) {
      DebugLog(D_INFO, DC_ALL)
      << string_format(
             "camp locker: before %s worker=[%s] locker=[%s] cleanup=[%s] ranged=[%s] medical=[%s]",
             worker.get_name(), camp_locker_item_debug_summary(live_state.current_items,
                                                               locker_policy),
             camp_locker_candidate_debug_summary(live_state.locker_candidates),
             camp_locker_carried_cleanup_debug_summary(live_state.carried_cleanup),
             camp_locker_ranged_readiness_debug_summary(live_state.ranged_readiness),
             camp_locker_medical_readiness_debug_summary(live_state.medical_readiness));
  }

  const time_point reservation_expiry = calendar::turn + 10_minutes;
  for (const auto &[slot, slot_plan] : live_state.plan) {
    if (slot_plan.selected_candidate == nullptr) {
      continue;
    }
    const std::optional<tripoint_abs_ms> candidate_tile =
        find_camp_locker_item_tile(locker_tiles, slot_plan.selected_candidate);
    if (!candidate_tile) {
      continue;
    }
    locker_reservations.push_back({worker.getID(), slot, *candidate_tile,
                                   slot_plan.selected_candidate->typeId(),
                                   reservation_expiry});
  }

  const int planned_slots = count_changed_camp_locker_slots(live_state.plan);
  if( verbose_logging ) {
      DebugLog(D_INFO, DC_ALL)
      << string_format(
             "camp locker: plan for %s locker_tiles=%d current_items=%d candidates=%d changed_slots=%d reservations=%d changes=[%s] cleanup=[%s] ranged=[%s] medical=[%s]",
             worker.get_name(), static_cast<int>(locker_tiles.size()),
             static_cast<int>(live_state.current_items.size()), candidate_count, planned_slots,
             static_cast<int>(locker_reservations.size()),
             camp_locker_plan_debug_summary(live_state.plan),
             camp_locker_carried_cleanup_debug_summary(live_state.carried_cleanup),
             camp_locker_ranged_readiness_debug_summary(live_state.ranged_readiness),
             camp_locker_medical_readiness_debug_summary(live_state.medical_readiness));
  }

  const tripoint_abs_ms locker_drop_tile = locker_tiles.front();
  const std::optional<tripoint_abs_ms> cleanup_drop_tile =
      find_camp_locker_cleanup_drop_tile(worker, fac_id, locker_tiles,
                                         get_dumping_spot());
  std::vector<item> displaced_items;
  bool applied_changes = false;

  struct removed_camp_locker_planned_item {
    camp_locker_slot slot = camp_locker_slot::underwear;
    const item *source = nullptr;
    item moved;
    camp_locker_extracted_contents extracted_contents;
  };

  std::vector<const item *> accepted_pre_removed_duplicate_sources;
  auto was_accepted_pre_removed_duplicate =
      [&accepted_pre_removed_duplicate_sources](const item *source) {
        return std::find(accepted_pre_removed_duplicate_sources.begin(),
                         accepted_pre_removed_duplicate_sources.end(),
                         source) !=
               accepted_pre_removed_duplicate_sources.end();
      };

  auto remove_planned_duplicate =
      [&](camp_locker_slot duplicate_slot, const item *duplicate,
          std::vector<removed_camp_locker_planned_item> &removed) {
        if (duplicate == nullptr ||
            was_accepted_pre_removed_duplicate(duplicate) ||
            std::any_of(removed.begin(), removed.end(),
                        [duplicate](const removed_camp_locker_planned_item &entry) {
                          return entry.source == duplicate;
                        })) {
          return true;
        }
        item *mutable_duplicate = const_cast<item *>(duplicate);
        camp_locker_extracted_contents extracted_duplicate_contents =
            extract_camp_locker_item_contents(*mutable_duplicate,
                                              cleanup_drop_tile.has_value());
        item moved_duplicate = remove_worker_equipped_item(worker, duplicate);
        if (moved_duplicate.is_null()) {
          restore_camp_locker_item_contents(*mutable_duplicate,
                                            extracted_duplicate_contents);
          return false;
        }
        removed.push_back({duplicate_slot, duplicate, std::move(moved_duplicate),
                           std::move(extracted_duplicate_contents)});
        return true;
      };

  auto restore_planned_duplicates =
      [&](std::vector<removed_camp_locker_planned_item> &removed) {
        for (auto it = removed.rbegin(); it != removed.rend(); ++it) {
          restore_camp_locker_item_contents(it->moved,
                                            it->extracted_contents);
          if (!equip_camp_locker_item(worker, it->slot, it->moved)) {
            store_camp_locker_item(locker_drop_tile, std::move(it->moved));
          }
        }
      };

  auto accept_planned_duplicates =
      [&](std::vector<removed_camp_locker_planned_item> &removed) {
        for (removed_camp_locker_planned_item &entry : removed) {
          applied_changes = true;
          applied_changes =
              place_camp_locker_item_contents(worker, entry.extracted_contents,
                                              locker_drop_tile,
                                              cleanup_drop_tile) ||
              applied_changes;
          accepted_pre_removed_duplicate_sources.push_back(entry.source);
          displaced_items.emplace_back(std::move(entry.moved));
        }
      };

  std::vector<const std::pair<const camp_locker_slot, camp_locker_slot_plan> *>
      apply_entries;
  apply_entries.reserve(live_state.plan.size());
  for (const auto &entry : live_state.plan) {
    if (entry.second.selected_candidate != nullptr) {
      apply_entries.push_back(&entry);
    }
  }
  std::stable_sort(
      apply_entries.begin(), apply_entries.end(),
      [](const auto *lhs, const auto *rhs) {
        return lhs->second.upgrade_selected && !rhs->second.upgrade_selected;
      });

  for (const auto *entry : apply_entries) {
    const camp_locker_slot slot = entry->first;
    const camp_locker_slot_plan &slot_plan = entry->second;

    item candidate =
        take_camp_locker_candidate(locker_tiles, slot_plan.selected_candidate);
    if (candidate.is_null()) {
      continue;
    }

    item replaced_current;
    camp_locker_extracted_contents extracted_replaced_contents;
    if (slot_plan.upgrade_selected && slot_plan.kept_current != nullptr) {
      item *mutable_current = const_cast<item *>(slot_plan.kept_current);
      extracted_replaced_contents = extract_camp_locker_item_contents(
          *mutable_current, cleanup_drop_tile.has_value());
      replaced_current =
          remove_worker_equipped_item(worker, slot_plan.kept_current);
      if (replaced_current.is_null()) {
        restore_camp_locker_item_contents(*mutable_current,
                                          extracted_replaced_contents);
        store_camp_locker_item(locker_drop_tile, std::move(candidate));
        continue;
      }
    }

    std::vector<removed_camp_locker_planned_item> pre_removed_duplicates;
    bool removed_all_planned_blockers = true;
    for (const item *duplicate : slot_plan.duplicate_current) {
      removed_all_planned_blockers =
          remove_planned_duplicate(slot, duplicate, pre_removed_duplicates) &&
          removed_all_planned_blockers;
    }
    if (is_camp_locker_protective_full_body_suit(candidate)) {
      for (const auto &blocker_entry : live_state.plan) {
        if (&blocker_entry == entry) {
          continue;
        }
        const camp_locker_slot_plan &blocker_plan = blocker_entry.second;
        if (blocker_plan.selected_candidate != nullptr ||
            blocker_plan.kept_current != nullptr) {
          continue;
        }
        for (const item *duplicate : blocker_plan.duplicate_current) {
          removed_all_planned_blockers =
              remove_planned_duplicate(blocker_entry.first, duplicate,
                                       pre_removed_duplicates) &&
              removed_all_planned_blockers;
        }
      }
    }

    if (removed_all_planned_blockers &&
        equip_camp_locker_item(worker, slot, candidate)) {
      applied_changes = true;
      applied_changes =
          place_camp_locker_item_contents(worker, extracted_replaced_contents,
                                          locker_drop_tile,
                                          cleanup_drop_tile) ||
          applied_changes;
      if (!replaced_current.is_null()) {
        displaced_items.emplace_back(std::move(replaced_current));
      }
      accept_planned_duplicates(pre_removed_duplicates);
      continue;
    }

    if (!replaced_current.is_null()) {
      restore_camp_locker_item_contents(replaced_current,
                                        extracted_replaced_contents);
      if (!equip_camp_locker_item(worker, slot, replaced_current)) {
        store_camp_locker_item(locker_drop_tile, std::move(replaced_current));
      }
    }
    restore_planned_duplicates(pre_removed_duplicates);
    store_camp_locker_item(locker_drop_tile, std::move(candidate));
  }

  for (const auto &plan_entry : live_state.plan) {
    const camp_locker_slot_plan &slot_plan = plan_entry.second;
    for (const item *duplicate : slot_plan.duplicate_current) {
      if (was_accepted_pre_removed_duplicate(duplicate)) {
        continue;
      }
      item *mutable_duplicate = const_cast<item *>(duplicate);
      camp_locker_extracted_contents extracted_duplicate_contents =
          extract_camp_locker_item_contents(*mutable_duplicate,
                                            cleanup_drop_tile.has_value());
      item removed = remove_worker_equipped_item(worker, duplicate);
      if (removed.is_null()) {
        restore_camp_locker_item_contents(*mutable_duplicate,
                                          extracted_duplicate_contents);
        continue;
      }
      applied_changes = true;
      applied_changes =
          place_camp_locker_item_contents(worker, extracted_duplicate_contents,
                                          locker_drop_tile,
                                          cleanup_drop_tile) ||
          applied_changes;
      displaced_items.emplace_back(std::move(removed));
    }
  }

  for (item &displaced : displaced_items) {
    store_camp_locker_item(locker_drop_tile, std::move(displaced));
  }

  if (cleanup_drop_tile) {
    std::list<item> removed_items = worker.remove_items_with(
        [&worker](const item &it) {
          return is_camp_locker_dumpable_carried_item(worker, it);
        });
    if (!removed_items.empty()) {
      applied_changes = true;
      if (probe != nullptr) {
        probe->cleanup_item_count = static_cast<int>(removed_items.size());
      }
      for (item &removed : removed_items) {
        store_camp_locker_item(*cleanup_drop_tile, std::move(removed));
      }
    }
  }

  applied_changes =
      service_camp_locker_ranged_readiness(worker, locker_tiles,
                                           locker_drop_tile, locker_policy) ||
      applied_changes;
  applied_changes =
      service_camp_locker_medical_readiness(worker, locker_tiles,
                                            locker_drop_tile) ||
      applied_changes;

  const std::vector<const item *> current_items_after =
      collect_camp_locker_current_items(worker);
  const std::vector<const item *> worker_items_after =
      collect_camp_locker_worker_items(worker);
  const std::vector<const item *> locker_items_after =
      collect_camp_locker_zone_items(locker_tiles, locker_reservations,
                                     worker.getID(), probe);
  const camp_locker_candidate_map locker_candidates_after =
      collect_camp_locker_candidates_impl(locker_items_after, locker_policy,
                                          probe, &worker);
  const camp_locker_ranged_readiness_state ranged_readiness_after =
      collect_camp_locker_ranged_readiness_state(worker, locker_policy,
                                                 locker_items_after, probe);
  const camp_locker_carried_cleanup_state carried_cleanup_after =
      collect_camp_locker_carried_cleanup_state(worker);
  const camp_locker_medical_readiness_state medical_readiness_after =
      collect_camp_locker_medical_readiness_state(worker_items_after,
                                                  locker_items_after);
  if( verbose_logging ) {
      DebugLog(D_INFO, DC_ALL)
      << string_format(
             "camp locker: after %s applied=%s worker=[%s] locker=[%s] cleanup=[%s] ranged=[%s] medical=[%s]",
             worker.get_name(), applied_changes ? "true" : "false",
             camp_locker_item_debug_summary(current_items_after, locker_policy),
             camp_locker_candidate_debug_summary(locker_candidates_after),
             camp_locker_carried_cleanup_debug_summary(carried_cleanup_after),
             camp_locker_ranged_readiness_debug_summary(
                 ranged_readiness_after),
             camp_locker_medical_readiness_debug_summary(
                 medical_readiness_after));
  }

  if (probe != nullptr) {
    probe->applied_changes = applied_changes;
  }
  return applied_changes;
}

bool basecamp::service_camp_locker(npc &worker) {
  return service_camp_locker_impl(worker, nullptr);
}

camp_locker_service_probe basecamp::measure_camp_locker_service(npc &worker) {
  camp_locker_service_probe probe;
  service_camp_locker_impl(worker, &probe, false);
  return probe;
}

std::string render_camp_locker_service_probe(
    const camp_locker_service_probe &probe) {
  return string_format(
      "locker_tiles=%d locker_items=%d current_items=%d worker_items=%d candidates=%d changed_slots=%d cleanup_items=%d mag_take=%d mag_reload=%d med_take=%d scans={collect_calls=%d tiles=%d top_level=%d returned=%d reserved=%d classify=%d accepted=%d mag_checks=%d ammo_checks=%d} applied=%s",
      probe.locker_tile_count, probe.locker_item_count, probe.current_item_count,
      probe.worker_item_count, probe.candidate_item_count,
      probe.changed_slot_count, probe.cleanup_item_count,
      probe.magazines_to_take, probe.magazines_to_reload,
      probe.medical_supplies_to_take, probe.metrics.zone_item_collection_calls,
      probe.metrics.zone_tile_visits,
      probe.metrics.zone_top_level_items_seen,
      probe.metrics.zone_items_returned,
      probe.metrics.zone_reserved_items_skipped,
      probe.metrics.candidate_item_checks,
      probe.metrics.candidate_items_accepted,
      probe.metrics.compatible_magazine_item_checks,
      probe.metrics.compatible_ammo_item_checks,
      probe.applied_changes ? "true" : "false" );
}

bool basecamp::is_locker_slot_enabled(camp_locker_slot slot) const {
  return locker_policy.is_enabled(slot);
}

void basecamp::set_locker_slot_enabled(camp_locker_slot slot, bool enabled) {
  if (locker_policy.is_enabled(slot) == enabled) {
    return;
  }
  locker_policy.set_enabled(slot, enabled);
  for (const npc_ptr &assignee : assigned_npcs) {
    if (assignee && assignee->assigned_camp && *assignee->assigned_camp == omt_pos) {
      mark_camp_locker_dirty(*assignee, true);
    }
  }
}

bool basecamp::locker_prefers_bulletproof() const {
  return locker_policy.prefers_bulletproof();
}

void basecamp::set_locker_prefers_bulletproof( bool enabled ) {
  if( locker_policy.prefers_bulletproof() == enabled ) {
    return;
  }
  locker_policy.set_prefers_bulletproof( enabled );
  for( const npc_ptr &assignee : assigned_npcs ) {
    if( assignee && assignee->assigned_camp && *assignee->assigned_camp == omt_pos ) {
      mark_camp_locker_dirty( *assignee, true );
    }
  }
}

void basecamp::handle_takeover_by(faction_id new_owner, bool violent_takeover) {
  get_event_bus().send<event_type::camp_taken_over>(get_owner(), new_owner,
                                                    name, violent_takeover);

  // If anyone was somehow assigned here, they aren't any longer.
  assigned_npcs.clear();
  camp_workers.clear();
  locker_service_queue.clear();
  locker_reservations.clear();

  add_msg_debug(debugmode::DF_CAMPS,
                "Camp %s owned by %s is being taken over by %s!", name,
                fac()->id.c_str(), new_owner->id.c_str());

  if (!violent_takeover) {
    set_owner(new_owner);
        return;
    }

    // Since it was violent, the old owner is going to be upset.
    // Currently only handles player faction, and very bluntly.
    if( new_owner == get_player_character().get_faction()->id ) {
        fac()->likes_u -= 100;
        fac()->trusts_u -= 100;
  }

  int num_of_owned_camps = 0;
  // Go over all camps in existence and count the ones belonging to current
  // owner
  // TODO: Remove this when camps stop being stored on the player
  for (tripoint_abs_omt camp_loc : get_player_character().camps) {
    std::optional<basecamp *> bcp = overmap_buffer.find_camp(camp_loc.xy());
        if( !bcp ) {
            continue;
        }
    basecamp *checked_camp = *bcp;
    // Note: Will count current basecamp object as well
    if (checked_camp->get_owner() == get_owner()) {
      add_msg_debug(
          debugmode::DF_CAMPS,
          "Camp %s at %s is owned by %s, adding it to plunder calculations.",
          checked_camp->camp_name(),
          checked_camp->camp_omt_pos().to_string_writable(),
          get_owner()->id.c_str());
      num_of_owned_camps++;
    }
  }

  if (num_of_owned_camps < 1) {
    debugmsg("Tried to take over a camp owned by %s, but they somehow own no "
             "camps!  Is the owner's faction id no longer valid?",
             get_owner().c_str());
    // Also abort the rest
    set_owner(new_owner);
    return;
  }

  // The faction taking over also seizes resources proportional to the number of
  // camps the previous owner had e.g. a single-camp faction has its entire
  // stockpile plundered, a 10-camp faction has 10% transferred
  nutrients captured_with_camp = fac()->food_supply() / num_of_owned_camps;
  nutrients taken_from_camp = -captured_with_camp;
  camp_food_supply(taken_from_camp);
  add_msg_debug(
      debugmode::DF_CAMPS,
      "Food supplies of %s plundered by %d kilocalories!  Total food supply "
      "reduced to %d kilocalories after losing %.1f%% of their camps.",
      fac()->id.c_str(), captured_with_camp.kcal(), fac()->food_supply().kcal(),
      1.0 / static_cast<double>(num_of_owned_camps) * 100.0);
  set_owner(new_owner);
    int previous_days_of_food = camp_food_supply_days( MODERATE_EXERCISE );
    // kinda a bug - rot time is lost here
    std::map<time_point, nutrients> added;
  added[calendar::turn_zero] = captured_with_camp;
  fac()->add_to_food_supply(added);
  add_msg_debug(debugmode::DF_CAMPS,
                "Food supply of new owner %s has increased to %d kilocalories "
                "due to takeover of camp %s!",
                fac()->id.c_str(), new_owner->food_supply().kcal(), name);
  if (new_owner == get_player_character().get_faction()->id) {
    popup(_("Through your looting of %s you found %d days worth of food and "
            "other resources."),
          name,
          camp_food_supply_days(MODERATE_EXERCISE) - previous_days_of_food);
  }
}

void basecamp::form_crafting_inventory() {
  map &here = get_camp_map();
  form_crafting_inventory(here);
  here.save();
}

// display names
std::string basecamp::expansion_tab(const point_rel_omt &dir) const {
  if (dir == base_camps::base_dir) {
    return _("Base Missions");
  }
  const auto &expansion_types =
      recipe_group::get_recipes_by_id("all_faction_base_expansions");

  const auto &e = expansions.find(dir);
  if (e != expansions.end()) {
        recipe_id id( base_camps::faction_encode_abs( e->second, 0 ) );
        const auto e_type = expansion_types.find( id );
        if( e_type != expansion_types.end() ) {
            //~ A particular faction camp / basecamp expansion
            return string_format( _( "%s Expansion" ), e_type->second );
        }
    }
  return _("Empty Expansion");
}

bool basecamp::point_within_camp(const tripoint_abs_omt &p) const {
  return std::any_of(expansions.begin(), expansions.end(),
                     [p](auto &e) { return p == e.second.pos; });
}

// legacy load and save
void basecamp::load_data(const std::string &data) {
  std::stringstream stream(data);
  stream >> name >> bb_pos.x() >> bb_pos.y();
  // add space to name
    replace( name.begin(), name.end(), '_', ' ' );
}

basecamp_action_components::basecamp_action_components(
    const recipe &making, const mapgen_arguments &args, int batch_size,
    basecamp &base)
    : making_(making), args_(args), batch_size_(batch_size), base_(base) {}

bool basecamp_action_components::choose_components() {
  const auto filter = is_crafting_component;
  avatar &player_character = get_avatar();
  const requirement_data *req;
    if( making_.is_blueprint() ) {
        const std::unordered_map<mapgen_arguments, build_reqs> &reqs_map =
            making_.blueprint_build_reqs().reqs_by_parameters;
        auto req_it = reqs_map.find( args_ );
        if( req_it == reqs_map.end() ) {
            debugmsg( "invalid argument selection for recipe" );
            return false;
        }
        req = &req_it->second.consolidated_reqs;
    } else {
        req = making_.deduped_requirements().select_alternative(
                  player_character, base_._inv, filter, batch_size_ );
    }
    if( !req ) {
        return false;
    }
    if( !item_selections_.empty() || !tool_selections_.empty() ) {
        debugmsg( "Reused basecamp_action_components" );
    return false;
  }
  for (const auto &it : req->get_components()) {
    comp_selection<item_comp> is = player_character.select_item_component(
        it, batch_size_, base_._inv, true, filter, !base_.by_radio);
    if (is.use_from == usage_from::cancel) {
      return false;
    }
        item_selections_.push_back( is );
  }
  // this may consume pseudo-resources from fake items
  for (const auto &it : req->get_tools()) {
    comp_selection<tool_comp> ts = player_character.select_tool_component(
        it, batch_size_, base_._inv, true, !base_.by_radio);
    if (ts.use_from == usage_from::cancel) {
      return false;
    }
        tool_selections_.push_back( ts );
    }
  return true;
}

void basecamp_action_components::consume_components() {
  map &target_map = base_.get_camp_map();
  avatar &player_character = get_avatar();
  std::vector<tripoint_bub_ms> src;
    src.reserve( base_.src_set.size() );
    for( const tripoint_abs_ms &p : base_.src_set ) {
    src.emplace_back(target_map.get_bub(p));
  }
  for (const comp_selection<item_comp> &sel : item_selections_) {
    std::list<item> consumed = player_character.consume_items(
        target_map, sel, batch_size_, is_crafting_component, src);
    for (item &comp : consumed) {
      consumed_components_.add(comp);
    }
    }
    // this may consume pseudo-resources from fake items
    for( const comp_selection<tool_comp> &sel : tool_selections_ ) {
        player_character.consume_tools( target_map, sel, batch_size_, src, &base_ );
    }
    // go back and consume the actual resources
    for( basecamp_resource &bcp_r : base_.resources ) {
        if( bcp_r.consumed > 0 ) {
            target_map.use_charges( src, bcp_r.ammo_id, bcp_r.consumed );
        }
    }
    target_map.save();
}

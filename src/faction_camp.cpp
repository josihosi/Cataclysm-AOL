#include "faction_camp.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "basecamp.h"
#include "build_reqs.h"
#include "calendar.h"
#include "cata_assert.h"
#include "cata_utility.h"
#include "cata_variant.h"
#include "catacharset.h"
#include "character.h"
#include "character_id.h"
#include "clzones.h"
#include "color.h"
#include "coordinates.h"
#include "crafting.h"
#include "crafting_gui.h"
#include "current_map.h"
#include "cursesdef.h"
#include "debug.h"
#include "enums.h"
#include "faction.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "global_vars.h"
#include "iexamine.h"
#include "input_context.h"
#include "input_enums.h"
#include "inventory.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_components.h"
#include "item_group.h"
#include "item_location.h"
#include "item_pocket.h"
#include "itype.h"
#include "kill_tracker.h"
#include "list.h"
#include "llm_prompt_templates.h"
#include "localized_comparator.h"
#include "map.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "mapdata.h"
#include "mapgen.h"
#include "mapgen_functions.h"
#include "mapgendata.h"
#include "math_parser_diag_value.h"
#include "mdarray.h"
#include "memory_fast.h"
#include "messages.h"
#include "mission.h"
#include "mission_companion.h"
#include "mongroup.h"
#include "npc.h"
#include "npctalk.h"
#include "omdata.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player_activity.h"
#include "point.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "recipe_groups.h"
#include "requirements.h"
#include "ret_val.h"
#include "rng.h"
#include "simple_pathfinding.h"
#include "skill.h"
#include "stomach.h"
#include "string_formatter.h"
#include "translation.h"
#include "translations.h"
#include "try_parse_integer.h"
#include "type_id.h"
#include "uilist.h"
#include "ui_manager.h"
#include "units.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "visitable.h"
#include "vpart_position.h"
#include "weather.h"

using basecamp_ai::parsed_camp_craft_order;
using basecamp_ai::parsed_camp_request_reference;

static const addiction_id addiction_alcohol( "alcohol" );

static const efftype_id effect_HACK_camp_vision_for_npc( "HACK_camp_vision_for_npc" );

static const faction_mission_id faction_mission_camp_clearcut( "camp_clearcut" );
static const faction_mission_id faction_mission_camp_collect_firewood( "camp_collect_firewood" );
static const faction_mission_id faction_mission_camp_combat_patrol( "camp_combat_patrol" );
static const faction_mission_id faction_mission_camp_cut_logs( "camp_cut_logs" );
static const faction_mission_id faction_mission_camp_foraging( "camp_foraging" );
static const faction_mission_id faction_mission_camp_gathering( "camp_gathering" );
static const faction_mission_id faction_mission_camp_harvest( "camp_harvest" );
static const faction_mission_id faction_mission_camp_hunting( "camp_hunting" );
static const faction_mission_id faction_mission_camp_menial( "camp_menial" );
static const faction_mission_id faction_mission_camp_plant( "camp_plant" );
static const faction_mission_id faction_mission_camp_plow( "camp_plow" );
static const faction_mission_id faction_mission_camp_relay_hide_site( "camp_relay_hide_site" );
static const faction_mission_id faction_mission_camp_scouting( "camp_scouting" );
static const faction_mission_id faction_mission_camp_setup_hide_site( "camp_setup_hide_site" );
static const faction_mission_id faction_mission_camp_survey_expansion( "camp_survey_expansion" );
static const faction_mission_id faction_mission_camp_survey_field( "camp_survey_field" );
static const faction_mission_id faction_mission_camp_trapping( "camp_trapping" );

static const furn_str_id furn_f_plant_harvest( "f_plant_harvest" );
static const furn_str_id furn_f_plant_seed( "f_plant_seed" );

static const item_group_id
Item_spawn_data_foraging_faction_camp_autumn( "foraging_faction_camp_autumn" );
static const item_group_id
Item_spawn_data_foraging_faction_camp_spring( "foraging_faction_camp_spring" );
static const item_group_id
Item_spawn_data_foraging_faction_camp_summer( "foraging_faction_camp_summer" );
static const item_group_id
Item_spawn_data_foraging_faction_camp_winter( "foraging_faction_camp_winter" );
static const item_group_id Item_spawn_data_forest( "forest" );
static const item_group_id
Item_spawn_data_gathering_faction_camp_firewood( "gathering_faction_camp_firewood" );

static const itype_id itype_camp_meal_large( "camp_meal_large" );
static const itype_id itype_camp_meal_medium( "camp_meal_medium" );
static const itype_id itype_camp_meal_small( "camp_meal_small" );
static const itype_id itype_duffelbag( "duffelbag" );
static const itype_id itype_fungal_seeds( "fungal_seeds" );
static const itype_id itype_log( "log" );
static const itype_id itype_marloss_seed( "marloss_seed" );
static const itype_id itype_seed_buckwheat( "seed_buckwheat" );
static const itype_id itype_stick_long( "stick_long" );

static const mongroup_id GROUP_CAMP_HUNTING( "GROUP_CAMP_HUNTING" );
static const mongroup_id GROUP_CAMP_HUNTING_LARGE( "GROUP_CAMP_HUNTING_LARGE" );
static const mongroup_id GROUP_CAMP_TRAPPING( "GROUP_CAMP_TRAPPING" );

static const oter_str_id oter_faction_hide_site_0( "faction_hide_site_0" );
static const oter_str_id oter_field( "field" );

static const oter_type_str_id oter_type_faction_hide_site_0( "faction_hide_site_0" );
static const oter_type_str_id oter_type_field( "field" );
static const oter_type_str_id oter_type_forest( "forest" );
static const oter_type_str_id oter_type_forest_thick( "forest_thick" );
static const oter_type_str_id oter_type_forest_trail( "forest_trail" );
static const oter_type_str_id oter_type_forest_trail_intersection( "forest_trail_intersection" );
static const oter_type_str_id oter_type_forest_water( "forest_water" );
static const oter_type_str_id oter_type_road( "road" );
static const oter_type_str_id oter_type_rural_road( "rural_road" );
static const oter_type_str_id oter_type_rural_road_forest( "rural_road_forest" );
static const oter_type_str_id oter_type_special_forest( "special_forest" );
static const oter_type_str_id oter_type_special_forest_thick( "special_forest_thick" );

static const skill_id skill_bashing( "bashing" );
static const skill_id skill_combat( "combat" );
static const skill_id skill_construction( "construction" );
static const skill_id skill_cutting( "cutting" );
static const skill_id skill_dodge( "dodge" );
static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_gun( "gun" );
static const skill_id skill_melee( "melee" );
static const skill_id skill_menial( "menial" );
static const skill_id skill_recruiting( "recruiting" );
static const skill_id skill_speech( "speech" );
static const skill_id skill_stabbing( "stabbing" );
static const skill_id skill_survival( "survival" );
static const skill_id skill_traps( "traps" );
static const skill_id skill_unarmed( "unarmed" );

static const ter_str_id ter_t_clay( "t_clay" );
static const ter_str_id ter_t_dirt( "t_dirt" );
static const ter_str_id ter_t_dirtmound( "t_dirtmound" );
static const ter_str_id ter_t_grass( "t_grass" );
static const ter_str_id ter_t_grass_dead( "t_grass_dead" );
static const ter_str_id ter_t_grass_golf( "t_grass_golf" );
static const ter_str_id ter_t_grass_long( "t_grass_long" );
static const ter_str_id ter_t_grass_tall( "t_grass_tall" );
static const ter_str_id ter_t_improvised_shelter( "t_improvised_shelter" );
static const ter_str_id ter_t_moss( "t_moss" );
static const ter_str_id ter_t_sand( "t_sand" );
static const ter_str_id ter_t_stump( "t_stump" );
static const ter_str_id ter_t_tree_young( "t_tree_young" );
static const ter_str_id ter_t_trunk( "t_trunk" );

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );

static const update_mapgen_id
update_mapgen_faction_expansion_salt_water_pipe_N( "faction_expansion_salt_water_pipe_N" );
static const update_mapgen_id
update_mapgen_faction_expansion_salt_water_pipe_NE( "faction_expansion_salt_water_pipe_NE" );
static const update_mapgen_id
update_mapgen_faction_expansion_salt_water_pipe_swamp_N( "faction_expansion_salt_water_pipe_swamp_N" );
static const update_mapgen_id
update_mapgen_faction_expansion_salt_water_pipe_swamp_NE( "faction_expansion_salt_water_pipe_swamp_NE" );
static const update_mapgen_id update_mapgen_faction_wall_level_E_0( "faction_wall_level_E_0" );
static const update_mapgen_id update_mapgen_faction_wall_level_E_1( "faction_wall_level_E_1" );
static const update_mapgen_id update_mapgen_faction_wall_level_N_0( "faction_wall_level_N_0" );
static const update_mapgen_id update_mapgen_faction_wall_level_N_1( "faction_wall_level_N_1" );
static const update_mapgen_id update_mapgen_faction_wall_level_S_0( "faction_wall_level_S_0" );
static const update_mapgen_id update_mapgen_faction_wall_level_S_1( "faction_wall_level_S_1" );
static const update_mapgen_id update_mapgen_faction_wall_level_W_0( "faction_wall_level_W_0" );
static const update_mapgen_id update_mapgen_faction_wall_level_W_1( "faction_wall_level_W_1" );

static const zone_type_id zone_type_CAMP_FOOD( "CAMP_FOOD" );
static const zone_type_id zone_type_CAMP_STORAGE( "CAMP_STORAGE" );

static const std::string faction_wall_level_n_0_string = "faction_wall_level_N_0";
static const std::string faction_wall_level_n_1_string = "faction_wall_level_N_1";

static const std::string camp_om_fortifications_trench_parameter = faction_wall_level_n_0_string;
static const std::string camp_om_fortifications_spiked_trench_parameter =
    faction_wall_level_n_1_string;

static const std::string var_time_between_succession =
    "time_between_succession";

static const std::string var_timer_time_of_last_succession =
    "timer_time_of_last_succession";

//  These strings are matched against recipe group 'building_type'. Definite candidates for JSON definitions of
//  the various UI strings corresponding to these groups.
// TODO: Replace with enum?
static const std::string base_recipe_group_string = "BASE";
static const std::string cook_recipe_group_string = "COOK";
static const std::string farm_recipe_group_string = "FARM";
static const std::string smith_recipe_group_string = "SMITH";

struct mass_volume {
    units::mass wgt = 0_gram;
    units::volume vol = 0_ml;
    int count = 0;
};

namespace base_camps
{
static const translation recover_ally_string = to_translation( "Recover Ally, " );
static const translation expansion_string = to_translation( " Expansion" );

static recipe_id select_camp_option( const std::map<recipe_id, translation> &pos_options,
                                     const std::string &option );
} // namespace base_camps

/**** Forward declaration of utility functions */
/**
 * Counts or destroys and drops the bash items of all terrain that matches @ref t in the map tile
 * @param comp NPC companion
 * @param omt_tgt the targeted OM tile
 * @param t terrain you are looking for
 * @param chance chance of destruction, 0 to 100
 * @param estimate if true, non-destructive count of the furniture
 * @param bring_back force the destruction of the furniture and bring back the drop items
 */
static int om_harvest_ter( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t,
                           int chance = 100,
                           bool estimate = false, bool bring_back = true );
// om_harvest_ter helper function that counts the furniture instances
static int om_harvest_ter_est( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t,
                               int chance = 100 );
static int om_harvest_ter_break( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t,
                                 int chance = 100 );
/// Collects all items in @ref omt_tgt with a @ref chance between 0 - 1.0, returns total
/// mass and volume
/// @ref take, whether you take the item or count it
static mass_volume om_harvest_itm( const npc_ptr &comp, const tripoint_abs_omt &omt_tgt,
                                   int chance = 100,
                                   bool take = true );
static void apply_camp_ownership( map &here, const tripoint_bub_ms &camp_pos, int radius );
/*
 * Counts or cuts trees into trunks and trunks into logs
 * @param omt_tgt the targeted OM tile
 * @param chance chance of destruction, 0 to 100
 * @param estimate if true, non-destructive count of trees
 * @force_cut_trunk if true and estimate is false, chop tree trunks into logs
 */
static int om_cutdown_trees( const tripoint_abs_omt &omt_tgt, int chance = 100,
                             bool estimate = false,
                             bool force_cut_trunk = true );
static int om_cutdown_trees_est( const tripoint_abs_omt &omt_tgt, int chance = 100 );
static int om_cutdown_trees_logs( const tripoint_abs_omt &omt_tgt, int chance = 100 );
static int om_cutdown_trees_trunks( const tripoint_abs_omt &omt_tgt, int chance = 100 );

/// Creates an improvised shelter at @ref omt_tgt and dumps the @ref itms into the building
static bool om_set_hide_site( npc &comp, const tripoint_abs_omt &omt_tgt,
                              const drop_locations &itms,
                              const drop_locations &itms_rem = {} );
/**
 * Opens the overmap so that you can select points for missions or constructions.
 * @param omt_pos start position, used for calculating travel distances
 * @param min_range
 * @param range max number of OM tiles the user can select
 * @param possible_om_types requires the user to reselect if the OM picked isn't in the list
 * @param must_see whether the user can select points in the unknown/fog of war
 * @param popup_notice toggles if the user should be shown ranges before being allowed to pick
 * @param source if you are selecting multiple points this is where the OM is centered to start
 * @param bounce
 */
static tripoint_abs_omt om_target_tile(
    const tripoint_abs_omt &omt_pos, int min_range = 1, int range = 1,
    const std::unordered_set<oter_type_str_id> &possible_om_types = {}, bool must_see = true,
    const tripoint_abs_omt &source = tripoint_abs_omt::invalid, bool bounce = false,
    const std::optional<std::string> &message = std::nullopt );
/**
 * Select waypoints and plot a path for a companion to travel
 * @param start start point
 * @param range_start maximum total distance traveled
 * @param bounce does visiting a faction hide site extend the range?
*/
static pf::simple_path<tripoint_abs_omt> om_companion_path(
    const tripoint_abs_omt &start, int range_start = 90, bool bounce = true );
/**
 * Can be used to calculate total trip time for an NPC mission or just the traveling portion.
 * @param journey path they will follow
 * @param trips how many trips back and forth the NPC will make
 */
static time_duration companion_travel_time_calc( const pf::simple_path<tripoint_abs_omt> &journey,
        int trips = 1 );
/// Determines how many trips it takes to move @ref mass and @ref volume of items
/// with @ref carry_mass and @ref carry_volume moved per trip, assuming round trips
static int om_carry_weight_to_trips( const units::mass &total_mass,
                                     const units::volume &total_volume, const npc_ptr &comp = nullptr );
static int om_carry_weight_to_trips( const units::mass &mass, const units::volume &volume,
                                     const units::mass &carry_mass, const units::volume &carry_volume );
/// Formats the variables into a standard looking description to be displayed in a ynquery window
static std::string camp_trip_description( const time_duration &total_time,
        const time_duration &working_time,
        const time_duration &travel_time,
        int dist_m, int trips, int need_food );

/*
 * check if a companion survives a random encounter
 * @param comp the companion
 * @param situation what the survivor is doing
 * @param favor a number added to the survivor's skills to see if he can avoid the encounter
 * @param threat a number indicating how dangerous the encounter is
 * TODO: Convert to JSON basic on dynamic line type structure
 */
static bool survive_random_encounter( npc &comp, std::string &situation, int favor, int threat );

static std::string mission_ui_activity_of( const mission_id &miss_id )
{
    const std::string dir_abbr = base_camps::all_directions.at(
                                     miss_id.dir.value() ).bracket_abbr.translated();

    switch( miss_id.id ) {
        case No_Mission:
            //  Should only happen for the unhiding functionality
            return _( "Obsolete mission" );

        case Camp_Distribute_Food:
            return _( "Distribute Food" );

        case Camp_Determine_Leadership:
            return _( "Choose New Leader" );

        case Camp_Have_Meal:
            return _( "Have A Meal" );

        case Camp_Requests:
            return _( "Crafting Requests" );

        case Camp_Hide_Mission:
            return _( "Hide Mission(s)" );

        case Camp_Reveal_Mission:
            return _( "Reveal Hidden Mission(s)" );

        case Camp_Assign_Jobs:
            return _( "Assign Jobs" );

        case Camp_Assign_Workers:
            return _( "Assign Workers" );

        case Camp_Abandon:
            return _( "Abandon Camp" );

        case Camp_Upgrade:
            return dir_abbr + _( " Upgrade Camp " );

        case Camp_Crafting:
            return dir_abbr + _( " Crafting" );

        case Camp_Gather_Materials:
            return dir_abbr + _( " Gather Materials" );

        case Camp_Collect_Firewood:
            return dir_abbr + _( " Collect Firewood" );

        case Camp_Menial:
            return dir_abbr + _( " Menial Labor" );

        case Camp_Survey_Field:
            return _( "Survey terrain and try to convert it to Field" );

        case Camp_Survey_Expansion:
            return _( "Expand Base" );

        case Camp_Cut_Logs:
            return dir_abbr + _( " Cut Logs" );

        case Camp_Clearcut:
            return dir_abbr + _( " Clear a forest" );

        case Camp_Setup_Hide_Site:
            return dir_abbr + _( " Setup Hide Site" );

        case Camp_Relay_Hide_Site:
            return dir_abbr + _( " Relay Hide Site" );

        case Camp_Foraging:
            return dir_abbr + _( " Forage for plants" );

        case Camp_Trapping:
            return dir_abbr + _( " Trap Small Game" );

        case Camp_Hunting:
            return dir_abbr + _( " Hunt Large Animals" );

        case Camp_OM_Fortifications:
            if( miss_id.parameters == camp_om_fortifications_trench_parameter ) {
                return dir_abbr + _( " Construct Map Fortifications" );
            } else {
                return dir_abbr + _( " Construct Spiked Trench" );
            }

        case Camp_Recruiting:
            return dir_abbr + _( " Recruit Companions" );

        case Camp_Scouting:
            return dir_abbr + _( " Scout Mission" );

        case Camp_Combat_Patrol:
            return dir_abbr + _( " Combat Patrol" );

        case Camp_Plow:
            return dir_abbr + _( " Plow Fields" );

        case Camp_Plant:
            return dir_abbr + _( " Plant Fields" );

        case Camp_Harvest:
            return dir_abbr + _( " Harvest Fields" );

        //  Actions that won't be used here
        case Scavenging_Patrol_Job:
        case Scavenging_Raid_Job:
        case Menial_Job:
        case Carpentry_Job:
        case Forage_Job:
        case Caravan_Commune_Center_Job:
        case Camp_Emergency_Recall:
        default:
            return "";

    }
}

static std::map<std::string, comp_list> companion_per_recipe_building_type( comp_list &npc_list )
{
    std::map<std::string, comp_list> result;

    for( const npc_ptr &comp : npc_list ) {
        const mission_id miss_id = comp->get_companion_mission().miss_id;
        const std::string bldg = recipe_group::get_building_of_recipe( miss_id.parameters );

        if( result[bldg].empty() ) {
            comp_list temp;
            result.insert( std::pair<std::string, comp_list>( bldg, temp ) );
        }
        result[bldg].emplace_back( comp );
    }
    return result;
}

static bool update_time_left( std::string &entry, const comp_list &npc_list )
{
    bool avail = false;
    Character &player_character = get_player_character();
    for( const auto &comp : npc_list ) {
        entry += comp->get_name();
        if( comp->companion_mission_time_ret < calendar::turn ) {
            entry +=  _( " [DONE]\n" );
            avail = true;
        } else {
            entry += " [" +
                     to_string( comp->companion_mission_time_ret - calendar::turn ) +
                     _( " left]\n" );
            avail = player_character.has_trait( trait_DEBUG_HS );
        }
    }
    if( avail ) {
        entry += _( "\n\nDo you wish to bring your allies back into your party?" );
    }
    return avail;
}

static bool update_time_fixed( std::string &entry, const comp_list &npc_list,
                               const time_duration &duration )
{
    bool avail = false;
    for( const auto &comp : npc_list ) {
        const time_duration elapsed = calendar::turn - comp->companion_mission_time;
        entry += "\n  " +  comp->get_name() + " [" + to_string( elapsed ) + " / " +
                 to_string( duration ) + "]";
        avail |= elapsed >= duration;
    }
    if( avail ) {
        entry += _( "\n\nDo you wish to bring your allies back into your party?" );
    }
    return avail;
}

static bool update_emergency_recall( std::string &entry, const comp_list &npc_list,
                                     const time_duration &duration )
{
    bool avail = false;
    for( const auto &comp : npc_list ) {
        const time_duration elapsed = calendar::turn - comp->companion_mission_time;
        const mission_id miss_id = comp->get_companion_mission().miss_id;

        entry += "\n  " + comp->get_name() + " [" + to_string( elapsed ) + " / " +
                 to_string( duration ) + "] " + mission_ui_activity_of( miss_id );
        avail |= elapsed >= duration;
    }
    if( avail ) {
        entry += _( "\n\nDo you wish to bring your allies back into your party?" );
    }
    return avail;
}

static bool extract_and_check_orientation_flags( const recipe_id &recipe,
        const point_rel_omt &dir,
        bool &mirror_horizontal,
        bool &mirror_vertical,
        int &rotation,
        std::string_view base_error_message,
        const std::string &actor )
{
    mirror_horizontal = recipe->has_flag( "MAP_MIRROR_HORIZONTAL" );
    mirror_vertical = recipe->has_flag( "MAP_MIRROR_VERTICAL" );
    rotation = 0;
    std::string dir_string;

    const auto check_rotation = [&]( const std::string & flag, int rotation_value ) {
        if( recipe->has_flag( flag ) ) {
            if( rotation != 0 ) {
                debugmsg(
                    "%s, the blueprint specifies multiple concurrent rotations, which is not "
                    "supported",
                    string_format( base_error_message, actor, recipe->get_blueprint().str() ) );
                return false;
            }
            rotation = rotation_value;
        }
        return true;
    };

    if( !check_rotation( "MAP_ROTATE_90", 1 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_180", 2 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_270", 3 ) ) {
        return false;
    }

    if( dir == point_rel_omt::north_west ) {
        dir_string = "NW";
    } else if( dir == point_rel_omt::north ) {
        dir_string = "N";
    } else if( dir == point_rel_omt::north_east ) {
        dir_string = "NE";
    } else if( dir == point_rel_omt::west ) {
        dir_string = "W";
    } else if( dir == point_rel_omt::zero ) {
        dir_string.clear();  //  Will result in "hidden" flags that can actually affect the core.
    } else if( dir == point_rel_omt::east ) {
        dir_string = "E";
    } else if( dir == point_rel_omt::south_west ) {
        dir_string = "SW";
    } else if( dir == point_rel_omt::south ) {
        dir_string = "S";
    } else if( dir == point_rel_omt::south_east ) {
        dir_string = "SE";
    }

    if( recipe->has_flag( "MAP_MIRROR_HORIZONTAL_IF_" + dir_string ) ) {
        if( mirror_horizontal ) {
            debugmsg(
                "%s, the blueprint specifies multiple concurrent horizontal mirroring, which is "
                "not supported",
                string_format( base_error_message, actor, recipe->get_blueprint().str() ) );
            return false;
        }
        mirror_horizontal = true;
    }

    if( recipe->has_flag( "MAP_MIRROR_VERTICAL_IF_" + dir_string ) ) {
        if( mirror_vertical ) {
            debugmsg(
                "%s, the blueprint specifies multiple concurrent vertical mirroring, which is not "
                "supported",
                string_format( base_error_message, actor, recipe->get_blueprint().str() ) );
            return false;
        }
        mirror_vertical = true;
    }

    if( !check_rotation( "MAP_ROTATE_90_IF_" + dir_string, 1 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_180_IF_" + dir_string, 2 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_270_IF_" + dir_string, 3 ) ) {
        return false;
    }

    return true;
}

static std::optional<basecamp *> get_basecamp( npc &p,
        std::string_view camp_type = "default" )
{
    tripoint_abs_omt omt_pos = p.pos_abs_omt();
    std::optional<basecamp *> bcp = overmap_buffer.find_camp( omt_pos.xy() );
    if( bcp ) {
        return bcp;
    }
    get_map().add_camp( omt_pos, "faction_camp" );
    bcp = overmap_buffer.find_camp( omt_pos.xy() );
    if( !bcp ) {
        return std::nullopt;
    }
    basecamp *temp_camp = *bcp;
    temp_camp->set_owner( p.get_fac_id() );
    temp_camp->define_camp( omt_pos, camp_type );
    return temp_camp;
}

/** @relates string_id */
template<>
const faction &string_id<faction>::obj() const
{
    return *g->faction_manager_ptr->get( *this );
}

faction *basecamp::fac() const
{
    return g->faction_manager_ptr->get( owner );
}

recipe_id base_camps::select_camp_option( const std::map<recipe_id, translation> &pos_options,
        const std::string &option )
{
    std::vector<std::string> pos_names;
    int choice = 0;

    pos_names.reserve( pos_options.size() );
    for( const auto &it : pos_options ) {
        pos_names.push_back( it.second.translated() );
    }

    std::sort( pos_names.begin(), pos_names.end(), localized_compare );

    uilist menu = uilist( );
    menu.title = option;
    menu.desc_enabled = true;
    for( const auto &it : pos_names ) {
        // This is pretty dumb, but we need to reduce the width
        // somehow.  It might be better if the JSON had two values in
        // it, one for the menu item and another for the description.
        size_t p = it.find( "  " );
        if( p == std::string::npos ) {
            menu.addentry( it );
        } else {
            menu.addentry_desc( it.substr( 0, p ), wrap60( it.substr( p + 2, it.size() ) ) );
        }
    }

    menu.query();
    choice = menu.ret;

    if( choice < 0 || static_cast<size_t>( choice ) >= pos_names.size() ) {
        popup( _( "You choose to wait…" ) );
        return recipe_id::NULL_ID();
    }

    std::string selected_name = pos_names[choice];

    std::map<recipe_id, translation>::const_iterator iter = find_if( pos_options.begin(),
    pos_options.end(), [selected_name]( const std::pair<const recipe_id, translation> &node ) {
        return node.second.translated() == selected_name;
    } );
    return iter->first;
}

void talk_function::start_camp( npc &p )
{
    const tripoint_abs_omt omt_pos = p.pos_abs_omt();
    const oter_id &omt_ref = overmap_buffer.ter( omt_pos );
    const std::optional<mapgen_arguments> *maybe_args = overmap_buffer.mapgen_args( omt_pos );
    const auto &pos_camps = recipe_group::get_recipes_by_id( "all_faction_base_types", omt_ref,
                            maybe_args );
    if( pos_camps.empty() ) {
        popup( _( "You cannot build a camp here." ) );
        return;
    }
    const recipe_id camp_type = base_camps::select_camp_option( pos_camps,
                                _( "Select a camp type:" ) );
    if( !camp_type ) {
        return;
    }

    for( const auto &om_near : om_building_region( omt_pos, 3 ) ) {
        const oter_id &om_type = oter_id( om_near.first );
        if( is_ot_match( "faction_base", om_type, ot_match_type::contains ) ||
            overmap_buffer.has_camp( om_near.second ) ) {
            popup( _( "You are too close to another camp!" ) );
            return;
        }
    }
    const recipe &making = camp_type.obj();
    const ret_val<void> has_vehicle_collision = run_mapgen_update_func( making.get_blueprint(),
            omt_pos, {} );
    if( !has_vehicle_collision.success() ) {
        popup( _( "%1$s failed to start the %2$s basecamp, %3$s appliance/vehicle seems to be in the way." ),
               p.disp_name(), making.get_blueprint().str(), has_vehicle_collision.str() );
        return;
    }
    std::optional<basecamp *> camp = get_basecamp( p, camp_type.str() );
    if( camp.has_value() ) {
        for( int tab_num = base_camps::TAB_MAIN; tab_num <= base_camps::TAB_NW; tab_num++ ) {
            std::vector<ui_mission_id> temp;
            camp.value()->hidden_missions.push_back( temp );
        }
    }
}

void talk_function::basecamp_mission( npc &p )
{
    const std::string title = _( "Base Missions" );
    const tripoint_abs_omt omt_pos = p.pos_abs_omt();
    mission_data mission_key;

    std::optional<basecamp *> temp_camp = get_basecamp( p );
    if( !temp_camp ) {
        return;
    }
    basecamp *bcp = *temp_camp;
    if( !bcp->allowed_access_by( p ) ) {
        popup( _( "%s isn't under your control!" ), bcp->camp_name() );
        return;
    }
    bcp->set_by_radio( get_avatar().dialogue_by_radio );
    map &here = bcp->get_camp_map();
    bcp->form_storage_zones( here, p.pos_abs() );
    bcp->get_available_missions( mission_key, here );
    if( display_and_choose_opts( mission_key, omt_pos, base_camps::id, title ) ) {
        bcp->handle_mission( mission_key.cur_key.id );
    }
    here.save();
    // This is to make sure the basecamp::camp_map is always usable and valid.
    // Otherwise when quick saving unloads submaps, basecamp::camp_map is still valid but becomes unusable.
    bcp->unload_camp_map();
}

void basecamp::add_available_recipes( mission_data &mission_key, mission_kind kind,
                                      const point_rel_omt &dir )
{
    const mission_id miss_id = {kind, "DUMMY_RECIPE_REPLACED_IN_CAMP", {}, dir};
    std::string miss_desc = string_format( _( "Notes:\n"
                                           "This will open the regular crafting screen where you may select a recipe and batch size.\n\n"
                                           "You will be prompted to select an NPC to perform the craft after selecting the recipe.\n\n"
                                           "Kcal cost will be shown last, you will be prompted to accept or back out." ) );
    mission_key.add_start( miss_id, _( "Crafting" ), miss_desc, true );
}

void basecamp::get_available_missions_by_dir( mission_data &mission_key, const point_rel_omt &dir )
{
    std::string entry;

    const std::string dir_id = base_camps::all_directions.at( dir ).id;
    const std::string dir_abbr = base_camps::all_directions.at( dir ).bracket_abbr.translated();

    {
        // return legacy workers. How old is this legacy?...
        mission_id miss_id = { Camp_Upgrade, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id ); // Don't match any blueprints

        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, base_camps::recover_ally_string.translated()
                                    + dir_abbr + base_camps::expansion_string.translated(),
                                    entry, avail );
        }
        // Generate upgrade missions for expansions
        std::vector<basecamp_upgrade> upgrades = available_upgrades( dir );

        std::sort( upgrades.begin(), upgrades.end(), []( const basecamp_upgrade & p,
                   const basecamp_upgrade & q )->bool {return p.name.translated_lt( q.name ); } );

        for( const basecamp_upgrade &upgrade : upgrades ) {
            miss_id.parameters = upgrade.bldg;
            miss_id.mapgen_args = upgrade.args;

            comp_list npc_list = get_mission_workers( miss_id );

            if( npc_list.empty() ) {
                std::string display_name = name_display_of( miss_id );
                const recipe &making = *recipe_id( miss_id.parameters );
                const int foodcost = time_to_food( base_camps::to_workdays( time_duration::from_moves(
                                                       making.blueprint_build_reqs().reqs_by_parameters.find( miss_id.mapgen_args )->second.time ) ),
                                                   making.exertion_level() );
                const int available_calories = fac()->food_supply().kcal();
                bool can_upgrade = upgrade.avail;
                entry = om_upgrade_description( upgrade.bldg, upgrade.args );
                if( foodcost > available_calories ) {
                    can_upgrade = false;
                    entry += string_format( _( "Total calorie cost: %s (have %d)" ),
                                            colorize( std::to_string( foodcost ), c_red ),
                                            available_calories );
                } else {
                    entry += string_format( _( "Total calorie cost: %s (have %d)" ),
                                            colorize( std::to_string( foodcost ), c_green ),
                                            available_calories );
                }
                mission_key.add_start( miss_id, display_name, entry, can_upgrade );
            } else {
                entry = action_of( miss_id.id );
                bool avail = update_time_left( entry, npc_list );
                mission_key.add_return( miss_id,
                                        base_camps::recover_ally_string.translated() + dir_abbr +
                                        " " + upgrade.name, entry, avail );
            }
        }
    }

    if( has_provides( "gathering", dir ) ) {
        const mission_id miss_id = { Camp_Gather_Materials, "", {}, dir };
        std::string gather_bldg = "null";
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_gathering->describe( npc_list.size(), std::function( [this]() {
            return gathering_description();
        } ) );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_gathering->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 3_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Ally from Gathering" ),
                                    entry, avail );
        }
    }
    if( has_provides( "firewood", dir ) ) {
        const mission_id miss_id = { Camp_Collect_Firewood, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_collect_firewood->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_collect_firewood->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 3_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Firewood Gatherers" ),
                                    entry, avail );
        }
    }
    if( has_provides( "sorting", dir ) ) {
        const mission_id miss_id = { Camp_Menial, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_menial->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_menial->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Menial Laborer" ),
                                    entry, avail );
        }
    }

    if( has_provides( "logging", dir ) ) {
        const mission_id miss_id = { Camp_Cut_Logs, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_cut_logs->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_cut_logs->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Log Cutter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "logging", dir ) ) {
        const mission_id miss_id = { Camp_Clearcut, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_clearcut->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_clearcut->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Clearcutter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "relaying", dir ) ) {
        const mission_id miss_id = { Camp_Setup_Hide_Site, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_setup_hide_site->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_setup_hide_site->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Hide Setup" ),
                                    entry, avail );
        }
    }

    if( has_provides( "relaying", dir ) ) {
        const mission_id miss_id = { Camp_Relay_Hide_Site, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_relay_hide_site->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ), entry,
                               npc_list.size() < faction_mission_camp_relay_hide_site->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Hide Relay" ),
                                    entry, avail );
        }
    }

    if( has_provides( "foraging", dir ) ) {
        const mission_id miss_id = { Camp_Foraging, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_foraging->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_foraging->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 4_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Foragers" ),
                                    entry, avail );
        }
    }

    if( has_provides( "trapping", dir ) ) {
        const mission_id miss_id = { Camp_Trapping, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_trapping->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_trapping->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 6_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Trappers" ),
                                    entry, avail );
        }
    }

    if( has_provides( "hunting", dir ) ) {
        const mission_id miss_id = { Camp_Hunting, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_hunting->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_hunting->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 6_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Hunter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "walls", dir ) ) {
        mission_id miss_id = {
            Camp_OM_Fortifications, camp_om_fortifications_trench_parameter, {}, dir
        };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = om_upgrade_description( faction_wall_level_n_0_string, {} );
        //  Should add check for materials as well as active mission.
        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Finish Map Fortifications" ),
                                    entry, avail );
        }

        entry = om_upgrade_description( faction_wall_level_n_1_string, {} );
        miss_id.parameters = camp_om_fortifications_spiked_trench_parameter;
        npc_list = get_mission_workers( miss_id );
        //  Should add check for materials as well as active mission.
        //  Should also check if there are any trenches to improve.
        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry,
                               npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Finish Map Fortification Update" ),
                                    entry, avail );
        }

        //  Code to deal with legacy construction (Changed during 0.F)
        miss_id.parameters.clear();
        npc_list = get_mission_workers( miss_id );

        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Finish Map Fortifications" ),
                                    entry, avail );
        }

    }

    if( has_provides( "recruiting", dir ) ) {
        const mission_id miss_id = { Camp_Recruiting, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = recruit_description( npc_list.size() );
        mission_key.add_start( miss_id, name_display_of( miss_id ), entry,
                               npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Recruiter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "scouting", dir ) ) {
        const mission_id miss_id = { Camp_Scouting, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_scouting->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_scouting->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Scout" ),
                                    entry, avail );
        }
    }

    if( has_provides( "patrolling", dir ) ) {
        const mission_id miss_id = { Camp_Combat_Patrol, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = faction_mission_camp_combat_patrol->describe( npc_list.size() );

        mission_key.add_start( miss_id, name_display_of( miss_id ),
                               entry, npc_list.size() < faction_mission_camp_combat_patrol->positions );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Combat Patrol" ),
                                    entry, avail );
        }
    }

    std::unordered_set<recipe_id> craft_recipes = recipe_deck_all();
    {
        mission_id miss_id = { Camp_Crafting, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id, true );
        // WTF?
        if( npc_list.size() < 3 ) {
            add_available_recipes( mission_key, Camp_Crafting, dir );
        }

        if( !npc_list.empty() ) {
            std::map<std::string, comp_list> lists = companion_per_recipe_building_type( npc_list );

            for( std::pair<std::string, comp_list> npcs : lists ) {
                const std::string bldg = npcs.first;
                miss_id.parameters = npcs.second.at( 0 )->get_companion_mission().miss_id.parameters;
                bool avail = false;
                entry.clear();

                //  Room for moving the match of recipe group 'building_type' to return string into JSON
                std::string return_craft;

                if( bldg == base_recipe_group_string ) {
                    return_craft = _( " (Finish) Crafting" );

                } else if( bldg == cook_recipe_group_string ) {
                    return_craft = _( " (Finish) Cooking" );

                } else if( bldg == farm_recipe_group_string ) {
                    return_craft = _( " (Finish) Crafting" );

                } else if( bldg == smith_recipe_group_string ) {
                    return_craft = _( " (Finish) Smithing" );
                }

                else {  //  No matching recipe group
                    return_craft = _( " (Finish) Crafting" );
                }

                for( npc_ptr &comp : npcs.second ) {
                    const bool done = comp->companion_mission_time_ret < calendar::turn;
                    avail |= done;
                    entry += comp->get_name() + " ";
                    if( done ) {
                        entry += _( "[DONE]\n" );
                    } else {
                        entry += " [" +
                                 to_string( comp->companion_mission_time_ret - calendar::turn ) +
                                 _( " left] " ) + action_of( miss_id.id );
                    }
                }

                mission_key.add_return( miss_id,
                                        dir_abbr + return_craft, entry, avail );
            }
        }

        if( !mission_key.entries[size_t( base_camps::all_directions.at( dir ).tab_order ) + 1].empty() ||
            ( !hidden_missions.empty() &&
              !hidden_missions[size_t( base_camps::all_directions.at( dir ).tab_order )].empty() ) ) {
            {
                const mission_id miss_id = { Camp_Hide_Mission, "", {}, dir };
                entry = string_format( _( "Hide one or more missions to clean up the UI." ) );
                mission_key.add( { miss_id, false }, name_display_of( miss_id ),
                                 entry );
            }
            {
                int count = 0;
                if( !hidden_missions.empty() ) {
                    count = hidden_missions[base_camps::all_directions.at( dir ).tab_order].size();
                }

                const mission_id miss_id = { Camp_Reveal_Mission, "", {}, dir };
                entry = string_format( _( "Reveal one or more missions previously hidden.\n"
                                          "Current number of hidden missions: %d" ),
                                       count );
                mission_key.add( { miss_id, false }, name_display_of( miss_id ),
                                 entry, false, count != 0 );
            }
        }
    }

    if( has_provides( "farming", dir ) ) {
        size_t plots = 0;
        const mission_id miss_id = { Camp_Plow, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( npc_list.size() < faction_mission_camp_plow->positions ) {
            entry = faction_mission_camp_plow->describe( npc_list.size(), nullptr, std::function( [this, &dir,
                  &plots]() {
                return farm_description( dir, plots, farm_ops::plow );
            } ) );

            mission_key.add_start( miss_id, name_display_of( miss_id ),
                                   entry, plots > 0 );
        }
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Plow fields" ), entry, avail );
        }
    }
    if( has_provides( "farming", dir ) ) {
        size_t plots = 0;
        const mission_id miss_id = { Camp_Plant, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( npc_list.size() < faction_mission_camp_plant->positions ) {
            entry = faction_mission_camp_plant->describe( npc_list.size(), nullptr, std::function( [this, &dir,
                  &plots]() {
                return farm_description( dir, plots, farm_ops::plant );
            } ) );
            const tripoint_abs_omt target_omt = omt_pos + dir;
            const tripoint_bub_ms target_pnt = get_map().get_bub( project_to<coords::ms>( target_omt ) );
            // FIXME/HACK: Always checks buckwheat seeds!
            mission_key.add_start( miss_id,
                                   name_display_of( miss_id ), entry,
                                   plots > 0 && warm_enough_to_plant( target_pnt, itype_seed_buckwheat ).success() );
        }
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Plant Fields" ), entry, avail );
        }
    }
    if( has_provides( "farming", dir ) ) {
        size_t plots = 0;
        const mission_id miss_id = { Camp_Harvest, "", {}, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( npc_list.size() < faction_mission_camp_harvest->positions ) {
            entry = faction_mission_camp_harvest->describe( npc_list.size(), nullptr, std::function( [this,
                    &dir,
            &plots]() {
                return farm_description( dir, plots, farm_ops::harvest );
            } ) );
            mission_key.add_start( miss_id,
                                   name_display_of( miss_id ), entry,
                                   plots > 0 );
        }
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Harvest Fields" ), entry, avail );
        }
    }
}

void basecamp::get_available_missions( mission_data &mission_key, map &here )
{
    std::string entry;

    const point_rel_omt &base_dir = base_camps::base_dir;
    const base_camps::direction_data &base_data = base_camps::all_directions.at( base_dir );
    const std::string base_dir_id = base_data.id;
    reset_camp_resources( here );

    // Missions that belong exclusively to the central tile
    {
        if( directions.size() < 8 ) {
            bool free_non_field_found = false;
            bool possible_expansion_found = false;

            for( const auto &dir : base_camps::all_directions ) {
                if( dir.first != base_camps::base_dir && expansions.find( dir.first ) == expansions.end() ) {
                    const oter_id &omt_ref = overmap_buffer.ter( omt_pos + dir.first );
                    if( !free_non_field_found && omt_ref->get_type_id() != oter_type_field ) {
                        free_non_field_found = true;
                    }
                    if( !possible_expansion_found ) {
                        const std::optional<mapgen_arguments> *maybe_args =
                            overmap_buffer.mapgen_args( omt_pos + dir.first );
                        possible_expansion_found = recipe_group::has_recipes_by_id( "all_faction_base_expansions",
                                                   omt_ref, maybe_args );
                    }
                    if( free_non_field_found && possible_expansion_found ) {
                        break;
                    }
                }
            }

            if( free_non_field_found ) {
                const mission_id miss_id = { Camp_Survey_Field, "", {}, base_dir };
                comp_list npc_list = get_mission_workers( miss_id );
                entry = faction_mission_camp_survey_field->describe( npc_list.size() );
                mission_key.add_start( miss_id, name_display_of( miss_id ),
                                       entry, npc_list.size() < faction_mission_camp_survey_field->positions );
                if( !npc_list.empty() ) {
                    entry = action_of( miss_id.id );
                    bool avail = update_time_left( entry, npc_list );
                    mission_key.add_return( miss_id, _( "Recover Field Surveyor" ),
                                            entry, avail );
                }
            }

            const mission_id miss_id = { Camp_Survey_Expansion, "", {}, base_dir };
            comp_list npc_list = get_mission_workers( miss_id );
            entry = faction_mission_camp_survey_expansion->describe( npc_list.size() );
            mission_key.add_start( miss_id, name_display_of( miss_id ),
                                   entry, npc_list.size() < faction_mission_camp_survey_expansion->positions &&
                                   possible_expansion_found );
            if( !npc_list.empty() ) {
                entry = action_of( miss_id.id );
                bool avail = update_time_left( entry, npc_list );
                mission_key.add_return( miss_id, _( "Recover Surveyor" ),
                                        entry, avail );
            }
        }
    }

    if( !by_radio ) {
        {
            const mission_id miss_id = { Camp_Distribute_Food, "", {}, base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Distribute food to your follower and fill your larders.  "
                                      "Place the food you wish to distribute in the camp food zone.  "
                                      "You must have a camp food zone, and a camp storage zone, "
                                      "or you will be prompted to create them using the zone manager.\n"
                                      "Effects:\n"
                                      "> Increases your faction's food supply value which in "
                                      "turn is used to pay laborers for their time\n\n"
                                      "Must have enjoyability >= -6\n"
                                      "Total faction food stock: %d kcal\nor %d / %d / %d days' rations\n"
                                      "where the days is measured for Extra / Moderate / No exercise levels" ),
                                   fac()->food_supply().kcal(), camp_food_supply_days( EXTRA_EXERCISE ),
                                   camp_food_supply_days( MODERATE_EXERCISE ), camp_food_supply_days( NO_EXERCISE ) );
            mission_key.add( { miss_id, false }, name_display_of( miss_id ),
                             entry );
        }
        {
            const mission_id miss_id = { Camp_Determine_Leadership, "", {}, base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Choose a new leader for your faction.\n"
                                      "<color_yellow>You will switch to playing as the new leader.</color>\n"
                                      "Difficulty: N/A\n"
                                      "Risk: None\n" ) );
            mission_key.add( { miss_id, false }, name_display_of( miss_id ),
                             entry );
        }
        {
            const mission_id miss_id = { Camp_Have_Meal, "", {}, base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Eat some food from the larder.\n"
                                      "Nutritional value depends on food stored in the larder.\n"
                                      "Difficulty: N/A\n"
                                      "Risk: None\n" ) );
            mission_key.add( { miss_id, false }, name_display_of( miss_id ),
                             entry );
        }
        {
            const mission_id miss_id = { Camp_Requests, "", {}, base_dir };
            int active_requests = 0;
            int archived_requests = 0;
            for( const camp_llm_request &request : camp_requests ) {
                if( request.status == "completed" || request.status == "cancelled" ) {
                    archived_requests++;
                } else {
                    active_requests++;
                }
            }
            entry = string_format( _( "Notes:\n"
                                      "Review the bulletin-board scratchpad for camp crafting requests.\n"
                                      "Effects:\n"
                                      "> Inspect request details, recent notes, and assignment state\n"
                                      "> Recall an active crafter directly from the board\n"
                                      "> Clear cancelled requests from the board if needed\n\n"
                                      "Active requests: %d\n"
                                      "Archived requests: %d\n"
                                      "Risk: None\n" ),
                                   active_requests, archived_requests );
            mission_key.add( { miss_id, false }, name_display_of( miss_id ),
                             entry );
        }
        {
            validate_assignees();
            const mission_id miss_id = { Camp_Assign_Jobs, "", {}, base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Assign repeating job duties to NPCs stationed here.\n"
                                      "Difficulty: N/A\n"
                                      "Effects:\n"
                                      "\n\nRisk: None\n"
                                      "Time: Ongoing" ) );
            mission_key.add( {miss_id, false}, name_display_of( miss_id ), entry );
        }
        {
            const mission_id miss_id = { Camp_Assign_Workers, "", {}, base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Assign followers to work at this camp." ) );
            mission_key.add( {miss_id, false}, name_display_of( miss_id ), entry );
        }
        {
            const mission_id miss_id = { Camp_Abandon, "", {}, base_dir };
            entry = _( "Notes:\nAbandon this camp" );
            mission_key.add( {miss_id, false}, name_display_of( miss_id ), entry );
        }
    }
    // Missions assigned to the central tile that could be done by an expansion
    get_available_missions_by_dir( mission_key, base_camps::base_dir );

    // Loop over expansions
    for( const point_rel_omt &dir : directions ) {
        get_available_missions_by_dir( mission_key, dir );
    }

    if( !camp_workers.empty() ) {
        const mission_id miss_id = { Camp_Emergency_Recall, "", {}, base_dir };
        entry = string_format( _( "Notes:\n"
                                  "Cancel a current mission and force the immediate return of a "
                                  "companion.  No work will be done on the mission and all "
                                  "resources used on the mission will be lost.\n\n"
                                  "WARNING: All resources used on the mission will be lost and "
                                  "no work will be done.  Only use this mission to recover a "
                                  "companion who cannot otherwise be recovered.\n\n"
                                  "Companions must be on missions for at least 24 hours before "
                                  "emergency recall becomes available." ) );
        bool avail = update_emergency_recall( entry, camp_workers, 24_hours );
        mission_key.add_return( miss_id, _( "Emergency Recall" ),
                                entry, avail );
    }

    std::vector<ui_mission_id> k;
    for( int tab_num = base_camps::TAB_MAIN; tab_num <= base_camps::TAB_NW; tab_num++ ) {
        if( temp_ui_mission_keys.size() < size_t( tab_num ) + 1 ) {
            temp_ui_mission_keys.push_back( k );
        } else {
            temp_ui_mission_keys[tab_num].clear();
        }

        //  mission_key offsets its entries by 1, reserving 0 for high prio entries.
        for( mission_entry &entry : mission_key.entries[size_t( tab_num ) + 1] ) {
            if( !entry.id.ret && entry.id.id.id != Camp_Reveal_Mission ) {
                temp_ui_mission_keys[tab_num].push_back( entry.id );
            }
        }
    }
}

void basecamp::choose_new_leader()
{
    time_point last_succession_time = time_point::from_turn(
                                          get_avatar().get_value( var_timer_time_of_last_succession ).dbl() );
    time_duration succession_cooldown = time_duration::from_turns(
                                            get_globals().get_global_value( var_time_between_succession ).dbl() );
    time_point next_succession_chance = last_succession_time + succession_cooldown;
    int current_time_int = to_seconds<int>( calendar::turn - calendar::turn_zero );
    if( next_succession_chance >= calendar::turn ) {
        popup( _( "It's too early for that.  A new leader can be chosen in %s." ),
               to_string( next_succession_chance - calendar::turn ) );
        return;
    }
    std::vector<std::string> choices;
    choices.reserve( 3 );
    int choice = 0;
    choices.emplace_back _( "autocratic" );
    choices.emplace_back _( "sortition" );
    choices.emplace_back _( "democratic" );

    choice = uilist( _( "Choose how the new leader will be determined." ), choices );

    if( choice < 0 || static_cast<size_t>( choice ) >= choices.size() ) {
        popup( _( "You choose to wait…" ) );
        return;
    }

    // Autocratic
    if( choice == 0 ) {
        if( !query_yn(
                _( "As an experienced leader, only you know what will be required of future leaders.  You will choose.\n\nIs this acceptable?" ) ) ) {
            return;
        }
        get_avatar().control_npc_menu( false );
        // Possible to exit menu and not choose a *new* leader. However this doesn't reset global timer. 100% on purpose, since you are "choosing" yourself.
        get_player_character().set_value( var_timer_time_of_last_succession, current_time_int );
    }

    // Vector of pairs containing a pointer to an NPC and their modified social score
    std::vector<std::pair<shared_ptr_fast<npc>, int>> followers;
    // You is still a nullptr! We never want to actually call the first value, this will crash.
    shared_ptr_fast<npc> you;
    followers.emplace_back( you, rng( 0, 5 ) +
                            rng( 0, get_avatar().get_skill_level( skill_speech ) * 2 ) );
    int charnum = 0;
    for( const character_id &elem : g->get_follower_list() ) {
        shared_ptr_fast<npc> follower = overmap_buffer.find_npc( elem );
        if( follower ) {
            // Yes this is a very barren representation of who gets elected in a democracy. Too bad!
            int popularity = rng( 0, 5 ) + rng( 0, follower->get_skill_level( skill_speech ) * 2 );
            followers.emplace_back( follower, popularity );
            charnum++;
        }
    }
    // Sortition
    if( choice == 1 ) {
        if( !query_yn(
                _( "You will allow fate to choose the next leader.  Whether it's by dice, drawing straws, or picking names out of a hat, it will be purely random.\n\nIs this acceptable?" ) ) ) {
            return;
        }
        int selected = rng( 0, charnum );
        // Vector starts at 0, we inserted 'you' first, 0 will always be 'you' pre-sort (that's why we don't sort unless democracy is called)
        if( selected == 0 ) {
            popup( _( "Fate calls for you to remain in your role as leader… for now." ) );
            get_player_character().set_value( var_timer_time_of_last_succession, current_time_int );
            return;
        }
        npc_ptr chosen = followers.at( selected ).first;
        popup( _( "Fate chooses %s to lead your faction." ), chosen->get_name() );
        get_avatar().control_npc( *chosen, false );
        return;
    }

    // Democratic
    if( choice == 2 ) {
        if( !query_yn(
                _( "A leader can only lead those willing to follow.  Everyone must get a say in choosing the new leader.\n\nIs this acceptable?" ) ) ) {
            return;
        }
        std::sort( followers.begin(), followers.end(), []( const auto & x, const auto & y ) {
            return x.second > y.second;
        } );
        npc_ptr elected = followers.at( 0 ).first;
        // you == nullptr
        if( elected == nullptr ) {
            popup( _( "You win the election!" ) );
            get_player_character().set_value( var_timer_time_of_last_succession, current_time_int );
            return;
        }
        popup( _( "%1$s wins the election with a popularity of %2$s!  The runner-up had a popularity of %3$s." ),
               elected->get_name(), followers.at( 0 ).second, followers.at( 1 ).second );
        get_avatar().control_npc( *elected, false );
    }
}

void basecamp::player_eats_meal()
{
    uilist smenu;
    smenu.text = _( "Have a meal?" );
    int i = 1;
    smenu.addentry( i++, true, '1', _( "Snack" ) );
    smenu.addentry( i++, true, '2', _( "Meal" ) );
    smenu.addentry( i++, true, '3', _( "Just stuff your face.  You're hungry!" ) );
    smenu.query();
    if( smenu.ret_act != "CONFIRM" ) {
        popup( _( "You decide not to have anything after all." ) );
        return;
    }
    int kcal_to_eat = smenu.ret * 750 - 250; // 500, 1250, 2000 kcal
    Character &you = get_player_character();
    const int &food_available = fac()->food_supply().kcal();
    if( food_available <= 0 ) {
        popup( _( "You check storage for some food, but there is nothing but dust and cobwebs…" ) );
        return;
    } else if( food_available <= kcal_to_eat ) {
        add_msg( _( "There's only one meal left.  Guess that's dinner!" ) );
        kcal_to_eat = food_available;
    }
    nutrients dinner = camp_food_supply( -kcal_to_eat );
    feed_workers( you, dinner, true );
}

static std::string camp_request_status_label( const camp_llm_request &request )
{
    if( request.status == "in_progress" ) {
        return _( "in progress" );
    }
    if( request.status == "completed" ) {
        return _( "completed" );
    }
    if( request.status == "cancelled" ) {
        return _( "cancelled" );
    }
    if( request.status == "blocked" ) {
        return _( "blocked" );
    }
    if( request.status == "awaiting_approval" ) {
        return _( "awaiting approval" );
    }
    return request.status;
}

static std::string camp_request_approval_label( const camp_llm_request &request )
{
    if( request.approval_state == "approved" ) {
        return _( "approved" );
    }
    if( request.approval_state == "waiting_player" ) {
        return _( "waiting for player" );
    }
    if( request.approval_state == "suggested_auto" ) {
        return _( "suggested auto-start" );
    }
    if( request.approval_state == "rejected" ) {
        return _( "rejected" );
    }
    if( request.approval_state == "not_needed" ) {
        return _( "not needed" );
    }
    return request.approval_state;
}

static std::string camp_request_kind_label( const camp_llm_request &request )
{
    if( request.request_kind == "craft_request" ) {
        return _( "crafting request" );
    }
    return request.request_kind;
}

static std::string camp_request_subject( const camp_llm_request &request )
{
    return basecamp_ai::camp_request_subject_for_display( request );
}

static std::string camp_request_bark_subject( const camp_llm_request &request )
{
    return basecamp_ai::camp_request_subject_for_display( request, false, true );
}

static std::string camp_request_summary_subject( const camp_llm_request &request )
{
    return basecamp_ai::camp_request_subject_for_display( request, true );
}

static std::string camp_request_subject_list( const std::vector<std::string> &subjects );

static std::string camp_request_summary( const camp_llm_request &request )
{
    std::string summary = string_format( "#%d %s — %s", request.request_id,
                                         camp_request_summary_subject( request ),
                                         camp_request_status_label( request ) );
    if( !request.assigned_worker_name.empty() ) {
        summary += string_format( _( " (%s)" ), request.assigned_worker_name );
    }
    return summary;
}

static std::string camp_request_spoken_status( const camp_llm_request &request )
{
    std::string spoken = camp_request_summary( request );
    if( request.status == "blocked" && !request.blockers.empty() ) {
        spoken += string_format( _( " — blocker: %s" ), request.blockers.front() );
    } else if( request.status == "in_progress" && request.eta_turn > calendar::turn ) {
        spoken += string_format( _( " — ETA %s" ), to_string( request.eta_turn - calendar::turn ) );
    } else if( request.status == "awaiting_approval" && request.approval_state == "suggested_auto" ) {
        spoken += _( " — ready to launch; looks routine" );
    } else if( request.status == "awaiting_approval" ) {
        spoken += _( " — waiting on your approval" );
    }
    return spoken;
}

static void add_camp_request_bark( const npc &speaker, const std::string &text )
{
    if( text.empty() ) {
        return;
    }
    add_msg( _( "%1$s says, \"%2$s\"" ), speaker.disp_name(), text );
}

[[maybe_unused]] static std::string camp_request_queue_bark( const camp_llm_request &request )
{
    return string_format( _( "Got it.  Pinned %s." ), camp_request_bark_subject( request ) );
}

[[maybe_unused]] static std::string camp_request_blocked_bark( const camp_llm_request &request )
{
    const bool has_resolved_recipe = !request.chosen_recipe_name.empty() &&
                                     request.chosen_recipe_name != request.requested_item_query;
    if( !request.blockers.empty() ) {
        if( has_resolved_recipe ) {
            return string_format( _( "Heard you.  %1$s matched %2$s, but it is blocked: %3$s" ),
                                  camp_request_bark_subject( request ), request.chosen_recipe_name,
                                  request.blockers.front() );
        }
        return string_format( _( "Heard you, but %1$s is blocked: %2$s" ),
                              camp_request_bark_subject( request ), request.blockers.front() );
    }
    if( has_resolved_recipe ) {
        return string_format( _( "Heard you.  %1$s matched %2$s, but it cannot start yet." ),
                              camp_request_bark_subject( request ), request.chosen_recipe_name );
    }
    return string_format( _( "Heard you, but %s cannot start yet." ),
                          camp_request_bark_subject( request ) );
}

[[maybe_unused]] static std::string camp_request_launch_bark( const camp_llm_request &request )
{
    return string_format( _( "Got it.  %1$s is crafting %2$s now." ),
                          request.assigned_worker_name, camp_request_subject( request ) );
}

static std::string camp_request_cancel_bark( const camp_llm_request &request )
{
    return string_format( _( "All right.  Took %s off the board." ), camp_request_bark_subject( request ) );
}

static std::string camp_request_clear_bark( const camp_llm_request &request )
{
    return string_format( _( "Cleared archived %s." ), camp_request_bark_subject( request ) );
}

static std::string camp_request_not_archived_bark( const camp_llm_request &request )
{
    return string_format( _( "%1$s is still %2$s.  Cancel it or let it finish first." ),
                          camp_request_bark_subject( request ), camp_request_status_label( request ) );
}

static std::string camp_request_archived_bark( const camp_llm_request &request )
{
    return string_format( _( "%1$s is already archived as %2$s.  Clear it if you want it gone." ),
                          camp_request_bark_subject( request ), camp_request_status_label( request ) );
}

static std::string camp_request_ambiguous_bark( std::string_view query,
        const std::vector<std::string> &matches )
{
    return string_format( _( "Need a clearer target for \"%1$s\": %2$s." ),
                          std::string( query ), camp_request_subject_list( matches ) );
}

static std::string camp_craft_ambiguous_bark( std::string_view query,
        const std::vector<std::string> &matches )
{
    return string_format( _( "Need a clearer craft order for \"%1$s\": %2$s." ),
                          std::string( query ), camp_request_subject_list( matches ) );
}

static std::string camp_craft_no_match_bark( std::string_view query )
{
    return string_format( _( "Heard craft order for \"%s\", but the camp could not match it to a known recipe." ),
                          std::string( query ) );
}

static std::string camp_craft_resolution_outcome_name(
    const basecamp_ai::camp_craft_resolution_outcome outcome )
{
    switch( outcome ) {
        case basecamp_ai::camp_craft_resolution_outcome::MATCH_START:
            return "MATCH_START";
        case basecamp_ai::camp_craft_resolution_outcome::MATCH_BLOCKED:
            return "MATCH_BLOCKED";
        case basecamp_ai::camp_craft_resolution_outcome::MATCH_AMBIGUOUS:
            return "MATCH_AMBIGUOUS";
        case basecamp_ai::camp_craft_resolution_outcome::NO_MATCH:
            break;
    }
    return "NO_MATCH";
}

static void log_camp_craft_trace( const std::string &utterance,
                                  const basecamp_ai::parsed_camp_craft_order &parsed,
                                  const size_t known_recipe_count,
                                  const size_t search_recipe_count,
                                  const basecamp_ai::camp_craft_resolution &resolution,
                                  const basecamp_ai::camp_craft_resolution_outcome outcome,
                                  const std::string &resolved_recipe = std::string(),
                                  const std::vector<std::string> &blockers = {},
                                  const int request_id = -1,
                                  const std::string &request_status = std::string() )
{
    const auto join_trace = []( const std::vector<std::string> &parts ) {
        std::string joined;
        for( size_t i = 0; i < parts.size(); ++i ) {
            if( i > 0 ) {
                joined += " | ";
            }
            joined += parts[i];
        }
        return joined.empty() ? std::string( "none" ) : joined;
    };

    const std::string top_matches = join_trace( resolution.match.score_notes );
    const std::string blocker_text = join_trace( blockers );
    DebugLog( D_INFO, DC_ALL ) << string_format(
        "camp_craft_trace heard=\"%1$s\" parsed_query=\"%2$s\" count=%3$d known_recipe_count=%4$d search_recipe_count=%5$d fallback_query=\"%6$s\" top_matches=%7$s resolved_recipe=\"%8$s\" outcome=%9$s blockers=%10$s request_id=%11$d request_status=\"%12$s\"",
        utterance, parsed.item_query, parsed.count,
        static_cast<int>( known_recipe_count ), static_cast<int>( search_recipe_count ),
        resolution.match.fallback_query.empty() ? "none" : resolution.match.fallback_query,
        top_matches,
        resolved_recipe.empty() && resolution.choice.has_value() ? resolution.choice->subject : resolved_recipe,
        camp_craft_resolution_outcome_name( outcome ), blocker_text, request_id,
        request_status.empty() ? "none" : request_status );
}

static std::string camp_request_not_found_bark( std::string_view query )
{
    return string_format( _( "Nothing on the board matches \"%s\"." ), std::string( query ) );
}

static std::string camp_request_details( const camp_llm_request &request )
{
    std::string details = string_format( _( "Crafting request #%1$d\n"
                                           "Kind: %2$s\n"
                                           "Craft order: %3$s\n"
                                           "Status: %4$s\n" ),
                                         request.request_id,
                                         camp_request_kind_label( request ),
                                         camp_request_subject( request ),
                                         camp_request_status_label( request ) );
    if( request.approval_state != "not_needed" ) {
        details += string_format( _( "Approval: %s\n" ), camp_request_approval_label( request ) );
    }
    if( !request.source_utterance.empty() ) {
        details += string_format( _( "Source utterance: %s\n" ), request.source_utterance );
    }
    if( !request.requested_item_query.empty() ) {
        details += string_format( _( "Heard request: %s\n" ), request.requested_item_query );
    }
    if( !request.chosen_recipe_name.empty() ) {
        details += string_format( _( "Resolved recipe: %s\n" ), request.chosen_recipe_name );
    }
    if( !request.chosen_recipe_id.is_empty() ) {
        details += string_format( _( "Recipe id: %s\n" ), request.chosen_recipe_id.str() );
    }
    if( !request.assigned_worker_name.empty() ) {
        details += string_format( _( "Assigned worker: %s\n" ), request.assigned_worker_name );
    }
    if( request.eta_turn > calendar::turn ) {
        details += string_format( _( "ETA: %s\n" ), to_string( request.eta_turn - calendar::turn ) );
    }
    if( !request.blockers.empty() ) {
        details += _( "Blockers:\n" );
        for( const std::string &blocker : request.blockers ) {
            details += string_format( "- %s\n", blocker );
        }
    }
    if( const std::string handoff = basecamp_ai::camp_request_handoff_snapshot( request ); !handoff.empty() ) {
        details += _( "Deterministic handoff:\n" );
        details += handoff;
    }
    if( !request.notes.empty() ) {
        details += _( "Recent notes:\n" );
        const size_t start = request.notes.size() > 4 ? request.notes.size() - 4 : 0;
        for( size_t idx = start; idx < request.notes.size(); ++idx ) {
            details += string_format( "- %s\n", request.notes[idx].text );
        }
    }
    return details;
}

static std::string camp_request_subject_list( const std::vector<std::string> &subjects )
{
    constexpr size_t max_listed_subjects = 3;
    std::string joined;
    for( size_t idx = 0; idx < subjects.size() && idx < max_listed_subjects; ++idx ) {
        if( idx > 0 ) {
            joined += ", ";
        }
        joined += subjects[idx];
    }
    if( subjects.size() > max_listed_subjects ) {
        joined += string_format( _( ", and %d more" ), static_cast<int>( subjects.size() - max_listed_subjects ) );
    }
    return joined;
}

static npc_ptr recall_crafting_request_worker( basecamp &camp, const camp_llm_request &request )
{
    const comp_list matching_workers = camp.get_mission_workers( request.active_mission_id );
    const auto worker_it = std::find_if( matching_workers.begin(), matching_workers.end(),
    [&]( const npc_ptr & comp ) {
        return comp != nullptr &&
               basecamp_ai::matches_assigned_camp_request_worker( request, comp->getID(), comp->disp_name() );
    } );
    if( worker_it == matching_workers.end() ) {
        popup( _( "That crafter is no longer available for recall." ) );
        return nullptr;
    }

    const npc_ptr &comp = *worker_it;
    const mission_id active_mission = comp->get_companion_mission().miss_id;
    const std::string return_msg = _( "responds to the emergency recall…" );
    camp.finish_return( *comp, false, return_msg, skill_menial.str(), 0, true );
    camp.cancel_crafting_request( active_mission, *comp,
                                  string_format( _( "%s was recalled before the craft order was finished." ),
                                          comp->disp_name() ) );
    return comp;
}

struct camp_craft_risk_snapshot {
    time_duration work_days = 0_turns;
    int kcal_cost = 0;
    int kcal_available = 0;
    int food_days = 0;
    std::string food_state = "comfortable";
    bool food_risky = false;
    bool suggested_auto = false;
};

static std::string camp_food_margin_state( int food_days )
{
    if( food_days <= 0 ) {
        return "critical";
    }
    if( food_days <= 1 ) {
        return "anxious";
    }
    if( food_days <= 3 ) {
        return "lean";
    }
    if( food_days <= 7 ) {
        return "watchful";
    }
    return "comfortable";
}

static camp_craft_risk_snapshot describe_camp_craft_risk( basecamp &camp, const recipe &making,
        const npc &worker, int batch_size )
{
    camp_craft_risk_snapshot snapshot;
    snapshot.work_days = base_camps::to_workdays( making.batch_duration( worker, batch_size ) );
    snapshot.kcal_cost = camp.time_to_food( snapshot.work_days, making.exertion_level() );
    snapshot.food_days = camp.camp_food_supply_days( NO_EXERCISE );
    snapshot.kcal_available = snapshot.food_days * camp.time_to_food( 24_hours, NO_EXERCISE );
    snapshot.food_state = camp_food_margin_state( snapshot.food_days );
    snapshot.food_risky = snapshot.food_state == "critical" || snapshot.food_state == "anxious";

    const bool short_job = snapshot.work_days <= 8_hours;
    const bool small_food_draw = snapshot.kcal_cost > 0 &&
                                 snapshot.kcal_cost * 8 <= std::max( snapshot.kcal_available, 1 );
    snapshot.suggested_auto = making.result()->phase == phase_id::SOLID &&
                              !snapshot.food_risky && short_job && small_food_draw;
    return snapshot;
}

static std::string camp_craft_food_note( const camp_craft_risk_snapshot &snapshot )
{
    if( snapshot.food_state == "critical" ) {
        return _( "Feasible on paper, but the pantry is about one bad sneeze from empty.  Better to leave this pinned until the food problem eases." );
    }
    if( snapshot.food_state == "anxious" ) {
        return _( "Can do it, but the camp is running anxious on rations.  Best not launch too many comfort jobs blind." );
    }
    if( snapshot.food_state == "lean" ) {
        return _( "Can do it, though the pantry is running lean enough to merit a second look before launch." );
    }
    return std::string();
}

static std::string camp_craft_approval_note( const camp_craft_risk_snapshot &snapshot )
{
    if( snapshot.suggested_auto ) {
        return _( "Routine enough that this one could probably be auto-started without much ceremony." );
    }
    if( snapshot.food_risky ) {
        return _( "Worth keeping on the board for approval first; camp food is already tight." );
    }
    if( snapshot.work_days > 8_hours ) {
        return _( "Big enough job that keeping it pinned for approval first is the sane option." );
    }
    return _( "Pinned for approval before anyone burns tools, ingredients, and calories on it." );
}

struct camp_request_launch_result {
    std::vector<std::string> started;
    std::vector<std::string> blocked;
    std::vector<std::string> pinned;
};

static std::string camp_request_launch_feedback( const std::vector<std::string> &started,
        const std::vector<std::string> &blocked,
        const std::vector<std::string> &pinned )
{
    std::string feedback;
    if( !started.empty() ) {
        feedback += string_format( _( "Started %1$d: %2$s\n" ), static_cast<int>( started.size() ),
                                   camp_request_subject_list( started ) );
    }
    if( !blocked.empty() ) {
        feedback += string_format( _( "Blocked %1$d: %2$s\n" ), static_cast<int>( blocked.size() ),
                                   camp_request_subject_list( blocked ) );
    }
    if( !pinned.empty() ) {
        feedback += string_format( _( "Still pinned %1$d: %2$s\n" ), static_cast<int>( pinned.size() ),
                                   camp_request_subject_list( pinned ) );
    }
    if( feedback.empty() ) {
        feedback = _( "Nothing changed on the board." );
    }
    return feedback;
}

static camp_request_launch_result attempt_camp_request_launches( const std::vector<int> &launch_ids,
        const std::function<camp_llm_request *( int )> &find_request,
        const std::function<void( int )> &approve_request )
{
    camp_request_launch_result result;
    result.started.reserve( launch_ids.size() );
    result.blocked.reserve( launch_ids.size() );
    result.pinned.reserve( launch_ids.size() );

    for( const int launch_id : launch_ids ) {
        camp_llm_request *request = find_request( launch_id );
        if( request == nullptr ) {
            continue;
        }
        const std::string subject = camp_request_subject( *request );
        approve_request( launch_id );
        request = find_request( launch_id );
        if( request == nullptr ) {
            continue;
        }
        if( request->status == "in_progress" ) {
            result.started.push_back( subject );
        } else if( request->status == "blocked" ) {
            result.blocked.push_back( subject );
        } else {
            result.pinned.push_back( subject );
        }
    }

    return result;
}

static bool camp_request_is_archived( const camp_llm_request &request )
{
    return request.status == "completed" || request.status == "cancelled";
}

camp_llm_request *basecamp::find_camp_request( int request_id )
{
    auto it = std::find_if( camp_requests.begin(), camp_requests.end(), [request_id]( const camp_llm_request & request ) {
        return request.request_id == request_id;
    } );
    return it == camp_requests.end() ? nullptr : &*it;
}

camp_llm_request *basecamp::find_active_crafting_request( const mission_id &miss_id,
        const character_id &worker_id )
{
    auto it = std::find_if( camp_requests.rbegin(), camp_requests.rend(), [&]( const camp_llm_request & request ) {
        return request.status == "in_progress" && request.assigned_worker_npc_id == worker_id &&
               is_equal( request.active_mission_id, miss_id );
    } );
    return it == camp_requests.rend() ? nullptr : &*it;
}

void basecamp::add_camp_request_note( camp_llm_request &request, const std::string &kind,
                                      const std::string &text )
{
    request.updated_turn = calendar::turn;
    if( text.empty() ) {
        return;
    }
    request.notes.push_back( camp_llm_note{ kind, text, calendar::turn } );
}

bool basecamp::can_assign_crafting_worker( const npc &worker, const recipe &making,
        bool require_available, std::string *reason ) const
{
    const auto fail = [&]( const std::string & msg ) {
        if( reason != nullptr ) {
            *reason = msg;
        }
        return false;
    };

    if( require_available && worker.has_companion_mission() ) {
        return fail( _( "is already working on something else" ) );
    }
    if( !making.skill_used.is_null() &&
        worker.get_skill_level( making.skill_used ) < making.get_difficulty( worker ) ) {
        return fail( _( "lacks necessary skills" ) );
    }
    if( !making.character_has_required_proficiencies( worker ) ) {
        return fail( _( "lacks necessary proficiencies" ) );
    }

    if( reason != nullptr ) {
        reason->clear();
    }
    return true;
}

npc_ptr basecamp::resolve_crafting_worker( const recipe &making, int batch_size,
        const character_id &preferred_worker_id, const std::string &preferred_worker_name,
        std::string *resolution_note, std::vector<std::string> *blockers )
{
    if( resolution_note != nullptr ) {
        resolution_note->clear();
    }
    if( blockers != nullptr ) {
        blockers->clear();
    }

    validate_assignees();

    struct worker_candidate {
        npc_ptr worker;
        time_duration duration;
        bool preferred = false;
    };

    std::vector<worker_candidate> candidates;
    std::vector<std::string> rejection_reasons;
    bool preferred_seen = false;
    bool preferred_eligible = false;
    std::string preferred_name = preferred_worker_name;

    const auto append_reason = [&]( const std::string & reason ) {
        if( reason.empty() ) {
            return;
        }
        if( std::find( rejection_reasons.begin(), rejection_reasons.end(), reason ) == rejection_reasons.end() ) {
            rejection_reasons.push_back( reason );
        }
    };

    for( const npc_ptr &guy : assigned_npcs ) {
        if( guy == nullptr ) {
            continue;
        }

        const bool preferred = preferred_worker_id.is_valid() && guy->getID() == preferred_worker_id;
        if( preferred ) {
            preferred_seen = true;
            preferred_name = guy->disp_name();
        }

        std::string reason;
        if( !can_assign_crafting_worker( *guy, making, true, &reason ) ) {
            append_reason( string_format( _( "%1$s %2$s." ), guy->disp_name(), reason ) );
            continue;
        }

        candidates.push_back( { guy, base_camps::to_workdays( making.batch_duration( *guy, batch_size ) ), preferred } );
        preferred_eligible = preferred_eligible || preferred;
    }

    if( candidates.empty() ) {
        if( preferred_worker_id.is_valid() && !preferred_seen ) {
            if( preferred_name.empty() ) {
                append_reason( _( "Assigned worker is no longer stationed at this camp." ) );
            } else {
                append_reason( string_format( _( "%s is no longer stationed at this camp." ), preferred_name ) );
            }
        }
        if( rejection_reasons.empty() ) {
            rejection_reasons.push_back( _( "No stationed worker can take this recipe right now." ) );
        }
        if( blockers != nullptr ) {
            *blockers = rejection_reasons;
        }
        return nullptr;
    }

    if( preferred_eligible ) {
        auto preferred_it = std::find_if( candidates.begin(), candidates.end(), []( const worker_candidate & candidate ) {
            return candidate.preferred;
        } );
        if( preferred_it != candidates.end() ) {
            return preferred_it->worker;
        }
    }

    auto best_it = std::min_element( candidates.begin(), candidates.end(), []( const worker_candidate & lhs,
    const worker_candidate & rhs ) {
        if( lhs.duration != rhs.duration ) {
            return lhs.duration < rhs.duration;
        }
        return lhs.worker->disp_name() < rhs.worker->disp_name();
    } );

    if( best_it == candidates.end() ) {
        return nullptr;
    }

    if( preferred_worker_id.is_valid() && resolution_note != nullptr ) {
        if( !preferred_seen ) {
            if( preferred_name.empty() ) {
                *resolution_note = string_format( _( "%s took over the work order." ), best_it->worker->disp_name() );
            } else {
                *resolution_note = string_format( _( "%1$s was gone, so %2$s took over the work order." ),
                                                  preferred_name, best_it->worker->disp_name() );
            }
        } else if( !preferred_name.empty() ) {
            *resolution_note = string_format( _( "%1$s could not take the order, so %2$s picked it up instead." ),
                                              preferred_name, best_it->worker->disp_name() );
        }
    }

    return best_it->worker;
}

std::vector<std::string> basecamp::release_crafting_tools( const recipe &making,
        const mapgen_arguments &args, int batch_size )
{
    map &target_map = get_camp_map();
    form_storage_zones( target_map, bb_pos );
    form_crafting_inventory( target_map );

    const auto filter = is_crafting_component;
    avatar &player_character = get_avatar();
    const requirement_data *req = nullptr;
    if( making.is_blueprint() ) {
        const std::unordered_map<mapgen_arguments, build_reqs> &reqs_map =
            making.blueprint_build_reqs().reqs_by_parameters;
        auto req_it = reqs_map.find( args );
        if( req_it == reqs_map.end() ) {
            return {};
        }
        req = &req_it->second.consolidated_reqs;
    } else {
        req = making.deduped_requirements().select_alternative( player_character, _inv, filter, batch_size );
    }
    if( req == nullptr ) {
        return {};
    }

    const auto drop_abs_pos = get_dumping_spot().raw() == tripoint::zero ? bb_pos : get_dumping_spot();
    const tripoint_bub_ms drop_pos = target_map.get_bub( drop_abs_pos );
    std::vector<std::string> released_notes;
    bool released_any = false;

    while( true ) {
        std::set<itype_id> missing_tools;
        for( const std::vector<tool_comp> &alternatives : req->get_tools() ) {
            const bool satisfied = std::any_of( alternatives.begin(), alternatives.end(),
            [&]( const tool_comp & tool ) {
                return tool.has( _inv, filter, batch_size );
            } );
            if( satisfied ) {
                continue;
            }
            for( const tool_comp &tool : alternatives ) {
                if( !tool.type.is_empty() ) {
                    missing_tools.insert( tool.type );
                }
            }
        }

        std::vector<quality_requirement> missing_qualities;
        for( const std::vector<quality_requirement> &alternatives : req->get_qualities() ) {
            const bool satisfied = std::any_of( alternatives.begin(), alternatives.end(),
            [&]( const quality_requirement & quality ) {
                return quality.has( _inv, filter, batch_size );
            } );
            if( !satisfied ) {
                missing_qualities.insert( missing_qualities.end(), alternatives.begin(), alternatives.end() );
            }
        }

        if( missing_tools.empty() && missing_qualities.empty() ) {
            break;
        }

        bool released_this_round = false;
        validate_assignees();
        for( const npc_ptr &guy : assigned_npcs ) {
            if( guy == nullptr || guy->has_companion_mission() ) {
                continue;
            }

            auto matches_missing_tool = [&]( const item & candidate ) {
                if( missing_tools.find( candidate.typeId() ) != missing_tools.end() ) {
                    return true;
                }
                if( !candidate.is_tool() && !candidate.is_container() ) {
                    return false;
                }
                return std::any_of( missing_qualities.begin(), missing_qualities.end(),
                [&]( const quality_requirement & quality ) {
                    return candidate.get_quality( quality.type ) >= quality.level;
                } );
            };

            std::vector<item *> carried = guy->inv_dump();
            auto carried_it = std::find_if( carried.begin(), carried.end(), [&]( item * candidate ) {
                return candidate != nullptr && matches_missing_tool( *candidate );
            } );
            if( carried_it == carried.end() ) {
                continue;
            }

            item released = guy->remove_item( **carried_it );
            const std::string released_name = released.tname();
            target_map.add_item_or_charges( drop_pos, std::move( released ) );
            released_notes.push_back( string_format( _( "%1$s turned over %2$s to camp stock." ),
                                                     guy->disp_name(), released_name ) );
            released_any = true;
            released_this_round = true;
            form_crafting_inventory( target_map );
            break;
        }

        if( !released_this_round ) {
            break;
        }
    }

    if( released_any ) {
        target_map.save();
        form_crafting_inventory( target_map );
    }

    return released_notes;
}

namespace
{

static std::string normalize_camp_request_text( std::string_view text )
{
    std::string out;
    out.reserve( text.size() );
    bool last_space = true;
    for( const char ch : text ) {
        const unsigned char uch = static_cast<unsigned char>( ch );
        if( std::isalnum( uch ) ) {
            out.push_back( static_cast<char>( std::tolower( uch ) ) );
            last_space = false;
        } else if( !last_space ) {
            out.push_back( ' ' );
            last_space = true;
        }
    }
    while( !out.empty() && out.back() == ' ' ) {
        out.pop_back();
    }
    if( !out.empty() && out.front() == ' ' ) {
        out.erase( 0, out.find_first_not_of( ' ' ) );
    }
    return out;
}

static bool consume_camp_request_prefix( std::string &text, const std::string_view prefix )
{
    if( text.size() < prefix.size() || text.compare( 0, prefix.size(), prefix ) != 0 ) {
        return false;
    }
    text.erase( 0, prefix.size() );
    while( !text.empty() && text.front() == ' ' ) {
        text.erase( text.begin() );
    }
    return true;
}

static bool strip_camp_request_suffix( std::string &text, const std::string_view suffix )
{
    if( text.size() < suffix.size() || text.compare( text.size() - suffix.size(), suffix.size(), suffix ) != 0 ) {
        return false;
    }
    text.erase( text.size() - suffix.size() );
    while( !text.empty() && text.back() == ' ' ) {
        text.pop_back();
    }
    return true;
}

static void strip_camp_request_articles( std::string &text )
{
    while( consume_camp_request_prefix( text, "the " ) ||
           consume_camp_request_prefix( text, "some " ) ||
           consume_camp_request_prefix( text, "a " ) ||
           consume_camp_request_prefix( text, "an " ) ) {
    }
}

static int parse_camp_count_word( std::string_view token )
{
    static const std::vector<std::pair<std::string_view, int>> word_numbers = {
        { "one", 1 }, { "two", 2 },   { "three", 3 }, { "four", 4 }, { "five", 5 },
        { "six", 6 }, { "seven", 7 }, { "eight", 8 }, { "nine", 9 }, { "ten", 10 }
    };
    const auto it = std::find_if( word_numbers.begin(), word_numbers.end(), [&]( const auto & entry ) {
        return entry.first == token;
    } );
    return it == word_numbers.end() ? 0 : it->second;
}

static void strip_camp_request_polite_prefixes( std::string &text )
{
    while( consume_camp_request_prefix( text, "please " ) ||
           consume_camp_request_prefix( text, "hey " ) ||
           consume_camp_request_prefix( text, "ok " ) ||
           consume_camp_request_prefix( text, "okay " ) ) {
    }
}

static void strip_camp_request_leading_prompt( std::string &text )
{
    bool changed = true;
    while( changed ) {
        const std::string before = text;
        strip_camp_request_polite_prefixes( text );
        consume_camp_request_prefix( text, "can you " ) ||
        consume_camp_request_prefix( text, "could you " ) ||
        consume_camp_request_prefix( text, "would you " ) ||
        consume_camp_request_prefix( text, "will you " );
        changed = text != before;
    }
}

static void strip_camp_request_board_reference( std::string &text )
{
    while( consume_camp_request_prefix( text, "that " ) ||
           consume_camp_request_prefix( text, "the " ) ) {
    }
    while( consume_camp_request_prefix( text, "plan for " ) ||
           consume_camp_request_prefix( text, "work order for " ) ||
           consume_camp_request_prefix( text, "order for " ) ||
           consume_camp_request_prefix( text, "request for " ) ||
           consume_camp_request_prefix( text, "job for " ) ||
           consume_camp_request_prefix( text, "plan " ) ||
           consume_camp_request_prefix( text, "work order " ) ||
           consume_camp_request_prefix( text, "order " ) ||
           consume_camp_request_prefix( text, "request " ) ||
           consume_camp_request_prefix( text, "job " ) ) {
    }
    strip_camp_request_articles( text );
}

static std::optional<int> parse_camp_request_id_reference( std::string text )
{
    while( consume_camp_request_prefix( text, "number " ) ||
           consume_camp_request_prefix( text, "id " ) ) {
    }
    if( text.empty() ) {
        return std::nullopt;
    }
    if( !std::all_of( text.begin(), text.end(), []( const char ch ) {
        return std::isdigit( static_cast<unsigned char>( ch ) );
    } ) ) {
        return std::nullopt;
    }
    return std::stoi( text );
}

static std::optional<int> parse_camp_signed_int( std::string_view raw_text )
{
    const std::string text = trim( raw_text );
    if( text.empty() ) {
        return std::nullopt;
    }

    const ret_val<int> parsed = try_parse_integer<int>( text, false );
    if( !parsed.success() ) {
        return std::nullopt;
    }
    return parsed.value();
}

static basecamp_ai::parsed_camp_request_reference finalize_camp_request_reference( std::string text,
        bool allow_all_requests )
{
    strip_camp_request_board_reference( text );
    strip_camp_request_suffix( text, " please" );

    basecamp_ai::parsed_camp_request_reference result;
    if( allow_all_requests && ( text == "all" || text == "them" || text == "everything" ||
                                text == "all of them" || text == "all work orders" ||
                                text == "all requests" || text == "all jobs" ||
                                text == "all ready work orders" ||
                                text == "all ready orders" ||
                                text == "all ready requests" ||
                                text == "all ready jobs" ) ) {
        result.all_requests = true;
        return result;
    }

    if( const std::optional<int> request_id = parse_camp_request_id_reference( text ) ) {
        result.request_id = *request_id;
        result.has_request_id = true;
    } else {
        result.query = text;
    }
    return result;
}

} // namespace

namespace basecamp_ai
{

std::string camp_request_subject_for_display( const camp_llm_request &request,
        bool include_resolved_recipe, bool include_request_id )
{
    const std::string item_name = !request.requested_item_query.empty() ? request.requested_item_query :
                                  request.chosen_recipe_name;
    std::string subject;
    if( item_name.empty() ) {
        subject = _( "crafting request" );
    } else if( request.requested_count > 0 ) {
        subject = string_format( _( "%1$d × %2$s" ), request.requested_count, item_name );
    } else {
        subject = item_name;
    }

    if( include_resolved_recipe && !request.requested_item_query.empty() &&
        !request.chosen_recipe_name.empty() && request.chosen_recipe_name != request.requested_item_query ) {
        subject += string_format( _( " (matched %s)" ), request.chosen_recipe_name );
    }

    if( include_request_id && request.request_id > 0 ) {
        subject += string_format( _( " (#%d)" ), request.request_id );
    }

    return subject;
}

static constexpr const char *basecamp_craft_handoff_snapshot_filename =
    "basecamp_craft_handoff_snapshot.txt";
static constexpr const char *basecamp_board_handoff_snapshot_filename =
    "basecamp_board_handoff_snapshot.txt";
static constexpr const char *basecamp_board_handoff_job_line_filename =
    "basecamp_board_handoff_job_line.txt";
static constexpr const char *llm_prompt_readme_filename = "README.txt";

static bool camp_request_is_archived_for_handoff( const camp_llm_request &request )
{
    return request.status == "completed" || request.status == "cancelled";
}

static std::string camp_request_next_token( const camp_llm_request &request )
{
    if( request.request_id <= 0 ) {
        return "none";
    }
    return string_format( camp_request_is_archived_for_handoff( request ) ? "delete_job=%d" : "job=%d",
                          request.request_id );
}

static std::string camp_request_details_token( const camp_llm_request &request )
{
    return request.request_id > 0 ? string_format( "show_job=%d", request.request_id ) : "none";
}

static std::string default_basecamp_craft_handoff_snapshot_template()
{
    return R"(board=show_board
details={{details_token}}
id={{request_id}}
query={{query}}
count={{count}}
source={{source_utterance}}
request={{request_subject}}
recipe={{resolved_recipe}}
status={{status}}
approval={{approval_state}}
worker={{worker}}
blockers={{blockers}}
next={{next_token}}
)";
}

static std::string default_basecamp_board_handoff_snapshot_template()
{
    return R"(board=show_board
active={{active_count}}
archived={{archived_count}}
{{jobs}})";
}

static std::string default_basecamp_board_handoff_job_line_template()
{
    return R"(job={{job_id}} subject={{request_subject}} status={{status}} approval={{approval_state}} worker={{worker}} details={{details_token}} next={{next_token}}
)";
}

std::string camp_request_handoff_snapshot( const camp_llm_request &request )
{
    if( request.request_kind != "craft_request" ) {
        return std::string();
    }

    const auto join_fields = []( const std::vector<std::string> &parts ) {
        std::string joined;
        for( size_t idx = 0; idx < parts.size(); ++idx ) {
            if( idx > 0 ) {
                joined += "; ";
            }
            joined += parts[idx];
        }
        return joined;
    };

    const std::string query = request.requested_item_query.empty() ? "none" :
                              request.requested_item_query;
    const std::string source_utterance = request.source_utterance.empty() ? "none" :
                                         request.source_utterance;
    const std::string request_subject = camp_request_subject_for_display( request );
    const std::string resolved_recipe = request.chosen_recipe_name.empty() ? "none" :
                                        request.chosen_recipe_name;
    const std::string worker = request.assigned_worker_name.empty() ? "none" :
                               request.assigned_worker_name;
    const std::string blockers = request.blockers.empty() ? "none" :
                                 join_fields( request.blockers );
    const std::string details_token = camp_request_details_token( request );
    const std::string next_token = camp_request_next_token( request );

    const std::string templ = llm_prompt_templates::load( basecamp_craft_handoff_snapshot_filename,
                              default_basecamp_craft_handoff_snapshot_template(),
    { "{{request_id}}", "{{details_token}}", "{{query}}", "{{count}}",
      "{{source_utterance}}", "{{request_subject}}", "{{resolved_recipe}}",
      "{{status}}", "{{approval_state}}", "{{worker}}", "{{blockers}}",
      "{{next_token}}" },
    { basecamp_craft_handoff_snapshot_filename, basecamp_board_handoff_snapshot_filename,
      basecamp_board_handoff_job_line_filename, llm_prompt_readme_filename } );

    return llm_prompt_templates::render( templ,
    {
        { "{{request_id}}", std::to_string( request.request_id ) },
        { "{{details_token}}", details_token },
        { "{{query}}", query },
        { "{{count}}", std::to_string( request.requested_count ) },
        { "{{source_utterance}}", source_utterance },
        { "{{request_subject}}", request_subject },
        { "{{resolved_recipe}}", resolved_recipe },
        { "{{status}}", request.status },
        { "{{approval_state}}", request.approval_state },
        { "{{worker}}", worker },
        { "{{blockers}}", blockers },
        { "{{next_token}}", next_token }
    } );
}

std::string camp_board_handoff_snapshot( const std::vector<camp_llm_request> &requests )
{
    const std::string board_templ = llm_prompt_templates::load( basecamp_board_handoff_snapshot_filename,
                                    default_basecamp_board_handoff_snapshot_template(),
    { "{{active_count}}", "{{archived_count}}", "{{jobs}}" },
    { basecamp_craft_handoff_snapshot_filename, basecamp_board_handoff_snapshot_filename,
      basecamp_board_handoff_job_line_filename, llm_prompt_readme_filename } );
    const std::string job_templ = llm_prompt_templates::load( basecamp_board_handoff_job_line_filename,
                                  default_basecamp_board_handoff_job_line_template(),
    { "{{job_id}}", "{{request_subject}}", "{{status}}", "{{approval_state}}",
      "{{worker}}", "{{details_token}}", "{{next_token}}" },
    { basecamp_craft_handoff_snapshot_filename, basecamp_board_handoff_snapshot_filename,
      basecamp_board_handoff_job_line_filename, llm_prompt_readme_filename } );

    int active_requests = 0;
    int archived_requests = 0;
    std::string jobs;
    for( const camp_llm_request &request : requests ) {
        if( request.request_kind != "craft_request" ) {
            continue;
        }

        if( camp_request_is_archived_for_handoff( request ) ) {
            ++archived_requests;
        } else {
            ++active_requests;
        }

        jobs += llm_prompt_templates::render( job_templ,
        {
            { "{{job_id}}", std::to_string( request.request_id ) },
            { "{{request_subject}}", camp_request_subject_for_display( request, true ) },
            { "{{status}}", request.status },
            { "{{approval_state}}", request.approval_state },
            { "{{worker}}", request.assigned_worker_name.empty() ? "none" : request.assigned_worker_name },
            { "{{details_token}}", camp_request_details_token( request ) },
            { "{{next_token}}", camp_request_next_token( request ) }
        } );
    }

    return llm_prompt_templates::render( board_templ,
    {
        { "{{active_count}}", std::to_string( active_requests ) },
        { "{{archived_count}}", std::to_string( archived_requests ) },
        { "{{jobs}}", jobs.empty() ? "jobs=none\n" : jobs }
    } );
}

static std::optional<parsed_camp_craft_order> parse_camp_craft_order_payload( std::string text )
{
    strip_camp_request_articles( text );
    strip_camp_request_suffix( text, " please" );

    int count = 1;
    size_t token_end = text.find( ' ' );
    const std::string first_token = token_end == std::string::npos ? text : text.substr( 0, token_end );
    bool consumed_count = false;
    if( !first_token.empty() && std::all_of( first_token.begin(), first_token.end(), []( const char ch ) {
        return std::isdigit( static_cast<unsigned char>( ch ) );
    } ) ) {
        count = std::max( std::stoi( first_token ), 1 );
        consumed_count = true;
    } else {
        const int parsed_count = parse_camp_count_word( first_token );
        if( parsed_count > 0 ) {
            count = parsed_count;
            consumed_count = true;
        }
    }
    if( consumed_count ) {
        text = token_end == std::string::npos ? std::string() : text.substr( token_end + 1 );
    }

    strip_camp_request_articles( text );
    strip_camp_request_suffix( text, " please" );
    if( text.empty() ) {
        return std::nullopt;
    }
    return basecamp_ai::parsed_camp_craft_order{ text, count };
}

std::optional<parsed_camp_craft_order> parse_heard_camp_craft_order( std::string_view utterance )
{
    std::string text = normalize_camp_request_text( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }

    strip_camp_request_leading_prompt( text );

    bool matched_prefix = false;
    for( const std::string_view prefix : {
             std::string_view( "craft " ),
             std::string_view( "can you craft " ), std::string_view( "could you craft " ),
             std::string_view( "would you craft " ), std::string_view( "will you craft " ) } ) {
        if( consume_camp_request_prefix( text, prefix ) ) {
            matched_prefix = true;
            break;
        }
    }
    if( !matched_prefix ) {
        return std::nullopt;
    }

    while( consume_camp_request_prefix( text, "me " ) ||
           consume_camp_request_prefix( text, "us " ) ||
           consume_camp_request_prefix( text, "for me " ) ||
           consume_camp_request_prefix( text, "for us " ) ) {
    }

    return parse_camp_craft_order_payload( text );
}

std::optional<parsed_camp_craft_order> parse_structured_camp_craft_order( std::string_view utterance )
{
    std::string text = trim( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }
    std::transform( text.begin(), text.end(), text.begin(), []( unsigned char ch ) {
        return static_cast<char>( std::tolower( ch ) );
    } );

    static constexpr std::string_view craft_prefix = "craft=";
    if( text.compare( 0, craft_prefix.size(), craft_prefix ) != 0 ) {
        return std::nullopt;
    }

    return parse_camp_craft_order_payload( trim( std::string_view( text ).substr( craft_prefix.size() ) ) );
}

std::optional<parsed_camp_job_token> parse_structured_camp_job_token( std::string_view utterance )
{
    std::string text = trim( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }
    std::transform( text.begin(), text.end(), text.begin(), []( unsigned char ch ) {
        return static_cast<char>( std::tolower( ch ) );
    } );

    if( text == "show_board" ) {
        return parsed_camp_job_token{ camp_job_token_kind::show_board, 0 };
    }
    if( text == "launch_ready_jobs" ) {
        return parsed_camp_job_token{ camp_job_token_kind::launch_ready_jobs, 0 };
    }
    if( text == "retry_blocked_jobs" ) {
        return parsed_camp_job_token{ camp_job_token_kind::retry_blocked_jobs, 0 };
    }
    if( text == "clear_archived_jobs" ) {
        return parsed_camp_job_token{ camp_job_token_kind::clear_archived_jobs, 0 };
    }

    static constexpr std::string_view show_prefix = "show_job=";
    static constexpr std::string_view job_prefix = "job=";
    static constexpr std::string_view delete_prefix = "delete_job=";
    camp_job_token_kind kind;
    std::string_view id_text;
    if( text.compare( 0, show_prefix.size(), show_prefix ) == 0 ) {
        kind = camp_job_token_kind::show_job;
        id_text = std::string_view( text ).substr( show_prefix.size() );
    } else if( text.compare( 0, delete_prefix.size(), delete_prefix ) == 0 ) {
        kind = camp_job_token_kind::delete_job;
        id_text = std::string_view( text ).substr( delete_prefix.size() );
    } else if( text.compare( 0, job_prefix.size(), job_prefix ) == 0 ) {
        kind = camp_job_token_kind::act_on_job;
        id_text = std::string_view( text ).substr( job_prefix.size() );
    } else {
        return std::nullopt;
    }

    const std::optional<int> request_id = parse_camp_signed_int( id_text );
    if( !request_id.has_value() || *request_id <= 0 ) {
        return std::nullopt;
    }

    return parsed_camp_job_token{ kind, *request_id };
}

std::optional<parsed_camp_request_reference> parse_heard_camp_clear_query( std::string_view utterance )
{
    std::string text = normalize_camp_request_text( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }

    strip_camp_request_leading_prompt( text );

    bool matched_prefix = false;
    for( const std::string_view prefix : {
             std::string_view( "clear " ), std::string_view( "erase " ),
             std::string_view( "remove " ), std::string_view( "clean up " ),
             std::string_view( "can you clear " ), std::string_view( "could you clear " ),
             std::string_view( "would you clear " ), std::string_view( "will you clear " ),
             std::string_view( "can you erase " ), std::string_view( "could you erase " ),
             std::string_view( "would you erase " ), std::string_view( "will you erase " ),
             std::string_view( "can you remove " ), std::string_view( "could you remove " ),
             std::string_view( "would you remove " ), std::string_view( "will you remove " ) } ) {
        if( consume_camp_request_prefix( text, prefix ) ) {
            matched_prefix = true;
            break;
        }
    }
    if( !matched_prefix ) {
        return std::nullopt;
    }

    strip_camp_request_suffix( text, " please" );

    std::string archive_target = text;
    strip_camp_request_articles( archive_target );
    if( archive_target == "archived requests" || archive_target == "archived work orders" ||
        archive_target == "archived jobs" || archive_target == "completed requests" ||
        archive_target == "completed work orders" || archive_target == "completed jobs" ||
        archive_target == "cancelled requests" || archive_target == "cancelled work orders" ||
        archive_target == "cancelled jobs" || archive_target == "old paperwork" ||
        archive_target == "archived paperwork" || archive_target == "completed paperwork" ||
        archive_target == "cancelled paperwork" || archive_target == "paperwork" ) {
        basecamp_ai::parsed_camp_request_reference result;
        result.all_requests = true;
        return result;
    }

    basecamp_ai::parsed_camp_request_reference result = finalize_camp_request_reference( text, false );
    if( !result.has_request_id && result.query.empty() ) {
        return std::nullopt;
    }
    return result;
}

std::optional<parsed_camp_request_reference> parse_heard_camp_cancel_query( std::string_view utterance )
{
    std::string text = normalize_camp_request_text( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }

    strip_camp_request_leading_prompt( text );

    bool matched_prefix = false;
    for( const std::string_view prefix : {
             std::string_view( "cancel " ), std::string_view( "scrap " ),
             std::string_view( "drop " ), std::string_view( "forget " ),
             std::string_view( "can you cancel " ), std::string_view( "could you cancel " ),
             std::string_view( "would you cancel " ), std::string_view( "will you cancel " ) } ) {
        if( consume_camp_request_prefix( text, prefix ) ) {
            matched_prefix = true;
            break;
        }
    }
    if( !matched_prefix ) {
        return std::nullopt;
    }

    basecamp_ai::parsed_camp_request_reference result = finalize_camp_request_reference( text, false );
    if( !result.has_request_id && result.query.empty() ) {
        return std::nullopt;
    }
    return result;
}

std::optional<parsed_camp_request_reference> parse_heard_camp_approval_query( std::string_view utterance )
{
    std::string text = normalize_camp_request_text( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }

    strip_camp_request_leading_prompt( text );

    bool matched_prefix = false;
    for( const std::string_view prefix : {
             std::string_view( "approve " ), std::string_view( "launch " ),
             std::string_view( "retry " ), std::string_view( "resume " ),
             std::string_view( "kick off " ), std::string_view( "go ahead with " ),
             std::string_view( "go ahead on " ), std::string_view( "can you approve " ),
             std::string_view( "could you approve " ), std::string_view( "would you approve " ),
             std::string_view( "will you approve " ), std::string_view( "can you launch " ),
             std::string_view( "could you launch " ), std::string_view( "would you launch " ),
             std::string_view( "will you launch " ), std::string_view( "can you retry " ),
             std::string_view( "could you retry " ), std::string_view( "would you retry " ),
             std::string_view( "will you retry " ) } ) {
        if( consume_camp_request_prefix( text, prefix ) ) {
            matched_prefix = true;
            break;
        }
    }
    if( !matched_prefix ) {
        return std::nullopt;
    }

    std::string batch_target = text;
    strip_camp_request_suffix( batch_target, " please" );
    strip_camp_request_articles( batch_target );
    if( batch_target == "all blocked work orders" || batch_target == "all blocked orders" ||
        batch_target == "all blocked requests" || batch_target == "all blocked jobs" ) {
        basecamp_ai::parsed_camp_request_reference result;
        result.all_blocked_requests = true;
        return result;
    }

    basecamp_ai::parsed_camp_request_reference result = finalize_camp_request_reference( text, true );
    if( !result.all_requests && !result.all_blocked_requests && !result.has_request_id &&
        result.query.empty() ) {
        return std::nullopt;
    }
    return result;
}

std::optional<parsed_camp_request_reference> parse_heard_camp_status_query( std::string_view utterance )
{
    std::string text = normalize_camp_request_text( utterance );
    if( text.empty() ) {
        return std::nullopt;
    }

    strip_camp_request_leading_prompt( text );

    for( const std::string_view exact : {
             std::string_view( "what s on the board" ),
             std::string_view( "what is on the board" ),
             std::string_view( "what s on the request board" ),
             std::string_view( "what is on the request board" ),
             std::string_view( "show the board" ),
             std::string_view( "show me the board" ),
             std::string_view( "check the board" ),
             std::string_view( "read the board" ),
             std::string_view( "board status" ),
             std::string_view( "camp board status" ),
             std::string_view( "any work orders" ),
             std::string_view( "any requests" ),
             std::string_view( "what requests do we have" ),
             std::string_view( "do we have any work orders" ) } ) {
        if( text == exact ) {
            basecamp_ai::parsed_camp_request_reference result;
            result.all_requests = true;
            return result;
        }
    }

    bool matched_prefix = false;
    for( const std::string_view prefix : {
             std::string_view( "status of " ), std::string_view( "status for " ),
             std::string_view( "what s the status of " ),
             std::string_view( "what is the status of " ),
             std::string_view( "show me the status of " ),
             std::string_view( "show the status of " ),
             std::string_view( "read off the status of " ) } ) {
        if( consume_camp_request_prefix( text, prefix ) ) {
            matched_prefix = true;
            break;
        }
    }
    if( !matched_prefix ) {
        return std::nullopt;
    }

    basecamp_ai::parsed_camp_request_reference result = finalize_camp_request_reference( text, false );
    if( !result.has_request_id && result.query.empty() ) {
        result.all_requests = true;
    }
    return result;
}

std::vector<int> collect_ready_camp_request_ids( const std::vector<camp_llm_request> &requests )
{
    std::vector<int> ready_ids;
    ready_ids.reserve( requests.size() );
    for( const camp_llm_request &request : requests ) {
        if( request.status == "awaiting_approval" ) {
            ready_ids.push_back( request.request_id );
        }
    }
    return ready_ids;
}

std::vector<int> collect_blocked_camp_request_ids( const std::vector<camp_llm_request> &requests )
{
    std::vector<int> blocked_ids;
    blocked_ids.reserve( requests.size() );
    for( const camp_llm_request &request : requests ) {
        if( request.status == "blocked" ) {
            blocked_ids.push_back( request.request_id );
        }
    }
    return blocked_ids;
}

std::vector<int> collect_archived_camp_request_ids( const std::vector<camp_llm_request> &requests )
{
    std::vector<int> archived_ids;
    archived_ids.reserve( requests.size() );
    for( const camp_llm_request &request : requests ) {
        if( request.status == "completed" || request.status == "cancelled" ) {
            archived_ids.push_back( request.request_id );
        }
    }
    return archived_ids;
}

bool matches_assigned_camp_request_worker( const camp_llm_request &request,
        const character_id &worker_id, std::string_view worker_name )
{
    if( request.assigned_worker_npc_id.is_valid() ) {
        return request.assigned_worker_npc_id == worker_id;
    }
    if( !request.assigned_worker_name.empty() ) {
        return request.assigned_worker_name == worker_name;
    }
    return false;
}

bool parse_relative_omt_delta( std::string_view dx_text, std::string_view dy_text,
                               point_rel_omt &delta, std::string &error )
{
    delta = point_rel_omt::zero;
    error.clear();

    const std::optional<int> parsed_dx = parse_camp_signed_int( dx_text );
    if( !parsed_dx.has_value() ) {
        error = "Overmap delta dx is invalid.";
        return false;
    }

    const std::optional<int> parsed_dy = parse_camp_signed_int( dy_text );
    if( !parsed_dy.has_value() ) {
        error = "Overmap delta dy is invalid.";
        return false;
    }

    delta = point_rel_omt( *parsed_dx, *parsed_dy );
    return true;
}

bool parse_overmap_movement_token( std::string_view token,
                                   parsed_overmap_movement_intent &intent,
                                   std::string &error )
{
    intent = parsed_overmap_movement_intent{};
    error.clear();

    const std::string text = trim( token );
    if( text == "stay" ) {
        return true;
    }

    static constexpr std::string_view format_error =
        "Overmap move token must use 'stay' or 'move_omt dx=<signed_int> dy=<signed_int>'.";
    static constexpr std::string_view prefix = "move_omt";
    if( text.size() <= prefix.size() || text.compare( 0, prefix.size(), prefix ) != 0 ||
        !std::isspace( static_cast<unsigned char>( text[prefix.size()] ) ) ) {
        error = std::string( format_error );
        return false;
    }

    const std::string fields = trim( std::string_view( text ).substr( prefix.size() ) );
    static constexpr std::string_view dx_prefix = "dx=";
    static constexpr std::string_view dy_prefix = "dy=";
    std::optional<std::string_view> dx_text;
    std::optional<std::string_view> dy_text;

    size_t pos = 0;
    while( pos < fields.size() ) {
        while( pos < fields.size() && std::isspace( static_cast<unsigned char>( fields[pos] ) ) ) {
            ++pos;
        }
        if( pos >= fields.size() ) {
            break;
        }

        size_t end = pos;
        while( end < fields.size() && !std::isspace( static_cast<unsigned char>( fields[end] ) ) ) {
            ++end;
        }

        const std::string_view field( fields.data() + pos, end - pos );
        if( field.compare( 0, dx_prefix.size(), dx_prefix ) == 0 ) {
            if( dx_text.has_value() ) {
                error = std::string( format_error );
                return false;
            }
            dx_text = field.substr( dx_prefix.size() );
        } else if( field.compare( 0, dy_prefix.size(), dy_prefix ) == 0 ) {
            if( dy_text.has_value() ) {
                error = std::string( format_error );
                return false;
            }
            dy_text = field.substr( dy_prefix.size() );
        } else {
            error = std::string( format_error );
            return false;
        }

        pos = end;
    }

    if( !dx_text.has_value() || !dy_text.has_value() || dx_text->empty() || dy_text->empty() ) {
        error = std::string( format_error );
        return false;
    }

    point_rel_omt delta = point_rel_omt::zero;
    if( !parse_relative_omt_delta( *dx_text, *dy_text, delta, error ) ) {
        return false;
    }

    intent.stay = false;
    intent.delta = delta;
    return true;
}

tripoint_abs_omt resolve_overmap_movement_target( const tripoint_abs_omt &origin,
        const parsed_overmap_movement_intent &intent )
{
    if( intent.stay ) {
        return origin;
    }

    return tripoint_abs_omt( origin.x() + intent.delta.x(),
                             origin.y() - intent.delta.y(),
                             origin.z() );
}

bool format_overmap_movement_token( const tripoint_abs_omt &origin,
                                    const tripoint_abs_omt &target,
                                    std::string &token,
                                    std::string &error )
{
    token.clear();
    error.clear();

    if( origin.z() != target.z() ) {
        error = "Overmap move token cannot encode z-level changes.";
        return false;
    }

    const int dx = target.x() - origin.x();
    const int dy = origin.y() - target.y();
    if( dx == 0 && dy == 0 ) {
        token = "stay";
        return true;
    }

    token = string_format( "move_omt dx=%+d dy=%+d", dx, dy );
    return true;
}

} // namespace basecamp_ai

namespace
{

static std::vector<std::string_view> split_camp_request_words( const std::string &text )
{
    std::vector<std::string_view> words;
    size_t word_start = 0;
    while( word_start < text.size() ) {
        const size_t word_end = text.find( ' ', word_start );
        if( word_end == std::string::npos ) {
            words.emplace_back( text.data() + word_start, text.size() - word_start );
            break;
        }
        words.emplace_back( text.data() + word_start, word_end - word_start );
        word_start = word_end + 1;
    }
    return words;
}

static bool contains_camp_request_phrase( std::string_view text, std::string_view phrase )
{
    if( text.size() < phrase.size() ) {
        return false;
    }
    size_t pos = text.find( phrase );
    while( pos != std::string::npos ) {
        const bool start_boundary = pos == 0 || text[pos - 1] == ' ';
        const size_t end_pos = pos + phrase.size();
        const bool end_boundary = end_pos == text.size() || text[end_pos] == ' ';
        if( start_boundary && end_boundary ) {
            return true;
        }
        pos = text.find( phrase, pos + 1 );
    }
    return false;
}

static std::string singularize_final_camp_request_word( std::string_view query )
{
    const std::string normalized_query = normalize_camp_request_text( query );
    if( normalized_query.empty() ) {
        return std::string();
    }

    const size_t last_space = normalized_query.rfind( ' ' );
    const size_t word_start = last_space == std::string::npos ? 0 : last_space + 1;
    const size_t word_length = normalized_query.size() - word_start;
    if( word_length <= 1 || normalized_query.back() != 's' ) {
        return std::string();
    }

    std::string singularized = normalized_query;
    singularized.pop_back();
    return singularized;
}

static int score_camp_request_text_match( std::string_view candidate, std::string_view query );

struct scored_camp_recipe_query {
    int score = 0;
    std::string fallback_query;
    bool used_fallback = false;
};

static scored_camp_recipe_query score_camp_recipe_query_details( const recipe &making,
        std::string_view query )
{
    const auto score_query = [&]( std::string_view candidate_query ) {
        int score = 0;
        score = std::max( score, score_camp_request_text_match( making.result_name(), candidate_query ) );
        score = std::max( score, score_camp_request_text_match( making.result()->nname( 1 ), candidate_query ) );
        score = std::max( score, score_camp_request_text_match( making.result()->nname( 2 ), candidate_query ) );
        return score;
    };

    scored_camp_recipe_query result;
    result.score = score_query( query );
    if( result.score >= 650 ) {
        return result;
    }

    result.fallback_query = singularize_final_camp_request_word( query );
    if( result.fallback_query.empty() ) {
        return result;
    }

    const int fallback_score = score_query( result.fallback_query );
    if( fallback_score > result.score ) {
        result.score = fallback_score;
        result.used_fallback = true;
    }
    return result;
}

static const std::unordered_set<recipe_id> &all_camp_craft_recipe_ids()
{
    static const std::unordered_set<recipe_id> all_recipes = []() {
        std::unordered_set<recipe_id> result;
        for( auto it = recipe_dict.begin(); it != recipe_dict.end(); ++it ) {
            const recipe &making = it->second;
            if( making.is_practice() || making.is_nested() || making.is_blueprint() ) {
                continue;
            }
            result.insert( it->first );
        }
        return result;
    }();
    return all_recipes;
}

static bool camp_request_words_match_in_order( const std::vector<std::string_view> &candidate_words,
        const std::vector<std::string_view> &query_words )
{
    if( candidate_words.empty() || query_words.empty() ) {
        return false;
    }
    size_t query_index = 0;
    for( const std::string_view candidate_word : candidate_words ) {
        if( candidate_word == query_words[query_index] ) {
            query_index++;
            if( query_index == query_words.size() ) {
                return true;
            }
        }
    }
    return false;
}

static int score_camp_request_text_match( std::string_view candidate, std::string_view query )
{
    const std::string normalized_candidate = normalize_camp_request_text( candidate );
    const std::string normalized_query = normalize_camp_request_text( query );
    if( normalized_candidate.empty() || normalized_query.empty() ) {
        return 0;
    }
    if( normalized_candidate == normalized_query ) {
        return 1000;
    }
    if( normalized_candidate.size() > normalized_query.size() &&
        normalized_candidate.compare( 0, normalized_query.size(), normalized_query ) == 0 &&
        normalized_candidate[normalized_query.size()] == ' ' ) {
        return 920;
    }
    if( contains_camp_request_phrase( normalized_candidate, normalized_query ) ) {
        return 880;
    }

    const std::vector<std::string_view> candidate_words = split_camp_request_words( normalized_candidate );
    const std::vector<std::string_view> query_words = split_camp_request_words( normalized_query );
    if( camp_request_words_match_in_order( candidate_words, query_words ) ) {
        return 800 + static_cast<int>( query_words.size() * 20 );
    }
    if( contains_camp_request_phrase( normalized_query, normalized_candidate ) ) {
        return 740;
    }
    return 0;
}

} // namespace

int basecamp_ai::score_camp_recipe_query( const recipe &making, std::string_view query )
{
    return score_camp_recipe_query_details( making, query ).score;
}

basecamp_ai::camp_craft_recipe_match basecamp_ai::match_camp_craft_query(
    const std::unordered_set<recipe_id> &available_recipes, std::string_view query )
{
    camp_craft_recipe_match result;
    std::unordered_map<recipe_id, scored_camp_recipe_query> top_scores;

    for( const recipe_id &known_recipe : available_recipes ) {
        if( known_recipe.is_empty() ) {
            continue;
        }
        const recipe &making = *known_recipe;
        const scored_camp_recipe_query scored = score_camp_recipe_query_details( making, query );
        if( scored.score <= 0 ) {
            continue;
        }
        if( scored.score > result.score ) {
            result.score = scored.score;
            result.recipe_ids = { known_recipe };
            top_scores.clear();
            top_scores.emplace( known_recipe, scored );
        } else if( scored.score == result.score ) {
            result.recipe_ids.push_back( known_recipe );
            top_scores.emplace( known_recipe, scored );
        }
    }

    if( result.score < 650 ) {
        result.score = 0;
        result.recipe_ids.clear();
        return result;
    }

    std::sort( result.recipe_ids.begin(), result.recipe_ids.end(), []( const recipe_id & lhs,
    const recipe_id & rhs ) {
        const recipe &left_recipe = *lhs;
        const recipe &right_recipe = *rhs;
        if( left_recipe.result_name() != right_recipe.result_name() ) {
            return left_recipe.result_name() < right_recipe.result_name();
        }
        return lhs.str() < rhs.str();
    } );

    for( const recipe_id &matched_recipe : result.recipe_ids ) {
        const std::string subject = matched_recipe->result_name();
        if( std::find( result.subjects.begin(), result.subjects.end(), subject ) == result.subjects.end() ) {
            result.subjects.push_back( subject );
        }

        const auto score_it = top_scores.find( matched_recipe );
        if( score_it != top_scores.end() ) {
            const scored_camp_recipe_query &scored = score_it->second;
            if( scored.used_fallback && result.fallback_query.empty() ) {
                result.fallback_query = scored.fallback_query;
            }
            result.score_notes.push_back( string_format( "%1$s score=%2$d via %3$s", subject,
                                          scored.score,
                                          scored.used_fallback ?
                                          string_format( "fallback \"%s\"", scored.fallback_query ) :
                                          "original query" ) );
        }
    }

    return result;
}

basecamp_ai::camp_craft_resolution basecamp_ai::resolve_camp_craft_query(
    const std::unordered_set<recipe_id> &available_recipes, std::string_view query,
    const std::function<camp_craft_recipe_candidate( const recipe & )> &evaluate_recipe )
{
    camp_craft_resolution result;
    result.match = match_camp_craft_query( available_recipes, query );

    if( result.match.recipe_ids.empty() ) {
        result.outcome = camp_craft_resolution_outcome::NO_MATCH;
        return result;
    }
    if( result.match.subjects.size() > 1 ) {
        result.outcome = camp_craft_resolution_outcome::MATCH_AMBIGUOUS;
        return result;
    }

    for( const recipe_id &matched_recipe : result.match.recipe_ids ) {
        const recipe &making = *matched_recipe;
        const camp_craft_recipe_candidate candidate = evaluate_recipe( making );
        const bool has_worker = candidate.worker != nullptr;

        const bool better = !result.choice.has_value() ||
                            ( has_worker && result.choice->candidate.worker == nullptr ) ||
                            ( has_worker && result.choice->candidate.worker != nullptr &&
                              ( result.choice->candidate.duration == 0_turns ||
                                candidate.duration < result.choice->candidate.duration ) ) ||
                            ( has_worker == ( result.choice->candidate.worker != nullptr ) &&
                              candidate.duration == result.choice->candidate.duration &&
                              making.result_name() < result.choice->subject ) ||
                            ( has_worker == ( result.choice->candidate.worker != nullptr ) &&
                              candidate.duration == result.choice->candidate.duration &&
                              making.result_name() == result.choice->subject &&
                              matched_recipe.str() < result.choice->recipe_id.str() );
        if( better ) {
            result.choice = resolved_camp_craft_recipe{ matched_recipe, making.result_name(), candidate };
        }
    }

    if( !result.choice.has_value() || result.choice->candidate.worker == nullptr ) {
        result.outcome = camp_craft_resolution_outcome::MATCH_BLOCKED;
    } else {
        result.outcome = camp_craft_resolution_outcome::MATCH_START;
    }

    return result;
}

namespace
{

static int score_camp_board_request_match( const camp_llm_request &request, std::string_view query )
{
    int subject_score = 0;
    subject_score = std::max( subject_score, score_camp_request_text_match( request.requested_item_query, query ) );
    subject_score = std::max( subject_score, score_camp_request_text_match( request.chosen_recipe_name, query ) );
    subject_score = std::max( subject_score, score_camp_request_text_match( request.source_utterance, query ) );

    int metadata_score = 0;
    metadata_score = std::max( metadata_score,
                               score_camp_request_text_match( request.assigned_worker_name, query ) );
    metadata_score = std::max( metadata_score,
                               score_camp_request_text_match( camp_request_status_label( request ), query ) );
    if( request.approval_state != "not_needed" ) {
        metadata_score = std::max( metadata_score,
                                   score_camp_request_text_match( camp_request_approval_label( request ), query ) );
    }

    return std::max( subject_score, metadata_score ) +
           ( subject_score > 0 && metadata_score > 0 ? metadata_score : 0 );
}

} // namespace

basecamp_ai::camp_request_match_result basecamp_ai::match_camp_request_reference(
    const std::vector<camp_llm_request> &requests,
    const parsed_camp_request_reference &reference,
    const std::function<bool( const camp_llm_request & )> &predicate )
{
    camp_request_match_result result;
    if( reference.all_requests ) {
        return result;
    }
    if( reference.has_request_id ) {
        const auto it = std::find_if( requests.begin(), requests.end(), [&]( const camp_llm_request & request ) {
            return request.request_id == reference.request_id && predicate( request );
        } );
        if( it != requests.end() ) {
            result.request_id = it->request_id;
            result.found = true;
            result.score = 1000;
        }
        return result;
    }
    if( reference.query.empty() ) {
        return result;
    }

    for( const camp_llm_request &request : requests ) {
        if( !predicate( request ) ) {
            continue;
        }
        const int score = score_camp_board_request_match( request, reference.query );
        if( score > result.score ) {
            result.score = score;
            result.request_id = request.request_id;
            result.found = true;
            result.ambiguous_matches = { camp_request_summary( request ) };
        } else if( score == result.score && score >= 650 ) {
            result.ambiguous_matches.push_back( camp_request_summary( request ) );
        }
    }

    if( result.score < 650 ) {
        result = camp_request_match_result{};
    } else if( result.ambiguous_matches.size() > 1 ) {
        result.found = false;
    } else {
        result.ambiguous_matches.clear();
    }
    return result;
}

int basecamp::add_crafting_request( const recipe &making, int batch_size, const mission_id &miss_id,
                                    const npc &worker, const std::string &source_utterance,
                                    const std::string &requested_item_query,
                                    const character_id &heard_by_npc_id )
{
    camp_llm_request request;
    request.request_id = next_camp_request_id++;
    request.source_utterance = source_utterance;
    request.requested_item_query = requested_item_query.empty() ? making.result_name() : requested_item_query;
    request.requested_count = batch_size;
    request.chosen_recipe_id = making.ident();
    request.chosen_recipe_name = making.result_name();
    request.status = "in_progress";
    request.approval_state = "not_needed";
    request.active_mission_id = miss_id;
    request.heard_by_npc_id = heard_by_npc_id;
    request.assigned_worker_npc_id = worker.getID();
    request.assigned_worker_name = worker.disp_name();
    request.created_turn = calendar::turn;
    request.updated_turn = calendar::turn;
    request.eta_turn = worker.companion_mission_time_ret;
    add_camp_request_note( request, "plan",
                           string_format( _( "%1$s started crafting %2$d × %3$s." ),
                                          worker.disp_name(), batch_size, making.result_name() ) );
    camp_requests.push_back( request );
    return request.request_id;
}

int basecamp::queue_crafting_request( const recipe &making, int batch_size, const npc &worker,
                                      const std::string &source_utterance,
                                      const std::string &requested_item_query,
                                      const character_id &heard_by_npc_id )
{
    camp_llm_request request;
    request.request_id = next_camp_request_id++;
    request.source_utterance = source_utterance;
    request.requested_item_query = requested_item_query.empty() ? making.result_name() : requested_item_query;
    request.requested_count = batch_size;
    request.chosen_recipe_id = making.ident();
    request.chosen_recipe_name = making.result_name();
    request.status = "awaiting_approval";
    request.approval_state = "waiting_player";
    request.heard_by_npc_id = heard_by_npc_id;
    request.assigned_worker_npc_id = worker.getID();
    request.assigned_worker_name = worker.disp_name();
    request.created_turn = calendar::turn;
    request.updated_turn = calendar::turn;
    add_camp_request_note( request, "plan",
                           string_format( _( "%1$s pinned crafting request #%2$d for %3$d × %4$s." ),
                                          worker.disp_name(), request.request_id, batch_size,
                                          making.result_name() ) );
    camp_requests.push_back( request );
    return request.request_id;
}

bool basecamp::handle_heard_camp_request( npc &listener, const std::string &utterance )
{
    if( !listener.assigned_camp || listener.assigned_camp->xy() != omt_pos.xy() ) {
        return false;
    }

    const auto handle_craft_order = [&]( const basecamp_ai::parsed_camp_craft_order &parsed ) {
        const std::unordered_set<recipe_id> known_camp_recipes = recipe_deck_all();
        const std::unordered_set<recipe_id> &search_recipes = all_camp_craft_recipe_ids();
        const basecamp_ai::camp_craft_resolution craft_resolution =
            basecamp_ai::resolve_camp_craft_query( search_recipes, parsed.item_query,
        [&]( const recipe & making ) {
            basecamp_ai::camp_craft_recipe_candidate candidate;
            candidate.worker = resolve_crafting_worker( making, parsed.count, listener.getID(), listener.disp_name(),
                               &candidate.resolution_note, &candidate.blockers );
            if( candidate.worker != nullptr ) {
                candidate.duration = base_camps::to_workdays( making.batch_duration( *candidate.worker, parsed.count ) );
            }
            return candidate;
        } );
        if( craft_resolution.outcome == basecamp_ai::camp_craft_resolution_outcome::NO_MATCH ) {
            log_camp_craft_trace( utterance, parsed, known_camp_recipes.size(), search_recipes.size(),
                                  craft_resolution, basecamp_ai::camp_craft_resolution_outcome::NO_MATCH );
            add_camp_request_bark( listener, camp_craft_no_match_bark( parsed.item_query ) );
            return true;
        }
        if( craft_resolution.outcome == basecamp_ai::camp_craft_resolution_outcome::MATCH_AMBIGUOUS ) {
            log_camp_craft_trace( utterance, parsed, known_camp_recipes.size(), search_recipes.size(),
                                  craft_resolution, basecamp_ai::camp_craft_resolution_outcome::MATCH_AMBIGUOUS );
            add_camp_request_bark( listener, camp_craft_ambiguous_bark( parsed.item_query,
                                   craft_resolution.match.subjects ) );
            return true;
        }
        if( !craft_resolution.choice.has_value() ) {
            log_camp_craft_trace( utterance, parsed, known_camp_recipes.size(), search_recipes.size(),
                                  craft_resolution, craft_resolution.outcome );
            return true;
        }

        const recipe *best_recipe = &*craft_resolution.choice->recipe_id;
        const npc_ptr &best_worker = craft_resolution.choice->candidate.worker;
        const std::string &best_resolution_note = craft_resolution.choice->candidate.resolution_note;
        const std::vector<std::string> &best_blockers = craft_resolution.choice->candidate.blockers;

        const npc &board_worker = best_worker != nullptr ? *best_worker : listener;
        const int request_id = queue_crafting_request( *best_recipe, parsed.count, board_worker, utterance,
                               parsed.item_query, listener.getID() );
        camp_llm_request *request = find_camp_request( request_id );
        if( request == nullptr ) {
            return true;
        }

        request->requested_item_query = parsed.item_query;
        request->heard_by_npc_id = listener.getID();
        add_camp_request_note( *request, "trace",
                               string_format( _( "Resolved \"%1$s\" to recipe \"%2$s\"." ),
                                              parsed.item_query, best_recipe->result_name() ) );
        if( !craft_resolution.match.fallback_query.empty() ) {
            add_camp_request_note( *request, "trace",
                                   string_format( _( "Fallback query used: \"%s\"." ),
                                                  craft_resolution.match.fallback_query ) );
        }
        if( !craft_resolution.match.score_notes.empty() ) {
            std::string top_matches;
            for( size_t i = 0; i < craft_resolution.match.score_notes.size(); ++i ) {
                if( i > 0 ) {
                    top_matches += " | ";
                }
                top_matches += craft_resolution.match.score_notes[i];
            }
            add_camp_request_note( *request, "trace",
                                   string_format( _( "Top matches: %s." ), top_matches ) );
        }
        if( best_worker == nullptr ) {
            request->status = "blocked";
            request->assigned_worker_npc_id = character_id();
            request->assigned_worker_name.clear();
            request->blockers = best_blockers;
            add_camp_request_note( *request, "blocked",
                                   best_blockers.empty() ?
                                   _( "The camp heard the order, but nobody can take it right now." ) :
                                   best_blockers.front() );
            log_camp_craft_trace( utterance, parsed, known_camp_recipes.size(), search_recipes.size(),
                                  craft_resolution, basecamp_ai::camp_craft_resolution_outcome::MATCH_BLOCKED,
                                  best_recipe->result_name(), request->blockers, request->request_id,
                                  request->status );
            add_camp_request_bark( listener, camp_request_blocked_bark( *request ) );
        } else {
            const camp_craft_risk_snapshot risk = describe_camp_craft_risk( *this, *best_recipe, *best_worker,
                                                  parsed.count );
            if( risk.suggested_auto ) {
                request->approval_state = "suggested_auto";
            }
            if( !best_resolution_note.empty() ) {
                add_camp_request_note( *request, "plan", best_resolution_note );
            }
            if( const std::string food_note = camp_craft_food_note( risk ); !food_note.empty() ) {
                add_camp_request_note( *request, "plan", food_note );
            }
            add_camp_request_note( *request, "plan", camp_craft_approval_note( risk ) );

            const bool started = approve_crafting_request( request_id );
            request = find_camp_request( request_id );
            if( request == nullptr ) {
                return true;
            }
            const basecamp_ai::camp_craft_resolution_outcome final_outcome =
                request->status == "blocked" ?
                basecamp_ai::camp_craft_resolution_outcome::MATCH_BLOCKED :
                basecamp_ai::camp_craft_resolution_outcome::MATCH_START;
            log_camp_craft_trace( utterance, parsed, known_camp_recipes.size(), search_recipes.size(),
                                  craft_resolution, final_outcome, best_recipe->result_name(),
                                  request->blockers, request->request_id, request->status );
            if( started && request->status == "in_progress" ) {
                add_camp_request_bark( listener, camp_request_launch_bark( *request ) );
            } else if( request->status == "blocked" ) {
                add_camp_request_bark( listener, camp_request_blocked_bark( *request ) );
            } else {
                add_camp_request_bark( listener, camp_request_queue_bark( *request ) );
            }
        }
        return true;
    };

    const auto handle_batch_launch = [&]( const bool retrying_blocked ) {
        const std::vector<int> launch_ids = retrying_blocked ?
                                            basecamp_ai::collect_blocked_camp_request_ids( camp_requests ) :
                                            basecamp_ai::collect_ready_camp_request_ids( camp_requests );
        if( launch_ids.empty() ) {
            add_camp_request_bark( listener,
                                   retrying_blocked ?
                                   _( "Nothing on the board is blocked for a retry." ) :
                                   _( "Nothing on the board is ready to launch." ) );
            return true;
        }

        const camp_request_launch_result launch_result = attempt_camp_request_launches( launch_ids,
                                                       [this]( const int request_id ) {
            return find_camp_request( request_id );
        },
        [this]( const int request_id ) {
            approve_crafting_request( request_id );
        } );
        add_camp_request_bark( listener,
                               retrying_blocked ?
                               string_format( _( "Retried the blocked pile: started %1$d, still blocked %2$d, pinned %3$d." ),
                                       static_cast<int>( launch_result.started.size() ),
                                       static_cast<int>( launch_result.blocked.size() ),
                                       static_cast<int>( launch_result.pinned.size() ) ) :
                               string_format( _( "Worked through the board: started %1$d, blocked %2$d, still pinned %3$d." ),
                                       static_cast<int>( launch_result.started.size() ),
                                       static_cast<int>( launch_result.blocked.size() ),
                                       static_cast<int>( launch_result.pinned.size() ) ) );
        return true;
    };

    const auto handle_clear_archived = [&]() {
        const size_t before = camp_requests.size();
        camp_requests.erase( std::remove_if( camp_requests.begin(), camp_requests.end(),
        []( const camp_llm_request &request ) {
            return request.status == "completed" || request.status == "cancelled";
        } ), camp_requests.end() );
        const int cleared = static_cast<int>( before - camp_requests.size() );
        if( cleared > 0 ) {
            add_camp_request_bark( listener,
                                   string_format( _( "Cleared %d archived crafting requests." ), cleared ) );
        } else {
            add_camp_request_bark( listener, _( "No archived crafting requests to clear." ) );
        }
        return true;
    };

    if( const std::optional<basecamp_ai::parsed_camp_job_token> structured_job =
            basecamp_ai::parse_structured_camp_job_token( utterance ) ) {
        if( structured_job->kind == basecamp_ai::camp_job_token_kind::show_board ) {
            add_camp_request_bark( listener, basecamp_ai::camp_board_handoff_snapshot( camp_requests ) );
            return true;
        }
        if( structured_job->kind == basecamp_ai::camp_job_token_kind::launch_ready_jobs ) {
            return handle_batch_launch( false );
        }
        if( structured_job->kind == basecamp_ai::camp_job_token_kind::retry_blocked_jobs ) {
            return handle_batch_launch( true );
        }
        if( structured_job->kind == basecamp_ai::camp_job_token_kind::clear_archived_jobs ) {
            return handle_clear_archived();
        }

        camp_llm_request *request = find_camp_request( structured_job->request_id );
        if( request == nullptr ) {
            add_camp_request_bark( listener,
                                   string_format( "missing_job=%d", structured_job->request_id ) );
            return true;
        }

        if( structured_job->kind == basecamp_ai::camp_job_token_kind::show_job ) {
            add_camp_request_bark( listener, basecamp_ai::camp_request_handoff_snapshot( *request ) );
            return true;
        }

        if( structured_job->kind == basecamp_ai::camp_job_token_kind::delete_job ) {
            if( camp_request_is_archived( *request ) ) {
                const std::string bark = camp_request_clear_bark( *request );
                clear_camp_request( request->request_id );
                add_camp_request_bark( listener, bark );
            } else {
                add_camp_request_bark( listener, camp_request_not_archived_bark( *request ) );
            }
            return true;
        }

        if( camp_request_is_archived( *request ) ) {
            add_camp_request_bark( listener, camp_request_archived_bark( *request ) );
            return true;
        }

        if( request->status == "in_progress" ) {
            add_camp_request_bark( listener, camp_request_spoken_status( *request ) );
            return true;
        }

        const int request_id = request->request_id;
        const std::string subject = camp_request_bark_subject( *request );
        approve_crafting_request( request_id );
        request = find_camp_request( request_id );
        if( request == nullptr ) {
            return true;
        }

        if( request->status == "in_progress" ) {
            add_camp_request_bark( listener, camp_request_launch_bark( *request ) );
        } else if( request->status == "blocked" ) {
            add_camp_request_bark( listener, camp_request_blocked_bark( *request ) );
        } else {
            add_camp_request_bark( listener,
                                   string_format( _( "Leaving %s pinned for now." ), subject ) );
        }
        return true;
    }

    if( const std::optional<basecamp_ai::parsed_camp_craft_order> structured_craft =
            basecamp_ai::parse_structured_camp_craft_order( utterance ) ) {
        return handle_craft_order( *structured_craft );
    }

    if( const std::optional<basecamp_ai::parsed_camp_request_reference> status_query =
            basecamp_ai::parse_heard_camp_status_query( utterance ) ) {
        if( camp_requests.empty() ) {
            add_camp_request_bark( listener, _( "Board's empty." ) );
            return true;
        }
        if( status_query->all_requests ) {
            std::vector<std::string> active_subjects;
            active_subjects.reserve( camp_requests.size() );
            int archived_requests = 0;
            for( const camp_llm_request &request : camp_requests ) {
                if( request.status == "completed" || request.status == "cancelled" ) {
                    archived_requests++;
                } else {
                    active_subjects.push_back( camp_request_subject( request ) );
                }
            }
            if( active_subjects.empty() ) {
                add_camp_request_bark( listener,
                                       string_format( _( "Board says nothing active, %d archived." ),
                                               archived_requests ) );
            } else {
                add_camp_request_bark( listener,
                                       string_format( _( "Board lists %1$d active, %2$d archived — %3$s." ),
                                               static_cast<int>( active_subjects.size() ), archived_requests,
                                               camp_request_subject_list( active_subjects ) ) );
            }
            return true;
        }

        const basecamp_ai::camp_request_match_result lookup =
            basecamp_ai::match_camp_request_reference( camp_requests, *status_query,
        []( const camp_llm_request & ) {
            return true;
        } );
        camp_llm_request *matched_request = lookup.found ? find_camp_request( lookup.request_id ) : nullptr;
        if( matched_request != nullptr ) {
            add_camp_request_bark( listener, camp_request_spoken_status( *matched_request ) );
        } else if( !lookup.ambiguous_matches.empty() ) {
            add_camp_request_bark( listener,
                                   camp_request_ambiguous_bark( status_query->query, lookup.ambiguous_matches ) );
        } else if( status_query->has_request_id ) {
            add_camp_request_bark( listener,
                                   string_format( _( "No crafting request #%d on the board." ),
                                           status_query->request_id ) );
        } else {
            add_camp_request_bark( listener, camp_request_not_found_bark( status_query->query ) );
        }
        return true;
    }

    if( const std::optional<basecamp_ai::parsed_camp_request_reference> clear_query =
            basecamp_ai::parse_heard_camp_clear_query( utterance ) ) {
        if( clear_query->all_requests ) {
            return handle_clear_archived();
        }

        const basecamp_ai::camp_request_match_result archived_lookup =
            basecamp_ai::match_camp_request_reference( camp_requests, *clear_query,
        []( const camp_llm_request &request ) {
            return request.status == "completed" || request.status == "cancelled";
        } );
        if( archived_lookup.found ) {
            camp_llm_request *request = find_camp_request( archived_lookup.request_id );
            if( request != nullptr ) {
                const std::string bark = camp_request_clear_bark( *request );
                clear_camp_request( request->request_id );
                add_camp_request_bark( listener, bark );
            }
            return true;
        }
        if( !archived_lookup.ambiguous_matches.empty() ) {
            add_camp_request_bark( listener,
                                   camp_request_ambiguous_bark( clear_query->query, archived_lookup.ambiguous_matches ) );
            return true;
        }

        if( clear_query->has_request_id ) {
            if( camp_llm_request *request = find_camp_request( clear_query->request_id ) ) {
                add_camp_request_bark( listener, camp_request_not_archived_bark( *request ) );
            } else {
                add_camp_request_bark( listener,
                                       string_format( _( "No crafting request #%d on the board." ),
                                               clear_query->request_id ) );
            }
            return true;
        }

        const basecamp_ai::camp_request_match_result live_lookup =
            basecamp_ai::match_camp_request_reference( camp_requests, *clear_query,
        []( const camp_llm_request &request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        if( live_lookup.found ) {
            if( camp_llm_request *request = find_camp_request( live_lookup.request_id ) ) {
                add_camp_request_bark( listener, camp_request_not_archived_bark( *request ) );
            }
        } else if( !live_lookup.ambiguous_matches.empty() ) {
            add_camp_request_bark( listener,
                                   camp_request_ambiguous_bark( clear_query->query, live_lookup.ambiguous_matches ) );
        } else {
            add_camp_request_bark( listener, camp_request_not_found_bark( clear_query->query ) );
        }
        return true;
    }

    if( const std::optional<basecamp_ai::parsed_camp_request_reference> cancel_query =
            basecamp_ai::parse_heard_camp_cancel_query( utterance ) ) {
        const basecamp_ai::camp_request_match_result lookup =
            basecamp_ai::match_camp_request_reference( camp_requests, *cancel_query,
        []( const camp_llm_request &request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        camp_llm_request *best_request = lookup.found ? find_camp_request( lookup.request_id ) : nullptr;
        if( best_request != nullptr ) {
            if( best_request->status == "in_progress" ) {
                recall_crafting_request_worker( *this, *best_request );
            } else {
                best_request->status = "cancelled";
                best_request->approval_state = "rejected";
                best_request->active_mission_id = mission_id();
                best_request->eta_turn = calendar::turn_zero;
                best_request->blockers.clear();
                add_camp_request_note( *best_request, "blocked",
                                       _( "Cancelled after a spoken order from camp staff." ) );
            }
            add_camp_request_bark( listener, camp_request_cancel_bark( *best_request ) );
        } else if( !lookup.ambiguous_matches.empty() ) {
            add_camp_request_bark( listener,
                                   camp_request_ambiguous_bark( cancel_query->query, lookup.ambiguous_matches ) );
        } else if( cancel_query->has_request_id ) {
            add_camp_request_bark( listener,
                                   string_format( _( "No live crafting request #%d." ), cancel_query->request_id ) );
        } else {
            add_camp_request_bark( listener, camp_request_not_found_bark( cancel_query->query ) );
        }
        return true;
    }

    if( const std::optional<basecamp_ai::parsed_camp_request_reference> approval_query =
            basecamp_ai::parse_heard_camp_approval_query( utterance ) ) {
        if( approval_query->all_requests || approval_query->all_blocked_requests ) {
            return handle_batch_launch( approval_query->all_blocked_requests );
        }

        const basecamp_ai::camp_request_match_result lookup =
            basecamp_ai::match_camp_request_reference( camp_requests, *approval_query,
        []( const camp_llm_request &request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        if( !lookup.found ) {
            if( !lookup.ambiguous_matches.empty() ) {
                add_camp_request_bark( listener,
                                       camp_request_ambiguous_bark( approval_query->query, lookup.ambiguous_matches ) );
            } else if( approval_query->has_request_id ) {
                add_camp_request_bark( listener,
                                       string_format( _( "No crafting request #%d on the board." ),
                                               approval_query->request_id ) );
            } else {
                add_camp_request_bark( listener, camp_request_not_found_bark( approval_query->query ) );
            }
            return true;
        }
        camp_llm_request *best_request = find_camp_request( lookup.request_id );
        if( best_request == nullptr ) {
            return true;
        }
        if( best_request->status == "in_progress" ) {
            add_camp_request_bark( listener,
                                   string_format( _( "%s is already underway." ),
                                           camp_request_bark_subject( *best_request ) ) );
            return true;
        }

        const int request_id = best_request->request_id;
        const std::string subject = camp_request_bark_subject( *best_request );
        approve_crafting_request( request_id );
        best_request = find_camp_request( request_id );
        if( best_request == nullptr ) {
            return true;
        }

        if( best_request->status == "in_progress" ) {
            add_camp_request_bark( listener, camp_request_launch_bark( *best_request ) );
        } else if( best_request->status == "blocked" ) {
            add_camp_request_bark( listener, camp_request_blocked_bark( *best_request ) );
        } else {
            add_camp_request_bark( listener,
                                   string_format( _( "Leaving %s pinned for now." ), subject ) );
        }
        return true;
    }

    const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
        basecamp_ai::parse_heard_camp_craft_order( utterance );
    if( !parsed ) {
        return false;
    }

    return handle_craft_order( *parsed );
}

bool basecamp::approve_crafting_request( int request_id )
{
    camp_llm_request *request = find_camp_request( request_id );
    if( request == nullptr ) {
        return false;
    }
    if( request->status == "in_progress" || request->status == "completed" ||
        request->status == "cancelled" ) {
        return false;
    }
    if( request->chosen_recipe_id.is_empty() || !request->chosen_recipe_id.is_valid() ) {
        request->status = "blocked";
        request->blockers = { _( "Chosen recipe is no longer valid." ) };
        add_camp_request_note( *request, "blocked", _( "The crafting request no longer points at a valid recipe." ) );
        return false;
    }

    const recipe &making = *request->chosen_recipe_id;
    const int batch_size = std::max( request->requested_count, 1 );

    std::string worker_resolution_note;
    std::vector<std::string> worker_blockers;
    npc_ptr guy_to_send = resolve_crafting_worker( making, batch_size,
                          request->assigned_worker_npc_id, request->assigned_worker_name,
                          &worker_resolution_note, &worker_blockers );
    if( guy_to_send == nullptr ) {
        request->status = "blocked";
        request->blockers = !worker_blockers.empty() ? worker_blockers :
                            std::vector<std::string>{ _( "No stationed worker can take this recipe right now." ) };
        if( !request->assigned_worker_name.empty() ) {
            add_camp_request_note( *request, "blocked",
                                   string_format( _( "%s could not take the order and nobody else was ready, so it stays pinned." ),
                                                  request->assigned_worker_name ) );
        } else {
            add_camp_request_note( *request, "blocked",
                                   _( "Nobody at camp could take that order right now, so it stays pinned." ) );
        }
        return false;
    }

    request->status = "awaiting_approval";
    request->blockers.clear();
    if( !worker_resolution_note.empty() ) {
        add_camp_request_note( *request, "plan", worker_resolution_note );
    }
    validate_sort_points();
    form_storage_zones( get_camp_map(), bb_pos );
    form_crafting_inventory( get_camp_map() );

    if( making.result()->phase != phase_id::SOLID && get_liquid_dumping_spot().empty() ) {
        std::string query_msg = string_format(
                                    _( "You don't have anything in which to store %s and may have to pour it out as soon as it is prepared!  Proceed?" ),
                                    making.result()->nname( batch_size ) );
        query_msg += "\n\n";
        query_msg +=
            _( "Eligible locations must be a terrain OR furniture (not item) that can contain liquid, and does not already have any items on its tile." );
        if( !query_yn( query_msg ) ) {
            add_camp_request_note( *request, "plan",
                                   _( "You left the order pinned until there is somewhere sensible to dump the liquid." ) );
            return false;
        }
    }

    mapgen_arguments arg;
    std::unique_ptr<basecamp_action_components> components =
        std::make_unique<basecamp_action_components>( making, arg, batch_size, *this );
    bool have_components = components->choose_components();
    std::vector<std::string> released_tool_notes;
    if( !have_components ) {
        released_tool_notes = release_crafting_tools( making, arg, batch_size );
        if( !released_tool_notes.empty() ) {
            components = std::make_unique<basecamp_action_components>( making, arg, batch_size, *this );
            have_components = components->choose_components();
        }
    }
    for( const std::string &note : released_tool_notes ) {
        add_camp_request_note( *request, "plan", note );
    }
    if( !have_components ) {
        request->status = "blocked";
        if( request->blockers.empty() ) {
            request->blockers = { _( "Camp storage could not supply the needed ingredients or tools." ) };
        }
        add_camp_request_note( *request, "blocked",
                               _( "The camp could not gather everything needed, so the crafting request stayed pinned." ) );
        return false;
    }

    time_duration work_days = base_camps::to_workdays( making.batch_duration( *guy_to_send,
                              batch_size ) );
    const int kcal_consumed = time_to_food( work_days, making.exertion_level() );
    const int kcal_have = fac()->food_supply().kcal();

    if( !query_yn( _( "This will cost %i kcal (you have %i stored), is that acceptable?" ),
                   kcal_consumed, kcal_have ) ) {
        add_camp_request_note( *request, "plan",
                               _( "You decided not to spend the camp rations on that order yet." ) );
        return false;
    }

    if( kcal_consumed > kcal_have ) {
        request->status = "blocked";
        request->blockers = { _( "Not enough stored food." ) };
        add_camp_request_note( *request, "blocked",
                               _( "The pantry is too thin to cover that job right now." ) );
        return false;
    }

    making.apply_all_morale_mods( *guy_to_send );

    const mission_id actual_id = { Camp_Crafting, making.ident().str(), {}, base_camps::base_dir };
    npc_ptr comp = start_mission( actual_id, work_days, true,
                                  _( "begins to work…" ), false, {}, making.required_skills,
                                  making.exertion_level(), 0_hours, guy_to_send );
    if( comp == nullptr ) {
        request->status = "blocked";
        request->blockers = { _( "Could not start the assigned craft mission." ) };
        add_camp_request_note( *request, "blocked",
                               _( "Tried to kick off the job, but the camp machinery still said no." ) );
        return false;
    }

    components->consume_components();
    item_components used = components->consumed_components();
    for( const item &results : making.create_results( batch_size, &used ) ) {
        comp->companion_mission_inv.add_item( results );
    }
    for( const item &byproducts : making.create_byproducts( batch_size ) ) {
        comp->companion_mission_inv.add_item( byproducts );
    }

    request->status = "in_progress";
    request->approval_state = "approved";
    request->active_mission_id = actual_id;
    request->assigned_worker_npc_id = comp->getID();
    request->assigned_worker_name = comp->disp_name();
    request->eta_turn = comp->companion_mission_time_ret;
    request->requested_count = batch_size;
    request->updated_turn = calendar::turn;
    request->blockers.clear();
    add_camp_request_note( *request, "plan",
                           string_format( _( "%1$s pulled request #%2$d off the board and started crafting %3$d × %4$s." ),
                                          comp->disp_name(), request->request_id,
                                          batch_size, making.result_name() ) );
    return true;
}

bool basecamp::complete_crafting_request( const mission_id &miss_id, const npc &worker,
        const std::string &note_text )
{
    camp_llm_request *request = find_active_crafting_request( miss_id, worker.getID() );
    if( request == nullptr ) {
        return false;
    }
    request->status = "completed";
    request->eta_turn = calendar::turn_zero;
    request->active_mission_id = mission_id();
    add_camp_request_note( *request, "completion",
                           note_text.empty() ? string_format( _( "%s returned with the finished job." ),
                                                   worker.disp_name() ) : note_text );
    return true;
}

bool basecamp::cancel_crafting_request( const mission_id &miss_id, const npc &worker,
                                        const std::string &note_text )
{
    camp_llm_request *request = find_active_crafting_request( miss_id, worker.getID() );
    if( request == nullptr ) {
        return false;
    }
    request->status = "cancelled";
    request->eta_turn = calendar::turn_zero;
    request->active_mission_id = mission_id();
    add_camp_request_note( *request, "blocked",
                           note_text.empty() ? string_format( _( "%s was recalled before finishing the job." ),
                                                   worker.disp_name() ) : note_text );
    return true;
}

bool basecamp::clear_camp_request( int request_id )
{
    auto it = std::remove_if( camp_requests.begin(), camp_requests.end(), [request_id]( const camp_llm_request & request ) {
        return request.request_id == request_id;
    } );
    if( it == camp_requests.end() ) {
        return false;
    }
    camp_requests.erase( it, camp_requests.end() );
    return true;
}

void basecamp::camp_request_ui()
{
    if( camp_requests.empty() ) {
        popup( _( "The bulletin-board scratchpad is empty." ) );
        return;
    }

    while( true ) {
        uilist menu;
        menu.title = _( "Crafting Requests" );
        menu.text = _( "Inspect the board, review crafting details, retry blocked work, recall an active crafter, or clear archived requests." );

        std::vector<int> request_ids;
        request_ids.reserve( camp_requests.size() );
        int entry_index = 0;
        bool has_archived_requests = false;
        bool has_launchable_requests = false;
        bool has_blocked_requests = false;
        for( const camp_llm_request &request : camp_requests ) {
            request_ids.push_back( request.request_id );
            menu.addentry( entry_index++, true, MENU_AUTOASSIGN, camp_request_summary( request ) );
            if( request.status == "completed" || request.status == "cancelled" ) {
                has_archived_requests = true;
            }
            if( request.status == "awaiting_approval" ) {
                has_launchable_requests = true;
            }
            if( request.status == "blocked" ) {
                has_blocked_requests = true;
            }
        }

        const int launch_ready_entry = entry_index++;
        menu.addentry( launch_ready_entry, has_launchable_requests, 'a', _( "Start all ready crafting requests" ) );
        const int retry_blocked_entry = entry_index++;
        menu.addentry( retry_blocked_entry, has_blocked_requests, 'r', _( "Retry all blocked crafting requests" ) );
        const int clear_archived_entry = entry_index;
        menu.addentry( clear_archived_entry, has_archived_requests, 'c', _( "Clear archived crafting requests" ) );
        menu.query();

        if( menu.ret < 0 ) {
            return;
        }

        if( menu.ret == launch_ready_entry || menu.ret == retry_blocked_entry ) {
            const std::vector<int> launch_ids = menu.ret == retry_blocked_entry ?
                                                basecamp_ai::collect_blocked_camp_request_ids( camp_requests ) :
                                                basecamp_ai::collect_ready_camp_request_ids( camp_requests );
            const camp_request_launch_result launch_result = attempt_camp_request_launches( launch_ids,
                                                           [this]( const int request_id ) {
                return find_camp_request( request_id );
            },
            [this]( const int request_id ) {
                approve_crafting_request( request_id );
            } );
            popup( camp_request_launch_feedback( launch_result.started,
                                                 launch_result.blocked,
                                                 launch_result.pinned ) );
            if( camp_requests.empty() ) {
                popup( _( "The board is empty again." ) );
                return;
            }
            continue;
        }

        if( menu.ret == clear_archived_entry ) {
            for( const int request_id : basecamp_ai::collect_archived_camp_request_ids( camp_requests ) ) {
                clear_camp_request( request_id );
            }
            if( camp_requests.empty() ) {
                popup( _( "The board is empty again." ) );
                return;
            }
            continue;
        }

        if( static_cast<size_t>( menu.ret ) >= request_ids.size() ) {
            continue;
        }

        const int request_id = request_ids[menu.ret];
        while( true ) {
            camp_llm_request *request = find_camp_request( request_id );
            if( request == nullptr ) {
                break;
            }

            const bool archived = request->status == "completed" || request->status == "cancelled";
            const bool can_recall = request->status == "in_progress" &&
                                    request->active_mission_id.id == Camp_Crafting;
            const bool can_approve = request->status == "awaiting_approval" || request->status == "blocked";
            const bool can_cancel_pending = can_approve;
            const std::string approve_label = request->status == "blocked" ?
                                              _( "Retry and start crafting request" ) :
                                              _( "Start crafting request" );

            uilist action_menu;
            action_menu.title = camp_request_summary( *request );
            action_menu.text = camp_request_details( *request );
            action_menu.addentry( 0, true, 'b', _( "Back" ) );
            action_menu.addentry( 1, can_approve, 'a', approve_label );
            action_menu.addentry( 2, can_recall, 'r', _( "Emergency recall crafter" ) );
            action_menu.addentry( 3, can_cancel_pending, 'c', _( "Cancel this crafting request" ) );
            action_menu.addentry( 4, archived, 'x', _( "Clear this crafting request" ) );
            action_menu.query();

            if( action_menu.ret <= 0 ) {
                break;
            }

            if( action_menu.ret == 1 ) {
                approve_crafting_request( request_id );
                break;
            }

            if( action_menu.ret == 2 ) {
                if( query_yn( _( "Recall %s and cancel this crafting request?  Spent materials stay spent." ),
                              request->assigned_worker_name ) ) {
                    recall_crafting_request_worker( *this, *request );
                }
                break;
            }

            if( action_menu.ret == 3 ) {
                request->status = "cancelled";
                request->approval_state = "rejected";
                request->active_mission_id = mission_id();
                request->eta_turn = calendar::turn_zero;
                request->blockers.clear();
                add_camp_request_note( *request, "blocked",
                                       _( "You cancelled the queued crafting request before it left the board." ) );
                break;
            }

            if( action_menu.ret == 4 ) {
                clear_camp_request( request_id );
                break;
            }
        }

        if( camp_requests.empty() ) {
            popup( _( "The board is empty again." ) );
            return;
        }
    }
}

bool basecamp::handle_mission( const ui_mission_id &miss_id )
{
    if( miss_id.id.id == No_Mission ) {
        return true;
    }

    const point_rel_omt &miss_dir = miss_id.id.dir.value();
    //  All missions should supply dir. Bug if they don't, so blow up during testing.

    switch( miss_id.id.id ) {
        case Camp_Distribute_Food:
            distribute_food();
            break;

        case Camp_Determine_Leadership:
            choose_new_leader();
            break;

        case Camp_Have_Meal:
            player_eats_meal();
            break;

        case Camp_Requests:
            camp_request_ui();
            break;

        case Camp_Hide_Mission:
            handle_hide_mission( miss_id.id.dir.value() );
            break;

        case Camp_Reveal_Mission:
            handle_reveal_mission( miss_id.id.dir.value() );
            break;

        case Camp_Assign_Jobs:
            job_assignment_ui();
            break;

        case Camp_Assign_Workers:
            worker_assignment_ui();
            break;

        case Camp_Abandon:
            abandon_camp();
            break;

        case Camp_Upgrade:
            if( miss_id.ret ) {
                upgrade_return( miss_id.id );
            } else {
                start_upgrade( miss_id.id );
            }
            break;

        case Camp_Emergency_Recall:
            emergency_recall( miss_id.id );
            break;

        case Camp_Crafting:
            if( miss_id.ret ) {
                const std::string bldg = recipe_group::get_building_of_recipe( miss_id.id.parameters );

                std::string msg;

                if( bldg == base_recipe_group_string ) {
                    msg = _( "returns to you with something…" );

                } else if( bldg == cook_recipe_group_string ) {
                    msg = _( "returns from your kitchen with something…" );

                } else if( bldg == farm_recipe_group_string ) {
                    msg = _( "returns from your farm with something…" );

                } else if( bldg == smith_recipe_group_string ) {
                    msg = _( "returns from your blacksmith shop with something…" );
                }

                else {
                    msg = _( "returns to you with something…" );
                }

                crafting_mission_return( miss_id.id,
                                         msg,
                                         skill_construction.str(), 2 );
            } else {
                start_crafting( miss_id.id );
            }
            break;

        case Camp_Gather_Materials:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 3_hours );
            } else {
                start_mission( miss_id.id, 3_hours, true,
                               _( "departs to search for materials…" ), false, {}, skill_survival, 0, LIGHT_EXERCISE );
            }
            break;

        case Camp_Collect_Firewood:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 3_hours );
            } else {
                start_mission( miss_id.id, 3_hours, true,
                               _( "departs to search for firewood…" ), false, {}, skill_survival, 0, LIGHT_EXERCISE );
            }
            break;

        case Camp_Menial:
            if( miss_id.ret ) {
                menial_return( miss_id.id );
            } else {
                start_menial_labor();
            }
            break;

        case Camp_Survey_Field:
            if( miss_id.ret ) {
                survey_field_return( miss_id.id );
            } else {
                start_mission( miss_id.id, 0_hours, true,
                               _( "departs to look for suitable fields…" ), false, {}, skill_gun, 0, MODERATE_EXERCISE );
            }
            break;

        case Camp_Survey_Expansion:
            if( miss_id.ret ) {
                survey_return( miss_id.id );
            } else {
                start_mission( miss_id.id, 3_hours, true,
                               _( "departs to survey land…" ), false, {}, skill_gun, 0, MODERATE_EXERCISE );
            }
            break;

        case Camp_Cut_Logs:
            if( miss_id.ret ) {
                mission_return( miss_id.id, 6_hours, true,
                                _( "returns from working in the woods…" ),
                                skill_construction.str(), 2 );
            } else {
                start_cut_logs( miss_id.id, ACTIVE_EXERCISE );
            }
            break;

        case Camp_Clearcut:
            if( miss_id.ret ) {
                mission_return( miss_id.id, 6_hours, true,
                                _( "returns from working in the woods…" ),
                                skill_construction.str(), 1 );
            } else {
                start_clearcut( miss_id.id, ACTIVE_EXERCISE );
            }
            break;

        case Camp_Setup_Hide_Site:
            if( miss_id.ret ) {
                mission_return( miss_id.id, 3_hours, true,
                                _( "returns from working on the hide site…" ), skill_survival.str(), 3 );
            } else {
                start_setup_hide_site( miss_id.id, LIGHT_EXERCISE );
            }
            break;

        case Camp_Relay_Hide_Site:
            if( miss_id.ret ) {
                const std::string msg = _( "returns from shuttling gear between the hide site…" );
                mission_return( miss_id.id, 3_hours, true,
                                msg, skill_survival.str(), 3 );
            } else {
                start_relay_hide_site( miss_id.id, LIGHT_EXERCISE );
            }
            break;

        case Camp_Foraging:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 4_hours );
            } else {
                start_mission( miss_id.id, 4_hours, true,
                               _( "departs to search for edible plants…" ), false, {}, skill_survival, 0, LIGHT_EXERCISE );
            }
            break;

        case Camp_Trapping:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 6_hours );
            } else {
                start_mission( miss_id.id, 6_hours, true,
                               _( "departs to set traps for small animals…" ), false, {}, skill_traps, 0, LIGHT_EXERCISE );
            }
            break;

        case Camp_Hunting:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 6_hours );
            } else {
                start_mission( miss_id.id, 6_hours, true,
                               _( "departs to hunt for meat…" ), false, {}, skill_gun, 0, MODERATE_EXERCISE );
            }
            break;

        case Camp_OM_Fortifications:
            if( miss_id.ret ) {
                fortifications_return( miss_id.id );
            } else {
                std::string bldg_exp = miss_id.id.parameters;
                start_fortifications( miss_id.id, ACTIVE_EXERCISE );
            }
            break;

        case Camp_Recruiting:
            if( miss_id.ret ) {
                recruit_return( miss_id.id,
                                recruit_evaluation() );
            } else {
                start_mission( miss_id.id, 4_days, true,
                               _( "departs to search for recruits…" ), false, {}, skill_gun, 0, MODERATE_EXERCISE );
            }
            break;

        case Camp_Scouting:
            if( miss_id.ret ) {
                combat_mission_return( miss_id.id );
            } else {
                start_combat_mission( miss_id.id, MODERATE_EXERCISE );
            }
            break;
        case Camp_Combat_Patrol:
            if( miss_id.ret ) {
                combat_mission_return( miss_id.id );
            } else {
                start_combat_mission( miss_id.id, BRISK_EXERCISE );
            }
            break;

        case Camp_Plow:
        case Camp_Plant:
        case Camp_Harvest:
            if( miss_id.ret ) {
                farm_return( miss_id.id, miss_dir );
            } else {
                start_farm_op( miss_dir, miss_id.id, MODERATE_EXERCISE );
            }
            break;

        default:
            break;
    }

    return true;
}

// camp faction companion mission start functions
npc_ptr basecamp::start_mission( const mission_id &miss_id, time_duration total_time,
                                 bool must_feed, const std::string &desc, bool /*group*/,
                                 const std::vector<item *> &equipment,
                                 const skill_id &skill_tested, int skill_level,
                                 float exertion_level, const time_duration &travel_time )
{
    std::map<skill_id, int> required_skills;
    required_skills[ skill_tested ] = skill_level;
    return start_mission( miss_id, total_time, must_feed, desc, false, equipment,
                          required_skills, exertion_level, travel_time );
}

npc_ptr basecamp::start_mission( const mission_id &miss_id, time_duration total_time,
                                 bool must_feed, const std::string &desc, bool /*group*/,
                                 const std::vector<item *> &equipment, const std::map<skill_id, int> &required_skills,
                                 float exertion_level, const time_duration &travel_time, const npc_ptr &preselected_choice )
{
    if( must_feed &&
        fac()->food_supply().kcal() < time_to_food( total_time, exertion_level, travel_time ) ) {
        popup( _( "You don't have enough food stored to feed your companion." ) );
        return nullptr;
    }
    npc_ptr comp = talk_function::individual_mission( omt_pos, base_camps::id, desc, miss_id,
                   false, equipment, required_skills, false, preselected_choice );
    if( comp != nullptr ) {
        comp->companion_mission_time_ret = calendar::turn + total_time;
        comp->companion_mission_exertion = exertion_level;
        if( must_feed ) {
            // TODO update spent food after assigning a specific worker to the mission and recalculating travel time
            feed_workers( *comp.get()->as_character(), camp_food_supply( total_time, exertion_level,
                          travel_time ) );
        }
        if( !equipment.empty() ) {
            map &target_map = get_camp_map();
            std::vector<tripoint_bub_ms> src_set_pt;
            src_set_pt.resize( src_set.size() );
            for( const tripoint_abs_ms &p : src_set ) {
                src_set_pt.emplace_back( target_map.get_bub( p ) );
            }
            for( item *i : equipment ) {
                int count = i->count();
                if( i->count_by_charges() ) {
                    target_map.use_charges( src_set_pt, i->typeId(), count );
                } else {
                    target_map.use_amount( src_set_pt, i->typeId(), count );
                }
            }
            target_map.save();
        }
    }
    return comp;
}

comp_list basecamp::start_multi_mission( const mission_id &miss_id,
        bool must_feed, const std::string &desc,
        // const std::vector<item*>& equipment, //  No support for extracting equipment from recipes currently..
        const skill_id &skill_tested, int skill_level )
{
    std::map<skill_id, int> required_skills;
    required_skills[skill_tested] = skill_level;
    return start_multi_mission( miss_id, must_feed, desc, required_skills );
}

comp_list basecamp::start_multi_mission( const mission_id &miss_id,
        bool must_feed, const std::string &desc,
        // const std::vector<item*>& equipment, //  No support for extracting equipment from recipes currently..
        const std::map<skill_id, int> &required_skills )
{
    const recipe &making = *recipe_id( miss_id.parameters );
    auto req_it = making.blueprint_build_reqs().reqs_by_parameters.find( miss_id.mapgen_args );
    cata_assert( req_it != making.blueprint_build_reqs().reqs_by_parameters.end() );
    const build_reqs &bld_reqs = req_it->second;
    time_duration base_time = time_duration::from_moves( bld_reqs.time );

    comp_list result;

    time_duration work_days;

    while( true ) { //  We'll break out of the loop when all the workers have been assigned
        work_days = base_camps::to_workdays( base_time / ( result.size() + 1 ) );

        if( must_feed &&
            fac()->food_supply().kcal() < time_to_food( work_days * ( result.size() + 1 ),
                    making.exertion_level() ) ) {
            if( result.empty() ) {
                popup( _( "You don't have enough food stored to feed your companion for this task." ) );
                return result;
            } else {
                popup( _( "You don't have enough food stored to feed a larger work crew." ) );
                break;
            }
        }

        //  We should allocate the full set of tools to each companion sent off on a mission,
        //  but currently recipes only provides a function to check for the full set of requirement,
        //  with no access to the tool subset, so no tools are actually assigned.
        npc_ptr comp = talk_function::individual_mission( omt_pos, base_camps::id, desc, miss_id,
                       false, {}, required_skills, !result.empty() );

        if( comp == nullptr ) {
            break;
        } else {
            result.push_back( comp );
        }
    }

    if( result.empty() ) {
        return result;
    } else {
        work_days = base_camps::to_workdays( base_time / result.size() );

        for( npc_ptr &comp : result ) {
            comp->companion_mission_time_ret = calendar::turn + work_days;
            comp->companion_mission_exertion = making.exertion_level();
        }
        if( must_feed ) {
            std::vector<std::reference_wrapper <Character>> work_party;
            for( npc_ptr &comp : result ) {
                work_party.emplace_back( *comp.get()->as_character() );
            }
            feed_workers( work_party, camp_food_supply( work_days * result.size(), making.exertion_level() ) );
        }
        return result;
    }
}

void basecamp::start_upgrade( const mission_id &miss_id )
{
    const recipe &making = *recipe_id( miss_id.parameters );
    if( making.get_blueprint() == update_mapgen_faction_expansion_salt_water_pipe_swamp_N ) {
        start_salt_water_pipe( miss_id );
        return;
    } else if( making.get_blueprint() == update_mapgen_faction_expansion_salt_water_pipe_N ) {
        continue_salt_water_pipe( miss_id );
        return;
    }

    auto req_it = making.blueprint_build_reqs().reqs_by_parameters.find( miss_id.mapgen_args );
    cata_assert( req_it != making.blueprint_build_reqs().reqs_by_parameters.end() );
    const build_reqs &bld_reqs = req_it->second;
    const requirement_data &reqs = bld_reqs.consolidated_reqs;

    //Stop upgrade if you don't have materials
    if( reqs.can_make_with_inventory( _inv, making.get_component_filter(), 1, craft_flags::none,
                                      false ) ) {
        bool must_feed = !making.has_flag( "NO_FOOD_REQ" );

        basecamp_action_components components( making, miss_id.mapgen_args, 1, *this );
        if( !components.choose_components() ) {
            return;
        }

        comp_list comp;
        if( bld_reqs.skills.empty() ) {
            if( making.skill_used.is_valid() ) {
                comp = start_multi_mission( miss_id, must_feed,
                                            _( "begins to upgrade the camp…" ),
                                            making.skill_used, making.difficulty );
            } else {
                comp = start_multi_mission( miss_id, must_feed,
                                            _( "begins to upgrade the camp…" ) );
            }
        } else {
            comp = start_multi_mission( miss_id, must_feed, _( "begins to upgrade the camp…" ),
                                        bld_reqs.skills );
        }
        if( comp.empty() ) {
            return;
        }
        components.consume_components();
        const point_rel_omt dir = miss_id.dir.value();  //  Will always have a value

        update_in_progress( miss_id.parameters,
                            dir );

        bool mirror_horizontal;
        bool mirror_vertical;
        int rotation;

        auto e = expansions.find( dir );
        if( e == expansions.end() ) {
            return;
        }
        const tripoint_abs_omt upos = e->second.pos;

        extract_and_check_orientation_flags( making.ident(),
                                             dir,
                                             mirror_horizontal,
                                             mirror_vertical,
                                             rotation,
                                             "%s failed to build the %s upgrade",
                                             "" );

        apply_construction_marker( making.get_blueprint(), upos,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, true );
    } else {
        popup( _( "You don't have the materials for the upgrade." ) );
    }
}

void basecamp::remove_camp( bool remove_from_overmap ) const
{
    std::set<tripoint_abs_omt> &known_camps = get_player_character().camps;
    known_camps.erase( omt_pos );

    if( remove_from_overmap ) {
        overmap_buffer.remove_camp( omt_pos.xy() );
    }

    map &here = get_map();
    tripoint_bub_ms pos_bub;
    // bb_pos may be {0,0,0} if you haven't examined the bulletin board on camp ever
    if( bb_pos != tripoint_abs_ms::zero ) {
        pos_bub = here.get_bub( bb_pos );
    } else {
        const tripoint_abs_sm sm_pos = coords::project_to<coords::sm>( omt_pos );
        const tripoint_abs_ms ms_pos = coords::project_to<coords::ms>( sm_pos );
        pos_bub = here.get_bub( ms_pos );
    }
    if( here.inbounds( pos_bub ) ) {
        here.remove_submap_camp( pos_bub );
    }
}

void basecamp::abandon_camp()
{
    validate_assignees();
    for( npc_ptr &guy : overmap_buffer.get_companion_mission_npcs( 10 ) ) {
        npc_companion_mission c_mission = guy->get_companion_mission();
        if( c_mission.role_id != base_camps::id ) {
            continue;
        }
        const std::string return_msg = _( "responds to the emergency recall…" );
        finish_return( *guy, false, return_msg, skill_menial.str(), 0, true );
    }
    for( npc_ptr &guy : get_npcs_assigned() ) {
        talk_function::stop_guard( *guy );
    }
    // We must send this message early, before the name is erased.
    add_msg( m_info, _( "You abandon %s." ), name );
    remove_camp();
}

void basecamp::scan_pseudo_items()
{
    map &here = get_map();
    for( auto &expansion : expansions ) {
        expansion.second.available_pseudo_items.clear();
        tripoint_abs_omt tile = omt_pos + expansion.first;
        tinymap expansion_map;
        expansion_map.load( tile, false );

        const tripoint_omt_ms mapmin{ 0, 0, omt_pos.z() };
        const tripoint_omt_ms mapmax{ 2 * SEEX - 1, 2 * SEEY - 1, omt_pos.z() };
        for( const tripoint_omt_ms &pos : expansion_map.points_in_rectangle( mapmin, mapmax ) ) {
            const furn_id &f = expansion_map.furn( pos );
            if( f != furn_str_id::NULL_ID() &&
                f.obj().crafting_pseudo_item.is_valid() &&
                f.obj().crafting_pseudo_item.obj().has_flag( flag_ALLOWS_REMOTE_USE ) ) {
                bool found = false;
                for( itype_id &element : expansion.second.available_pseudo_items ) {
                    if( element == f.obj().crafting_pseudo_item ) {
                        found = true;
                        break;
                    }
                }
                if( !found ) {
                    expansion.second.available_pseudo_items.push_back( expansion_map.furn(
                                pos ).obj().crafting_pseudo_item );
                }
            }

            const optional_vpart_position &vp = expansion_map.veh_at( pos );
            if( vp.has_value() &&
                vp->vehicle().is_appliance() ) {
                for( const auto &[tool, discard_] : vp->get_tools( here ) ) {
                    if( tool.has_flag( flag_PSEUDO ) &&
                        tool.has_flag( flag_ALLOWS_REMOTE_USE ) ) {
                        bool found = false;
                        for( itype_id &element : expansion.second.available_pseudo_items ) {
                            if( element == tool.typeId() ) {
                                found = true;
                                break;
                            }
                        }
                        if( !found ) {
                            expansion.second.available_pseudo_items.push_back( tool.typeId() );
                        }
                    }
                }
            }
        }
    }
}

void basecamp::worker_assignment_ui()
{
    int entries_per_page = 0;
    catacurses::window w_followers;

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const point term( TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0,
                          TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0 );

        w_followers = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                          point( term.y, term.x ) );
        entries_per_page = FULL_SCREEN_HEIGHT - 5;

        ui.position_from_window( w_followers );
    } );
    ui.mark_resize();

    size_t selection = 0;
    input_context ctxt( "FACTION_MANAGER" );
    ctxt.register_action( "INSPECT_NPC" );
    ctxt.register_navigate_ui_list();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    validate_assignees();
    g->validate_npc_followers();

    std::vector<npc *> followers;
    npc *cur_npc = nullptr;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_followers );

        // entries_per_page * page number
        const size_t top_of_page = entries_per_page * ( selection / entries_per_page );

        mvwvline( w_followers, point( 45, 0 ), BORDER_COLOR, LINE_XOXO, FULL_SCREEN_HEIGHT - 2 ) ;
        draw_border( w_followers );
        const nc_color col = c_white;
        const std::string no_npcs = _( "You have no companions following you." );
        if( !followers.empty() ) {
            draw_scrollbar( w_followers, selection, entries_per_page, followers.size(),
                            point( 0, 3 ) );
            for( size_t i = top_of_page; i < followers.size(); i++ ) {
                const int y = i - top_of_page + 3;
                trim_and_print( w_followers, point( 1, y ), 43, selection == i ? hilite( col ) : col,
                                followers[i]->disp_name() );
            }
        } else {
            mvwprintz( w_followers, point( 1, 4 ), c_light_red, no_npcs );
        }
        mvwprintz( w_followers, point( 1, FULL_SCREEN_HEIGHT - 2 ), c_light_gray,
                   _( "Press %s to inspect this follower." ), ctxt.get_desc( "INSPECT_NPC" ) );
        mvwprintz( w_followers, point( 1, FULL_SCREEN_HEIGHT - 1 ), c_light_gray,
                   _( "Press %s to assign this follower to this camp." ), ctxt.get_desc( "CONFIRM" ) );
        wnoutrefresh( w_followers );
    } );

    while( true ) {
        // create a list of npcs stationed at this camp
        overmap_buffer.populate_followers_vec( followers, true, true );
        cur_npc = nullptr;
        if( !followers.empty() ) {
            cur_npc = followers[selection];
        }

        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        if( action == "INSPECT_NPC" ) {
            if( cur_npc ) {
                cur_npc->disp_info();
            }
        } else if( navigate_ui_list( action, selection, 5, followers.size(), true ) ) {
        } else if( action == "CONFIRM" ) {
            if( !followers.empty() && cur_npc ) {
                talk_function::assign_camp( *cur_npc );
            }
        } else if( action == "QUIT" ) {
            break;
        }
    }
}

void basecamp::job_assignment_ui()
{
    int entries_per_page = 0;
    catacurses::window w_jobs;

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const point term( TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0,
                          TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0 );

        w_jobs = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                     point( term.y, term.x ) );

        entries_per_page = FULL_SCREEN_HEIGHT - 5;

        ui.position_from_window( w_jobs );
    } );
    ui.mark_resize();

    size_t selection = 0;
    input_context ctxt( "FACTION_MANAGER" );
    ctxt.register_action( "INSPECT_NPC" );
    ctxt.register_navigate_ui_list();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    validate_assignees();

    std::vector<npc *> stationed_npcs;
    npc *cur_npc = nullptr;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_jobs );
        const size_t top_of_page = entries_per_page * ( selection / entries_per_page );
        mvwvline( w_jobs, point( 45, 0 ), BORDER_COLOR, LINE_XOXO, FULL_SCREEN_HEIGHT - 2 );
        draw_border( w_jobs );
        mvwprintz( w_jobs, point( 46, 1 ), c_white, _( "Job/Priority" ) );
        const nc_color col = c_white;
        const std::string no_npcs = _( "There are no NPCs stationed here" );
        if( !stationed_npcs.empty() ) {
            draw_scrollbar( w_jobs, selection, entries_per_page, stationed_npcs.size(),
                            point( 0, 3 ) );
            for( size_t i = top_of_page; i < stationed_npcs.size(); i++ ) {
                const int y = i - top_of_page + 3;
                trim_and_print( w_jobs, point( 1, y ), 43, selection == i ? hilite( col ) : col,
                                stationed_npcs[i]->disp_name() );
            }
            if( selection < stationed_npcs.size() ) {
                int start_y = 3;
                if( cur_npc ) {
                    if( cur_npc->has_job() ) {
                        for( activity_id &elem : cur_npc->job.get_prioritised_vector() ) {
                            const int priority = cur_npc->job.get_priority_of_job( elem );
                            player_activity test_act = player_activity( elem );
                            mvwprintz( w_jobs, point( 46, start_y ), c_light_gray, string_format( _( "%s : %s" ),
                                       test_act.get_verb(), std::to_string( priority ) ) );
                            start_y++;
                        }
                    } else {
                        mvwprintz( w_jobs, point( 46, start_y ), c_light_red, _( "No current job." ) );
                    }
                }
            } else {
                mvwprintz( w_jobs, point( 46, 4 ), c_light_red, no_npcs );
            }
        } else {
            mvwprintz( w_jobs, point( 46, 4 ), c_light_red, no_npcs );
        }
        mvwprintz( w_jobs, point( 1, FULL_SCREEN_HEIGHT - 2 ), c_light_gray,
                   _( "Press %s to inspect this follower." ), ctxt.get_desc( "INSPECT_NPC" ) );
        mvwprintz( w_jobs, point( 1, FULL_SCREEN_HEIGHT - 1 ), c_light_gray,
                   _( "Press %s to change this workers job priorities." ), ctxt.get_desc( "CONFIRM" ) );
        wnoutrefresh( w_jobs );
    } );

    while( true ) {
        // create a list of npcs stationed at this camp
        stationed_npcs.clear();
        for( const auto &elem : get_npcs_assigned() ) {
            if( elem ) {
                stationed_npcs.push_back( elem.get() );
            }
        }
        cur_npc = nullptr;
        // entries_per_page * page number
        if( !stationed_npcs.empty() ) {
            cur_npc = stationed_npcs[selection];
        }

        ui_manager::redraw();

        const std::string action = ctxt.handle_input();
        if( action == "INSPECT_NPC" ) {
            if( cur_npc ) {
                cur_npc->disp_info();
            }
        } else if( navigate_ui_list( action, selection, 5, stationed_npcs.size(), true ) ) {
        } else if( action == "CONFIRM" ) {
            if( cur_npc ) {
                while( true ) {
                    uilist smenu;
                    smenu.text = _( "Assign job priority (0 to disable)" );
                    int count = 0;
                    std::vector<activity_id> job_vec = cur_npc->job.get_prioritised_vector();
                    smenu.addentry( count++, true, 'C', _( "Clear all priorities" ) );
                    smenu.addentry( count++, true, 'S', _( "Set all priorities" ) );
                    for( const activity_id &elem : job_vec ) {
                        player_activity test_act = player_activity( elem );
                        const int priority = cur_npc->job.get_priority_of_job( elem );
                        smenu.addentry( count, true, MENU_AUTOASSIGN, string_format( _( "%s : %s" ), test_act.get_verb(),
                                        std::to_string( priority ) ) );
                        count++;
                    }
                    smenu.query();
                    if( smenu.ret == UILIST_CANCEL ) {
                        break;
                    }
                    if( smenu.ret == 0 ) {
                        cur_npc->job.clear_all_priorities();
                    } else if( smenu.ret == 1 ) {
                        int priority = 0;
                        query_int( priority, false, _( "Priority for all jobs " ) );
                        cur_npc->job.set_all_priorities( priority );
                    } else if( smenu.ret > 1 && smenu.ret <= static_cast<int>( job_vec.size() ) + 1 ) {
                        activity_id sel_job = job_vec[size_t( smenu.ret - 2 )];
                        player_activity test_act = player_activity( sel_job );
                        int priority = 0;
                        query_int( priority, false, _( "Priority for %s " ), test_act.get_verb() );
                        cur_npc->job.set_task_priority( sel_job, priority );
                    } else {
                        break;
                    }
                }
            }
        } else if( action == "QUIT" ) {
            break;
        }
    }
}

void basecamp::start_menial_labor()
{
    if( fac()->food_supply().kcal() < time_to_food( 3_hours ) ) {
        popup( _( "You don't have enough food stored to feed your companion." ) );
        return;
    }
    shared_ptr_fast<npc> comp = talk_function::companion_choose();
    if( comp == nullptr ) {
        return;
    }
    validate_sort_points();

    comp->assign_activity( zone_sort_activity_actor() );
    popup( _( "%s goes off to clean toilets and sort loot." ), comp->disp_name() );
}

static void change_cleared_terrain( tripoint_abs_omt forest )
{
    if( om_cutdown_trees_est( forest ) < 5 ) {
        const oter_id &omt_trees = overmap_buffer.ter( forest );

        // Oter types before and after clear cutting, if value (after) is linear, key (before) should also be linear, otherwise any combination of rotatable/linear/not rotatable should work
        static const std::unordered_map<oter_type_str_id, oter_type_str_id> clear_cut_conversion = {
            { oter_type_forest, oter_type_field },
            { oter_type_forest_thick, oter_type_field },
            { oter_type_forest_trail, oter_type_field },
            { oter_type_forest_trail_intersection, oter_type_field },
            { oter_type_rural_road_forest, oter_type_rural_road },
            { oter_type_special_forest, oter_type_field },
            { oter_type_special_forest_thick, oter_type_field }
        };

        auto converted_it = clear_cut_conversion.find( omt_trees->get_type_id() );
        if( converted_it == clear_cut_conversion.end() ) {
            popup( _( "%s isn't a recognized terrain.  Please file a bug report." ), omt_trees.id().c_str() );
            return;
        } else {
            const oter_type_str_id &oter_type_clearcut = converted_it->second;
            oter_id oter_to_place;
            // Maintain rotation
            if( oter_type_clearcut->is_linear() ) {
                oter_to_place = oter_type_clearcut->get_linear( omt_trees->get_line() );
            } else {
                oter_to_place = oter_type_clearcut->get_rotated( omt_trees->get_dir() );
            }
            overmap_buffer.ter_set( forest, oter_to_place );
        }
        popup( _( "The logged tile has been cleared and cannot be logged further after this mission." ),
               omt_trees.id().c_str() );
    }
}

static const std::unordered_set<oter_type_str_id> terrains_forest = { oter_type_forest, oter_type_forest_thick, oter_type_forest_trail, oter_type_forest_trail_intersection, oter_type_rural_road_forest, oter_type_special_forest, oter_type_special_forest_thick };
static const std::unordered_set<oter_type_str_id> terrains_field_swamp_forest = [] {
    std::unordered_set<oter_type_str_id> tmp = terrains_forest;
    tmp.insert( oter_type_field );
    tmp.insert( oter_type_forest_water );
    return tmp;
}();

void basecamp::start_cut_logs( const mission_id &miss_id, float exertion_level )
{
    tripoint_abs_omt forest = om_target_tile( omt_pos, 1, 50, terrains_forest, true,
                              tripoint_abs_omt::invalid, false,
                              _( "Select a forest (or road/trail) from %d to %d tiles away." ) );
    if( !forest.is_invalid() ) {
        standard_npc sample_npc( "Temp" );
        sample_npc.set_fake( true );
        const int tree_est = om_cutdown_trees_est( forest, 50 );
        const int tree_young_est = om_harvest_ter_est( sample_npc, forest,
                                   ter_t_tree_young, 50 );
        pf::simple_path<tripoint_abs_omt> path = overmap_buffer.get_travel_path( omt_pos, forest,
                overmap_path_params::for_npc() );
        //Very roughly what the player does  + 6 hours for prep, clean up
        time_duration chop_time = 6_hours + 1_hours * tree_est + 7_minutes * tree_young_est;
        // approximate average yields
        // TODO use bash results from t_tree_young to get a better estimate
        const int trips = om_carry_weight_to_trips(
                              tree_est * itype_log->weight * 7.5 + tree_young_est * itype_stick_long->weight * 2.5,
                              tree_est * itype_log->volume * 7.5 + tree_young_est * itype_stick_long->volume * 2.5
                          );
        const time_duration travel_time = companion_travel_time_calc( path, trips );
        const time_duration total_time = base_camps::to_workdays( travel_time + chop_time );
        const int need_food = time_to_food( total_time, exertion_level, travel_time );
        if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time,
                       chop_time, travel_time, path.dist, trips, need_food ) ) ) {
            return;
        }

        npc_ptr comp = start_mission( miss_id,
                                      total_time, true,
                                      _( "departs to cut logs…" ), false, {},
                                      skill_fabrication, 2, exertion_level, travel_time );
        if( comp != nullptr ) {
            om_cutdown_trees_logs( forest, 50 );
            om_harvest_ter( *comp, forest, ter_t_tree_young, 50 );
            const mass_volume results = om_harvest_itm( comp, forest, 95 );
            const int trips = om_carry_weight_to_trips( results.wgt, results.vol, comp );
            const time_duration travel_time = companion_travel_time_calc( path, trips );
            const time_duration total_time = base_camps::to_workdays( travel_time + chop_time );
            comp->companion_mission_time_ret = calendar::turn + total_time;
            change_cleared_terrain( forest );
        }
    }
}

void basecamp::start_clearcut( const mission_id &miss_id, float exertion_level )
{
    popup( _( "Forests are the only valid cutting locations, with forest dirt roads, forest rural roads, and trails being valid as well.  Note that it's likely both forest and field roads look exactly the same after having been cleared." ) );
    tripoint_abs_omt forest = om_target_tile( omt_pos, 1, 50, terrains_forest );
    if( !forest.is_invalid() ) {
        standard_npc sample_npc( "Temp" );
        sample_npc.set_fake( true );
        const int tree_est = om_cutdown_trees_est( forest, 95 );
        const int tree_young_est = om_harvest_ter_est( sample_npc, forest,
                                   ter_t_tree_young, 95 );
        pf::simple_path<tripoint_abs_omt> path = overmap_buffer.get_travel_path( omt_pos, forest,
                overmap_path_params::for_npc() );
        //Very roughly what the player does + 6 hours for prep, clean up
        const time_duration chop_time = 6_hours + 1_hours * tree_est + 7_minutes * tree_young_est;
        const time_duration travel_time = companion_travel_time_calc( path, 2 );
        const time_duration total_time = base_camps::to_workdays( travel_time + chop_time );
        const int need_food = time_to_food( total_time, exertion_level, travel_time );
        if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time,
                       chop_time, travel_time, path.dist, 2, need_food ) ) ) {
            return;
        }

        npc_ptr comp = start_mission( miss_id,
                                      total_time,
                                      true, _( "departs to clear a forest…" ), false, {},
                                      skill_fabrication, 1, exertion_level, travel_time );
        if( comp != nullptr ) {
            om_cutdown_trees_trunks( forest, 95 );
            om_harvest_ter_break( *comp, forest, ter_t_tree_young, 95 );
            change_cleared_terrain( forest );
        }
    }
}

void basecamp::start_setup_hide_site( const mission_id &miss_id, float exertion_level )
{
    tripoint_abs_omt forest = om_target_tile( omt_pos, 10, 90, terrains_field_swamp_forest, true,
                              omt_pos, true, _( "Select a forest, swamp, or field from %d to %d tiles away." ) );
    if( !forest.is_invalid() ) {
        pf::simple_path<tripoint_abs_omt> path = overmap_buffer.get_travel_path( omt_pos, forest,
                overmap_path_params::for_npc() );
        Character *pc = &get_player_character();
        const inventory_filter_preset preset( []( const item_location & location ) {
            return !location->can_revive() && !location->will_spill();
        } );

        units::volume total_volume;
        units::mass total_mass;

        drop_locations losing_equipment = give_equipment( pc, preset,
                                          _( "These are the items you've selected so far." ), _( "Select items to send" ), total_volume,
                                          total_mass );

        const int trips = om_carry_weight_to_trips( total_mass, total_volume );
        const time_duration build_time = 6_hours;
        const time_duration travel_time = companion_travel_time_calc( path, trips );
        const time_duration total_time = base_camps::to_workdays( travel_time + build_time );
        const int need_food = time_to_food( total_time, exertion_level, travel_time );
        if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time,
                       build_time, travel_time, path.dist, trips, need_food ) ) ) {
            return;
        }
        npc_ptr comp = start_mission( miss_id,
                                      total_time, true,
                                      _( "departs to build a hide site…" ), false, {},
                                      skill_survival, 3, exertion_level, travel_time );
        if( comp != nullptr ) {
            const int trips = om_carry_weight_to_trips( total_mass, total_volume, comp );
            const time_duration travel_time = companion_travel_time_calc( path, trips );
            const time_duration total_time = base_camps::to_workdays( travel_time + build_time );
            comp->companion_mission_time_ret = calendar::turn + total_time;
            om_set_hide_site( *comp, forest, losing_equipment );
        }
    }
}

static const tripoint_omt_ms relay_site_stash{ 11, 10, 0 };

void basecamp::start_relay_hide_site( const mission_id &miss_id, float exertion_level )
{
    const std::unordered_set<oter_type_str_id> hide_locations = { oter_type_faction_hide_site_0 };
    tripoint_abs_omt forest = om_target_tile( omt_pos, 10, 90, hide_locations, true, omt_pos, true,
                              string_format( _( "Select an existing hide site from %d to %d tiles away." ), 10, 90 ) );
    if( !forest.is_invalid() ) {
        pf::simple_path<tripoint_abs_omt> path = overmap_buffer.get_travel_path( omt_pos, forest,
                overmap_path_params::for_npc() );
        Character *pc = &get_player_character();
        const inventory_filter_preset preset( []( const item_location & location ) {
            return !location->can_revive() && !location->will_spill();
        } );

        units::volume total_export_volume;
        units::mass total_export_mass;

        drop_locations losing_equipment = give_equipment( pc, preset,
                                          _( "These are the items you've selected so far." ), _( "Select items to send" ),
                                          total_export_volume, total_export_mass );

        //Check items in improvised shelters at hide site
        tinymap target_bay;
        target_bay.load( forest, false );
        // Redundant as long as map operations aren't using get_map() in a transitive call chain. Added for future proofing.
        swap_map swap( *target_bay.cast_to_map() );

        units::volume total_import_volume;
        units::mass total_import_mass;

        drop_locations gaining_equipment = get_equipment( &target_bay, relay_site_stash, pc, preset,
                                           _( "These are the items you've selected so far." ), _( "Select items to bring back" ),
                                           total_import_volume, total_import_mass );

        if( !losing_equipment.empty() || !gaining_equipment.empty() ) {
            // After calculating round trips for each direction, only the greater one is used.
            const int trips = std::max(
                                  om_carry_weight_to_trips( total_import_mass, total_import_volume ),
                                  om_carry_weight_to_trips( total_export_mass, total_export_volume )
                              );
            // We're not actually constructing anything, just loading/unloading/performing very light maintenance
            const time_duration build_time = 5_minutes;
            const time_duration travel_time = companion_travel_time_calc( path, trips );
            const time_duration total_time = base_camps::to_workdays( travel_time + build_time );
            const int need_food = time_to_food( total_time, exertion_level, travel_time );
            if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time, build_time,
                           travel_time, path.dist, trips, need_food ) ) ) {
                return;
            }

            npc_ptr comp = start_mission( miss_id,
                                          total_time, true,
                                          _( "departs for the hide site…" ), false, {},
                                          skill_survival, 3, exertion_level, travel_time );
            if( comp != nullptr ) {
                // recalculate trips based on actual load
                const int trips = std::max(
                                      om_carry_weight_to_trips( total_import_mass, total_import_volume, comp ),
                                      om_carry_weight_to_trips( total_export_mass, total_export_volume, comp )
                                  );
                const time_duration travel_time = companion_travel_time_calc( path, trips );
                const time_duration total_time = base_camps::to_workdays( travel_time + build_time );
                comp->companion_mission_time_ret = calendar::turn + total_time;
                om_set_hide_site( *comp, forest, losing_equipment, gaining_equipment );
            }
        } else {
            popup( _( "You need equipment to transport to/from the hide site…" ) );
        }
    }
}

// Stupid "the const qualified parameter 'comp' is copied for each invocation; consider making it a reference [performance-unnecessary-value-param,-warnings-as-errors]" demands the pointer to be referenced...
static void apply_fortifications( const mission_id &miss_id, const npc_ptr *comp, bool start )
{
    std::vector<tripoint_abs_omt> &build_point = comp[0]->companion_mission_points;
    static const std::array<std::pair<update_mapgen_id, update_mapgen_id>, 4> pits { {
            { update_mapgen_faction_wall_level_S_0, update_mapgen_faction_wall_level_N_0 },
            { update_mapgen_faction_wall_level_E_0,  update_mapgen_faction_wall_level_W_0 },
            { update_mapgen_faction_wall_level_S_1, update_mapgen_faction_wall_level_N_1 },
            { update_mapgen_faction_wall_level_E_1,  update_mapgen_faction_wall_level_W_1 }
        } };
    const bool is_ns = build_point[0].y() != build_point[1].y();
    const bool is_spiked =  miss_id.parameters == faction_wall_level_n_1_string ||
                            //  Handling of old format (changed mid 0.F) below
                            ( miss_id.parameters.empty() &&
                              comp[0]->companion_mission_role_id == faction_wall_level_n_1_string );
    const std::pair<update_mapgen_id, update_mapgen_id> &build = is_spiked ?
            ( is_ns ? pits[0] : pits[1] ) :
            ( is_ns ? pits[2] : pits[3] );

    //Add fences
    for( size_t pt = 0; pt < build_point.size(); pt++ ) {
        //First point is always at top or west since they are built in a line and sorted
        if( pt == 0 ) {
            if( !start ) {
                run_mapgen_update_func( build.first, build_point[pt], {} );
            }
            apply_construction_marker( build.first, build_point[pt],
                                       miss_id.mapgen_args, false,
                                       false, false, start );
        } else if( pt == build_point.size() - 1 ) {
            if( !start ) {
                run_mapgen_update_func( build.second, build_point[pt], {} );
            }
            apply_construction_marker( build.second, build_point[pt],
                                       miss_id.mapgen_args, false,
                                       false, false, start );
        } else {
            if( !start ) {
                run_mapgen_update_func( build.first, build_point[pt], {} );
                run_mapgen_update_func( build.second, build_point[pt], {} );
            }
            apply_construction_marker( build.first, build_point[pt],
                                       miss_id.mapgen_args, false,
                                       false, false, start );
            apply_construction_marker( build.second, build_point[pt],
                                       miss_id.mapgen_args, false,
                                       false, false, start );
        }
    }
}

void basecamp::start_fortifications( const mission_id &miss_id, float exertion_level )
{
    popup( _( "Select a start and end point.  Line must be straight.  Fields, forests, and "
              "swamps are valid fortification locations.  In addition to existing fortification "
              "constructions." ) );
    tripoint_abs_omt start = om_target_tile( omt_pos, 2, 90, terrains_field_swamp_forest, true, omt_pos,
                             false, _( "Select a start point from %d to %d tiles away." ) );
    if( start.is_invalid() ) {
        return;
    }
    tripoint_abs_omt stop = om_target_tile( omt_pos, 2, 90, terrains_field_swamp_forest, true, start,
                                            false, _( "Select an end point from %d to %d tiles away." ) );
    if( stop.is_invalid() ) {
        return;
    }
    const recipe &making = recipe_id( miss_id.parameters ).obj();
    bool change_x = start.x() != stop.x();
    bool change_y = start.y() != stop.y();
    if( change_x && change_y ) {
        popup( _( "Construction line must be straight!" ) );
        return;
    }
    if( miss_id.parameters == faction_wall_level_n_1_string ) {
        std::vector<tripoint_abs_omt> tmp_line = line_to( stop, start );
        // line_to doesn't include the origin point
        tmp_line.emplace_back( stop );
        int line_count = tmp_line.size();
        int yes_count = 0;
        for( tripoint_abs_omt &elem : tmp_line ) {
            if( std::find( fortifications.begin(), fortifications.end(), elem ) != fortifications.end() ) {
                yes_count += 1;
            }
        }
        if( yes_count < line_count ) {
            popup( _( "Spiked pits must be built over existing trenches!" ) );
            return;
        }
    }
    std::vector<tripoint_abs_omt> fortify_om;
    if( ( change_x && stop.x() < start.x() ) || ( change_y && stop.y() < start.y() ) ) {
        //line_to doesn't include the origin point
        fortify_om.push_back( stop );
        std::vector<tripoint_abs_omt> tmp_line = line_to( stop, start );
        fortify_om.insert( fortify_om.end(), tmp_line.begin(), tmp_line.end() );
    } else {
        fortify_om.push_back( start );
        std::vector<tripoint_abs_omt> tmp_line = line_to( start, stop );
        fortify_om.insert( fortify_om.end(), tmp_line.begin(), tmp_line.end() );
    }
    int trips = 0;
    time_duration build_time = 0_hours;
    time_duration travel_time = 0_hours;
    int dist = 0;
    for( tripoint_abs_omt &fort_om : fortify_om ) {
        const oter_type_str_id &omt_ref = overmap_buffer.ter( fort_om )->get_type_id();
        if( terrains_field_swamp_forest.find( omt_ref ) == terrains_field_swamp_forest.end() ) {
            popup( _( "Invalid terrain in construction path." ) );
            return;
        }
        // spiked pit requires traveling back and forth to carry components
        // TODO calculate whether one trip can carry multiple tiles worth of components
        if( miss_id.parameters == faction_wall_level_n_1_string ) {
            trips += 2;
            const pf::simple_path<tripoint_abs_omt> &path = overmap_buffer.get_travel_path( omt_pos, fort_om,
                    overmap_path_params::for_npc() );
            travel_time += companion_travel_time_calc( path, 2 );
            dist += path.dist * 2;
        }
        build_time += making.batch_duration( get_player_character() ); // TODO calculate for NPC, not player
    }
    // trench requires just one triangular trip
    if( miss_id.parameters != faction_wall_level_n_1_string ) {
        trips = 1;
        const pf::simple_path<tripoint_abs_omt> &path1 = overmap_buffer.get_travel_path( omt_pos, start,
                overmap_path_params::for_npc() );
        const pf::simple_path<tripoint_abs_omt> &path2 = overmap_buffer.get_travel_path( start, stop,
                overmap_path_params::for_npc() );
        const pf::simple_path<tripoint_abs_omt> &path3 = overmap_buffer.get_travel_path( stop, omt_pos,
                overmap_path_params::for_npc() );
        travel_time = companion_travel_time_calc( path1, 1 ) +
                      companion_travel_time_calc( path2, 1 ) +
                      companion_travel_time_calc( path3, 1 );
        dist = path1.dist + path2.dist + path3.dist;
    }
    const time_duration total_time = base_camps::to_workdays( travel_time + build_time );
    const int need_food = time_to_food( total_time, exertion_level, travel_time );
    if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time, build_time,
                   travel_time, dist, trips, need_food ) ) ) {
        return;
    } else if( !making.deduped_requirements().can_make_with_inventory( _inv,
               making.get_component_filter(), ( fortify_om.size() * 2 ) - 2 ) ) {
        popup( _( "You don't have the material to build the fortification." ) );
        return;
    }

    const int batch_size = fortify_om.size() * 2 - 2;
    mapgen_arguments arg;  //  Created with a default value.
    basecamp_action_components components( making, arg, batch_size, *this );
    if( !components.choose_components() ) {
        return;
    }

    npc_ptr comp = start_mission(
                       miss_id, total_time, true,
                       _( "begins constructing fortifications…" ), false, {},
                       making.required_skills, exertion_level, travel_time );
    if( comp != nullptr ) {
        components.consume_components();
        for( tripoint_abs_omt &pt : fortify_om ) {
            comp->companion_mission_points.push_back( pt );
        }

        apply_fortifications( miss_id, &comp, true );
    }
}

static const int max_salt_water_pipe_distance = 10;
static const int max_salt_water_pipe_length =
    20;  //  It has to be able to wind around terrain it can't pass through, like the rest of the camp.

//  Hard coded strings used to construct expansion "provides" needed by recipes to coordinate salt water pipe construction
static const std::string salt_water_pipe_string_base = "salt_water_pipe_";
static const std::string salt_water_pipe_string_suffix = "_scheduled";
static const double diagonal_salt_pipe_cost = std::sqrt( 2.0 );
static const double salt_pipe_legal = 0.0;
static const double salt_pipe_illegal = -0.1;
static const double salt_pipe_swamp = -0.2;
static constexpr size_t path_map_size = 2 * max_salt_water_pipe_distance + 1;
using PathMap = cata::mdarray<double, point, path_map_size, path_map_size>;

//  The logic discourages diagonal connections when there are horizontal ones
//  of the same number of tiles, as the original approach resulted in rather
//  odd paths. At the time of this writing there is no corresponding
//  construction cost difference, though, as that doesn't match with the fixed
//  recipe approach taken.
static point_rel_omt check_salt_pipe_neighbors( PathMap &path_map, point_rel_omt pt )
{
    point_rel_omt found = point_rel_omt::invalid;
    double lowest_found = -10000.0;
    double cost;

    for( int i = -1; i <= 1; i++ ) {
        for( int k = -1; k <= 1; k++ ) {
            if( pt.x() + i > -max_salt_water_pipe_distance &&
                pt.x() + i < max_salt_water_pipe_distance &&
                pt.y() + k > -max_salt_water_pipe_distance &&
                pt.y() + k < max_salt_water_pipe_distance ) {
                if( i != 0 && k != 0 ) {
                    cost = diagonal_salt_pipe_cost;
                } else {
                    cost = 1.0;
                }

                if( path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k]
                    ==
                    salt_pipe_legal ||
                    ( path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k] >
                      0.0 &&
                      path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k] >
                      path_map[max_salt_water_pipe_distance + pt.x()][max_salt_water_pipe_distance + pt.y()] + cost ) ) {
                    path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k] =
                        path_map[max_salt_water_pipe_distance + pt.x()][max_salt_water_pipe_distance + pt.y()] + cost;

                } else if( path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y()
                           +
                           k] <=
                           salt_pipe_swamp ) {
                    if( path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k]
                        ==
                        salt_pipe_swamp ||
                        path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k] < -
                        ( path_map[max_salt_water_pipe_distance + pt.x()][max_salt_water_pipe_distance + pt.y()] +
                          cost ) ) {
                        path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k] = -
                                ( path_map[max_salt_water_pipe_distance + pt.x()][max_salt_water_pipe_distance + pt.y()] + cost );

                        if( path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance + pt.y() + k] >
                            lowest_found ) {
                            lowest_found = path_map[max_salt_water_pipe_distance + pt.x() + i][max_salt_water_pipe_distance +
                                           pt.y()
                                           + k];
                            found = pt + point( i, k );
                        }
                    }
                }
            }
        }
    }
    return found;
}

static int salt_water_pipe_segment_of( const recipe &making );

int salt_water_pipe_segment_of( const recipe &making )
{
    int segment_number = -1;
    const auto &requirements = making.blueprint_requires();
    for( auto const &element : requirements ) {
        if( element.first.substr( 0, salt_water_pipe_string_base.length() ) == salt_water_pipe_string_base
            &&
            element.first.substr( element.first.length() - salt_water_pipe_string_suffix.length(),
                                  salt_water_pipe_string_suffix.length() ) == salt_water_pipe_string_suffix ) {
            try {
                segment_number = stoi( element.first.substr( salt_water_pipe_string_base.length(),
                                       element.first.length() - salt_water_pipe_string_suffix.length() - 1 ) );
            } catch( ... ) {
                std::string msg = "Recipe 'blueprint_requires' that matches the hard coded '";
                msg += salt_water_pipe_string_base;
                msg += "#";
                msg += salt_water_pipe_string_suffix;
                msg += "' pattern without having a number in the # position";
                debugmsg( msg );
                return -1;
            }
            if( segment_number < 1 || segment_number >= max_salt_water_pipe_length ) {
                std::string msg = "Recipe 'blueprint_requires' that matches the hard coded '";
                msg += salt_water_pipe_string_base;
                msg += "#";
                msg += salt_water_pipe_string_suffix;
                msg += "' pattern with a number outside the ones supported and generated by the code";
                debugmsg( msg );
                return -1;
            }
        }
    }
    if( segment_number == -1 ) {
        debugmsg( "Failed to find recipe 'blueprint_requires' that matches the hard coded '" +
                  salt_water_pipe_string_base + "#" + salt_water_pipe_string_suffix + "' pattern" );
    }

    return segment_number;
}

//  Defines the direction of a tile adjacent to an expansion to which the expansion should make a connection.
//  Support operation for the salt water pipe functionality, but might be used if some other functionality
//  has a use for it.
//  The operation (ab)uses the conditional rotation/mirror flags of a recipe that uses a blueprint that isn't
//  actually going to be used as a blueprint for something constructed in the expansion itself to determine
//  the direction of the tile to connect to.
static point_rel_omt connection_direction_of( const point_rel_omt &dir, const recipe &making );

point_rel_omt connection_direction_of( const point_rel_omt &dir, const recipe &making )
{
    point_rel_omt connection_dir = point_rel_omt::north;
    const std::string suffix = base_camps::all_directions.at( dir ).id.substr( 1,
                               base_camps::all_directions.at( dir ).id.length() - 2 );
    int count = 0;

    if( making.has_flag( "MAP_ROTATE_90_IF_" + suffix ) ) {
        connection_dir = point_rel_omt::east;
        count++;
    }
    if( making.has_flag( "MAP_ROTATE_180_IF_" + suffix ) ) {
        connection_dir = point_rel_omt::south;
        count++;
    }
    if( making.has_flag( "MAP_ROTATE_270_IF_" + suffix ) ) {
        connection_dir = point_rel_omt::west;
        count++;
    }
    if( count > 1 ) {
        popup( _( "Bug, Incorrect recipe: More than one rotation per orientation isn't valid" ) );
        return point_rel_omt::invalid;
    }

    if( making.has_flag( "MAP_MIRROR_HORIZONTAL_IF_" + suffix ) ) {
        connection_dir.x() = -connection_dir.x();
    }
    if( making.has_flag( "MAP_MIRROR_VERTICALL_IF_" + suffix ) ) {
        connection_dir.y() = -connection_dir.y();
    }

    return connection_dir;
}

static void salt_water_pipe_orientation_adjustment( const point_rel_omt &dir, bool &orthogonal,
        bool &mirror_vertical, bool &mirror_horizontal, int &rotation )
{
    orthogonal = true;
    mirror_horizontal = false;
    mirror_vertical = false;
    rotation = 0;

    switch( base_camps::all_directions.at( dir ).tab_order ) {
        case base_camps::tab_mode::TAB_MAIN: //  Should not happen. We would have had to define the same point twice.
        case base_camps::tab_mode::TAB_N:    //  This is the reference direction for orthogonal orientations.
            break;

        case base_camps::tab_mode::TAB_NE:
            orthogonal = false;
            break;  //  This is the reference direction for diagonal orientations.

        case base_camps::tab_mode::TAB_E:
            rotation = 1;
            break;

        case base_camps::tab_mode::TAB_SE:
            orthogonal = false;
            rotation = 1;
            break;

        case base_camps::tab_mode::TAB_S:
            mirror_vertical = true;
            break;

        case base_camps::tab_mode::TAB_SW:
            orthogonal = false;
            rotation = 2;
            break;

        case base_camps::tab_mode::TAB_W:
            rotation = 1;
            mirror_vertical = true;
            break;

        case base_camps::tab_mode::TAB_NW:
            orthogonal = false;
            rotation = 3;
            break;
    }
}

bool basecamp::common_salt_water_pipe_construction(
    const mission_id &miss_id, expansion_salt_water_pipe *pipe, int segment_number )
{
    const recipe &making = recipe_id(
                               miss_id.parameters ).obj(); //  Actually a template recipe that we'll rotate and mirror as required.
    int remaining_segments = pipe->segments.size() - 1;

    if( segment_number == 0 ) {
        if( !query_yn( _( "Number of additional sessions required: %i" ), remaining_segments ) ) {
            return false;
        }
    }

    mapgen_arguments arg;  //  Created with a default value.
    basecamp_action_components components( making, arg, 1, *this );
    if( !components.choose_components() ) {
        return false;
    }

    comp_list comp;

    if( segment_number == 0 ) {
        comp = start_multi_mission( miss_id, true,
                                    _( "Start constructing salt water pipes…" ),
                                    making.required_skills );

        point_rel_omt connection_dir = pipe->connection_direction;
        const int segment_number = 0;

        point_rel_omt next_construction_direction;

        if( segment_number == pipe->segments.size() - 1 ) {
            next_construction_direction = { -connection_dir.x(), -connection_dir.y()};
        } else {
            next_construction_direction = { pipe->segments[segment_number + 1].point.x() - pipe->segments[segment_number].point.x(),
                                            pipe->segments[segment_number + 1].point.y() - pipe->segments[segment_number].point.y()
                                          };
        }

        bool orthogonal = true;
        bool mirror_horizontal = false;
        bool mirror_vertical = false;
        int rotation = 0;

        salt_water_pipe_orientation_adjustment( next_construction_direction, orthogonal, mirror_vertical,
                                                mirror_horizontal, rotation );

        if( orthogonal ) {
            apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_swamp_N,
                                       pipe->segments[segment_number].point,
                                       miss_id.mapgen_args, mirror_horizontal,
                                       mirror_vertical, rotation, true );
        } else {
            apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_swamp_NE,
                                       pipe->segments[segment_number].point,
                                       miss_id.mapgen_args, mirror_horizontal,
                                       mirror_vertical, rotation, true );
        }

    } else {
        comp = start_multi_mission( miss_id, true,
                                    _( "Continue constructing salt water pipes…" ),
                                    making.required_skills );

        point_rel_omt connection_dir = pipe->connection_direction;

        const point_rel_omt previous_construction_direction = { pipe->segments[segment_number - 1].point.x() - pipe->segments[segment_number].point.x(),
                                                                pipe->segments[segment_number - 1].point.y() - pipe->segments[segment_number].point.y()
                                                              };

        point_rel_omt next_construction_direction;

        if( segment_number == static_cast<int>( pipe->segments.size() - 1 ) ) {
            next_construction_direction = { -connection_dir.x(), -connection_dir.y()};
        } else {
            next_construction_direction = { pipe->segments[segment_number + 1].point.x() - pipe->segments[segment_number].point.x(),
                                            pipe->segments[segment_number + 1].point.y() - pipe->segments[segment_number].point.y()
                                          };
        }

        bool orthogonal = true;
        bool mirror_horizontal = false;
        bool mirror_vertical = false;
        int rotation = 0;

        salt_water_pipe_orientation_adjustment( previous_construction_direction, orthogonal,
                                                mirror_vertical, mirror_horizontal, rotation );

        if( orthogonal ) {
            apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_N,
                                       pipe->segments[segment_number].point,
                                       miss_id.mapgen_args, mirror_horizontal,
                                       mirror_vertical, rotation, true );
        } else {
            apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_NE,
                                       pipe->segments[segment_number].point,
                                       miss_id.mapgen_args, mirror_horizontal,
                                       mirror_vertical, rotation, true );
        }

        salt_water_pipe_orientation_adjustment( next_construction_direction, orthogonal, mirror_vertical,
                                                mirror_horizontal, rotation );

        if( orthogonal ) {
            apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_N,
                                       pipe->segments[segment_number].point,
                                       miss_id.mapgen_args, mirror_horizontal,
                                       mirror_vertical, rotation, true );
        } else {
            apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_NE,
                                       pipe->segments[segment_number].point,
                                       miss_id.mapgen_args, mirror_horizontal,
                                       mirror_vertical, rotation, true );
        }
    }

    if( !comp.empty() ) {
        components.consume_components();
        update_in_progress( miss_id.parameters, miss_id.dir.value() );  // Dir will always have a value
        pipe->segments[segment_number].started = true;
    }

    return !comp.empty();
}

void basecamp::start_salt_water_pipe( const mission_id &miss_id )
{
    const point_rel_omt dir = miss_id.dir.value();  //  Will always have a value
    const recipe &making = recipe_id( miss_id.parameters ).obj();
    point_rel_omt connection_dir = connection_direction_of( dir, making );

    if( connection_dir.is_invalid() ) {
        return;
    }

    expansion_salt_water_pipe *pipe = nullptr;

    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            if( element->segments[0].finished ) {
                debugmsg( "Trying to start construction of a salt water pipe that's already been constructed" );
            }
            //  Assume we've started the construction but it has been cancelled.
            return;
        }
    }

    pipe = new expansion_salt_water_pipe;
    pipe->expansion = dir;
    pipe->connection_direction = connection_dir;
    const oter_type_str_id &allowed_start_location = oter_type_forest_water;
    const std::unordered_set<oter_type_str_id> allowed_locations = { oter_type_forest, oter_type_forest_thick, oter_type_forest_trail, oter_type_field, oter_type_road };
    PathMap path_map;

    for( int i = -max_salt_water_pipe_distance; i <= max_salt_water_pipe_distance; i++ ) {
        for( int k = -max_salt_water_pipe_distance; k <= max_salt_water_pipe_distance; k++ ) {
            tripoint_abs_omt tile = tripoint_abs_omt( omt_pos.x() + dir.x() + connection_dir.x() + i,
                                    omt_pos.y() + dir.y() + connection_dir.y() + k, omt_pos.z() );
            const oter_type_str_id &omt_ref = overmap_buffer.ter( tile )->get_type_id();
            if( allowed_locations.find( omt_ref ) != allowed_locations.end() ) {
                path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_legal;
            } else if( omt_ref == allowed_start_location ) {
                path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_swamp;
            } else {
                path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_illegal;
            }
            //  if this is an expansion tile, forbid it. Only allocated ones have their type changed.
            if( i >= -dir.x() - connection_dir.x() - 1 && i <= -dir.x() - connection_dir.x() + 1 &&
                k >= -dir.y() - connection_dir.y() - 1 && k <= -dir.y() - connection_dir.y() + 1 ) {
                path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_illegal;
            }
        }
    }

    if( path_map[max_salt_water_pipe_distance][max_salt_water_pipe_distance] == salt_pipe_illegal ) {
        auto e = expansions.find( dir );
        basecamp::update_provides( miss_id.parameters, e->second );

        popup( _( "This functionality cannot be constructed as the tile directly adjacent to "
                  "this expansion is not of a type a pipe can be constructed through.  Supported "
                  "terrain is forest, field, road, and swamp.  This recipe will now be "
                  "removed from the set of available recipes and won't show up again." ) );
        return;
    }

    point_rel_omt destination;
    double destination_cost = -10000.0;
    bool path_found = false;

    if( path_map[max_salt_water_pipe_distance][max_salt_water_pipe_distance] ==
        salt_pipe_swamp ) { //  The connection_dir tile is a swamp tile
        destination = point_rel_omt::zero;
        path_found = true;
    } else {
        path_map[max_salt_water_pipe_distance][max_salt_water_pipe_distance] =
            1.0;  //  Always an orthogonal connection to the connection tile.

        for( int distance = 1; distance <= max_salt_water_pipe_length; distance++ ) {
            int dist = distance > max_salt_water_pipe_distance ? max_salt_water_pipe_distance : distance;
            for( int i = -dist; i <= dist; i++ ) { //  No path that can be extended can reach further than dist.
                for( int k = -dist; k <= dist; k++ ) {
                    if( path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] >
                        0.0 ) { // Tile has been assigned a distance and isn't a swamp
                        point_rel_omt temp = check_salt_pipe_neighbors( path_map, { i, k } );
                        if( !temp.is_invalid() ) {
                            if( path_map[max_salt_water_pipe_distance + temp.x()][max_salt_water_pipe_distance + temp.y()] >
                                destination_cost ) {
                                destination_cost = path_map[max_salt_water_pipe_distance + temp.x()][max_salt_water_pipe_distance +
                                                   temp.y()];
                                destination = temp;
                                path_found = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if( !path_found ) {
        auto e = expansions.find( dir );
        basecamp::update_provides( miss_id.parameters, e->second );

        popup( _( "This functionality cannot be constructed as no valid path to a swamp has "
                  "been found with a maximum length (20 tiles) at a maximum range of 10 tiles.  "
                  "Supported terrain is forest, field, and road.  This recipe will now be "
                  "removed from the set of available recipes and won't show up again." ) );
        return;
    };

    point_rel_omt candidate;
    //  Flip the sign of the starting swamp tile to fit the logic expecting positive values rather than check that it isn't the first one every time.
    path_map[max_salt_water_pipe_distance + destination.x()][max_salt_water_pipe_distance +
            destination.y()]
        = -path_map[max_salt_water_pipe_distance + destination.x()][max_salt_water_pipe_distance +
                destination.y()];

    while( destination != point_rel_omt::zero ) {
        pipe->segments.push_back( { tripoint_abs_omt( omt_pos.x() + dir.x() + connection_dir.x() + destination.x(), omt_pos.y() + dir.y() + connection_dir.y() + destination.y(), omt_pos.z() ), false, false} );
        path_found = false;  //  Reuse of existing variable after its original usability has been passed.
        for( int i = -1; i <= 1; i++ ) {
            for( int k = -1; k <= 1; k++ ) {
                if( destination.x() + i > -max_salt_water_pipe_distance &&
                    destination.x() + i < max_salt_water_pipe_distance &&
                    destination.y() + k > -max_salt_water_pipe_distance &&
                    destination.y() + k < max_salt_water_pipe_distance ) {
                    if( path_map[max_salt_water_pipe_distance + destination.x() + i][max_salt_water_pipe_distance +
                            destination.y() + k] > 0.0 &&
                        path_map[max_salt_water_pipe_distance + destination.x() + i][max_salt_water_pipe_distance +
                                destination.y() + k] < path_map[max_salt_water_pipe_distance +
                                        destination.x()][max_salt_water_pipe_distance + destination.y()] ) {
                        if( path_found ) {
                            if( path_map [max_salt_water_pipe_distance + candidate.x()][max_salt_water_pipe_distance +
                                    candidate.y()] >
                                path_map[max_salt_water_pipe_distance + destination.x() + i][max_salt_water_pipe_distance +
                                        destination.y() + k] ) {
                                candidate = destination + point( i, k );
                            }
                        } else {
                            candidate = destination +  point( i, k );
                            path_found = true;
                        }
                    }
                }
            }
        }
        destination = candidate;
    }

    pipe->segments.push_back( { tripoint_abs_omt( omt_pos.x() + dir.x() + connection_dir.x(), omt_pos.y() + dir.y() + connection_dir.y(), omt_pos.z() ), false, false} );

    if( common_salt_water_pipe_construction( miss_id, pipe, 0 ) ) {
        pipe->segments[0].started = true;
        salt_water_pipes.push_back( pipe );

        //  Provide "salt_water_pipe_*_scheduled" for all the segments needed.
        //  The guts of basecamp::update_provides modified to feed it generated tokens rather than
        //  those actually in the recipe, as the tokens needed can't be determined by the recipe.
        //  Shouldn't need to check that the tokens don't exist previously, so could just set them to 1.
        auto e = expansions.find( dir );
        for( size_t i = 1; i < pipe->segments.size(); i++ ) {
            std::string token = salt_water_pipe_string_base;
            token += std::to_string( i );
            token += salt_water_pipe_string_suffix;
            if( e->second.provides.find( token ) == e->second.provides.end() ) {
                e->second.provides[token] = 0;
            }
            e->second.provides[token]++;
        }
    } else {
        delete pipe;
    }
}

void basecamp::continue_salt_water_pipe( const mission_id &miss_id )
{
    const point_rel_omt dir = miss_id.dir.value();  //  Dir will always have a value
    const recipe &making = recipe_id( miss_id.parameters ).obj();

    expansion_salt_water_pipe *pipe = nullptr;

    bool pipe_is_new = true;
    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            pipe = element;
            pipe_is_new = false;
            break;
        }
    }

    if( pipe_is_new ) {
        debugmsg( "Trying to continue construction of a salt water pipe that isn't in progress" );
        return;
    }

    const int segment_number = salt_water_pipe_segment_of( making );

    if( segment_number == -1 ) {
        return;
    }

    if( pipe->segments[segment_number].finished ) {
        debugmsg( "Trying to construct a segment that's already constructed" );
        return;
    }

    common_salt_water_pipe_construction( miss_id, pipe, segment_number );
}

void basecamp::start_combat_mission( const mission_id &miss_id, float exertion_level )
{
    popup( _( "Select checkpoints until you reach maximum range or select the last point again "
              "to end.  Cancel to undo a selection." ) );
    tripoint_abs_omt start = omt_pos;
    const pf::simple_path<tripoint_abs_omt> &scout_path = om_companion_path( start, 90,
            true );
    const std::vector<tripoint_abs_omt> &scout_points = scout_path.points;
    if( scout_points.empty() ) {
        return;
    }
    int trips = 2;
    if( scout_points.front() == start ) {
        trips = 1;
    }
    const time_duration travel_time = companion_travel_time_calc( scout_path, trips );
    const time_duration total_time = base_camps::to_workdays( travel_time );
    // on this mission, all time is travel, but different exertion level for scout vs combat,
    // so we use work time calculation instead of travel time to avoid the constant travel exertion
    const int need_food = time_to_food( total_time, exertion_level );
    if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time, 0_hours,
                   travel_time, scout_path.dist, trips, need_food ) ) ) {
        return;
    }
    npc_ptr comp = start_mission( miss_id, travel_time, true, _( "departs on patrol…" ),
                                  false, {}, skill_survival, 3, exertion_level );
    if( comp != nullptr ) {
        comp->companion_mission_points = scout_points;
    }
}

void basecamp::start_crafting( const mission_id &miss_id )
{
    if( assigned_npcs.empty() ) {
        popup( _( "There's nobody assigned to work at the camp!" ) );
        return;
    }
    int num_to_make = 1;
    npc dummy;
    for( const recipe_id &some_known_recipe : recipe_deck_all() ) {
        dummy.learn_recipe( &*some_known_recipe, true );
    }
    for( const npc_ptr &guy : assigned_npcs ) {
        if( guy.get() ) {
            // give the dummy a combination of all their skills
            for( auto &skill_level_pair : guy->get_all_skills() ) {
                if( skill_level_pair.second.level() > dummy.get_skill_level( skill_level_pair.first ) ) {
                    dummy.set_skill_level( skill_level_pair.first, skill_level_pair.second.level() );
                }
            }
            // and their proficiencies
            for( auto &some_proficiency : guy->known_proficiencies() ) {
                dummy.add_proficiency( some_proficiency, true, true );
            }
        }
    }

    // Force dummy to always be able to see for crafting. We don't care if it's dark out.
    dummy.add_effect( effect_HACK_camp_vision_for_npc, 1_turns, true );

    validate_sort_points();
    form_storage_zones( get_camp_map(), bb_pos );
    form_crafting_inventory( get_camp_map() );

    std::pair<Character *, const recipe *> crafter_recipe_pair = select_crafter_and_crafting_recipe(
                num_to_make, recipe_id(), &dummy, "", true, &_inv );
    if( !crafter_recipe_pair.second ) {
        return; // player aborted crafting
    }
    const recipe *making = crafter_recipe_pair.second;

    // validates if we have anywhere to place liquid
    form_storage_zones( get_camp_map(), bb_pos );

    if( making->result()->phase != phase_id::SOLID && get_liquid_dumping_spot().empty() ) {
        // we do a little shuffling to minimize unique strings to translate
        std::string query_msg = string_format(
                                    _( "You don't have anything in which to store %s and may have to pour it out as soon as it is prepared!  Proceed?" ),
                                    making->result()->nname( num_to_make ) );
        query_msg += "\n\n";
        query_msg +=
            _( "Eligible locations must be a terrain OR furniture (not item) that can contain liquid, and does not already have any items on its tile." );
        if( !query_yn( query_msg ) ) {
            return;
        }
    }

    struct crafting_worker_choice {
        npc_ptr worker;
        bool can_be_picked = false;
        time_duration duration = 0_turns;
        std::string detail;
    };

    std::vector<crafting_worker_choice> worker_choices;
    worker_choices.reserve( assigned_npcs.size() );
    for( const npc_ptr &guy : assigned_npcs ) {
        if( guy == nullptr ) {
            continue;
        }

        std::string refusal_reason;
        const bool can_be_picked = can_assign_crafting_worker( *guy, *making, true, &refusal_reason );
        crafting_worker_choice choice;
        choice.worker = guy;
        choice.can_be_picked = can_be_picked;
        if( can_be_picked ) {
            choice.duration = base_camps::to_workdays( making->batch_duration( *guy, num_to_make ) );
            choice.detail = to_string( choice.duration );
        } else {
            choice.detail = string_format( "<color_red>%s</color>", refusal_reason );
        }
        worker_choices.push_back( std::move( choice ) );
    }

    std::sort( worker_choices.begin(), worker_choices.end(), []( const crafting_worker_choice & lhs,
    const crafting_worker_choice & rhs ) {
        if( lhs.can_be_picked != rhs.can_be_picked ) {
            return lhs.can_be_picked > rhs.can_be_picked;
        }
        if( lhs.can_be_picked && lhs.duration != rhs.duration ) {
            return lhs.duration < rhs.duration;
        }
        return lhs.worker->disp_name() < rhs.worker->disp_name();
    } );

    if( worker_choices.empty() ) {
        return;
    }

    uilist choose_crafter;
    choose_crafter.title = _( "Choose a companion to craft" );
    int i = 0;
    for( const crafting_worker_choice &choice : worker_choices ) {
        choose_crafter.addentry_col( i++, choice.can_be_picked, input_event(),
                                     choice.worker->disp_name(), choice.detail );
    }

    choose_crafter.query();

    if( choose_crafter.ret < 0 || static_cast<size_t>( choose_crafter.ret ) >= worker_choices.size() ) {
        return; // player aborted selection
    }

    npc_ptr guy_to_send = worker_choices[choose_crafter.ret].worker;

    uilist handling_menu;
    handling_menu.title = _( "Craft order handling" );
    handling_menu.text = string_format( _( "%1$s is ready to tackle %2$d × %3$s." ),
                                        guy_to_send->disp_name(), num_to_make, making->result_name() );
    handling_menu.addentry( 0, true, 's', _( "Start now" ) );
    handling_menu.addentry( 1, true, 'q', _( "Queue as a crafting request" ) );
    handling_menu.addentry( 2, true, 'b', _( "Back" ) );
    handling_menu.query();

    if( handling_menu.ret == 1 ) {
        queue_crafting_request( *making, num_to_make, *guy_to_send );
        popup( _( "Pinned that crafting request to the board for later approval." ) );
        return;
    }
    if( handling_menu.ret != 0 ) {
        return;
    }

    mapgen_arguments arg;  //  Created with a default value.
    std::unique_ptr<basecamp_action_components> components =
        std::make_unique<basecamp_action_components>( *making, arg, num_to_make, *this );
    bool have_components = components->choose_components();
    std::vector<std::string> released_tool_notes;
    if( !have_components ) {
        released_tool_notes = release_crafting_tools( *making, arg, num_to_make );
        if( !released_tool_notes.empty() ) {
            components = std::make_unique<basecamp_action_components>( *making, arg, num_to_make, *this );
            have_components = components->choose_components();
        }
    }
    if( !have_components ) {
        return;
    }

    for( const std::string &note : released_tool_notes ) {
        add_msg( m_info, "%s", note );
    }

    time_duration work_days = base_camps::to_workdays( making->batch_duration( *guy_to_send,
                              num_to_make ) );

    int kcal_consumed = time_to_food( work_days, making->exertion_level() );
    int kcal_have = fac()->food_supply().kcal();

    // TODO: Some way to optionally skip this or default cursor selection to "Yes", it could be annoying
    if( !query_yn( _( "This will cost %i kcal (you have %i stored), is that acceptable?" ),
                   kcal_consumed, kcal_have ) ) {
        return; // won't spend food on this
    }

    if( kcal_consumed > kcal_have ) {
        popup( _( "Not enough stored food." ) );
        return;
    }

    // Move this to applying when they return?
    making->apply_all_morale_mods( *guy_to_send );

    // Now that we know the actual thing we're crafting we will properly form our mission_id
    mission_id actual_id = {miss_id.id, making->ident().str(), miss_id.mapgen_args, miss_id.dir };
    npc_ptr comp = start_mission( actual_id, work_days, true,
                                  _( "begins to work…" ), false, {}, making->required_skills, making->exertion_level(),
                                  0_hours, guy_to_send );
    if( comp != nullptr ) {
        components->consume_components();
        item_components used = components->consumed_components();
        for( const item &results : making->create_results( num_to_make, &used ) ) {
            comp->companion_mission_inv.add_item( results );
        }
        for( const item &byproducts : making->create_byproducts( num_to_make ) ) {
            comp->companion_mission_inv.add_item( byproducts );
        }
        add_crafting_request( *making, num_to_make, actual_id, *comp );
    }
}

static bool farm_valid_seed( const item &itm )
{
    return itm.is_seed() && itm.typeId() != itype_marloss_seed && itm.typeId() != itype_fungal_seeds;
}

std::pair<size_t, std::string> basecamp::farm_action( const point_rel_omt &dir, farm_ops op,
        const npc_ptr &comp )
{
    size_t plots_cnt = 0;
    std::string crops;

    const auto e_data = expansions.find( dir );
    const tripoint_abs_omt omt_tgt = e_data->second.pos;

    const auto is_dirtmound = []( const tripoint_omt_ms & pos, tinymap & bay1, tinymap & bay2 ) {
        return ( bay1.ter( pos ) == ter_t_dirtmound ) && ( !bay2.has_furn( pos ) );
    };
    const auto is_unplowed = []( const tripoint_omt_ms & pos, tinymap & farm_map ) {
        const ter_id &farm_ter = farm_map.ter( pos );
        return farm_ter->has_flag( ter_furn_flag::TFLAG_PLOWABLE );
    };

    std::set<std::string> plant_names;
    std::vector<item *> seed_inv;
    if( comp ) {
        seed_inv = comp->companion_mission_inv.items_with( farm_valid_seed );
    }

    // farm_map is what the area actually looks like
    smallmap farm_map;
    farm_map.load( omt_tgt, false );
    // farm_json is what the area should look like according to jsons (loaded on demand)
    std::unique_ptr<small_fake_map> farm_json;
    tripoint_omt_ms mapmin{ 0, 0, omt_tgt.z() };
    tripoint_omt_ms mapmax{ 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() };
    bool done_planting = false;
    Character &player_character = get_player_character();
    map &here = get_map();
    for( const tripoint_omt_ms &pos : farm_map.points_in_rectangle( mapmin, mapmax ) ) {
        if( done_planting ) {
            break;
        }
        switch( op ) {
            case farm_ops::plow: {
                if( !farm_json ) {
                    farm_json = std::make_unique<small_fake_map>();
                    mapgendata dat( omt_tgt, *farm_json->cast_to_map(), 0, calendar::turn, nullptr );
                    std::string omt_id = dat.terrain_type()->get_mapgen_id();
                    if( !run_mapgen_func( omt_id, dat ) ) {
                        debugmsg( "Failed to run mapgen for farm map" );
                        break;
                    }
                    // Add mapgen from expansion upgrades to fake_map
                    for( auto const &provide : e_data->second.provides ) {
                        if( !recipe_id( provide.first ).is_valid() ) {
                            continue;
                        }
                        const recipe &making = *recipe_id( provide.first );
                        const update_mapgen_id update_id = making.get_blueprint();
                        if( !has_update_mapgen_for( update_id ) ) {
                            continue;
                        }
                        bool mirror_horizontal;
                        bool mirror_vertical;
                        int rotation;
                        if( !extract_and_check_orientation_flags( making.ident(),
                                dir,
                                mirror_horizontal,
                                mirror_vertical,
                                rotation,
                                "%s failed to apply orientation flags to the %s upgrade",
                                "farm_action" ) ) {
                            continue;
                        }
                        farm_json->rotate( 4 - rotation );
                        farm_json->mirror( mirror_horizontal, mirror_vertical );
                        const ret_val<void> has_vehicle_collision = run_mapgen_update_func( update_id, dat, false );
                        if( !has_vehicle_collision.success() ) {
                            debugmsg( "farm_action failed to apply the %1$s map update to %2$s, collision with %3$s vehicle/appliance.",
                                      provide.first, omt_id, has_vehicle_collision.str() );
                        }
                        farm_json->rotate( rotation );
                        farm_json->mirror( mirror_horizontal, mirror_vertical );
                    }
                }
                // Needs to be plowed to match json
                if( is_dirtmound( pos, *farm_json, farm_map ) && is_unplowed( pos, farm_map ) ) {
                    plots_cnt += 1;
                    if( comp ) {
                        farm_map.ter_set( pos, ter_t_dirtmound );
                    }
                }
                break;
            }
            case farm_ops::plant:
                if( is_dirtmound( pos, farm_map, farm_map ) ) {
                    plots_cnt += 1;
                    if( comp ) {
                        if( seed_inv.empty() ) {
                            done_planting = true;
                            break;
                        }
                        item *tmp_seed = seed_inv.back();
                        seed_inv.pop_back();
                        std::list<item> used_seed;
                        used_seed.push_back( *tmp_seed );
                        if( tmp_seed->count_by_charges() ) {
                            tmp_seed->charges -= 1;
                            if( tmp_seed->charges > 0 ) {
                                seed_inv.push_back( tmp_seed );
                            }
                        }
                        used_seed.front().set_age( 0_turns );
                        farm_map.add_item_or_charges( pos, used_seed.front() );
                        farm_map.set( pos, ter_t_dirt, furn_f_plant_seed );
                        if( !tmp_seed->count_by_charges() ) {
                            comp->companion_mission_inv.remove_item( tmp_seed );
                        }
                    }
                }
                break;
            case farm_ops::harvest:
                if( farm_map.furn( pos ) == furn_f_plant_harvest ) {
                    // Can't use item_stack::only_item() since there might be fertilizer
                    map_stack items = farm_map.i_at( pos );
                    const map_stack::iterator seed = std::find_if( items.begin(), items.end(), []( const item & it ) {
                        return it.is_seed();
                    } );
                    if( seed != items.end() && farm_valid_seed( *seed ) ) {
                        plots_cnt += 1;
                        if( comp ) {
                            int skillLevel = round( comp->get_skill_level( skill_survival ) );
                            ///\EFFECT_SURVIVAL increases number of plants harvested from a seed
                            int plant_count = rng( skillLevel / 2, skillLevel );
                            plant_count *= farm_map.furn( pos )->plant->harvest_multiplier;
                            plant_count = std::min( std::max( plant_count, 1 ), 12 );
                            int seed_cnt = std::max( 1, rng( plant_count / 4, plant_count / 2 ) );
                            for( item &i : iexamine::get_harvest_items( *seed->type, plant_count,
                                    seed_cnt, true ) ) {
                                here.add_item_or_charges( player_character.pos_bub(), i );
                            }
                            farm_map.i_clear( pos );
                            farm_map.furn_set( pos, furn_str_id::NULL_ID() );
                            farm_map.ter_set( pos, ter_t_dirt );
                        } else {
                            plant_names.insert( item::nname( itype_id( seed->type->seed->fruit_id ) ) );
                        }
                    }
                }
                break;
            default:
                // let the callers handle no op argument
                break;
        }
    }
    if( comp ) {
        farm_map.save();
    }

    int total_c = 0;
    for( const std::string &i : plant_names ) {
        if( total_c < 5 ) {
            crops += "    " + i + "\n";
            total_c++;
        } else if( total_c == 5 ) {
            crops += _( "+ more\n" );
            break;
        }
    }

    return std::make_pair( plots_cnt, crops );
}

void basecamp::start_farm_op( const point_rel_omt &dir, const mission_id &miss_id,
                              float exertion_level )
{
    map &here = get_map();

    farm_ops op = farm_ops::plow;
    if( miss_id.id == Camp_Plow ) {
        op = farm_ops::plow;
    } else if( miss_id.id == Camp_Plant ) {
        op = farm_ops::plant;
    } else if( miss_id.id == Camp_Harvest ) {
        op = farm_ops::harvest;
    } else {
        debugmsg( "Farm operations called with no matching operation" );
        return;
    }

    std::pair<size_t, std::string> farm_data = farm_action( dir, op );
    size_t plots_cnt = farm_data.first;
    if( !plots_cnt ) {
        return;
    }

    time_duration work = 0_minutes;
    switch( op ) {
        case farm_ops::harvest:
            work += 3_minutes * plots_cnt;
            start_mission( miss_id, work, true,
                           _( "begins to harvest the field…" ), false, {}, skill_survival, 1, exertion_level );
            break;
        case farm_ops::plant: {
            inventory_filter_preset seed_filter( []( const item_location & loc ) {
                return loc->is_seed() && loc->typeId() != itype_marloss_seed && loc->typeId() != itype_fungal_seeds;
            } );
            drop_locations seed_selection = give_basecamp_equipment( seed_filter,
                                            _( "Which seeds do you wish to have planted?" ), _( "Selected seeds" ),
                                            _( "You have no additional seeds to give your companions…" ) );
            if( seed_selection.empty() ) {
                return;
            }
            size_t seed_cnt = 0;
            std::vector<item *> to_plant;
            for( std::pair<item_location, int> &seeds : seed_selection ) {
                size_t num_seeds = seeds.second;
                item_location seed = seeds.first;
                seed.overflow( here );
                if( seed->count_by_charges() ) {
                    seed->charges = num_seeds;
                }
                to_plant.push_back( &*seed );
                seed_cnt += num_seeds;
            }

            if( !seed_cnt ) {
                return;
            }
            work += 1_minutes * seed_cnt;
            start_mission( miss_id, work, true,
                           _( "begins planting the field…" ), false, to_plant,
                           skill_survival, 1, exertion_level );
            break;
        }
        case farm_ops::plow:
            work += 5_minutes * plots_cnt;
            start_mission( miss_id, work, true, _( "begins plowing the field…" ), false, {}, {},
                           exertion_level );
            break;
        default:
            debugmsg( "Farm operations called with no operation" );
    }
}
// camp faction companion mission recovery functions
npc_ptr basecamp::companion_choose_return( const mission_id &miss_id,
        time_duration min_duration )
{
    return talk_function::companion_choose_return( omt_pos, base_camps::id, miss_id,
            calendar::turn - min_duration );
}

npc_ptr basecamp::companion_crafting_choose_return( const mission_id &miss_id )
{
    comp_list preliminary_npc_list = get_mission_workers( miss_id, true );
    comp_list npc_list;
    std::map<std::string, comp_list> lists = companion_per_recipe_building_type( preliminary_npc_list );
    const std::string bldg = recipe_group::get_building_of_recipe( miss_id.parameters );

    for( const npc_ptr &comp : lists[bldg] ) {
        if( comp->companion_mission_time_ret < calendar::turn ) {
            npc_list.emplace_back( comp );
        }
    }

    return talk_function::companion_choose_return( npc_list );
}

void basecamp::finish_return( npc &comp, const bool fixed_time, const std::string &return_msg,
                              const std::string &skill, int difficulty, const bool cancel )
{
    popup( "%s %s", comp.get_name(), return_msg );
    // this is the time the mission was expected to take, or did take for fixed time missions
    time_duration reserve_time = comp.companion_mission_time_ret - comp.companion_mission_time;
    time_duration mission_time = reserve_time;
    if( !fixed_time ) {
        mission_time = calendar::turn - comp.companion_mission_time;
    }
    if( !cancel ) {
        talk_function::companion_skill_trainer( comp, skill, mission_time, difficulty );
    }

    // Missions that are not fixed_time pay their food costs at the end, instead of up-front.
    int need_food = 0;
    if( !fixed_time ) {
        // Assume the travel time is constant and any unplanned time was non-travel
        need_food = time_to_food( mission_time, comp.companion_mission_exertion,
                                  comp.companion_mission_travel_time );
    }
    if( fac()->food_supply().kcal() < need_food ) {
        popup( _( "Your companion seems disappointed that your pantry is empty…" ) );
    }
    // movng all the logic from talk_function::companion return here instead of polluting
    // mission_companion
    comp.reset_companion_mission();
    comp.companion_mission_time = calendar::before_time_starts;
    comp.companion_mission_time_ret = calendar::before_time_starts;
    if( !cancel ) {
        for( size_t i = 0; i < comp.companion_mission_inv.size(); i++ ) {
            for( const item &it : comp.companion_mission_inv.const_stack( i ) ) {
                if( !it.count_by_charges() || it.charges > 0 ) {
                    place_results( it );
                }
            }
        }
    }
    comp.companion_mission_inv.clear();
    comp.companion_mission_points.clear();
    // npc *may* be active, or not if outside the reality bubble
    g->reload_npcs();
    validate_assignees();

    // Missions that are not fixed_time can try to draw more food than is in the food supply
    feed_workers( comp, camp_food_supply( -need_food ) );
    if( has_water() ) {
        comp.set_thirst( 0 );
    }
    comp.set_sleepiness( 0 );
    comp.set_sleep_deprivation( 0 );
}

npc_ptr basecamp::mission_return( const mission_id &miss_id, time_duration min_duration,
                                  bool fixed_time, const std::string &return_msg,
                                  const std::string &skill, int difficulty )
{
    npc_ptr comp = companion_choose_return( miss_id, min_duration );
    if( comp != nullptr ) {
        finish_return( *comp, fixed_time, return_msg, skill, difficulty );
    }
    return comp;
}

npc_ptr basecamp::crafting_mission_return( const mission_id &miss_id, const std::string &return_msg,
        const std::string &skill, int difficulty )
{
    npc_ptr comp = companion_crafting_choose_return( miss_id );
    if( comp != nullptr ) {
        finish_return( *comp, false, return_msg, skill, difficulty );
        complete_crafting_request( miss_id, *comp,
                                   string_format( _( "%s returned with the finished craft order." ),
                                           comp->disp_name() ) );
    }
    return comp;
}

npc_ptr basecamp::emergency_recall( const mission_id &miss_id )
{
    npc_ptr comp = talk_function::companion_choose_return( omt_pos, base_camps::id, miss_id,
                   calendar::turn - 24_hours, false );
    if( comp != nullptr ) {
        const mission_id active_mission = comp->get_companion_mission().miss_id;

        //  Special handing for camp upgrades. If multiple companions are assigned, the remaining time
        //  is divided between the remaining workers. Note that this logic relies on there being only
        //  a single instance of each construction active, and thus all workers assigned to that
        //  blueprint are on the same mission. Won't work with e.g. crafting, as different instances of
        //  the same crafting mission may be in various stages of completion concurrently.
        if( comp->get_companion_mission().miss_id.id == Camp_Upgrade ) {
            comp_list npc_list = get_mission_workers( comp->get_companion_mission().miss_id );

            if( npc_list.size() > 1 ) {
                time_duration remaining_time = comp->companion_mission_time_ret - calendar::turn;
                if( remaining_time > time_duration::from_turns( 0 ) ) {
                    remaining_time = remaining_time * npc_list.size() / ( npc_list.size() - 1 );
                }

                for( npc_ptr &worker : npc_list ) {
                    if( worker != comp ) {
                        worker->companion_mission_time_ret = calendar::turn + remaining_time;
                    }
                }
            }
        }

        const std::string return_msg = _( "responds to the emergency recall…" );
        // FIXME this might charge food for someone who was already fed?
        finish_return( *comp, false, return_msg, skill_menial.str(), 0, true );
        if( active_mission.id == Camp_Crafting ) {
            cancel_crafting_request( active_mission, *comp,
                                     string_format( _( "%s was recalled before the craft order was finished." ),
                                             comp->disp_name() ) );
        }
    }
    return comp;

}

bool basecamp::upgrade_return( const mission_id &miss_id )
{
    const point_rel_omt dir = miss_id.dir.value();  //  Will always have a value
    const std::string bldg = miss_id.parameters.empty() ? next_upgrade( dir, 1 ) : miss_id.parameters;
    if( bldg == "null" ) {
        return false;
    }

    auto e = expansions.find( dir );
    if( e == expansions.end() ) {
        return false;
    }
    const tripoint_abs_omt upos = e->second.pos;
    const recipe &making = *recipe_id( bldg );

    comp_list npc_list = get_mission_workers( miss_id );
    if( npc_list.empty() ) {
        return false;
    }

    std::string companion_list;
    for( const npc_ptr &comp : npc_list ) {
        if( comp != npc_list[0] ) {
            companion_list += ", ";
        }
        companion_list += comp->disp_name();
    }

    bool mirror_horizontal;
    bool mirror_vertical;
    int rotation;

    if( !extract_and_check_orientation_flags( making.ident(),
            dir,
            mirror_horizontal,
            mirror_vertical,
            rotation,
            "%s failed to build the %s upgrade",
            companion_list ) ) {
        return false;
    }

    if( making.get_blueprint() == update_mapgen_faction_expansion_salt_water_pipe_swamp_N ) {
        return salt_water_pipe_swamp_return( miss_id, npc_list );
    } else if( making.get_blueprint() == update_mapgen_faction_expansion_salt_water_pipe_N ) {
        return salt_water_pipe_return( miss_id, npc_list );
    }

    const ret_val<void> has_vehicle_collision = run_mapgen_update_func( making.get_blueprint(), upos,
            miss_id.mapgen_args, nullptr, true,
            mirror_horizontal, mirror_vertical, rotation );
    if( !has_vehicle_collision.success() ) {
        popup( _( "%1$s failed to build the %2$s upgrade, %3$s vehicle/appliance seems to be in the way." ),
               companion_list,
               making.get_blueprint().str(),
               has_vehicle_collision.str() );
        return false;
    }

    apply_construction_marker( making.get_blueprint(), upos,
                               miss_id.mapgen_args, mirror_horizontal,
                               mirror_vertical, rotation, false );

    update_provides( bldg, e->second );
    update_resources( bldg );
    if( oter_str_id( bldg ).is_valid() ) {
        overmap_buffer.ter_set( upos, oter_id( bldg ) );
    }
    const std::string msg = _( "returns from upgrading the camp having earned a bit of "
                               "experience…" );

    for( const npc_ptr &comp : npc_list ) {
        finish_return( *comp, false, msg, "construction", making.difficulty );
    }

    return true;
}

bool basecamp::menial_return( const mission_id &miss_id )
{
    const std::string msg = _( "returns from doing the dirty work to keep the camp running…" );
    npc_ptr comp = mission_return( miss_id,
                                   3_hours, true, msg, skill_menial.str(), 2 );
    if( comp == nullptr ) {
        return false;
    }
    comp->revert_after_activity();
    return true;
}

bool basecamp::gathering_return( const mission_id &miss_id, time_duration min_time )
{
    npc_ptr comp = companion_choose_return( miss_id, min_time );
    if( comp == nullptr ) {
        return false;
    }

    std::string task_description = _( "gathering materials" );
    int danger = 20;
    int favor = 2;
    int threat = 10;
    std::string skill_group = "gathering";
    float skill = 2 * comp->get_skill_level( skill_survival ) + comp->get_per();
    int checks_per_cycle = 6;
    if( miss_id.id == Camp_Foraging ) {
        task_description = _( "foraging for edible plants" );
        danger = 15;
        checks_per_cycle = 12;
    } else if( miss_id.id == Camp_Trapping ) {
        task_description = _( "trapping small animals" );
        favor = 1;
        danger = 15;
        skill_group = "trapping";
        skill = 2 * comp->get_skill_level( skill_traps ) + comp->get_per();
        checks_per_cycle = 4;
    } else if( miss_id.id == Camp_Hunting ) {
        task_description = _( "hunting for meat" );
        danger = 10;
        favor = 0;
        skill_group = "hunting";
        skill = 1.5 * comp->get_skill_level( skill_gun ) + comp->get_per() / 2.0;
        threat = 12;
        checks_per_cycle = 2;
    }

    time_duration mission_time = calendar::turn - comp->companion_mission_time;
    if( one_in( danger ) && !survive_random_encounter( *comp, task_description, favor, threat ) ) {
        return false;
    }
    const std::string msg = string_format( _( "returns from %s carrying supplies and has a bit "
                                           "more experience…" ), task_description );
    finish_return( *comp, false, msg, skill_group, 1 );

    item_group_id itemlist( "forest" );
    if( miss_id.id == Camp_Collect_Firewood ) {
        itemlist = Item_spawn_data_gathering_faction_camp_firewood;
    } else if( miss_id.id == Camp_Gather_Materials ) {
        itemlist = Item_spawn_data_forest;
    } else if( miss_id.id == Camp_Foraging ) {
        switch( season_of_year( calendar::turn ) ) {
            case SPRING:
                itemlist = Item_spawn_data_foraging_faction_camp_spring;
                break;
            case SUMMER:
                itemlist = Item_spawn_data_foraging_faction_camp_summer;
                break;
            case AUTUMN:
                itemlist = Item_spawn_data_foraging_faction_camp_autumn;
                break;
            case WINTER:
                itemlist = Item_spawn_data_foraging_faction_camp_winter;
                break;
            default:
                debugmsg( "Invalid season" );
        }
    }
    if( miss_id.id == Camp_Trapping ||
        miss_id.id == Camp_Hunting ) {
        hunting_results( round( skill ), miss_id, checks_per_cycle * mission_time / min_time, 30 );
    } else {
        search_results( round( skill ), itemlist, checks_per_cycle * mission_time / min_time, 15 );
    }

    return true;
}

void basecamp::fortifications_return( const mission_id &miss_id )
{
    npc_ptr comp = companion_choose_return( miss_id, 3_hours );
    if( comp != nullptr ) {
        std::vector<tripoint_abs_omt> &build_point = comp->companion_mission_points;
        for( std::vector<tripoint_abs_omt>::iterator::value_type point : build_point ) {
            if( miss_id.parameters == faction_wall_level_n_0_string ||
                //  Handling of old format (changed mid 0.F) below
                ( miss_id.parameters.empty() &&
                  comp->companion_mission_role_id == faction_wall_level_n_0_string ) ) {
                tripoint_abs_omt fort_point = point;
                fortifications.push_back( fort_point );
            }
        }

        apply_fortifications( miss_id, &comp, false );

        const std::string msg = _( "returns from constructing fortifications…" );
        finish_return( *comp, true, msg, skill_construction.str(), 2 );
    }
}

bool basecamp::salt_water_pipe_swamp_return( const mission_id &miss_id,
        const comp_list &npc_list )
{
    const point_rel_omt dir = miss_id.dir.value();  //  Will always have a value

    expansion_salt_water_pipe *pipe = nullptr;

    bool found = false;
    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            pipe = element;
            found = true;
            break;
        }
    }

    if( !found ) {
        popup( _( "Error: Failed to find the pipe task that was to be constructed" ) );
        return false;
    }

    point_rel_omt connection_dir = pipe->connection_direction;
    const int segment_number = 0;

    point_rel_omt next_construction_direction;

    if( segment_number == pipe->segments.size() - 1 ) {
        next_construction_direction = { -connection_dir.x(), -connection_dir.y()};
    } else {
        next_construction_direction = { pipe->segments[segment_number + 1].point.x() - pipe->segments[segment_number].point.x(),
                                        pipe->segments[segment_number + 1].point.y() - pipe->segments[segment_number].point.y()
                                      };
    }

    bool orthogonal = true;
    bool mirror_horizontal = false;
    bool mirror_vertical = false;
    int rotation = 0;

    salt_water_pipe_orientation_adjustment( next_construction_direction, orthogonal, mirror_vertical,
                                            mirror_horizontal, rotation );

    if( orthogonal ) {
        run_mapgen_update_func( update_mapgen_faction_expansion_salt_water_pipe_swamp_N,
                                pipe->segments[segment_number].point, {}, nullptr, true,
                                mirror_horizontal, mirror_vertical, rotation );
        apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_swamp_N,
                                   pipe->segments[segment_number].point,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, false );
    } else {
        run_mapgen_update_func( update_mapgen_faction_expansion_salt_water_pipe_swamp_NE,
                                pipe->segments[segment_number].point, {}, nullptr, true,
                                mirror_horizontal, mirror_vertical, rotation );
        apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_swamp_NE,
                                   pipe->segments[segment_number].point,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, false );
    }

    pipe->segments[segment_number].finished = true;

    auto e = expansions.find( dir );
    //  Should be safe as the caller has already checked the result. Repeating rather than adding an additional parameter to the function.

    size_t finished_segments = 0;
    for( const std::vector<expansion_salt_water_pipe_segment>::iterator::value_type &element :
         pipe->segments ) {
        if( element.finished ) {
            finished_segments++;
        }
    }

    //  This is the last segment, so we can now allow the salt water pump to be constructed.
    if( finished_segments == pipe->segments.size() ) {
        const std::string token = salt_water_pipe_string_base + "0" + salt_water_pipe_string_suffix;
        if( e->second.provides.find( token ) == e->second.provides.end() ) {
            e->second.provides[token] = 0;
        }
        e->second.provides[token]++;
    }

    update_provides( miss_id.parameters, e->second );
    update_resources( miss_id.parameters );

    for( const npc_ptr &comp : npc_list ) {
        finish_return( *comp, true,
                       _( "returns from construction of the salt water pipe swamp segment…" ), "construction", 2 );
    }

    return true;
}

bool basecamp::salt_water_pipe_return( const mission_id &miss_id,
                                       const comp_list &npc_list )
{
    const recipe &making = recipe_id( miss_id.parameters ).obj();
    const point_rel_omt dir = miss_id.dir.value();  //  Will always have a value

    expansion_salt_water_pipe *pipe = nullptr;

    bool found = false;
    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            pipe = element;
            found = true;
            break;
        }
    }

    if( !found ) {
        popup( _( "Error: Failed to find the pipe task that was to be constructed" ) );
        return false;
    }

    point_rel_omt connection_dir = pipe->connection_direction;
    const int segment_number = salt_water_pipe_segment_of( making );

    if( segment_number == -1 ) {
        return false;
    }

    const point_rel_omt previous_construction_direction = { pipe->segments[segment_number - 1].point.x() - pipe->segments[segment_number].point.x(),
                                                            pipe->segments[segment_number - 1].point.y() - pipe->segments[segment_number].point.y()
                                                          };

    point_rel_omt next_construction_direction;

    if( segment_number == static_cast<int>( pipe->segments.size() - 1 ) ) {
        next_construction_direction = { -connection_dir.x(), -connection_dir.y()};
    } else {
        next_construction_direction = { pipe->segments[segment_number + 1].point.x() - pipe->segments[segment_number].point.x(),
                                        pipe->segments[segment_number + 1].point.y() - pipe->segments[segment_number].point.y()
                                      };
    }

    bool orthogonal = true;
    bool mirror_horizontal = false;
    bool mirror_vertical = false;
    int rotation = 0;

    salt_water_pipe_orientation_adjustment( previous_construction_direction, orthogonal,
                                            mirror_vertical, mirror_horizontal, rotation );

    if( orthogonal ) {
        run_mapgen_update_func( update_mapgen_faction_expansion_salt_water_pipe_N,
                                pipe->segments[segment_number].point, {}, nullptr, true,
                                mirror_horizontal, mirror_vertical, rotation );
        apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_N,
                                   pipe->segments[segment_number].point,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, false );
    } else {
        run_mapgen_update_func( update_mapgen_faction_expansion_salt_water_pipe_NE,
                                pipe->segments[segment_number].point, {}, nullptr, true,
                                mirror_horizontal, mirror_vertical, rotation );
        apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_NE,
                                   pipe->segments[segment_number].point,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, false );
    }

    salt_water_pipe_orientation_adjustment( next_construction_direction, orthogonal, mirror_vertical,
                                            mirror_horizontal, rotation );

    if( orthogonal ) {
        run_mapgen_update_func( update_mapgen_faction_expansion_salt_water_pipe_N,
                                pipe->segments[segment_number].point, {}, nullptr, true,
                                mirror_horizontal, mirror_vertical, rotation );
        apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_N,
                                   pipe->segments[segment_number].point,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, false );
    } else {
        run_mapgen_update_func( update_mapgen_faction_expansion_salt_water_pipe_NE,
                                pipe->segments[segment_number].point, {}, nullptr, true,
                                mirror_horizontal, mirror_vertical, rotation );
        apply_construction_marker( update_mapgen_faction_expansion_salt_water_pipe_NE,
                                   pipe->segments[segment_number].point,
                                   miss_id.mapgen_args, mirror_horizontal,
                                   mirror_vertical, rotation, false );
    }

    pipe->segments[segment_number].finished = true;

    auto e = expansions.find( dir );
    //  Should be safe as the caller has already checked the result. Repeating rather than adding an additional parameter to the function.

    size_t finished_segments = 0;
    for( const std::vector<expansion_salt_water_pipe_segment>::iterator::value_type &element :
         pipe->segments ) {
        if( element.finished ) {
            finished_segments++;
        }
    }

    //  This is the last segment, so we can now allow the salt water pump to be constructed.
    if( finished_segments == pipe->segments.size() ) {
        const std::string token = salt_water_pipe_string_base + "0" + salt_water_pipe_string_suffix;
        if( e->second.provides.find( token ) == e->second.provides.end() ) {
            e->second.provides[token] = 0;
        }
        e->second.provides[token]++;
    }

    update_provides( miss_id.parameters, e->second );
    update_resources( miss_id.parameters );

    for( const npc_ptr &comp : npc_list ) {
        finish_return( *comp, true, _( "returns from construction of a salt water pipe segment…" ),
                       "construction", 2 );
    }

    return true;
}

void basecamp::recruit_return( const mission_id &miss_id, int score )
{
    const std::string msg = _( "returns from searching for recruits with "
                               "a bit more experience…" );
    npc_ptr comp = mission_return( miss_id, 4_days, true, msg, skill_recruiting.str(), 2 );
    if( comp == nullptr ) {
        return;
    }

    npc_ptr recruit;
    //Success of finding an NPC to recruit, based on survival/tracking
    float skill = comp->get_skill_level( skill_survival );
    if( rng( 1, 20 ) + skill > 17 ) {
        recruit = make_shared_fast<npc>();
        recruit->normalize();
        recruit->randomize();
        popup( _( "%s encountered %s…" ), comp->get_name(), recruit->get_name() );
    } else {
        popup( _( "%s didn't find anyone to recruit…" ), comp->get_name() );
        return;
    }
    //Chance of convincing them to come back
    skill = ( 100 * comp->get_skill_level( skill_speech ) + score ) / 100;
    if( rng( 1, 20 ) + skill  > 19 ) {
        popup( _( "%s convinced %s to hear a recruitment offer from you…" ), comp->get_name(),
               recruit->get_name() );
    } else {
        popup( _( "%s wasn't interested in anything %s had to offer…" ), recruit->get_name(),
               comp->get_name() );
        return;
    }
    //Stat window
    int rec_m = 0;
    int appeal = rng( -5, 3 ) + std::min( skill / 3, 3.0f );
    int food_desire = rng( 0, 5 );
    while( true ) {
        std::string description = _( "NPC Overview:\n\n" );
        description += string_format( _( "Name:  %s\n\n" ), right_justify( recruit->get_name(), 20 ) );
        description += string_format( _( "Strength:        %10d\n" ), recruit->get_str_base() );
        description += string_format( _( "Dexterity:       %10d\n" ), recruit->get_dex_base() );
        description += string_format( _( "Intelligence:    %10d\n" ), recruit->get_int_base() );
        description += string_format( _( "Perception:      %10d\n\n" ), recruit->get_per_base() );
        description += _( "Top 3 Skills:\n" );

        const auto skillslist = Skill::get_skills_sorted_by(
        [&]( const Skill & a, const Skill & b ) {
            const int level_a = recruit->get_skill_level( a.ident() );
            const int level_b = recruit->get_skill_level( b.ident() );
            return localized_compare( std::make_pair( -level_a, a.name() ),
                                      std::make_pair( -level_b, b.name() ) );
        } );

        description += string_format( "%s:          %4d\n", right_justify( skillslist[0]->name(), 12 ),
                                      static_cast<int>( recruit->get_skill_level( skillslist[0]->ident() ) ) );
        description += string_format( "%s:          %4d\n", right_justify( skillslist[1]->name(), 12 ),
                                      static_cast<int>( recruit->get_skill_level( skillslist[1]->ident() ) ) );
        description += string_format( "%s:          %4d\n\n", right_justify( skillslist[2]->name(), 12 ),
                                      static_cast<int>( recruit->get_skill_level( skillslist[2]->ident() ) ) );

        description += _( "Asking for:\n" );
        description += string_format( _( "> Food:     %10d days\n\n" ), food_desire );
        description += string_format( _( "Faction Food:%9d days\n\n" ),
                                      camp_food_supply_days( NO_EXERCISE ) );
        description += string_format( _( "Recruit Chance: %10d%%\n\n" ),
                                      std::min( 100 * ( 10 + appeal ) / 20, 100 ) );
        description += _( "Select an option:" );

        std::vector<std::string> rec_options;
        rec_options.reserve( 4 );
        rec_options.emplace_back( _( "Increase Food" ) );
        rec_options.emplace_back( _( "Decrease Food" ) );
        rec_options.emplace_back( _( "Make Offer" ) );
        rec_options.emplace_back( _( "Not Interested" ) );

        rec_m = uilist( description, rec_options );
        if( rec_m < 0 || rec_m == 3 || static_cast<size_t>( rec_m ) >= rec_options.size() ) {
            popup( _( "You decide you aren't interested…" ) );
            return;
        }

        if( rec_m == 0 && food_desire + 1 <= camp_food_supply_days( NO_EXERCISE ) ) {
            food_desire++;
            appeal++;
        }
        if( rec_m == 1 ) {
            if( food_desire > 0 ) {
                food_desire--;
                appeal--;
            }
        }
        if( rec_m == 2 ) {
            break;
        }
    }
    // Roll for recruitment
    if( rng( 1, 20 ) + appeal >= 10 ) {
        popup( _( "%s has been convinced to join!" ), recruit->get_name() );
    } else {
        popup( _( "%s wasn't interested…" ), recruit->get_name() );
        // nullptr;
        return;
    }
    // Time durations always subtract from camp food supply
    camp_food_supply( 1_days * food_desire );
    avatar &player_character = get_avatar();
    recruit->spawn_at_precise( player_character.pos_abs() + point( -4, -4 ) );
    overmap_buffer.insert_npc( recruit );
    recruit->form_opinion( player_character );
    recruit->mission = NPC_MISSION_NULL;
    recruit->add_new_mission( mission::reserve_random( ORIGIN_ANY_NPC,
                              recruit->pos_abs_omt(),
                              recruit->getID() ) );
    talk_function::follow( *recruit );
    g->load_npcs();
}

void basecamp::combat_mission_return( const mission_id &miss_id )
{
    npc_ptr comp = companion_choose_return( miss_id, 3_hours );
    if( comp != nullptr ) {
        bool patrolling = miss_id.id == Camp_Combat_Patrol;
        comp_list patrol;
        npc_ptr guy = overmap_buffer.find_npc( comp->getID() );
        if( guy ) {
            patrol.push_back( guy );
        }
        for( tripoint_abs_omt &pt : comp->companion_mission_points ) {
            // // Crossing water is at least temporarily avoided due to the introduction of
            // // pathfinding to the mission system.
            // const oter_id &omt_ref = overmap_buffer.ter( pt );
            // int swim = comp->get_skill_level( skill_swimming );
            // if( is_river( omt_ref ) && swim < 2 ) {
            //     if( swim == 0 ) {
            //         popup( _( "Your companion hit a river and didn't know how to swim…" ) );
            //     } else {
            //         popup( _( "Your companion hit a river and didn't know how to swim well "
            //                   "enough to cross…" ) );
            //     }
            //     break;
            // }
            comp->death_drops = false;
            bool outcome = talk_function::companion_om_combat_check( patrol, pt, patrolling );
            comp->death_drops = true;
            if( outcome ) {
                overmap_buffer.reveal( pt, 2 );
            } else if( comp->is_dead() ) {
                popup( _( "%s didn't return from patrol…" ), comp->get_name() );
                comp->place_corpse( pt );
                overmap_buffer.add_note( pt, "DEAD NPC" );
                overmap_buffer.remove_npc( comp->getID() );
                return;
            }
        }
        const std::string msg = _( "returns from patrol…" );
        finish_return( *comp, true, msg, skill_combat.str(), 4 );
    }
}

bool basecamp::survey_field_return( const mission_id &miss_id )
{
    const std::string abort_msg = _( "stops looking for terrain to turn into fields…" );
    npc_ptr comp = companion_choose_return( miss_id, 0_hours );
    if( comp == nullptr ) {
        return false;
    }


    tripoint_abs_omt where( get_player_character().pos_abs_omt() );

    while( true ) {
        where = ui::omap::choose_point( string_format(
                                            _( "Select a tile up to %d tiles away." ), 1 ) );
        if( where.is_invalid() ) {
            return false;
        }

        int dist = rl_dist( where.xy(), omt_pos.xy() );
        if( dist != 1 ) {
            popup( _( "You must select a tile within %d range of the camp" ), 1 );
            continue;
        }
        if( omt_pos.z() != where.z() ) {
            popup( _( "Expansions must be on the same level as the camp" ) );
            continue;
        }
        const point_rel_omt dir = talk_function::om_simple_dir( omt_pos, where );
        if( expansions.find( dir ) != expansions.end() ) {
            if( query_yn(
                    _( "You already have an expansion at that location.  Do you want to finish this mission?  If not, another tile can be checked." ) ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }

        if( overmap_buffer.ter_existing( where ) == oter_field ) {
            if( query_yn(
                    _( "This location is already a field.  Do you want to finish this mission?  If not, another tile can be checked." ) ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }

        tinymap target;
        target.load( where, false );
        int mismatch_tiles = 0;
        const tripoint_omt_ms mapmin{ 0, 0, where.z() };
        const tripoint_omt_ms mapmax = { 2 * SEEX - 1, 2 * SEEY - 1, where.z() };
        const std::unordered_set<ter_str_id> match_terrains = { ter_t_clay, ter_t_dirt, ter_t_dirtmound, ter_t_grass, ter_t_grass_dead, ter_t_grass_golf, ter_t_grass_long, ter_t_grass_tall, ter_t_moss, ter_t_sand };
        for( const tripoint_omt_ms &p : target.points_in_rectangle( mapmin, mapmax ) ) {
            if( match_terrains.find( target.ter( p ).id() ) == match_terrains.end() ) {
                mismatch_tiles++;
            }
        }

        if( mismatch_tiles > 0 ) {
            if( query_yn(
                    _( "This location has %d tiles blocking it from being converted.  Do you want to finish this mission?  If not, another tile can be checked." ),
                    mismatch_tiles ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }

        overmap_buffer.ter_set( where, oter_field );
        if( query_yn(
                _( "This location has now been converted into a field!  Do you want to finish the mission?  If not, another tile can be checked." ) ) ) {
            finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
            return true;
        } else {
            continue;
        }
    }
}

bool basecamp::survey_return( const mission_id &miss_id )
{
    const std::string abort_msg = _( "gives up trying to create an expansion…" );
    npc_ptr comp = companion_choose_return( miss_id, 3_hours );
    if( comp == nullptr ) {
        return false;
    }


    tripoint_abs_omt where( get_player_character().pos_abs_omt() );

    while( true ) {
        where = ui::omap::choose_point( string_format(
                                            _( "Select a tile up to %d tiles away." ), 1 ) );
        if( where.is_invalid() ) {
            return false;
        }

        int dist = rl_dist( where.xy(), omt_pos.xy() );
        if( dist != 1 ) {
            popup( _( "You must select a tile within %d range of the camp" ), 1 );
            continue;
        }
        if( omt_pos.z() != where.z() ) {
            popup( _( "Expansions must be on the same level as the camp" ) );
            continue;
        }
        const point_rel_omt dir = talk_function::om_simple_dir( omt_pos, where );
        if( expansions.find( dir ) != expansions.end() ) {
            if( query_yn(
                    _( "You already have an expansion at that location.  Do you want to finish this mission?  If not, another tile can be checked." ) ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }

        const oter_id &omt_ref = overmap_buffer.ter( where );
        const std::optional<mapgen_arguments> *maybe_args = overmap_buffer.mapgen_args( where );
        const auto &pos_expansions = recipe_group::get_recipes_by_id( "all_faction_base_expansions",
                                     omt_ref, maybe_args );
        if( pos_expansions.empty() ) {
            if( query_yn(
                    _( "You can't build any expansion in a %s.  Do you want to finish this mission?  If not, another tile can be checked." ),
                    omt_ref.id().c_str() ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }

        const recipe_id expansion_type = base_camps::select_camp_option( pos_expansions,
                                         _( "Select an expansion:" ) );

        bool mirror_horizontal;
        bool mirror_vertical;
        int rotation;

        if( expansion_type == recipe_id::NULL_ID() ||
            !extract_and_check_orientation_flags( expansion_type,
                    dir,
                    mirror_horizontal,
                    mirror_vertical,
                    rotation,
                    "%s failed to build the %s expansion",
                    comp->disp_name() ) ) {
            if( query_yn(
                    _( "Do you want to finish this mission?  If not, another tile can be checked." ) ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }

        const ret_val<void> has_vehicle_collision = run_mapgen_update_func( update_mapgen_id(
                    expansion_type.str() ), where, {}, nullptr, true,
                mirror_horizontal, mirror_vertical, rotation );
        if( !has_vehicle_collision.success() ) {
            popup( _( "%1$s failed to add the %2$s expansion, %3$s vehicle/appliance seems to be in the way." ),
                   comp->disp_name(),
                   expansion_type->blueprint_name(),
                   has_vehicle_collision.str() );
            if( query_yn(
                    _( "Do you want to finish this mission?  If not, another tile can be checked, or you can try again after clearing away the obstacle." ) ) ) {
                finish_return( *comp, true, abort_msg, skill_construction.str(), 0 );
                return true;
            } else {
                continue;
            }
        }
        overmap_buffer.ter_set( where, oter_id( expansion_type.str() ) );
        add_expansion( expansion_type.str(), where, dir );
        const std::string msg = _( "returns from surveying for the expansion." );
        finish_return( *comp, true, msg, skill_construction.str(), 2 );
        return true;
    }
}

bool basecamp::farm_return( const mission_id &miss_id, const point_rel_omt &dir )
{
    farm_ops op;
    if( miss_id.id == Camp_Plow ) {
        op = farm_ops::plow;
    } else if( miss_id.id == Camp_Plant ) {
        op = farm_ops::plant;
    } else if( miss_id.id == Camp_Harvest ) {
        op = farm_ops::harvest;
    } else {
        debugmsg( "Farm operations called with no matching operation" );
        return false;
    }

    const std::string msg = _( "returns from working your fields…" );
    npc_ptr comp = talk_function::companion_choose_return( omt_pos, base_camps::id, miss_id,
                   calendar::before_time_starts );
    if( comp == nullptr ) {
        return false;
    }

    farm_action( dir, op, comp );

    Character &player_character = get_player_character();
    //Give any seeds the NPC didn't use back to you.
    for( size_t i = 0; i < comp->companion_mission_inv.size(); i++ ) {
        for( const item &it : comp->companion_mission_inv.const_stack( i ) ) {
            if( it.charges > 0 ) {
                player_character.i_add( it );
            }
        }
    }
    finish_return( *comp, true, msg, skill_survival.str(), 2 );
    return true;
}

// window manipulation
void talk_function::draw_camp_tabs( const catacurses::window &win,
                                    const base_camps::tab_mode cur_tab,
                                    const std::vector<std::vector<mission_entry>> &entries )
{
    werase( win );
    const int width = getmaxx( win );
    mvwhline( win, point( 0, 2 ), BORDER_COLOR, LINE_OXOX, width );

    std::vector<std::string> tabs( base_camps::all_directions.size() );
    for( const auto &direction : base_camps::all_directions ) {
        tabs.at( direction.second.tab_order ) = direction.second.tab_title.translated();
    }
    const int tab_step = 3;
    int tab_space = 1;
    int tab_x = 0;
    for( auto &t : tabs ) {
        bool tab_empty = entries[tab_x + 1].empty();
        draw_subtab( win, tab_space, t, tab_x == cur_tab, false, tab_empty );
        tab_space += tab_step + utf8_width( t );
        tab_x++;
    }
    wnoutrefresh( win );
}

std::string talk_function::name_mission_tabs(
    const tripoint_abs_omt &omt_pos, const std::string &role_id,
    const std::string &cur_title, base_camps::tab_mode cur_tab )
{
    if( role_id != base_camps::id ) {
        return cur_title;
    }
    std::optional<basecamp *> temp_camp = overmap_buffer.find_camp( omt_pos.xy() );
    if( !temp_camp ) {
        return cur_title;
    }
    basecamp *bcp = *temp_camp;
    for( const auto &direction : base_camps::all_directions ) {
        if( cur_tab == direction.second.tab_order ) {
            return bcp->expansion_tab( direction.first );
        }
    }
    return bcp->expansion_tab( base_camps::base_dir );
}

// recipes and craft support functions
int basecamp::recipe_batch_max( const recipe &making ) const
{
    int max_batch = 0;
    const int max_checks = 9;
    for( size_t batch_size = 1000; batch_size > 0; batch_size /= 10 ) {
        for( int iter = 0; iter < max_checks; iter++ ) {
            const time_duration &work_days = base_camps::to_workdays( making.batch_duration(
                                                 get_player_character(), max_batch + batch_size ) );
            int food_req = time_to_food( work_days );
            bool can_make = making.deduped_requirements().can_make_with_inventory(
                                _inv, making.get_component_filter(), max_batch + batch_size );
            if( can_make && fac()->food_supply().kcal() > food_req ) {
                max_batch += batch_size;
            } else {
                break;
            }
        }
    }
    return max_batch;
}

void basecamp::search_results( int skill, const item_group_id &group_id, int attempts,
                               int difficulty )
{
    for( int i = 0; i < attempts; i++ ) {
        if( skill > rng( 0, difficulty ) ) {
            item result = item_group::item_from( group_id, calendar::turn );
            if( !result.is_null() ) {
                place_results( result );
            }
        }
    }
}

void basecamp::hunting_results( int skill, const mission_id &miss_id, int attempts, int difficulty )
{
    // corpses do not exist as discrete items, so we use monster groups instead
    int base_group_chance = GROUP_CAMP_HUNTING->freq_total;
    int mission_specific_chance = 0;
    mongroup_id mission_specific_group;
    if( miss_id.id == Camp_Trapping ) {
        mission_specific_group = GROUP_CAMP_TRAPPING;
        mission_specific_chance += GROUP_CAMP_TRAPPING->freq_total;
    } else if( miss_id.id == Camp_Hunting ) {
        mission_specific_group = GROUP_CAMP_HUNTING_LARGE;
        mission_specific_chance += GROUP_CAMP_HUNTING_LARGE->freq_total;
    }
    const int total_chance = base_group_chance + mission_specific_chance;
    int successful_hunts = 0;
    for( int i = 0; i < attempts; i++ ) {
        if( skill > rng( 0, difficulty ) ) {
            successful_hunts++;
        }
    }

    if( successful_hunts <= 0 ) {
        return;
    }

    int results_from_base_group = 0;
    int results_from_mission_group = 0;
    for( ; successful_hunts > 0; successful_hunts-- ) {
        if( x_in_y( base_group_chance, total_chance ) ) {
            results_from_base_group++;
        } else {
            results_from_mission_group++;
        }
    }

    while( results_from_base_group > 0 ) {
        make_corpse_from_group( MonsterGroupManager::GetResultFromGroup( GROUP_CAMP_HUNTING,
                                &results_from_base_group ) );
    }

    while( results_from_mission_group > 0 ) {
        make_corpse_from_group( MonsterGroupManager::GetResultFromGroup( mission_specific_group,
                                &results_from_mission_group ) );
    }
}

void basecamp::make_corpse_from_group( const std::vector<MonsterGroupResult> &group )
{
    for( const MonsterGroupResult &monster : group ) {
        const mtype_id target = monster.id;
        item result = item::make_corpse( target, calendar::turn, "" );
        if( !result.is_null() ) {
            int num_to_spawn = monster.pack_size;
            do {
                place_results( result );
                num_to_spawn--;
            } while( num_to_spawn > 0 );
        }
    }
}

int om_harvest_ter_est( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t, int chance )
{
    return om_harvest_ter( comp, omt_tgt, t, chance, true, false );
}
int om_harvest_ter_break( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t, int chance )
{
    return om_harvest_ter( comp, omt_tgt, t, chance, false, false );
}
int om_harvest_ter( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t, int chance,
                    bool estimate, bool bring_back )
{
    const ter_t &ter_tgt = t.obj();
    tinymap target_bay;
    target_bay.load( omt_tgt, false );
    // Redundant as long as map operations aren't using get_map() in a transitive call chain. Added for future proofing.
    swap_map swap( *target_bay.cast_to_map() );
    int harvested = 0;
    int total = 0;
    const tripoint_omt_ms mapmin{ 0, 0, omt_tgt.z() };
    const tripoint_omt_ms mapmax{ 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() };
    for( const tripoint_omt_ms &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        if( target_bay.ter( p ) == t && x_in_y( chance, 100 ) ) {
            total++;
            if( estimate ) {
                continue;
            }
            if( bring_back ) {
                const std::optional<map_ter_bash_info> &bash = ter_tgt.bash;
                if( bash ) {
                    for( const item &itm : item_group::items_from( ter_tgt.bash->drop_group,
                            calendar::turn ) ) {
                        comp.companion_mission_inv.push_back( itm );
                    }
                    harvested++;
                    target_bay.ter_set( p, ter_tgt.bash->ter_set );
                }
            }
        }
    }
    target_bay.save();
    if( bring_back ) {
        return harvested;
    }
    return total;
}

int om_cutdown_trees_est( const tripoint_abs_omt &omt_tgt, int chance )
{
    return om_cutdown_trees( omt_tgt, chance, true, false );
}
int om_cutdown_trees_logs( const tripoint_abs_omt &omt_tgt, int chance )
{
    return om_cutdown_trees( omt_tgt, chance, false, true );
}
int om_cutdown_trees_trunks( const tripoint_abs_omt &omt_tgt, int chance )
{
    return om_cutdown_trees( omt_tgt, chance, false, false );
}
int om_cutdown_trees( const tripoint_abs_omt &omt_tgt, int chance, bool estimate,
                      bool force_cut_trunk )
{
    smallmap target_bay;
    // Redundant as long as map operations used aren't using get_map() transitively, but this makes it safe to do so later.
    swap_map swap( *target_bay.cast_to_map() );
    target_bay.load( omt_tgt, false );
    int harvested = 0;
    int total = 0;
    tripoint_omt_ms mapmin{ 0, 0, omt_tgt.z() };
    tripoint_omt_ms mapmax{ 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() + 1 };
    for( const tripoint_omt_ms &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        if( target_bay.ter( p ).obj().has_flag( ter_furn_flag::TFLAG_TREE ) && rng( 0, 100 ) < chance ) {
            total++;
            if( estimate ) {
                continue;
            }
            // get a random number that is either 1 or -1
            point_rel_ms dir( 3 * ( 2 * rng( 0, 1 ) - 1 ) + rng( -1, 1 ), 3 * rng( -1, 1 ) + rng( -1, 1 ) );

            target_bay.cut_down_tree( p, dir );
            target_bay.collapse_at( p, true, true, false );
            harvested++;
        }
    }
    if( estimate ) {
        return total;
    }
    if( !force_cut_trunk ) {
        target_bay.save();
        return harvested;
    }
    // having cut down the trees, cut the trunks into logs
    for( const tripoint_omt_ms &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        const ter_id &t = target_bay.ter( p );
        if( t == ter_t_trunk || t == ter_t_stump ) {
            target_bay.ter_set( p, ter_t_dirt );
            target_bay.spawn_item( p, itype_log, rng( 2, 3 ), 0, calendar::turn );
            harvested++;
        }
    }
    target_bay.save();
    return harvested;
}

mass_volume om_harvest_itm( const npc_ptr &comp, const tripoint_abs_omt &omt_tgt, int chance,
                            bool take )
{
    tinymap target_bay;
    target_bay.load( omt_tgt, false );
    // Redundant as long as map operations aren't using get_map() in a transitive call chain. Added for future proofing.
    swap_map swap( *target_bay.cast_to_map() );
    units::mass harvested_m = 0_gram;
    units::volume harvested_v = 0_ml;
    units::mass total_m = 0_gram;
    units::volume total_v = 0_ml;
    int total_num = 0;
    int harvested_num = 0;
    tripoint_omt_ms mapmin{ 0, 0, omt_tgt.z() };
    tripoint_omt_ms mapmax{ 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() };
    tripoint_range<tripoint_omt_ms> xxx = target_bay.points_in_rectangle( mapmin, mapmax );
    for( const tripoint_omt_ms &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        for( const item &i : target_bay.i_at( p ) ) {
            if( !i.made_of_from_type( phase_id::LIQUID ) ) {
                total_m += i.weight( true );
                total_v += i.volume( true );
                total_num += 1;
                if( take && x_in_y( chance, 100 ) ) {
                    if( comp ) {
                        comp->companion_mission_inv.push_back( i );
                    }
                    harvested_m += i.weight( true );
                    harvested_v += i.volume( true );
                    harvested_num += 1;
                }
            }
        }
        if( take ) {
            target_bay.i_clear( p );
        }
    }
    target_bay.save();
    mass_volume results;
    if( take ) {
        results.wgt = harvested_m;
        results.vol = harvested_v;
        results.count = harvested_num;
    } else {
        results.wgt = total_m;
        results.vol = total_v;
        results.count = total_num;
    }
    return results;
}

tripoint_abs_omt om_target_tile( const tripoint_abs_omt &omt_pos, int min_range, int range,
                                 const std::unordered_set<oter_type_str_id> &possible_om_types, bool must_see,
                                 const tripoint_abs_omt &source, bool bounce, const std::optional<std::string> &message )
{
    bool errors = false;
    const std::unordered_set<oter_type_str_id> bounce_locations = { oter_type_faction_hide_site_0 };

    tripoint_abs_omt where;
    ui::omap::range_mark( omt_pos, range );
    ui::omap::range_mark( omt_pos, min_range, true, "Y;X: MIN RANGE" );
    const std::string &real_message = string_format(
                                          message ? *message : _( "Select a location from %d to %d tiles away." ), min_range, range );
    if( source.is_invalid() ) {
        where = ui::omap::choose_point( real_message );
    } else {
        where = ui::omap::choose_point( real_message, source );
    }
    ui::omap::range_mark( omt_pos, range, false );
    ui::omap::range_mark( omt_pos, min_range, false, "Y;X: MIN RANGE" );

    if( where.is_invalid() ) {
        return where;
    }
    int dist = rl_dist( where.xy(), omt_pos.xy() );
    if( dist > range || dist < min_range ) {
        popup( _( "You must select a target from %d to %d range.  Your selection was range %d." ),
               min_range, range, dist );
        errors = true;
    }

    tripoint_abs_omt omt_tgt = where;

    const oter_type_str_id &omt_ref = overmap_buffer.ter( omt_tgt )->get_type_id();

    if( must_see && overmap_buffer.seen( omt_tgt ) == om_vision_level::unseen ) {
        errors = true;
        popup( _( "You must be able to see the target that you select." ) );
    }

    if( !errors ) {
        for( const oter_type_str_id &pos_om : bounce_locations ) {
            if( bounce && pos_om == omt_ref && range > 5 ) {
                if( query_yn( _( "Do you want to bounce off this location to extend range?" ) ) ) {
                    ui::omap::line_mark( omt_pos, omt_tgt );
                    tripoint_abs_omt dest =
                        om_target_tile( omt_tgt, 2, range * .75, possible_om_types, true, omt_tgt, true );
                    ui::omap::line_mark( omt_pos, omt_tgt, false );
                    return dest;
                }
            }
        }

        if( possible_om_types.empty() || possible_om_types.find( omt_ref ) != possible_om_types.end() ) {
            return omt_tgt;
        }
    }

    return om_target_tile( omt_pos, min_range, range, possible_om_types, must_see, omt_pos );
}

bool om_set_hide_site( npc &comp, const tripoint_abs_omt &omt_tgt,
                       const drop_locations &itms,
                       const drop_locations &itms_rem )
{
    tinymap target_bay;
    target_bay.load( omt_tgt, false );
    // Redundant as long as map operations aren't using get_map() in a transitive call chain. Added for future proofing.
    swap_map swap( *target_bay.cast_to_map() );
    target_bay.ter_set( relay_site_stash, ter_t_improvised_shelter );
    for( drop_location it : itms_rem ) {
        item *i = it.first.get_item();
        item split_item;

        if( i->count() != it.second ) { //  We're not moving the whole stack, and so have to split it.
            split_item = i->split( it.second );
        }

        if( split_item.is_null() ) {
            comp.companion_mission_inv.add_item( *i );
            target_bay.i_rem( relay_site_stash, i );
        } else {
            comp.companion_mission_inv.add_item( split_item );
        }
    }

    for( drop_location it : itms ) {
        item *i = it.first.get_item();
        item split_item;

        if( i->count() != it.second ) { //  We're not moving the whole stack, and so have to split it.
            split_item = i->split( it.second );
        }

        if( split_item.is_null() ) {
            split_item = *i;  // create a copy of the original item, move that, and then destroy the original,
            // as drop_location knows how to do that so we don't have to search the ground and inventory for it.
            target_bay.add_item_or_charges( relay_site_stash, split_item );
            it.first.remove_item();
        } else {
            target_bay.add_item_or_charges( relay_site_stash, split_item );
        }
    }

    target_bay.save();

    overmap_buffer.ter_set( omt_tgt, oter_faction_hide_site_0 );

    overmap_buffer.reveal( omt_tgt.xy(), 3, 0 );
    return true;
}

// path and travel time
static time_duration companion_travel_time_calc( const pf::simple_path<tripoint_abs_omt> &journey,
        int trips )
{
    return journey.cost * trips * 1_seconds ;
}

int om_carry_weight_to_trips( const units::mass &total_mass, const units::volume &total_volume,
                              const units::mass &carry_mass, const units::volume &carry_volume )
{
    // FIXME handle single high volume items correctly
    // character can wield up to weight capacity x4, and walk at reduced speed with that single item
    // hauling may be appropriate
    int trips_m = ( total_mass + carry_mass - 1_milligram ) / carry_mass;
    int trips_v = ( total_volume + carry_volume - 1_ml ) / carry_volume;
    // return based on round trips
    return 2 * std::max( trips_m, trips_v );
}

int om_carry_weight_to_trips( const units::mass &total_mass, const units::volume &total_volume,
                              const npc_ptr &comp )
{
    units::mass max_m = comp ? comp->weight_capacity() - comp->weight_carried() : 60_kilogram;
    //Assume an additional pack will be carried in addition to normal gear
    units::volume sack_v = item( itype_duffelbag ).get_volume_capacity();
    units::volume max_v = comp ? comp->free_space() : sack_v;
    max_v += sack_v;
    return om_carry_weight_to_trips( total_mass, total_volume, max_m, max_v );
}

pf::simple_path<tripoint_abs_omt> om_companion_path( const tripoint_abs_omt &start, int range_start,
        bool bounce )
{
    std::vector<pf::simple_path<tripoint_abs_omt>> scout_segments;
    tripoint_abs_omt last = start;
    int range = range_start;
    int def_range = range_start;
    while( true ) {
        std::optional<std::string> message;
        if( range == 0 ) {
            message = _( "Confirm again to finalize the path, or cancel to undo." );
        }
        tripoint_abs_omt spt = om_target_tile( last, 0, range, {}, false, last, false, message );
        if( spt.is_invalid() ) {
            if( scout_segments.empty() ) {
                return {};
            }
            ui::omap::path_mark( scout_segments.back().points, false );
            range += scout_segments.back().cost / 24;
            scout_segments.pop_back();
            if( scout_segments.empty() ) {
                last = start;
            } else {
                last = scout_segments.back().points.front();
            }
            continue;
        }
        if( last == spt ) {
            break;
        }
        const pf::simple_path<tripoint_abs_omt> &note_pts = overmap_buffer.get_travel_path( last, spt,
                overmap_path_params::for_npc() );
        if( note_pts.points.empty() ) {
            debugmsg( "Got empty travel path during mission planning." );
            continue;
        }
        ui::omap::path_mark( note_pts.points );
        if( note_pts.cost / 24 > range ) {
            ui::omap::choose_point( _( "This path is too far, continue to undo and try again." ), spt );
            ui::omap::path_mark( note_pts.points, false );
            continue;
        }
        scout_segments.emplace_back( note_pts );
        range -= note_pts.cost / 24;
        last = spt;

        const oter_type_str_id &omt_ref = overmap_buffer.ter( last )->get_type_id();

        if( bounce && omt_ref == oter_type_faction_hide_site_0 ) {
            range = def_range * .75;
            def_range = range;
        }
    }
    for( const pf::simple_path<tripoint_abs_omt> &scout_segment : scout_segments ) {
        ui::omap::path_mark( scout_segment.points, false );
    }
    return std::accumulate( scout_segments.begin(), scout_segments.end(),
                            pf::simple_path<tripoint_abs_omt>(), []( pf::simple_path<tripoint_abs_omt> a,
    const pf::simple_path<tripoint_abs_omt> &b ) {
        a.points.insert( a.points.begin(), b.points.begin(), b.points.end() );
        a.dist += b.dist;
        a.cost += b.cost;
        return a;
    }
                          );
}

// camp utility functions
// mission support functions
drop_locations basecamp::give_basecamp_equipment( inventory_filter_preset &preset,
        const std::string &title, const std::string &column_title, const std::string &msg_empty ) const
{
    inventory_multiselector inv_s( get_player_character(), preset, column_title );

    inv_s.add_basecamp_items( *this );
    inv_s.set_title( title );

    if( inv_s.empty() ) {
        popup( std::string( msg_empty ), PF_GET_KEY );
        return {};
    }
    drop_locations selected = inv_s.execute();
    return selected;
}

drop_locations basecamp::give_equipment( Character *pc, const inventory_filter_preset &preset,
        const std::string &msg, const std::string &title, units::volume &total_volume,
        units::mass &total_mass )
{
    auto make_raw_stats = [&total_volume,
                           &total_mass]( const std::vector<std::pair<item_location, int>> &locs
    ) {
        total_volume = 0_ml;
        for( const auto &pair : locs ) {
            total_volume += pair.first->volume( false, true, pair.second );
        }

        total_mass = 0_gram;
        for( const auto &pair : locs ) {
            total_mass += pair.first->weight();
        }

        auto to_string = []( int val ) -> std::string {
            if( val == INT_MAX )
            {
                return pgettext( "short for infinity", "inf" );
            }
            return string_format( "%3d", val );
        };
        using stats = inventory_selector::stats;
        return inventory_selector::convert_stats_to_header_stats( stats{ {
                display_stat( _( "Volume (L)" ), total_volume.value() / 1000, INT_MAX, to_string ),
                display_stat( _( "Weight (kg)" ), total_mass.value() / 1000000, INT_MAX, to_string )
            } } );
    };

    inventory_multiselector inv_s( *pc, preset, msg,
                                   make_raw_stats, /*allow_select_contained =*/ true );

    inv_s.add_character_items( *pc );
    inv_s.add_nearby_items( PICKUP_RANGE );
    inv_s.set_title( title );
    inv_s.set_hint( _( "To select items, type a number before selecting." ) );

    if( inv_s.empty() ) {
        popup( std::string( _( "You have nothing to send." ) ), PF_GET_KEY );
        return {};
    }
    drop_locations selected = inv_s.execute();
    return selected;
}

drop_locations basecamp::get_equipment( tinymap *target_bay, const tripoint_omt_ms &target,
                                        Character *pc,
                                        const inventory_filter_preset &preset,
                                        const std::string &msg, const std::string &title, units::volume &total_volume,
                                        units::mass &total_mass )
{
    auto make_raw_stats = [&total_volume,
                           &total_mass]( const std::vector<std::pair<item_location, int>> &locs
    ) {
        total_volume = 0_ml;
        for( const auto &pair : locs ) {
            total_volume += pair.first->volume( false, true, pair.second );
        }

        total_mass = 0_gram;
        for( const auto &pair : locs ) {
            total_mass += pair.first->weight();
        }

        auto to_string = []( int val ) -> std::string {
            if( val == INT_MAX )
            {
                return pgettext( "short for infinity", "inf" );
            }
            return string_format( "%3d", val );
        };
        using stats = inventory_selector::stats;
        return inventory_selector::convert_stats_to_header_stats( stats{ {
                display_stat( _( "Volume (L)" ), total_volume.value() / 1000, INT_MAX, to_string ),
                display_stat( _( "Weight (kg)" ), total_mass.value() / 1000000, INT_MAX, to_string )
            } } );
    };

    inventory_multiselector inv_s( *pc, preset, msg,
                                   make_raw_stats, /*allow_select_contained =*/ true );

    inv_s.add_remote_map_items( target_bay, target );
    inv_s.set_title( title );
    inv_s.set_hint( _( "To select items, type a number before selecting." ) );

    if( inv_s.empty() ) {
        popup( std::string( _( "You have nothing to retrieve." ) ), PF_GET_KEY );
        return {};
    }
    drop_locations selected = inv_s.execute();
    return selected;
}

bool basecamp::validate_sort_points()
{
    zone_manager &mgr = zone_manager::get_manager();
    map *here = &get_map();
    const tripoint_abs_ms abspos = get_player_character().pos_abs();
    if( !mgr.has_near( zone_type_CAMP_STORAGE, abspos, MAX_VIEW_DISTANCE, get_owner() ) ||
        !mgr.has_near( zone_type_CAMP_FOOD, abspos, MAX_VIEW_DISTANCE, get_owner() ) ) {
        if( query_yn( _( "You do not have sufficient sort zones.  Do you want to add them?" ) ) ) {
            return set_sort_points();
        } else {
            return false;
        }
    } else {
        form_storage_zones( *here, abspos );
    }
    return true;
}

bool basecamp::set_sort_points()
{
    popup( _( "Please create some sorting zones.  You must create a camp food zone, and a camp storage zone." ) );
    zone_manager_ui::display_zone_manager();
    return validate_sort_points();
}

// camp analysis functions
std::vector<std::pair<std::string, tripoint_abs_omt>> talk_function::om_building_region(
            const tripoint_abs_omt &omt_pos, int range, bool purge )
{
    std::vector<std::pair<std::string, tripoint_abs_omt>> om_camp_region;
    for( const tripoint_abs_omt &omt_near_pos : points_in_radius( omt_pos, range ) ) {
        const oter_id &omt_rnear = overmap_buffer.ter( omt_near_pos );
        std::string om_rnear_id = omt_rnear.id().c_str();
        if( !purge || ( om_rnear_id.find( "faction_base_" ) != std::string::npos &&
                        // TODO: this exclusion check can be removed once primitive field camp OMTs have been purged
                        om_rnear_id.find( "faction_base_camp" ) == std::string::npos ) ) {
            om_camp_region.emplace_back( om_rnear_id, omt_near_pos );
        }
    }
    return om_camp_region;
}

point_rel_omt talk_function::om_simple_dir( const tripoint_abs_omt &omt_pos,
        const tripoint_abs_omt &omt_tar )
{
    point_rel_omt diff = omt_tar.xy() - omt_pos.xy();
    return { clamp( diff.x(), -1, 1 ), clamp( diff.y(), -1, 1 ) };
}

// mission descriptions
std::string camp_trip_description( const time_duration &total_time,
                                   const time_duration &working_time,
                                   const time_duration &travel_time, int dist_m, int trips,
                                   int need_food )
{
    std::string entry = "\n";
    //A square is roughly 1 m
    if( dist_m > 1000 ) {
        entry += string_format( _( ">Distance:%15.2f (km)\n" ), dist_m / 1000.0 );
        entry += string_format( _( ">One Way: %15d trips\n" ), trips );
        entry += string_format( _( ">Covered: %15.2f (km)\n" ), dist_m / 1000.0 * trips );
    } else {
        entry += string_format( _( ">Distance:%15d (m)\n" ), dist_m );
        entry += string_format( _( ">One Way: %15d trips\n" ), trips );
        entry += string_format( _( ">Covered: %15d (m)\n" ), dist_m * trips );
    }
    entry += string_format( _( ">Travel:  %s\n" ), right_justify( to_string( travel_time ), 23 ) );
    entry += string_format( _( ">Working: %s\n" ), right_justify( to_string( working_time ), 23 ) );
    entry += "----                   ----\n";
    entry += string_format( _( "Total:    %s\n" ), right_justify( to_string( total_time ), 23 ) );
    entry += string_format( _( "Food:     %15d (kcal)\n\n" ), need_food );
    return entry;
}

std::string basecamp::craft_description( const recipe_id &itm )
{
    const recipe &making = itm.obj();

    std::vector<std::string> component_print_buffer;
    int pane = FULL_SCREEN_WIDTH;
    const requirement_data &req = making.simple_requirements();
    auto tools = req.get_folded_tools_list( pane, c_white, _inv, 1 );
    auto comps = req.get_folded_components_list( pane, c_white, _inv,
                 making.get_component_filter(), 1 );

    component_print_buffer.insert( component_print_buffer.end(), tools.begin(), tools.end() );
    component_print_buffer.insert( component_print_buffer.end(), comps.begin(), comps.end() );

    std::string comp;
    for( auto &elem : component_print_buffer ) {
        str_append( comp, elem, "\n" );
    }
    comp = string_format( _( "Skill used: %s\nDifficulty: %d\n%s\nTime: %s\nCalories per craft: %s\n" ),
                          making.skill_used.obj().name(), making.difficulty, comp,
                          to_string( base_camps::to_workdays( making.batch_duration( get_player_character() ) ) ),
                          time_to_food( base_camps::to_workdays( making.batch_duration( get_player_character() ) ),
                                        itm.obj().exertion_level() ) );
    return comp;
}

int basecamp::recruit_evaluation( int &sbase, int &sexpansions, int &sfaction, int &sbonus ) const
{
    auto e = expansions.find( base_camps::base_dir );
    if( e == expansions.end() ) {
        sbase = 0;
        sexpansions = 0;
        sfaction = 0;
        sbonus = 0;
        return 0;
    }
    sbase = e->second.cur_level * 5;
    sexpansions = expansions.size() * 2;

    //How could we ever starve?
    //More than 5 farms at recruiting base
    int farm = 0;
    for( const point_rel_omt &dir : directions ) {
        if( has_provides( "farming", dir ) ) {
            farm++;
        }
    }
    sfaction = std::min( fac()->food_supply().kcal() / 10000, 10 );
    sfaction += std::min( camp_discipline() / 10, 5 );
    sfaction += std::min( camp_morale() / 10, 5 );

    //Secret or Hidden Bonus
    //Please avoid openly discussing so that there is some mystery to the system
    sbonus = 0;
    if( farm >= 5 ) {
        sbonus += 10;
    }
    //More machine than man
    //Bionics count > 10, respect > 75
    if( get_player_character().get_bionics().size() > 10 && camp_discipline() > 75 ) {
        sbonus += 10;
    }
    //Survival of the fittest
    if( g->get_kill_tracker().npc_kill_count() > 10 ) {
        sbonus += 10;
    }
    return sbase + sexpansions + sfaction + sbonus;
}
int basecamp::recruit_evaluation() const
{
    int sbase;
    int sexpansions;
    int sfaction;
    int sbonus;
    return recruit_evaluation( sbase, sexpansions, sfaction, sbonus );
}

std::string basecamp::recruit_description( int npc_count ) const
{
    int sbase;
    int sexpansions;
    int sfaction;
    int sbonus;
    int total = recruit_evaluation( sbase, sexpansions, sfaction, sbonus );
    std::string desc = string_format( _( "Notes:\n"
                                         "Recruiting additional followers is very dangerous and "
                                         "expensive.  The outcome is heavily dependent on the "
                                         "skill of the companion you send and the appeal of "
                                         "your base.\n\n"
                                         "Skill used: social\n"
                                         "Difficulty: 2\n"
                                         "Base Score:                   +%3d%%\n"
                                         "> Expansion Bonus:            +%3d%%\n"
                                         "> Faction Bonus:              +%3d%%\n"
                                         "> Special Bonus:              +%3d%%\n\n"
                                         "Total: Skill                  +%3d%%\n\n"
                                         "Risk: High\n"
                                         "Time: 4 Days\n"
                                         "Positions: %d/1\n" ), sbase, sexpansions, sfaction,
                                      sbonus, total, npc_count );
    return desc;
}

std::vector<std::string> basecamp::gathering_description() const
{
    item_group_id itemlist = Item_spawn_data_forest;
    std::vector<std::string> output;

    // Functions like the debug item group tester but only rolls 6 times so the player
    // doesn't have perfect knowledge
    std::map<std::string, int> itemnames;
    for( size_t a = 0; a < 6; a++ ) {
        const std::vector<item> items = item_group::items_from( itemlist, calendar::turn );
        for( const item &it : items ) {
            itemnames[it.display_name()]++;
        }
    }
    // Invert the map to get sorting!
    std::multimap<int, std::string> itemnames2;
    for( const auto &e : itemnames ) {
        itemnames2.insert( std::pair<int, std::string>( e.second, e.first ) );
    }
    for( const auto &e : itemnames2 ) {
        output.emplace_back( e.second );
    }
    return output;
}

std::vector<std::string> basecamp::farm_description( const point_rel_omt &dir, size_t &plots_count,
        farm_ops operation )
{
    std::pair<size_t, std::string> farm_data = farm_action( dir, operation );
    plots_count = farm_data.first;
    switch( operation ) {
        case farm_ops::harvest:
            return { _( "Harvestable: " ) + std::to_string( plots_count ), farm_data.second };
        case farm_ops::plant:
            return { _( "Ready for Planting: " ) + std::to_string( plots_count ) };
        case farm_ops::plow:
            return { _( "Needs Plowing: " ) + std::to_string( plots_count ) };
        default:
            debugmsg( "Farm operations called with no operation" );
            return {};
    }
}

// food supply

int basecamp::camp_food_supply_days( float exertion_level ) const
{
    return fac()->food_supply().kcal() / time_to_food( 24_hours, exertion_level );
}

nutrients basecamp::camp_food_supply( nutrients change )
{
    if( !fac()->consumes_food ) {
        return change;
    }
    if( change.calories >= 0 ) {
        debugmsg( "camp_food_supply cannot add to stockpile" );
        return change;
    }
    if( change.vitamins().empty() ) {
        // We've been passed a raw kcal value, we should also consume a proportional amount of vitamins
        // Kcals are used as a proxy to consume vitamins.
        // e.g. if you have a larder with 10k kcal, 100 vitamin A, 200 vitamin B then consuming 1000 kcal will
        // consume 10 vitamin A and *20* vitamin B. In other words, we assume the vitamins are uniformly distributed with the kcals
        // This isn't a perfect assumption but it's a necessary one to abstract away the food items themselves
        double percent_consumed = std::abs( static_cast<double>( change.calories ) ) /
                                  fac()->food_supply().calories;
        change = -fac()->food_supply();
        change *= percent_consumed;
    }

    // change is negative, so flip the sign
    nutrients left = fac()->consume_food_supply( -change );
    if( left.calories >= 0 ) {
        //Whoops, we don't have enough food. Empty the larder! No crumb shall go un-eaten!
        faction *yours = get_player_character().get_faction();
        if( fac()->id == yours->id ) {
            // scaling penalty with faction based on how overrun the larder is
            yours->likes_u -= left.kcal() / 1250;
            yours->respects_u -= left.kcal() / 625;
            yours->trusts_u -= left.kcal() / 625;
        }
    }
    // food consumed
    return change - left;
}

nutrients basecamp::camp_food_supply( int change )
{
    nutrients added;
    // Kcal to calories
    added.calories = ( change * 1000 );
    return camp_food_supply( added );
}

nutrients basecamp::camp_food_supply( const time_duration &total_time, float exertion_level,
                                      const time_duration &travel_time )
{
    return camp_food_supply( -time_to_food( total_time, exertion_level, travel_time ) );
}

void basecamp::feed_workers( const std::vector<std::reference_wrapper <Character>> &workers,
                             nutrients food, bool is_player_meal )
{
    const int num_workers = workers.size();
    if( num_workers == 0 ) {
        debugmsg( "feed_workers called without any workers to feed!" );
        return;
    }
    if( !is_player_meal && get_option<bool>( "NO_NPC_FOOD" ) ) {
        return;
    }

    // Split the food into equal sized portions.
    food /= num_workers;
    for( const auto &worker_reference : workers ) {
        Character &worker = worker_reference.get();
        if( !allowed_access_by( worker ) ) {
            debugmsg( "Character %s improperly tried to access food stores at camp %s, please report this error.",
                      worker.disp_name(), name );
        }
        item food_item = make_fake_food( food );
        // Handle allergies and other stuff
        bool query_player = !worker.is_npc();
        const ret_val<edible_rating> rating = worker.will_eat( food_item, query_player );
        switch( rating.value() ) {
            case EDIBLE:
                // I'd like to use consume_activity_actor here, but our little trick with make_fake_food() requires that the
                // item be consumed immediately.
                worker.consume( food_item );
                worker.update_stomach( calendar::turn, calendar::turn ); //Handles setting hunger appropriately
                break;
            case TOO_FULL:
                worker.add_msg_player_or_npc( m_neutral,
                                              _( "You are too full to eat right now, and put the meal back into storage." ),
                                              _( "<npcname> is too full to eat right now, and puts the meal back into storage." ) );
                camp_food_supply( food );
                break;
            case INEDIBLE:
            case INEDIBLE_MUTATION:
                debugmsg( "Always-edible food somehow inedible, please report this error." );
                camp_food_supply( food );
                break;
            case ALLERGY:
                worker.add_msg_if_npc( m_bad,
                                       _( "%s takes one look at the food and declines, explaining they're allergic." ),
                                       worker.get_name() );
                camp_food_supply( food );
                break;
            case ALLERGY_WEAK:
                worker.add_msg_if_npc( m_bad,
                                       _( "%s takes a bite but spits it out.  It seems something in the food disagrees with them." ),
                                       worker.get_name() );
                camp_food_supply( food );
                break;
            case CANNIBALISM:
                worker.add_msg_if_npc( m_bad,
                                       _( "%s thanks you for the meal, but when they see what's in the meal their attitude suddenly changes!" ),
                                       worker.get_name() );
                if( worker.is_npc() ) {
                    worker.as_npc()->mutiny();
                }
                // Food specifically does not go back in the larder.
                break;
            case PARASITES:
            // None of these should ever happen.
            case ROTTEN:
            case NAUSEA:
            case NO_TOOL:
                break;
        }
    }
}

void basecamp::feed_workers( Character &worker, nutrients food, bool is_player_meal )
{
    std::vector<std::reference_wrapper <Character>> work_party;
    work_party.emplace_back( worker );
    feed_workers( work_party, std::move( food ), is_player_meal );
}

int basecamp::time_to_food( time_duration total_time, float work_exertion_level,
                            time_duration travel_time ) const
{
    // logic here reverses base_camps::to_workdays
    const int days = to_days<int>( total_time );
    const time_duration work_and_travel_time = days * work_day_hours * 1_hours + total_time - days *
            24_hours;
    const time_duration work_time = work_and_travel_time - travel_time;

    float effective_hours = work_time * work_exertion_level / 1_hours +
                            travel_time * MODERATE_EXERCISE / 1_hours +
                            days * work_day_rest_hours * NO_EXERCISE +
                            days * work_day_idle_hours * SLEEP_EXERCISE ;
    return base_metabolic_rate * effective_hours / 24;
}

item basecamp::make_fake_food( const nutrients &to_use ) const
{
    //TODO: If we're dynamically deciding its nutrition why not weight + volume and them just append the name like this?
    item food_item = to_use.kcal() > 3000 ?
                     item( itype_camp_meal_large ) : to_use.kcal() > 1000 ?
                     item( itype_camp_meal_medium ) : item( itype_camp_meal_small );
    // Set the default nutritional of the item.
    // This doesn't persist through save/load, but that's ok, we will be eating it immediately.
    food_item.get_comestible()->set_default_nutrition( to_use );
    return food_item;
}

static time_point rot_time( const item &it, item *const container )
{
    if( !it.goes_bad() ) {
        return calendar::turn_zero;
    }
    float spoil_mod = 1.0f;
    if( container ) {
        if( item_pocket *const pocket = container->contained_where( it ) ) {
            spoil_mod = pocket->spoil_multiplier();
        }
    }
    // Container seals and prevents any spoilage.
    if( spoil_mod == 0 ) {
        return calendar::turn_zero;
    }
    // @TODO: this does not handle fridges or things like root cellar, but maybe it shouldn't.
    const time_duration rots_in = ( it.get_shelf_life() - it.get_rot() ) / spoil_mod;

    // truncate to nearest day (gross)
    return ( ( calendar::turn + rots_in ) / 1_days ) * 1_days;
}

static std::optional<nutrients> nutrients_if_distributable( const item &it,
        bool distribute_vitamins )
{
    if( !it.is_comestible() ) {
        return std::nullopt;
    }
    // Always reject in-progress craft item
    if( it.is_craft() ) {
        return std::nullopt;
    }
    // Stuff like butchery refuse and other disgusting stuff
    if( it.get_comestible_fun() < -6 ) {
        return std::nullopt;
    }
    if( it.has_flag( flag_INEDIBLE ) ) {
        return std::nullopt;
    }
    if( it.rotten() ) {
        return std::nullopt;
    }
    // Alcohol is specifically excluded until it can be turned into a vitamin/tracked by the larder
    if( it.get_comestible()->addictions.count( addiction_alcohol ) ) {
        return std::nullopt;
    }

    // Can distribute COMESTIBLE type items with 0kcal, if they have vitamins and player selected option to do so
    nutrients from_it = default_character_compute_effective_nutrients( it ) * it.count();
    if( from_it.kcal() < 0 && ( !distribute_vitamins  || from_it.vitamins().empty() ) ) {
        return std::nullopt;
    }

    return from_it;
}

// just a helper for this common emplace or add to map operation
static void add_consumed_nutrients( std::map<time_point, nutrients> &into, time_point time,
                                    const nutrients &nutr )
{
    auto ret = into.emplace( time, nutr );
    if( !ret.second ) {
        ret.first->second += nutr;
    }
}

// returns success if the item should be removed
// Checks the contents of the item for nutrients, and removes ones with nutrients
// nutrients gained from this item and it's contents are the value of the ret_val
static ret_val<std::map<time_point, nutrients>> nutrients_from( item &it, item *const container,
        bool distribute_vitamins )
{
    // nutrients consumed and when they will rot
    std::map<time_point, nutrients> consumed;
    if( it.is_food_container() ) {
        std::vector<item *> to_remove;
        it.visit_items( [&]( item * content, item * const container ) {
            std::optional<nutrients> from_item = nutrients_if_distributable( *content, distribute_vitamins );
            if( from_item.has_value() ) {
                // we perform a magic act here and remove the item that's preserving it while keeping it preserved
                to_remove.push_back( content );
                add_consumed_nutrients( consumed, rot_time( *content, container ), *from_item );
                return VisitResponse::SKIP;
            }
            return VisitResponse::NEXT;
        } );
        // no contents that are valid food
        if( to_remove.empty() ) {
            return ret_val<std::map<time_point, nutrients>>::make_failure( consumed );
        }
        for( item *const food : to_remove ) {
            it.remove_item( *food );
        }
        it.on_contents_changed();
        return ret_val<std::map<time_point, nutrients>>::make_failure( consumed );
    }
    std::optional<nutrients> from_this = nutrients_if_distributable( it, distribute_vitamins );
    if( !from_this.has_value() ) {
        return ret_val<std::map<time_point, nutrients>>::make_failure( consumed );
    }
    add_consumed_nutrients( consumed, rot_time( it, container ), *from_this );
    return ret_val<std::map<time_point, nutrients>>::make_success( consumed );
}

// mission support
bool basecamp::distribute_food( bool player_command )
{
    if( !validate_sort_points() ) {
        if( player_command ) {
            popup( _( "You do not have a camp food zone.  Aborting…" ) );
        } else {
            debugmsg( "NPC-initiated food distribution at %s failed due to lacking zones", name );
        }
        return false;
    }

    bool distribute_vitamins = false; // NPCs only ever distribute food
    if( player_command ) {
        distribute_vitamins = query_yn(
                                  _( "Do you also wish to distribute comestibles without any calorie value (i.e. multivitamins, mutagens)?" ) );
    }

    map &here = get_map();
    zone_manager &mgr = zone_manager::get_manager();
    if( here.check_vehicle_zones( here.get_abs_sub().z() ) ) {
        mgr.cache_vzones();
    }
    const tripoint_abs_ms &abspos = get_dumping_spot();
    const std::unordered_set<tripoint_abs_ms> &z_food =
        mgr.get_near( zone_type_CAMP_FOOD, abspos, MAX_VIEW_DISTANCE, nullptr, get_owner() );

    // @FIXME: items under a vehicle cargo part will get taken even if there's no non-vehicle zone there
    // @FIXME: items in a vehicle cargo part will get taken even if the zone is on the ground underneath
    std::map<time_point, nutrients> all_nutrients;
    int num_skipped = 0;
    for( const tripoint_abs_ms &p_food_stock_abs : z_food ) {
        const tripoint_bub_ms p_food_stock = here.get_bub( p_food_stock_abs );
        map_stack items = here.i_at( p_food_stock );
        for( auto iter = items.begin(); iter != items.end(); ) {
            ret_val<std::map<time_point, nutrients>> ret = nutrients_from( *iter, nullptr,
                                                  distribute_vitamins );
            if( ret.success() ) {
                iter = items.erase( iter );
            } else {
                num_skipped++;
                ++iter;
            }
            for( const std::pair<const time_point, nutrients> &entry : ret.value() ) {
                add_consumed_nutrients( all_nutrients, entry.first, entry.second );
            }
        }
        if( const std::optional<vpart_reference> ovp = here.veh_at( p_food_stock ).cargo() ) {
            vehicle_stack items = ovp->items();
            for( auto iter = items.begin(); iter != items.end(); ) {
                ret_val<std::map<time_point, nutrients>> ret = nutrients_from( *iter, nullptr,
                                                      distribute_vitamins );
                if( ret.success() ) {
                    iter = items.erase( iter );
                } else {
                    num_skipped++;
                    ++iter;
                }
                for( const std::pair<const time_point, nutrients> &entry : ret.value() ) {
                    add_consumed_nutrients( all_nutrients, entry.first, entry.second );
                }
            }
        }
    }

    if( all_nutrients.empty() ) {
        if( player_command ) {
            std::string fail_msg = _( "No suitable items are located at the drop points…" );
            fail_msg += "\n\n";
            fail_msg += string_format(
                            _( "%d items were skipped for being unsuitable (not food, or too disgusting to eat, or already rotten, etc.  )" ),
                            num_skipped );
            popup( fail_msg );
        }
        return false;
    }

    nutrients nutrients_to_add = fac()->add_to_food_supply( all_nutrients );

    if( player_command ) {
        if( nutrients_to_add.kcal() > 0 ) {
            popup( string_format( _( "You distribute %d kcal worth of food to your companions." ),
                                  nutrients_to_add.kcal() ) );
        } else {
            popup( _( "You distribute vitamins and medicine to your companions." ) );
        }
    }

    return true;
}

void faction_template::add_kcal( int kcal, time_point expires )
{
    prune_food_supply();
    for( auto it = _food_supply.begin(); it != _food_supply.end(); ++it ) {
        if( it->first < expires ) {
            continue;
        }
        if( expires == it->first ) {
            it->second.calories += kcal * 1000;
        }
        if( it->first > expires ) {
            std::pair<time_point, nutrients> add;
            add.first = expires;
            add.second.calories = kcal;
            _food_supply.insert( it, add );
        }
        break;
    }
}

nutrients faction_template::food_supply() const
{
    nutrients ret;
    for( const std::pair<time_point, nutrients> &entry : _food_supply ) {
        // Ignore expired entries. We don't prune them because this function is const
        if( entry.first != calendar::turn_zero && entry.first < calendar::turn ) {
            continue;
        }
        ret += entry.second;
    }
    return ret;
}

nutrients faction_template::add_to_food_supply( const std::map<time_point, nutrients> &added )
{
    prune_food_supply();
    // ensure there is at least one entry (the one for food that is not perishable)
    if( _food_supply.empty() ) {
        _food_supply.emplace( _food_supply.begin(), calendar::turn_zero, nutrients() );
    }
    // track vitamins/calories added
    nutrients accumulator;
    // iteration over a map is lowest key -> highest key, the same direction of the list
    auto list_iter = _food_supply.begin();
    for( const std::pair<const time_point, nutrients> &entry : added ) {
        // on the exact right entry
        if( entry.first == list_iter->first ) {
            list_iter->second += entry.second;
        }
        // behind the entry we're looking for
        while( entry.first > list_iter->first ) {
            ++list_iter;
            // end of list
            if( list_iter == _food_supply.end() ) {
                list_iter = _food_supply.emplace( list_iter, entry.first, entry.second );
                continue;
            }
            // entry we're on is still behind this
            if( entry.first > list_iter->first ) {
                continue;
            }
            // we found an entry for this existing
            if( entry.first == list_iter->first ) {
                list_iter->second += entry.second;
            }
            // we've gone far enough that we found an entry further in the future, add behind it
            if( list_iter->first > entry.first ) {
                list_iter = _food_supply.emplace( list_iter, entry.first, entry.second );
            }
            break;
        }
        // the next entry is further in the future, add behind it
        if( entry.first < list_iter->first ) {
            list_iter = _food_supply.emplace( list_iter, entry.first, entry.second );
        }
        // next list entry, we've inserted into this one
        ++list_iter;
        accumulator += entry.second;
    }
    return accumulator;
}

static nutrients consume_left_behind( nutrients &consumer, const nutrients &source )
{
    nutrients left;
    // consume nutrients
    consumer -= source;
    // anything driven negative has been over-consumed, and is left behind
    // so flip the sign, and remove all negative elements
    left = -consumer;
    left.ensure_positive();
    // And remove everything that was left behind from the consumer
    consumer.ensure_positive();
    return left;
}

nutrients faction_template::consume_food_supply( const nutrients &consumed )
{
    prune_food_supply();
    // the nutrients we need to supply
    nutrients to_supply = consumed;

    // only non-perishable food
    if( _food_supply.size() < 2 ) {
        if( _food_supply.empty() ) {
            debugmsg( "Consuming food but no food in store??" );
            return nutrients();
        }
        _food_supply.front().second = consume_left_behind( to_supply, _food_supply.front().second );
        // whatever remains was not consumed
        return to_supply;
    }
    for( auto it = _food_supply.begin(); it != _food_supply.end(); ) {
        // start by skipping the non-perishable food
        if( it == _food_supply.begin() ) {
            ++it;
            continue;
        }
        it->second = consume_left_behind( to_supply, it->second );
        // remove consumed entries
        if( it->second.calories == 0 && it->second.vitamins().empty() ) {
            it = _food_supply.erase( it );
        } else {
            ++it;
        }
        if( to_supply.calories == 0 && to_supply.vitamins().empty() ) {
            break;
        }
    }
    // we consumed everything perishable, consume non-perishable
    if( to_supply.calories > 0 ) {
        // eat everything here
        _food_supply.front().second = consume_left_behind( to_supply, _food_supply.front().second );
    }
    return to_supply;
}

void faction_template::empty_food_supply()
{
    _food_supply.clear();
}

void faction_template::prune_food_supply()
{
    for( auto it = _food_supply.begin(); it != _food_supply.end(); ) {
        if( it->first != calendar::turn_zero && it->first < calendar::turn ) {
            it = _food_supply.erase( it );
        } else {
            ++it;
        }
    }
}

cata::list<std::pair<time_point, nutrients>> &faction_template::debug_food_supply()
{
    return _food_supply;
}



std::string basecamp::name_display_of( const mission_id &miss_id )
{
    const std::string dir_abbr = base_camps::all_directions.at(
                                     miss_id.dir.value() ).bracket_abbr.translated();
    std::vector<basecamp_upgrade> upgrades;
    std::vector<std::string> pos_names;

    switch( miss_id.id ) {
        case No_Mission:
        case Scavenging_Patrol_Job:
        case Scavenging_Raid_Job:
        case Menial_Job:
        case Carpentry_Job:
        case Forage_Job:
        case Caravan_Commune_Center_Job:

        //  Faction camp tasks
        case Camp_Distribute_Food:
        case Camp_Determine_Leadership:
        case Camp_Have_Meal:
        case Camp_Requests:
        case Camp_Hide_Mission:
        case Camp_Reveal_Mission:
        case Camp_Assign_Jobs:
        case Camp_Assign_Workers:
        case Camp_Abandon:
        case Camp_Emergency_Recall:
        case Camp_Gather_Materials:
        case Camp_Collect_Firewood:
        case Camp_Menial:
        case Camp_Survey_Field:
        case Camp_Survey_Expansion:
        case Camp_Cut_Logs:
        case Camp_Clearcut:
        case Camp_Setup_Hide_Site:
        case Camp_Relay_Hide_Site:
        case Camp_Foraging:
        case Camp_Trapping:
        case Camp_Hunting:
        case Camp_OM_Fortifications:
        case Camp_Recruiting:
        case Camp_Scouting:
        case Camp_Combat_Patrol:
        case Camp_Plow:
        case Camp_Plant:
        case Camp_Harvest:
            return mission_ui_activity_of( miss_id );

        case Camp_Upgrade: {
            upgrades = available_upgrades( miss_id.dir.value() );

            auto upgrade_it = std::find_if(
                                  upgrades.begin(), upgrades.end(),
            [&]( const basecamp_upgrade & upgrade ) {
                return upgrade.bldg == miss_id.parameters;
            } );
            if( upgrade_it == upgrades.end() ) {
                return mission_ui_activity_of( miss_id ) + _( "<No longer valid construction>" );
            }
            std::string result = mission_ui_activity_of( miss_id ) + upgrade_it->name;
            const recipe &rec = *recipe_id( upgrade_it->bldg );
            for( const std::pair<const std::string, cata_variant> &arg : miss_id.mapgen_args.map ) {
                result +=
                    string_format(
                        " (%s)", rec.blueprint_parameter_ui_string( arg.first, arg.second ) );
            }
            return result;
        }
        case Camp_Crafting: {
            const std::string dir_id = base_camps::all_directions.at( miss_id.dir.value() ).id;
            const std::string dir_abbr = base_camps::all_directions.at(
                                             miss_id.dir.value() ).bracket_abbr.translated();

            const std::map<recipe_id, translation> &recipes = recipe_deck( miss_id.dir.value() );
            const auto it = recipes.find( recipe_id( miss_id.parameters ) );
            if( it != recipes.end() ) {
                return dir_abbr + it->second;
            } else {
                return dir_abbr + _( " <Unsupported recipe>" );
            }
        }
        default:
            return "";
    }
}

void basecamp::handle_reveal_mission( const point_rel_omt &dir )
{
    if( hidden_missions.empty() ) { //  Should never happen
        return;
    }
    const base_camps::direction_data &base_data = base_camps::all_directions.at( dir );

    while( true ) {
        std::vector<std::string> pos_names;
        int choice = 0;
        pos_names.reserve( hidden_missions[size_t( base_data.tab_order )].size() );

        for( ui_mission_id &id : hidden_missions[size_t( base_data.tab_order )] ) {
            pos_names.push_back( name_display_of( id.id ) );
        }

        choice = uilist( _( "Select mission(s) to reveal, escape when done" ), pos_names );

        if( choice < 0 || static_cast<size_t>( choice ) >= pos_names.size() ) {
            popup( _( "You're done for now…" ) );
            return;
        }

        hidden_missions[size_t( base_data.tab_order )].erase( hidden_missions[size_t(
                    base_data.tab_order )].begin() + choice );
    }
}

void basecamp::handle_hide_mission( const point_rel_omt &dir )
{
    const base_camps::direction_data &base_data = base_camps::all_directions.at( dir );
    const size_t previously_hidden_count = hidden_missions[size_t( base_data.tab_order )].size();

    while( true ) {
        std::vector<std::string> pos_names;
        std::vector<ui_mission_id> reference;
        int choice = 0;
        pos_names.reserve( hidden_missions[size_t( base_data.tab_order )].size() );
        reference.reserve( hidden_missions[size_t( base_data.tab_order )].size() );

        for( ui_mission_id &miss_id : temp_ui_mission_keys[size_t( base_data.tab_order )] ) {
            if( !miss_id.ret &&
                miss_id.id.id != Camp_Reveal_Mission ) {

                // Filter out the onces we're hiding in this loop, as temp_ui_mission_keys isn't refreshed
                bool hidden = false;
                for( size_t i = previously_hidden_count; i < hidden_missions[size_t( base_data.tab_order )].size();
                     i++ ) {
                    if( is_equal( hidden_missions[size_t( base_data.tab_order )].at( i ), miss_id ) ) {
                        hidden = true;
                        break;
                    }
                }
                if( !hidden ) {
                    pos_names.push_back( name_display_of( miss_id.id ) );
                    reference.push_back( miss_id );
                }
            }
        }

        choice = uilist( _( "Select mission(s) to hide, escape when done" ), pos_names );

        if( choice < 0 || static_cast<size_t>( choice ) >= pos_names.size() ) {
            popup( _( "You're done for now…" ) );
            return;
        }

        hidden_missions[size_t( base_data.tab_order )].push_back( reference[choice] );
    }
}

// morale
int basecamp::camp_discipline( int change ) const
{
    fac()->respects_u += change;
    return fac()->respects_u;
}

int basecamp::camp_morale( int change ) const
{
    fac()->likes_u += change;
    return fac()->likes_u;
}

void basecamp::place_results( const item &result )
{
    map &target_bay = get_camp_map();
    form_storage_zones( target_bay, bb_pos );
    tripoint_bub_ms new_spot = target_bay.get_bub( get_dumping_spot() );
    // Special handling for liquids
    // find any storage-zoned LIQUIDCONT we can dump them in, set that as the item's destination instead
    if( result.made_of( phase_id::LIQUID ) ) {
        for( tripoint_abs_ms potential_spot : get_liquid_dumping_spot() ) {
            // No items at a potential spot? Set the destination there and stop checking.
            // We could check if the item at the tile are the same as the item we're placing, but liquids of the same typeid
            // don't always mix depending on their components...
            if( target_bay.i_at( target_bay.get_bub( potential_spot ) ).empty() ) {
                new_spot = target_bay.get_bub( potential_spot );
                break;
            }
            // We've processed the last spot and haven't found anywhere to put it, we'll end up using dumping_spot.
            // Throw a warning to let players know what's going on. Unfortunately we can't back out of finishing the mission this deep in the process.
            if( potential_spot == get_liquid_dumping_spot().back() ) {
                popup( _( "No eligible locations found to place resulting liquids, placing them at random.\n\nEligible locations must be a terrain OR furniture (not item) that can contain liquid, and does not already have any items on its tile." ) );
            }
        }
    }
    target_bay.add_item_or_charges( new_spot, result, true );
    apply_camp_ownership( target_bay, new_spot, 10 );
    target_bay.save();
}

void apply_camp_ownership( map &here, const tripoint_bub_ms &camp_pos, int radius )
{
    for( const tripoint_bub_ms &p : here.points_in_rectangle( camp_pos + point(
                -radius, -radius ),
            camp_pos + point( radius, radius ) ) ) {
        map_stack items = here.i_at( p.xy() );
        for( item &elem : items ) {
            elem.set_owner( get_player_character() );
        }
    }
}

// combat and danger
// this entire system is stupid
bool survive_random_encounter( npc &comp, std::string &situation, int favor, int threat )
{
    popup( _( "While %s, a silent specter approaches %s…" ), situation, comp.get_name() );
    float skill_1 = comp.get_skill_level( skill_survival );
    float skill_2 = comp.get_skill_level( skill_speech );
    if( skill_1 + favor > rng( 0, 10 ) ) {
        popup( _( "%s notices the antlered horror and slips away before it gets too close." ),
               comp.get_name() );
        talk_function::companion_skill_trainer( comp, "gathering", 10_minutes, 10 - favor );
    } else if( skill_2 + favor > rng( 0, 10 ) ) {
        popup( _( "Another survivor approaches %s asking for directions." ), comp.get_name() );
        popup( _( "Fearful that he may be an agent of some hostile faction, "
                  "%s doesn't mention the camp." ), comp.get_name() );
        popup( _( "The two part on friendly terms and the survivor isn't seen again." ) );
        talk_function::companion_skill_trainer( comp, skill_recruiting, 10_minutes, 10 - favor );
    } else {
        popup( _( "%s didn't detect the ambush until it was too late!" ), comp.get_name() );
        float skill = comp.get_skill_level( skill_melee ) +
                      0.5 * comp.get_skill_level( skill_survival ) +
                      comp.get_skill_level( skill_bashing ) +
                      comp.get_skill_level( skill_cutting ) +
                      comp.get_skill_level( skill_stabbing ) +
                      comp.get_skill_level( skill_unarmed ) + comp.get_skill_level( skill_dodge );
        int monsters = rng( 0, threat );
        if( skill * rng( 8, 12 ) > ( monsters * rng( 8, 12 ) ) ) {
            if( one_in( 2 ) ) {
                popup( _( "The bull moose charged %s from the tree line…" ), comp.get_name() );
                popup( _( "Despite being caught off guard %s was able to run away until the "
                          "moose gave up pursuit." ), comp.get_name() );
            } else {
                popup( _( "The jabberwock grabbed %s by the arm from behind and "
                          "began to scream." ), comp.get_name() );
                popup( _( "Terrified, %s spun around and delivered a massive kick "
                          "to the creature's torso…" ), comp.get_name() );
                popup( _( "Collapsing into a pile of gore, %s walked away unscathed…" ),
                       comp.get_name() );
                popup( _( "(Sounds like bullshit, you wonder what really happened.)" ) );
            }
            talk_function::companion_skill_trainer( comp, skill_combat, 10_minutes, 10 - favor );
        } else {
            if( one_in( 2 ) ) {
                popup( _( "%s turned to find the hideous black eyes of a giant wasp "
                          "staring back from only a few feet away…" ), comp.get_name() );
                popup( _( "The screams were terrifying, there was nothing anyone could do." ) );
            } else {
                popup( _( "Pieces of %s were found strewn across a few bushes." ), comp.get_name() );
                popup( _( "(You wonder if your companions are fit to work on their own…)" ) );
            }
            overmap_buffer.remove_npc( comp.getID() );
            return false;
        }
    }
    return true;
}

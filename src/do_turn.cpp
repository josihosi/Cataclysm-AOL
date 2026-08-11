#include "do_turn.h"

#if defined(EMSCRIPTEN)
#include <emscripten.h>
#endif

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <ratio>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "action.h"
#include "activity_type.h"
#include "avatar.h"
#include "bandit_live_world.h"
#include "bandit_live_world_probe.h"
#include "bandit_mark_generation.h"
#include "basecamp.h"
#include "bionics.h"
#include "cached_options.h"
#include "calendar.h"
#ifdef TILES
#include "cata_imgui.h"
#endif
#include "cata_variant.h"
#include "clzones.h"
#include "coordinates.h"
#include "creature_tracker.h"
#include "debug.h"
#include "dialogue_win.h"
#include "debug_capture.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "explosion.h"
#include "faction.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "gamemode.h"
#include "help.h"
#include "horde_entity.h"
#include "input.h"
#include "input_context.h"
#include "item_wakeup.h"
#include "item.h"
#include "magic_enchantment.h"
#include "map.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "memorial_logger.h"
#include "messages.h"
#include "llm_intent.h"
#include "line.h"
#include "lightmap.h"
#include "mission.h"
#include "monster.h"
#include "mongroup.h"
#include "mtype.h"
#include "music.h"
#include "npc.h"
#include "npctrade.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "pathfinding.h"
#include "pimpl.h"
#include "player_activity.h"
#include "point.h"
#include "popup.h"
#include "rng.h"
#include "regional_settings.h"
#include "scent_map.h"
#include "sdlsound.h"
#include "simple_pathfinding.h"
#include "sounds.h"
#include "stats_tracker.h"
#include "string_formatter.h"
#include "timed_event.h"
#include "trap.h"
#include "translations.h"
#include "type_id.h"
#include "uilist.h"
#include "ui_manager.h"
#include "units.h"
#include "vehicle.h"
#include "veh_type.h"
#include "vpart_position.h"
#include "weather.h"
#include "weather_gen.h"
#include "weather_type.h"
#include "worldfactory.h"
#include "zombie_rider_overmap_ai.h"

static const activity_id ACT_AUTODRIVE( "ACT_AUTODRIVE" );
static const activity_id ACT_FIRSTAID( "ACT_FIRSTAID" );
static const activity_id ACT_MIGRATION_CANCEL( "ACT_MIGRATION_CANCEL" );
static const activity_id ACT_MOVE_LOOT( "ACT_MOVE_LOOT" );
static const activity_id ACT_OPERATION( "ACT_OPERATION" );

static const bionic_id bio_alarm( "bio_alarm" );

static const efftype_id effect_controlled( "controlled" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_npc_suspend( "npc_suspend" );
static const efftype_id effect_run( "run" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_psi_stunned( "psi_stunned" );

static const event_statistic_id event_statistic_last_words( "last_words" );

static const json_character_flag json_flag_CANNOT_MOVE( "CANNOT_MOVE" );
static const json_character_flag json_flag_CANNOT_ATTACK( "CANNOT_ATTACK" );
static const json_character_flag json_flag_NO_SCENT( "NO_SCENT" );
static const json_character_flag json_flag_SEESLEEP( "SEESLEEP" );
static constexpr int hostile_scout_immobility_grace_minutes = 6 * 60;

static const mtype_id mon_zombie_rider( "mon_zombie_rider" );
static const species_id species_ZOMBIE( "ZOMBIE" );

static const ter_str_id ter_t_flat_roof( "t_flat_roof" );
static const ter_str_id ter_t_tile_flat_roof( "t_tile_flat_roof" );

static const trait_id trait_DEBUG_CLAIRVOYANCE( "DEBUG_CLAIRVOYANCE" );
static const trait_id trait_HAS_NEMESIS( "HAS_NEMESIS" );
static const trait_id trait_NPC_STATIC_NPC( "NPC_STATIC_NPC" );

#if defined(__ANDROID__)
extern std::map<std::string, std::list<input_event>> quick_shortcuts_map;
extern bool add_best_key_for_action_to_quick_shortcuts( action_id action,
        const std::string &category, bool back );
#endif

#define dbg(x) DebugLog((x),D_GAME) << __FILE__ << ":" << __LINE__ << ": "

std::string live_bandit_homeward_boundary_discriminator_for_test();
std::string live_bandit_homeward_unsafe_current_route_read_for_test( character_id member_id );
std::string live_bandit_homeward_partner_route_read_for_test(
    character_id member_id, character_id partner_id );
std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>>
live_bandit_homeward_boundary_steps_for_test();

bool live_bandit_local_handoff_position_is_motor_addressable(
    const tripoint_abs_ms &position, const tripoint_abs_sm &motor_center,
    const int motor_radius_sm )
{
    return motor_radius_sm >= 0 &&
           square_dist( project_to<coords::sm>( position ).xy(), motor_center.xy() ) <=
           motor_radius_sm;
}

namespace
{
bool live_bandit_can_make_ordinary_visual_observation( const Character &observer )
{
    return !observer.is_blind() && !observer.has_effect( effect_narcosis ) &&
           ( !observer.in_sleep_state() || observer.has_flag( json_flag_SEESLEEP ) );
}

bool site_contains_omt( const bandit_live_world::site_record &site, const tripoint_abs_omt &omt )
{
    return std::find( site.footprint.begin(), site.footprint.end(), omt ) != site.footprint.end();
}

static constexpr int live_bandit_basecamp_reach_radius = 30;
static constexpr int live_bandit_basecamp_storage_zone_scan_radius = live_bandit_basecamp_reach_radius * 2;
static constexpr int live_bandit_camp_adjacent_radius_submaps = 24;
static constexpr std::size_t live_bandit_response_source_omt_cap = 64;
static const faction_id faction_your_followers( "your_followers" );
static const zone_type_id zone_type_CAMP_STORAGE( "CAMP_STORAGE" );

void live_bandit_refresh_basecamp_storage_tiles( const avatar &u, basecamp &camp )
{
    zone_manager::get_manager().cache_data();
    std::unordered_set<tripoint_abs_ms> storage_tiles =
        zone_manager::get_manager().get_near( zone_type_CAMP_STORAGE, u.pos_abs(),
                live_bandit_basecamp_storage_zone_scan_radius, nullptr, camp.get_owner() );
    const std::unordered_set<tripoint_abs_ms> follower_storage_tiles =
        zone_manager::get_manager().get_near( zone_type_CAMP_STORAGE, u.pos_abs(),
                live_bandit_basecamp_storage_zone_scan_radius, nullptr, faction_your_followers );
    storage_tiles.insert( follower_storage_tiles.begin(), follower_storage_tiles.end() );
    if( !storage_tiles.empty() ) {
        camp.set_storage_tiles( storage_tiles );
    }
}

basecamp *live_bandit_nearest_basecamp( const avatar &u )
{
    if( std::optional<basecamp *> bcp = overmap_buffer.find_camp( u.pos_abs_omt().xy() ) ) {
        return *bcp;
    }

    const std::vector<camp_reference> camps_near_player = overmap_buffer.get_camps_near(
                u.pos_abs_sm(), live_bandit_camp_adjacent_radius_submaps );
    if( !camps_near_player.empty() ) {
        return camps_near_player.front().camp;
    }

    return nullptr;
}

bool live_bandit_player_near_basecamp( const avatar &u )
{
    return live_bandit_nearest_basecamp( u ) != nullptr;
}

bool live_bandit_player_in_rolling_travel_scene( const avatar &u )
{
    if( u.in_vehicle && u.controlling_vehicle ) {
        return true;
    }

    return overmap_buffer.ter( u.pos_abs_omt() )->is_road();
}

bool live_bandit_seen_by_nearby_ally( const map &here, const avatar &u,
                                      const tripoint_bub_ms &target );

bool live_bandit_tile_has_smoke( const map &here, const tripoint_bub_ms &tile )
{
    return here.get_field_intensity( tile, fd_smoke ) > 0;
}

bool live_bandit_smoke_between( const map &here, const tripoint_bub_ms &from,
                                const tripoint_bub_ms &to )
{
    for( const tripoint_bub_ms &pt : line_to( from, to ) ) {
        if( pt == from || pt == to ) {
            continue;
        }
        if( live_bandit_tile_has_smoke( here, pt ) ) {
            return true;
        }
    }
    return false;
}

bandit_live_world::local_gate_input live_bandit_make_gate_input(
    const bandit_live_world::site_record &site, const avatar &u )
{
    bandit_live_world::local_gate_input input;
    input.darkness_or_concealment = is_night( calendar::turn );
    input.rolling_travel_scene = live_bandit_player_in_rolling_travel_scene( u );
    input.basecamp_or_camp_scene = !input.rolling_travel_scene &&
                                      live_bandit_player_near_basecamp( u );
    if( input.rolling_travel_scene ) {
        input.local_threat = 1;
        input.local_opportunity = 2;
    } else if( input.basecamp_or_camp_scene ) {
        input.local_threat = 3;
        input.local_opportunity = 2;
        input.recent_exposure = true;
    }

    map &here = get_map();
    int closest_member_distance = rl_dist( site.anchor, u.pos_abs_omt() );
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing != nullptr ) {
        for( const character_id &member_id : outing->member_ids ) {
            if( outing->member_is_resolved( member_id ) ) {
                continue;
            }
            const npc *member_npc = g->find_npc( member_id );
            if( member_npc == nullptr ) {
                continue;
            }
            const tripoint_bub_ms member_pos = member_npc->pos_bub( here );
            input.current_exposure |= get_player_view().sees( here, member_pos ) ||
                                      live_bandit_seen_by_nearby_ally( here, u, member_pos );
            const bool smoke_on_member = live_bandit_tile_has_smoke( here, member_pos );
            const bool smoke_on_sightline = live_bandit_smoke_between( here, u.pos_bub( here ),
                                                member_pos );
            input.smoke_on_watcher_tile |= smoke_on_member;
            input.smoke_between_watcher_and_camp |= smoke_on_sightline;
            input.smoke_obscured_lead |= smoke_on_member || smoke_on_sightline;
            const int distance = rl_dist( member_npc->pos_abs_omt(), u.pos_abs_omt() );
            closest_member_distance = std::min( closest_member_distance, distance );
            const bandit_live_world::member_record *member = site.find_member( member_id );
            const bool saved_local_contact = member != nullptr &&
                                             member->state == bandit_live_world::member_state::local_contact;
            input.local_contact_established |= distance <= 1 || saved_local_contact;
        }
    }
    input.standoff_distance = closest_member_distance;
    if( input.local_contact_established && !input.rolling_travel_scene ) {
        input.local_threat = std::min( input.local_threat, 1 );
        input.local_opportunity = std::max( input.local_opportunity, 3 );
        input.recent_exposure = false;
    }
    return input;
}

std::string live_bandit_omt_token( const tripoint_abs_omt &omt )
{
    std::ostringstream out;
    out << omt.x() << ',' << omt.y() << ',' << omt.z();
    return out.str();
}

int live_bandit_item_value( const item &it )
{
    if( it.has_flag( flag_INTEGRATED ) || it.has_flag( flag_NO_TAKEOFF ) ) {
        return 0;
    }
    return std::max( 0, it.price( true ) );
}

int live_bandit_character_goods_value( const Character &who )
{
    int value = 0;
    for( const item *it : who.inv_dump() ) {
        if( it == nullptr ) {
            continue;
        }
        value += live_bandit_item_value( *it );
    }
    return value;
}

int live_bandit_nearby_ground_goods_value( const avatar &u )
{
    map &here = get_map();
    int value = 0;
    for( const tripoint_bub_ms &pt : here.points_in_radius( u.pos_bub(),
            live_bandit_basecamp_reach_radius ) ) {
        if( !here.accessible_items( pt ) ) {
            continue;
        }
        for( const item &it : here.i_at( pt ) ) {
            value += live_bandit_item_value( it );
        }
    }
    return value;
}

int live_bandit_basecamp_storage_goods_value( const avatar &u, const basecamp &camp,
        const int nearby_radius_to_skip )
{
    map &here = get_map();
    int value = 0;
    for( const tripoint_abs_ms &storage_tile : camp.get_storage_tiles() ) {
        if( nearby_radius_to_skip >= 0 &&
            rl_dist( storage_tile, u.pos_abs() ) <= nearby_radius_to_skip ) {
            continue;
        }

        const tripoint_bub_ms local_tile = here.get_bub( storage_tile );
        if( !here.inbounds( local_tile ) ) {
            continue;
        }
        if( here.accessible_items( local_tile ) ) {
            for( const item &it : here.i_at( local_tile ) ) {
                value += live_bandit_item_value( it );
            }
        }
        const std::optional<vpart_reference> cargo_part = here.veh_at( local_tile ).cargo();
        if( cargo_part ) {
            for( const item &it : cargo_part->items() ) {
                value += live_bandit_item_value( it );
            }
        }
    }
    return value;
}

int live_bandit_basecamp_assigned_npc_goods_value( const avatar &u, basecamp &camp,
        const int nearby_radius_to_skip )
{
    int value = 0;
    std::set<character_id> counted;
    const auto count_assigned = [&]( const npc &assigned, const bool assigned_to_this_camp ) {
        if( assigned.is_dead() || !assigned.is_player_ally() ) {
            return;
        }
        if( !assigned_to_this_camp && nearby_radius_to_skip >= 0 &&
            rl_dist( assigned.pos_abs(), u.pos_abs() ) <= nearby_radius_to_skip ) {
            return;
        }
        if( counted.insert( assigned.getID() ).second ) {
            value += live_bandit_character_goods_value( assigned );
        }
    };
    for( const npc_ptr &assigned : camp.get_npcs_assigned() ) {
        if( assigned == nullptr ) {
            continue;
        }
        count_assigned( *assigned, true );
    }
    for( const npc &assigned : g->all_npcs() ) {
        const bool assigned_to_this_camp = assigned.assigned_camp &&
                                           *assigned.assigned_camp == camp.camp_omt_pos();
        const bool in_basecamp_side_pool = rl_dist( assigned.pos_abs(), u.pos_abs() ) <=
                                           live_bandit_basecamp_storage_zone_scan_radius;
        if( !assigned_to_this_camp && !in_basecamp_side_pool ) {
            continue;
        }
        count_assigned( assigned, assigned_to_this_camp );
    }
    return value;
}

int live_bandit_current_vehicle_goods_value( const avatar &u )
{
    if( !u.in_vehicle ) {
        return 0;
    }

    map &here = get_map();
    const optional_vpart_position player_vehicle = here.veh_at( u.pos_bub() );
    if( !player_vehicle ) {
        return 0;
    }

    int value = 0;
    vehicle &veh = player_vehicle->vehicle();
    for( const vpart_reference &part_ref : veh.get_all_parts() ) {
        for( const item &it : veh.get_items( part_ref.part() ) ) {
            value += live_bandit_item_value( it );
        }
    }
    return value;
}

int live_bandit_nearby_basecamp_defender_count( const avatar &u )
{
    static constexpr int nearby_defender_radius = 30;
    int defenders = 0;
    for( const npc &guy : g->all_npcs() ) {
        if( !guy.is_player_ally() || guy.is_dead() ||
            rl_dist( guy.pos_abs(), u.pos_abs() ) > nearby_defender_radius ) {
            continue;
        }
        defenders++;
    }
    return defenders;
}

bandit_live_world::shakedown_goods_pool live_bandit_make_shakedown_goods_pool(
    const bandit_live_world::local_gate_input &input, const avatar &u )
{
    bandit_live_world::shakedown_goods_pool pool;
    pool.basecamp_or_camp_scene = input.basecamp_or_camp_scene;
    pool.player_carried_value = live_bandit_character_goods_value( u );

    static constexpr int nearby_companion_radius = 12;
    for( const npc &guy : g->all_npcs() ) {
        if( !guy.is_player_ally() || rl_dist( guy.pos_abs(), u.pos_abs() ) > nearby_companion_radius ) {
            continue;
        }
        if( input.basecamp_or_camp_scene && guy.assigned_camp ) {
            continue;
        }
        pool.companion_carried_value += live_bandit_character_goods_value( guy );
    }

    if( input.basecamp_or_camp_scene ) {
        pool.reachable_basecamp_value = live_bandit_nearby_ground_goods_value( u );
        if( basecamp *camp = live_bandit_nearest_basecamp( u ) ) {
            live_bandit_refresh_basecamp_storage_tiles( u, *camp );
            pool.reachable_basecamp_value += live_bandit_basecamp_storage_goods_value( u, *camp,
                                             live_bandit_basecamp_reach_radius );
            pool.companion_carried_value += live_bandit_basecamp_assigned_npc_goods_value( u, *camp,
                                            nearby_companion_radius );
        }
    } else {
        pool.vehicle_carried_value = live_bandit_current_vehicle_goods_value( u );
    }

    return pool;
}

int live_bandit_select_shakedown_payment( const bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::shakedown_surface &surface, avatar &u )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return 0;
    }
    npc *trader = nullptr;
    for( const character_id &member_id : outing->member_ids ) {
        if( npc *candidate = g->find_npc( member_id ) ) {
            trader = candidate;
            break;
        }
    }
    if( trader == nullptr ) {
        DebugLog( D_INFO, DC_ALL ) << "shakedown_trade_ui result=no_trader demanded="
                                   << surface.demanded_value << " reachable="
                                   << surface.reachable_goods_value << '\n';
        return 0;
    }

    const int player_value = live_bandit_character_goods_value( u );
    const int companion_value = [&u, &input]() {
        int value = 0;
        static constexpr int nearby_companion_radius = 12;
        for( const npc &guy : g->all_npcs() ) {
            if( !guy.is_player_ally() || rl_dist( guy.pos_abs(), u.pos_abs() ) > nearby_companion_radius ) {
                continue;
            }
            if( input.basecamp_or_camp_scene && guy.assigned_camp ) {
                continue;
            }
            value += live_bandit_character_goods_value( guy );
        }
        return value;
    }();
    basecamp *payment_basecamp = input.basecamp_or_camp_scene ?
                                 live_bandit_nearest_basecamp( u ) : nullptr;
    if( payment_basecamp != nullptr ) {
        live_bandit_refresh_basecamp_storage_tiles( u, *payment_basecamp );
    }
    const int basecamp_storage_value = payment_basecamp != nullptr ?
                                       live_bandit_basecamp_storage_goods_value( u, *payment_basecamp,
                                               live_bandit_basecamp_reach_radius ) : 0;
    static constexpr int nearby_companion_radius = 12;
    const int basecamp_npc_value = payment_basecamp != nullptr ?
                                   live_bandit_basecamp_assigned_npc_goods_value( u, *payment_basecamp,
                                           nearby_companion_radius ) : 0;
    const int scene_value = input.basecamp_or_camp_scene ?
                            live_bandit_nearby_ground_goods_value( u ) + basecamp_storage_value :
                            live_bandit_current_vehicle_goods_value( u );

    DebugLog( D_INFO, DC_ALL ) << "shakedown_trade_ui opened demanded="
                               << surface.demanded_value << " reachable="
                               << surface.reachable_goods_value
                               << " player_pool=" << player_value
                               << " nearby_npc_pool=" << companion_value
                               << " basecamp_npc_pool=" << basecamp_npc_value
                               << " scene_pool=" << scene_value
                               << " basecamp_storage_pool=" << basecamp_storage_value
                               << " trader=" << trader->getID().get_value()
                               << " trade_api=npc_trading::trade"
                               << " title=Pay:\n";
    const bool paid = npc_trading::trade( *trader, surface.demanded_value, _( "Pay:" ),
                                          input.basecamp_or_camp_scene ? live_bandit_basecamp_reach_radius : 1,
                                          nearby_companion_radius, payment_basecamp );
    DebugLog( D_INFO, DC_ALL ) << "shakedown_trade_ui result="
                               << ( paid ? "paid" : "cancel_or_short" )
                               << " demanded=" << surface.demanded_value
                               << " reachable=" << surface.reachable_goods_value
                               << " trader=" << trader->getID().get_value() << '\n';
    return paid ? surface.demanded_value : 0;
}

bool live_bandit_shakedown_already_opened( const bandit_live_world::site_record &site )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return false;
    }
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return false;
    }
    for( const character_id &member_id : outing->member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member != nullptr && member->last_writeback_summary.find( "shakedown_surface" ) !=
            std::string::npos ) {
            return true;
        }
    }
    return false;
}

struct live_bandit_paid_release_plan {
    bandit_live_world::site_record candidate;
    std::vector<character_id> member_ids;
};

std::optional<live_bandit_paid_release_plan> live_bandit_prepare_paid_release(
    const bandit_live_world::site_record &site )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return std::nullopt;
    }
    const std::string activity_id = outing->activity_id;
    const int generation = outing->generation;
    live_bandit_paid_release_plan plan;
    plan.member_ids = outing->member_ids;
    plan.candidate = site;
    if( !bandit_live_world::release_matching_external_reservation(
            plan.candidate, activity_id, generation,
            "shakedown payment release preflight" ) ) {
        return std::nullopt;
    }
    return plan;
}

void live_bandit_commit_paid_release( bandit_live_world::site_record &site,
                                      live_bandit_paid_release_plan plan,
                                      const bandit_live_world::shakedown_surface &surface,
                                      const int surrendered_value )
{
    bandit_live_world::shakedown_outcome outcome;
    outcome.paid = true;
    outcome.basecamp_or_camp_scene = surface.includes_basecamp_inventory;
    outcome.demanded_value = surface.demanded_value;
    outcome.surrendered_value = surrendered_value;
    outcome.reachable_goods_value = surface.reachable_goods_value;
    bandit_live_world::apply_shakedown_outcome( plan.candidate, outcome );

    const std::string summary = string_format( "shakedown_surface paid toll=%d demanded=%d reachable=%d",
                                surrendered_value, surface.demanded_value,
                                surface.reachable_goods_value );
    DebugLog( D_INFO, DC_ALL ) << summary << '\n';
    for( const character_id &member_id : plan.member_ids ) {
        const bandit_live_world::member_record *member = plan.candidate.find_member( member_id );
        if( member != nullptr && member->state == bandit_live_world::member_state::at_home ) {
            bandit_live_world::update_member_state(
                plan.candidate, member_id, bandit_live_world::member_state::at_home, summary );
        }
    }
    site = std::move( plan.candidate );
    for( const character_id &member_id : plan.member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != bandit_live_world::member_state::at_home ) {
            continue;
        }
        if( npc *member_npc = g->find_npc( member_id ) ) {
            member_npc->set_attitude( NPCATT_NULL );
            std::vector<tripoint_abs_omt> path = overmap_buffer.get_travel_path( member_npc->pos_abs_omt(),
                                                   site.anchor, overmap_path_params::for_npc() ).points;
            if( !path.empty() ) {
                member_npc->goal = site.anchor;
                member_npc->omt_path = std::move( path );
                member_npc->set_mission( NPC_MISSION_TRAVELLING );
            }
        }
    }
}

void live_bandit_choose_fight( bandit_live_world::site_record &site,
                               const bandit_live_world::shakedown_surface &surface, const avatar &u )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return;
    }
    const std::vector<character_id> member_ids = outing->member_ids;
    bandit_live_world::shakedown_outcome outcome;
    outcome.fought = true;
    outcome.basecamp_or_camp_scene = surface.includes_basecamp_inventory;
    outcome.demanded_value = surface.demanded_value;
    outcome.reachable_goods_value = surface.reachable_goods_value;
    bandit_live_world::apply_shakedown_outcome( site, outcome );
    if( surface.includes_basecamp_inventory ) {
        bandit_live_world::begin_shakedown_basecamp_defender_observation( site,
                live_bandit_nearby_basecamp_defender_count( u ) );
    }

    const std::string summary = string_format( "shakedown_surface fight demanded=%d reachable=%d",
                                surface.demanded_value, surface.reachable_goods_value );
    DebugLog( D_INFO, DC_ALL ) << summary << '\n';
    for( const character_id &member_id : member_ids ) {
        bandit_live_world::update_member_state( site, member_id,
                                                bandit_live_world::member_state::local_contact, summary );
        if( npc *member_npc = g->find_npc( member_id ) ) {
            member_npc->set_attitude( NPCATT_KILL );
        }
    }
}

std::pair<std::string, nc_color> live_bandit_shakedown_speaker( const bandit_live_world::site_record &site )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return { _( "Bandit" ), c_red };
    }
    for( const character_id &member_id : outing->member_ids ) {
        if( const npc *member_npc = g->find_npc( member_id ) ) {
            return { member_npc->disp_name(), member_npc->basic_symbol_color() };
        }
    }
    return { _( "Bandit" ), c_red };
}

enum class live_bandit_shakedown_response : int {
    pay,
    fight,
};

live_bandit_shakedown_response query_live_bandit_shakedown_dialogue(
    const bandit_live_world::site_record &site,
    const bandit_live_world::shakedown_surface &surface )
{
    dialogue_window d_win;
    d_win.is_not_conversation = true;
    const std::pair<std::string, nc_color> speaker = live_bandit_shakedown_speaker( site );
    d_win.add_to_history( surface.bark, speaker.first, speaker.second );
    d_win.add_history_separator();
    d_win.add_to_history( string_format(
                              _( "Reachable goods: %1$d\nDemanded toll: %2$d\nOpening: %3$s" ),
                              surface.reachable_goods_value, surface.demanded_value,
                              surface.opening_summary ) );

    ui_adaptor ui;
    const auto resize_cb = [&]( ui_adaptor & ui ) {
        d_win.resize( ui );
    };
    ui.on_screen_resize( resize_cb );
    resize_cb( ui );

    std::vector<talk_data> responses;
    responses.push_back( talk_data{ c_light_green, "p", _( "Pay." ) } );
    responses.push_back( talk_data{ c_light_red, "f", _( "Fight." ) } );
    d_win.set_responses( responses );

    input_context ctxt( "DIALOGUE_CHOOSE_RESPONSE" );
    d_win.set_up_scrolling( ctxt );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "ANY_INPUT" );
    ctxt.register_action( "QUIT" );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        d_win.draw( speaker.first );
    } );

    while( true ) {
        ui_manager::redraw();
        std::string action = ctxt.handle_input();
        const input_event evt = ctxt.get_raw_input();
        d_win.handle_scrolling( action, ctxt );
        if( action == "CONFIRM" ) {
            if( d_win.sel_response == 0 ) {
                return live_bandit_shakedown_response::pay;
            }
            return live_bandit_shakedown_response::fight;
        }
        if( action == "QUIT" ) {
            return live_bandit_shakedown_response::fight;
        }
        if( action == "ANY_INPUT" && ( evt.type == input_event_t::keyboard_char ||
                                        evt.type == input_event_t::keyboard_code ) &&
            !evt.sequence.empty() ) {
            switch( evt.get_first_input() ) {
                case 'p':
                case 'P':
                    return live_bandit_shakedown_response::pay;
                case 'f':
                case 'F':
                    return live_bandit_shakedown_response::fight;
                default:
                    break;
            }
        }
    }
}

bool open_live_bandit_shakedown_surface( bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::local_gate_decision &decision )
{
    if( live_bandit_shakedown_already_opened( site ) ) {
        return false;
    }

    avatar &u = get_avatar();
    const bandit_live_world::shakedown_goods_pool pool =
        live_bandit_make_shakedown_goods_pool( input, u );
    const bandit_live_world::shakedown_surface surface =
        bandit_live_world::build_shakedown_surface( site, input, decision, pool );
    DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_shakedown_surface_report( site,
                               surface );
    if( !surface.valid ) {
        return false;
    }
    bandit_live_world::mark_shakedown_reopen_used( site );

    DebugLog( D_INFO, DC_ALL ) << "shakedown_surface_dialogue_window opening="
                               << ( surface.opening_id.empty() ? "none" : surface.opening_id )
                               << " responses=pay/fight payment_surface=npc_trade_ui\n";
    const live_bandit_shakedown_response response =
        query_live_bandit_shakedown_dialogue( site, surface );

    bool payment_failed = false;
    if( response == live_bandit_shakedown_response::pay ) {
        std::optional<live_bandit_paid_release_plan> release_plan =
            live_bandit_prepare_paid_release( site );
        if( !release_plan ) {
            payment_failed = true;
            add_msg( m_warning,
                     _( "The bandits cannot safely settle the demand.  The standoff turns into a fight." ) );
        } else {
            const int surrendered_value = live_bandit_select_shakedown_payment( site, input, surface, u );
            if( surrendered_value >= surface.demanded_value ) {
                add_msg( m_bad, _( "You complete the shakedown payment through trade." ) );
                live_bandit_commit_paid_release(
                    site, std::move( *release_plan ), surface, surrendered_value );
                return true;
            }
            payment_failed = true;
            add_msg( m_warning,
                     _( "You do not complete the shakedown payment.  The demand turns into a fight." ) );
        }
    }

    if( response == live_bandit_shakedown_response::fight ) {
        add_msg( m_bad, _( "You choose to fight the shakedown." ) );
    } else if( payment_failed ) {
        add_msg( m_bad, _( "The bandits come at you." ) );
    }
    live_bandit_choose_fight( site, surface, u );
    return true;
}

int live_bandit_current_minutes()
{
    return to_minutes<int>( calendar::turn - calendar::start_of_cataclysm );
}

bool live_bandit_member_routing_home( const npc &member_npc, const bandit_live_world::site_record &site )
{
    return member_npc.is_travelling() && member_npc.has_omt_destination() &&
           !member_npc.omt_path.empty() && site_contains_omt( site, member_npc.goal );
}

bool live_bandit_member_routing_burn_egress(
    const npc &member_npc, const bandit_live_world::active_outing_state &outing )
{
    return outing.phase == bandit_live_world::scout_phase::burned_withdrawal &&
           member_npc.pos_abs_omt() != outing.local_handoff.egress_omt &&
           member_npc.is_travelling() && member_npc.has_omt_destination() &&
           member_npc.goal == outing.local_handoff.egress_omt;
}

bool live_bandit_abandon_unreachable_return( character_id member_id );
bool live_bandit_abort_alternate_watch_reposition( character_id member_id );

std::unordered_set<tripoint_abs_omt> live_bandit_covert_route_exclusions(
    const bandit_live_world::active_outing_state &outing )
{
    std::unordered_set<tripoint_abs_omt> exclusions;
    if( outing.schema_version != 10 ) {
        return exclusions;
    }
    const std::optional<int> burn_ring_distance =
        bandit_live_world::target_footprint_watch_distance(
            outing.selected_watch_omt, outing.target_footprint );
    if( !burn_ring_distance || *burn_ring_distance <= 0 ) {
        exclusions.insert( outing.target_footprint.begin(), outing.target_footprint.end() );
        return exclusions;
    }
    const int radius = *burn_ring_distance - 1;
    for( const tripoint_abs_omt &target : outing.target_footprint ) {
        for( int dy = -radius; dy <= radius; ++dy ) {
            for( int dx = -radius; dx <= radius; ++dx ) {
                const tripoint_abs_omt candidate( target.x() + dx, target.y() + dy,
                                                  target.z() );
                const std::optional<int> distance =
                    bandit_live_world::target_footprint_watch_distance(
                        candidate, outing.target_footprint );
                if( distance && *distance < *burn_ring_distance ) {
                    exclusions.insert( candidate );
                }
            }
        }
    }
    return exclusions;
}

bool live_bandit_route_respects_covert_ring(
    const bandit_live_world::active_outing_state &outing,
    const std::vector<tripoint_abs_omt> &path )
{
    if( outing.schema_version != 10 ) {
        return true;
    }
    const std::optional<int> burn_ring_distance =
        bandit_live_world::target_footprint_watch_distance(
            outing.selected_watch_omt, outing.target_footprint );
    return burn_ring_distance &&
           std::all_of( path.begin(), path.end(), [&]( const tripoint_abs_omt &omt ) {
        const std::optional<int> distance =
            bandit_live_world::target_footprint_watch_distance(
                omt, outing.target_footprint );
        return distance && *distance >= *burn_ring_distance;
    } );
}

bool live_bandit_update_local_path( npc &member_npc, const tripoint_bub_ms &destination )
{
    bandit_live_world_probe::scoped_loaded_covert_member member_scope(
        bandit_live_world_probe::active() );
    return member_npc.update_path( destination, false, false );
}

bool live_bandit_update_local_path_avoiding(
    npc &member_npc, const pathfinding_target &destination,
    const std::function<bool( const tripoint_bub_ms & )> &additional_avoid )
{
    bandit_live_world_probe::scoped_loaded_covert_member member_scope(
        bandit_live_world_probe::active() );
    if( destination.contains( member_npc.pos_bub() ) ) {
        member_npc.path.clear();
        return true;
    }
    const std::function<bool( const tripoint_bub_ms & )> npc_avoid =
        member_npc.get_path_avoid();
    const auto combined_avoid = [&npc_avoid,
                &additional_avoid]( const tripoint_bub_ms & step ) {
        return npc_avoid( step ) || additional_avoid( step );
    };
    member_npc.path = get_map().route(
                          member_npc.pos_bub(), destination,
                          member_npc.get_pathfinding_settings( false ),
                          combined_avoid );
    return !member_npc.path.empty();
}

bool live_bandit_move_to_omt_destination_avoiding(
    npc &member_npc,
    const std::function<bool( const std::vector<tripoint_bub_ms> & )> &path_validator,
    const std::function<bool( const tripoint_bub_ms & )> &additional_avoid )
{
    map &here = get_map();
    const tripoint_abs_omt current_omt = member_npc.pos_abs_omt();
    if( member_npc.goal == npc::no_goal_point || member_npc.omt_path.empty() ||
        member_npc.goal == current_omt ) {
        member_npc.go_to_omt_destination( path_validator );
        return true;
    }
    if( !member_npc.path.empty() && path_validator( member_npc.path ) ) {
        member_npc.move_to_next();
        return true;
    }
    member_npc.path.clear();
    if( member_npc.omt_path.back() == current_omt ) {
        member_npc.omt_path.pop_back();
    }
    if( member_npc.omt_path.empty() ) {
        member_npc.move_pause();
        return false;
    }
    const tripoint_bub_ms next_center =
        here.get_bub( project_to<coords::ms>( member_npc.omt_path.back() ) ) +
        point( SEEX, SEEY );
    if( !here.inbounds( next_center ) ||
        !live_bandit_update_local_path_avoiding(
            member_npc, pathfinding_target::radius( next_center, 2 ), additional_avoid ) ||
        !path_validator( member_npc.path ) ) {
        member_npc.path.clear();
        member_npc.move_pause();
        return false;
    }
    member_npc.move_to_next();
    return true;
}

std::vector<tripoint_abs_omt> live_bandit_member_route_to(
    const npc &member_npc, const bandit_live_world::site_record &site,
    const tripoint_abs_omt &destination )
{
    std::unordered_set<tripoint_abs_omt> route_exclusions =
        live_bandit_covert_route_exclusions( site.active_outing );
    const bool remembered_egress_fallback = site.active_outing.schema_version == 10 &&
            ( site.active_outing.phase ==
              bandit_live_world::scout_phase::burned_withdrawal ||
              site.active_outing.phase ==
              bandit_live_world::scout_phase::returning_exposed ||
              site.active_outing.phase ==
              bandit_live_world::scout_phase::returning_report ||
              site.active_outing.phase ==
              bandit_live_world::scout_phase::returning_home );
    if( remembered_egress_fallback ) {
        route_exclusions.insert( site.active_outing.selected_watch_omt );
        route_exclusions.insert( site.active_outing.failed_covert_egress_omts.begin(),
                                 site.active_outing.failed_covert_egress_omts.end() );
        route_exclusions.insert( site.active_outing.failed_covert_egress_route_omts.begin(),
                                 site.active_outing.failed_covert_egress_route_omts.end() );
        route_exclusions.erase( member_npc.pos_abs_omt() );
    }
    std::vector<tripoint_abs_omt> path;
    {
        bandit_live_world_probe::scoped_section route_solve(
            bandit_live_world_probe::section::loaded_covert_overmap_route_solve );
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::loaded_covert_overmap_route_solves );
        path = overmap_buffer.get_travel_path(
                   member_npc.pos_abs_omt(), destination,
                   overmap_path_params::for_npc(), route_exclusions ).points;
    }
    if( !live_bandit_route_respects_covert_ring( site.active_outing, path ) ||
        ( site.active_outing.phase == bandit_live_world::scout_phase::burned_withdrawal &&
          !bandit_live_world::covert_scout_egress_route_respects_retry_memory(
              site.active_outing, member_npc.pos_abs_omt(), path, false ) ) ) {
        path.clear();
    }
    return path;
}

bool live_bandit_route_member_to( npc &member_npc,
                                  const bandit_live_world::site_record &site,
                                  const tripoint_abs_omt &destination )
{
    std::vector<tripoint_abs_omt> path = live_bandit_member_route_to(
                                            member_npc, site, destination );
    if( path.empty() ) {
        return false;
    }
    member_npc.goal = destination;
    member_npc.omt_path = std::move( path );
    member_npc.set_mission( NPC_MISSION_TRAVELLING );
    return true;
}

bool live_bandit_route_member_home( npc &member_npc, const bandit_live_world::site_record &site )
{
    if( site_contains_omt( site, member_npc.pos_abs_omt() ) ) {
        return true;
    }
    if( live_bandit_member_routing_home( member_npc, site ) &&
        live_bandit_route_respects_covert_ring(
            site.active_outing, member_npc.omt_path ) ) {
        return true;
    }
    member_npc.omt_path.clear();
    return live_bandit_route_member_to( member_npc, site, site.anchor );
}

struct live_bandit_covert_egress_plan {
    std::vector<bandit_live_world::covert_scout_egress_candidate> candidates;
    std::map<tripoint_abs_omt,
        std::map<character_id, std::vector<tripoint_abs_omt>>> routes;
};

live_bandit_covert_egress_plan live_bandit_plan_covert_egress(
    const bandit_live_world::site_record &site,
    const std::vector<bandit_live_world::covert_scout_burn_read> &reads )
{
    live_bandit_covert_egress_plan plan;
    const bandit_live_world::active_outing_state &outing = site.active_outing;
    const tripoint_abs_omt route_origin = outing.covert_egress_attempts > 0 ?
            outing.local_handoff.egress_omt : outing.selected_watch_omt;
    const std::optional<int> origin_distance =
        bandit_live_world::target_footprint_watch_distance(
            route_origin, outing.target_footprint );
    if( !origin_distance ) {
        return plan;
    }
    const int now_minutes = live_bandit_current_minutes();
    const std::unordered_set<tripoint_abs_omt> target_exclusions =
        live_bandit_covert_route_exclusions( outing );
    std::unordered_set<tripoint_abs_omt> retry_footing_exclusions(
        outing.failed_covert_egress_omts.begin(),
        outing.failed_covert_egress_omts.end() );
    if( outing.covert_egress_attempts > 0 ) {
        retry_footing_exclusions.insert( outing.selected_watch_omt );
        retry_footing_exclusions.insert(
            outing.current_covert_egress_route_omts.begin(),
            outing.current_covert_egress_route_omts.end() );
        retry_footing_exclusions.insert(
            outing.failed_covert_egress_route_omts.begin(),
            outing.failed_covert_egress_route_omts.end() );
    }
    const auto score_known_danger = [&](
        bandit_live_world::covert_scout_egress_candidate &candidate,
        const tripoint_abs_omt &route_omt ) {
        for( const bandit_live_world::sortie_observation &observation : outing.observations ) {
            const int observation_distance = observation.source_omt.z() == route_omt.z() ?
                    std::max( std::abs( observation.source_omt.x() - route_omt.x() ),
                              std::abs( observation.source_omt.y() - route_omt.y() ) ) :
                    std::numeric_limits<int>::max();
            const bool private_observer_present = observation.share_state ==
                    bandit_live_world::sortie_observation_share_state::observer_private &&
                    std::find( outing.member_ids.begin(), outing.member_ids.end(),
                               observation.observer_id ) != outing.member_ids.end() &&
                    !outing.member_is_resolved( observation.observer_id ) &&
                    std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(),
                               observation.observer_id ) == outing.casualty_ids.end() &&
                    std::any_of( reads.begin(), reads.end(),
            [&observation]( const bandit_live_world::covert_scout_burn_read &read ) {
                return read.npc_id == observation.observer_id && read.present;
            } );
            const bool observed_danger = observation.record_schema_version == 1 &&
                    observation.sense == bandit_live_world::sortie_observation_sense::visual &&
                    ( observation.share_state ==
                      bandit_live_world::sortie_observation_share_state::shared ||
                      private_observer_present ) &&
                    observation.target_revision == outing.target_lead_revision &&
                    observation.observed_minutes <= now_minutes &&
                    observation.expiry_minutes >= now_minutes &&
                    observation.observed_power_high > 0 &&
                    !observation.defender_ids.empty() &&
                    observation_distance <= observation.uncertainty_radius_omt &&
                    ( observation.kind ==
                      bandit_live_world::sortie_observation_kind::hard_danger ||
                      observation.kind ==
                      bandit_live_world::sortie_observation_kind::certainty );
            if( observed_danger ) {
                candidate.soft_danger = std::max(
                                            candidate.soft_danger,
                                            std::min( 200, observation.observed_power_high ) );
                candidate.hard_danger |= observation.kind ==
                                         bandit_live_world::sortie_observation_kind::hard_danger;
            }
        }
        for( const bandit_live_world::covert_scout_burn_read &read : reads ) {
            for( const tripoint_abs_omt &observer_position :
                 read.perceived_target_observer_positions ) {
                if( observer_position.z() != route_omt.z() ) {
                    continue;
                }
                const int observer_distance = std::max(
                                                  std::abs( observer_position.x() - route_omt.x() ),
                                                  std::abs( observer_position.y() - route_omt.y() ) );
                if( observer_distance <= 1 ) {
                    candidate.soft_danger = std::max( candidate.soft_danger, 1 );
                    candidate.hard_danger |= observer_distance == 0;
                }
            }
        }
    };

    for( int dy = -1; dy <= 1; ++dy ) {
        for( int dx = -1; dx <= 1; ++dx ) {
            if( dx == 0 && dy == 0 ) {
                continue;
            }
            bandit_live_world::covert_scout_egress_candidate candidate;
            candidate.omt = tripoint_abs_omt( route_origin.x() + dx,
                                              route_origin.y() + dy,
                                              route_origin.z() );
            const std::optional<int> candidate_distance =
                bandit_live_world::target_footprint_watch_distance(
                    candidate.omt, outing.target_footprint );
            if( !candidate_distance || *candidate_distance < *origin_distance ) {
                continue;
            }
            if( outing.covert_egress_attempts > 0 &&
                ( *candidate_distance <= *origin_distance ||
                  retry_footing_exclusions.count( candidate.omt ) > 0 ) ) {
                continue;
            }
            candidate.concealed = overmap_buffer.ter( candidate.omt )->get_see_cost() > 0;
            score_known_danger( candidate, candidate.omt );
            std::map<character_id, std::vector<tripoint_abs_omt>> routes;
            std::vector<tripoint_abs_omt> route_footprint;
            int maximum_route_cost = 0;
            bool all_routes_ready = true;
            for( const character_id member_id : outing.member_ids ) {
                if( outing.member_is_resolved( member_id ) ||
                    std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                    outing.casualty_ids.end() ) {
                    continue;
                }
                npc *member = g->find_npc( member_id );
                if( member == nullptr || member->is_dead() ) {
                    all_routes_ready = false;
                    break;
                }
                std::unordered_set<tripoint_abs_omt> member_exclusions = target_exclusions;
                member_exclusions.insert( retry_footing_exclusions.begin(),
                                          retry_footing_exclusions.end() );
                member_exclusions.erase( member->pos_abs_omt() );
                pf::simple_path<tripoint_abs_omt> path;
                {
                    bandit_live_world_probe::scoped_section route_solve(
                        bandit_live_world_probe::section::loaded_covert_overmap_route_solve );
                    bandit_live_world_probe::increment(
                        bandit_live_world_probe::counter::loaded_covert_overmap_route_solves );
                    path = overmap_buffer.get_travel_path(
                               member->pos_abs_omt(), candidate.omt,
                               overmap_path_params::for_npc(), member_exclusions );
                }
                if( path.points.empty() || path.cost < 0 ) {
                    all_routes_ready = false;
                    break;
                }
                if( !bandit_live_world::covert_scout_egress_route_respects_retry_memory(
                        outing, member->pos_abs_omt(), path.points, true ) ) {
                    all_routes_ready = false;
                    break;
                }
                for( const tripoint_abs_omt &route_omt : path.points ) {
                    if( route_omt != outing.selected_watch_omt ) {
                        score_known_danger( candidate, route_omt );
                    }
                    if( route_omt != member->pos_abs_omt() &&
                        std::find( route_footprint.begin(), route_footprint.end(), route_omt ) ==
                        route_footprint.end() ) {
                        route_footprint.push_back( route_omt );
                    }
                }
                if( route_footprint.size() > static_cast<std::size_t>(
                            bandit_live_world::covert_scout_egress_route_omt_cap() ) ) {
                    all_routes_ready = false;
                    break;
                }
                maximum_route_cost = std::max( maximum_route_cost, path.cost );
                routes.emplace( member_id, path.points );
            }
            candidate.reachable = all_routes_ready;
            candidate.route_cost = all_routes_ready ? maximum_route_cost : -1;
            if( all_routes_ready ) {
                std::sort( route_footprint.begin(), route_footprint.end(),
                []( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) {
                    return std::make_tuple( lhs.z(), lhs.y(), lhs.x() ) <
                           std::make_tuple( rhs.z(), rhs.y(), rhs.x() );
                } );
                candidate.route_omts = route_footprint;
                plan.routes.emplace( candidate.omt, std::move( routes ) );
            }
            plan.candidates.push_back( candidate );
        }
    }
    return plan;
}

bool live_bandit_fail_burned_egress( const character_id member_id )
{
    bandit_live_world::site_record *owner = nullptr;
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        if( site.active_outing.phase != bandit_live_world::scout_phase::burned_withdrawal ||
            std::find( site.active_outing.member_ids.begin(),
                       site.active_outing.member_ids.end(), member_id ) ==
            site.active_outing.member_ids.end() ) {
            continue;
        }
        if( owner != nullptr ) {
            return false;
        }
        owner = &site;
    }
    if( owner == nullptr ) {
        return false;
    }
    std::vector<bandit_live_world::covert_scout_burn_read> retry_reads;
    retry_reads.reserve( owner->active_outing.member_ids.size() );
    for( const character_id retry_member_id : owner->active_outing.member_ids ) {
        if( owner->active_outing.member_is_resolved( retry_member_id ) ||
            std::find( owner->active_outing.casualty_ids.begin(),
                       owner->active_outing.casualty_ids.end(), retry_member_id ) !=
            owner->active_outing.casualty_ids.end() ) {
            continue;
        }
        npc *member = g->find_npc( retry_member_id );
        if( member == nullptr || member->is_dead() ) {
            return false;
        }
        bandit_live_world::covert_scout_burn_read read;
        read.npc_id = retry_member_id;
        read.position = member->pos_abs_omt();
        read.present = true;
        retry_reads.push_back( read );
    }
    if( retry_reads.empty() ) {
        return false;
    }
    const live_bandit_covert_egress_plan plan = live_bandit_plan_covert_egress(
                *owner, retry_reads );
    std::vector<bandit_live_world::covert_scout_egress_candidate> retry_candidates =
        plan.candidates;
    retry_candidates.erase( std::remove_if( retry_candidates.begin(), retry_candidates.end(),
    [owner]( const bandit_live_world::covert_scout_egress_candidate &candidate ) {
        return candidate.omt == owner->active_outing.local_handoff.egress_omt ||
               std::find( owner->active_outing.failed_covert_egress_omts.begin(),
                          owner->active_outing.failed_covert_egress_omts.end(), candidate.omt ) !=
               owner->active_outing.failed_covert_egress_omts.end();
    } ), retry_candidates.end() );
    const std::optional<bandit_live_world::covert_scout_egress_candidate> expected_retry =
        bandit_live_world::select_covert_scout_egress(
            owner->active_outing.local_handoff.egress_omt,
            owner->active_outing.target_footprint, retry_candidates,
            owner->active_outing.selected_watch_omt );
    std::vector<std::pair<npc *, std::vector<tripoint_abs_omt>>> retry_bindings;
    if( expected_retry ) {
        const auto selected_routes = plan.routes.find( expected_retry->omt );
        if( selected_routes == plan.routes.end() ) {
            return false;
        }
        retry_bindings.reserve( owner->active_outing.member_ids.size() );
        for( const character_id retry_member_id : owner->active_outing.member_ids ) {
            if( owner->active_outing.member_is_resolved( retry_member_id ) ||
                std::find( owner->active_outing.casualty_ids.begin(),
                           owner->active_outing.casualty_ids.end(), retry_member_id ) !=
                owner->active_outing.casualty_ids.end() ) {
                continue;
            }
            npc *member = g->find_npc( retry_member_id );
            const auto member_route = selected_routes->second.find( retry_member_id );
            if( member == nullptr || member->is_dead() ||
                member_route == selected_routes->second.end() ) {
                return false;
            }
            retry_bindings.emplace_back( member, member_route->second );
        }
    }
    bool home_routes_ready = true;
    std::vector<std::pair<npc *, std::vector<tripoint_abs_omt>>> home_bindings;
    home_bindings.reserve( retry_reads.size() );
    std::unordered_set<tripoint_abs_omt> home_exclusions =
        live_bandit_covert_route_exclusions( owner->active_outing );
    home_exclusions.insert( owner->active_outing.selected_watch_omt );
    home_exclusions.insert( owner->active_outing.local_handoff.egress_omt );
    home_exclusions.insert( owner->active_outing.failed_covert_egress_omts.begin(),
                            owner->active_outing.failed_covert_egress_omts.end() );
    home_exclusions.insert( owner->active_outing.current_covert_egress_route_omts.begin(),
                            owner->active_outing.current_covert_egress_route_omts.end() );
    home_exclusions.insert( owner->active_outing.failed_covert_egress_route_omts.begin(),
                            owner->active_outing.failed_covert_egress_route_omts.end() );
    for( const bandit_live_world::covert_scout_burn_read &read : retry_reads ) {
        npc *member = g->find_npc( read.npc_id );
        if( member == nullptr || member->is_dead() ) {
            home_routes_ready = false;
            break;
        }
        std::unordered_set<tripoint_abs_omt> member_exclusions = home_exclusions;
        member_exclusions.erase( member->pos_abs_omt() );
        std::vector<tripoint_abs_omt> home_route;
        {
            bandit_live_world_probe::scoped_section route_solve(
                bandit_live_world_probe::section::loaded_covert_overmap_route_solve );
            bandit_live_world_probe::increment(
                bandit_live_world_probe::counter::loaded_covert_overmap_route_solves );
            home_route = overmap_buffer.get_travel_path(
                             member->pos_abs_omt(), owner->anchor,
                             overmap_path_params::for_npc(), member_exclusions ).points;
        }
        if( home_route.empty() ||
            !live_bandit_route_respects_covert_ring( owner->active_outing, home_route ) ) {
            home_routes_ready = false;
            break;
        }
        home_bindings.emplace_back( member, std::move( home_route ) );
    }
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( *owner );
    if( !cursor ) {
        return false;
    }
    const bandit_live_world::covert_scout_egress_failure_effect effect =
        bandit_live_world::resolve_covert_scout_burned_egress_failure(
            *owner, *cursor, retry_candidates, live_bandit_current_minutes() );
    if( effect.result ==
        bandit_live_world::covert_scout_egress_failure_result::rejected ) {
        return false;
    }
    if( effect.result ==
        bandit_live_world::covert_scout_egress_failure_result::retried ) {
        for( std::pair<npc *, std::vector<tripoint_abs_omt>> &binding : retry_bindings ) {
            npc *member = binding.first;
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
            member->goal = effect.egress_omt;
            member->omt_path = std::move( binding.second );
            member->set_mission( NPC_MISSION_TRAVELLING );
        }
    } else if( home_routes_ready && home_bindings.size() == retry_reads.size() ) {
        for( std::pair<npc *, std::vector<tripoint_abs_omt>> &binding : home_bindings ) {
            npc *member = binding.first;
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
            member->goal = owner->anchor;
            member->omt_path = std::move( binding.second );
            member->set_mission( NPC_MISSION_TRAVELLING );
        }
    }
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world covert_egress_failure"
                               << " site=" << owner->site_id
                               << " activity=" << owner->active_outing.activity_id
                               << " failed=" << effect.failed_egress_omt.to_string()
                               << " next=" << effect.egress_omt.to_string()
                               << " attempts=" << owner->active_outing.covert_egress_attempts
                               << " result=" << ( effect.result ==
                                      bandit_live_world::covert_scout_egress_failure_result::retried ?
                                      "retried" : "exhausted" ) << '\n';
    if( effect.result ==
        bandit_live_world::covert_scout_egress_failure_result::exhausted &&
        ( !home_routes_ready || home_bindings.size() != retry_reads.size() ) ) {
        return live_bandit_abandon_unreachable_return( member_id );
    }
    return true;
}

std::optional<std::vector<bandit_live_world::active_member_observation>>
live_bandit_read_unreachable_return_members( const bandit_live_world::site_record &site,
        const int current_minutes )
{
    std::vector<bandit_live_world::active_member_observation> observations;
    observations.reserve( site.active_outing.member_ids.size() );
    for( const character_id member_id : site.active_outing.member_ids ) {
        bandit_live_world::active_member_observation observation;
        observation.npc_id = member_id;
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member == nullptr ) {
            return std::nullopt;
        }
        if( site.active_outing.member_is_resolved( member_id ) ) {
            if( member->state == bandit_live_world::member_state::at_home ) {
                observation.state = bandit_live_world::active_member_observation_state::home;
                observation.summary = "persisted return arrival";
            } else if( member->state == bandit_live_world::member_state::dead ) {
                observation.state = bandit_live_world::active_member_observation_state::dead;
                observation.summary = "persisted return casualty dead";
            } else if( member->state == bandit_live_world::member_state::missing ) {
                observation.state = bandit_live_world::active_member_observation_state::missing;
                observation.summary = "persisted return casualty missing";
            } else {
                return std::nullopt;
            }
            observations.push_back( observation );
            continue;
        }
        const npc *member_npc = g->find_npc( member_id );
        if( member_npc == nullptr ) {
            if( site.active_outing.missing_deadline_minutes < 0 ||
                current_minutes < site.active_outing.missing_deadline_minutes ) {
                return std::nullopt;
            }
            observation.state = bandit_live_world::active_member_observation_state::missing;
            observation.summary = "returning scout absent beyond persisted missing grace";
        } else if( member_npc->is_dead() ) {
            observation.state = bandit_live_world::active_member_observation_state::dead;
            observation.summary = "returning scout npc dead";
        } else if( site_contains_omt( site, member_npc->pos_abs_omt() ) ) {
            observation.state = bandit_live_world::active_member_observation_state::home;
            observation.summary = "returning scout physically on camp footprint";
        } else {
            observation.state = bandit_live_world::active_member_observation_state::returning_home;
            observation.summary = "living returning scout stranded away from camp";
        }
        observations.push_back( observation );
    }
    return observations;
}

bool live_bandit_abandon_unreachable_return( const character_id member_id )
{
    bandit_live_world::site_record *owner = nullptr;
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        if( site.active_outing.schema_version != 10 ||
            site.active_outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            site.active_outing.owner != bandit_live_world::simulation_owner::local ||
            ( site.active_outing.phase != bandit_live_world::scout_phase::returning_exposed &&
              site.active_outing.phase != bandit_live_world::scout_phase::returning_report &&
              site.active_outing.phase != bandit_live_world::scout_phase::returning_home ) ||
            std::find( site.active_outing.member_ids.begin(),
                       site.active_outing.member_ids.end(), member_id ) ==
            site.active_outing.member_ids.end() ) {
            continue;
        }
        if( owner != nullptr ) {
            return false;
        }
        owner = &site;
    }
    if( owner == nullptr ) {
        return false;
    }
    const std::vector<character_id> stranded_ids = owner->active_outing.member_ids;
    const int current_minutes = live_bandit_current_minutes();
    const std::optional<std::vector<bandit_live_world::active_member_observation>> observations =
        live_bandit_read_unreachable_return_members( *owner, current_minutes );
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( *owner );
    if( !observations || !cursor ||
        !bandit_live_world::abandon_covert_scout_unreachable_return(
            *owner, *cursor, *observations, current_minutes ) ) {
        return false;
    }
    for( const character_id stranded_id : stranded_ids ) {
        if( npc *member_npc = g->find_npc( stranded_id ) ) {
            member_npc->path.clear();
            member_npc->omt_path.clear();
            member_npc->goal = npc::no_goal_point;
            member_npc->set_guard_pos( member_npc->pos_abs() );
            member_npc->set_mission( NPC_MISSION_GUARD );
        }
    }
    return true;
}

bool live_bandit_abort_alternate_watch_reposition( const character_id member_id )
{
    bandit_live_world::site_record *owner = nullptr;
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        if( site.active_outing.kind !=
            bandit_live_world::outing_kind::structural_sortie ||
            site.active_outing.schema_version != 10 ||
            site.active_outing.owner != bandit_live_world::simulation_owner::local ||
            !site.active_outing.alternate_watch_reposition_pending ||
            std::find( site.active_outing.member_ids.begin(),
                       site.active_outing.member_ids.end(), member_id ) ==
            site.active_outing.member_ids.end() ) {
            continue;
        }
        if( owner != nullptr ) {
            return false;
        }
        owner = &site;
    }
    if( owner == nullptr ) {
        return false;
    }
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( *owner );
    if( !cursor ||
        bandit_live_world::abort_local_pair_alternate_watch_reposition(
            *owner, *cursor, live_bandit_current_minutes(),
            "alternate watch physical route became unavailable" ) !=
        bandit_live_world::local_handoff_commit_result::applied ) {
        return false;
    }
    for( const character_id routed_member_id : owner->active_outing.member_ids ) {
        if( npc *member_npc = g->find_npc( routed_member_id ) ) {
            member_npc->omt_path.clear();
            live_bandit_route_member_home( *member_npc, *owner );
        }
    }
    return true;
}

bool live_bandit_seen_by_nearby_ally( const map &here, const avatar &u,
                                      const tripoint_bub_ms &target )
{
    static constexpr int nearby_observer_radius = 30;
    for( const npc &guy : g->all_npcs() ) {
        if( !guy.is_player_ally() || guy.is_dead() ||
            rl_dist( guy.pos_abs(), u.pos_abs() ) > nearby_observer_radius ) {
            continue;
        }
        if( guy.sees( here, target ) ) {
            return true;
        }
    }
    return false;
}

bool live_bandit_try_sight_avoid_reposition( npc &member_npc,
        const bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &gate_input,
        const bandit_live_world::local_gate_decision &gate_decision )
{
    if( gate_decision.posture != bandit_live_world::local_gate_posture::stalk &&
        gate_decision.posture != bandit_live_world::local_gate_posture::hold_off ) {
        return false;
    }

    map &here = get_map();
    avatar &u = get_avatar();
    const tripoint_bub_ms current = member_npc.pos_bub( here );
    const bool current_player_exposure = get_player_view().sees( here, current );
    const bool current_camp_exposure = live_bandit_seen_by_nearby_ally( here, u, current );
    const bool current_exposure = current_player_exposure || current_camp_exposure;
    if( !current_exposure && !gate_input.recent_exposure && !gate_input.smoke_obscured_lead ) {
        return false;
    }

    const int current_player_distance = rl_dist( current, u.pos_bub( here ) );
    std::vector<bandit_live_world::sight_avoid_candidate> candidates;
    for( const tripoint_bub_ms &candidate_tile : here.points_in_radius( current, 1 ) ) {
        if( candidate_tile == current ) {
            continue;
        }
        bandit_live_world::sight_avoid_candidate candidate;
        candidate.tile = here.get_abs( candidate_tile );
        candidate.passable = member_npc.can_move_to( candidate_tile, true );
        candidate.visible_to_player = get_player_view().sees( here, candidate_tile );
        candidate.visible_to_camp = live_bandit_seen_by_nearby_ally( here, u, candidate_tile );
        candidate.cover_score = rl_dist( candidate_tile, u.pos_bub( here ) ) - current_player_distance;
        candidate.smoke_obscured = live_bandit_tile_has_smoke( here, candidate_tile );
        candidates.push_back( candidate );
    }

    int passable_candidate_count = 0;
    int smoke_clear_candidate_count = 0;
    for( const bandit_live_world::sight_avoid_candidate &candidate : candidates ) {
        if( candidate.passable ) {
            passable_candidate_count++;
            if( !candidate.smoke_obscured ) {
                smoke_clear_candidate_count++;
            }
        }
    }

    const bandit_live_world::sight_avoid_decision decision =
        bandit_live_world::choose_sight_avoid_reposition( member_npc.pos_abs(), current_exposure,
                gate_input.recent_exposure, candidates, gate_input.smoke_obscured_lead );
    if( !decision.repositions ) {
        const bool blocked_reposition = decision.reason.rfind( "blocked:", 0 ) == 0;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sight_avoid: "
                                   << ( blocked_reposition ? "blocked" : "still stalking" )
                                   << " site=" << site.site_id
                                   << " active_group=" << site.active_outing.activity_id
                                   << " active_job=" << site.active_outing.job_type
                                   << " profile=" << bandit_live_world::to_string( site.profile )
                                   << " posture=" << bandit_live_world::to_string( gate_decision.posture )
                                   << " npc=" << member_npc.getID().get_value() << " reason=" << decision.reason
                                   << " blocked_reposition=" << ( blocked_reposition ? "yes" : "no" )
                                   << " blocked_no_cover=" << ( current_exposure || gate_input.recent_exposure ||
                                          gate_input.smoke_obscured_lead ? "yes" : "no" )
                                   << " candidates=" << candidates.size()
                                   << " passable_candidates=" << passable_candidate_count
                                   << " smoke_clear_candidates=" << smoke_clear_candidate_count
                                   << " current_exposure=" << ( current_exposure ? "yes" : "no" )
                                   << " player_exposure=" << ( current_player_exposure ? "yes" : "no" )
                                   << " camp_exposure=" << ( current_camp_exposure ? "yes" : "no" )
                                   << " recent_exposure=" << ( gate_input.recent_exposure ? "yes" : "no" )
                                   << " smoke_obscured=" << ( gate_input.smoke_obscured_lead ? "yes" : "no" )
                                   << " smoke_on_watcher=" << ( gate_input.smoke_on_watcher_tile ? "yes" : "no" )
                                   << " smoke_sightline=" << ( gate_input.smoke_between_watcher_and_camp ? "yes" : "no" )
                                   << " shakedown=" << ( gate_decision.opens_shakedown_surface ? "yes" : "no" )
                                   << " combat_forward=" << ( gate_decision.combat_forward ? "yes" : "no" )
                                   << '\n';
        return false;
    }

    const tripoint_bub_ms destination_bub = here.get_bub( decision.destination );
    member_npc.move_to( destination_bub, true );
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sight_avoid: "
                               << ( gate_input.smoke_obscured_lead ? "smoke-obscured" : "exposed" )
                               << " -> repositioned"
                               << " site=" << site.site_id
                               << " active_group=" << site.active_outing.activity_id
                               << " active_job=" << site.active_outing.job_type
                               << " profile=" << bandit_live_world::to_string( site.profile )
                               << " posture=" << bandit_live_world::to_string( gate_decision.posture )
                               << " npc=" << member_npc.getID().get_value() << " from="
                               << current.to_string_writable() << " to="
                               << destination_bub.to_string_writable()
                               << " distance=" << rl_dist( current, destination_bub )
                               << " reason=" << decision.reason
                               << " current_exposure=" << ( current_exposure ? "yes" : "no" )
                               << " player_exposure=" << ( current_player_exposure ? "yes" : "no" )
                               << " camp_exposure=" << ( current_camp_exposure ? "yes" : "no" )
                               << " recent_exposure=" << ( gate_input.recent_exposure ? "yes" : "no" )
                               << " smoke_obscured=" << ( gate_input.smoke_obscured_lead ? "yes" : "no" )
                               << " smoke_on_watcher=" << ( gate_input.smoke_on_watcher_tile ? "yes" : "no" )
                               << " smoke_sightline=" << ( gate_input.smoke_between_watcher_and_camp ? "yes" : "no" )
                               << " shakedown=" << ( gate_decision.opens_shakedown_surface ? "yes" : "no" )
                               << " combat_forward=" << ( gate_decision.combat_forward ? "yes" : "no" ) << '\n';
    return true;
}

bool live_bandit_try_fight_advance( npc &member_npc,
                                    const bandit_live_world::site_record &site,
                                    const bandit_live_world::local_gate_input &gate_input,
                                    const bandit_live_world::local_gate_decision &gate_decision )
{
    if( !gate_decision.combat_forward ) {
        return false;
    }

    avatar &u = get_avatar();
    map &here = get_map();
    member_npc.set_attitude( NPCATT_KILL );
    const tripoint_bub_ms before = member_npc.pos_bub( here );
    const bool adjacent = rl_dist( member_npc.pos_abs(), u.pos_abs() ) <= 1;
    const bool sees_player = member_npc.sees( here, u );
    bool path_found = false;
    bool moved = false;
    bool attacked = false;
    if( adjacent && sees_player && !member_npc.has_flag( json_flag_CANNOT_ATTACK ) ) {
        attacked = member_npc.melee_attack( u, true );
    } else if( !adjacent && !member_npc.has_flag( json_flag_CANNOT_MOVE ) ) {
        path_found = member_npc.update_path( u.pos_bub( here ), false );
        if( path_found ) {
            member_npc.move_to_next();
            moved = member_npc.pos_bub( here ) != before;
        }
    }

    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world shakedown_fight_advance"
                               << " site=" << site.site_id
                               << " active_group=" << ( site.active_external_outing() == nullptr ?
                                      "none" : site.active_external_outing()->activity_id )
                               << " profile=" << bandit_live_world::to_string( site.profile )
                               << " posture=" << bandit_live_world::to_string( gate_decision.posture )
                               << " npc=" << member_npc.getID().get_value()
                               << " from=" << before.to_string_writable()
                               << " to=" << member_npc.pos_bub( here ).to_string_writable()
                               << " target=" << u.pos_bub( here ).to_string_writable()
                               << " moved=" << ( moved ? "yes" : "no" )
                               << " attacked=" << ( attacked ? "yes" : "no" )
                               << " path_found=" << ( path_found ? "yes" : "no" )
                               << " adjacent=" << ( adjacent ? "yes" : "no" )
                               << " sees_player=" << ( sees_player ? "yes" : "no" )
                               << " basecamp_or_camp=" << ( gate_input.basecamp_or_camp_scene ? "yes" : "no" )
                               << " combat_forward=" << ( gate_decision.combat_forward ? "yes" : "no" )
                               << '\n';
    return attacked || moved || path_found || adjacent;
}

bool live_bandit_handle_hostile_shakedown_contact( bandit_live_world::site_record &site,
        const avatar &u )
{
    bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( site.retired_empty_site || outing == nullptr || !outing->is_active() ||
        outing != &site.active_hostile_operation.reservation ||
        site.active_hostile_operation.operation_kind !=
        bandit_live_world::hostile_operation_kind::shakedown ||
        outing->owner != bandit_live_world::simulation_owner::local ) {
        return false;
    }

    const bool has_loaded_local_contact = std::any_of(
            outing->member_ids.begin(), outing->member_ids.end(), [&site]( const character_id member_id ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        npc *member_npc = g->find_npc( member_id );
        return member != nullptr && member->state == bandit_live_world::member_state::local_contact &&
               member_npc != nullptr && !member_npc->is_dead();
    } );
    if( !has_loaded_local_contact ) {
        return false;
    }

    bandit_live_world::local_gate_input gate_input = live_bandit_make_gate_input( site, u );
    gate_input.local_contact_established = true;
    const bandit_live_world::local_gate_decision gate_decision =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    if( gate_decision.combat_forward ) {
        bool changed = false;
        for( const character_id member_id : outing->member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                continue;
            }
            if( npc *member_npc = g->find_npc( member_id ) ) {
                changed |= live_bandit_try_fight_advance( *member_npc, site, gate_input,
                           gate_decision );
            }
        }
        return changed;
    }
    if( gate_decision.opens_shakedown_surface ) {
        return open_live_bandit_shakedown_surface( site, gate_input, gate_decision );
    }
    return false;
}

bool live_bandit_apply_shakedown_defender_aftermath( bandit_live_world::site_record &site,
        const avatar &u )
{
    if( site.retired_empty_site || !site.shakedown_basecamp_defender_observation_pending ) {
        return false;
    }
    const int live_defenders = live_bandit_nearby_basecamp_defender_count( u );
    const bandit_live_world::shakedown_aftermath_effect defender_effect =
        bandit_live_world::apply_shakedown_basecamp_defender_observation( site, live_defenders );
    if( !defender_effect.valid ) {
        return false;
    }
    DebugLog( D_INFO, DC_ALL )
            << "bandit shakedown aftermath: basecamp defender strength dropped from "
            << site.shakedown_basecamp_defenders_at_fight << " to " << live_defenders
            << "; stronger reopen available="
            << ( site.shakedown_reopen_available ? "yes" : "no" );
    return true;
}

bool note_live_bandit_local_turn_sight_avoid()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state *outing = site.active_external_outing();
        if( site.retired_empty_site || outing == nullptr || !outing->is_active() ||
            outing->owner != bandit_live_world::simulation_owner::local ||
            outing->member_ids.empty() ||
            bandit_live_world::active_outing_requires_homeward_routing( *outing ) ) {
            continue;
        }

        bool has_loaded_local_contact_member = false;
        for( const character_id &member_id : outing->member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                continue;
            }
            npc *member_npc = g->find_npc( member_id );
            if( member_npc != nullptr && !member_npc->is_dead() ) {
                has_loaded_local_contact_member = true;
                break;
            }
        }
        if( !has_loaded_local_contact_member ) {
            continue;
        }

        bandit_live_world::local_gate_input gate_input = live_bandit_make_gate_input( site,
                get_avatar() );
        gate_input.local_contact_established = true;
        const bandit_live_world::local_gate_decision gate_decision =
            bandit_live_world::choose_local_gate_posture( site, gate_input );
        DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_local_gate_report( site, gate_input,
                                   gate_decision )
                                   << "- live_existing_active_group=yes\n";
        if( gate_decision.combat_forward ) {
            for( const character_id &member_id : outing->member_ids ) {
                const bandit_live_world::member_record *member = site.find_member( member_id );
                if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                    continue;
                }
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    changed |= live_bandit_try_fight_advance( *member_npc, site, gate_input,
                               gate_decision );
                }
            }
            continue;
        }
        if( gate_decision.opens_shakedown_surface ) {
            changed |= open_live_bandit_shakedown_surface( site, gate_input, gate_decision );
            continue;
        }
        if( gate_decision.posture != bandit_live_world::local_gate_posture::stalk &&
            gate_decision.posture != bandit_live_world::local_gate_posture::hold_off ) {
            continue;
        }
        for( const character_id &member_id : outing->member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                continue;
            }
            if( npc *member_npc = g->find_npc( member_id ) ) {
                changed |= live_bandit_try_sight_avoid_reposition( *member_npc, site, gate_input,
                           gate_decision );
            }
        }
    }
    return changed;
}

int burn_live_bandit_covert_scouts()
{
    struct target_character {
        const Character *actor = nullptr;
        std::string stable_id;
    };

    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    std::set<character_id> claimed_members;
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            return 0;
        }
    }

    avatar &u = get_avatar();
    map &here = get_map();
    std::vector<target_character> target_characters = { { &u, "avatar" } };
    for( const npc &defender : g->all_npcs() ) {
        if( defender.is_player_ally() && !defender.is_dead() && defender.is_active() &&
            here.inbounds( defender.pos_bub( here ) ) ) {
            target_characters.push_back( { &defender, "npc:" +
                                           std::to_string( defender.getID().get_value() ) } );
        }
    }
    std::sort( target_characters.begin(), target_characters.end(),
    []( const target_character & lhs, const target_character & rhs ) {
        return lhs.stable_id < rhs.stable_id;
    } );

    int burned = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool targets_player_camp = std::any_of(
            outing.target_footprint.begin(), outing.target_footprint.end(),
        []( const tripoint_abs_omt & target_omt ) {
            return overmap_buffer.is_player_camp_omt( target_omt );
        } );
        if( site.retired_empty_site || !targets_player_camp || outing.schema_version != 10 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }
        std::vector<target_character> bounded_target_characters = target_characters;
        std::sort( bounded_target_characters.begin(), bounded_target_characters.end(),
        [&u, &outing]( const target_character & lhs, const target_character & rhs ) {
            return std::make_tuple( lhs.actor == &u ? 0 : 1,
                                    rl_dist( lhs.actor->pos_abs_omt(),
                                             outing.selected_watch_omt ), lhs.stable_id ) <
                   std::make_tuple( rhs.actor == &u ? 0 : 1,
                                    rl_dist( rhs.actor->pos_abs_omt(),
                                             outing.selected_watch_omt ), rhs.stable_id );
        } );
        bounded_target_characters.resize( std::min<std::size_t>(
                                               bounded_target_characters.size(),
                                               bandit_live_world::covert_scout_burn_observer_cap() ) );
        std::vector<target_character> bounded_target_observers;
        std::copy_if( target_characters.begin(), target_characters.end(),
                      std::back_inserter( bounded_target_observers ),
        []( const target_character & target ) {
            return live_bandit_can_make_ordinary_visual_observation( *target.actor );
        } );
        std::sort( bounded_target_observers.begin(), bounded_target_observers.end(),
        [&u, &outing]( const target_character & lhs, const target_character & rhs ) {
            return std::make_tuple( lhs.actor == &u ? 0 : 1,
                                    rl_dist( lhs.actor->pos_abs_omt(),
                                             outing.selected_watch_omt ), lhs.stable_id ) <
                   std::make_tuple( rhs.actor == &u ? 0 : 1,
                                    rl_dist( rhs.actor->pos_abs_omt(),
                                             outing.selected_watch_omt ), rhs.stable_id );
        } );
        bounded_target_observers.resize( std::min<std::size_t>(
                                             bounded_target_observers.size(),
                                             bandit_live_world::covert_scout_burn_observer_cap() ) );

        std::vector<bandit_live_world::covert_scout_burn_read> reads;
        reads.reserve( outing.member_ids.size() );
        for( const character_id member_id : outing.member_ids ) {
            bandit_live_world::covert_scout_burn_read read;
            read.npc_id = member_id;
            npc *member = g->find_npc( member_id );
            if( outing.alternate_watch_reposition_pending && member != nullptr &&
                !member->is_dead() ) {
                // A split companion outside the active bubble cannot contribute sight, but its
                // exact authoritative NPC position still guards the one-party burn transaction.
                read.position = member->pos_abs_omt();
            }
            if( member != nullptr && !member->is_dead() &&
                member->has_ecology_covert_noncombat_relationship( u ) && member->is_active() &&
                here.inbounds( member->pos_bub( here ) ) ) {
                read.present = true;
                read.position = member->pos_abs_omt();
                const bool scout_can_observe =
                    live_bandit_can_make_ordinary_visual_observation( *member );
                for( const target_character &observer : bounded_target_observers ) {
                    if( observer.actor == member || observer.actor->is_dead_state() ) {
                        continue;
                    }
                    const bool target_saw_scout =
                        observer.actor->sees_without_clairvoyance( here, *member );
                    const bool scout_saw_target =
                        scout_can_observe &&
                        member->sees_without_clairvoyance( here, *observer.actor );
                    if( scout_saw_target ) {
                        const tripoint_abs_omt observer_position =
                            observer.actor->pos_abs_omt();
                        if( std::find( read.perceived_target_observer_positions.begin(),
                                      read.perceived_target_observer_positions.end(),
                                      observer_position ) ==
                            read.perceived_target_observer_positions.end() ) {
                            read.perceived_target_observer_positions.push_back(
                                observer_position );
                            std::sort(
                                read.perceived_target_observer_positions.begin(),
                                read.perceived_target_observer_positions.end(),
                            [&read]( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) {
                                return std::make_tuple( rl_dist( lhs, read.position ),
                                                        lhs.z(), lhs.y(), lhs.x() ) <
                                       std::make_tuple( rl_dist( rhs, read.position ),
                                                        rhs.z(), rhs.y(), rhs.x() );
                            } );
                            read.perceived_target_observer_positions.resize(
                                std::min<std::size_t>(
                                    read.perceived_target_observer_positions.size(),
                                    bandit_live_world::covert_scout_burn_observer_cap() ) );
                        }
                    }
                    if( ( target_saw_scout || scout_saw_target ) &&
                        read.target_observer_id.empty() ) {
                        read.target_observer_id = observer.stable_id;
                        read.target_observer_position = observer.actor->pos_abs_omt();
                        read.target_saw_scout = target_saw_scout;
                        read.scout_saw_target = scout_saw_target;
                    }
                    if( target_saw_scout && scout_saw_target &&
                        !( read.target_saw_scout && read.scout_saw_target ) ) {
                        read.target_observer_id = observer.stable_id;
                        read.target_observer_position = observer.actor->pos_abs_omt();
                        read.target_saw_scout = true;
                        read.scout_saw_target = true;
                    }
                }
                if( scout_can_observe ) {
                    const bool scout_has_gun = member->get_wielded_item() &&
                                               member->get_wielded_item()->is_gun();
                    const auto append_visible_defender = [&]( const target_character & defender ) {
                        if( defender.actor == member || defender.actor->is_dead_state() ||
                            !member->sees_without_clairvoyance( here, *defender.actor ) ) {
                            return;
                        }
                        bandit_live_world::covert_scout_burn_read::visible_defender_read
                        defender_read;
                        defender_read.stable_id = defender.stable_id;
                        defender_read.position = defender.actor->pos_abs_omt();
                        defender_read.normalized_power =
                            bandit_live_world::normalize_hostile_camp_character_power(
                                member->evaluate_character_threat_without_perception_fuzz(
                                    *defender.actor, scout_has_gun, true ) );
                        const item_location defender_weapon = defender.actor->get_wielded_item();
                        defender_read.equipment_detail = !defender_weapon ? 0 :
                                                         defender_weapon->is_gun() ? 2 : 1;
                        read.visible_defenders.push_back( std::move( defender_read ) );
                    };
                    for( const target_character &defender : bounded_target_characters ) {
                        append_visible_defender( defender );
                    }
                    const bool selected_observer_retained = std::any_of(
                            read.visible_defenders.begin(), read.visible_defenders.end(),
                    [&read]( const auto & defender ) {
                        return defender.stable_id == read.target_observer_id;
                    } );
                    if( read.scout_saw_target && !selected_observer_retained ) {
                        const auto selected_observer = std::find_if(
                                                           target_characters.begin(),
                                                           target_characters.end(),
                        [&read]( const target_character & target ) {
                            return target.stable_id == read.target_observer_id;
                        } );
                        if( selected_observer != target_characters.end() ) {
                            if( read.visible_defenders.size() ==
                                static_cast<std::size_t>(
                                    bandit_live_world::covert_scout_burn_observer_cap() ) ) {
                                read.visible_defenders.pop_back();
                            }
                            append_visible_defender( *selected_observer );
                        }
                    }
                    std::sort( read.visible_defenders.begin(), read.visible_defenders.end(),
                    []( const auto & lhs, const auto & rhs ) {
                        return lhs.stable_id < rhs.stable_id;
                    } );
                }
                if( read.target_saw_scout && read.scout_saw_target &&
                    std::find( read.perceived_target_observer_positions.begin(),
                               read.perceived_target_observer_positions.end(),
                               read.target_observer_position ) ==
                    read.perceived_target_observer_positions.end() ) {
                    if( read.perceived_target_observer_positions.size() ==
                        static_cast<std::size_t>(
                            bandit_live_world::covert_scout_burn_observer_cap() ) ) {
                        read.perceived_target_observer_positions.pop_back();
                    }
                    read.perceived_target_observer_positions.push_back(
                        read.target_observer_position );
                }
            }
            reads.push_back( std::move( read ) );
        }

        std::map<character_id, std::vector<tripoint_abs_omt>> egress_routes;
        std::vector<bandit_live_world::covert_scout_egress_candidate> egress_candidates;
        const bool reciprocal_exposure = std::any_of( reads.begin(), reads.end(),
        []( const bandit_live_world::covert_scout_burn_read & read ) {
            return read.target_saw_scout && read.scout_saw_target;
        } );
        if( reciprocal_exposure &&
            !site.active_outing.alternate_watch_reposition_pending ) {
            live_bandit_covert_egress_plan plan = live_bandit_plan_covert_egress( site, reads );
            egress_candidates = plan.candidates;
            const std::optional<bandit_live_world::covert_scout_egress_candidate> selected =
                bandit_live_world::select_covert_scout_egress(
                    site.active_outing.selected_watch_omt,
                    site.active_outing.target_footprint, egress_candidates );
            if( selected ) {
                egress_routes = std::move( plan.routes.at( selected->omt ) );
            }
        }
        const int current_minutes = live_bandit_current_minutes();
        const std::optional<bandit_live_world::structural_local_zombie_read> danger_read =
            reciprocal_exposure && current_minutes > cursor->last_advanced_minutes ?
            bandit_live_world::read_live_structural_local_zombie_observation( site ) :
            std::nullopt;
        if( danger_read ) {
            struct watch_exit_member_backup {
                shared_ptr_fast<npc> member;
                tripoint_abs_ms position;
                tripoint_abs_omt goal;
                std::vector<tripoint_abs_omt> omt_path;
                npc_mission mission = NPC_MISSION_NULL;
                npc_mission previous_mission = NPC_MISSION_NULL;
                std::optional<tripoint_abs_ms> ordered_position;
                std::optional<tripoint_abs_ms> ai_guard_position;
                std::vector<tripoint_bub_ms> local_path;
            };
            std::vector<watch_exit_member_backup> backups;
            std::map<character_id, std::vector<tripoint_abs_omt>> home_routes;
            bool home_routes_ready = true;
            for( const character_id member_id : site.active_outing.member_ids ) {
                shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
                if( !member || member->is_dead() ) {
                    home_routes_ready = false;
                    continue;
                }
                backups.push_back( { member, member->pos_abs(), member->goal,
                                     member->omt_path, member->mission,
                                     member->previous_mission, member->goto_to_this_pos,
                                     member->get_ai_guard_pos(), member->path } );
                std::vector<tripoint_abs_omt> route = live_bandit_member_route_to(
                            *member, site, site.anchor );
                if( route.empty() ) {
                    home_routes_ready = false;
                    continue;
                }
                home_routes.emplace( member_id, std::move( route ) );
            }
            home_routes_ready &= backups.size() == site.active_outing.member_ids.size() &&
                                 home_routes.size() == site.active_outing.member_ids.size();
            const std::optional<std::vector<bandit_live_world::active_member_observation>>
            unreachable_reads = home_routes_ready ? std::nullopt :
                                live_bandit_read_unreachable_return_members(
                                    site, current_minutes );
            const bandit_live_world::local_structural_watch_exit_plan exit_plan =
                bandit_live_world::plan_local_structural_watch_exit(
                    site, *cursor, reads, egress_candidates, *danger_read,
                    current_minutes, home_routes_ready,
                    unreachable_reads.value_or(
                        std::vector<bandit_live_world::active_member_observation>() ) );
            if( exit_plan.applicable ) {
                if( !exit_plan.valid ) {
                    continue;
                }
                const auto find_backup = [&backups]( const character_id member_id ) {
                    return std::find_if( backups.begin(), backups.end(),
                    [member_id]( const watch_exit_member_backup & backup ) {
                        return backup.member && backup.member->getID() == member_id;
                    } );
                };
                const auto restore_member = [&find_backup, &backups](
                const character_id member_id ) {
                    const auto backup = find_backup( member_id );
                    if( backup == backups.end() ) {
                        return;
                    }
                    backup->member->goal = backup->goal;
                    backup->member->omt_path = backup->omt_path;
                    backup->member->mission = backup->mission;
                    backup->member->previous_mission = backup->previous_mission;
                    backup->member->goto_to_this_pos = backup->ordered_position;
                    if( backup->ai_guard_position ) {
                        backup->member->set_ai_guard_pos( *backup->ai_guard_position );
                    } else {
                        backup->member->clear_ai_guard_pos();
                    }
                    backup->member->path = backup->local_path;
                };
                const auto prepare_member = [&site, &exit_plan, &home_routes,
                                             &find_backup, &backups](
                const character_id member_id ) {
                    const auto backup = find_backup( member_id );
                    if( backup == backups.end() ) {
                        return exit_plan.kind == bandit_live_world::
                               local_structural_watch_exit_kind::hard_danger_unreachable;
                    }
                    if( backup->member->is_dead() ||
                        backup->member->pos_abs() != backup->position ) {
                        return false;
                    }
                    backup->member->goto_to_this_pos = std::nullopt;
                    backup->member->clear_ai_guard_pos();
                    backup->member->path.clear();
                    backup->member->omt_path.clear();
                    if( exit_plan.kind == bandit_live_world::
                        local_structural_watch_exit_kind::hard_danger_return ) {
                        const auto route = home_routes.find( member_id );
                        if( route == home_routes.end() ) {
                            return false;
                        }
                        backup->member->goal = site.anchor;
                        backup->member->omt_path = route->second;
                        backup->member->mission = NPC_MISSION_TRAVELLING;
                    } else {
                        backup->member->goal = npc::no_goal_point;
                        backup->member->set_guard_pos( backup->member->pos_abs() );
                        backup->member->set_mission( NPC_MISSION_GUARD );
                    }
                    return true;
                };
                const bandit_live_world::local_handoff_commit_result committed =
                    bandit_live_world::commit_local_structural_watch_exit(
                        site, exit_plan, prepare_member, restore_member );
                if( committed == bandit_live_world::local_handoff_commit_result::applied ) {
                    burned++;
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit_live_world local_watch_exit"
                            << " site=" << site.site_id
                            << " priority=hard_danger"
                            << " result=" << ( exit_plan.kind == bandit_live_world::
                                               local_structural_watch_exit_kind::hard_danger_return ?
                                               "returning_home" : "closed_unreachable" ) << '\n';
                }
                continue;
            }
        }
        const bandit_live_world::covert_scout_burn_effect effect =
            bandit_live_world::apply_covert_scout_burn(
                site, *cursor, reads, egress_candidates, current_minutes, danger_read );
        if( effect.result != bandit_live_world::covert_scout_burn_result::applied ) {
            continue;
        }
        burned++;
        for( const character_id member_id : site.active_outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member == nullptr || member->is_dead() ) {
                continue;
            }
            const auto egress_route = egress_routes.find( member_id );
            if( egress_route == egress_routes.end() ) {
                if( site.active_outing.phase ==
                    bandit_live_world::scout_phase::returning_exposed ) {
                    member->omt_path.clear();
                    live_bandit_route_member_home( *member, site );
                }
                continue;
            }
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
            member->goal = npc::no_goal_point;
            member->omt_path.clear();
            member->goal = effect.egress_omt;
            member->omt_path = std::move( egress_route->second );
            member->set_mission( NPC_MISSION_TRAVELLING );
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world covert_burn"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " observer=" << effect.observer_id.get_value()
                                   << " target_observer=" << effect.target_observer_id
                                   << " origin=" << effect.burn_origin_omt.to_string()
                                   << " egress=" << effect.egress_omt.to_string() << '\n';
    }
    return burned;
}

int record_live_bandit_covert_visible_defenders()
{
    struct target_character {
        const Character *actor = nullptr;
        std::string stable_id;
    };

    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    std::set<character_id> claimed_members;
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            return 0;
        }
    }

    avatar &u = get_avatar();
    map &here = get_map();
    std::vector<target_character> target_characters = { { &u, "avatar" } };
    for( const npc &defender : g->all_npcs() ) {
        if( defender.is_player_ally() && !defender.is_dead() && defender.is_active() &&
            here.inbounds( defender.pos_bub( here ) ) ) {
            target_characters.push_back( { &defender, "npc:" +
                                           std::to_string( defender.getID().get_value() ) } );
        }
    }

    const int current_minutes = live_bandit_current_minutes();
    int recorded = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool targets_player_camp = std::any_of(
            outing.target_footprint.begin(), outing.target_footprint.end(),
        []( const tripoint_abs_omt & target_omt ) {
            return overmap_buffer.is_player_camp_omt( target_omt );
        } );
        if( site.retired_empty_site || !targets_player_camp || outing.schema_version != 10 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            !outing.local_handoff.is_active() ||
            !outing.local_handoff.cohesion_assembled || outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<target_character> bounded_targets = target_characters;
        std::sort( bounded_targets.begin(), bounded_targets.end(),
        [&u, &outing]( const target_character & lhs, const target_character & rhs ) {
            return std::make_tuple( lhs.actor == &u ? 0 : 1,
                                    rl_dist( lhs.actor->pos_abs_omt(),
                                             outing.selected_watch_omt ), lhs.stable_id ) <
                   std::make_tuple( rhs.actor == &u ? 0 : 1,
                                    rl_dist( rhs.actor->pos_abs_omt(),
                                             outing.selected_watch_omt ), rhs.stable_id );
        } );
        bounded_targets.resize( std::min<std::size_t>(
                                    bounded_targets.size(),
                                    bandit_live_world::covert_visible_defender_read_cap() ) );

        std::vector<npc *> observers;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member != nullptr && !member->is_dead() && member->is_active() &&
                here.inbounds( member->pos_bub( here ) ) &&
                member->pos_abs_omt() == outing.selected_watch_omt &&
                member->has_ecology_covert_noncombat_relationship( u ) &&
                live_bandit_can_make_ordinary_visual_observation( *member ) ) {
                observers.push_back( member );
            }
        }
        std::sort( observers.begin(), observers.end(), [&outing]( const npc *lhs, const npc *rhs ) {
            return std::make_tuple( lhs->getID() == outing.leader_id ? 0 : 1,
                                    lhs->getID().get_value() ) <
                   std::make_tuple( rhs->getID() == outing.leader_id ? 0 : 1,
                                    rhs->getID().get_value() );
        } );

        for( npc *observer : observers ) {
            const bool scout_has_gun = observer->get_wielded_item() &&
                                       observer->get_wielded_item()->is_gun();
            std::vector<bandit_live_world::covert_scout_burn_read::visible_defender_read>
            visible_defenders;
            for( const target_character &defender : bounded_targets ) {
                if( defender.actor == observer || defender.actor->is_dead_state() ||
                    !observer->sees_without_clairvoyance( here, *defender.actor ) ) {
                    continue;
                }
                bandit_live_world::covert_scout_burn_read::visible_defender_read read;
                read.stable_id = defender.stable_id;
                read.position = defender.actor->pos_abs_omt();
                read.normalized_power = bandit_live_world::normalize_hostile_camp_character_power(
                                            observer->evaluate_character_threat_without_perception_fuzz(
                                                *defender.actor, scout_has_gun, true ) );
                const item_location defender_weapon = defender.actor->get_wielded_item();
                read.equipment_detail = !defender_weapon ? 0 :
                                        defender_weapon->is_gun() ? 2 : 1;
                visible_defenders.push_back( std::move( read ) );
            }
            std::sort( visible_defenders.begin(), visible_defenders.end(),
            []( const auto & lhs, const auto & rhs ) {
                return lhs.stable_id < rhs.stable_id;
            } );
            if( visible_defenders.empty() ) {
                continue;
            }

            const bandit_live_world::sortie_observation_effect effect =
                bandit_live_world::record_covert_visible_defender_observations(
                    site, *cursor, observer->getID(), observer->pos_abs_omt(),
                    visible_defenders, current_minutes );
            if( effect.valid ) {
                if( effect.changed ) {
                    recorded++;
                }
                break;
            }
        }
    }
    return recorded;
}

int record_live_bandit_covert_vehicle_wealth_cues()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    std::set<character_id> claimed_members;
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            return 0;
        }
    }

    avatar &u = get_avatar();
    map &here = get_map();
    VehicleList loaded_vehicles = here.get_vehicles();
    std::sort( loaded_vehicles.begin(), loaded_vehicles.end(), []( const auto &lhs,
    const auto &rhs ) {
        if( lhs.v == nullptr || rhs.v == nullptr ) {
            return lhs.v != nullptr;
        }
        return lhs.v->pos_abs() < rhs.v->pos_abs();
    } );

    const int current_minutes = live_bandit_current_minutes();
    int recorded = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool targets_player_camp = std::any_of(
            outing.target_footprint.begin(), outing.target_footprint.end(),
        []( const tripoint_abs_omt & target_omt ) {
            return overmap_buffer.is_player_camp_omt( target_omt );
        } );
        if( site.retired_empty_site || !targets_player_camp || outing.schema_version != 10 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            !outing.local_handoff.is_active() || !outing.local_handoff.cohesion_assembled ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<npc *> observers;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member != nullptr && !member->is_dead() && member->is_active() &&
                here.inbounds( member->pos_bub( here ) ) &&
                member->pos_abs_omt() == outing.selected_watch_omt &&
                member->has_ecology_covert_noncombat_relationship( u ) &&
                member->clairvoyance() == 0 &&
                live_bandit_can_make_ordinary_visual_observation( *member ) ) {
                observers.push_back( member );
            }
        }
        std::sort( observers.begin(), observers.end(), [&outing]( const npc *lhs,
        const npc *rhs ) {
            return std::make_tuple( lhs->getID() == outing.leader_id ? 0 : 1,
                                    lhs->getID().get_value() ) <
                   std::make_tuple( rhs->getID() == outing.leader_id ? 0 : 1,
                                    rhs->getID().get_value() );
        } );

        for( npc *observer : observers ) {
            std::vector<bandit_live_world::covert_vehicle_wealth_read> reads;
            for( const wrapped_vehicle &wrapped : loaded_vehicles ) {
                const vehicle *veh = wrapped.v;
                if( veh == nullptr || veh->is_appliance() ||
                    veh->get_owner() != faction_your_followers ||
                    std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                               veh->pos_abs_omt() ) == outing.target_footprint.end() ) {
                    continue;
                }
                bandit_live_world::covert_vehicle_wealth_read read;
                read.origin = veh->pos_abs();
                for( const tripoint_abs_ms &point : veh->get_points() ) {
                    const tripoint_abs_omt point_omt = project_to<coords::omt>( point );
                    if( std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                                  point_omt ) != outing.target_footprint.end() &&
                        here.inbounds( point ) &&
                        observer->sees( here, here.get_bub( point ) ) ) {
                        read.ordinarily_visible_occupied_points.push_back( point );
                    }
                }
                if( !read.ordinarily_visible_occupied_points.empty() ) {
                    reads.push_back( std::move( read ) );
                    if( reads.size() == static_cast<std::size_t>(
                                bandit_live_world::covert_vehicle_wealth_cue_cap() ) ) {
                        break;
                    }
                }
            }
            if( reads.empty() ) {
                continue;
            }
            const bandit_live_world::sortie_observation_effect effect =
                bandit_live_world::record_covert_vehicle_wealth_observations(
                    site, *cursor, observer->getID(), observer->pos_abs_omt(), reads,
                    current_minutes );
            if( effect.valid ) {
                if( effect.changed ) {
                    recorded++;
                }
                break;
            }
        }
    }
    return recorded;
}

int record_live_bandit_covert_generation_infrastructure_cues()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    std::set<character_id> claimed_members;
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            return 0;
        }
    }

    avatar &u = get_avatar();
    map &here = get_map();
    VehicleList loaded_vehicles = here.get_vehicles();
    std::sort( loaded_vehicles.begin(), loaded_vehicles.end(), []( const auto &lhs,
    const auto &rhs ) {
        if( lhs.v == nullptr || rhs.v == nullptr ) {
            return lhs.v != nullptr;
        }
        return lhs.v->pos_abs() < rhs.v->pos_abs();
    } );

    const int current_minutes = live_bandit_current_minutes();
    int recorded = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool targets_player_camp = std::any_of(
            outing.target_footprint.begin(), outing.target_footprint.end(),
        []( const tripoint_abs_omt & target_omt ) {
            return overmap_buffer.is_player_camp_omt( target_omt );
        } );
        if( site.retired_empty_site || !targets_player_camp || outing.schema_version != 10 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            !outing.local_handoff.is_active() || !outing.local_handoff.cohesion_assembled ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<npc *> observers;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member != nullptr && !member->is_dead() && member->is_active() &&
                here.inbounds( member->pos_bub( here ) ) &&
                member->pos_abs_omt() == outing.selected_watch_omt &&
                member->has_ecology_covert_noncombat_relationship( u ) &&
                member->clairvoyance() == 0 &&
                live_bandit_can_make_ordinary_visual_observation( *member ) ) {
                observers.push_back( member );
            }
        }
        std::sort( observers.begin(), observers.end(), [&outing]( const npc *lhs,
        const npc *rhs ) {
            return std::make_tuple( lhs->getID() == outing.leader_id ? 0 : 1,
                                    lhs->getID().get_value() ) <
                   std::make_tuple( rhs->getID() == outing.leader_id ? 0 : 1,
                                    rhs->getID().get_value() );
        } );

        for( npc *observer : observers ) {
            std::vector<bandit_live_world::covert_generation_infrastructure_read> reads;
            std::set<tripoint_abs_ms> generation_part_positions;
            for( const wrapped_vehicle &wrapped : loaded_vehicles ) {
                vehicle *veh = wrapped.v;
                if( veh == nullptr || !veh->is_appliance() ||
                    veh->get_owner() != faction_your_followers ||
                    std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                               veh->pos_abs_omt() ) == outing.target_footprint.end() ) {
                    continue;
                }
                const auto append_visible_generation_parts = [&]( const vpart_bitflags flag ) {
                    for( const vpart_reference &part : veh->get_avail_parts( flag ) ) {
                        const tripoint_abs_ms part_position = part.pos_abs();
                        const tripoint_abs_omt part_omt = project_to<coords::omt>( part_position );
                        if( std::find( outing.target_footprint.begin(),
                                      outing.target_footprint.end(), part_omt ) ==
                            outing.target_footprint.end() || !here.inbounds( part_position ) ||
                            !observer->sees( here, here.get_bub( part_position ) ) ||
                            !generation_part_positions.insert( part_position ).second ) {
                            continue;
                        }
                        reads.push_back( { veh->pos_abs(), part_position } );
                        if( reads.size() == static_cast<std::size_t>(
                                    bandit_live_world::covert_generation_infrastructure_cue_cap() ) ) {
                            return;
                        }
                    }
                };
                append_visible_generation_parts( VPFLAG_SOLAR_PANEL );
                if( reads.size() < static_cast<std::size_t>(
                        bandit_live_world::covert_generation_infrastructure_cue_cap() ) ) {
                    append_visible_generation_parts( VPFLAG_WIND_TURBINE );
                }
                if( reads.size() < static_cast<std::size_t>(
                        bandit_live_world::covert_generation_infrastructure_cue_cap() ) ) {
                    append_visible_generation_parts( VPFLAG_WATER_WHEEL );
                }
                if( reads.size() == static_cast<std::size_t>(
                            bandit_live_world::covert_generation_infrastructure_cue_cap() ) ) {
                    break;
                }
            }
            if( reads.empty() ) {
                continue;
            }
            const bandit_live_world::sortie_observation_effect effect =
                bandit_live_world::record_covert_generation_infrastructure_observations(
                    site, *cursor, observer->getID(), observer->pos_abs_omt(), reads,
                    current_minutes );
            if( effect.valid ) {
                if( effect.changed ) {
                    recorded++;
                }
                break;
            }
        }
    }
    return recorded;
}

int record_live_bandit_covert_cargo_handling_cues()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    std::set<character_id> claimed_members;
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            return 0;
        }
    }

    avatar &u = get_avatar();
    map &here = get_map();
    std::vector<const npc *> cargo_handlers;
    for( const npc &candidate : g->all_npcs() ) {
        if( !candidate.is_dead() && candidate.is_active() &&
            here.inbounds( candidate.pos_bub( here ) ) &&
            candidate.get_fac_id() == faction_your_followers &&
            candidate.activity.id() == ACT_MOVE_LOOT ) {
            cargo_handlers.push_back( &candidate );
        }
    }
    std::sort( cargo_handlers.begin(), cargo_handlers.end(), []( const npc *lhs,
    const npc *rhs ) {
        return lhs->getID() < rhs->getID();
    } );

    const int current_minutes = live_bandit_current_minutes();
    int recorded = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool targets_player_camp = std::any_of(
            outing.target_footprint.begin(), outing.target_footprint.end(),
        []( const tripoint_abs_omt & target_omt ) {
            return overmap_buffer.is_player_camp_omt( target_omt );
        } );
        if( site.retired_empty_site || !targets_player_camp || outing.schema_version != 10 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            !outing.local_handoff.is_active() || !outing.local_handoff.cohesion_assembled ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<npc *> observers;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member != nullptr && !member->is_dead() && member->is_active() &&
                here.inbounds( member->pos_bub( here ) ) &&
                member->pos_abs_omt() == outing.selected_watch_omt &&
                member->has_ecology_covert_noncombat_relationship( u ) &&
                member->clairvoyance() == 0 &&
                live_bandit_can_make_ordinary_visual_observation( *member ) ) {
                observers.push_back( member );
            }
        }
        std::sort( observers.begin(), observers.end(), [&outing]( const npc *lhs,
        const npc *rhs ) {
            return std::make_tuple( lhs->getID() == outing.leader_id ? 0 : 1,
                                    lhs->getID().get_value() ) <
                   std::make_tuple( rhs->getID() == outing.leader_id ? 0 : 1,
                                    rhs->getID().get_value() );
        } );

        for( npc *observer : observers ) {
            std::vector<bandit_live_world::covert_cargo_handling_read> reads;
            for( const npc *handler : cargo_handlers ) {
                const tripoint_abs_omt handler_omt = handler->pos_abs_omt();
                if( std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                               handler_omt ) == outing.target_footprint.end() ||
                    !observer->sees( here, handler->pos_bub( here ) ) ||
                    !observer->sees_without_clairvoyance( here, *handler ) ) {
                    continue;
                }
                reads.push_back( { handler->getID(), handler_omt } );
                if( reads.size() == static_cast<std::size_t>(
                            bandit_live_world::covert_cargo_handling_cue_cap() ) ) {
                    break;
                }
            }
            if( reads.empty() ) {
                continue;
            }
            const bandit_live_world::sortie_observation_effect effect =
                bandit_live_world::record_covert_cargo_handling_observations(
                    site, *cursor, observer->getID(), observer->pos_abs_omt(), reads,
                    current_minutes );
            if( effect.valid ) {
                if( effect.changed ) {
                    recorded++;
                }
                break;
            }
        }
    }
    return recorded;
}

bool note_live_bandit_aftermath()
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    const int scout_sortie_limit_minutes = bandit_live_world::ordinary_scout_sortie_limit_minutes();
    bool changed = false;

    for( bandit_live_world::site_record &site : state.sites ) {
        changed |= live_bandit_apply_shakedown_defender_aftermath( site, u );
        bandit_live_world::active_outing_state *external_outing = site.active_external_outing();
        if( external_outing != nullptr && external_outing != &site.active_outing ) {
            changed |= live_bandit_handle_hostile_shakedown_contact( site, u );
            continue;
        }
        if( site.retired_empty_site || !site.active_outing.is_active() || site.active_outing.member_ids.empty() ) {
            continue;
        }

        if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site ) ) {
            changed |= bandit_live_world::note_active_sortie_started(
                           site, *cursor, current_minutes );
        }

        if( site.active_outing.kind == bandit_live_world::outing_kind::structural_sortie &&
            site.active_outing.owner == bandit_live_world::simulation_owner::local &&
            bandit_live_world::scout_phase_requires_homeward_only(
                site.active_outing.phase ) ) {
            std::vector<bandit_live_world::local_pair_casualty_read> casualty_reads;
            for( const character_id member_id : site.active_outing.member_ids ) {
                if( site.active_outing.member_is_resolved( member_id ) ||
                    std::find( site.active_outing.casualty_ids.begin(),
                               site.active_outing.casualty_ids.end(), member_id ) !=
                    site.active_outing.casualty_ids.end() ) {
                    continue;
                }
                const bandit_live_world::member_record *member = site.find_member( member_id );
                npc *member_npc = g->find_npc( member_id );
                const bool missing_after_deadline = member_npc == nullptr &&
                        site.active_outing.missing_deadline_minutes >= 0 &&
                        current_minutes >= site.active_outing.missing_deadline_minutes;
                const bool dead = ( member != nullptr &&
                                    member->state == bandit_live_world::member_state::dead ) ||
                                  ( member_npc != nullptr && member_npc->is_dead() );
                const bool missing = ( member != nullptr &&
                                       member->state == bandit_live_world::member_state::missing ) ||
                                     missing_after_deadline;
                if( !dead && !missing ) {
                    continue;
                }
                const auto snapshot = std::find_if(
                                          site.active_outing.local_handoff.members.begin(),
                                          site.active_outing.local_handoff.members.end(),
                [&member_id]( const bandit_live_world::local_handoff_member_snapshot & read ) {
                    return read.npc_id == member_id;
                } );
                if( snapshot == site.active_outing.local_handoff.members.end() ) {
                    continue;
                }
                casualty_reads.push_back( {
                    member_id,
                    dead ? bandit_live_world::member_state::dead :
                    bandit_live_world::member_state::missing,
                    member_npc != nullptr ? member_npc->pos_abs() : snapshot->exit_position
                } );
            }
            if( !casualty_reads.empty() ) {
                const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                    bandit_live_world::current_external_simulation_cursor( site );
                if( cursor && bandit_live_world::reconcile_local_pair_casualties(
                        site, *cursor, casualty_reads, current_minutes ) ) {
                    changed = true;
                }
            }
        }

        std::vector<bandit_live_world::active_member_observation> observations;
        const std::vector<character_id> active_member_ids = site.active_outing.member_ids;
        observations.reserve( active_member_ids.size() );
        bool has_unresolved_burn_survivor = false;
        bool every_unresolved_burn_survivor_at_egress = true;
        if( site.active_outing.phase == bandit_live_world::scout_phase::burned_withdrawal ) {
            for( const character_id member_id : active_member_ids ) {
                if( site.active_outing.member_is_resolved( member_id ) ||
                    std::find( site.active_outing.casualty_ids.begin(),
                               site.active_outing.casualty_ids.end(), member_id ) !=
                    site.active_outing.casualty_ids.end() ) {
                    continue;
                }
                has_unresolved_burn_survivor = true;
                const npc *member_npc = g->find_npc( member_id );
                every_unresolved_burn_survivor_at_egress &= member_npc != nullptr &&
                        !member_npc->is_dead() && member_npc->pos_abs_omt() ==
                        site.active_outing.local_handoff.egress_omt;
            }
        }
        const bool burned_egress_pending =
            site.active_outing.phase == bandit_live_world::scout_phase::burned_withdrawal &&
            ( !has_unresolved_burn_survivor || !every_unresolved_burn_survivor_at_egress );
        for( const character_id &member_id : active_member_ids ) {
            bandit_live_world::active_member_observation observation;
            observation.npc_id = member_id;
            npc *member_npc = g->find_npc( member_id );
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr ) {
                observation.summary = "member record missing";
                observations.push_back( observation );
                continue;
            }
            if( site.active_outing.member_is_resolved( member_id ) ) {
                if( member->state == bandit_live_world::member_state::at_home ) {
                    observation.state = bandit_live_world::active_member_observation_state::home;
                    observation.summary = "persisted outing member already returned home";
                } else if( member->state == bandit_live_world::member_state::dead ) {
                    observation.state = bandit_live_world::active_member_observation_state::dead;
                    observation.summary = "persisted outing casualty dead";
                } else if( member->state == bandit_live_world::member_state::missing ) {
                    observation.state = bandit_live_world::active_member_observation_state::missing;
                    observation.summary = "persisted outing casualty missing";
                } else {
                    observation.summary = "persisted outing resolution disagrees with member state";
                }
                observations.push_back( observation );
                continue;
            }
            if( member->state == bandit_live_world::member_state::dead ||
                member->state == bandit_live_world::member_state::missing ) {
                observation.state = member->state == bandit_live_world::member_state::dead ?
                                    bandit_live_world::active_member_observation_state::dead :
                                    bandit_live_world::active_member_observation_state::missing;
                observation.summary = member->state == bandit_live_world::member_state::dead ?
                                      "outing casualty dead before resolution receipt" :
                                      "outing casualty missing before resolution receipt";
                observations.push_back( observation );
                continue;
            }
            if( member_npc == nullptr ) {
                if( site.active_outing.missing_deadline_minutes >= 0 &&
                    current_minutes >= site.active_outing.missing_deadline_minutes ) {
                    observation.state = bandit_live_world::active_member_observation_state::missing;
                    observation.summary = "member unresolved beyond persisted missing grace";
                } else {
                    observation.summary = "member not currently loaded; awaiting bounded missing grace";
                }
                if( site.last_shakedown_outcome == "fight_unresolved" ) {
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit shakedown aftermath: active member "
                            << member_id.get_value() << " not currently loaded for "
                            << site.active_outing.target_id;
                }
                observations.push_back( observation );
                continue;
            }

            const bool routing_burn_egress = !member_npc->is_dead() &&
                    live_bandit_member_routing_burn_egress(
                        *member_npc, site.active_outing );
            if( member_npc->is_dead() ) {
                observation.state = bandit_live_world::active_member_observation_state::dead;
                observation.summary = "npc dead";
                if( site.last_shakedown_outcome == "fight_unresolved" ) {
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit shakedown aftermath: active member "
                            << member_id.get_value() << " dead during active outing near "
                            << site.active_outing.target_id;
                }
            } else if( site_contains_omt( site, member_npc->pos_abs_omt() ) ) {
                if( member->state == bandit_live_world::member_state::local_contact ||
                    bandit_live_world::scout_sortie_should_return_home( site, current_minutes,
                            scout_sortie_limit_minutes ) ) {
                    observation.state = bandit_live_world::active_member_observation_state::home;
                    observation.summary = "npc back on home footprint after scout sortie";
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit_live_world scout_sortie: home footprint observed"
                            << " site=" << site.site_id
                            << " active_group=" << site.active_outing.activity_id
                            << " member=" << member_id.get_value()
                            << " pos=" << member_npc->pos_abs_omt().to_string()
                            << " elapsed_minutes=" << ( current_minutes -
                                    ( site.active_outing.local_contact_minutes >= 0 ?
                                      site.active_outing.local_contact_minutes :
                                      site.active_outing.started_minutes ) ) << '\n';
                } else {
                    observation.summary = "outbound member still on home footprint";
                }
            } else if( !site.active_outing.alternate_watch_reposition_pending &&
                       !burned_egress_pending && !routing_burn_egress &&
                       bandit_live_world::scout_sortie_should_return_home( site, current_minutes,
                       scout_sortie_limit_minutes ) && live_bandit_route_member_home( *member_npc, site ) ) {
                observation.state = bandit_live_world::active_member_observation_state::returning_home;
                observation.summary = "scout sortie limit reached; returning home";
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world scout_sortie: linger limit reached -> return_home"
                        << " site=" << site.site_id
                        << " active_group=" << site.active_outing.activity_id
                        << " member=" << member_id.get_value()
                        << " elapsed_minutes=" << ( current_minutes -
                                ( site.active_outing.local_contact_minutes >= 0 ?
                                  site.active_outing.local_contact_minutes :
                                  site.active_outing.started_minutes ) )
                        << " limit_minutes=" << scout_sortie_limit_minutes << '\n';
            } else if( rl_dist( member_npc->pos_abs_omt(), u.pos_abs_omt() ) <= 1 &&
                       ( member_npc->is_active() || !member_npc->is_travelling() ) ) {
                observation.state = bandit_live_world::active_member_observation_state::local_contact;
                observation.summary = "local contact near player target";
                if( site.last_shakedown_outcome == "fight_unresolved" ) {
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit shakedown aftermath: active member "
                            << member_id.get_value() << " in local contact at "
                            << member_npc->pos_abs_omt().to_string() << " player="
                            << u.pos_abs_omt().to_string();
                }
                if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                    bandit_live_world::current_external_simulation_cursor( site ) ) {
                    changed |= bandit_live_world::note_active_sortie_local_contact(
                                   site, *cursor, member_id, current_minutes );
                }
                if( live_bandit_member_routing_home( *member_npc, site ) ) {
                    observation.state = bandit_live_world::active_member_observation_state::returning_home;
                    observation.summary = "scout returning home after sortie limit";
                }
            } else if( member->state == bandit_live_world::member_state::local_contact ) {
                if( !member_npc->is_active() &&
                    !site.active_outing.alternate_watch_reposition_pending &&
                    !burned_egress_pending &&
                    !routing_burn_egress &&
                    ( !member_npc->is_travelling() || !member_npc->has_omt_destination() ||
                      !site_contains_omt( site, member_npc->goal ) ) ) {
                    live_bandit_route_member_home( *member_npc, site );
                }
                if( routing_burn_egress ) {
                    observation.summary = "burned scout withdrawing toward persisted egress";
                } else if( member_npc->is_travelling() && member_npc->has_omt_destination() &&
                    site_contains_omt( site, member_npc->goal ) ) {
                    observation.state = bandit_live_world::active_member_observation_state::returning_home;
                    observation.summary = "travelling back toward home footprint";
                } else {
                    observation.summary = "local contact unresolved";
                    if( site.last_shakedown_outcome == "fight_unresolved" ) {
                        DebugLog( D_INFO, DC_ALL )
                                << "bandit shakedown aftermath: active member "
                                << member_id.get_value() << " unresolved at "
                                << member_npc->pos_abs_omt().to_string() << " player="
                                << u.pos_abs_omt().to_string();
                    }
                }
            } else {
                observation.summary = "still outbound";
            }

            observations.push_back( observation );
        }

        const bool scout_phase_outing = site.active_outing.kind ==
                                        bandit_live_world::outing_kind::scout_sortie;
        const bool physical_report_scout = scout_phase_outing &&
                                           site.active_outing.job_type == "scout";
        if( physical_report_scout ) {
            const std::string site_id = site.site_id;
            const std::string group_id = site.active_outing.activity_id;
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            const bandit_live_world::scout_resolution_effect resolution = cursor ?
                    bandit_live_world::apply_active_scout_observations(
                        site, *cursor, observations, current_minutes ) :
                    bandit_live_world::scout_resolution_effect();
            changed |= resolution.changed;
            if( resolution.completed ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world scout_report: all members resolved -> finalized"
                        << " site=" << site_id
                        << " active_group=" << group_id << '\n';
                continue;
            }
            if( resolution.provisional_report_applied ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world scout_report: first survivor -> provisional"
                        << " site=" << site_id
                        << " active_group=" << group_id
                        << " newly_returned=" << resolution.newly_returned
                        << " cargo_credited=" << ( resolution.cargo_credited ? "yes" : "no" ) << '\n';
            }
        }

        if( scout_phase_outing && !physical_report_scout ) {
            const std::optional<bandit_pursuit_handoff::return_packet> packet =
                bandit_live_world::resolve_active_group_aftermath( site, observations );
            if( packet && bandit_live_world::apply_return_packet( site, *packet ) ) {
                continue;
            }
        }

        bool has_unresolved_member = false;
        bool any_unresolved_member_returning_home = false;
        bool every_unresolved_member_returning_home = true;
        bool every_unresolved_member_returning_or_home = true;
        std::vector<bandit_live_world::covert_scout_member_acquire_read> acquire_reads;
        map &here = get_map();
        for( const bandit_live_world::active_member_observation &observation : observations ) {
            if( site.active_outing.member_is_resolved( observation.npc_id ) ) {
                continue;
            }
            has_unresolved_member = true;
            const bool returning_home = observation.state ==
                                        bandit_live_world::active_member_observation_state::returning_home;
            const bool already_home = observation.state ==
                                      bandit_live_world::active_member_observation_state::home;
            any_unresolved_member_returning_home |= returning_home;
            every_unresolved_member_returning_home &= returning_home;
            every_unresolved_member_returning_or_home &= returning_home || already_home;

            bandit_live_world::covert_scout_member_acquire_read read;
            read.npc_id = observation.npc_id;
            read.returning_home = returning_home || already_home;
            npc *member_npc = g->find_npc( observation.npc_id );
            read.position_known = member_npc != nullptr && !member_npc->is_dead();
            if( read.position_known ) {
                read.position = member_npc->pos_abs_omt();
                const bool in_bounds = here.inbounds( member_npc->pos_bub( here ) );
                const bool locally_loaded = member_npc->is_active() && in_bounds;
                if( locally_loaded ) {
                    read.mutual_target_visibility_evaluated = true;
                    read.mutual_target_visibility =
                        u.sees_without_clairvoyance( here, *member_npc ) ||
                        member_npc->sees_without_clairvoyance( here, u );
                    for( const npc &defender : g->all_npcs() ) {
                        if( read.mutual_target_visibility || &defender == member_npc ||
                            !defender.is_player_ally() ) {
                            continue;
                        }
                        read.mutual_target_visibility =
                            defender.sees_without_clairvoyance( here, *member_npc ) ||
                            member_npc->sees_without_clairvoyance( here, defender );
                    }
                } else if( !in_bounds ) {
                    // Ordinary Creature visibility exists only in the active map.  A known NPC
                    // outside that map is authoritatively outside current loaded acquire.  An
                    // inactive but in-bounds NPC remains unknown until materialized.
                    read.mutual_target_visibility_evaluated = true;
                }
            }
            acquire_reads.push_back( read );
        }
        const bool targets_player_camp = std::any_of(
            site.active_outing.target_footprint.begin(),
            site.active_outing.target_footprint.end(), []( const tripoint_abs_omt & target_omt ) {
            return overmap_buffer.is_player_camp_omt( target_omt );
        } );
        const bool covert_player_camp = targets_player_camp &&
                                        site.active_outing.schema_version == 10;
        const bool outside_target_acquire = !covert_player_camp ||
                                            bandit_live_world::
                                            covert_scout_party_cleared_target_acquire_range(
                                                site.active_outing, acquire_reads );
        if( covert_player_camp && !burned_egress_pending &&
            site.active_outing.phase ==
            bandit_live_world::scout_phase::burned_withdrawal ) {
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( cursor && bandit_live_world::complete_covert_scout_burned_egress(
                    site, *cursor, acquire_reads, current_minutes ) ) {
                changed = true;
                continue;
            }
            const auto visible_survivor = std::find_if(
                                              acquire_reads.begin(), acquire_reads.end(),
            []( const bandit_live_world::covert_scout_member_acquire_read & read ) {
                return read.mutual_target_visibility;
            } );
            if( cursor && visible_survivor != acquire_reads.end() &&
                bandit_live_world::fail_live_covert_scout_burned_egress(
                    visible_survivor->npc_id ) ) {
                changed = true;
                continue;
            }
            if( std::any_of( acquire_reads.begin(), acquire_reads.end(),
            []( const bandit_live_world::covert_scout_member_acquire_read & read ) {
                return !read.mutual_target_visibility_evaluated;
            } ) ) {
                continue;
            }
        }
        const bool transition_party_returning_home = has_unresolved_member &&
                ( covert_player_camp ? every_unresolved_member_returning_or_home &&
                  outside_target_acquire : any_unresolved_member_returning_home );
        const bool active_group_returning_home = has_unresolved_member &&
                ( covert_player_camp ? every_unresolved_member_returning_or_home :
                  every_unresolved_member_returning_home ) && outside_target_acquire;
        if( transition_party_returning_home &&
            site.active_outing.phase != bandit_live_world::scout_phase::returning_home ) {
            if( scout_phase_outing ) {
                if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                    bandit_live_world::current_external_simulation_cursor( site ) ) {
                    const bandit_live_world::scout_phase_transition_result transition =
                        bandit_live_world::transition_active_scout_phase(
                            site, *cursor, site.active_outing.phase,
                            bandit_live_world::scout_phase::returning_home, current_minutes,
                            "live party returning home" );
                    changed |= transition ==
                               bandit_live_world::scout_phase_transition_result::applied;
                }
            } else {
                const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                    bandit_live_world::current_external_simulation_cursor( site );
                if( cursor ) {
                    bandit_live_world::site_record candidate = site;
                    const bandit_live_world::simulation_owner_transition_result advance =
                        bandit_live_world::advance_external_simulation(
                            candidate, cursor->activity_id, cursor->generation, cursor->owner,
                            cursor->handoff_epoch, cursor->last_advanced_minutes,
                            cursor->covert_egress_revision, current_minutes );
                    if( advance ==
                        bandit_live_world::simulation_owner_transition_result::applied ) {
                        candidate.active_outing.phase =
                            bandit_live_world::scout_phase::returning_home;
                        if( candidate.active_outing.kind ==
                            bandit_live_world::outing_kind::structural_sortie &&
                            candidate.active_outing.local_handoff.is_active() ) {
                            candidate.active_outing.local_handoff.phase =
                                bandit_live_world::scout_phase::returning_home;
                        }
                        candidate.active_outing.last_progress_minutes = current_minutes;
                        site = std::move( candidate );
                        changed = true;
                    }
                }
            }
        }

        if( covert_player_camp && active_group_returning_home &&
            site.active_outing.phase == bandit_live_world::scout_phase::returning_home &&
            site.active_outing.local_handoff.cohesion_abort_return ) {
            if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site ) ) {
                changed |= bandit_live_world::release_covert_cohesion_abort_after_target_clear(
                               site, *cursor, acquire_reads );
            }
        }

        if( bandit_live_world::active_outing_requires_homeward_routing(
                site.active_outing ) ) {
            const int progress_anchor = std::max( site.active_outing.started_minutes,
                                                  site.active_outing.last_progress_minutes );
            const bool immobility_grace_expired = progress_anchor >= 0 &&
                    current_minutes >= progress_anchor &&
                    current_minutes - progress_anchor >=
                    hostile_scout_immobility_grace_minutes;
            const auto immobile_member = std::find_if(
                site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
            [&site, immobility_grace_expired]( const character_id member_id ) {
                const npc *member_npc = g->find_npc( member_id );
                return immobility_grace_expired &&
                       !site.active_outing.member_is_resolved( member_id ) &&
                       member_npc != nullptr && !member_npc->is_dead() &&
                       member_npc->has_flag( json_flag_CANNOT_MOVE ) &&
                       !site_contains_omt( site, member_npc->pos_abs_omt() );
            } );
            if( immobile_member != site.active_outing.member_ids.end() ) {
                if( site.active_outing.phase ==
                    bandit_live_world::scout_phase::burned_withdrawal ) {
                    changed |= bandit_live_world::fail_live_covert_scout_burned_egress(
                                   *immobile_member );
                }
                if( live_bandit_abandon_unreachable_return( *immobile_member ) ) {
                    changed = true;
                    continue;
                }
            }
        }

        if( active_group_returning_home ) {
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world scout_sortie: returning_home -> local_gate skipped"
                    << " site=" << site.site_id
                    << " active_group=" << site.active_outing.activity_id << '\n';
            if( !bandit_live_world::active_outing_requires_homeward_routing(
                    site.active_outing ) ) {
                continue;
            }
        }

        if( bandit_live_world::active_outing_requires_homeward_routing(
                site.active_outing ) ) {
            if( burned_egress_pending ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world scout_sortie: burned egress pending"
                        << " site=" << site.site_id
                        << " active_group=" << site.active_outing.activity_id
                        << " egress=" << site.active_outing.local_handoff.egress_omt.to_string()
                        << '\n';
                continue;
            }
            bool home_route_failed = false;
            for( const character_id &member_id : site.active_outing.member_ids ) {
                if( site.active_outing.member_is_resolved( member_id ) ) {
                    continue;
                }
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    const bool was_routing_home = live_bandit_member_routing_home(
                                                      *member_npc, site );
                    const bool routes_home = live_bandit_route_member_home( *member_npc, site );
                    changed |= !was_routing_home && routes_home;
                    home_route_failed |= !routes_home;
                }
            }
            if( home_route_failed ) {
                const std::vector<character_id> stranded_ids = site.active_outing.member_ids;
                const int current_minutes = live_bandit_current_minutes();
                const std::optional<std::vector<bandit_live_world::active_member_observation>>
                return_reads = live_bandit_read_unreachable_return_members(
                                   site, current_minutes );
                const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                    bandit_live_world::current_external_simulation_cursor( site );
                if( return_reads && cursor &&
                    bandit_live_world::abandon_covert_scout_unreachable_return(
                        site, *cursor, *return_reads, current_minutes ) ) {
                    changed = true;
                    for( const character_id member_id : stranded_ids ) {
                        if( npc *member_npc = g->find_npc( member_id ) ) {
                            member_npc->path.clear();
                            member_npc->omt_path.clear();
                            member_npc->goal = npc::no_goal_point;
                            member_npc->set_guard_pos( member_npc->pos_abs() );
                            member_npc->set_mission( NPC_MISSION_GUARD );
                        }
                    }
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit_live_world scout_sortie: unreachable return -> orphaned"
                            << " site=" << site.site_id << '\n';
                }
            }
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world scout_sortie: homeward-only phase -> target gate skipped"
                    << " site=" << site.site_id
                    << " active_group=" << site.active_outing.activity_id
                    << " phase=" << bandit_live_world::to_string( site.active_outing.phase ) << '\n';
            continue;
        }

        if( site.active_outing.alternate_watch_reposition_pending ) {
            // The persisted alternate destination owns both real NPC routes until the pair
            // arrives or the bounded abort seam releases it.  Local posture movement would
            // otherwise overwrite that transaction with a stalk/hold/combat destination.
            continue;
        }

        bandit_live_world::local_gate_input gate_input = live_bandit_make_gate_input( site, u );
        gate_input.local_contact_established |= std::any_of( observations.begin(), observations.end(),
        []( const bandit_live_world::active_member_observation & observation ) {
            return observation.state ==
                   bandit_live_world::active_member_observation_state::local_contact;
        } );
        const bandit_live_world::local_gate_decision gate_decision =
            bandit_live_world::choose_local_gate_posture( site, gate_input );
        DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_local_gate_report( site, gate_input,
                                   gate_decision )
                                   << "- live_existing_active_group=yes\n";
        if( gate_decision.combat_forward ) {
            for( const character_id &member_id : site.active_outing.member_ids ) {
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    const bandit_live_world::member_record *member = site.find_member( member_id );
                    if( member != nullptr && member->state == bandit_live_world::member_state::local_contact ) {
                        changed |= live_bandit_try_fight_advance( *member_npc, site, gate_input,
                                   gate_decision );
                    }
                }
            }
            continue;
        }
        if( gate_decision.posture == bandit_live_world::local_gate_posture::stalk ||
            gate_decision.posture == bandit_live_world::local_gate_posture::hold_off ) {
            for( const character_id &member_id : site.active_outing.member_ids ) {
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    const bandit_live_world::member_record *member = site.find_member( member_id );
                    if( member != nullptr && member->state == bandit_live_world::member_state::local_contact ) {
                        changed |= live_bandit_try_sight_avoid_reposition( *member_npc, site,
                                   gate_input, gate_decision );
                    }
                }
            }
        }
        if( gate_decision.opens_shakedown_surface ) {
            changed |= open_live_bandit_shakedown_surface( site, gate_input, gate_decision );
            if( !site.active_outing.is_active() || site.active_outing.member_ids.empty() ) {
                continue;
            }
        }

        if( !scout_phase_outing ) {
            const std::optional<bandit_pursuit_handoff::return_packet> packet =
                bandit_live_world::resolve_active_group_aftermath( site, observations );
            if( !packet ) {
                continue;
            }
            const std::string site_id = site.site_id;
            const std::string group_id = site.active_outing.activity_id;
            const bool applied_return = bandit_live_world::apply_return_packet( site, *packet );
            changed |= applied_return;
            if( applied_return && packet->job_type == bandit_dry_run::job_template::scout ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world scout_report: returned -> pressure refreshed"
                        << " site=" << site_id
                        << " active_group=" << group_id
                        << " remaining_pressure="
                        << bandit_pursuit_handoff::to_string( packet->remaining_pressure ) << '\n';
            }
        }
    }

    return changed;
}

struct live_bandit_signal_observation {
    bandit_live_world::live_signal_mark mark;
    bandit_mark_generation::signal_input signal;
    tripoint_abs_omt source_omt;
    std::optional<tripoint_abs_ms> source_ms;
    int range_cap_omt = 0;
    int horde_signal_power = 0;
    std::string weather_summary;
    bool has_light_projection = false;
    bandit_mark_generation::light_projection light_projection;
};

struct live_bandit_sound_observation {
    tripoint_abs_omt source_omt;
    int volume = 0;
    sounds::significant_sound_t kind = sounds::significant_sound_t::none;
    int emitted_minutes = -1;
};

static constexpr int live_bandit_system_envelope_omt = 40;
static constexpr int live_bandit_local_source_scan_radius_ms = 60;

struct live_bandit_local_source_reading {
    int fire_intensity = 0;
    int smoke_intensity = 0;
    int light_intensity = 0;
    bandit_mark_generation::light_source_band light_source =
        bandit_mark_generation::light_source_band::ordinary;
    int representative_light_intensity = 0;
    std::optional<tripoint_abs_ms> light_source_pos;
    bool outside = false;
    int side_leakage = 0;
    bool elevated_roof_exposed = false;
};

npc_template_id live_bandit_template_for_site( bandit_live_world::owned_site_kind site_kind )
{
    switch( site_kind ) {
        case bandit_live_world::owned_site_kind::cannibal_camp:
            return npc_template_id( "cannibal_hunter" );
        case bandit_live_world::owned_site_kind::bandit_camp:
        case bandit_live_world::owned_site_kind::bandit_work_camp:
        case bandit_live_world::owned_site_kind::bandit_cabin:
        case bandit_live_world::owned_site_kind::looters:
        case bandit_live_world::owned_site_kind::bandits_block:
            return npc_template_id( "bandit" );
        case bandit_live_world::owned_site_kind::none:
            break;
    }
    return npc_template_id::NULL_ID();
}

void refresh_live_bandit_member_readiness( bandit_live_world::world_state &state )
{
    const int now_minutes = live_bandit_current_minutes();
    std::unordered_map<int, npc *> loaded_npcs;
    overmap_buffer.foreach_npc( [&loaded_npcs]( npc &guy ) {
        loaded_npcs.emplace( guy.getID().get_value(), &guy );
    } );

    for( bandit_live_world::site_record &site : state.sites ) {
        for( bandit_live_world::member_record &member : site.members ) {
            if( member.state != bandit_live_world::member_state::at_home ) {
                continue;
            }
            if( bandit_live_world::member_has_abstract_wound_recovery(
                    member, now_minutes ) ) {
                member.wounded_or_unready = true;
                continue;
            }
            if( member.abstract_wound_until_minutes >= 0 ) {
                member.abstract_wound_until_minutes = -1;
            }
            const auto found = loaded_npcs.find( member.npc_id.get_value() );
            const npc *guy = found == loaded_npcs.end() ? nullptr : found->second;
            bandit_live_world::routine_member_readiness_snapshot snapshot;
            snapshot.present = guy != nullptr;
            if( guy != nullptr ) {
                snapshot.dead = guy->is_dead();
                snapshot.hp_percent = guy->hp_percentage();
                snapshot.sleeping = guy->in_sleep_state();
                snapshot.incapacitated = guy->has_effect( effect_downed ) ||
                                         guy->has_effect( effect_stunned ) ||
                                         guy->has_effect( effect_psi_stunned ) ||
                                         guy->has_effect( effect_narcosis );
            }
            member.wounded_or_unready = bandit_live_world::routine_member_is_unready( snapshot );
        }
    }
}

std::vector<bandit_live_world::response_member_power_read>
live_bandit_response_member_power_reads_impl( const bandit_live_world::site_record &site )
{
    const bandit_live_world::roster_view roster = site.roster();
    if( !roster.valid ) {
        return {};
    }
    std::vector<tripoint_abs_omt> source_omts = site.footprint;
    source_omts.push_back( site.anchor );
    std::sort( source_omts.begin(), source_omts.end() );
    source_omts.erase( std::unique( source_omts.begin(), source_omts.end() ),
                       source_omts.end() );
    if( source_omts.size() > live_bandit_response_source_omt_cap ) {
        return {};
    }
    std::unordered_map<int, npc *> overmap_npcs;
    for( const tripoint_abs_omt &source_omt : source_omts ) {
        for( const shared_ptr_fast<npc> &guy :
             overmap_buffer.get_npcs_near_omt( source_omt, 0 ) ) {
            if( guy != nullptr ) {
                overmap_npcs.emplace( guy->getID().get_value(), guy.get() );
            }
        }
    }

    std::vector<bandit_live_world::response_member_power_read> reads;
    reads.reserve( roster.physically_present_ids.size() );
    for( const character_id npc_id : roster.physically_present_ids ) {
        bandit_live_world::response_member_power_read read;
        read.npc_id = npc_id;
        const auto found = overmap_npcs.find( npc_id.get_value() );
        npc *guy = found == overmap_npcs.end() ? nullptr : found->second;
        read.authoritative_present = guy != nullptr;
        if( guy != nullptr ) {
            const tripoint_abs_omt position = guy->pos_abs_omt();
            read.at_source_camp = position == site.anchor || site_contains_omt( site, position );
            bandit_live_world::routine_member_readiness_snapshot snapshot;
            snapshot.dead = guy->is_dead();
            snapshot.hp_percent = guy->hp_percentage();
            snapshot.sleeping = guy->in_sleep_state();
            snapshot.incapacitated = guy->has_effect( effect_downed ) ||
                                     guy->has_effect( effect_stunned ) ||
                                     guy->has_effect( effect_psi_stunned ) ||
                                     guy->has_effect( effect_narcosis );
            read.ready = !bandit_live_world::routine_member_is_unready( snapshot );
            if( read.at_source_camp && read.ready ) {
                const bool has_gun = guy->get_wielded_item() &&
                                     guy->get_wielded_item()->is_gun();
                read.normalized_power =
                    bandit_live_world::normalize_hostile_camp_character_power(
                        guy->evaluate_character_threat_without_perception_fuzz(
                            *guy, has_gun, false ) );
            }
        }
        reads.push_back( std::move( read ) );
    }
    return reads;
}

int live_bandit_materialize_abstract_members_for_routine(
    bandit_live_world::world_state &state, bandit_live_world::site_record &site )
{
    if( site.source_kind != bandit_live_world::anchor_source_kind::overmap_special ||
        site.source_id.empty() || site.living_total <= 0 ) {
        return 0;
    }

    int members_to_create = bandit_live_world::routine_scout_materialization_count( site );
    const bandit_live_world::hostile_site_profile profile = site.profile ==
            bandit_live_world::hostile_site_profile::none ?
            bandit_live_world::profile_for_site_kind( site.site_kind ) : site.profile;
    if( profile == bandit_live_world::hostile_site_profile::small_hostile_site &&
        !site.retired_empty_site && !site.has_active_outside_pressure() ) {
        const bandit_live_world::roster_view roster = site.roster();
        if( roster.valid ) {
            members_to_create = std::min( roster.unmaterialized_home_total,
                                          std::max( 0, 1 - roster.ready_concrete_total ) );
        }
    }
    if( members_to_create <= 0 ) {
        return 0;
    }

    const npc_template_id template_id = live_bandit_template_for_site( site.site_kind );
    if( template_id.is_null() || !template_id.is_valid() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world lazy materialization skipped: site="
                                   << site.site_id << " reason=invalid_template template="
                                   << template_id.str() << '\n';
        return 0;
    }

    const auto special_lookup = [&site]( const tripoint_abs_omt & candidate ) -> std::optional<std::string> {
        if( candidate.z() != site.anchor.z() ) {
            return std::nullopt;
        }
        if( std::find( site.footprint.begin(), site.footprint.end(), candidate ) != site.footprint.end() ) {
            return site.source_id;
        }
        return std::nullopt;
    };

    int created_members = 0;
    for( int i = 0; i < members_to_create; ++i ) {
        shared_ptr_fast<npc> bandit = make_shared_fast<npc>();
        bandit->normalize();
        bandit->load_npc_template( template_id );
        const tripoint_abs_omt spawn_omt = site.footprint.empty() ? site.anchor :
                                           site.footprint[i % site.footprint.size()];
        bandit->spawn_at_omt( spawn_omt );
        bandit->toggle_trait( trait_NPC_STATIC_NPC );
        if( bandit_live_world::claim_tracked_spawn( state, template_id.str(), bandit->getID(),
                bandit->pos_abs(), site.source_id, std::nullopt, special_lookup ) ) {
            overmap_buffer.insert_npc( bandit );
            created_members++;
        } else {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world lazy materialization skipped member: site="
                                       << site.site_id << " reason=claim_failed template="
                                       << template_id.str() << '\n';
        }
    }

    if( created_members > 0 ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world lazy materialized abstract roster: site="
                                   << site.site_id << " created_members=" << created_members
                                   << " concrete_live_members=" << site.count_live_members()
                                   << " living_total=" << site.living_total
                                   << " template=" << template_id.str() << '\n';
    }
    return created_members;
}

struct live_bandit_local_handoff_member_backup {
    shared_ptr_fast<npc> member;
    tripoint_abs_ms position;
    tripoint_abs_omt goal;
    std::vector<tripoint_abs_omt> omt_path;
    npc_mission mission = NPC_MISSION_NULL;
    npc_mission previous_mission = NPC_MISSION_NULL;
    std::optional<tripoint_abs_ms> ordered_position;
    std::optional<tripoint_abs_ms> ai_guard_position;
    std::vector<tripoint_bub_ms> local_path;
};

std::vector<std::string> live_bandit_local_zombie_ids( std::vector<std::string> type_ids )
{
    static constexpr std::size_t id_cap = 16;
    std::sort( type_ids.begin(), type_ids.end() );
    unsigned long long hash = 1469598103934665603ULL;
    for( const std::string &type_id : type_ids ) {
        for( const unsigned char byte : type_id ) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xffU;
        hash *= 1099511628211ULL;
    }

    std::unordered_map<std::string, int> ordinal_by_type;
    std::vector<std::string> ids;
    const std::size_t concrete_cap = type_ids.size() > id_cap ? id_cap - 1 : id_cap;
    for( const std::string &type_id : type_ids ) {
        if( ids.size() >= concrete_cap ) {
            break;
        }
        ids.push_back( "local-zombie:" + type_id + ':' +
                       std::to_string( ++ordinal_by_type[type_id] ) );
    }
    if( type_ids.size() > id_cap ) {
        ids.push_back( "local-zombie:overflow:" + std::to_string( hash ) );
    }
    std::sort( ids.begin(), ids.end() );
    return ids;
}

std::optional<bandit_live_world::structural_local_zombie_read>
live_bandit_local_zombie_read_impl( const bandit_live_world::site_record &site )
{
    static constexpr std::size_t monster_scan_cap = 64;
    const bandit_live_world::active_outing_state &outing = site.active_outing;
    if( outing.kind != bandit_live_world::outing_kind::structural_sortie ||
        outing.schema_version < 8 ||
        outing.owner != bandit_live_world::simulation_owner::local ||
        !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ||
        outing.member_ids.size() != 2 ) {
        return std::nullopt;
    }

    std::vector<character_id> observer_ids = outing.member_ids;
    std::stable_sort( observer_ids.begin(), observer_ids.end(), [&outing](
    const character_id lhs, const character_id rhs ) {
        return std::make_tuple( lhs != outing.leader_id, lhs.get_value() ) <
               std::make_tuple( rhs != outing.leader_id, rhs.get_value() );
    } );
    map &here = get_map();
    std::vector<const monster *> inspected_monsters;
    inspected_monsters.reserve( monster_scan_cap );
    for( const monster &critter : g->all_monsters() ) {
        if( inspected_monsters.size() >= monster_scan_cap ) {
            break;
        }
        inspected_monsters.push_back( &critter );
    }
    if( inspected_monsters.empty() ) {
        return std::nullopt;
    }

    std::optional<bandit_live_world::structural_local_zombie_read> best;
    for( const character_id observer_id : observer_ids ) {
        npc *observer = g->find_npc( observer_id );
        if( observer == nullptr || !observer->is_active() || observer->is_dead() ||
            !here.inbounds( observer->pos_abs() ) ||
            observer->pos_abs_omt() != outing.local_handoff.route_position ) {
            continue;
        }

        int danger = 0;
        int farthest_distance = 0;
        std::vector<std::string> type_ids;
        for( const monster *candidate : inspected_monsters ) {
            const monster &critter = *candidate;
            const tripoint_abs_omt source_omt = critter.pos_abs_omt();
            const bool source_on_route = source_omt == outing.local_handoff.route_position;
            const bool alive = !critter.is_dead();
            const bool hallucination = critter.is_hallucination();
            const bool zombie_species = critter.type != nullptr &&
                                        critter.type->in_species( species_ZOMBIE );
            const bool zombie_rider = critter.type != nullptr &&
                                      critter.type->id == mon_zombie_rider;
            const bool hostile = alive &&
                                 critter.attitude_to( *observer ) == Creature::Attitude::HOSTILE;
            const bool visible = alive && here.inbounds( critter.pos_abs() ) &&
                                 observer->sees( here, critter );
            if( !bandit_live_world::structural_local_zombie_candidate_is_eligible(
                    alive, hallucination, zombie_species, zombie_rider, hostile, visible,
                    source_on_route ) ) {
                continue;
            }

            const int distance = rl_dist( observer->pos_abs(), critter.pos_abs() );
            const int monster_danger = static_cast<int>( std::ceil(
                                           observer->evaluate_monster( critter,
                                                   std::max( 1, distance ) ) ) );
            danger = std::min( 200, danger + std::clamp( monster_danger, 1, 200 ) );
            farthest_distance = std::max( farthest_distance, distance );
            type_ids.push_back( critter.type->id.str() );
        }
        if( type_ids.empty() || danger <= 0 ) {
            continue;
        }

        bandit_live_world::structural_local_zombie_read read;
        read.observer_id = observer_id;
        read.source_omt = outing.local_handoff.route_position;
        read.inspected_monsters = static_cast<int>( inspected_monsters.size() );
        read.visible_count = static_cast<int>( type_ids.size() );
        read.danger_low = danger;
        read.danger_high = danger;
        read.visual_quality = farthest_distance <= 12 ? 3 : farthest_distance <= 24 ? 2 : 1;
        read.stable_threat_ids = live_bandit_local_zombie_ids( std::move( type_ids ) );
        if( !best || std::make_tuple( read.visible_count, read.danger_high,
                                     read.observer_id == outing.leader_id ) >
            std::make_tuple( best->visible_count, best->danger_high,
                             best->observer_id == outing.leader_id ) ) {
            best = std::move( read );
        }
    }
    return best;
}

int record_live_bandit_local_zombie_observations()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int now_minutes = live_bandit_current_minutes();
    int recorded = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor || cursor->owner != bandit_live_world::simulation_owner::local ||
            now_minutes <= cursor->last_advanced_minutes ) {
            continue;
        }
        const std::optional<bandit_live_world::structural_local_zombie_read> read =
            bandit_live_world::read_live_structural_local_zombie_observation( site );
        if( !read ) {
            continue;
        }
        const bandit_live_world::sortie_observation_effect effect =
            bandit_live_world::record_structural_local_zombie_observation(
                site, *cursor, *read, now_minutes );
        if( effect.valid ) {
            recorded++;
        }
    }
    return recorded;
}

std::map<character_id, tripoint_abs_ms> maintain_live_bandit_local_pair_cohesion()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    std::map<character_id, tripoint_abs_ms> assembly_orders;
    const auto append_assembly_orders = [&assembly_orders](
    const bandit_live_world::active_outing_state & outing ) {
        for( const std::pair<const character_id, tripoint_abs_ms> &order :
             bandit_live_world::local_pair_assembly_orders( outing ) ) {
            assembly_orders.emplace( order );
        }
    };
    struct pending_cohesion {
        bandit_live_world::site_record *site = nullptr;
        bandit_live_world::local_cohesion_plan plan;
        int cohesion_minutes = 0;
        int cohesion_deadline_before = -1;
        std::string cohesion_reads;
    };
    std::vector<pending_cohesion> pending;
    std::set<character_id> claimed_members;
    bool ownership_preflight_failed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            ownership_preflight_failed = true;
        }
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.schema_version < 7 ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<bandit_live_world::local_cohesion_member_read> reads;
        reads.reserve( outing.local_handoff.members.size() );
        for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
             outing.local_handoff.members ) {
            bandit_live_world::local_cohesion_member_read read;
            read.npc_id = snapshot.npc_id;
            read.dead = snapshot.dead;
            read.current_position = snapshot.exit_position;
            if( !snapshot.dead ) {
                shared_ptr_fast<npc> member = overmap_buffer.find_npc( snapshot.npc_id );
                if( member && !member->is_dead() ) {
                    read.present = true;
                    read.current_position = member->pos_abs();
                }
            }
            reads.push_back( read );
        }
        const int cohesion_minutes = live_bandit_current_minutes();
        const int cohesion_deadline_before =
            outing.local_handoff.cohesion_deadline_minutes;
        std::ostringstream cohesion_reads;
        for( std::size_t index = 0; index < outing.local_handoff.members.size(); ++index ) {
            const bandit_live_world::local_handoff_member_snapshot &snapshot =
                outing.local_handoff.members[index];
            const auto read = std::find_if( reads.begin(), reads.end(),
            [&snapshot]( const bandit_live_world::local_cohesion_member_read &candidate ) {
                return candidate.npc_id == snapshot.npc_id;
            } );
            const int best_distance = index <
                                      outing.local_handoff.cohesion_best_staging_distances.size() ?
                                      outing.local_handoff.cohesion_best_staging_distances[index] : -1;
            cohesion_reads << ( index == 0 ? "" : ";" )
                           << snapshot.npc_id.get_value()
                           << ":present=" << ( read != reads.end() && read->present ? "yes" : "no" )
                           << ",dead=" << ( read != reads.end() && read->dead ? "yes" : "no" )
                           << ",current=" << ( read != reads.end() ?
                                                 read->current_position.to_string() : "missing" )
                           << ",staging=" << snapshot.staging_position.to_string()
                           << ",distance=" << ( read != reads.end() ?
                                                 rl_dist( read->current_position,
                                                          snapshot.staging_position ) : -1 )
                           << ",best_before=" << best_distance;
        }
        const bandit_live_world::local_cohesion_plan plan =
            bandit_live_world::plan_local_pair_cohesion(
                site, *cursor, cohesion_minutes, reads );
        if( !plan.valid ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_cohesion"
                                       << " site=" << site.site_id
                                       << " activity=" << site.active_outing.activity_id
                                       << " minute=" << cohesion_minutes
                                       << " plan_valid=no"
                                       << " deadline_before=" << cohesion_deadline_before
                                       << " deadline_after=" << cohesion_deadline_before
                                       << " cohesion_reads=" << cohesion_reads.str()
                                       << '\n';
            continue;
        }
        pending.push_back( { &site, plan, cohesion_minutes, cohesion_deadline_before,
                             cohesion_reads.str() } );
    }
    if( ownership_preflight_failed ) {
        return {};
    }

    for( pending_cohesion &work : pending ) {
        bandit_live_world::site_record &site = *work.site;
        const bandit_live_world::local_cohesion_plan &plan = work.plan;
        struct order_backup {
            npc *member = nullptr;
            std::optional<tripoint_abs_ms> ordered_position;
            std::optional<tripoint_abs_ms> ai_guard_position;
            std::vector<tripoint_bub_ms> path;
        };
        std::vector<order_backup> backups;
        bool route_attempted = false;
        bool route_failed = false;
        int routed_path_steps = 0;
        for( const std::pair<character_id, tripoint_abs_ms> &order : plan.movement_orders ) {
            npc *member = g->find_npc( order.first );
            if( member == nullptr || member->is_dead() || !here.inbounds( member->pos_abs() ) ||
                !here.inbounds( order.second ) ) {
                continue;
            }
            backups.push_back( { member, member->goto_to_this_pos,
                                 member->get_ai_guard_pos(), member->path } );
            member->goto_to_this_pos = order.second;
            member->clear_ai_guard_pos();
            route_attempted = true;
            if( !live_bandit_update_local_path( *member, here.get_bub( order.second ) ) ) {
                route_failed = true;
            } else {
                routed_path_steps += static_cast<int>( member->path.size() );
            }
        }

        if( !bandit_live_world::commit_local_pair_cohesion(
                site, plan, route_attempted, route_failed ) ) {
            for( const order_backup &backup : backups ) {
                backup.member->goto_to_this_pos = backup.ordered_position;
                if( backup.ai_guard_position ) {
                    backup.member->set_ai_guard_pos( *backup.ai_guard_position );
                } else {
                    backup.member->clear_ai_guard_pos();
                }
                backup.member->path = backup.path;
            }
            // A same-minute cohesion re-read has no persisted delta, but the local owner
            // must retain motor priority for the rest of that minute.  Any attempted route
            // that fails to commit instead remains fail-closed.
            if( !route_attempted && plan.movement_orders.empty() ) {
                append_assembly_orders( site.active_outing );
            }
            continue;
        }

        for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
             site.active_outing.local_handoff.members ) {
            npc *member = g->find_npc( snapshot.npc_id );
            if( member == nullptr || member->is_dead() ) {
                continue;
            }
            if( site.active_outing.local_handoff.cohesion_abort_return ||
                ( site.active_outing.local_handoff.cohesion_assembled &&
                  bandit_live_world::scout_phase_requires_homeward_only(
                      site.active_outing.phase ) ) ) {
                member->goto_to_this_pos = std::nullopt;
                member->clear_ai_guard_pos();
                member->path.clear();
            } else if( site.active_outing.local_handoff.cohesion_assembled ) {
                member->goto_to_this_pos = std::nullopt;
                member->set_ai_guard_pos( snapshot.staging_position );
                member->path.clear();
            }
        }
        std::ostringstream member_positions;
        for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
             site.active_outing.local_handoff.members ) {
            npc *member = g->find_npc( snapshot.npc_id );
            member_positions << ( member_positions.tellp() > 0 ? ";" : "" )
                             << snapshot.npc_id.get_value() << ':';
            if( member == nullptr ) {
                member_positions << "missing";
            } else {
                member_positions << member->pos_abs().to_string()
                                 << "->" << snapshot.staging_position.to_string();
            }
        }
        std::ostringstream cohesion_best_after;
        for( std::size_t index = 0; index < plan.snapshot.members.size(); ++index ) {
            const bandit_live_world::local_handoff_member_snapshot &snapshot =
                plan.snapshot.members[index];
            const int best_distance = index < plan.snapshot.cohesion_best_staging_distances.size() ?
                                       plan.snapshot.cohesion_best_staging_distances[index] : -1;
            cohesion_best_after << ( index == 0 ? "" : ";" )
                                << snapshot.npc_id.get_value()
                                << ":staging=" << snapshot.staging_position.to_string()
                                << ",best=" << best_distance;
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_cohesion"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " minute=" << work.cohesion_minutes
                                   << " leader=" << site.active_outing.leader_id.get_value()
                                   << " assembled=" <<
                                   ( site.active_outing.local_handoff.cohesion_assembled ? "yes" : "no" )
                                   << " failed_routes=" <<
                                   site.active_outing.local_handoff.cohesion_reroutes_used
                                   << " movement_orders=" << plan.movement_orders.size()
                                   << " route_attempted=" << ( route_attempted ? "yes" : "no" )
                                   << " route_failed=" << ( route_failed ? "yes" : "no" )
                                   << " path_steps=" << routed_path_steps
                                   << " member_positions=" << member_positions.str()
                                   << " plan_valid=yes"
                                   << " deadline_before=" << work.cohesion_deadline_before
                                   << " deadline_after=" << plan.snapshot.cohesion_deadline_minutes
                                   << " cohesion_reads=" << work.cohesion_reads
                                   << " cohesion_best_after=" << cohesion_best_after.str()
                                   << " abort=" <<
                                   ( site.active_outing.local_handoff.cohesion_abort_return ? "yes" : "no" )
                                   << '\n';
        append_assembly_orders( site.active_outing );
    }
    return assembly_orders;
}

bool complete_loaded_live_bandit_alternate_watch_repositions()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( !outing.alternate_watch_reposition_pending ||
            outing.owner != bandit_live_world::simulation_owner::local ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }
        std::vector<npc *> members;
        std::vector<bandit_live_world::local_alternate_watch_member_read> reads;
        members.reserve( outing.member_ids.size() );
        reads.reserve( outing.member_ids.size() );
        bool any_loaded = false;
        bool complete = true;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member == nullptr || member->is_dead() ) {
                complete = false;
                break;
            }
            any_loaded |= member->is_active() || here.inbounds( member->pos_abs() );
            bandit_live_world::local_alternate_watch_member_read read;
            read.npc_id = member_id;
            read.readable = true;
            read.alternate_route_confirmed =
                member->goal == outing.alternate_watch_omt ||
                member->pos_abs_omt() == outing.alternate_watch_omt;
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            reads.push_back( read );
            members.push_back( member );
        }
        if( !complete || !any_loaded || reads.size() != 2 ) {
            continue;
        }
        const bandit_live_world::local_alternate_watch_reposition_plan plan =
            bandit_live_world::plan_local_pair_alternate_watch_reposition(
                site, *cursor, live_bandit_current_minutes(), reads );
        if( !plan.valid ||
            bandit_live_world::commit_loaded_local_pair_alternate_watch_reposition(
                site, plan ) !=
            bandit_live_world::local_handoff_commit_result::applied ) {
            continue;
        }
        for( npc *member : members ) {
            member->goal = npc::no_goal_point;
            member->omt_path.clear();
            member->mission = NPC_MISSION_NULL;
            member->previous_mission = NPC_MISSION_NULL;
            member->goto_to_this_pos = std::nullopt;
            member->path.clear();
            member->set_guard_pos( member->pos_abs() );
        }
        changed = true;
    }
    return changed;
}

bool complete_loaded_live_bandit_route_arrivals()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.schema_version != 10 ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            !outing.local_handoff.is_active() ||
            !outing.local_handoff.cohesion_assembled ||
            outing.waypoint_index + 1 >= static_cast<int>( outing.shared_route.size() ) ||
            outing.local_handoff.egress_omt != outing.selected_watch_omt ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }
        std::vector<npc *> members;
        std::vector<bandit_live_world::local_route_arrival_member_read> reads;
        bool any_loaded = false;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member == nullptr || member->is_dead() ) {
                reads.clear();
                break;
            }
            any_loaded |= member->is_active() || here.inbounds( member->pos_abs() );
            bandit_live_world::local_route_arrival_member_read read;
            read.npc_id = member_id;
            read.readable = true;
            read.route_confirmed = member->goal == outing.selected_watch_omt ||
                                   member->pos_abs_omt() == outing.selected_watch_omt;
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            reads.push_back( read );
            members.push_back( member );
        }
        if( !any_loaded || reads.size() != 2 ||
            bandit_live_world::commit_local_pair_route_arrival(
                site, *cursor, live_bandit_current_minutes(), reads ) !=
            bandit_live_world::local_handoff_commit_result::applied ) {
            continue;
        }
        for( npc *member : members ) {
            member->goal = npc::no_goal_point;
            member->omt_path.clear();
            member->mission = NPC_MISSION_NULL;
            member->previous_mission = NPC_MISSION_NULL;
            member->goto_to_this_pos = std::nullopt;
            member->path.clear();
            member->set_guard_pos( member->pos_abs() );
        }
        changed = true;
    }
    return changed;
}

bool dematerialize_live_bandit_structural_handoffs()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.schema_version < 7 ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<live_bandit_local_handoff_member_backup> backups;
        std::vector<bandit_live_world::local_dematerialization_member_read> reads;
        backups.reserve( outing.local_handoff.members.size() );
        reads.reserve( outing.local_handoff.members.size() );
        bool preflight_failed = false;
        for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
             outing.local_handoff.members ) {
            const bandit_live_world::member_record *persisted_member = site.find_member(
                        snapshot.npc_id );
            const bool casualty_recorded = std::find( outing.casualty_ids.begin(),
                                           outing.casualty_ids.end(), snapshot.npc_id ) !=
                                           outing.casualty_ids.end();
            if( snapshot.dead ) {
                if( persisted_member == nullptr ||
                    persisted_member->state != bandit_live_world::member_state::dead ||
                    !outing.member_is_resolved( snapshot.npc_id ) || !casualty_recorded ) {
                    preflight_failed = true;
                    break;
                }
                bandit_live_world::local_dematerialization_member_read read;
                read.npc_id = snapshot.npc_id;
                read.readable = true;
                read.dead = true;
                read.current_position = snapshot.exit_position;
                reads.push_back( read );
                continue;
            }
            shared_ptr_fast<npc> member = overmap_buffer.find_npc( snapshot.npc_id );
            const bool stable_unloaded = member && !member->is_active() &&
                                         !here.inbounds( member->pos_abs() );
            const bool inactive_homeward_arrival = member && !member->is_active() &&
                    here.inbounds( member->pos_abs() ) &&
                    bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
                    !outing.member_is_resolved( snapshot.npc_id ) &&
                    std::find( outing.member_ids.begin(), outing.member_ids.end(),
                               snapshot.npc_id ) != outing.member_ids.end() &&
                    site_contains_omt( site, member->pos_abs_omt() );
            const bool loaded_homeward_arrival = member && member->is_active() &&
                    here.inbounds( member->pos_abs() ) &&
                    bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
                    site_contains_omt( site, member->pos_abs_omt() );
            if( persisted_member == nullptr || casualty_recorded || !member ||
                member->is_dead() || ( !stable_unloaded && !inactive_homeward_arrival &&
                                       !loaded_homeward_arrival ) ) {
                preflight_failed = true;
                break;
            }
            backups.push_back( { member, member->pos_abs(), member->goal, member->omt_path,
                                 member->mission, member->previous_mission,
                                 member->goto_to_this_pos, member->get_ai_guard_pos(), member->path } );
            bandit_live_world::local_dematerialization_member_read read;
            read.npc_id = member->getID();
            read.readable = true;
            read.homeward_route_confirmed = live_bandit_member_routing_home( *member, site ) ||
                                            site_contains_omt( site, member->pos_abs_omt() );
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            reads.push_back( read );
        }
        if( preflight_failed ) {
            continue;
        }
        const auto find_backup = [&backups]( const character_id member_id ) {
            return std::find_if( backups.begin(), backups.end(),
            [&member_id]( const live_bandit_local_handoff_member_backup & backup ) {
                return backup.member->getID() == member_id;
            } );
        };
        const auto quiesce_member = [&find_backup, &backups, &here, &site](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            if( snapshot.dead ) {
                return true;
            }
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() || backup->member->is_dead() ||
                backup->member->pos_abs() != snapshot.exit_position ) {
                return false;
            }
            const bool stable_unloaded = !backup->member->is_active() &&
                                         !here.inbounds( backup->member->pos_abs() );
            const bool inactive_homeward_arrival = !backup->member->is_active() &&
                    here.inbounds( backup->member->pos_abs() ) &&
                    bandit_live_world::scout_phase_requires_homeward_only(
                        site.active_outing.phase ) &&
                    !site.active_outing.member_is_resolved( snapshot.npc_id ) &&
                    std::find( site.active_outing.member_ids.begin(),
                               site.active_outing.member_ids.end(), snapshot.npc_id ) !=
                    site.active_outing.member_ids.end() &&
                    site_contains_omt( site, backup->member->pos_abs_omt() );
            const bool loaded_homeward_arrival = backup->member->is_active() &&
                    here.inbounds( backup->member->pos_abs() ) &&
                    bandit_live_world::scout_phase_requires_homeward_only(
                        site.active_outing.phase ) &&
                    site_contains_omt( site, backup->member->pos_abs_omt() );
            if( !stable_unloaded && !inactive_homeward_arrival && !loaded_homeward_arrival ) {
                return false;
            }
            backup->member->goal = npc::no_goal_point;
            backup->member->omt_path.clear();
            backup->member->mission = NPC_MISSION_NULL;
            backup->member->previous_mission = NPC_MISSION_NULL;
            backup->member->goto_to_this_pos = std::nullopt;
            backup->member->clear_ai_guard_pos();
            backup->member->path.clear();
            if( loaded_homeward_arrival ) {
                backup->member->on_unload();
                g->remove_npc( backup->member->getID() );
            }
            return !backup->member->is_active();
        };
        const auto rollback_member = [&find_backup, &backups, &here](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() ) {
                return;
            }
            backup->member->goal = backup->goal;
            backup->member->omt_path = backup->omt_path;
            backup->member->mission = backup->mission;
            backup->member->previous_mission = backup->previous_mission;
            backup->member->goto_to_this_pos = backup->ordered_position;
            if( backup->ai_guard_position ) {
                backup->member->set_ai_guard_pos( *backup->ai_guard_position );
            } else {
                backup->member->clear_ai_guard_pos();
            }
            backup->member->path = backup->local_path;
            if( here.inbounds( backup->position ) && !backup->member->is_active() ) {
                g->load_npcs();
            }
        };

        if( outing.alternate_watch_reposition_pending ) {
            std::vector<bandit_live_world::local_alternate_watch_member_read> alternate_reads;
            alternate_reads.reserve( reads.size() );
            for( const bandit_live_world::local_dematerialization_member_read &read : reads ) {
                const auto backup = find_backup( read.npc_id );
                bandit_live_world::local_alternate_watch_member_read alternate_read;
                alternate_read.npc_id = read.npc_id;
                alternate_read.readable = read.readable;
                alternate_read.dead = read.dead;
                alternate_read.alternate_route_confirmed =
                    backup != backups.end() &&
                    ( backup->member->goal == outing.alternate_watch_omt ||
                      backup->member->pos_abs_omt() == outing.alternate_watch_omt );
                alternate_read.hp_percent = read.hp_percent;
                alternate_read.current_position = read.current_position;
                alternate_reads.push_back( alternate_read );
            }
            const bandit_live_world::local_alternate_watch_reposition_plan alternate_plan =
                bandit_live_world::plan_local_pair_alternate_watch_reposition(
                    site, *cursor, live_bandit_current_minutes(), alternate_reads );
            if( !alternate_plan.valid ) {
                continue;
            }
            const bandit_live_world::local_handoff_commit_result alternate_result =
                bandit_live_world::commit_local_pair_alternate_watch_reposition(
                    site, alternate_plan, quiesce_member, rollback_member );
            if( alternate_result !=
                bandit_live_world::local_handoff_commit_result::applied ) {
                continue;
            }
            changed = true;
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world local alternate-watch reposition committed"
                    << " site=" << site.site_id
                    << " activity=" << site.active_outing.activity_id
                    << " generation=" << site.active_outing.generation
                    << " epoch=" << site.active_outing.handoff_epoch
                    << " route_position="
                    << site.active_outing.local_handoff.route_position.to_string()
                    << " members=" << reads.size() << '\n';
            continue;
        }

        const bandit_live_world::local_dematerialization_plan plan =
            bandit_live_world::plan_local_pair_dematerialization(
                site, *cursor, live_bandit_current_minutes(), reads, outing.cargo );
        if( !plan.valid ) {
            continue;
        }
        const bandit_live_world::local_handoff_commit_result result =
            bandit_live_world::commit_local_pair_dematerialization(
                site, plan, quiesce_member, rollback_member );
        if( result != bandit_live_world::local_handoff_commit_result::applied ) {
            continue;
        }
        changed = true;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_dematerialization committed"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " generation=" << site.active_outing.generation
                                   << " epoch=" << site.active_outing.handoff_epoch
                                   << " phase=" << bandit_live_world::to_string(
                                       site.active_outing.phase )
                                   << " route_position="
                                   << site.active_outing.local_handoff.route_position.to_string()
                                   << " members=" << reads.size() << '\n';
    }
    return changed;
}

std::vector<tripoint_abs_ms> live_bandit_local_handoff_entry_positions(
    const tripoint_abs_omt &route_position, const tripoint_abs_omt &approach_from,
    const std::size_t required_count,
    const std::vector<tripoint_abs_ms> &excluded_positions = {} )
{
    struct entry_candidate {
        tripoint_bub_ms bubble;
        tripoint_abs_ms absolute;
        int edge_distance = 0;
    };

    std::vector<entry_candidate> candidates;
    map &here = get_map();
    const tripoint_abs_sm motor_center = get_player_character().pos_abs_sm();
    const tripoint_abs_ms omt_origin = project_to<coords::ms>( route_position );
    const int max_local = coords::map_squares_per( coords::omt ) - 1;
    const int dx = route_position.x() - approach_from.x();
    const int dy = route_position.y() - approach_from.y();
    for( const tripoint_bub_ms &point : here.points_on_zlevel( route_position.z() ) ) {
        const tripoint_abs_ms absolute = here.get_abs( point );
        if( project_to<coords::omt>( absolute ) != route_position ||
            !live_bandit_local_handoff_position_is_motor_addressable(
                absolute, motor_center, HALF_MAPSIZE - 1 ) || !here.passable( point ) ||
            !g->is_empty( point ) ||
            std::find( excluded_positions.begin(), excluded_positions.end(), absolute ) !=
            excluded_positions.end() ) {
            continue;
        }
        const int local_x = absolute.x() - omt_origin.x();
        const int local_y = absolute.y() - omt_origin.y();
        int edge_distance = 0;
        if( dx > 0 ) {
            edge_distance += local_x;
        } else if( dx < 0 ) {
            edge_distance += max_local - local_x;
        }
        if( dy > 0 ) {
            edge_distance += local_y;
        } else if( dy < 0 ) {
            edge_distance += max_local - local_y;
        }
        candidates.push_back( { point, absolute, edge_distance } );
    }
    std::sort( candidates.begin(), candidates.end(), []( const entry_candidate &lhs,
    const entry_candidate &rhs ) {
        return std::tie( lhs.edge_distance, lhs.absolute ) <
               std::tie( rhs.edge_distance, rhs.absolute );
    } );

    if( required_count == 1 && !candidates.empty() ) {
        return { candidates.front().absolute };
    }
    if( required_count != 2 ) {
        return {};
    }
    for( std::size_t first = 0; first < candidates.size(); ++first ) {
        for( std::size_t second = first + 1; second < candidates.size(); ++second ) {
            if( rl_dist( candidates[first].bubble, candidates[second].bubble ) <= 1 ) {
                return { candidates[first].absolute, candidates[second].absolute };
            }
        }
    }
    return {};
}

bool materialize_live_bandit_structural_handoffs()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool homeward_candidate = outing.is_active() &&
                                        outing.kind == bandit_live_world::outing_kind::structural_sortie &&
                                        outing.owner == bandit_live_world::simulation_owner::abstract &&
                                        bandit_live_world::active_outing_requires_homeward_routing( outing );
        const auto log_homeward_rejection = [&site, &outing, homeward_candidate](
        const std::string_view reason ) {
            if( !homeward_candidate ) {
                return;
            }
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world homeward materialization rejected"
                    << " site=" << site.site_id
                    << " activity=" << outing.activity_id
                    << " generation=" << outing.generation
                    << " phase=" << bandit_live_world::to_string( outing.phase )
                    << " waypoint=" << outing.waypoint_index
                    << " route_size=" << outing.shared_route.size()
                    << " reason=" << reason << '\n';
        };
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::abstract ||
            outing.shared_route.empty() || outing.waypoint_index <= 0 ||
            outing.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ) {
            log_homeward_rejection( "ineligible route cursor" );
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            log_homeward_rejection( "invalid simulation cursor" );
            continue;
        }

        std::vector<character_id> surviving_member_ids;
        for( const character_id &member_id : outing.member_ids ) {
            if( !outing.member_is_resolved( member_id ) &&
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) ==
                outing.casualty_ids.end() ) {
                surviving_member_ids.push_back( member_id );
            }
        }
        const bool resumes_physical_homeward_cursor = outing.local_handoff.is_abstract_resume();
        const bool resumes_assembled_homeward_pair = resumes_physical_homeward_cursor &&
                outing.local_handoff.cohesion_assembled &&
                bandit_live_world::scout_phase_requires_homeward_only( outing.phase );
        tripoint_abs_omt route_position = resumes_physical_homeward_cursor ?
                                          outing.local_handoff.route_position :
                                          outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
        tripoint_abs_omt approach_from = resumes_physical_homeward_cursor ?
                                         outing.local_handoff.approach_from :
                                         outing.shared_route[static_cast<std::size_t>( outing.waypoint_index - 1 )];
        std::vector<tripoint_abs_ms> entry_positions =
            live_bandit_local_handoff_entry_positions( route_position, approach_from,
                    surviving_member_ids.size() );
        tripoint_abs_omt egress_omt = resumes_physical_homeward_cursor ?
                                      outing.shared_route.back() :
                                      outing.waypoint_index + 1 <
                                      static_cast<int>( outing.shared_route.size() ) ?
                                      outing.shared_route[static_cast<std::size_t>(
                                              outing.waypoint_index + 1 )] : route_position;
        tripoint_abs_omt staging_facing_omt = egress_omt;
        if( !resumes_physical_homeward_cursor && outing.schema_version >= 10 &&
            outing.phase == bandit_live_world::scout_phase::observing &&
            outing.selected_watch_kind != bandit_live_world::structural_watch_kind::none &&
            route_position == outing.selected_watch_omt ) {
            const std::optional<tripoint_abs_omt> target_facing_omt =
                bandit_live_world::nearest_target_footprint_omt(
                    route_position, outing.target_footprint );
            if( !target_facing_omt ) {
                log_homeward_rejection( "watch target facing unavailable" );
                continue;
            }
            staging_facing_omt = *target_facing_omt;
        }
        std::vector<tripoint_abs_ms> staging_positions =
            live_bandit_local_handoff_entry_positions( route_position, staging_facing_omt,
                    surviving_member_ids.size(), entry_positions );
        const std::size_t initial_entry_count = entry_positions.size();
        const std::size_t initial_staging_count = staging_positions.size();
        const std::optional<int> current_target_distance =
            bandit_live_world::target_footprint_watch_distance(
                outing.local_handoff.route_position, outing.target_footprint );
        const bool assessed_homeward_cursor = resumes_physical_homeward_cursor &&
                outing.local_handoff.route_position ==
                outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
        std::optional<bandit_live_world::site_record> advanced_homeward_site;
        if( resumes_assembled_homeward_pair && surviving_member_ids.size() == 2 &&
            ( entry_positions.size() != surviving_member_ids.size() ||
              staging_positions.size() != surviving_member_ids.size() ) ) {
            for( std::size_t route_index = static_cast<std::size_t>( outing.waypoint_index + 1 );
                 route_index + 1 < outing.shared_route.size(); ++route_index ) {
                std::vector<tripoint_abs_omt> candidate_routes = {
                    outing.shared_route[route_index]
                };
                const std::vector<tripoint_abs_omt> segment = line_to(
                            outing.shared_route[route_index], outing.shared_route[route_index + 1] );
                candidate_routes.insert( candidate_routes.end(), segment.begin(), segment.end() );
                for( std::size_t candidate_index = 0;
                     candidate_index < candidate_routes.size(); ++candidate_index ) {
                    const tripoint_abs_omt candidate_route = candidate_routes[candidate_index];
                    if( candidate_route == outing.shared_route.back() ||
                        candidate_route == outing.shared_route[static_cast<std::size_t>(
                                    outing.waypoint_index )] ) {
                        continue;
                    }
                    const tripoint_abs_omt candidate_approach = candidate_index == 0 ?
                            outing.shared_route[route_index - 1] :
                            candidate_routes[candidate_index - 1];
                    const tripoint_abs_omt candidate_facing =
                        candidate_index + 1 < candidate_routes.size() ?
                        candidate_routes[candidate_index + 1] :
                        outing.shared_route[route_index + 1];
                    const bool derived_route =
                        std::find( outing.shared_route.begin(), outing.shared_route.end(),
                                   candidate_route ) == outing.shared_route.end();
                    const std::optional<int> candidate_target_distance =
                        bandit_live_world::target_footprint_watch_distance(
                            candidate_route, outing.target_footprint );
                    if( derived_route &&
                        ( !assessed_homeward_cursor || !current_target_distance ||
                          !candidate_target_distance ||
                          *candidate_target_distance < *current_target_distance ) ) {
                        continue;
                    }
                    std::vector<tripoint_abs_ms> candidate_entries =
                        live_bandit_local_handoff_entry_positions(
                            candidate_route, candidate_approach, surviving_member_ids.size() );
                    std::vector<tripoint_abs_ms> candidate_staging =
                        live_bandit_local_handoff_entry_positions(
                            candidate_route, candidate_facing,
                            surviving_member_ids.size(), candidate_entries );
                    if( candidate_entries.size() == surviving_member_ids.size() &&
                        candidate_staging.size() != surviving_member_ids.size() ) {
                        candidate_staging = candidate_entries;
                    }
                    if( candidate_entries.size() != surviving_member_ids.size() ||
                        candidate_staging.size() != surviving_member_ids.size() ) {
                        DebugLog( D_INFO, DC_ALL )
                                << "bandit_live_world homeward materialization candidate rejected"
                                << " site=" << site.site_id
                                << " activity=" << outing.activity_id
                                << " generation=" << outing.generation
                                << " phase=" << bandit_live_world::to_string( outing.phase )
                                << " candidate_route=" << candidate_route.to_string()
                                << " entry_count=" << candidate_entries.size()
                                << " staging_count=" << candidate_staging.size()
                                << " required_survivors=" << surviving_member_ids.size() << '\n';
                        continue;
                    }

                    route_position = candidate_route;
                    approach_from = candidate_approach;
                    egress_omt = outing.shared_route.back();
                    entry_positions = std::move( candidate_entries );
                    staging_positions = std::move( candidate_staging );
                    advanced_homeward_site = site;
                    bandit_live_world::local_handoff_snapshot &resume =
                        advanced_homeward_site->active_outing.local_handoff;
                    resume.route_position = route_position;
                    resume.approach_from = approach_from;
                    resume.egress_omt = egress_omt;
                    for( bandit_live_world::local_handoff_member_snapshot &member : resume.members ) {
                        const auto surviving = std::find( surviving_member_ids.begin(),
                                                          surviving_member_ids.end(), member.npc_id );
                        if( surviving != surviving_member_ids.end() && !member.dead ) {
                            member.exit_position = entry_positions[static_cast<std::size_t>(
                                                       std::distance( surviving_member_ids.begin(), surviving ) )];
                        }
                    }
                    break;
                }
                if( advanced_homeward_site ) {
                    break;
                }
            }
        }
        if( entry_positions.size() != surviving_member_ids.size() ||
            staging_positions.size() != surviving_member_ids.size() ) {
            if( homeward_candidate ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world homeward materialization rejected"
                        << " site=" << site.site_id
                        << " activity=" << outing.activity_id
                        << " generation=" << outing.generation
                        << " phase=" << bandit_live_world::to_string( outing.phase )
                        << " waypoint=" << outing.waypoint_index
                        << " route_size=" << outing.shared_route.size()
                        << " route_position=" << route_position.to_string()
                        << " abstract_resume=" << ( resumes_physical_homeward_cursor ? "yes" : "no" )
                        << " cohesion_assembled=" <<
                           ( outing.local_handoff.cohesion_assembled ? "yes" : "no" )
                        << " homeward_phase=" <<
                           ( bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ?
                             "yes" : "no" )
                        << " survivor_pair=" << ( surviving_member_ids.size() == 2 ? "yes" : "no" )
                        << " recenter_gate=" << ( resumes_assembled_homeward_pair ? "yes" : "no" )
                        << " assessed_cursor=" << ( assessed_homeward_cursor ? "yes" : "no" )
                        << " target_distance_known=" << ( current_target_distance ? "yes" : "no" )
                        << " initial_entry_count=" << initial_entry_count
                        << " initial_staging_count=" << initial_staging_count
                        << " final_entry_count=" << entry_positions.size()
                        << " final_staging_count=" << staging_positions.size()
                        << " required_survivors=" << surviving_member_ids.size()
                        << " reason=loaded bubble lacks paired entry or staging positions\n";
            }
            continue;
        }

        std::vector<live_bandit_local_handoff_member_backup> backups;
        std::vector<bandit_live_world::local_handoff_member_read> reads;
        backups.reserve( surviving_member_ids.size() );
        reads.reserve( surviving_member_ids.size() );
        bool preflight_failed = false;
        for( std::size_t index = 0; index < surviving_member_ids.size(); ++index ) {
            const character_id member_id = surviving_member_ids[index];
            shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( !member || member->is_dead() || member->is_active() ) {
                preflight_failed = true;
                log_homeward_rejection( !member ? "member unavailable" :
                                        member->is_dead() ? "member dead" :
                                        "member already active" );
                break;
            }
            backups.push_back( { member, member->pos_abs(), member->goal, member->omt_path,
                                 member->mission, member->previous_mission,
                                 member->goto_to_this_pos, member->get_ai_guard_pos(), member->path } );
            bandit_live_world::local_handoff_member_read read;
            read.npc_id = member_id;
            read.bindable = true;
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            read.entry_position = entry_positions[index];
            read.staging_position = staging_positions[index];
            reads.push_back( read );
        }
        if( preflight_failed ) {
            continue;
        }

        const bandit_live_world::local_handoff_plan plan =
            bandit_live_world::plan_local_pair_handoff(
                advanced_homeward_site ? *advanced_homeward_site : site,
                *cursor, live_bandit_current_minutes(), reads );
        if( !plan.valid ) {
            log_homeward_rejection( plan.notes.empty() ? "handoff plan invalid" :
                                    plan.notes.front() );
            continue;
        }
        const auto find_backup = [&backups]( const character_id member_id ) {
            return std::find_if( backups.begin(), backups.end(),
            [&member_id]( const live_bandit_local_handoff_member_backup & backup ) {
                return backup.member->getID() == member_id;
            } );
        };
        const bool homeward_handoff =
            bandit_live_world::scout_phase_requires_homeward_only( outing.phase );
        std::string bind_failure_reason;
        const auto bind_member = [&find_backup, &backups, &site, homeward_handoff,
                                  &bind_failure_reason](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            if( snapshot.dead ) {
                return true;
            }
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() ) {
                bind_failure_reason = "member backup unavailable";
                return false;
            }
            shared_ptr_fast<npc> member = overmap_buffer.remove_npc( snapshot.npc_id );
            if( !member || member != backup->member ) {
                bind_failure_reason = "member ownership changed during bind";
                if( member ) {
                    overmap_buffer.insert_npc( member );
                }
                return false;
            }
            member->spawn_at_precise( snapshot.entry_position );
            member->in_vehicle = false;
            member->controlling_vehicle = false;
            member->goal = npc::no_goal_point;
            member->omt_path.clear();
            member->mission = NPC_MISSION_NULL;
            member->previous_mission = NPC_MISSION_NULL;
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
            if( homeward_handoff && !live_bandit_route_member_home( *member, site ) ) {
                bind_failure_reason = "camp route unavailable after spawn";
                overmap_buffer.insert_npc( member );
                return false;
            }
            overmap_buffer.insert_npc( member );
            return true;
        };
        const auto rollback_member = [&find_backup, &backups](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() ) {
                return;
            }
            shared_ptr_fast<npc> member = overmap_buffer.remove_npc( snapshot.npc_id );
            if( !member ) {
                return;
            }
            member->spawn_at_precise( backup->position );
            member->goal = backup->goal;
            member->omt_path = backup->omt_path;
            member->mission = backup->mission;
            member->previous_mission = backup->previous_mission;
            member->goto_to_this_pos = backup->ordered_position;
            if( backup->ai_guard_position ) {
                member->set_ai_guard_pos( *backup->ai_guard_position );
            } else {
                member->clear_ai_guard_pos();
            }
            member->path = backup->local_path;
            overmap_buffer.insert_npc( member );
        };
        const bandit_live_world::local_handoff_commit_result result =
            bandit_live_world::commit_local_pair_handoff(
                site, plan, bind_member, rollback_member );
        if( result != bandit_live_world::local_handoff_commit_result::applied ) {
            log_homeward_rejection( bind_failure_reason.empty() ?
                                    "handoff commit rejected" : bind_failure_reason );
            continue;
        }
        g->load_npcs();
        changed = true;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_handoff committed"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " generation=" << site.active_outing.generation
                                   << " epoch=" << site.active_outing.handoff_epoch
                                   << " phase=" << bandit_live_world::to_string(
                                       site.active_outing.phase )
                                   << " route_position=" << route_position.to_string()
                                   << " members=" << surviving_member_ids.size() << '\n';
    }
    return changed;
}

bandit_mark_generation::smoke_weather_band live_bandit_smoke_weather_band()
{
    const weather_manager &weather = get_weather_const();
    if( weather.weather_id.str() == "portal_storm" ) {
        return bandit_mark_generation::smoke_weather_band::portal_storm;
    }
    if( weather.weather_id->rains || weather.weather_id->precip >= precip_class::light ) {
        return bandit_mark_generation::smoke_weather_band::rain;
    }
    if( weather.weather_id->sight_penalty >= 2.0f ) {
        return bandit_mark_generation::smoke_weather_band::fog;
    }
    if( weather.windspeed >= 20 ) {
        return bandit_mark_generation::smoke_weather_band::windy;
    }
    return bandit_mark_generation::smoke_weather_band::clear;
}

bandit_mark_generation::light_time_band live_bandit_light_time_band()
{
    if( is_night( calendar::turn ) ) {
        return bandit_mark_generation::light_time_band::night;
    }
    if( is_dawn( calendar::turn ) || is_dusk( calendar::turn ) ) {
        return bandit_mark_generation::light_time_band::twilight;
    }
    return bandit_mark_generation::light_time_band::daylight;
}

bandit_mark_generation::light_weather_band live_bandit_light_weather_band()
{
    const weather_manager &weather = get_weather_const();
    if( weather.weather_id.str() == "portal_storm" ) {
        return bandit_mark_generation::light_weather_band::portal_storm;
    }
    if( weather.weather_id->rains || weather.weather_id->precip >= precip_class::light ) {
        return bandit_mark_generation::light_weather_band::rain;
    }
    if( weather.weather_id->sight_penalty >= 2.0f ) {
        return bandit_mark_generation::light_weather_band::fog;
    }
    return bandit_mark_generation::light_weather_band::clear;
}

std::string live_bandit_source_mark_id( const std::string &kind, const tripoint_abs_omt &omt )
{
    std::ostringstream out;
    out << "live_" << kind << '@' << omt.x() << ',' << omt.y() << ',' << omt.z();
    return out.str();
}

bool live_bandit_fire_source_is_elevated_roof_exposed( const map &here,
        const tripoint_bub_ms &p )
{
    if( p.z() <= 0 ) {
        return false;
    }
    const ter_id terrain = here.ter( p );
    return terrain == ter_t_flat_roof || terrain == ter_t_tile_flat_roof;
}

int live_bandit_light_intensity_from_luminance( const float luminance )
{
    if( luminance >= 25.0f ) {
        return 3;
    }
    if( luminance >= 8.0f ) {
        return 2;
    }
    return luminance > 0.0f ? 1 : 0;
}

int live_bandit_light_side_leakage_near( const map &here, const tripoint_bub_ms &p )
{
    if( here.is_outside( p ) ) {
        return 0;
    }

    int leakage = 0;
    for( int dx = -2; dx <= 2; ++dx ) {
        for( int dy = -2; dy <= 2; ++dy ) {
            if( dx == 0 && dy == 0 ) {
                continue;
            }
            const tripoint_bub_ms candidate( p.x() + dx, p.y() + dy, p.z() );
            if( !here.inbounds( candidate ) || !here.is_outside( candidate ) ) {
                continue;
            }
            const int distance = std::max( std::abs( dx ), std::abs( dy ) );
            leakage = std::max( leakage, distance <= 1 ? 2 : 1 );
        }
    }
    return leakage;
}

void live_bandit_note_light_source( live_bandit_local_source_reading &reading,
                                    const int intensity,
                                    const bandit_mark_generation::light_source_band source,
                                    const tripoint_abs_ms &source_pos )
{
    if( intensity <= 0 ) {
        return;
    }
    const bool source_is_searchlight =
        source == bandit_mark_generation::light_source_band::searchlight;
    const bool reading_is_searchlight =
        reading.light_source == bandit_mark_generation::light_source_band::searchlight;
    const bool source_class_wins = source_is_searchlight && !reading_is_searchlight;
    const bool same_class = source_is_searchlight == reading_is_searchlight;
    const tripoint_abs_ms current_pos = reading.light_source_pos.value_or( source_pos );
    const bool stable_position_wins = source_pos.z() < current_pos.z() ||
                                      ( source_pos.z() == current_pos.z() &&
                                        ( source_pos.y() < current_pos.y() ||
                                          ( source_pos.y() == current_pos.y() &&
                                            source_pos.x() < current_pos.x() ) ) );
    const bool representative_wins = source_class_wins ||
                                     ( same_class &&
                                       ( intensity > reading.representative_light_intensity ||
                                         ( intensity == reading.representative_light_intensity &&
                                           stable_position_wins ) ) );
    if( !reading.light_source_pos || representative_wins ) {
        reading.light_source = source;
        reading.representative_light_intensity = intensity;
        reading.light_source_pos = source_pos;
    }
    reading.light_intensity = std::max( reading.light_intensity, intensity );
}

std::vector<live_bandit_signal_observation> observe_live_bandit_field_signals_near_player()
{
    avatar &u = get_avatar();
    map &here = get_map();
    std::map<tripoint_abs_omt, live_bandit_local_source_reading> readings;

    for( const tripoint_bub_ms &p : here.points_in_radius( u.pos_bub(),
            live_bandit_local_source_scan_radius_ms ) ) {
        const int fire_intensity = here.get_field_intensity( p, fd_fire );
        const int smoke_intensity = here.get_field_intensity( p, fd_smoke );
        int light_intensity = 0;
        bandit_mark_generation::light_source_band light_source =
            bandit_mark_generation::light_source_band::ordinary;

        for( const std::pair<const field_type_id, field_entry> &field_entry : here.field_at( p ) ) {
            light_intensity = std::max( light_intensity,
                                        live_bandit_light_intensity_from_luminance(
                                            field_entry.second.get_intensity_level().light_emitted ) );
        }

        light_intensity = std::max( light_intensity,
                                    live_bandit_light_intensity_from_luminance( here.ter( p )->light_emitted ) );
        light_intensity = std::max( light_intensity,
                                    live_bandit_light_intensity_from_luminance( here.furn( p )->light_emitted ) );

        for( const item &it : here.i_at( p ) ) {
            float luminance = 0.0f;
            units::angle width = 0_degrees;
            units::angle direction = 0_degrees;
            if( it.getlight( luminance, width, direction ) ) {
                light_intensity = std::max( light_intensity,
                                            live_bandit_light_intensity_from_luminance( luminance ) );
                if( width > 0_degrees && luminance >= 8.0f ) {
                    light_source = bandit_mark_generation::light_source_band::searchlight;
                }
            }
        }

        if( fire_intensity <= 0 && smoke_intensity <= 0 && light_intensity <= 0 ) {
            continue;
        }
        const tripoint_abs_omt source_omt = coords::project_to<coords::omt>( here.get_abs( p ) );
        live_bandit_local_source_reading &reading = readings[source_omt];
        reading.fire_intensity = std::max( reading.fire_intensity, fire_intensity );
        reading.smoke_intensity = std::max( reading.smoke_intensity, smoke_intensity );
        live_bandit_note_light_source( reading, light_intensity, light_source, here.get_abs( p ) );
        reading.outside |= here.is_outside( p );
        reading.side_leakage = std::max( reading.side_leakage,
                                         live_bandit_light_side_leakage_near( here, p ) );
        reading.elevated_roof_exposed |= live_bandit_fire_source_is_elevated_roof_exposed( here, p );
    }

    for( wrapped_vehicle &wrapped_veh : here.get_vehicles() ) {
        vehicle *veh = wrapped_veh.v;
        if( veh == nullptr ) {
            continue;
        }
        for( vehicle_part *part : veh->lights() ) {
            if( part == nullptr ) {
                continue;
            }
            const tripoint_bub_ms p = veh->bub_part_pos( here, *part );
            if( !here.inbounds( p ) || rl_dist( u.pos_bub(), p ) > live_bandit_local_source_scan_radius_ms ) {
                continue;
            }
            const vpart_info &info = part->info();
            const bool directional = info.has_flag( VPFLAG_CONE_LIGHT ) ||
                                     info.has_flag( VPFLAG_WIDE_CONE_LIGHT );
            const int light_intensity = live_bandit_light_intensity_from_luminance( info.bonus );
            const tripoint_abs_omt source_omt = coords::project_to<coords::omt>( here.get_abs( p ) );
            live_bandit_local_source_reading &reading = readings[source_omt];
            live_bandit_note_light_source( reading, light_intensity,
                                           directional ? bandit_mark_generation::light_source_band::searchlight :
                                           bandit_mark_generation::light_source_band::ordinary,
                                           here.get_abs( p ) );
            reading.outside |= here.is_outside( p );
            reading.side_leakage = std::max( reading.side_leakage,
                                             live_bandit_light_side_leakage_near( here, p ) );
            reading.elevated_roof_exposed |= live_bandit_fire_source_is_elevated_roof_exposed( here, p );
        }
    }

    std::vector<live_bandit_signal_observation> observations;
    observations.reserve( readings.size() * 2 );
    const bandit_mark_generation::smoke_weather_band weather_band = live_bandit_smoke_weather_band();
    const bandit_mark_generation::light_time_band light_time = live_bandit_light_time_band();
    const bandit_mark_generation::light_weather_band light_weather = live_bandit_light_weather_band();
    const weather_manager &weather = get_weather_const();
    for( const std::pair<const tripoint_abs_omt, live_bandit_local_source_reading> &entry : readings ) {
        const tripoint_abs_omt &source_omt = entry.first;
        const live_bandit_local_source_reading &reading = entry.second;
        bandit_mark_generation::local_field_signal_reading adapter_reading;
        adapter_reading.smoke_id = live_bandit_source_mark_id( "smoke", source_omt );
        adapter_reading.light_id = live_bandit_source_mark_id( "light", source_omt );
        adapter_reading.envelope_id = "local_field@" + live_bandit_omt_token( source_omt );
        adapter_reading.region_id = live_bandit_omt_token( source_omt );
        adapter_reading.observed_range_omt = 0;
        adapter_reading.fire_intensity = reading.fire_intensity;
        adapter_reading.smoke_intensity = reading.smoke_intensity;
        adapter_reading.light_intensity = reading.light_intensity;
        adapter_reading.light_source = reading.light_source;
        adapter_reading.outside = reading.outside;
        adapter_reading.side_leakage = reading.side_leakage;
        adapter_reading.elevated_roof_exposed = reading.elevated_roof_exposed;
        adapter_reading.smoke_weather = weather_band;
        adapter_reading.light_time = light_time;
        adapter_reading.light_weather = light_weather;

        const bandit_mark_generation::local_field_signal_projection field_projection =
            bandit_mark_generation::adapt_local_field_signal_reading( adapter_reading );
        const bandit_mark_generation::smoke_projection &projection = field_projection.smoke;
        if( field_projection.has_smoke_packet && !projection.viable ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal rejected: packet="
                                       << projection.packet.id
                                       << " kind=smoke reason=below_threshold weather="
                                       << bandit_mark_generation::to_string( weather_band )
                                       << " observed_range_omt=" << projection.packet.observed_range_omt
                                       << " projected_range_omt=" << projection.projected_range_omt
                                       << " visibility_score=" << projection.visibility_score << '\n';
        } else if( field_projection.has_smoke_packet ) {
            live_bandit_signal_observation observation;
            observation.signal = projection.signal;
            observation.source_omt = source_omt;
            observation.range_cap_omt = projection.projected_range_omt;
            observation.weather_summary = projection.weather_effect.summary;
            observation.mark.mark_id = projection.packet.id;
            observation.mark.kind = "smoke";
            observation.mark.source_omt = source_omt;
            observation.mark.observed_range_omt = projection.packet.observed_range_omt;
            observation.mark.range_cap_omt = projection.projected_range_omt;
            observation.mark.strength = projection.signal.strength;
            observation.mark.confidence = projection.signal.confidence;
            observation.mark.bounty_add = projection.signal.bounty_add;
            observation.mark.threat_add = projection.signal.threat_add;
            observation.mark.notes = projection.signal.notes;
            observations.push_back( observation );
        }

        if( !field_projection.has_light_packet ) {
            continue;
        }

        const bandit_mark_generation::light_projection &light_projection = field_projection.light;
        if( !light_projection.viable ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal rejected: packet="
                                       << light_projection.packet.id
                                       << " kind=light reason=below_threshold time="
                                       << bandit_mark_generation::to_string( light_time )
                                       << " weather=" << bandit_mark_generation::to_string( light_weather )
                                       << " exposure="
                                       << bandit_mark_generation::to_string( light_projection.packet.exposure )
                                       << " observed_range_omt=" << light_projection.packet.observed_range_omt
                                       << " projected_range_omt=" << light_projection.projected_range_omt
                                       << " visibility_score=" << light_projection.visibility_score << '\n';
            continue;
        }

        live_bandit_signal_observation light_observation;
        light_observation.signal = light_projection.signal;
        light_observation.source_omt = source_omt;
        light_observation.source_ms = reading.light_source_pos;
        light_observation.range_cap_omt = light_projection.projected_range_omt;
        light_observation.weather_summary = light_projection.concealment.summary;
        light_observation.mark.mark_id = light_projection.packet.id;
        light_observation.mark.kind = light_projection.signal.kind;
        light_observation.mark.source_omt = source_omt;
        light_observation.mark.observed_range_omt = light_projection.packet.observed_range_omt;
        light_observation.mark.range_cap_omt = light_projection.projected_range_omt;
        light_observation.mark.strength = light_projection.signal.strength;
        light_observation.mark.confidence = light_projection.signal.confidence;
        light_observation.mark.bounty_add = light_projection.signal.bounty_add;
        light_observation.mark.threat_add = light_projection.signal.threat_add;
        light_observation.mark.notes = light_projection.signal.notes;
        light_observation.horde_signal_power =
            bandit_mark_generation::horde_signal_power_from_light_projection( light_projection );
        light_observation.has_light_projection = true;
        light_observation.light_projection = light_projection;
        observations.push_back( light_observation );
    }

    int smoke_packets = 0;
    int light_packets = 0;
    for( const live_bandit_signal_observation &observation : observations ) {
        if( observation.mark.kind == "light" || observation.mark.kind == "searchlight" ) {
            light_packets++;
        } else if( observation.mark.kind == "smoke" ) {
            smoke_packets++;
        }
    }

    if( observations.empty() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal scan: signal_packet=no kind=smoke/fire/light"
                                   << " scan_radius_ms=" << live_bandit_local_source_scan_radius_ms
                                   << " weather=" << bandit_mark_generation::to_string( weather_band )
                                   << " light_time=" << bandit_mark_generation::to_string( light_time )
                                   << " light_weather=" << bandit_mark_generation::to_string( light_weather )
                                   << " raw_weather=" << weather.weather_id.str()
                                   << " sight_penalty=" << weather.weather_id->sight_penalty
                                   << " windspeed=" << weather.windspeed
                                   << '\n';
    } else {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal scan: signal_packet=yes kind=smoke/fire/light"
                                   << " packets=" << observations.size()
                                   << " smoke_packets=" << smoke_packets
                                   << " light_packets=" << light_packets
                                   << " scan_radius_ms=" << live_bandit_local_source_scan_radius_ms
                                   << " weather=" << bandit_mark_generation::to_string( weather_band )
                                   << " light_time=" << bandit_mark_generation::to_string( light_time )
                                   << " light_weather=" << bandit_mark_generation::to_string( light_weather )
                                   << " raw_weather=" << weather.weather_id.str()
                                   << " sight_penalty=" << weather.weather_id->sight_penalty
                                   << " windspeed=" << weather.windspeed
                                   << '\n';
    }
    return observations;
}

int bootstrap_live_bandit_abstract_sites_near_player()
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const tripoint_abs_omt center = u.pos_abs_omt();

    const auto special_lookup = []( const tripoint_abs_omt &candidate ) -> std::optional<std::string> {
        if( const std::optional<overmap_special_id> special =
                overmap_buffer.overmap_special_at_existing( candidate ) ) {
            return special->str();
        }
        return std::nullopt;
    };

    const bandit_live_world::abstract_bootstrap_result result =
        bandit_live_world::register_abstract_sites_near( state, center,
                live_bandit_system_envelope_omt, special_lookup );

    if( result.created_sites > 0 ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world abstract_bootstrap created_sites="
                                   << result.created_sites << " recognized_tiles=" << result.recognized_tiles
                                   << " scan_radius_omt=" << live_bandit_system_envelope_omt
                                   << " total_sites=" << state.sites.size() << '\n';
    }
    return result.created_sites;
}

int signal_live_hordes_from_light_observations(
    const std::vector<live_bandit_signal_observation> &signals )
{
    int signaled_sources = 0;
    for( const live_bandit_signal_observation &signal : signals ) {
        if( signal.horde_signal_power <= 0 ) {
            continue;
        }
        const tripoint_abs_sm source_sm = coords::project_to<coords::sm>( signal.source_omt );
        overmap_buffer.signal_hordes( source_sm, signal.horde_signal_power );
        signaled_sources++;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world horde light signal: packet="
                                   << signal.mark.mark_id << " kind=" << signal.mark.kind
                                   << " source_omt=" << signal.source_omt.to_string()
                                   << " source_sm=" << source_sm.to_string()
                                   << " current_signal=yes"
                                   << " horde_signal_power=" << signal.horde_signal_power
                                   << " range_cap_omt=" << signal.range_cap_omt
                                   << " weather=" << signal.weather_summary << '\n';
    }
    return signaled_sources;
}

void advance_zombie_rider_light_memories()
{
    std::map<tripoint_abs_omt, zombie_rider_overmap_ai::rider_light_memory> &memories =
        overmap_buffer.global_state.zombie_rider_light_memory;
    time_point &last_turn = overmap_buffer.global_state.zombie_rider_light_memory_last_turn;
    if( last_turn == calendar::turn_zero && memories.empty() ) {
        last_turn = calendar::turn;
        return;
    }
    if( calendar::turn < last_turn ) {
        memories.clear();
        last_turn = calendar::turn;
        return;
    }

    const int elapsed_turns = to_turns<int>( calendar::turn - last_turn );
    if( elapsed_turns <= 0 ) {
        return;
    }

    for( auto iter = memories.begin(); iter != memories.end(); ) {
        zombie_rider_overmap_ai::advance_light_memory( iter->second, elapsed_turns );
        if( !iter->second.active() ) {
            iter = memories.erase( iter );
        } else {
            ++iter;
        }
    }
    last_turn = calendar::turn;
}

struct live_zombie_rider_pressure_summary {
    int commanded = 0;
    int combat_ready = 0;
    int investigate = 0;
    int circle_harass = 0;
    int direct_attack = 0;
    int withdraw = 0;
    int selected_wounded = 0;
};

std::string live_zombie_rider_aggregate_posture(
    const live_zombie_rider_pressure_summary &summary )
{
    if( summary.commanded == 0 ) {
        return "none";
    }
    if( summary.investigate == summary.commanded ) {
        return "investigate";
    }
    if( summary.circle_harass == summary.commanded ) {
        return "circle_harass";
    }
    if( summary.direct_attack == summary.commanded ) {
        return "direct_attack";
    }
    if( summary.withdraw == summary.commanded ) {
        return "withdraw";
    }
    return "mixed";
}

Creature *nearest_live_camp_defender( const tripoint_abs_ms &source, const monster &rider,
                                      int &defender_strength )
{
    Creature *nearest = nullptr;
    int nearest_distance = INT_MAX;
    defender_strength = 0;
    avatar &u = get_avatar();
    if( !u.is_dead_state() && u.posz() == source.z() && rl_dist( u.pos_abs(), source ) <= 30 ) {
        defender_strength++;
        nearest = &u;
        nearest_distance = rl_dist( rider.pos_abs(), u.pos_abs() );
    }
    for( npc &guy : g->all_npcs() ) {
        if( guy.is_dead() || !guy.is_player_ally() || guy.posz() != source.z() ||
            rl_dist( guy.pos_abs(), source ) > 30 ) {
            continue;
        }
        defender_strength++;
        const int distance = rl_dist( rider.pos_abs(), guy.pos_abs() );
        if( distance < nearest_distance ) {
            nearest = &guy;
            nearest_distance = distance;
        }
    }
    defender_strength = std::max( 1, defender_strength );
    return nearest;
}

bool live_camp_has_actionable_opening( map &here, const tripoint_abs_ms &source,
                                       monster &rider, const Creature *defender )
{
    if( defender == nullptr || defender->posz() != rider.posz() ||
        !rider.sees( here, *defender ) ) {
        return false;
    }
    const tripoint_bub_ms source_bub = here.get_bub( source );
    if( !here.inbounds( source_bub ) ) {
        return false;
    }

    int nearby_barriers = 0;
    for( const tripoint_bub_ms &candidate : here.points_in_radius( source_bub, 6 ) ) {
        if( here.inbounds( candidate ) && !here.passable( candidate ) ) {
            nearby_barriers++;
        }
    }
    if( nearby_barriers < 8 ) {
        return false;
    }

    return !here.route( rider, pathfinding_target::point( defender->pos_bub() ) ).empty();
}

live_zombie_rider_pressure_summary command_live_zombie_riders_to_light(
    const zombie_rider_overmap_ai::rider_convergence_result &convergence,
    const std::unordered_map<std::string, monster *> &live_riders_by_id,
    const tripoint_abs_ms &light_source, int memory_turns )
{
    live_zombie_rider_pressure_summary summary;
    if( !convergence.should_converge ) {
        return summary;
    }
    map &here = get_map();
    for( const std::string &rider_id : convergence.rider_ids ) {
        const auto rider_iter = live_riders_by_id.find( rider_id );
        if( rider_iter != live_riders_by_id.end() && rider_iter->second != nullptr &&
            rider_iter->second->hp_percentage() > 50 ) {
            summary.combat_ready++;
        }
    }
    for( size_t rider_index = 0; rider_index < convergence.rider_ids.size(); ++rider_index ) {
        const std::string &rider_id = convergence.rider_ids[rider_index];
        const auto rider_iter = live_riders_by_id.find( rider_id );
        if( rider_iter == live_riders_by_id.end() || rider_iter->second == nullptr ) {
            continue;
        }
        monster &rider = *rider_iter->second;
        int defender_strength = 0;
        Creature *defender = nearest_live_camp_defender( light_source, rider, defender_strength );
        const bool rider_wounded = rider.hp_percentage() <= 50;
        const bool actionable_opening = live_camp_has_actionable_opening(
                                            here, light_source, rider, defender );

        zombie_rider_overmap_ai::rider_camp_pressure_input pressure_input;
        pressure_input.light_memory_active = true;
        pressure_input.rider_count = summary.combat_ready;
        pressure_input.band_formed =
            summary.combat_ready >= zombie_rider_overmap_ai::rider_band_minimum_size;
        pressure_input.breach_or_opening = actionable_opening;
        pressure_input.defender_strength = defender_strength;
        pressure_input.rider_wounded = rider_wounded;
        const zombie_rider_overmap_ai::rider_camp_pressure_result pressure =
            zombie_rider_overmap_ai::choose_camp_pressure_posture( pressure_input );

        zombie_rider_overmap_ai::set_camp_pressure_intent( rider, pressure.posture,
                light_source, memory_turns, static_cast<int>( rider_index ) );
        rider.unset_dest();
        if( pressure.posture != zombie_rider_overmap_ai::rider_camp_pressure_posture::withdraw ) {
            rider.anger = std::max( rider.anger, 100 );
        }
        summary.commanded++;
        summary.selected_wounded += rider_wounded ? 1 : 0;
        switch( pressure.posture ) {
            case zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate:
                summary.investigate++;
                break;
            case zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass:
                summary.circle_harass++;
                break;
            case zombie_rider_overmap_ai::rider_camp_pressure_posture::direct_attack:
                summary.direct_attack++;
                break;
            case zombie_rider_overmap_ai::rider_camp_pressure_posture::withdraw:
                summary.withdraw++;
                break;
            case zombie_rider_overmap_ai::rider_camp_pressure_posture::none:
                break;
        }
        DebugLog( D_INFO, DC_ALL ) << "zombie_rider camp_pressure_apply: rider=" << rider_id
                                   << " posture=" << zombie_rider_overmap_ai::to_string( pressure.posture )
                                   << " reason=" << pressure.reason
                                   << " source=" << light_source.to_string_writable()
                                   << " rider_pos=" << rider.pos_abs().to_string_writable()
                                   << " slot=" << rider_index
                                   << " intent_turns=" << memory_turns
                                   << " combat_ready_riders=" << summary.combat_ready
                                   << " defender_strength=" << defender_strength
                                   << " opening=" << ( actionable_opening ? "yes" : "no" )
                                   << " rider_wounded=" << ( rider_wounded ? "yes" : "no" ) << '\n';
    }
    return summary;
}

int signal_live_zombie_riders_from_light_observations(
    const std::vector<live_bandit_signal_observation> &signals )
{
    advance_zombie_rider_light_memories();
    int signaled_sources = 0;
    const int world_age_days = std::max( 0, to_days<int>( calendar::turn -
                                         calendar::start_of_cataclysm ) );

    std::vector<zombie_rider_overmap_ai::rider_overmap_agent> riders;
    std::unordered_map<std::string, monster *> live_riders_by_id;
    int wounded_riders = 0;
    for( monster &critter : g->all_monsters() ) {
        if( critter.type->id != mon_zombie_rider || critter.is_dead() ) {
            continue;
        }
        zombie_rider_overmap_ai::rider_overmap_agent rider;
        rider.rider_id = "active@" + critter.pos_abs().to_string();
        rider.pos = critter.pos_abs_omt();
        rider.available = true;
        const std::optional<zombie_rider_overmap_ai::rider_camp_pressure_intent> existing_intent =
            zombie_rider_overmap_ai::get_camp_pressure_intent( critter );
        rider.already_in_band = existing_intent.has_value();
        rider.cooldown_turns = critter.has_effect( effect_run ) ? 1 : 0;
        riders.push_back( rider );
        live_riders_by_id.emplace( rider.rider_id, &critter );
        if( critter.hp_percentage() <= 50 ) {
            wounded_riders++;
        }
    }

    std::vector<bool> used_signal( signals.size(), false );
    for( size_t signal_index = 0; signal_index < signals.size(); ++signal_index ) {
        const live_bandit_signal_observation &signal = signals[signal_index];
        if( used_signal[signal_index] || !signal.has_light_projection || signal.horde_signal_power <= 0 ) {
            continue;
        }

        used_signal[signal_index] = true;
        int aggregate_sources = 1;
        int aggregate_horde_signal_power = signal.horde_signal_power;
        bandit_mark_generation::light_projection aggregate_projection = signal.light_projection;
        for( size_t peer_index = signal_index + 1; peer_index < signals.size(); ++peer_index ) {
            const live_bandit_signal_observation &peer = signals[peer_index];
            if( used_signal[peer_index] || !peer.has_light_projection || peer.horde_signal_power <= 0 ) {
                continue;
            }
            const int distance = std::max( std::abs( peer.source_omt.x() - signal.source_omt.x() ),
                                           std::abs( peer.source_omt.y() - signal.source_omt.y() ) );
            if( peer.source_omt.z() != signal.source_omt.z() || distance > 1 ) {
                continue;
            }
            used_signal[peer_index] = true;
            aggregate_sources++;
            aggregate_horde_signal_power += peer.horde_signal_power;
        }

        if( aggregate_sources > 1 ) {
            const int nearby_light_bonus = std::min( aggregate_sources - 1, 4 ) * 2;
            aggregate_projection.packet.id += "_cluster";
            aggregate_projection.visibility_score = std::clamp(
                    aggregate_projection.visibility_score + nearby_light_bonus, 0, 60 );
            aggregate_projection.projected_range_omt = std::clamp(
                    aggregate_projection.projected_range_omt + nearby_light_bonus, 0, 30 );
            aggregate_projection.review_summary += "; nearby camp-light cluster sources=" +
                    std::to_string( aggregate_sources );
            aggregate_projection.signal.notes.push_back( "nearby camp-light cluster sources=" +
                    std::to_string( aggregate_sources ) + ", combined_horde_signal_power=" +
                    std::to_string( aggregate_horde_signal_power ) );
        }

        const int rider_horde_signal_power =
            bandit_mark_generation::horde_signal_power_from_light_projection( aggregate_projection );
        const zombie_rider_overmap_ai::rider_light_interest interest =
            zombie_rider_overmap_ai::evaluate_light_attraction( aggregate_projection, world_age_days,
                    static_cast<int>( riders.size() ) );
        zombie_rider_overmap_ai::rider_light_memory &memory =
            overmap_buffer.global_state.zombie_rider_light_memory[signal.source_omt];
        zombie_rider_overmap_ai::refresh_light_memory( memory, interest );
        const zombie_rider_overmap_ai::rider_convergence_result convergence =
            zombie_rider_overmap_ai::evaluate_rider_convergence( memory, signal.source_omt, riders );
        const tripoint_abs_ms light_source = signal.source_ms.value_or(
                                                coords::project_to<coords::ms>( signal.source_omt ) );
        const live_zombie_rider_pressure_summary pressure = command_live_zombie_riders_to_light(
                    convergence, live_riders_by_id, light_source, memory.turns_remaining );
        zombie_rider_overmap_ai::reserve_rider_convergence( riders, convergence );

        DebugLog( D_INFO, DC_ALL ) << "zombie_rider camp_light: signal=yes source_omt="
                                   << signal.source_omt.to_string()
                                   << " world_age_days=" << world_age_days
                                   << " horde_signal_power=" << rider_horde_signal_power
                                   << " aggregate_sources=" << aggregate_sources
                                   << " aggregate_horde_signal_power=" << aggregate_horde_signal_power
                                   << " interest=" << ( interest.should_investigate ? "yes" : "no" )
                                   << " interest_reason=" << interest.reason
                                   << " interest_score=" << interest.interest_score
                                   << " memory_active=" << ( memory.active() ? "yes" : "no" )
                                   << " memory_turns=" << memory.turns_remaining
                                   << " riders_observed=" << riders.size()
                                   << " selected_riders=" << convergence.selected_riders
                                   << " live_riders_commanded=" << pressure.commanded
                                   << " combat_ready_riders=" << pressure.combat_ready
                                   << " cap=" << convergence.cap
                                   << " band_formed=" << ( convergence.band_formed ? "yes" : "no" )
                                   << " band_size=" << convergence.band_size
                                   << " convergence_reason=" << convergence.reason
                                   << " posture=" << live_zombie_rider_aggregate_posture( pressure )
                                   << " applied_investigate=" << pressure.investigate
                                   << " applied_circle_harass=" << pressure.circle_harass
                                   << " applied_direct_attack=" << pressure.direct_attack
                                   << " applied_withdraw=" << pressure.withdraw
                                   << " selected_wounded=" << pressure.selected_wounded
                                   << " wounded_riders_observed=" << wounded_riders << '\n';
        if( !memory.active() ) {
            overmap_buffer.global_state.zombie_rider_light_memory.erase( signal.source_omt );
        }
        signaled_sources++;
    }
    return signaled_sources;
}

std::optional<std::string> live_bandit_structural_terrain_id( const tripoint_abs_omt &omt )
{
    return overmap_buffer.ter( omt ).id().str();
}

bandit_live_world::structural_threat_read live_bandit_structural_threat_read(
    const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead &lead )
{
    bandit_live_world::structural_threat_read threat;
    const std::string terrain_id = live_bandit_structural_terrain_id( lead.omt ).value_or( std::string() );
    const bandit_live_world::structural_bounty_read read =
        bandit_live_world::classify_structural_bounty_terrain( terrain_id );
    threat.threat = read.latent_threat;
    threat.observed = true;
    threat.summary = "live structural terrain threat read: " + terrain_id;
    return threat;
}

bool live_bandit_overmap_los_from( const tripoint_abs_omt &origin,
                                   const tripoint_abs_omt &target, const int sight_points,
                                   const int retained_age_minutes = -1 )
{
    const int available_sight_points = retained_age_minutes >= 0 &&
                                       sight_points < std::numeric_limits<int>::max() ?
                                       sight_points + 1 : sight_points;
    const point_rel_omt offset = target.xy() - origin.xy();
    if( target.z() != origin.z() || available_sight_points < 0 ||
        offset.x() < -available_sight_points || offset.x() > available_sight_points ||
        offset.y() < -available_sight_points || offset.y() > available_sight_points ) {
        return false;
    }
    std::vector<int> terrain_see_costs;
    for( const tripoint_abs_omt &omt : line_to( origin, target ) ) {
        terrain_see_costs.push_back(
            static_cast<int>( overmap_buffer.ter( omt )->get_see_cost() ) );
    }
    return retained_age_minutes >= 0 ?
           bandit_live_world::structural_observer_route_is_retained(
               sight_points, terrain_see_costs, retained_age_minutes ) :
           bandit_live_world::structural_observer_route_is_visible(
               sight_points, terrain_see_costs );
}

weather_type_id live_bandit_remote_weather_at( const tripoint_abs_omt &origin )
{
    const tripoint_abs_ms origin_ms = project_to<coords::ms>( origin );
    const weather_generator_id &weather_generator = overmap_buffer.get_settings( origin ).weather;
    return weather_generator->get_weather_conditions( origin_ms, calendar::turn, g->get_seed() );
}

struct live_bandit_structural_visibility_details {
    std::string weather_id = "none";
    float remote_light = -1.0f;
    int ordinary_sight_range_ms = -1;
    float weather_sight_penalty = -1.0f;
    int elevation_omt = 0;
    bool has_optic = false;
    int sight_points = -1;
};

live_bandit_structural_visibility_details live_bandit_structural_observer_sight(
    const npc &observer, const tripoint_abs_omt &origin )
{
    const weather_type_id weather = live_bandit_remote_weather_at( origin );
    const float remote_light = origin.z() < 0 ? LIGHT_AMBIENT_MINIMAL :
                               std::max<float>( LIGHT_AMBIENT_MINIMAL,
                                       sun_moon_light_at( calendar::turn ) *
                                       weather->light_multiplier + weather->light_modifier );
    static const json_character_flag enhanced_vision( "ENHANCED_VISION" );
    const bool has_optic = observer.cache_has_item_with( flag_ZOOM ) ||
                           observer.has_flag( enhanced_vision ) ||
                           observer.cache_has_item_with( "is_gun", &item::is_gun,
    []( const item & gun ) {
        return std::any_of( gun.gunmods().begin(), gun.gunmods().end(),
        []( const item * mod ) {
            return mod != nullptr && mod->has_flag( flag_ZOOM );
        } );
    } );
    live_bandit_structural_visibility_details details;
    details.weather_id = weather.str();
    details.remote_light = remote_light;
    details.ordinary_sight_range_ms = observer.sight_range( remote_light, remote_light );
    details.weather_sight_penalty = std::max( 1.0f, weather->sight_penalty );
    details.elevation_omt = origin.z();
    details.has_optic = has_optic;
    bandit_live_world::structural_observer_visibility_read visibility;
    visibility.ordinary_sight_range_ms = details.ordinary_sight_range_ms;
    visibility.weather_sight_penalty = details.weather_sight_penalty;
    visibility.elevation_omt = details.elevation_omt;
    visibility.has_optic = details.has_optic;
    details.sight_points = bandit_live_world::structural_observer_omt_sight_range( visibility );
    return details;
}

struct live_bandit_omt_threat_read {
    int visible_count = 0;
    int danger_low = 0;
    int danger_high = 0;
    std::vector<std::string> stable_ids;
};

std::vector<std::string> live_bandit_bounded_threat_ids( std::vector<std::string> ids )
{
    static constexpr std::size_t id_cap = 16;
    std::sort( ids.begin(), ids.end() );
    ids.erase( std::unique( ids.begin(), ids.end() ), ids.end() );
    if( ids.size() <= id_cap ) {
        return ids;
    }
    unsigned long long hash = 1469598103934665603ULL;
    for( const std::string &id : ids ) {
        for( const unsigned char byte : id ) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xffU;
        hash *= 1099511628211ULL;
    }
    ids.resize( id_cap - 1 );
    ids.push_back( "overflow:" + std::to_string( hash ) );
    std::sort( ids.begin(), ids.end() );
    return ids;
}

live_bandit_omt_threat_read live_bandit_threats_at_existing_omt(
    const npc &observer, const tripoint_abs_omt &omt )
{
    static constexpr std::size_t max_concrete_groups = 16;
    static constexpr std::size_t max_concrete_monsters = 64;
    live_bandit_omt_threat_read read;
    point_abs_om overmap_position;
    tripoint_om_omt local_omt;
    std::tie( overmap_position, local_omt ) = project_remain<coords::om>( omt );
    overmap *existing = overmap_buffer.get_existing( overmap_position );
    if( existing == nullptr ) {
        return read;
    }

    const auto add_danger = [&read]( const int low, const int high ) {
        read.danger_low = std::min( 200, read.danger_low + std::clamp( low, 0, 200 ) );
        read.danger_high = std::min( 200, read.danger_high + std::clamp( high, 0, 200 ) );
    };
    const tripoint_om_ms omt_origin = project_to<coords::ms>( local_omt );
    for( int y = 0; y < 2 * SEEY; ++y ) {
        for( int x = 0; x < 2 * SEEX; ++x ) {
            const tripoint_om_ms entity_position = omt_origin + point_rel_ms( x, y );
            const horde_entity *entity_ptr = existing->entity_at( entity_position );
            if( entity_ptr == nullptr ) {
                continue;
            }
            const horde_entity &entity = *entity_ptr;
            const mtype *type = entity.get_type();
            if( type == nullptr ) {
                continue;
            }
            bool hostile = entity.monster_data &&
                           entity.monster_data->attitude_to( observer ) ==
                           Creature::Attitude::HOSTILE;
            if( !entity.monster_data ) {
                const faction *observer_faction = observer.get_faction();
                hostile = type->aggro_character && type->agro >= 10 &&
                          ( observer_faction == nullptr ||
                            type->default_faction != observer_faction->mon_faction );
            }
            if( !hostile ) {
                continue;
            }
            const int danger = entity.monster_data ?
                               static_cast<int>( std::ceil( observer.evaluate_monster(
                                       *entity.monster_data, 1 ) ) ) :
                               static_cast<int>( std::ceil( std::min<float>(
                                       std::max<float>( type->get_total_difficulty(),
                                               NPC_DANGER_VERY_LOW ),
                                       NPC_MONSTER_DANGER_MAX ) ) );
            add_danger( danger, danger );
            read.visible_count++;
            read.stable_ids.push_back( "entity:" +
                                       project_combine( overmap_position,
                                               entity_position ).to_string() + ':' +
                                       type->id.str() );
        }
    }

    std::size_t concrete_monsters_inspected = 0;
    for( const mongroup *group : overmap_buffer.monsters_at( omt, max_concrete_groups ) ) {
        if( group == nullptr || group->is_safe() ) {
            continue;
        }
        if( group->monsters.empty() && group->population == 0 ) {
            continue;
        }
        const std::string group_id = "group:" + group->abs_pos.to_string() + ':' +
                                     group->type.str() + ':' +
                                     std::to_string( group->target.x() ) + ',' +
                                     std::to_string( group->target.y() ) + ':' +
                                     ( group->horde ? "horde" : "spawn" );
        if( !group->monsters.empty() ) {
            int hostile_monsters = 0;
            for( const monster &monster : group->monsters ) {
                if( concrete_monsters_inspected >= max_concrete_monsters ) {
                    break;
                }
                concrete_monsters_inspected++;
                if( monster.attitude_to( observer ) != Creature::Attitude::HOSTILE ) {
                    continue;
                }
                const int danger = static_cast<int>( std::ceil(
                                       observer.evaluate_monster( monster, 1 ) ) );
                add_danger( danger, danger );
                hostile_monsters++;
            }
            if( hostile_monsters == 0 ) {
                continue;
            }
            read.visible_count += hostile_monsters;
        } else {
            continue;
        }
        read.stable_ids.push_back( group_id );
        if( concrete_monsters_inspected >= max_concrete_monsters ) {
            break;
        }
    }
    read.stable_ids = live_bandit_bounded_threat_ids( std::move( read.stable_ids ) );
    return read;
}

std::vector<bandit_live_world::abstract_threat_detour_read>
live_bandit_structural_detour_reads( const bandit_live_world::site_record &site,
                                    const tripoint_abs_omt &current_omt,
                                    const tripoint_abs_omt &threat_omt )
{
    std::vector<tripoint_abs_omt> candidates;
    for( int dx = -1; dx <= 1; ++dx ) {
        for( int dy = -1; dy <= 1; ++dy ) {
            if( dx == 0 && dy == 0 ) {
                continue;
            }
            candidates.emplace_back( current_omt.x() + dx, current_omt.y() + dy,
                                     current_omt.z() );
        }
    }
    std::sort( candidates.begin(), candidates.end(), [&site, &threat_omt](
    const tripoint_abs_omt & lhs, const tripoint_abs_omt & rhs ) {
        return std::make_tuple( -rl_dist( lhs, threat_omt ), rl_dist( lhs, site.anchor ), lhs ) <
               std::make_tuple( -rl_dist( rhs, threat_omt ), rl_dist( rhs, site.anchor ), rhs );
    } );
    std::vector<bandit_live_world::abstract_threat_detour_read> reads;
    const overmap_path_params npc_path = overmap_path_params::for_npc();
    for( const tripoint_abs_omt &candidate : candidates ) {
        if( candidate == threat_omt || reads.size() >= 2 ) {
            continue;
        }
        bandit_live_world::abstract_threat_detour_read read;
        read.omt = candidate;
        const oter_id &terrain = overmap_buffer.ter_existing( candidate );
        read.passable = terrain.is_valid() &&
                        npc_path.get_cost( terrain->get_travel_cost_type() ) >= 0 &&
                        rl_dist( candidate, site.anchor ) < rl_dist( current_omt, site.anchor );
        reads.push_back( read );
    }
    return reads;
}

bandit_live_world::abstract_threat_read live_bandit_structural_abstract_threat_read(
    const bandit_live_world::site_record &site,
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request )
{
    bandit_live_world::abstract_threat_read result;
    live_bandit_structural_visibility_details visibility;
    bool first_forward_acquired = false;
    bool first_forward_checked = false;
    const auto bounded_debug_token = []( const std::string & value ) {
        static constexpr std::size_t token_cap = 120;
        std::string token;
        token.reserve( std::min( value.size(), token_cap ) );
        for( const unsigned char ch : value ) {
            if( token.size() >= token_cap ) {
                break;
            }
            token.push_back( ch >= 33U && ch <= 126U && ch != '=' ?
                             static_cast<char>( ch ) : '_' );
        }
        return token.empty() ? std::string( "none" ) : token;
    };
    const auto log_visibility = [&]() {
        const bool has_first_forward = !request.visible_forward_omts.empty();
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world structural_visibility:"
                                   << " site=" << bounded_debug_token( site.site_id )
                                   << " activity=" << bounded_debug_token( outing.activity_id )
                                   << " observer=" << outing.leader_id.get_value()
                                   << " current_omt=" << request.current_omt.to_string()
                                   << " remote_light=" << visibility.remote_light
                                   << " weather=" << bounded_debug_token( visibility.weather_id )
                                   << " sight_penalty=" << visibility.weather_sight_penalty
                                   << " npc_sight_ms=" << visibility.ordinary_sight_range_ms
                                   << " elevation_omt=" << visibility.elevation_omt
                                   << " optic=" << ( visibility.has_optic ? "yes" : "no" )
                                   << " sight_points=" << visibility.sight_points
                                   << " forward_candidates=" << request.visible_forward_omts.size()
                                   << " first_forward_omt=" << ( has_first_forward ?
                                           request.visible_forward_omts.front().to_string() : "none" )
                                   << " first_forward_distance=" << ( has_first_forward ?
                                           rl_dist( request.current_omt,
                                                    request.visible_forward_omts.front() ) : -1 )
                                   << " first_forward_acquired=" << ( first_forward_checked ?
                                           ( first_forward_acquired ? "yes" : "no" ) : "not_checked" )
                                   << " outcome=" << ( result.observed ? "observed" :
                                           "no_visible_threat" )
                                   << " threat_omt=" << ( result.observed ?
                                           result.threat_omt.to_string() : "none" ) << '\n';
    };
    if( outing.kind != bandit_live_world::outing_kind::structural_sortie ||
        request.party_power <= 0 || request.visible_forward_omts.size() > 3 ) {
        log_visibility();
        return result;
    }
    const shared_ptr_fast<npc> observer = overmap_buffer.find_npc( outing.leader_id );
    if( !observer || observer->is_dead() ) {
        log_visibility();
        return result;
    }
    result.local_reality = get_map().inbounds( request.current_omt );
    visibility = live_bandit_structural_observer_sight( *observer, request.current_omt );
    const int sight_points = visibility.sight_points;
    std::vector<tripoint_abs_omt> permitted_omts;
    permitted_omts.push_back( request.current_omt );
    permitted_omts.insert( permitted_omts.end(), request.visible_forward_omts.begin(),
                           request.visible_forward_omts.end() );
    for( std::size_t index = 0; index < permitted_omts.size(); ++index ) {
        const tripoint_abs_omt &omt = permitted_omts[index];
        const bool overlap = index == 0;
        const bool acquired = overlap || live_bandit_overmap_los_from(
                                  request.current_omt, omt, sight_points );
        if( index == 1 ) {
            first_forward_acquired = acquired;
            first_forward_checked = true;
        }
        const bool retained = !acquired && request.retained_threat_omt &&
                              omt == *request.retained_threat_omt &&
                              live_bandit_overmap_los_from(
                                  request.current_omt, omt, sight_points,
                                  request.retained_threat_age_minutes );
        if( !acquired && !retained ) {
            continue;
        }
        const live_bandit_omt_threat_read threat =
            live_bandit_threats_at_existing_omt( *observer, omt );
        if( threat.stable_ids.empty() || threat.danger_high <= 0 ||
            ( !overlap && threat.visible_count < HORDE_VISIBILITY_SIZE ) ||
            ( retained && !bandit_live_world::structural_observer_retained_threat_matches(
                  request, omt, threat.stable_ids ) ) ) {
            continue;
        }
        result.observed = true;
        result.overlap = overlap;
        result.threat_omt = omt;
        result.danger_low = threat.danger_low;
        result.danger_high = threat.danger_high;
        result.visual_quality = std::clamp( sight_points, 1, 3 );
        result.uncertainty_radius_omt = overlap ? 0 : 1;
        result.equipment_detail = sight_points >= 3 ? 1 : 0;
        result.stable_threat_ids = threat.stable_ids;
        result.summary = "ordinary OMT observer read live threat at " + omt.to_string();
        const bool hard_danger = std::min( 1000, 5 * result.danger_high ) >= 750 ||
                                 result.danger_low >= std::min( 200, 2 * request.party_power );
        if( hard_danger ) {
            result.detours = live_bandit_structural_detour_reads(
                                 site, request.current_omt, omt );
        }
        log_visibility();
        return result;
    }
    log_visibility();
    return result;
}

bandit_live_world::structural_route_read live_bandit_structural_route_read(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan,
    int &watch_path_budget )
{
    bandit_live_world::structural_route_read read;
    const overmap_path_params npc_route = overmap_path_params::for_npc();
    const auto path = overmap_buffer.get_travel_path(
                site.anchor, plan.target_omt, npc_route );
    if( path.points.empty() || path.cost < 0 ) {
        read.summary = "live structural route solve found no passable route";
        return read;
    }

    const int source_node_cost = npc_route.get_cost(
                                     overmap_buffer.ter_existing( site.anchor )->get_travel_cost_type() );
    const bool diagonal_departure = path.points.size() >= 2 &&
                                    path.points[path.points.size() - 2].x() != site.anchor.x() &&
                                    path.points[path.points.size() - 2].y() != site.anchor.y();
    read.complete_route_cost =
        bandit_live_world::normalize_structural_live_round_trip_cost_omt(
            path.cost, source_node_cost, diagonal_departure );
    if( read.complete_route_cost < 0 ) {
        read.summary = "live structural route solve produced invalid boundary cost";
        return read;
    }
    read.max_segment_risk = 0;
    for( const tripoint_abs_omt &omt : path.points ) {
        if( omt == plan.target_omt ) {
            continue;
        }
        const std::string terrain_id = live_bandit_structural_terrain_id( omt ).value_or(
                                           std::string() );
        const bandit_live_world::structural_bounty_read terrain =
            bandit_live_world::classify_structural_bounty_terrain( terrain_id );
        read.max_segment_risk = std::max(
            read.max_segment_risk,
            bandit_live_world::structural_terrain_static_risk( terrain.terrain_fit_class ) );
    }
    read.reachable = read.complete_route_cost <= 18;
    read.summary = string_format(
                       "live structural route solve %s raw_cost=%d source_cost=%d diagonal=%s round_trip_omt=%d",
                       read.reachable ? "accepted" : "exceeded complete-route cap", path.cost,
                       source_node_cost, diagonal_departure ? "yes" : "no",
                       read.complete_route_cost );
    if( !read.reachable ) {
        return read;
    }
    const bandit_live_world::camp_map_lead *lead =
        site.intelligence_map.find_lead( plan.lead_id );
    if( ( plan.frontier_sector < 0 && lead == nullptr ) ||
        ( lead != nullptr &&
          lead->kind == bandit_live_world::camp_lead_kind::structural_bounty ) ) {
        return read;
    }

    std::vector<tripoint_abs_omt> target_footprint = { plan.target_omt };
    if( const std::optional<basecamp *> camp = overmap_buffer.find_camp(
                plan.target_omt.xy() ); camp && *camp != nullptr &&
        ( ( *camp )->camp_omt_pos() == plan.target_omt ||
          ( *camp )->point_within_camp( plan.target_omt ) ) ) {
        target_footprint = { ( *camp )->camp_omt_pos() };
        for( const point_rel_omt &direction : ( *camp )->directions ) {
            const tripoint_abs_omt expansion_omt = ( *camp )->camp_omt_pos() + direction;
            if( ( *camp )->point_within_camp( expansion_omt ) ) {
                target_footprint.push_back( expansion_omt );
            }
        }
        if( std::find( target_footprint.begin(), target_footprint.end(),
                       plan.target_omt ) == target_footprint.end() ) {
            target_footprint.push_back( plan.target_omt );
        }
    }

    const std::unordered_set<tripoint_abs_omt> target_footprint_exclusions(
        target_footprint.begin(), target_footprint.end() );
    std::vector<std::pair<tripoint_abs_omt, std::vector<tripoint_abs_omt>>> watch_paths;
    const auto terrain_lookup = [&npc_route]( const tripoint_abs_omt &candidate,
    const std::vector<tripoint_abs_omt> &footprint ) {
        bandit_live_world::structural_watch_terrain_read terrain;
        terrain.concealed = overmap_buffer.ter( candidate )->get_see_cost() > 0;
        const auto nearest = std::min_element( footprint.begin(), footprint.end(),
        [&candidate]( const tripoint_abs_omt & lhs, const tripoint_abs_omt & rhs ) {
            const int lhs_distance = std::max( std::abs( candidate.x() - lhs.x() ),
                                               std::abs( candidate.y() - lhs.y() ) );
            const int rhs_distance = std::max( std::abs( candidate.x() - rhs.x() ),
                                               std::abs( candidate.y() - rhs.y() ) );
            return std::make_tuple( lhs_distance, lhs.z(), lhs.y(), lhs.x() ) <
                   std::make_tuple( rhs_distance, rhs.z(), rhs.y(), rhs.x() );
        } );
        if( nearest == footprint.end() || nearest->z() != candidate.z() ) {
            return terrain;
        }
        const std::vector<tripoint_abs_omt> approach = line_to( candidate, *nearest );
        if( approach.size() < 3 ) {
            return terrain;
        }
        terrain.intervening_omts_clear = true;
        for( auto omt = approach.begin(); omt != std::prev( approach.end() ); ++omt ) {
            const oter_id &omt_terrain = overmap_buffer.ter( *omt );
            if( std::find( footprint.begin(), footprint.end(), *omt ) != footprint.end() ||
                npc_route.get_cost( omt_terrain->get_travel_cost_type() ) < 0 ) {
                terrain.intervening_omts_clear = false;
                break;
            }
            terrain.intervening_see_costs.push_back(
                static_cast<int>( omt_terrain->get_see_cost() ) );
        }
        return terrain;
    };
    const auto watch_route_lookup = [&site, &npc_route, &source_node_cost, &target_footprint,
                                    &target_footprint_exclusions,
                                    &watch_path_budget,
                                    &watch_paths]( const tripoint_abs_omt &candidate ) {
        bandit_live_world::structural_watch_route_read route;
        if( watch_path_budget <= 0 ) {
            return route;
        }
        watch_path_budget--;
        const auto watch_path = overmap_buffer.get_travel_path(
                                    site.anchor, candidate, npc_route,
                                    target_footprint_exclusions );
        if( watch_path.cost < 0 ||
            !bandit_live_world::structural_watch_route_avoids_target_footprint(
                watch_path.points, target_footprint ) ) {
            return route;
        }
        const bool diagonal_departure = watch_path.points.size() >= 2 &&
                                        watch_path.points[watch_path.points.size() - 2].x() !=
                                        site.anchor.x() &&
                                        watch_path.points[watch_path.points.size() - 2].y() !=
                                        site.anchor.y();
        route.route_cost = bandit_live_world::normalize_structural_live_round_trip_cost_omt(
                               watch_path.cost, source_node_cost, diagonal_departure );
        route.reachable = route.route_cost >= 0 && route.route_cost <= 18;
        if( !route.reachable ||
            bandit_live_world::make_structural_watch_shared_route(
                site.anchor, candidate, watch_path.points, target_footprint ).empty() ) {
            route.reachable = false;
            return route;
        }
        watch_paths.emplace_back( candidate, watch_path.points );
        return route;
    };
    const bandit_live_world::structural_watch_geography_read watch =
        bandit_live_world::read_structural_watch_geography(
            target_footprint, site.anchor, terrain_lookup, watch_route_lookup );
    read.watch_geography_supplied = true;
    read.target_footprint = watch.target_footprint;
    read.watch_candidates = watch.routed_candidates;
    const int reachable_watch_candidates = static_cast<int>( std::count_if(
            watch.routed_candidates.begin(), watch.routed_candidates.end(),
    []( const bandit_live_world::watch_selection_candidate & candidate ) {
        return candidate.reachable;
    } ) );
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world watch_geography_preflight"
                               << " site=" << site.site_id
                               << " target=" << plan.target_omt
                               << " footprint=" << watch.target_footprint.size()
                               << " candidates=" << watch.candidate_omts_considered
                               << " concealed=" << watch.concealed_candidates
                               << " clear=" << watch.clear_intervening_candidates
                               << " visible=" << watch.visible_intervening_candidates
                               << " qualified=" << watch.qualified_candidates
                               << " nonadjacent=" << watch.nonadjacent_qualified_candidates
                               << " route_reads=" << watch.route_reads
                               << " route_reachable=" << reachable_watch_candidates
                               << " selected_omt=" << ( watch.selection.valid ?
                                       watch.selection.omt.to_string() : "none" )
                               << " selected_route_cost=" << watch.selection.route_cost
                               << " outcome=" << ( watch.selection.valid ? "selected" :
                                       "no_bounded_safe_watch_geography" ) << '\n';
    if( !watch.selection.valid ) {
        read.reachable = false;
        read.summary = "live structural route abandoned: no bounded safe watch geography";
    } else {
        const auto selected_path = std::find_if( watch_paths.begin(), watch_paths.end(),
        [&watch]( const auto & entry ) {
            return entry.first == watch.selection.omt;
        } );
        if( selected_path == watch_paths.end() ) {
            read.reachable = false;
            read.summary = "live structural route abandoned: selected watch path was not retained";
            return read;
        }
        read.watch_shared_route = bandit_live_world::make_structural_watch_shared_route(
                                      site.anchor, watch.selection.omt, selected_path->second,
                                      watch.target_footprint );
        if( read.watch_shared_route.empty() ) {
            read.reachable = false;
            read.summary = "live structural route abandoned: watch shared route was malformed";
            return read;
        }
        const bandit_live_world::watch_selection_result alternate_selection =
            bandit_live_world::select_alternate_watch_ring_candidate(
                watch.target_footprint, watch.routed_candidates,
                watch.selection.omt );
        if( alternate_selection.valid ) {
            const auto alternate_path = std::find_if(
                                            watch_paths.begin(), watch_paths.end(),
            [&alternate_selection]( const auto & entry ) {
                return entry.first == alternate_selection.omt;
            } );
            if( alternate_path != watch_paths.end() ) {
                read.alternate_watch_shared_route =
                    bandit_live_world::make_structural_watch_shared_route(
                        site.anchor, alternate_selection.omt,
                        alternate_path->second, watch.target_footprint );
            }
        }
        read.complete_route_cost = watch.selection.route_cost;
        read.summary += "; watch geography selected " +
                        watch.selection.omt.to_string() + " route_reads=" +
                        std::to_string( watch.route_reads );
    }
    return read;
}

std::string live_bandit_structural_route_analyzer_record(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan,
    const std::string &selector,
    const bandit_live_world::structural_route_read &read )
{
    const auto selected_watch = std::find_if( read.watch_candidates.begin(),
    read.watch_candidates.end(), [&read](
    const bandit_live_world::watch_selection_candidate &candidate ) {
        return read.watch_shared_route.size() > 2 &&
               read.watch_shared_route[2] == candidate.omt;
    } );
    const bool selected = read.reachable && selected_watch != read.watch_candidates.end() &&
                          !read.watch_shared_route.empty();
    std::string selected_fields;
    if( selected ) {
        selected_fields = " watch=" + selected_watch->omt.to_string() +
                          " route_cost=" + std::to_string( selected_watch->route_cost );
    }
    return "bandit_live_world structural_route_analyzer site=" + site.site_id +
           " lead=" + plan.lead_id + " target=" + plan.target_omt.to_string() +
           " selector=" + selector + " outcome=" + ( selected ? "selected" : "rejected" ) +
           selected_fields + " summary=" + read.summary;
}

std::vector<bandit_live_world::structural_signal_read> live_bandit_structural_signal_reads(
    const std::vector<live_bandit_signal_observation> &signals,
    const std::vector<live_bandit_sound_observation> &sound_events,
    const bandit_live_world::site_record &,
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request )
{
    std::vector<bandit_live_world::structural_signal_read> result;
    const bool request_current_inbounds = get_map().inbounds( request.current_omt );
    if( !signals.empty() || !sound_events.empty() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal_adapter request"
                                   << " current_omt=" << request.current_omt
                                   << " map_origin_omt="
                                   << coords::project_to<coords::omt>( get_map().get_abs_sub() )
                                   << " current_inbounds=" << ( request_current_inbounds ? "yes" : "no" )
                                   << " field_signals=" << signals.size()
                                   << " sound_events=" << sound_events.size() << '\n';
    }
    if( outing.kind != bandit_live_world::outing_kind::structural_sortie ||
        outing.owner != bandit_live_world::simulation_owner::abstract ||
        request.party_power <= 0 || request.visible_forward_omts.size() > 3 ||
        request_current_inbounds ) {
        if( !sound_events.empty() ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sound_adapter rejected_request"
                                       << " current_omt=" << request.current_omt
                                       << " current_inbounds=" << ( request_current_inbounds ? "yes" : "no" )
                                       << " outing_kind=" << static_cast<int>( outing.kind )
                                       << " owner=" << static_cast<int>( outing.owner )
                                       << " party_power=" << request.party_power
                                       << " forward_count=" << request.visible_forward_omts.size()
                                       << " events=" << sound_events.size() << '\n';
        }
        return result;
    }
    const shared_ptr_fast<npc> observer = overmap_buffer.find_npc( outing.leader_id );
    if( !observer || observer->is_dead() ) {
        return result;
    }
    const int sight_points = live_bandit_structural_observer_sight(
                                 *observer, request.current_omt ).sight_points;

    std::vector<const live_bandit_signal_observation *> candidates;
    for( const live_bandit_signal_observation &signal : signals ) {
        const bool supported_kind = signal.mark.kind == "smoke" ||
                                    signal.mark.kind == "light" ||
                                    signal.mark.kind == "searchlight";
        const bool source_is_permitted = signal.source_omt == request.current_omt ||
                                         std::find( request.visible_forward_omts.begin(),
                                                 request.visible_forward_omts.end(),
                                                 signal.source_omt ) !=
                                         request.visible_forward_omts.end();
        const int scout_range = rl_dist( request.current_omt, signal.source_omt );
        if( supported_kind && source_is_permitted && signal.range_cap_omt > 0 &&
            signal.range_cap_omt <= 40 && scout_range <= signal.range_cap_omt &&
            ( signal.source_omt == request.current_omt ||
              live_bandit_overmap_los_from( request.current_omt, signal.source_omt,
                      sight_points ) ) ) {
            candidates.push_back( &signal );
        }
    }
    std::sort( candidates.begin(), candidates.end(), [&request](
    const live_bandit_signal_observation *lhs,
    const live_bandit_signal_observation *rhs ) {
        return std::make_tuple( rl_dist( request.current_omt, lhs->source_omt ),
                                lhs->source_omt.z(), lhs->source_omt.y(), lhs->source_omt.x(),
                                lhs->mark.kind, lhs->mark.mark_id ) <
               std::make_tuple( rl_dist( request.current_omt, rhs->source_omt ),
                                rhs->source_omt.z(), rhs->source_omt.y(), rhs->source_omt.x(),
                                rhs->mark.kind, rhs->mark.mark_id );
    } );

    for( const bool select_smoke : { true, false } ) {
        const auto found = std::find_if( candidates.begin(), candidates.end(), [select_smoke](
        const live_bandit_signal_observation * candidate ) {
            return ( candidate->mark.kind == "smoke" ) == select_smoke;
        } );
        if( found == candidates.end() ) {
            continue;
        }
        const live_bandit_signal_observation &signal = **found;
        const int scout_range = rl_dist( request.current_omt, signal.source_omt );
        bandit_live_world::structural_signal_read read;
        read.sense = select_smoke ? bandit_live_world::sortie_observation_sense::smoke :
                     bandit_live_world::sortie_observation_sense::light;
        read.source_omt = signal.source_omt;
        read.range_cap_omt = signal.range_cap_omt;
        read.strength = std::clamp( signal.mark.strength, 1, 6 );
        read.confidence = std::clamp( 40 + 10 * signal.mark.confidence, 0, 60 );
        read.uncertainty_radius_omt = std::clamp( std::max( 1, ( scout_range + 2 ) / 3 ) +
                                                ( select_smoke ? 1 : 0 ), 1, 40 );
        read.summary = signal.weather_summary;
        result.push_back( std::move( read ) );
    }

    struct audible_sound {
        const live_bandit_sound_observation *event = nullptr;
        int effective_volume = 0;
        int distance_omt = 0;
    };
    std::vector<audible_sound> audible;
    const int now_minutes = live_bandit_current_minutes();
    const int weather_attenuation = live_bandit_remote_weather_at(
                                        request.current_omt )->sound_attn;
    for( const live_bandit_sound_observation &event : sound_events ) {
        const bool supported_kind = event.kind == sounds::significant_sound_t::gunfire ||
                                    event.kind == sounds::significant_sound_t::alarm ||
                                    event.kind == sounds::significant_sound_t::explosion;
        const bool source_is_permitted = event.source_omt == request.current_omt ||
                                         std::find( request.visible_forward_omts.begin(),
                                                 request.visible_forward_omts.end(),
                                                 event.source_omt ) !=
                                         request.visible_forward_omts.end();
        const int distance_omt = rl_dist( request.current_omt, event.source_omt );
        const int vertical_omt = std::abs( request.current_omt.z() - event.source_omt.z() );
        const int conservative_distance_ms = ( distance_omt + 1 ) * 2 * SEEX - 1 +
                                             vertical_omt * 10 * SEEX;
        const int effective_volume = static_cast<int>( std::floor(
                                         std::max( 0, event.volume - weather_attenuation ) *
                                         observer->hearing_ability() ) );
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sound_adapter event"
                                   << " source_omt=" << event.source_omt
                                   << " current_omt=" << request.current_omt
                                   << " supported=" << ( supported_kind ? "yes" : "no" )
                                   << " permitted=" << ( source_is_permitted ? "yes" : "no" )
                                   << " volume=" << event.volume
                                   << " emitted_minutes=" << event.emitted_minutes
                                   << " now_minutes=" << now_minutes
                                   << " observer_deaf=" << ( observer->is_deaf() ? "yes" : "no" )
                                   << " weather_attenuation=" << weather_attenuation
                                   << " effective_volume=" << effective_volume
                                   << " required_volume=" << conservative_distance_ms << '\n';
        if( !supported_kind || !source_is_permitted || event.volume < 24 ||
            event.emitted_minutes < 0 || event.emitted_minutes > now_minutes ||
            now_minutes - event.emitted_minutes > 180 || observer->is_deaf() ) {
            continue;
        }
        if( effective_volume >= conservative_distance_ms ) {
            audible.push_back( { &event, effective_volume, distance_omt } );
        }
    }
    std::sort( audible.begin(), audible.end(), []( const audible_sound &lhs,
    const audible_sound &rhs ) {
        return std::make_tuple( -lhs.effective_volume, -lhs.event->emitted_minutes,
                                lhs.distance_omt, lhs.event->source_omt.z(),
                                lhs.event->source_omt.y(), lhs.event->source_omt.x(),
                                static_cast<int>( lhs.event->kind ) ) <
               std::make_tuple( -rhs.effective_volume, -rhs.event->emitted_minutes,
                                rhs.distance_omt, rhs.event->source_omt.z(),
                                rhs.event->source_omt.y(), rhs.event->source_omt.x(),
                                static_cast<int>( rhs.event->kind ) );
    } );
    if( !audible.empty() ) {
        const audible_sound &selected = audible.front();
        const live_bandit_sound_observation &event = *selected.event;
        bandit_live_world::structural_signal_read read;
        read.sense = bandit_live_world::sortie_observation_sense::sound;
        switch( event.kind ) {
            case sounds::significant_sound_t::gunfire:
                read.sound_kind = bandit_live_world::structural_sound_kind::gunfire;
                read.summary = "uncertain gunfire heard along the committed route";
                break;
            case sounds::significant_sound_t::alarm:
                read.sound_kind = bandit_live_world::structural_sound_kind::alarm;
                read.summary = "uncertain alarm heard along the committed route";
                break;
            case sounds::significant_sound_t::explosion:
                read.sound_kind = bandit_live_world::structural_sound_kind::explosion;
                read.summary = "uncertain explosion heard along the committed route";
                break;
            case sounds::significant_sound_t::none:
                break;
        }
        read.source_omt = event.source_omt;
        read.emitted_minutes = event.emitted_minutes;
        read.range_cap_omt = std::clamp( std::max( 1,
                                                ( selected.effective_volume - ( 2 * SEEX - 1 ) ) /
                                                ( 2 * SEEX ) ), 1, 40 );
        read.strength = std::clamp( selected.effective_volume / ( 2 * SEEX ), 1, 6 );
        const int age_minutes = now_minutes - event.emitted_minutes;
        read.confidence = std::clamp( 45 + selected.effective_volume / 12 -
                                      5 * ( age_minutes / 30 ), 25, 75 );
        read.uncertainty_radius_omt = std::clamp( 1 + selected.distance_omt / 2 +
                                                age_minutes / 60, 1, 40 );
        result.push_back( std::move( read ) );
    }
    return result;
}

bandit_live_world::structural_bounty_maintenance_result maintain_live_bandit_structural_bounty(
    const std::vector<live_bandit_signal_observation> &live_signals,
    const std::vector<live_bandit_sound_observation> &live_sounds )
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    static constexpr int structural_scan_budget = 12;
    static constexpr int structural_dispatch_cap = 2;
    static constexpr int structural_watch_path_budget = 8;
    int watch_paths_remaining = structural_watch_path_budget;
    const bool observer_enabled = get_avatar().has_trait( trait_DEBUG_CLAIRVOYANCE );
    const auto route_lookup = [&watch_paths_remaining, observer_enabled](
    const bandit_live_world::site_record & site,
    const bandit_live_world::structural_outing_plan & plan ) {
        const bandit_live_world::structural_route_read read =
            live_bandit_structural_route_read( site, plan, watch_paths_remaining );
        if( observer_enabled ) {
            DebugLog( D_INFO, DC_ALL ) << live_bandit_structural_route_analyzer_record(
                                      site, plan,
                                      plan.frontier_sector >= 0 ? "frontier" : "non_frontier", read ) << '\n';
        }
        return read;
    };
    bandit_live_world::structural_bounty_maintenance_result result =
        bandit_live_world::advance_structural_bounty_maintenance( state, current_minutes,
                structural_scan_budget, structural_dispatch_cap, live_bandit_structural_terrain_id,
                live_bandit_structural_threat_read, route_lookup,
                live_bandit_structural_abstract_threat_read,
                [&live_signals, &live_sounds](
                    const bandit_live_world::site_record & site,
                    const bandit_live_world::active_outing_state & outing,
                    const bandit_live_world::structural_threat_observer_request & request ) {
                    return live_bandit_structural_signal_reads( live_signals, live_sounds,
                            site, outing, request );
                }, []( bandit_live_world::world_state & world, const std::size_t site_index ) {
                    if( site_index >= world.sites.size() ) {
                        return 0;
                    }
                    return live_bandit_materialize_abstract_members_for_routine(
                               world, world.sites[site_index] );
                }, []( const bandit_live_world::site_record & site ) {
                    return live_bandit_response_member_power_reads_impl( site );
                } );
    DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_structural_bounty_maintenance_report( result );
    DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_evidence_debug_report(
                                  state, current_minutes );
    return result;
}

bandit_live_world::structural_signal_record_result record_live_bandit_structural_sounds(
    bandit_live_world::world_state &state,
    const std::vector<live_bandit_sound_observation> &live_sounds )
{
    const std::vector<live_bandit_signal_observation> no_field_signals;
    return bandit_live_world::record_structural_signal_observations(
               state, live_bandit_current_minutes(),
               [&no_field_signals, &live_sounds](
                   const bandit_live_world::site_record & site,
                   const bandit_live_world::active_outing_state & outing,
               const bandit_live_world::structural_threat_observer_request & request ) {
        return live_bandit_structural_signal_reads(
                   no_field_signals, live_sounds, site, outing, request );
    } );
}

int advance_live_bandit_local_scout_assessments()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    int completed = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            outing.last_advanced_minutes > current_minutes ) {
            continue;
        }
        const std::string activity_id = outing.activity_id;
        const int generation = outing.generation;
        const int target_revision = outing.target_lead_revision;
        const bandit_live_world::site_record before_assessment = site;
        const bandit_live_world::scout_assessment_result result =
            bandit_live_world::advance_structural_scout_assessment(
                site, activity_id, generation, target_revision, current_minutes );
        if( result == bandit_live_world::scout_assessment_result::normal_success ) {
            completed++;
        } else if( result ==
                   bandit_live_world::scout_assessment_result::alternate_watch_reposition_required ) {
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( !cursor ) {
                site = before_assessment;
                continue;
            }
            std::vector<live_bandit_local_handoff_member_backup> backups;
            std::map<character_id, std::vector<tripoint_abs_omt>> routes;
            backups.reserve( site.active_outing.member_ids.size() );
            bool preflight_failed = false;
            for( const character_id member_id : site.active_outing.member_ids ) {
                shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
                if( !member || member->is_dead() ||
                    member->pos_abs_omt() != site.active_outing.selected_watch_omt ) {
                    preflight_failed = true;
                    break;
                }
                std::vector<tripoint_abs_omt> route = live_bandit_member_route_to(
                            *member, site, site.active_outing.alternate_watch_omt );
                if( route.empty() ) {
                    preflight_failed = true;
                    break;
                }
                backups.push_back( { member, member->pos_abs(), member->goal,
                                     member->omt_path, member->mission,
                                     member->previous_mission, member->goto_to_this_pos,
                                     member->get_ai_guard_pos(), member->path } );
                routes.emplace( member_id, std::move( route ) );
            }
            if( preflight_failed || backups.size() != 2 || routes.size() != 2 ) {
                if( bandit_live_world::abort_local_pair_alternate_watch_reposition(
                        site, *cursor, current_minutes,
                        "alternate watch route unavailable at departure" ) ==
                    bandit_live_world::local_handoff_commit_result::applied ) {
                    for( const live_bandit_local_handoff_member_backup &backup : backups ) {
                        backup.member->omt_path.clear();
                        live_bandit_route_member_home( *backup.member, site );
                    }
                    completed++;
                } else {
                    site = before_assessment;
                }
                continue;
            }
            const auto restore_routes = [&backups]() {
                for( const live_bandit_local_handoff_member_backup &backup : backups ) {
                    backup.member->goal = backup.goal;
                    backup.member->omt_path = backup.omt_path;
                    backup.member->mission = backup.mission;
                    backup.member->previous_mission = backup.previous_mission;
                    backup.member->goto_to_this_pos = backup.ordered_position;
                    if( backup.ai_guard_position ) {
                        backup.member->set_ai_guard_pos( *backup.ai_guard_position );
                    } else {
                        backup.member->clear_ai_guard_pos();
                    }
                    backup.member->path = backup.local_path;
                }
            };
            bool routes_bound = true;
            for( const live_bandit_local_handoff_member_backup &backup : backups ) {
                const auto route = routes.find( backup.member->getID() );
                if( route == routes.end() || backup.member->is_dead() ||
                    backup.member->pos_abs_omt() != site.active_outing.selected_watch_omt ) {
                    routes_bound = false;
                    break;
                }
                backup.member->goal = site.active_outing.alternate_watch_omt;
                backup.member->omt_path = route->second;
                backup.member->mission = NPC_MISSION_TRAVELLING;
                backup.member->previous_mission = NPC_MISSION_NULL;
                backup.member->goto_to_this_pos = std::nullopt;
                backup.member->clear_ai_guard_pos();
                backup.member->path.clear();
            }
            if( !routes_bound ||
                bandit_live_world::start_local_pair_alternate_watch_reposition(
                    site, *cursor, current_minutes ) !=
                bandit_live_world::local_handoff_commit_result::applied ) {
                restore_routes();
                site = before_assessment;
                continue;
            }
            completed++;
        }
    }
    return completed;
}

} // namespace

bandit_live_world::structural_route_read live_bandit_structural_route_read_for_test(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan, int &watch_path_budget )
{
    return live_bandit_structural_route_read( site, plan, watch_path_budget );
}

std::vector<bandit_live_world::structural_route_read>
live_bandit_structural_route_analyzer_reads_for_test(
    const bandit_live_world::site_record &site,
    const std::vector<bandit_live_world::structural_outing_plan> &plans,
    int &watch_path_budget )
{
    std::vector<bandit_live_world::structural_route_read> reads;
    reads.reserve( plans.size() );
    for( const bandit_live_world::structural_outing_plan &plan : plans ) {
        reads.push_back( live_bandit_structural_route_read( site, plan, watch_path_budget ) );
    }
    return reads;
}

std::string live_bandit_structural_route_analyzer_record_for_test(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan,
    const std::string &selector,
    const bandit_live_world::structural_route_read &read )
{
    return live_bandit_structural_route_analyzer_record( site, plan, selector, read );
}

void run_live_bandit_structural_route_analyzer_for_debug()
{
    if( !get_avatar().has_trait( trait_DEBUG_CLAIRVOYANCE ) ) {
        return;
    }

    const bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int now_minutes = live_bandit_current_minutes();
    static constexpr int structural_watch_path_budget = 8;
    int watch_paths_remaining = structural_watch_path_budget;
    for( const bandit_live_world::site_record &site : state.sites ) {
        const std::vector<bandit_live_world::structural_outing_plan> candidates =
            bandit_live_world::plan_structural_bounty_outing_candidates( site, now_minutes, false );
        auto log_plan = [&site](
            const bandit_live_world::structural_outing_plan &plan,
            const std::string &selector,
            const bandit_live_world::structural_route_read &read ) {
            DebugLog( D_INFO, DC_ALL ) << live_bandit_structural_route_analyzer_record(
                                      site, plan, selector, read ) << '\n';
        };
        if( !candidates.empty() ) {
            const std::vector<bandit_live_world::structural_route_read> reads =
                live_bandit_structural_route_analyzer_reads_for_test(
                    site, candidates, watch_paths_remaining );
            for( std::size_t index = 0; index < candidates.size(); ++index ) {
                log_plan( candidates[index], "non_frontier", reads[index] );
            }
            continue;
        }

        const bandit_live_world::structural_outing_plan frontier =
            bandit_live_world::plan_frontier_outing( site, now_minutes );
        if( frontier.valid ) {
            const std::vector<bandit_live_world::structural_route_read> reads =
                live_bandit_structural_route_analyzer_reads_for_test(
                    site, { frontier }, watch_paths_remaining );
            log_plan( frontier, "frontier", reads.front() );
        }
    }
}

namespace bandit_live_world
{
int burn_live_covert_scouts()
{
    return burn_live_bandit_covert_scouts();
}

int record_live_covert_visible_defenders()
{
    return record_live_bandit_covert_visible_defenders();
}

int record_live_covert_vehicle_wealth_cues()
{
    return record_live_bandit_covert_vehicle_wealth_cues();
}

int record_live_covert_generation_infrastructure_cues()
{
    return record_live_bandit_covert_generation_infrastructure_cues();
}

int record_live_covert_cargo_handling_cues()
{
    return record_live_bandit_covert_cargo_handling_cues();
}

std::vector<response_member_power_read> live_response_member_power_reads(
    const site_record &site )
{
    return live_bandit_response_member_power_reads_impl( site );
}

response_party_selection_result select_live_capable_response_party(
    const site_record &site, const camp_report_policy policy, const int danger_high )
{
    return select_capable_response_party(
               site, policy, danger_high, live_response_member_power_reads( site ) );
}

bool fail_live_covert_scout_burned_egress( const character_id member_id )
{
    return live_bandit_fail_burned_egress( member_id );
}

std::optional<structural_local_zombie_read> read_live_structural_local_zombie_observation(
    const site_record &site )
{
    return live_bandit_local_zombie_read_impl( site );
}
} // namespace bandit_live_world

namespace turn_handler
{
bool cleanup_at_end()
{
    avatar &u = get_avatar();
    if( g->uquit == QUIT_DIED || g->uquit == QUIT_SUICIDE ) {
        // Put (non-hallucinations) into the overmap so they are not lost.
        for( monster &critter : g->all_monsters() ) {
            g->despawn_monster( critter );
        }
        // if player has "hunted" trait, remove their nemesis monster on death
        if( u.has_trait( trait_HAS_NEMESIS ) ) {
            overmap_buffer.remove_nemesis();
        }
        // Reset NPC factions and disposition
        g->reset_npc_dispositions();
        // Save the factions', missions and set the NPC's overmap coordinates
        // Npcs are saved in the overmap.
        g->save_factions_missions_npcs(); //missions need to be saved as they are global for all saves.

        // and the overmap, and the local map.
        g->save_maps(); //Omap also contains the npcs who need to be saved.

        //save achievements entry
        g->save_achievements();

        g->death_screen();
        std::chrono::seconds time_since_load =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - g->time_of_last_load );
        std::chrono::seconds total_time_played = g->time_played_at_last_load + time_since_load;
        get_event_bus().send<event_type::game_over>( total_time_played );
        // Struck the save_player_data here to forestall Weirdness
        g->move_save_to_graveyard();
        g->write_memorial_file( g->stats().value_of( event_statistic_last_words )
                                .get<cata_variant_type::string>() );
        get_memorial().clear();
        std::vector<std::string> characters = g->list_active_saves();
        // remove current player from the active characters list, as they are dead
        std::vector<std::string>::iterator curchar = std::find( characters.begin(),
                characters.end(), u.get_save_id() );
        if( curchar != characters.end() ) {
            characters.erase( curchar );
        }

        if( characters.empty() ) {
            bool queryDelete = false;
            bool queryReset = false;

            if( get_option<std::string>( "WORLD_END" ) == "query" ) {
                bool decided = false;
                std::string buffer = _( "Warning: NPC interactions and some other global flags "
                                        "will not all reset when starting a new character in an "
                                        "already-played world.  This can lead to some strange "
                                        "behavior.\n\n"
                                        "Are you sure you wish to keep this world?"
                                      );

                while( !decided ) {
                    uilist smenu;
                    smenu.allow_cancel = false;
                    smenu.addentry( 0, true, 'r', "%s", _( "Reset world" ) );
                    smenu.addentry( 1, true, 'd', "%s", _( "Delete world" ) );
                    smenu.addentry( 2, true, 'k', "%s", _( "Keep world" ) );
                    smenu.query();

                    switch( smenu.ret ) {
                        case 0:
                            queryReset = true;
                            decided = true;
                            break;
                        case 1:
                            queryDelete = true;
                            decided = true;
                            break;
                        case 2:
                            decided = query_yn( buffer );
                            break;
                    }
                }
            }

            if( queryDelete || get_option<std::string>( "WORLD_END" ) == "delete" ) {
                world_generator->delete_world( world_generator->active_world->world_name, true );

            } else if( queryReset || get_option<std::string>( "WORLD_END" ) == "reset" ) {
                world_generator->delete_world( world_generator->active_world->world_name, false );
            }
        } else if( get_option<std::string>( "WORLD_END" ) != "keep" ) {
            std::string tmpmessage;
            for( auto &character : characters ) {
                tmpmessage += "\n  ";
                tmpmessage += character;
            }
            popup( _( "World retained.  Characters remaining:%s" ), tmpmessage );
        }
        if( g->gamemode ) {
            g->gamemode = std::make_unique<special_game>(); // null gamemode or something..
        }
    }

    //Reset any offset due to driving
    g->set_driving_view_offset( point_rel_ms::zero );

    //clear all sound channels
    sfx::fade_audio_channel( sfx::channel::any, 300 );
    sfx::fade_audio_group( sfx::group::weather, 300 );
    sfx::fade_audio_group( sfx::group::time_of_day, 300 );
    sfx::fade_audio_group( sfx::group::context_themes, 300 );
    sfx::fade_audio_group( sfx::group::low_stamina, 300 );

    zone_manager::get_manager().clear();

    MAPBUFFER.clear();
    overmap_buffer.clear();

#if defined(__ANDROID__)
    quick_shortcuts_map.clear();
#endif
    return true;
}

} // namespace turn_handler

void handle_key_blocking_activity()
{
    if( test_mode ) {
        return;
    }
    avatar &u = get_avatar();
    const bool has_unfinished_activity = u.activity && (
            u.activity.id()->based_on() == based_on_type::NEITHER
            || u.activity.moves_left > 0 );
    if( has_unfinished_activity || u.has_destination() ) {
        input_context ctxt = get_default_mode_input_context();
        const std::string action = ctxt.handle_input( 0 );
        bool refresh = true;
        if( action == "pause" ) {
            if( u.activity.is_interruptible_with_kb() ) {
                g->cancel_activity_query( _( "Confirm:" ) );
            }
        } else if( action == "zoom_in" ) {
            g->zoom_in();
            g->mark_main_ui_adaptor_resize();
        } else if( action == "zoom_out" ) {
            g->zoom_out();
            g->mark_main_ui_adaptor_resize();
        } else if( action == "player_data" ) {
            u.disp_info( true );
        } else if( action == "messages" ) {
            Messages::display_messages();
        } else if( action == "help" ) {
            get_help().display_help();
        } else if( action != "HELP_KEYBINDINGS" ) {
            refresh = false;
        }
        if( refresh ) {
            ui_manager::redraw();
            refresh_display();
        }
    } else {
        refresh_display();
        inp_mngr.pump_events();
    }
}

namespace
{
struct live_bandit_pair_boundary_step {
    tripoint_abs_ms departure;
    tripoint_abs_ms exit;
};

bool live_bandit_local_step_respects_nonreentry(
    const npc &member,
    const bandit_live_world::covert_scout_relationship_read &relationship,
    map &here, const tripoint_bub_ms &step )
{
    const tripoint_abs_omt step_omt = project_to<coords::omt>( here.get_abs( step ) );
    const std::optional<int> distance =
        bandit_live_world::target_footprint_watch_distance(
            step_omt, relationship.target_footprint );
    return distance && *distance >= relationship.minimum_target_distance &&
           ( step_omt == member.pos_abs_omt() ||
             std::find( relationship.forbidden_route_omts.begin(),
                        relationship.forbidden_route_omts.end(), step_omt ) ==
             relationship.forbidden_route_omts.end() );
}

struct live_bandit_safe_local_route_read {
    bool safe = false;
    bool solved = false;
    std::size_t path_size = 0;
};

live_bandit_safe_local_route_read live_bandit_safe_local_route_to(
    npc &member,
    const bandit_live_world::covert_scout_relationship_read &relationship,
    map &here, const tripoint_abs_ms &destination )
{
    live_bandit_safe_local_route_read read;
    const tripoint_bub_ms local_destination = here.get_bub( destination );
    if( member.pos_abs() == destination ) {
        read.safe = live_bandit_local_step_respects_nonreentry(
                        member, relationship, here, local_destination );
        return read;
    }
    const std::function<bool( const tripoint_bub_ms & )> npc_avoid =
        member.get_path_avoid();
    const auto combined_avoid = [&member, &relationship, &here,
                                 &npc_avoid]( const tripoint_bub_ms &step ) {
        return npc_avoid( step ) ||
               !live_bandit_local_step_respects_nonreentry(
                   member, relationship, here, step );
    };
    const std::vector<tripoint_bub_ms> route = here.route(
                member.pos_bub(), pathfinding_target::point( local_destination ),
                member.get_pathfinding_settings( false ), combined_avoid );
    read.solved = true;
    read.path_size = route.size();
    read.safe = !route.empty() &&
                std::all_of( route.begin(), route.end(),
    [&member, &relationship, &here]( const tripoint_bub_ms & step ) {
        return live_bandit_local_step_respects_nonreentry(
                   member, relationship, here, step );
    } );
    return read;
}

std::map<character_id, live_bandit_pair_boundary_step>
live_bandit_pair_boundary_steps(
    const std::map<character_id, tripoint_abs_omt> &destinations,
    std::vector<std::string> *route_discriminators = nullptr,
    const bool require_safe_routes = false )
{
    std::map<character_id, live_bandit_pair_boundary_step> result;
    map &here = get_map();
    for( const bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 || destinations.size() < 2 ) {
            continue;
        }
        const auto first_destination = destinations.find( outing.member_ids[0] );
        const auto second_destination = destinations.find( outing.member_ids[1] );
        if( first_destination == destinations.end() || second_destination == destinations.end() ||
            first_destination->second != second_destination->second ) {
            continue;
        }
        const tripoint_abs_omt destination = first_destination->second;
        const tripoint_abs_ms destination_center =
            project_to<coords::ms>( destination ) + point( SEEX, SEEY );
        npc *first = g->find_npc( outing.member_ids[0] );
        npc *second = g->find_npc( outing.member_ids[1] );
        if( here.inbounds( destination_center ) || first == nullptr || second == nullptr ||
            first->is_dead() || second->is_dead() || !first->is_active() ||
            !second->is_active() ) {
            continue;
        }

        struct candidate {
            tripoint_abs_ms departure;
            tripoint_abs_ms exit;
        };
        const auto route_rank = []( const npc & member,
        const tripoint_abs_omt &route_omt ) -> std::optional<std::size_t> {
            const tripoint_abs_omt current = member.pos_abs_omt();
            std::size_t rank = 0;
            for( auto route = member.omt_path.rbegin(); route != member.omt_path.rend(); ++route ) {
                if( *route == current ) {
                    continue;
                }
                ++rank;
                if( *route == route_omt ) {
                    return rank;
                }
            }
            return route_omt == current ? std::optional<std::size_t>( 0 ) : std::nullopt;
        };
        std::vector<candidate> candidates;
        for( const tripoint_bub_ms &point : here.points_on_zlevel( destination.z() ) ) {
            const tripoint_abs_ms departure = here.get_abs( point );
            if( !here.passable( point ) ||
                ( !g->is_empty( point ) && departure != first->pos_abs() &&
                  departure != second->pos_abs() ) ) {
                continue;
            }
            for( int dy = -1; dy <= 1; ++dy ) {
                for( int dx = -1; dx <= 1; ++dx ) {
                    if( dx == 0 && dy == 0 ) {
                        continue;
                    }
                    const tripoint_abs_ms exit = departure + point_rel_ms( dx, dy );
                    if( !here.inbounds( exit ) ) {
                        candidates.push_back( { departure, exit } );
                    }
                }
            }
        }
        std::sort( candidates.begin(), candidates.end(), []( const candidate &lhs,
        const candidate &rhs ) {
            return std::tie( lhs.departure, lhs.exit ) <
                   std::tie( rhs.departure, rhs.exit );
        } );

        std::optional<std::pair<candidate, candidate>> selected;
        std::tuple<std::size_t, std::size_t, int, int,
            tripoint_abs_ms, tripoint_abs_ms> selected_score;
        std::vector<std::pair<candidate, candidate>> complete_pairs;
        for( const candidate &first_candidate : candidates ) {
            for( const candidate &second_candidate : candidates ) {
                if( first_candidate.departure == second_candidate.departure ||
                    first_candidate.exit == second_candidate.exit ||
                    // Preserve the adjacent two-slot formation already required at handoff.
                    rl_dist( first_candidate.exit, second_candidate.exit ) > 1 ) {
                    continue;
                }
                const std::optional<std::size_t> first_route_rank = route_rank(
                            *first, project_to<coords::omt>( first_candidate.exit ) );
                const std::optional<std::size_t> second_route_rank = route_rank(
                            *second, project_to<coords::omt>( second_candidate.exit ) );
                if( !first_route_rank || !second_route_rank ) {
                    continue;
                }
                if( require_safe_routes || route_discriminators != nullptr ) {
                    complete_pairs.emplace_back( first_candidate, second_candidate );
                }
                const int first_distance = rl_dist( first->pos_abs(), first_candidate.departure );
                const int second_distance = rl_dist( second->pos_abs(), second_candidate.departure );
                const auto score = std::make_tuple(
                                       std::max( *first_route_rank, *second_route_rank ),
                                       *first_route_rank + *second_route_rank,
                                       std::max( first_distance, second_distance ),
                                       first_distance + second_distance,
                                       first_candidate.departure, second_candidate.departure );
                if( !selected || score < selected_score ) {
                    selected = std::make_pair( first_candidate, second_candidate );
                    selected_score = score;
                }
            }
        }
        std::optional<std::pair<candidate, candidate>> safe_pair;
        if( require_safe_routes || route_discriminators != nullptr ) {
            const auto pair_identity = [first, second](
            const std::pair<candidate, candidate> &pair ) {
                std::ostringstream identity;
                identity << first->getID().get_value() << ':'
                         << pair.first.departure.to_string() << '>'
                         << pair.first.exit.to_string() << '|'
                         << second->getID().get_value() << ':'
                         << pair.second.departure.to_string() << '>'
                         << pair.second.exit.to_string();
                return identity.str();
            };
            unsigned long long complete_identity = 14695981039346656037ULL;
            for( const std::pair<candidate, candidate> &pair : complete_pairs ) {
                const std::string identity = pair_identity( pair );
                for( const unsigned char byte : identity ) {
                    complete_identity ^= byte;
                    complete_identity *= 1099511628211ULL;
                }
                complete_identity ^= 0xffU;
                complete_identity *= 1099511628211ULL;
            }
            const std::optional<bandit_live_world::covert_scout_relationship_read>
            first_relationship = bandit_live_world::read_active_covert_scout_homeward_member(
                                     overmap_buffer.global_state.bandit_live_world,
                                     first->getID() );
            const std::optional<bandit_live_world::covert_scout_relationship_read>
            second_relationship = bandit_live_world::read_active_covert_scout_homeward_member(
                                      overmap_buffer.global_state.bandit_live_world,
                                      second->getID() );
            std::size_t route_pairs_evaluated = 0;
            std::size_t first_route_size = 0;
            std::size_t second_route_size = 0;
            std::map<tripoint_abs_ms, live_bandit_safe_local_route_read> first_route_reads;
            std::map<tripoint_abs_ms, live_bandit_safe_local_route_read> second_route_reads;
            const auto read_route = [&here](
            npc &member,
            const bandit_live_world::covert_scout_relationship_read &relationship,
            const tripoint_abs_ms &departure,
            std::map<tripoint_abs_ms, live_bandit_safe_local_route_read> &route_reads ) ->
            const live_bandit_safe_local_route_read & {
                const auto found = route_reads.find( departure );
                if( found != route_reads.end() ) {
                    return found->second;
                }
                return route_reads.emplace(
                           departure, live_bandit_safe_local_route_to(
                               member, relationship, here, departure ) ).first->second;
            };
            if( first_relationship && second_relationship ) {
                for( const std::pair<candidate, candidate> &pair : complete_pairs ) {
                    route_pairs_evaluated++;
                    const live_bandit_safe_local_route_read &first_route = read_route(
                                *first, *first_relationship,
                                pair.first.departure, first_route_reads );
                    if( !first_route.safe ) {
                        continue;
                    }
                    const live_bandit_safe_local_route_read &second_route = read_route(
                                *second, *second_relationship,
                                pair.second.departure, second_route_reads );
                    if( !second_route.safe ) {
                        continue;
                    }
                    safe_pair = pair;
                    first_route_size = first_route.path_size;
                    second_route_size = second_route.path_size;
                    break;
                }
            }
            const auto solved_count = []( const auto & route_reads ) {
                return std::count_if( route_reads.begin(), route_reads.end(),
                []( const auto & entry ) {
                    return entry.second.solved;
                } );
            };
            if( route_discriminators != nullptr ) {
                std::ostringstream discriminator;
                discriminator << "site=" << site.site_id
                              << " generation=" << outing.generation
                              << " members=" << first->getID().get_value() << ','
                              << second->getID().get_value()
                              << " member_positions=" << first->pos_abs().to_string() << ','
                              << second->pos_abs().to_string()
                              << " complete_pairs=" << complete_pairs.size()
                              << " complete_identity=fnv1a:" << complete_identity
                              << " route_pairs_evaluated=" << route_pairs_evaluated
                              << " route_reads=" <<
                              first_route_reads.size() + second_route_reads.size()
                              << " route_solves=" <<
                              solved_count( first_route_reads ) + solved_count( second_route_reads )
                              << " relationships_complete=" <<
                              ( first_relationship && second_relationship ? "yes" : "no" )
                              << " safe_both=" << ( safe_pair ? "yes" : "no" )
                              << " verdict=" <<
                              ( !first_relationship || !second_relationship ? "unavailable" :
                                safe_pair ? "H1" : "H0" )
                              << " selected_pair=" <<
                              ( selected ? pair_identity( *selected ) : "none" )
                              << " safe_pair=" <<
                              ( safe_pair ? pair_identity( *safe_pair ) : "none" )
                              << " first_route_path=" << first_route_size
                              << " second_route_path=" << second_route_size;
                route_discriminators->push_back( discriminator.str() );
            }
        }
        const std::optional<std::pair<candidate, candidate>> &transition_pair =
            require_safe_routes ? safe_pair : selected;
        if( transition_pair ) {
            result.emplace( first->getID(), live_bandit_pair_boundary_step{
                transition_pair->first.departure, transition_pair->first.exit
            } );
            result.emplace( second->getID(), live_bandit_pair_boundary_step{
                transition_pair->second.departure, transition_pair->second.exit
            } );
        }
    }
    return result;
}

void log_live_bandit_homeward_motor_diagnostics(
    const bandit_live_world::world_state &state,
    const std::set<character_id> &homeward_member_ids )
{
    std::map<character_id, tripoint_abs_omt> homeward_destinations;
    for( const bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        for( const character_id member_id : outing.member_ids ) {
            if( homeward_member_ids.count( member_id ) > 0 ) {
                homeward_destinations.emplace( member_id, site.anchor );
            }
        }
    }
    const std::map<character_id, live_bandit_pair_boundary_step> homeward_boundary_steps =
        live_bandit_pair_boundary_steps( homeward_destinations, nullptr, true );
    map &here = get_map();
    for( const bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        std::ostringstream members;
        bool first_member = true;
        for( const character_id member_id : outing.member_ids ) {
            if( std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                outing.casualty_ids.end() ) {
                continue;
            }
            if( !first_member ) {
                members << ';';
            }
            first_member = false;
            const shared_ptr_fast<npc> persistent_member = overmap_buffer.find_npc( member_id );
            npc *loaded_member = g->find_npc( member_id );
            const npc *member = loaded_member != nullptr ? loaded_member : persistent_member.get();
            const auto boundary_step = homeward_boundary_steps.find( member_id );
            members << "id=" << member_id.get_value()
                    << ",present=" << ( member != nullptr ? "yes" : "no" )
                    << ",active=" << ( member != nullptr && member->is_active() ? "yes" : "no" )
                    << ",loaded=" << ( loaded_member != nullptr ? "yes" : "no" );
            if( member != nullptr ) {
                members << ",pos_abs=" << member->pos_abs().to_string()
                        << ",pos_omt=" << member->pos_abs_omt().to_string()
                        << ",goal=" << member->goal.to_string()
                        << ",mission=" << static_cast<int>( member->mission )
                        << ",is_travelling=" << ( member->is_travelling() ? "yes" : "no" )
                        << ",has_omt_destination=" <<
                        ( member->has_omt_destination() ? "yes" : "no" )
                        << ",omt_path=" << member->omt_path.size()
                        << ",local_path=" << member->path.size()
                        << ",motor_inbounds=" << ( here.inbounds( member->pos_abs() ) ? "yes" : "no" );
            }
            members << ",homeward_owned=" <<
                    ( homeward_member_ids.count( member_id ) > 0 ? "yes" : "no" )
                    << ",boundary_owned=" <<
                    ( boundary_step != homeward_boundary_steps.end() ? "yes" : "no" );
            if( boundary_step != homeward_boundary_steps.end() ) {
                members << ",boundary_departure=" <<
                        boundary_step->second.departure.to_string()
                        << ",boundary_exit=" << boundary_step->second.exit.to_string()
                        << ",at_boundary_departure=" <<
                        ( member != nullptr &&
                          member->pos_abs() == boundary_step->second.departure ? "yes" : "no" );
            }
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world homeward_motor_diag"
                                   << " site=" << site.site_id
                                   << " activity=" << outing.activity_id
                                   << " generation=" << outing.generation
                                   << " phase=" << bandit_live_world::to_string( outing.phase )
                                   << " members=[" << members.str() << "]\n";
    }
}

void complete_live_bandit_ingress_boundary_steps(
    const std::map<character_id, live_bandit_pair_boundary_step> &steps )
{
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 ) {
            continue;
        }
        const auto first_step = steps.find( outing.member_ids[0] );
        const auto second_step = steps.find( outing.member_ids[1] );
        npc *first = g->find_npc( outing.member_ids[0] );
        npc *second = g->find_npc( outing.member_ids[1] );
        if( first_step == steps.end() || second_step == steps.end() || first == nullptr ||
            second == nullptr || first->pos_abs() != first_step->second.departure ||
            second->pos_abs() != second_step->second.departure ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }
        const tripoint_abs_ms first_prior = first->pos_abs();
        const tripoint_abs_ms second_prior = second->pos_abs();
        first->setpos( first_step->second.exit, false );
        second->setpos( second_step->second.exit, false );
        std::vector<bandit_live_world::local_route_arrival_member_read> reads;
        for( npc *member : { first, second } ) {
            bandit_live_world::local_route_arrival_member_read read;
            read.npc_id = member->getID();
            read.readable = true;
            read.route_confirmed = member->pos_abs_omt() == outing.selected_watch_omt;
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            reads.push_back( read );
        }
        if( bandit_live_world::commit_local_pair_route_arrival(
                site, *cursor, live_bandit_current_minutes(), reads ) !=
            bandit_live_world::local_handoff_commit_result::applied ) {
            first->setpos( first_prior, false );
            second->setpos( second_prior, false );
            continue;
        }
        for( npc *member : { first, second } ) {
            member->goal = npc::no_goal_point;
            member->omt_path.clear();
            member->mission = NPC_MISSION_NULL;
            member->previous_mission = NPC_MISSION_NULL;
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
            member->on_unload();
            g->remove_npc( member->getID() );
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local ingress boundary committed"
                                   << " site=" << site.site_id
                                   << " activity=" << outing.activity_id
                                   << " generation=" << outing.generation
                                   << " route_position=" <<
                                   outing.local_handoff.route_position.to_string()
                                   << " members=2\n";
    }
}

void complete_live_bandit_homeward_boundary_steps(
    const std::map<character_id, live_bandit_pair_boundary_step> &steps )
{
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        const auto first_step = steps.find( outing.member_ids[0] );
        const auto second_step = steps.find( outing.member_ids[1] );
        npc *first = g->find_npc( outing.member_ids[0] );
        npc *second = g->find_npc( outing.member_ids[1] );
        if( first_step == steps.end() || second_step == steps.end() || first == nullptr ||
            second == nullptr || first->pos_abs() != first_step->second.departure ||
            second->pos_abs() != second_step->second.departure ) {
            continue;
        }
        const std::array<live_bandit_local_handoff_member_backup, 2> backups = { {
                { overmap_buffer.find_npc( first->getID() ), first->pos_abs(), first->goal,
                  first->omt_path, first->mission, first->previous_mission,
                  first->goto_to_this_pos, first->get_ai_guard_pos(), first->path },
                { overmap_buffer.find_npc( second->getID() ), second->pos_abs(), second->goal,
                  second->omt_path, second->mission, second->previous_mission,
                  second->goto_to_this_pos, second->get_ai_guard_pos(), second->path }
            } };
        first->setpos( first_step->second.exit, false );
        second->setpos( second_step->second.exit, false );
        if( !live_bandit_route_member_home( *first, site ) ||
            !live_bandit_route_member_home( *second, site ) ) {
            for( const live_bandit_local_handoff_member_backup &backup : backups ) {
                backup.member->setpos( backup.position, false );
                backup.member->goal = backup.goal;
                backup.member->omt_path = backup.omt_path;
                backup.member->mission = backup.mission;
                backup.member->previous_mission = backup.previous_mission;
                backup.member->goto_to_this_pos = backup.ordered_position;
                if( backup.ai_guard_position ) {
                    backup.member->set_ai_guard_pos( *backup.ai_guard_position );
                } else {
                    backup.member->clear_ai_guard_pos();
                }
                backup.member->path = backup.local_path;
            }
            continue;
        }
        const tripoint_abs_omt boundary_omt = first->pos_abs_omt();
        for( npc *member : { first, second } ) {
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
            member->on_unload();
            g->remove_npc( member->getID() );
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward boundary committed"
                                   << " site=" << site.site_id
                                   << " activity=" << outing.activity_id
                                   << " generation=" << outing.generation
                                   << " route_position=" << boundary_omt.to_string()
                                   << " destination=" << site.anchor.to_string()
                                   << " members=2\n";
    }
}

void monmove()
{
    g->cleanup_dead();
    map &m = get_map();
    avatar &u = get_avatar();

    for( monster &critter : g->all_monsters() ) {
        if( !m.inbounds( critter.pos_abs() ) ) {
            continue;
        }
        const tripoint_bub_ms critter_pos = critter.pos_bub( m );

        // Critters in impassable tiles get pushed away, unless it's not impassable for them
        if( !critter.is_dead() && ( m.impassable( critter_pos ) &&
                                    !m.get_impassable_field_at( critter_pos ).has_value() ) &&
            !critter.can_move_to( critter_pos ) ) {
            dbg( D_ERROR ) << "game:monmove: " << critter.name()
                           << " can't move to its location!  (" << critter_pos.x()
                           << ":" << critter_pos.y() << ":" << critter_pos.z() << "), "
                           << m.tername( critter_pos );
            add_msg_debug( debugmode::DF_MONSTER, "%s can't move to its location!  (%d,%d,%d), %s",
                           critter.name(),
                           critter_pos.x(), critter_pos.y(), critter_pos.z(), m.tername( critter_pos ) );
            bool okay = false;
            for( const tripoint_bub_ms &dest : m.points_in_radius( critter_pos, 3 ) ) {
                if( critter.can_move_to( dest ) && g->is_empty( dest ) ) {
                    critter.setpos( m, dest );
                    okay = true;
                    break;
                }
            }
            if( !okay ) {
                // die of "natural" cause (overpopulation is natural)
                critter.die( &m, nullptr );
            }
        }

        if( !critter.is_dead() ) {
            critter.process_turn();
        }

        m.creature_in_field( critter );
        if( calendar::once_every( 1_days ) ) {
            if( critter.has_flag( mon_flag_MILKABLE ) ) {
                critter.refill_udders();
            }
            critter.try_biosignature();
            critter.try_reproduce();
        }
        while( critter.get_moves() > 0 && !critter.is_dead() && !critter.has_effect( effect_ridden ) ) {
            critter.made_footstep = false;
            // Controlled critters don't make their own plans
            if( !critter.has_effect( effect_controlled ) ) {
                // Formulate a path to follow
                critter.plan();
            } else {
                critter.set_moves( 0 );
                break;
            }
            critter.move(); // Move one square, possibly hit u
            critter.process_triggers();
            m.creature_in_field( critter );
        }

        if( !critter.is_dead() && !critter.is_hallucination() &&
            rl_dist( u.pos_abs(), critter.pos_abs() ) < u.enchantment_cache->modify_value(
                enchant_vals::mod::MOTION_ALARM, 0 ) ) {
            if( u.has_active_bionic( bio_alarm ) ) {
                u.mod_power_level( -bio_alarm->power_trigger );
                add_msg( m_warning, _( "Your motion alarm goes off!" ) );
                g->cancel_activity_or_ignore_query( distraction_type::motion_alarm,
                                                    _( "Your motion alarm goes off!" ) );
            } else {
                add_msg( m_warning, _( "You suddenly feel alerted!" ) );
                g->cancel_activity_or_ignore_query( distraction_type::motion_alarm,
                                                    _( "Your instincts warn you for danger!" ) );
            }
            if( u.has_effect( effect_sleep ) ) {
                u.wake_up();
            }
        }
    }

    g->cleanup_dead();

    // The remaining monsters are all alive, but may be outside of the reality bubble.
    // If so, despawn them. This is not the same as dying, they will be stored for later and the
    // monster::die function is not called.
    g->despawn_nonlocal_monsters();

    // Now, do active NPCs.  Cohesion owns the first local cursor advance so
    // evidence recording can never delay an incomplete pair's safety update.
    std::map<character_id, tripoint_abs_ms> pair_assembly_orders;
    std::set<character_id> pair_homeward_travel_ids;
    std::map<character_id, character_id> pair_homeward_partner_ids;
    std::map<character_id, tripoint_abs_omt> pair_ingress_travel_destinations;
    std::map<character_id, live_bandit_pair_boundary_step> pair_ingress_boundary_steps;
    std::map<character_id, live_bandit_pair_boundary_step> pair_homeward_boundary_steps;
    std::vector<std::string> homeward_boundary_discriminators;
    std::set<character_id> profiled_covert_member_ids;
    std::set<character_id> logged_homeward_route_result_ids;
    const bool log_homeward_route_result = calendar::once_every( 60_minutes );
    {
        bandit_live_world_probe::scoped_section prepass(
            bandit_live_world_probe::section::loaded_covert_prepass );
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::loaded_covert_prepass_calls );
        bandit_live_world::burn_live_covert_scouts();
        pair_assembly_orders = maintain_live_bandit_local_pair_cohesion();
        if( calendar::once_every( 1_minutes ) ) {
            const int local_zombie_observations = record_live_bandit_local_zombie_observations();
            if( local_zombie_observations > 0 ) {
                // The first cohesion pass owns safety movement.  Re-read exact positions after
                // recording so an assembled pair can communicate the new fact before either NPC
                // moves or dies later in this turn.
                pair_assembly_orders = maintain_live_bandit_local_pair_cohesion();
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_zombie_observations="
                                           << local_zombie_observations << '\n';
            }
            const int visible_defender_observations =
                bandit_live_world::record_live_covert_visible_defenders();
            if( visible_defender_observations > 0 ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world visible_defender_observations="
                        << visible_defender_observations << '\n';
            }
            const int vehicle_wealth_observations =
                bandit_live_world::record_live_covert_vehicle_wealth_cues();
            if( vehicle_wealth_observations > 0 ) {
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world vehicle_wealth_observations="
                                           << vehicle_wealth_observations << '\n';
            }
            const int generation_infrastructure_observations =
                bandit_live_world::record_live_covert_generation_infrastructure_cues();
            if( generation_infrastructure_observations > 0 ) {
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world generation_infrastructure_observations="
                        << generation_infrastructure_observations << '\n';
            }
            const int cargo_handling_observations =
                bandit_live_world::record_live_covert_cargo_handling_cues();
            if( cargo_handling_observations > 0 ) {
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world cargo_handling_observations="
                                           << cargo_handling_observations << '\n';
            }
            const int local_scout_assessment_updates =
                advance_live_bandit_local_scout_assessments();
            if( local_scout_assessment_updates > 0 ) {
                // Assessment may release staging ownership into a homeward phase or an
                // alternate-watch reposition.  Refresh the cached motor view so that release
                // takes effect in this NPC turn rather than one turn later.
                pair_assembly_orders = maintain_live_bandit_local_pair_cohesion();
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_scout_assessment_updates="
                                           << local_scout_assessment_updates << '\n';
            }
        }
        pair_homeward_travel_ids = bandit_live_world::local_pair_homeward_travel_ids(
                                       overmap_buffer.global_state.bandit_live_world );
        pair_ingress_travel_destinations =
            bandit_live_world::local_pair_ingress_travel_destinations(
                overmap_buffer.global_state.bandit_live_world );
        pair_ingress_boundary_steps = live_bandit_pair_boundary_steps(
                                          pair_ingress_travel_destinations );
        std::map<character_id, tripoint_abs_omt> pair_homeward_destinations;
        for( const bandit_live_world::site_record &site :
             overmap_buffer.global_state.bandit_live_world.sites ) {
            if( site.active_outing.member_ids.size() == 2 &&
                pair_homeward_travel_ids.count( site.active_outing.member_ids[0] ) > 0 &&
                pair_homeward_travel_ids.count( site.active_outing.member_ids[1] ) > 0 ) {
                pair_homeward_partner_ids.emplace( site.active_outing.member_ids[0],
                                                   site.active_outing.member_ids[1] );
                pair_homeward_partner_ids.emplace( site.active_outing.member_ids[1],
                                                   site.active_outing.member_ids[0] );
            }
            for( const character_id member_id : site.active_outing.member_ids ) {
                if( pair_homeward_travel_ids.count( member_id ) > 0 ) {
                    pair_homeward_destinations.emplace( member_id, site.anchor );
                }
            }
        }
        pair_homeward_boundary_steps = live_bandit_pair_boundary_steps(
                                           pair_homeward_destinations,
                                           log_homeward_route_result &&
                                           u.has_trait( trait_DEBUG_CLAIRVOYANCE ) ?
                                           &homeward_boundary_discriminators : nullptr, true );
        for( const std::string &discriminator : homeward_boundary_discriminators ) {
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world homeward_boundary_pair_discriminator "
                    << discriminator << '\n';
        }
        if( bandit_live_world_probe::active() ) {
            for( const bandit_live_world::site_record &site :
                 overmap_buffer.global_state.bandit_live_world.sites ) {
                const bandit_live_world::active_outing_state &outing = site.active_outing;
                if( !site.retired_empty_site && outing.is_active() &&
                    outing.kind == bandit_live_world::outing_kind::structural_sortie &&
                    outing.owner == bandit_live_world::simulation_owner::local &&
                    outing.local_handoff.is_active() && outing.member_ids.size() == 2 ) {
                    profiled_covert_member_ids.insert( outing.member_ids.begin(),
                                                       outing.member_ids.end() );
                }
            }
        }
    }
    for( npc &guy : g->all_npcs() ) {
        const bool profiled_covert_member =
            profiled_covert_member_ids.count( guy.getID() ) > 0;
        std::optional<bandit_live_world_probe::scoped_section> member_motor;
        bandit_live_world_probe::scoped_loaded_covert_member member_scope(
            profiled_covert_member );
        if( profiled_covert_member ) {
            member_motor.emplace(
                bandit_live_world_probe::section::loaded_covert_member_motor );
            bandit_live_world_probe::increment(
                bandit_live_world_probe::counter::loaded_covert_members_processed );
        }
        int turns = 0;
        int real_count = 0;
        const int count_limit = std::max( 10, guy.get_moves() / 64 );
        if( guy.is_mounted() ) {
            guy.check_mount_is_spooked();
        }
        m.creature_in_field( guy );
        if( !guy.has_effect( effect_npc_suspend ) ) {
            guy.process_turn();
        }
        while( !guy.is_dead() && ( !guy.in_sleep_state() ||
                                   guy.activity.id() == ACT_OPERATION || guy.activity.id() == ACT_MIGRATION_CANCEL ) &&
               guy.get_moves() > 0 && turns < 10 ) {
            const int moves = guy.get_moves();
            const bool has_destination = guy.has_destination_activity();
            const auto assembly_order = pair_assembly_orders.find( guy.getID() );
            const auto ingress_destination = pair_ingress_travel_destinations.find(
                                                 guy.getID() );
            const auto ingress_boundary = pair_ingress_boundary_steps.find( guy.getID() );
            const auto homeward_boundary = pair_homeward_boundary_steps.find( guy.getID() );
            if( assembly_order != pair_assembly_orders.end() ) {
                if( guy.has_flag( json_flag_CANNOT_MOVE ) ||
                    !m.inbounds( assembly_order->second ) ||
                    guy.pos_abs() == assembly_order->second ) {
                    guy.move_pause();
                } else {
                    if( live_bandit_update_local_path(
                            guy, m.get_bub( assembly_order->second ) ) ) {
                        guy.move_to_next();
                    } else {
                        guy.path.clear();
                        guy.move_pause();
                    }
                }
            } else if( ingress_destination != pair_ingress_travel_destinations.end() ) {
                const std::optional<bandit_live_world::covert_scout_relationship_read>
                relationship = bandit_live_world::read_active_covert_scout_member(
                                   overmap_buffer.global_state.bandit_live_world, guy.getID() );
                const auto local_path_respects_watch_ring = [relationship, &m, &guy](
                const std::vector<tripoint_bub_ms> &candidate_path ) {
                    return relationship && std::all_of(
                               candidate_path.begin(), candidate_path.end(),
                    [relationship, &m, &guy]( const tripoint_bub_ms & step ) {
                        const tripoint_abs_omt step_omt =
                            project_to<coords::omt>( m.get_abs( step ) );
                        const std::optional<int> distance =
                            bandit_live_world::target_footprint_watch_distance(
                                step_omt, relationship->target_footprint );
                        return distance && *distance >= relationship->minimum_target_distance &&
                               ( step_omt == guy.pos_abs_omt() ||
                                 std::find( relationship->forbidden_route_omts.begin(),
                                            relationship->forbidden_route_omts.end(), step_omt ) ==
                                 relationship->forbidden_route_omts.end() );
                    } );
                };
                if( !relationship || guy.has_flag( json_flag_CANNOT_MOVE ) ||
                    !guy.is_travelling() ||
                    guy.goal != ingress_destination->second || guy.omt_path.empty() ) {
                    // The overmap cadence owns route binding and repair.  Keep this loaded
                    // member inert until that exact owner route is present.
                    guy.move_pause();
                } else if( ingress_boundary != pair_ingress_boundary_steps.end() ) {
                    if( guy.pos_abs() == ingress_boundary->second.departure ) {
                        guy.move_pause();
                    } else if( live_bandit_update_local_path(
                                   guy, m.get_bub( ingress_boundary->second.departure ) ) &&
                               local_path_respects_watch_ring( guy.path ) ) {
                        guy.move_to_next();
                    } else {
                        guy.path.clear();
                        guy.move_pause();
                    }
                } else {
                    guy.go_to_omt_destination( local_path_respects_watch_ring );
                }
            } else if( pair_homeward_travel_ids.count( guy.getID() ) > 0 ) {
                const std::optional<bandit_live_world::covert_scout_relationship_read>
                relationship = bandit_live_world::read_active_covert_scout_homeward_member(
                                   overmap_buffer.global_state.bandit_live_world, guy.getID() );
                const auto partner_id = pair_homeward_partner_ids.find( guy.getID() );
                npc *partner = partner_id == pair_homeward_partner_ids.end() ? nullptr :
                               g->find_npc( partner_id->second );
                const auto step_preserves_pair_cohesion = [&m, partner](
                const tripoint_bub_ms & step ) {
                    return partner == nullptr ||
                           rl_dist( m.get_abs( step ), partner->pos_abs() ) <=
                           bandit_live_world::local_pair_cohesion_radius();
                };
                Creature *threat = guy.current_target();
                const auto is_adjacent_non_camp_threat = [&]( const Creature *candidate ) {
                    return relationship && candidate != nullptr && candidate != &guy &&
                           rl_dist( guy.pos_abs(), candidate->pos_abs() ) <= 1 &&
                           guy.attitude_to( *candidate ) == Creature::Attitude::HOSTILE &&
                           std::find( relationship->target_footprint.begin(),
                                      relationship->target_footprint.end(),
                                      candidate->pos_abs_omt() ) ==
                           relationship->target_footprint.end();
                };
                if( !is_adjacent_non_camp_threat( threat ) ) {
                    threat = nullptr;
                    creature_tracker &creatures = get_creature_tracker();
                    for( const tripoint_bub_ms &nearby :
                         m.points_in_radius( guy.pos_bub( m ), 1 ) ) {
                        Creature *candidate = creatures.creature_at( nearby );
                        if( is_adjacent_non_camp_threat( candidate ) ) {
                            threat = candidate;
                            break;
                        }
                    }
                }
                const bool adjacent_non_camp_threat = relationship && threat != nullptr &&
                        is_adjacent_non_camp_threat( threat );
                const tripoint_abs_ms egress_center = relationship ?
                        project_to<coords::ms>( relationship->egress_omt ) + point( SEEX, SEEY ) :
                        guy.pos_abs();
                const tripoint_bub_ms local_egress = m.get_bub( egress_center );
                const auto choose_noninward_step = [&]( const bool require_field_clear ) {
                    std::optional<tripoint_bub_ms> result;
                    int result_forced_danger = std::numeric_limits<int>::max();
                    int result_field_danger = std::numeric_limits<int>::max();
                    const tripoint_bub_ms current = guy.pos_bub( m );
                    const tripoint_abs_omt current_omt = guy.pos_abs_omt();
                    for( const tripoint_bub_ms &candidate : m.points_in_radius( current, 1 ) ) {
                        if( candidate == current || !m.inbounds( candidate ) ||
                            !m.has_floor_or_water( candidate ) || !g->is_empty( candidate ) ) {
                            continue;
                        }
                        const tripoint_abs_omt candidate_omt =
                            project_to<coords::omt>( m.get_abs( candidate ) );
                        if( candidate_omt != current_omt &&
                            std::find( relationship->forbidden_route_omts.begin(),
                                       relationship->forbidden_route_omts.end(), candidate_omt ) !=
                            relationship->forbidden_route_omts.end() ) {
                            continue;
                        }
                        const bool actor_field_danger =
                            guy.sees_dangerous_field( candidate );
                        const trap &candidate_trap = m.tr_at( candidate );
                        const bool actor_trap_danger =
                            candidate_trap.can_see( candidate, guy ) &&
                            !candidate_trap.is_benign();
                        const bool traversable =
                            guy.can_move_to_ignoring_danger( candidate, true );
                        const bool ordinary_move = traversable && !actor_field_danger &&
                                                   !actor_trap_danger;
                        if( !traversable ||
                            ( require_field_clear && !ordinary_move ) ) {
                            continue;
                        }
                        const std::optional<int> candidate_distance = relationship ?
                                bandit_live_world::target_footprint_watch_distance(
                                    candidate_omt, relationship->target_footprint ) : std::nullopt;
                        if( !candidate_distance ||
                            *candidate_distance < relationship->minimum_target_distance ) {
                            continue;
                        }
                        int field_danger = 0;
                        for( const auto &entry : m.field_at( candidate ) ) {
                            if( guy.is_dangerous_field( entry.second ) ) {
                                field_danger += entry.second.get_field_intensity();
                            }
                        }
                        if( actor_trap_danger ) {
                            field_danger += 1000;
                        }
                        const int forced_danger = ordinary_move ? 0 : 1;
                        if( !result || std::make_tuple( forced_danger, field_danger,
                                             rl_dist( candidate, local_egress ),
                                             candidate.z(), candidate.y(), candidate.x() ) <
                            std::make_tuple( result_forced_danger, result_field_danger,
                                             rl_dist( *result, local_egress ),
                                             result->z(), result->y(), result->x() ) ) {
                            result = candidate;
                            result_forced_danger = forced_danger;
                            result_field_danger = field_danger;
                        }
                    }
                    return result;
                };
                const auto local_step_respects_nonreentry = [relationship, &m, &guy](
                const tripoint_bub_ms & step ) {
                    return relationship && live_bandit_local_step_respects_nonreentry(
                               guy, *relationship, m, step );
                };
                const auto local_path_respects_nonreentry = [relationship,
                            &local_step_respects_nonreentry](
                const std::vector<tripoint_bub_ms> &candidate_path ) {
                    return relationship && std::all_of(
                               candidate_path.begin(), candidate_path.end(),
                               local_step_respects_nonreentry );
                };
                const bool immediate_field_hazard = relationship &&
                        guy.sees_dangerous_field( guy.pos_bub( m ) );
                const std::optional<tripoint_bub_ms> field_escape = immediate_field_hazard ?
                        choose_noninward_step( true ) : std::nullopt;
                const std::optional<tripoint_bub_ms> forced_field_escape =
                    immediate_field_hazard && !field_escape ?
                    choose_noninward_step( false ) : std::nullopt;
                if( !relationship ) {
                    // Once committed contact ends the covert relationship, ordinary combat owns
                    // the actor again.
                    guy.move();
                } else if( guy.has_flag( json_flag_CANNOT_MOVE ) ) {
                    if( immediate_field_hazard && relationship->phase ==
                        bandit_live_world::scout_phase::burned_withdrawal ) {
                        bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                    }
                    if( adjacent_non_camp_threat &&
                        !guy.has_flag( json_flag_CANNOT_ATTACK ) ) {
                        guy.melee_attack( *threat, true );
                    } else {
                        guy.move_pause();
                    }
                } else if( field_escape ) {
                    guy.move_to( *field_escape, true );
                } else if( forced_field_escape ) {
                    guy.move_to( *forced_field_escape, true, nullptr, true );
                } else if( adjacent_non_camp_threat &&
                           !guy.has_flag( json_flag_CANNOT_ATTACK ) ) {
                    // Defense follows immediate field survival but still precedes squad routing.
                    guy.melee_attack( *threat, true );
                } else if( homeward_boundary != pair_homeward_boundary_steps.end() ) {
                    if( guy.pos_abs() == homeward_boundary->second.departure ) {
                        guy.move_pause();
                    } else {
                        const auto avoid_nonreentry =
                        [&local_step_respects_nonreentry]( const tripoint_bub_ms & step ) {
                            return !local_step_respects_nonreentry( step );
                        };
                        const pathfinding_target boundary_target = pathfinding_target::point(
                                    m.get_bub( homeward_boundary->second.departure ) );
                        const bool route_found = live_bandit_update_local_path_avoiding(
                                                     guy, boundary_target, avoid_nonreentry );
                        const bool route_safe = route_found &&
                                                local_path_respects_nonreentry( guy.path );
                        const bool next_step_cohesive = partner == nullptr || guy.path.empty() ||
                                                        step_preserves_pair_cohesion(
                                                            guy.path.front() );
                        const std::size_t path_size_before_movement = guy.path.size();
                        const tripoint_abs_ms position_before_movement = guy.pos_abs();
                        const int moves_before_movement = guy.get_moves();
                        const bool emit_route_result = log_homeward_route_result &&
                                logged_homeward_route_result_ids.insert( guy.getID() ).second;
                        const bool classify_rejection = emit_route_result && !route_found &&
                                u.has_trait( trait_DEBUG_CLAIRVOYANCE );
                        std::optional<std::string> rejection_classifier;
                        if( classify_rejection ) {
                            const pathfinding_settings &settings =
                                guy.get_pathfinding_settings( false );
                            const auto avoid_none = []( const tripoint_bub_ms & ) {
                                return false;
                            };
                            const std::vector<tripoint_bub_ms> baseline_path =
                                m.route( guy.pos_bub(), boundary_target, settings, avoid_none );
                            const std::vector<tripoint_bub_ms> npc_avoid_path =
                                m.route( guy.pos_bub(), boundary_target, settings,
                                         guy.get_path_avoid() );
                            const std::vector<tripoint_bub_ms> covert_avoid_path =
                                m.route( guy.pos_bub(), boundary_target, settings,
                                         avoid_nonreentry );
                            std::ostringstream classifier;
                            classifier << " classifier=enabled"
                                       << " baseline_found=" <<
                                       ( baseline_path.empty() ? "no" : "yes" )
                                       << " baseline_path=" << baseline_path.size()
                                       << " npc_avoid_found=" <<
                                       ( npc_avoid_path.empty() ? "no" : "yes" )
                                       << " npc_avoid_path=" << npc_avoid_path.size()
                                       << " covert_avoid_found=" <<
                                       ( covert_avoid_path.empty() ? "no" : "yes" )
                                       << " covert_avoid_path=" << covert_avoid_path.size();
                            rejection_classifier = classifier.str();
                        }
                        if( route_safe && next_step_cohesive ) {
                            guy.move_to_next();
                        } else if( route_safe ) {
                            guy.move_pause();
                        } else {
                            guy.path.clear();
                            live_bandit_move_to_omt_destination_avoiding(
                                guy, local_path_respects_nonreentry, avoid_nonreentry );
                        }
                        if( emit_route_result ) {
                            DebugLog( D_INFO, DC_ALL )
                                    << "bandit_live_world homeward_boundary_route_result"
                                    << " member=" << guy.getID().get_value()
                                    << " departure=" <<
                                    homeward_boundary->second.departure.to_string()
                                    << " exit=" << homeward_boundary->second.exit.to_string()
                                    << " route_found=" << ( route_found ? "yes" : "no" )
                                    << " route_safe=" << ( route_safe ? "yes" : "no" )
                                    << " path_before=" << path_size_before_movement
                                    << " action=" << ( route_safe ? "move_to_next" : "omt_fallback" )
                                    << " pos_before=" << position_before_movement.to_string()
                                    << " moves_before=" << moves_before_movement
                                    << " pos_after=" << guy.pos_abs().to_string()
                                    << " moves_after=" << guy.get_moves()
                                    << rejection_classifier.value_or( "" ) << '\n';
                        }
                    }
                } else if( relationship &&
                           bandit_live_world::scout_phase_requires_homeward_only(
                               relationship->phase ) ) {
                    // A pair may only leave the loaded bubble through a selected safe boundary.
                    // Before that selection becomes available, the local owner may advance along
                    // its existing loaded OMT route.  Preflight without changing NPC state so an
                    // unavailable next local route remains completely inert.
                    if( !guy.is_travelling() || !guy.has_omt_destination() ||
                        guy.omt_path.empty() ) {
                        break;
                    }
                    const tripoint_abs_omt current_omt = guy.pos_abs_omt();
                    auto next_omt = guy.omt_path.rbegin();
                    while( next_omt != guy.omt_path.rend() && *next_omt == current_omt ) {
                        ++next_omt;
                    }
                    if( next_omt == guy.omt_path.rend() ) {
                        break;
                    }
                    const tripoint_bub_ms next_center = m.get_bub(
                                project_to<coords::ms>( *next_omt ) ) + point( SEEX, SEEY );
                    const auto avoid_nonreentry =
                    [&local_step_respects_nonreentry]( const tripoint_bub_ms & step ) {
                        return !local_step_respects_nonreentry( step );
                    };
                    const pathfinding_target next_target = pathfinding_target::radius( next_center, 2 );
                    const std::function<bool( const tripoint_bub_ms & )> npc_avoid =
                        guy.get_path_avoid();
                    const auto combined_avoid = [&npc_avoid, &avoid_nonreentry](
                    const tripoint_bub_ms & step ) {
                        return npc_avoid( step ) || avoid_nonreentry( step );
                    };
                    std::vector<tripoint_bub_ms> preflight_path =
                        m.inbounds( next_center ) ? m.route(
                            guy.pos_bub(), next_target, guy.get_pathfinding_settings( false ),
                            combined_avoid ) : std::vector<tripoint_bub_ms>();
                    if( preflight_path.empty() ||
                        !local_path_respects_nonreentry( preflight_path ) ) {
                        break;
                    }
                    if( !step_preserves_pair_cohesion( preflight_path.front() ) ) {
                        guy.move_pause();
                        continue;
                    }
                    while( !guy.omt_path.empty() && guy.omt_path.back() == current_omt ) {
                        guy.omt_path.pop_back();
                    }
                    guy.path = std::move( preflight_path );
                    guy.move_to_next();
                } else if( guy.is_travelling() && guy.has_omt_destination() &&
                           !guy.has_flag( json_flag_CANNOT_MOVE ) ) {
                    const auto avoid_nonreentry =
                    [&local_step_respects_nonreentry]( const tripoint_bub_ms & step ) {
                        return !local_step_respects_nonreentry( step );
                    };
                    const bool local_route_failed =
                        !live_bandit_move_to_omt_destination_avoiding(
                            guy, local_path_respects_nonreentry, avoid_nonreentry );
                    if( local_route_failed ) {
                        if( relationship->phase ==
                            bandit_live_world::scout_phase::burned_withdrawal ) {
                            bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                        }
                    }
                } else if( ( relationship->phase ==
                             bandit_live_world::scout_phase::burned_withdrawal ||
                             relationship->phase ==
                             bandit_live_world::scout_phase::returning_exposed ) &&
                           guy.pos_abs_omt() != relationship->egress_omt &&
                           !guy.has_flag( json_flag_CANNOT_MOVE ) ) {
                    if( m.inbounds( local_egress ) &&
                        live_bandit_update_local_path( guy, local_egress ) &&
                        local_path_respects_nonreentry( guy.path ) ) {
                        guy.move_to_next();
                    } else {
                        guy.path.clear();
                        const std::optional<tripoint_bub_ms> survival_step =
                            choose_noninward_step( false );
                        if( survival_step ) {
                            guy.move_to( *survival_step, true, nullptr, true );
                            bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                        } else {
                            bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                            guy.move_pause();
                        }
                    }
                } else {
                    guy.move_pause();
                }
            } else {
                guy.move();
            }
            if( moves == guy.get_moves() ) {
                // Count every time we exit npc::move() without spending any moves.
                real_count++;
                if( has_destination == guy.has_destination_activity() || real_count > count_limit ) {
                    turns++;
                }
            }
            // Turn on debug mode when in infinite loop
            // It has to be done before the last turn, otherwise
            // there will be no meaningful debug output.
            if( turns == 9 ) {
                debugmsg( "NPC '%s' entered infinite loop, npc activity id: '%s'",
                          guy.get_name(), guy.activity.id().str() );
            }
        }

        // If we spun too long trying to decide what to do (without spending moves),
        // Invoke cognitive suspension to prevent an infinite loop.
        if( turns == 10 ) {
            add_msg( _( "%s faints!" ), guy.get_name() );
            guy.reboot();
        }

        if( !guy.is_dead() ) {
            guy.npc_update_body();
        }
    }
    complete_live_bandit_ingress_boundary_steps( pair_ingress_boundary_steps );
    complete_live_bandit_homeward_boundary_steps( pair_homeward_boundary_steps );
    g->cleanup_dead();
}

void overmap_npc_move()
{
    const auto perf_started = std::chrono::steady_clock::now();
    avatar &u = get_avatar();
    bandit_live_world::world_state &bandit_state = overmap_buffer.global_state.bandit_live_world;
    note_live_bandit_aftermath();
    complete_loaded_live_bandit_route_arrivals();
    complete_loaded_live_bandit_alternate_watch_repositions();
    bool dematerialized_handoffs = dematerialize_live_bandit_structural_handoffs();
    const auto aftermath_done = std::chrono::steady_clock::now();
    std::vector<std::string> empty_site_retirement_reports;
    bandit_live_world::retire_empty_hostile_sites( bandit_state, &empty_site_retirement_reports );
    const auto retirement_done = std::chrono::steady_clock::now();
    for( const std::string &report : empty_site_retirement_reports ) {
        DebugLog( D_INFO, DC_ALL ) << report << '\n';
    }
    const bool dispatch_cadence_due = calendar::once_every( 30_minutes );
    const bool signal_cadence_due = dispatch_cadence_due || calendar::once_every( 5_minutes );
    const bool structural_cadence_due = calendar::once_every( 60_minutes );
    std::vector<live_bandit_signal_observation> live_signals;
    std::vector<live_bandit_sound_observation> live_sounds;
    sounds::consume_significant_sounds( [&live_sounds]( const tripoint_abs_omt &source_omt,
    const int volume, const sounds::significant_sound_t kind,
    const int emitted_minutes ) {
        live_sounds.push_back( { source_omt, volume, kind, emitted_minutes } );
    } );
    if( signal_cadence_due || structural_cadence_due ) {
        bootstrap_live_bandit_abstract_sites_near_player();
    }
    if( signal_cadence_due ) {
        live_signals = observe_live_bandit_field_signals_near_player();
        signal_live_hordes_from_light_observations( live_signals );
        signal_live_zombie_riders_from_light_observations( live_signals );
    }
    const auto signal_done = std::chrono::steady_clock::now();
    if( dispatch_cadence_due || structural_cadence_due ) {
        refresh_live_bandit_member_readiness( bandit_state );
    }
    if( structural_cadence_due ) {
        maintain_live_bandit_structural_bounty( live_signals, live_sounds );
    } else if( !live_sounds.empty() ) {
        const bandit_live_world::structural_signal_record_result recorded =
            record_live_bandit_structural_sounds( bandit_state, live_sounds );
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sound_observation sites="
                                   << recorded.sites_considered
                                   << " active=" << recorded.active_outings_considered
                                   << " callbacks=" << recorded.callbacks_invoked
                                   << " recorded=" << recorded.sites_recorded
                                   << " facts=" << recorded.facts_recorded << '\n';
    }
    materialize_live_bandit_structural_handoffs();
    const auto dispatch_done = std::chrono::steady_clock::now();
    const std::set<character_id> local_pair_homeward_member_ids =
        bandit_live_world::local_pair_homeward_travel_ids( bandit_state );
    if( structural_cadence_due ) {
        log_live_bandit_homeward_motor_diagnostics( bandit_state,
                local_pair_homeward_member_ids );
    }
    const std::map<character_id, tripoint_abs_omt> local_pair_alternate_destinations =
        bandit_live_world::local_pair_alternate_watch_travel_destinations( bandit_state );
    const std::map<character_id, tripoint_abs_omt> local_pair_ingress_destinations =
        bandit_live_world::local_pair_ingress_travel_destinations( bandit_state );
    std::map<character_id, tripoint_abs_omt> local_pair_forward_destinations =
        local_pair_ingress_destinations;
    local_pair_forward_destinations.insert( local_pair_alternate_destinations.begin(),
                                            local_pair_alternate_destinations.end() );
    // A locally owned pair that has not released staging ownership must not fall through to
    // ordinary overmap travel.  In particular, a stale goal/path from before materialization can
    // otherwise advance one member (or rewrite the pair's route) while the loaded cohesion motor
    // is still assembling it.  Build this view only after the ownership preflight so malformed or
    // aliased outings fail closed with the same owner used by the local motor.
    std::map<character_id, tripoint_abs_ms> local_pair_assembly_orders;
    std::set<character_id> assembly_claimed_members;
    bool assembly_ownership_preflight_failed = false;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        if( !bandit_live_world::claim_local_pair_site_ownership(
                site, assembly_claimed_members ) ) {
            assembly_ownership_preflight_failed = true;
            break;
        }
    }
    if( !assembly_ownership_preflight_failed ) {
        for( const bandit_live_world::site_record &site : bandit_state.sites ) {
            for( const auto &order : bandit_live_world::local_pair_assembly_orders(
                     site.active_outing ) ) {
                if( !local_pair_assembly_orders.emplace( order ).second ) {
                    assembly_ownership_preflight_failed = true;
                    break;
                }
            }
            if( assembly_ownership_preflight_failed ) {
                break;
            }
        }
    }
    if( assembly_ownership_preflight_failed ) {
        local_pair_assembly_orders.clear();
    }
    const auto local_pair_member_reached_camp = [](
    const bandit_live_world::world_state & state, const character_id member_id,
    const tripoint_abs_omt & position ) {
        return std::any_of( state.sites.begin(), state.sites.end(),
        [&member_id, &position]( const bandit_live_world::site_record & site ) {
            const bandit_live_world::active_outing_state &outing = site.active_outing;
            return !site.retired_empty_site && outing.is_active() &&
                   outing.kind == bandit_live_world::outing_kind::structural_sortie &&
                   outing.owner == bandit_live_world::simulation_owner::local &&
                   bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
                   std::find( outing.member_ids.begin(), outing.member_ids.end(), member_id ) !=
                   outing.member_ids.end() &&
                   std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) ==
                   outing.casualty_ids.end() && site_contains_omt( site, position );
        } );
    };
    std::vector<npc *> travelling_npcs;
    bool local_pair_needs_reload = false;
    static constexpr int move_search_radius = 600;
    for( auto &elem : overmap_buffer.get_npcs_near_player( move_search_radius ) ) {
        if( !elem ) {
            continue;
        }
        npc *npc_to_add = elem.get();
        const auto assembly_order = local_pair_assembly_orders.find( npc_to_add->getID() );
        if( assembly_order != local_pair_assembly_orders.end() ) {
            // Assembly owns both members until the authoritative state exposes ingress,
            // alternate reposition, or homeward routing.  Drop incompatible generic travel
            // state so this NPC cannot advance or rewrite a route between local turns.
            npc_to_add->goto_to_this_pos = std::nullopt;
            npc_to_add->clear_ai_guard_pos();
            npc_to_add->path.clear();
            npc_to_add->goal = npc::no_goal_point;
            npc_to_add->omt_path.clear();
            npc_to_add->mission = NPC_MISSION_NULL;
            npc_to_add->previous_mission = NPC_MISSION_NULL;
            continue;
        }
        const auto alternate_destination = local_pair_alternate_destinations.find(
                                               npc_to_add->getID() );
        const auto forward_destination = local_pair_forward_destinations.find(
                                             npc_to_add->getID() );
        const bool locally_owned_travel_member =
            local_pair_homeward_member_ids.count( npc_to_add->getID() ) > 0 ||
            forward_destination != local_pair_forward_destinations.end();
        if( locally_owned_travel_member && npc_to_add->is_active() ) {
            if( alternate_destination != local_pair_alternate_destinations.end() &&
                npc_to_add->has_flag( json_flag_CANNOT_MOVE ) ) {
                const int current_minutes = live_bandit_current_minutes();
                const auto owner = std::find_if( bandit_state.sites.begin(),
                bandit_state.sites.end(), [&npc_to_add]( const bandit_live_world::site_record & site ) {
                    return site.active_outing.alternate_watch_reposition_pending &&
                           std::find( site.active_outing.member_ids.begin(),
                                      site.active_outing.member_ids.end(),
                                      npc_to_add->getID() ) !=
                           site.active_outing.member_ids.end();
                } );
                if( owner != bandit_state.sites.end() ) {
                    const int owner_progress = std::max(
                                                   owner->active_outing.started_minutes,
                                                   owner->active_outing.last_progress_minutes );
                    if( owner_progress >= 0 && current_minutes >= owner_progress &&
                        current_minutes - owner_progress >=
                        hostile_scout_immobility_grace_minutes &&
                        live_bandit_abort_alternate_watch_reposition(
                            npc_to_add->getID() ) ) {
                        continue;
                    }
                }
            }
            if( forward_destination != local_pair_forward_destinations.end() &&
                npc_to_add->pos_abs_omt() != forward_destination->second &&
                ( !npc_to_add->has_omt_destination() ||
                  npc_to_add->goal != forward_destination->second ||
                  npc_to_add->omt_path.empty() ) ) {
                const bandit_live_world::site_record *forward_owner = nullptr;
                for( const bandit_live_world::site_record &site : bandit_state.sites ) {
                    if( std::find( site.active_outing.member_ids.begin(),
                                  site.active_outing.member_ids.end(),
                                  npc_to_add->getID() ) !=
                        site.active_outing.member_ids.end() ) {
                        forward_owner = &site;
                        break;
                    }
                }
                const bool route_ready = forward_owner != nullptr &&
                                         live_bandit_route_member_to(
                                             *npc_to_add, *forward_owner,
                                             forward_destination->second );
                if( !route_ready ) {
                    if( alternate_destination != local_pair_alternate_destinations.end() ) {
                        live_bandit_abort_alternate_watch_reposition( npc_to_add->getID() );
                    }
                } else {
                    npc_to_add->goto_to_this_pos = std::nullopt;
                    npc_to_add->clear_ai_guard_pos();
                    npc_to_add->path.clear();
                }
            }
            // Reload an active member that crossed the current bubble, then give the complete
            // pair a second transactional dematerialization opportunity below.  Inactive members
            // remain eligible for ordinary physical overmap travel when the pair is still split;
            // both retain local ownership until they can be snapshotted together.
            local_pair_needs_reload |= !get_map().inbounds( npc_to_add->pos_abs() );
            continue;
        }
        const bool reached_owned_destination =
            ( local_pair_homeward_member_ids.count( npc_to_add->getID() ) > 0 &&
              local_pair_member_reached_camp(
                  bandit_state, npc_to_add->getID(), npc_to_add->pos_abs_omt() ) ) ||
            ( forward_destination != local_pair_forward_destinations.end() &&
              npc_to_add->pos_abs_omt() == forward_destination->second );
        if( locally_owned_travel_member && reached_owned_destination ) {
            // Hold an early arrival for the complete-pair transaction.  The generic travelling
            // fallback would clear its reached camp goal and may assign an unrelated destination.
            // An inactive arrival inside the current bubble must first be reloaded so the second
            // dematerialization opportunity can snapshot the complete pair transactionally.
            local_pair_needs_reload |= !npc_to_add->is_active() &&
                                       get_map().inbounds( npc_to_add->pos_abs() );
            continue;
        }
        if( ( !npc_to_add->is_active() ||
              rl_dist( u.pos_bub(), npc_to_add->pos_bub() ) > SEEX * 2 ) &&
            ( npc_to_add->mission == NPC_MISSION_TRAVELLING ||
              forward_destination != local_pair_forward_destinations.end() ) ) {
            travelling_npcs.push_back( npc_to_add );
        }
    }
    bool npcs_need_reload = false;
    std::set<character_id> blocked_ingress_members;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 ) {
            continue;
        }
        const auto first_destination = local_pair_ingress_destinations.find(
                                           outing.member_ids[0] );
        const auto second_destination = local_pair_ingress_destinations.find(
                                            outing.member_ids[1] );
        if( first_destination == local_pair_ingress_destinations.end() ||
            second_destination == local_pair_ingress_destinations.end() ||
            first_destination->second != second_destination->second ) {
            continue;
        }
        const auto member_is_eligible = [&travelling_npcs,
                                        &local_pair_ingress_destinations](
                                            const character_id member_id ) {
            const auto member = std::find_if( travelling_npcs.begin(), travelling_npcs.end(),
            [&member_id]( const npc *candidate ) {
                return candidate != nullptr && candidate->getID() == member_id;
            } );
            if( member == travelling_npcs.end() ) {
                return false;
            }
            const npc *candidate = *member;
            const auto destination = local_pair_ingress_destinations.find( member_id );
            return destination != local_pair_ingress_destinations.end() &&
                   candidate->is_travelling() && !candidate->has_flag( json_flag_CANNOT_MOVE ) &&
                   candidate->has_omt_destination() && candidate->goal == destination->second &&
                   !candidate->omt_path.empty();
        };
        if( !member_is_eligible( outing.member_ids[0] ) ||
            !member_is_eligible( outing.member_ids[1] ) ) {
            blocked_ingress_members.insert( outing.member_ids.begin(), outing.member_ids.end() );
        }
    }
    for( npc *&elem : travelling_npcs ) {
        if( blocked_ingress_members.count( elem->getID() ) > 0 ) {
            continue;
        }
        const bandit_live_world::site_record *local_owner = nullptr;
        const auto alternate_destination = local_pair_alternate_destinations.find(
                                               elem->getID() );
        const auto forward_destination = local_pair_forward_destinations.find(
                                             elem->getID() );
        if( local_pair_homeward_member_ids.count( elem->getID() ) > 0 ||
            forward_destination != local_pair_forward_destinations.end() ) {
            for( const bandit_live_world::site_record &site : bandit_state.sites ) {
                const bandit_live_world::active_outing_state &outing = site.active_outing;
                const bool owns_homeward_member =
                    bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
                    local_pair_homeward_member_ids.count( elem->getID() ) > 0;
                const bool owns_alternate_reposition_member =
                    outing.alternate_watch_reposition_pending &&
                    alternate_destination != local_pair_alternate_destinations.end() &&
                    outing.alternate_watch_omt == alternate_destination->second;
                const bool owns_ingress_member =
                    !outing.alternate_watch_reposition_pending &&
                    forward_destination != local_pair_forward_destinations.end() &&
                    outing.selected_watch_omt == forward_destination->second;
                if( !site.retired_empty_site && outing.is_active() &&
                    outing.kind == bandit_live_world::outing_kind::structural_sortie &&
                    outing.owner == bandit_live_world::simulation_owner::local &&
                    ( owns_homeward_member || owns_alternate_reposition_member ||
                      owns_ingress_member ) &&
                    std::find( outing.member_ids.begin(), outing.member_ids.end(), elem->getID() ) !=
                    outing.member_ids.end() &&
                    std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), elem->getID() ) ==
                    outing.casualty_ids.end() ) {
                    local_owner = &site;
                    break;
                }
            }
        }
        if( local_owner != nullptr &&
            forward_destination != local_pair_forward_destinations.end() &&
            ( !elem->has_omt_destination() ||
              elem->goal != forward_destination->second ) ) {
            if( !live_bandit_route_member_to(
                    *elem, *local_owner, forward_destination->second ) ) {
                if( alternate_destination != local_pair_alternate_destinations.end() ) {
                    live_bandit_abort_alternate_watch_reposition( elem->getID() );
                }
                continue;
            }
        }
        if( elem->has_omt_destination() ) {
            if( local_owner != nullptr && !elem->omt_path.empty() &&
                !live_bandit_route_respects_covert_ring(
                    local_owner->active_outing, elem->omt_path ) ) {
                elem->omt_path.clear();
            }
            if( !elem->omt_path.empty() ) {
                if( rl_dist( elem->omt_path.back(), elem->pos_abs_omt() ) > 2 ) {
                    // recalculate path, we got distracted doing something else probably
                    elem->omt_path.clear();
                } else if( elem->omt_path.back() == elem->pos_abs_omt() ) {
                    elem->omt_path.pop_back();
                }
            }
            if( elem->omt_path.empty() ) {
                if( local_owner != nullptr ) {
                    const bandit_live_world::scout_phase phase =
                        local_owner->active_outing.phase;
                    if( !live_bandit_route_member_to( *elem, *local_owner, elem->goal ) ) {
                        if( local_owner->active_outing.alternate_watch_reposition_pending &&
                            live_bandit_abort_alternate_watch_reposition(
                                elem->getID() ) ) {
                            continue;
                        }
                        if( phase == bandit_live_world::scout_phase::burned_withdrawal ) {
                            bandit_live_world::fail_live_covert_scout_burned_egress( elem->getID() );
                            if( live_bandit_member_routing_home( *elem, *local_owner ) ) {
                                continue;
                            }
                        }
                        live_bandit_abandon_unreachable_return( elem->getID() );
                    }
                } else {
                    elem->omt_path = overmap_buffer.get_travel_path(
                                         elem->pos_abs_omt(), elem->goal,
                                         overmap_path_params::for_npc() ).points;
                    if( elem->omt_path.empty() ) {
                        // Ordinary unowned travel retains the generic unreachable-goal behavior.
                        elem->goal = npc::no_goal_point;
                    }
                }
            } else {
                if( local_owner != nullptr && elem->has_flag( json_flag_CANNOT_MOVE ) ) {
                    const int progress_anchor = std::max(
                                                    local_owner->active_outing.started_minutes,
                                                    local_owner->active_outing.last_progress_minutes );
                    const int current_minutes = live_bandit_current_minutes();
                    if( progress_anchor >= 0 && current_minutes >= progress_anchor &&
                        current_minutes - progress_anchor >=
                        hostile_scout_immobility_grace_minutes ) {
                        if( !local_owner->active_outing.alternate_watch_reposition_pending ||
                            !live_bandit_abort_alternate_watch_reposition(
                                elem->getID() ) ) {
                            live_bandit_abandon_unreachable_return( elem->getID() );
                        }
                    }
                } else {
                    elem->travel_overmap( elem->omt_path.back() );
                    // A homeward pair that crossed the bubble remains unloaded until its
                    // complete camp-arrival snapshot can commit below.
                    npcs_need_reload |=
                        local_pair_homeward_member_ids.count( elem->getID() ) == 0;
                }
            }
        }
        if( local_pair_homeward_member_ids.count( elem->getID() ) == 0 &&
            forward_destination == local_pair_forward_destinations.end() &&
            !elem->has_omt_destination() && calendar::once_every( 1_hours ) && one_in( 3 ) ) {
            // travelling destination is reached/not set, try different one
            elem->set_omt_destination();
        }
    }
    dematerialized_handoffs |= dematerialize_live_bandit_structural_handoffs();
    if( npcs_need_reload || local_pair_needs_reload ) {
        g->reload_npcs();
    }
    complete_loaded_live_bandit_route_arrivals();
    complete_loaded_live_bandit_alternate_watch_repositions();
    dematerialized_handoffs |= dematerialize_live_bandit_structural_handoffs();
    const auto travel_done = std::chrono::steady_clock::now();

    if( signal_cadence_due || dispatch_cadence_due || !empty_site_retirement_reports.empty() ) {
        int active_sites = 0;
        std::map<std::string, int> active_job_mix;
        for( const bandit_live_world::site_record &site : bandit_state.sites ) {
            if( !site.active_outing.is_active() || site.active_outing.member_ids.empty() ) {
                continue;
            }
            active_sites++;
            const std::string profile = bandit_live_world::to_string( site.profile );
            const std::string job = site.active_outing.job_type.empty() ? "unknown" : site.active_outing.job_type;
            active_job_mix[profile + ":" + job]++;
        }
        std::ostringstream active_jobs;
        bool first_job = true;
        for( const std::pair<const std::string, int> &entry : active_job_mix ) {
            if( !first_job ) {
                active_jobs << ',';
            }
            first_job = false;
            active_jobs << entry.first << '=' << entry.second;
        }
        const auto elapsed_us = []( const auto &from, const auto &to ) {
            return std::chrono::duration_cast<std::chrono::microseconds>( to - from ).count();
        };
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world perf: sites=" << bandit_state.sites.size()
                                   << " active_sites=" << active_sites
                                   << " active_job_mix=" << ( active_job_mix.empty() ? "none" : active_jobs.str() )
                                   << " signals=" << live_signals.size()
                                   << " significant_sounds=" << live_sounds.size()
                                   << " retired_reports=" << empty_site_retirement_reports.size()
                                   << " travelling_npcs=" << travelling_npcs.size()
                                   << " npcs_need_reload=" << ( npcs_need_reload ? "yes" : "no" )
                                   << " dematerialized_handoffs="
                                   << ( dematerialized_handoffs ? "yes" : "no" )
                                   << " signal_cadence_due=" << ( signal_cadence_due ? "yes" : "no" )
                                   << " dispatch_cadence_due=" << ( dispatch_cadence_due ? "yes" : "no" )
                                   << " aftermath_us=" << elapsed_us( perf_started, aftermath_done )
                                   << " retirement_us=" << elapsed_us( aftermath_done, retirement_done )
                                   << " signal_us=" << elapsed_us( retirement_done, signal_done )
                                   << " dispatch_us=" << elapsed_us( signal_done, dispatch_done )
                                   << " travel_us=" << elapsed_us( dispatch_done, travel_done )
                                   << " total_us=" << elapsed_us( perf_started, travel_done ) << '\n';
    }
}

} // namespace

std::string live_bandit_homeward_boundary_discriminator_for_test()
{
    const bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    const std::set<character_id> homeward_member_ids =
        bandit_live_world::local_pair_homeward_travel_ids( state );
    std::map<character_id, tripoint_abs_omt> homeward_destinations;
    for( const bandit_live_world::site_record &site : state.sites ) {
        for( const character_id member_id : site.active_outing.member_ids ) {
            if( homeward_member_ids.count( member_id ) > 0 ) {
                homeward_destinations.emplace( member_id, site.anchor );
            }
        }
    }
    std::vector<std::string> discriminators;
    live_bandit_pair_boundary_steps( homeward_destinations, &discriminators, true );
    if( discriminators.empty() ) {
        return "discriminator_count=0 verdict=unavailable";
    }
    return "discriminator_count=" + std::to_string( discriminators.size() ) + ' ' +
           discriminators.front();
}

std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>>
live_bandit_homeward_boundary_steps_for_test()
{
    const bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    const std::set<character_id> homeward_member_ids =
        bandit_live_world::local_pair_homeward_travel_ids( state );
    std::map<character_id, tripoint_abs_omt> homeward_destinations;
    for( const bandit_live_world::site_record &site : state.sites ) {
        for( const character_id member_id : site.active_outing.member_ids ) {
            if( homeward_member_ids.count( member_id ) > 0 ) {
                homeward_destinations.emplace( member_id, site.anchor );
            }
        }
    }
    const std::map<character_id, live_bandit_pair_boundary_step> steps =
        live_bandit_pair_boundary_steps( homeward_destinations, nullptr, true );
    std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>> result;
    for( const auto &step : steps ) {
        result.emplace( step.first,
                        std::make_pair( step.second.departure, step.second.exit ) );
    }
    return result;
}

std::string live_bandit_homeward_unsafe_current_route_read_for_test(
    const character_id member_id )
{
    npc *member = g->find_npc( member_id );
    std::optional<bandit_live_world::covert_scout_relationship_read> relationship =
        bandit_live_world::read_active_covert_scout_homeward_member(
            overmap_buffer.global_state.bandit_live_world, member_id );
    if( member == nullptr || !relationship ) {
        return "available=no";
    }
    const std::optional<int> current_distance =
        bandit_live_world::target_footprint_watch_distance(
            member->pos_abs_omt(), relationship->target_footprint );
    if( !current_distance || *current_distance == std::numeric_limits<int>::max() ) {
        return "available=no";
    }
    relationship->minimum_target_distance = *current_distance + 1;
    const live_bandit_safe_local_route_read read = live_bandit_safe_local_route_to(
                *member, *relationship, get_map(), member->pos_abs() );
    std::ostringstream receipt;
    receipt << "available=yes current_distance=" << *current_distance
            << " minimum_distance=" << relationship->minimum_target_distance
            << " safe=" << ( read.safe ? "yes" : "no" )
            << " solved=" << ( read.solved ? "yes" : "no" )
            << " path=" << read.path_size;
    return receipt.str();
}

std::string live_bandit_homeward_partner_route_read_for_test(
    const character_id member_id, const character_id partner_id )
{
    npc *member = g->find_npc( member_id );
    const npc *partner = g->find_npc( partner_id );
    const std::optional<bandit_live_world::covert_scout_relationship_read> relationship =
        bandit_live_world::read_active_covert_scout_homeward_member(
            overmap_buffer.global_state.bandit_live_world, member_id );
    if( member == nullptr || partner == nullptr || !relationship ) {
        return "available=no";
    }
    const tripoint_bub_ms partner_position = partner->pos_bub();
    const std::function<bool( const tripoint_bub_ms & )> npc_avoid =
        member->get_path_avoid();
    const live_bandit_safe_local_route_read read = live_bandit_safe_local_route_to(
                *member, *relationship, get_map(), partner->pos_abs() );
    std::ostringstream receipt;
    receipt << "available=yes endpoint_avoided=" <<
            ( npc_avoid( partner_position ) ? "yes" : "no" )
            << " safe=" << ( read.safe ? "yes" : "no" )
            << " solved=" << ( read.solved ? "yes" : "no" )
            << " path=" << read.path_size;
    return receipt.str();
}

bool process_live_bandit_aftermath_for_test()
{
    return note_live_bandit_aftermath();
}

bool materialize_live_bandit_structural_handoffs_for_test()
{
    return materialize_live_bandit_structural_handoffs();
}

void maintain_live_bandit_local_pair_cohesion_for_test()
{
    maintain_live_bandit_local_pair_cohesion();
}

bool dematerialize_live_bandit_structural_handoffs_for_test()
{
    return dematerialize_live_bandit_structural_handoffs();
}

void process_monsters_and_npcs_turn_for_test()
{
    monmove();
}

void process_overmap_npc_move_for_test()
{
    overmap_npc_move();
}

void game::handle_progress_ui()
{
    avatar &u = get_avatar();

    // handle activity/progress/waiting UI
    const bool player_is_sleeping = u.has_effect( effect_sleep );
    bool wait_redraw = false;
    std::string wait_message;
    time_duration wait_refresh_rate;
    if( player_is_sleeping ) {
        wait_redraw = true;
        wait_message = _( "Wait till you wake up…" );
        wait_refresh_rate = 30_minutes;
    } else if( const std::optional<std::string> progress = u.activity.get_progress_message( u ) ) {
        wait_redraw = true;
        wait_message = *progress;
        if( u.activity.is_interruptible() && u.activity.interruptable_with_kb ) {
            wait_message += string_format( _( "\n%s to interrupt" ), press_x( ACTION_PAUSE ) );
        }
        if( u.activity.id() == ACT_AUTODRIVE ) {
            wait_refresh_rate = 1_turns;
        } else if( u.activity.id() == ACT_FIRSTAID ) {
            wait_refresh_rate = 5_turns;
        } else {
            wait_refresh_rate = 5_minutes;
        }
    }
    if( wait_redraw ) {
        if( first_redraw_since_waiting_started ||
            calendar::once_every( std::min( 1_minutes, wait_refresh_rate ) ) ) {
            if( first_redraw_since_waiting_started || calendar::once_every( wait_refresh_rate ) ) {
                ui_manager::redraw();
            }

            // Avoid redrawing the main UI every time due to invalidation
#ifdef TILES
            // If an ImGui window just closed and cleared the buffer, do a full
            // redraw now before blocking UIs below.
            if( cataimgui::clear_pending() ) {
                ui_manager::redraw();
            }
#endif
            ui_adaptor dummy( ui_adaptor::disable_uis_below {} );
            if( !wait_popup ) {
                wait_popup = std::make_unique<static_popup>();
            }
            wait_popup->on_top( true ).wait_message( "%s", wait_message );
            ui_manager::redraw();
            refresh_display();
            first_redraw_since_waiting_started = false;
        }
    } else {
        // Nothing to wait for now
        wait_popup_reset();
        first_redraw_since_waiting_started = true;
    }
}

bool game::do_turn()
{
    if( is_game_over() ) {
        return turn_handler::cleanup_at_end();
    }

    drain_renderer_recovery();

    weather_manager &weather = get_weather();

    // Increment game turn
    if( new_game ) {
        new_game = false;
        weather.on_game_start();
    } else {
        gamemode->per_turn();
        calendar::turn += 1_turns;
    }
    //used for dimension swapping
    if( swapping_dimensions ) {
        swapping_dimensions = false;
    }
    play_music( music::get_music_id_string() );

    // starting a new turn, clear out temperature cache
    weather.temperature_cache.clear();

    if( npcs_dirty ) {
        load_npcs();
    }

    timed_event_manager &timed_events = get_timed_events();
    timed_events.process();
    get_item_wakeups().process( calendar::turn );
    llm_intent::process_responses();
    llm_intent::enqueue_random_requests();
    mission::process_all();
    avatar &u = get_avatar();
    map &m = get_map();
    // If controlling a vehicle that is owned by someone else
    if( u.in_vehicle && u.controlling_vehicle ) {
        vehicle *veh = veh_pointer_or_null( m.veh_at( u.pos_bub() ) );
        if( veh && !veh->handle_potential_theft( u, true ) ) {
            veh->handle_potential_theft( u, false, false );
        }
    }

    // If you're inside a wall or something and haven't been telefragged, let's get you out.
    if( ( m.impassable( u.pos_bub() ) && !m.impassable_field_at( u.pos_bub() ) ) &&
        !m.has_flag( ter_furn_flag::TFLAG_CLIMBABLE, u.pos_bub() ) ) {
        u.stagger();
    }

    // If riding a horse - chance to spook
    if( u.is_mounted() ) {
        u.check_mount_is_spooked();
    }
    if( calendar::once_every( 1_days ) ) {
        overmap_buffer.process_mongroups();
    }

    // Move hordes every turn, move_hordes has its own rate limiting
    overmap_buffer.move_hordes();
    if( calendar::once_every( time_duration::from_minutes( 2.5 ) ) ) {
        if( u.has_trait( trait_HAS_NEMESIS ) ) {
            overmap_buffer.move_nemesis();
        }
    }

    debug_hour_timer.print_time();

    u.update_body();

    // Auto-save if autosave is enabled
    if( get_option<bool>( "AUTOSAVE" ) &&
        calendar::once_every( 1_turns * get_option<int>( "AUTOSAVE_TURNS" ) ) &&
        !u.is_dead_state() ) {
        autosave();
    }

    weather.update_weather();

    reset_light_level();
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
        m.set_lightmap_cache_dirty( z );
    }

    perhaps_add_random_npc( /* ignore_spawn_timers_and_rates = */ false );

    // process avatar activities (ignoring user input)
    while( u.get_moves() > 0 && u.activity ) {
        u.activity.do_turn( u );
    }

    // Process NPC sound events before they move or they hear themselves talking
    for( npc &guy : all_npcs() ) {
        if( rl_dist( guy.pos_bub(), u.pos_bub() ) < MAX_VIEW_DISTANCE ) {
            sounds::process_sound_markers( &guy );
        }
    }

    music::deactivate_music_id( music::music_id::sound );

    // Process sound events into sound markers for display to the player.
    sounds::process_sound_markers( &u );

    if( u.is_deaf() ) {
        sfx::do_hearing_loss();
    }

    // avatar processes human input through handle_action()
    if( !u.has_effect( effect_sleep ) || uquit == QUIT_WATCH ) {
        if( u.get_moves() > 0 || uquit == QUIT_WATCH ) {
            while( u.get_moves() > 0 || uquit == QUIT_WATCH ) {

                // handle_action() may cause map updates, creatures to die
                m.process_falling();
                cleanup_dead();

                mon_info_update();
                // Process any new sounds the player caused during their turn.
                for( npc &guy : all_npcs() ) {
                    if( rl_dist( guy.pos_bub(), u.pos_bub() ) < MAX_VIEW_DISTANCE ) {
                        sounds::process_sound_markers( &guy );
                    }
                }
                explosion_handler::process_explosions();
                sounds::process_sound_markers( &u );
                if( !u.activity && uquit != QUIT_WATCH
                    && ( !u.has_distant_destination() || calendar::once_every( 10_seconds ) ) ) {
                    wait_popup_reset();
                    ui_manager::redraw();
                }

                if( queue_screenshot ) {
                    take_screenshot();
                    queue_screenshot = false;
                }

                if( handle_action() ) {
                    ++moves_since_last_save;
                    u.action_taken();
                }

                if( is_game_over() ) {
                    return turn_handler::cleanup_at_end();
                }

                if( uquit == QUIT_WATCH ) {
                    break;
                }

                // avatar processes moves for activities started by handle_action()
                while( u.get_moves() > 0 && u.activity ) {
                    u.activity.do_turn( u );
                }
            }
            // Reset displayed sound markers now that the turn is over.
            // We only want this to happen if the player had a chance to examine the sounds.
            sounds::reset_markers();
        } else {
            // Rate limit key polling to 10 times a second.
            static auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() );
            const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() );
            if( ( now - start ).count() > 100 ) {
                handle_key_blocking_activity();
                start = now;
            }

            mon_info_update();

            // If player is performing a task, a monster is dangerously close,
            // and monster can reach to the player or it has some sort of a ranged attack,
            // warn them regardless of previous safemode warnings
            if( u.activity ) {
                for( std::pair<const distraction_type, std::string> &dist : u.activity.get_distractions() ) {
                    if( cancel_activity_or_ignore_query( dist.first, dist.second ) ) {
                        break;
                    }
                }
            }
        }
    }

    if( driving_view_offset.x() != 0 || driving_view_offset.y() != 0 ) {
        // Still have a view offset, but might not be driving anymore,
        // or the option has been deactivated,
        // might also happen when someone dives from a moving car.
        // or when using the handbrake.
        vehicle *veh = veh_pointer_or_null( m.veh_at( u.pos_bub() ) );
        calc_driving_offset( veh );
    }

    scent_map &scent = get_scent();
    // No-scent debug mutation has to be processed here or else it takes time to start working
    if( !u.has_flag( json_flag_NO_SCENT ) ) {
        scent.set( u.pos_bub(), u.scent, u.get_type_of_scent() );
        overmap_buffer.set_scent( u.pos_abs_omt(),  u.scent );
    }
    scent.update( u.pos_bub(), m );

    // We need floor cache before checking falling 'n stuff
    m.build_floor_caches();

    m.process_falling();
    m.vehmove();
    m.process_fields();
    m.process_items();
    explosion_handler::process_explosions();
    m.creature_in_field( u );

    // Apply sounds from previous turn to monster and NPC AI.
    sounds::process_sounds();
    const int levz = m.get_abs_sub().z();
    // Update vision caches for monsters. If this turns out to be expensive,
    // consider a stripped down cache just for monsters.
    m.build_map_cache( levz, true );

    // process monster and npc turn
    monmove();
    note_live_bandit_local_turn_sight_avoid();
    if( calendar::once_every( time_between_npc_OM_moves ) ) {
        overmap_npc_move();
    }
    m.furniture_terrain_emit_fields();
    // required after monsters move and fields emit
    mon_info_update();

    // replenish avatar moves
    u.process_turn();

    if( u.get_moves() < 0 && get_option<bool>( "FORCE_REDRAW" ) ) {
        ui_manager::redraw();
        refresh_display();
    }

    if( levz >= 0 && !u.is_underwater() ) {
        handle_weather_effects( weather.weather_id );
    }

    handle_progress_ui();

    m.invalidate_visibility_cache();

    u.update_bodytemp();
    u.update_body_wetness( *weather.weather_precise );
    u.apply_wetness_morale( weather.temperature );

    if( calendar::once_every( 1_minutes ) ) {
        u.update_morale();
        for( npc &guy : all_npcs() ) {
            guy.update_morale();
            guy.check_and_recover_morale();
        }
    }

    if( calendar::once_every( 9_turns ) ) {
        u.check_and_recover_morale();
    }

    if( !u.is_deaf() ) {
        sfx::remove_hearing_loss();
    }
    sfx::do_danger_music();
    sfx::do_vehicle_engine_sfx();
    sfx::do_vehicle_exterior_engine_sfx();
    sfx::do_low_stamina_sfx();

    // reset player noise
    u.volume = 0;

    // Calculate bionic power balance
    u.power_balance = u.get_power_level() - u.power_prev_turn;
    u.power_prev_turn = u.get_power_level();

#if defined(EMSCRIPTEN)
    // This will cause a prompt to be shown if the window is closed, until the
    // game is saved.
    EM_ASM( window.game_unsaved = true; );
#endif

    debug_menu::debug_capture::tick_if_active();
    return false;
}

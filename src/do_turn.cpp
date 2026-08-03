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

static const mtype_id mon_zombie_rider( "mon_zombie_rider" );

static const ter_str_id ter_t_flat_roof( "t_flat_roof" );
static const ter_str_id ter_t_tile_flat_roof( "t_tile_flat_roof" );

static const trait_id trait_HAS_NEMESIS( "HAS_NEMESIS" );
static const trait_id trait_NPC_STATIC_NPC( "NPC_STATIC_NPC" );

#if defined(__ANDROID__)
extern std::map<std::string, std::list<input_event>> quick_shortcuts_map;
extern bool add_best_key_for_action_to_quick_shortcuts( action_id action,
        const std::string &category, bool back );
#endif

#define dbg(x) DebugLog((x),D_GAME) << __FILE__ << ":" << __LINE__ << ": "

namespace
{
std::string live_bandit_player_target_id( const tripoint_abs_omt &player_omt )
{
    std::ostringstream out;
    out << "player@" << player_omt.x() << ',' << player_omt.y() << ',' << player_omt.z();
    return out.str();
}

std::string join_live_bandit_notes( const std::vector<std::string> &notes )
{
    std::ostringstream out;
    for( size_t i = 0; i < notes.size(); ++i ) {
        if( i > 0 ) {
            out << " | ";
        }
        out << notes[i];
    }
    return out.str();
}

bool site_contains_omt( const bandit_live_world::site_record &site, const tripoint_abs_omt &omt )
{
    return std::find( site.footprint.begin(), site.footprint.end(), omt ) != site.footprint.end();
}

static constexpr int live_bandit_basecamp_reach_radius = 30;
static constexpr int live_bandit_basecamp_storage_zone_scan_radius = live_bandit_basecamp_reach_radius * 2;
static constexpr int live_bandit_camp_adjacent_radius_submaps = 24;
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
    for( const character_id &member_id : site.active_outing.member_ids ) {
        if( site.active_outing.member_is_resolved( member_id ) ) {
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
        const bool smoke_on_sightline = live_bandit_smoke_between( here, u.pos_bub( here ), member_pos );
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

std::string live_bandit_gate_summary( const bandit_live_world::dispatch_plan &plan,
                                      const bandit_live_world::local_gate_decision &decision,
                                      const tripoint_abs_omt &dispatch_goal )
{
    std::ostringstream out;
    out << "dispatch " << bandit_live_world::to_string( decision.posture )
        << " toward " << plan.target_id
        << " via goal@" << live_bandit_omt_token( dispatch_goal );
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
    npc *trader = nullptr;
    for( const character_id &member_id : site.active_outing.member_ids ) {
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
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member != nullptr && member->last_writeback_summary.find( "shakedown_surface" ) !=
            std::string::npos ) {
            return true;
        }
    }
    return false;
}

bool live_bandit_shakedown_was_paid( const bandit_live_world::site_record &site )
{
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.last_writeback_summary.find( "shakedown_surface paid" ) != std::string::npos ) {
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
    for( const character_id &member_id : site.active_outing.member_ids ) {
        bandit_live_world::update_member_state( site, member_id,
                                                bandit_live_world::member_state::local_contact, summary );
        if( npc *member_npc = g->find_npc( member_id ) ) {
            member_npc->set_attitude( NPCATT_KILL );
        }
    }
}

std::pair<std::string, nc_color> live_bandit_shakedown_speaker( const bandit_live_world::site_record &site )
{
    for( const character_id &member_id : site.active_outing.member_ids ) {
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
           site_contains_omt( site, member_npc.goal );
}

bool live_bandit_route_member_home( npc &member_npc, const bandit_live_world::site_record &site )
{
    if( live_bandit_member_routing_home( member_npc, site ) ) {
        return true;
    }

    std::vector<tripoint_abs_omt> path = overmap_buffer.get_travel_path( member_npc.pos_abs_omt(),
                                           site.anchor, overmap_path_params::for_npc() ).points;
    if( path.empty() ) {
        return false;
    }
    member_npc.goal = site.anchor;
    member_npc.omt_path = std::move( path );
    member_npc.set_mission( NPC_MISSION_TRAVELLING );
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
                               << " active_group=" << site.active_outing.activity_id
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

bool note_live_bandit_local_turn_sight_avoid()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        if( site.retired_empty_site || !site.active_outing.is_active() ||
            site.active_outing.owner != bandit_live_world::simulation_owner::local ||
            site.active_outing.member_ids.empty() ) {
            continue;
        }

        bool has_loaded_local_contact_member = false;
        for( const character_id &member_id : site.active_outing.member_ids ) {
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
            for( const character_id &member_id : site.active_outing.member_ids ) {
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
        for( const character_id &member_id : site.active_outing.member_ids ) {
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

bool note_live_bandit_aftermath()
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    const int scout_sortie_limit_minutes = bandit_live_world::ordinary_scout_sortie_limit_minutes();
    bool changed = false;

    for( bandit_live_world::site_record &site : state.sites ) {
        if( site.retired_empty_site || !site.active_outing.is_active() || site.active_outing.member_ids.empty() ) {
            continue;
        }

        if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site ) ) {
            changed |= bandit_live_world::note_active_sortie_started(
                           site, *cursor, current_minutes );
        }

        std::vector<bandit_live_world::active_member_observation> observations;
        const std::vector<character_id> active_member_ids = site.active_outing.member_ids;
        observations.reserve( active_member_ids.size() );
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
            } else if( bandit_live_world::scout_sortie_should_return_home( site, current_minutes,
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
                    ( !member_npc->is_travelling() || !member_npc->has_omt_destination() ||
                      !site_contains_omt( site, member_npc->goal ) ) ) {
                    std::vector<tripoint_abs_omt> path = overmap_buffer.get_travel_path( member_npc->pos_abs_omt(),
                                                           site.anchor, overmap_path_params::for_npc() ).points;
                    if( !path.empty() ) {
                        member_npc->goal = site.anchor;
                        member_npc->omt_path = std::move( path );
                        member_npc->set_mission( NPC_MISSION_TRAVELLING );
                    }
                }
                if( member_npc->is_travelling() && member_npc->has_omt_destination() &&
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

        if( site.shakedown_basecamp_defender_observation_pending ) {
            const bandit_live_world::shakedown_aftermath_effect defender_effect =
                bandit_live_world::apply_shakedown_basecamp_defender_observation( site,
                        live_bandit_nearby_basecamp_defender_count( u ) );
            if( defender_effect.valid ) {
                changed = true;
                DebugLog( D_INFO, DC_ALL )
                        << "bandit shakedown aftermath: basecamp defender strength dropped from "
                        << site.shakedown_basecamp_defenders_at_fight << " to "
                        << live_bandit_nearby_basecamp_defender_count( u )
                        << "; stronger reopen available="
                        << ( site.shakedown_reopen_available ? "yes" : "no" );
            }
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

        const bool party_returning_home = std::any_of( observations.begin(), observations.end(),
        []( const bandit_live_world::active_member_observation & observation ) {
            return observation.state ==
                   bandit_live_world::active_member_observation_state::returning_home;
        } );
        if( party_returning_home &&
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
                            current_minutes );
                    if( advance ==
                        bandit_live_world::simulation_owner_transition_result::applied ) {
                        candidate.active_outing.phase =
                            bandit_live_world::scout_phase::returning_home;
                        candidate.active_outing.last_progress_minutes = current_minutes;
                        site = std::move( candidate );
                        changed = true;
                    }
                }
            }
        }

        bool unresolved_member_returning_home = false;
        bool active_group_returning_home = true;
        for( const bandit_live_world::active_member_observation &observation : observations ) {
            if( site.active_outing.member_is_resolved( observation.npc_id ) ) {
                continue;
            }
            unresolved_member_returning_home = true;
            if( observation.state !=
                bandit_live_world::active_member_observation_state::returning_home ) {
                active_group_returning_home = false;
                break;
            }
        }
        active_group_returning_home &= unresolved_member_returning_home;
        if( active_group_returning_home ) {
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world scout_sortie: returning_home -> local_gate skipped"
                    << " site=" << site.site_id
                    << " active_group=" << site.active_outing.activity_id << '\n';
            continue;
        }

        if( scout_phase_outing && bandit_live_world::scout_phase_requires_homeward_only(
                site.active_outing.phase ) ) {
            for( const character_id &member_id : site.active_outing.member_ids ) {
                if( site.active_outing.member_is_resolved( member_id ) ) {
                    continue;
                }
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    const bool was_routing_home = live_bandit_member_routing_home(
                                                      *member_npc, site );
                    const bool routes_home = live_bandit_route_member_home( *member_npc, site );
                    changed |= !was_routing_home && routes_home;
                }
            }
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world scout_sortie: homeward-only phase -> target gate skipped"
                    << " site=" << site.site_id
                    << " active_group=" << site.active_outing.activity_id
                    << " phase=" << bandit_live_world::to_string( site.active_outing.phase ) << '\n';
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

bool has_active_player_pressure( const bandit_live_world::site_record &site )
{
    return site.active_outing.is_active() && !site.active_outing.member_ids.empty() &&
           string_starts_with( site.active_outing.target_id, "player@" );
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

struct live_bandit_dispatch_candidate {
    int distance = 0;
    size_t site_index = 0;
    const live_bandit_signal_observation *signal = nullptr;
    std::string remembered_lead_id;
    std::string reason;
    int signal_distance = 0;
    int cap = 0;
};

static constexpr int live_bandit_system_envelope_omt = 40;
static constexpr int live_bandit_direct_player_range_omt = 10;
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

int live_bandit_materialize_abstract_members_for_dispatch(
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

bool note_live_bandit_structural_deaths_before_cleanup()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        if( !site.active_outing.is_active() ||
            site.active_outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            site.active_outing.schema_version < 7 ||
            site.active_outing.owner != bandit_live_world::simulation_owner::local ||
            !site.active_outing.local_handoff.is_active() ) {
            continue;
        }
        const std::vector<bandit_live_world::local_handoff_member_snapshot> snapshots =
            site.active_outing.local_handoff.members;
        for( const bandit_live_world::local_handoff_member_snapshot &snapshot : snapshots ) {
            if( snapshot.dead ) {
                continue;
            }
            npc *member = g->find_npc( snapshot.npc_id );
            if( member == nullptr || !member->is_dead() ) {
                continue;
            }
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( cursor && bandit_live_world::record_local_pair_member_death(
                    site, *cursor, snapshot.npc_id, member->pos_abs(),
                    live_bandit_current_minutes() ) ) {
                changed = true;
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_handoff physical death"
                                           << " site=" << site.site_id
                                           << " activity=" << site.active_outing.activity_id
                                           << " generation=" << site.active_outing.generation
                                           << " member=" << snapshot.npc_id.get_value()
                                           << " position=" << member->pos_abs().to_string() << '\n';
            }
        }
    }
    return changed;
}

bool maintain_live_bandit_local_pair_cohesion()
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
        const bandit_live_world::local_cohesion_plan plan =
            bandit_live_world::plan_local_pair_cohesion(
                site, *cursor, live_bandit_current_minutes(), reads );
        if( !plan.valid ) {
            continue;
        }

        struct order_backup {
            npc *member = nullptr;
            std::optional<tripoint_abs_ms> ordered_position;
            std::optional<tripoint_abs_ms> ai_guard_position;
            std::vector<tripoint_bub_ms> path;
        };
        std::vector<order_backup> backups;
        bool route_attempted = false;
        bool route_failed = false;
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
            if( !member->update_path( here.get_bub( order.second ), false, false ) ) {
                route_failed = true;
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
            continue;
        }

        changed = true;
        for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
             site.active_outing.local_handoff.members ) {
            npc *member = g->find_npc( snapshot.npc_id );
            if( member == nullptr || member->is_dead() ) {
                continue;
            }
            if( site.active_outing.local_handoff.cohesion_abort_return ) {
                member->goto_to_this_pos = std::nullopt;
                member->clear_ai_guard_pos();
                member->path.clear();
            } else if( site.active_outing.local_handoff.cohesion_assembled ) {
                member->goto_to_this_pos = std::nullopt;
                member->set_ai_guard_pos( snapshot.staging_position );
                member->path.clear();
            }
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_cohesion"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " leader=" << site.active_outing.leader_id.get_value()
                                   << " assembled=" <<
                                   ( site.active_outing.local_handoff.cohesion_assembled ? "yes" : "no" )
                                   << " failed_routes=" <<
                                   site.active_outing.local_handoff.cohesion_reroutes_used
                                   << " abort=" <<
                                   ( site.active_outing.local_handoff.cohesion_abort_return ? "yes" : "no" )
                                   << '\n';
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
            if( persisted_member == nullptr || casualty_recorded || !member ||
                member->is_dead() || member->is_active() ||
                here.inbounds( member->pos_abs() ) ) {
                preflight_failed = true;
                break;
            }
            backups.push_back( { member, member->pos_abs(), member->goal, member->omt_path,
                                 member->mission, member->previous_mission,
                                 member->goto_to_this_pos, member->get_ai_guard_pos(), member->path } );
            bandit_live_world::local_dematerialization_member_read read;
            read.npc_id = member->getID();
            read.readable = true;
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            reads.push_back( read );
        }
        if( preflight_failed ) {
            continue;
        }

        const bandit_live_world::local_dematerialization_plan plan =
            bandit_live_world::plan_local_pair_dematerialization(
                site, *cursor, live_bandit_current_minutes(), reads, outing.cargo );
        if( !plan.valid ) {
            continue;
        }
        const auto find_backup = [&backups]( const character_id member_id ) {
            return std::find_if( backups.begin(), backups.end(),
            [&member_id]( const live_bandit_local_handoff_member_backup & backup ) {
                return backup.member->getID() == member_id;
            } );
        };
        const auto quiesce_member = [&find_backup, &backups](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            if( snapshot.dead ) {
                return true;
            }
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() || backup->member->is_dead() ||
                backup->member->is_active() || backup->member->pos_abs() != snapshot.exit_position ) {
                return false;
            }
            backup->member->goal = npc::no_goal_point;
            backup->member->omt_path.clear();
            backup->member->mission = NPC_MISSION_NULL;
            backup->member->previous_mission = NPC_MISSION_NULL;
            backup->member->goto_to_this_pos = std::nullopt;
            backup->member->clear_ai_guard_pos();
            backup->member->path.clear();
            return true;
        };
        const auto rollback_member = [&find_backup, &backups](
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
        };
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
    const tripoint_abs_ms omt_origin = project_to<coords::ms>( route_position );
    const int max_local = coords::map_squares_per( coords::omt ) - 1;
    const int dx = route_position.x() - approach_from.x();
    const int dy = route_position.y() - approach_from.y();
    for( const tripoint_bub_ms &point : here.points_on_zlevel( route_position.z() ) ) {
        const tripoint_abs_ms absolute = here.get_abs( point );
        if( project_to<coords::omt>( absolute ) != route_position || !here.passable( point ) ||
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
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::abstract ||
            outing.shared_route.empty() || outing.waypoint_index <= 0 ||
            outing.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
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
        const tripoint_abs_omt route_position = outing.shared_route[static_cast<std::size_t>(
                                                    outing.waypoint_index )];
        const tripoint_abs_omt approach_from = outing.shared_route[static_cast<std::size_t>(
                                                   outing.waypoint_index - 1 )];
        const std::vector<tripoint_abs_ms> entry_positions =
            live_bandit_local_handoff_entry_positions( route_position, approach_from,
                    surviving_member_ids.size() );
        const tripoint_abs_omt egress_omt = outing.waypoint_index + 1 <
                                           static_cast<int>( outing.shared_route.size() ) ?
                                           outing.shared_route[static_cast<std::size_t>(
                                                   outing.waypoint_index + 1 )] : route_position;
        const std::vector<tripoint_abs_ms> staging_positions =
            live_bandit_local_handoff_entry_positions( route_position, egress_omt,
                    surviving_member_ids.size(), entry_positions );
        if( entry_positions.size() != surviving_member_ids.size() ||
            staging_positions.size() != surviving_member_ids.size() ) {
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
                site, *cursor, live_bandit_current_minutes(), reads );
        if( !plan.valid ) {
            continue;
        }
        const auto find_backup = [&backups]( const character_id member_id ) {
            return std::find_if( backups.begin(), backups.end(),
            [&member_id]( const live_bandit_local_handoff_member_backup & backup ) {
                return backup.member->getID() == member_id;
            } );
        };
        const auto bind_member = [&find_backup, &backups](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() ) {
                return false;
            }
            shared_ptr_fast<npc> member = overmap_buffer.remove_npc( snapshot.npc_id );
            if( !member || member != backup->member ) {
                if( member ) {
                    overmap_buffer.insert_npc( member );
                }
                return false;
            }
            member->spawn_at_precise( snapshot.entry_position );
            member->goal = npc::no_goal_point;
            member->omt_path.clear();
            member->mission = NPC_MISSION_NULL;
            member->previous_mission = NPC_MISSION_NULL;
            member->goto_to_this_pos = std::nullopt;
            member->clear_ai_guard_pos();
            member->path.clear();
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
            continue;
        }
        g->load_npcs();
        changed = true;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_handoff committed"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " generation=" << site.active_outing.generation
                                   << " epoch=" << site.active_outing.handoff_epoch
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
    const tripoint_abs_omt player_omt = u.pos_abs_omt();
    const weather_manager &weather = get_weather_const();
    for( const std::pair<const tripoint_abs_omt, live_bandit_local_source_reading> &entry : readings ) {
        const tripoint_abs_omt &source_omt = entry.first;
        const live_bandit_local_source_reading &reading = entry.second;
        bandit_mark_generation::local_field_signal_reading adapter_reading;
        adapter_reading.smoke_id = live_bandit_source_mark_id( "smoke", source_omt );
        adapter_reading.light_id = live_bandit_source_mark_id( "light", source_omt );
        adapter_reading.envelope_id = live_bandit_player_target_id( player_omt );
        adapter_reading.region_id = live_bandit_omt_token( source_omt );
        adapter_reading.observed_range_omt = rl_dist( player_omt, source_omt );
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

int refresh_live_bandit_signal_marks(
    const std::vector<live_bandit_signal_observation> &signals )
{
    if( signals.empty() ) {
        return 0;
    }

    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    if( state.sites.empty() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal maintenance skipped: "
                                   << "reason=empty_ownership_state signal_packet=yes packets="
                                   << signals.size() << '\n';
        return 0;
    }

    int matched_sites = 0;
    int refreshed_sites = 0;
    int matched_smoke_sites = 0;
    int matched_light_sites = 0;
    int rejected_by_system_range = 0;
    int rejected_by_signal_range = 0;
    int rejected_retired_empty_site = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        if( site.retired_empty_site ) {
            rejected_retired_empty_site++;
            continue;
        }
        if( rl_dist( site.anchor, u.pos_abs_omt() ) > live_bandit_system_envelope_omt ) {
            rejected_by_system_range++;
            continue;
        }

        std::vector<const live_bandit_signal_observation *> matching_signals;
        bool smoke_signal_matched = false;
        bool light_signal_matched = false;
        for( const live_bandit_signal_observation &signal : signals ) {
            const int signal_distance = rl_dist( site.anchor, signal.source_omt );
            if( signal_distance > signal.range_cap_omt ) {
                continue;
            }
            matching_signals.push_back( &signal );
            if( signal.mark.kind == "light" || signal.mark.kind == "searchlight" ) {
                light_signal_matched = true;
            } else if( signal.mark.kind == "smoke" ) {
                smoke_signal_matched = true;
            }
        }

        if( matching_signals.empty() ) {
            rejected_by_signal_range++;
            continue;
        }

        matched_sites++;
        if( light_signal_matched ) {
            matched_light_sites++;
        }
        if( smoke_signal_matched ) {
            matched_smoke_sites++;
        }
        bool site_refreshed = false;
        for( const live_bandit_signal_observation *signal : matching_signals ) {
            site_refreshed |= bandit_live_world::record_live_signal_mark( site, signal->mark );
        }
        if( site_refreshed ) {
            refreshed_sites++;
        }
    }

    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal maintenance: signal_packet=yes packets="
                               << signals.size() << " matched_sites=" << matched_sites
                               << " refreshed_sites=" << refreshed_sites
                               << " matched_smoke_sites=" << matched_smoke_sites
                               << " matched_light_sites=" << matched_light_sites
                               << " rejected_by_system_range=" << rejected_by_system_range
                               << " rejected_by_signal_range=" << rejected_by_signal_range
                               << " rejected_retired_empty_site=" << rejected_retired_empty_site
                               << " scan_radius_omt=" << live_bandit_system_envelope_omt << '\n';
    return refreshed_sites;
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
                                   const tripoint_abs_omt &target, int sight_points )
{
    const point_rel_omt offset = target.xy() - origin.xy();
    if( target.z() != origin.z() || sight_points < 0 ||
        offset.x() < -sight_points || offset.x() > sight_points ||
        offset.y() < -sight_points || offset.y() > sight_points ) {
        return false;
    }
    for( const tripoint_abs_omt &omt : line_to( origin, target ) ) {
        sight_points -= static_cast<int>( overmap_buffer.ter( omt )->get_see_cost() );
        if( sight_points < 0 ) {
            return false;
        }
    }
    return true;
}

int live_bandit_structural_observer_sight( const npc &observer,
        const tripoint_abs_omt &origin )
{
    const tripoint_abs_ms origin_ms = project_to<coords::ms>( origin );
    const weather_generator_id &weather_generator = overmap_buffer.get_settings( origin ).weather;
    const weather_type_id weather = weather_generator->get_weather_conditions(
                                        origin_ms, calendar::turn, g->get_seed() );
    const float remote_light = origin.z() < 0 ? LIGHT_AMBIENT_MINIMAL :
                               std::max<float>( LIGHT_AMBIENT_MINIMAL,
                                       sun_moon_light_at( calendar::turn ) *
                                       weather->light_multiplier + weather->light_modifier );
    int sight = observer.overmap_sight_range( remote_light, remote_light );
    sight += std::max( 0, origin.z() ) * 2;
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
    if( has_optic ) {
        sight *= 2;
    }
    const float weather_penalty = std::max( 1.0f, weather->sight_penalty );
    return std::max( 0, static_cast<int>( std::floor( sight / weather_penalty ) ) );
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
    if( outing.kind != bandit_live_world::outing_kind::structural_sortie ||
        request.party_power <= 0 || request.visible_forward_omts.size() > 3 ) {
        return result;
    }
    const shared_ptr_fast<npc> observer = overmap_buffer.find_npc( outing.leader_id );
    if( !observer || observer->is_dead() ) {
        return result;
    }
    result.local_reality = get_map().inbounds( request.current_omt );
    const int sight_points = live_bandit_structural_observer_sight( *observer,
                             request.current_omt );
    std::vector<tripoint_abs_omt> permitted_omts;
    permitted_omts.push_back( request.current_omt );
    permitted_omts.insert( permitted_omts.end(), request.visible_forward_omts.begin(),
                           request.visible_forward_omts.end() );
    for( std::size_t index = 0; index < permitted_omts.size(); ++index ) {
        const tripoint_abs_omt &omt = permitted_omts[index];
        const bool overlap = index == 0;
        if( !overlap && !live_bandit_overmap_los_from(
                request.current_omt, omt, sight_points ) ) {
            continue;
        }
        const live_bandit_omt_threat_read threat =
            live_bandit_threats_at_existing_omt( *observer, omt );
        if( threat.stable_ids.empty() || threat.danger_high <= 0 ||
            ( !overlap && threat.visible_count < HORDE_VISIBILITY_SIZE ) ) {
            continue;
        }
        result.observed = true;
        result.overlap = overlap;
        result.threat_omt = omt;
        result.danger_low = threat.danger_low;
        result.danger_high = threat.danger_high;
        result.stable_threat_ids = threat.stable_ids;
        result.summary = "ordinary OMT observer read live threat at " + omt.to_string();
        const bool hard_danger = std::min( 1000, 5 * result.danger_high ) >= 750 ||
                                 result.danger_low >= std::min( 200, 2 * request.party_power );
        if( hard_danger ) {
            result.detours = live_bandit_structural_detour_reads(
                                 site, request.current_omt, omt );
        }
        return result;
    }
    return result;
}

bandit_live_world::structural_route_read live_bandit_structural_route_read(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan )
{
    bandit_live_world::structural_route_read read;
    const auto path = overmap_buffer.get_travel_path(
                site.anchor, plan.target_omt, overmap_path_params::for_npc() );
    if( path.points.empty() || path.cost < 0 ) {
        read.summary = "live structural route solve found no passable route";
        return read;
    }

    const long long one_way_cost = ( static_cast<long long>( path.cost ) + 23 ) / 24;
    read.complete_route_cost = static_cast<int>( std::min<long long>(
                                   std::numeric_limits<int>::max(), one_way_cost * 2 ) );
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
    read.summary = read.reachable ? "live structural route solve accepted" :
                   "live structural route solve exceeded complete-route cap";
    return read;
}

bandit_live_world::structural_bounty_maintenance_result maintain_live_bandit_structural_bounty()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    static constexpr int structural_scan_budget = 12;
    static constexpr int structural_dispatch_cap = 2;
    bandit_live_world::structural_bounty_maintenance_result result =
        bandit_live_world::advance_structural_bounty_maintenance( state, live_bandit_current_minutes(),
                structural_scan_budget, structural_dispatch_cap, live_bandit_structural_terrain_id,
                live_bandit_structural_threat_read, live_bandit_structural_route_read,
                live_bandit_structural_abstract_threat_read );
    DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_structural_bounty_maintenance_report( result );
    return result;
}

bandit_live_world::camp_map_dispatch_pressure live_bandit_camp_map_dispatch_pressure(
    const bandit_live_world::camp_map_lead &lead )
{
    bandit_live_world::camp_map_dispatch_pressure pressure;
    pressure.opening_available = lead.status != bandit_live_world::camp_lead_status::active;
    pressure.opening_state = pressure.opening_available ? "opening_present_or_not_required" :
                             "no_opening_after_bounded_stalk_window";
    return pressure;
}

void note_live_bandit_no_opening_result( bandit_live_world::camp_map_lead &lead )
{
    if( lead.status != bandit_live_world::camp_lead_status::active ) {
        return;
    }
    lead.status = bandit_live_world::camp_lead_status::stale;
    lead.confidence = std::max( 0, lead.confidence - 1 );
    lead.last_checked_minutes = live_bandit_current_minutes();
    lead.last_outcome = "no_opening_return_hold_decay";
}

bool steer_live_bandit_dispatch_toward_player(
    const std::vector<live_bandit_signal_observation> &signals )
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    if( state.sites.empty() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch skipped: empty ownership state\n";
        return false;
    }

    std::vector<live_bandit_dispatch_candidate> candidate_sites;
    candidate_sites.reserve( state.sites.size() );
    int active_player_pressure = 0;
    int rejected_by_range = 0;
    int rejected_no_signal = 0;
    int rejected_retired_empty_site = 0;
    for( size_t i = 0; i < state.sites.size(); ++i ) {
        const bandit_live_world::site_record &site = state.sites[i];
        if( site.retired_empty_site ) {
            rejected_retired_empty_site++;
            continue;
        }
        if( has_active_player_pressure( site ) ) {
            active_player_pressure++;
        }
        const int distance = rl_dist( site.anchor, u.pos_abs_omt() );
        if( distance > live_bandit_system_envelope_omt ) {
            rejected_by_range++;
            continue;
        }
        if( distance <= live_bandit_direct_player_range_omt ) {
            live_bandit_dispatch_candidate candidate;
            candidate.distance = distance;
            candidate.site_index = i;
            candidate.reason = "direct_player_range";
            candidate.cap = live_bandit_direct_player_range_omt;
            candidate_sites.push_back( candidate );
            continue;
        }

        const live_bandit_signal_observation *best_signal = nullptr;
        int best_signal_distance = 0;
        for( const live_bandit_signal_observation &signal : signals ) {
            const int signal_distance = rl_dist( site.anchor, signal.source_omt );
            if( signal_distance > signal.range_cap_omt ) {
                continue;
            }
            if( best_signal == nullptr || signal_distance < best_signal_distance ) {
                best_signal = &signal;
                best_signal_distance = signal_distance;
            }
        }
        if( best_signal != nullptr ) {
            live_bandit_dispatch_candidate candidate;
            candidate.distance = distance;
            candidate.site_index = i;
            candidate.signal = best_signal;
            candidate.reason = "live_signal";
            candidate.signal_distance = best_signal_distance;
            candidate.cap = best_signal->range_cap_omt;
            candidate_sites.push_back( candidate );
        } else if( const bandit_live_world::camp_map_lead *remembered_lead =
                   bandit_live_world::find_camp_map_dispatch_lead_for_target( site, u.pos_abs_omt(),
                           live_bandit_player_target_id( u.pos_abs_omt() ) ) ) {
            live_bandit_dispatch_candidate candidate;
            candidate.distance = rl_dist( site.anchor, remembered_lead->omt );
            candidate.site_index = i;
            candidate.remembered_lead_id = remembered_lead->lead_id;
            candidate.reason = "remembered_camp_map_lead";
            candidate.cap = std::max( 2, remembered_lead->radius_omt );
            candidate_sites.push_back( candidate );
        } else if( signals.empty() ) {
            rejected_no_signal++;
        } else {
            rejected_by_range++;
        }
    }

    if( candidate_sites.empty() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch skipped: sites=" << state.sites.size()
                                   << " candidates=0 scan_radius_omt=" << live_bandit_system_envelope_omt
                                   << " signal_packet=" << ( signals.empty() ? "no" : "yes" )
                                   << " rejected_no_signal=" << rejected_no_signal
                                   << " rejected_by_range=" << rejected_by_range
                                   << " rejected_retired_empty_site=" << rejected_retired_empty_site
                                   << " direct_cap=" << live_bandit_direct_player_range_omt
                                   << " player=" << u.pos_abs_omt().to_string() << '\n';
        return false;
    }

    static constexpr int max_simultaneous_player_pressure = 2;
    if( active_player_pressure >= max_simultaneous_player_pressure ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch skipped: active_player_pressure="
                                   << active_player_pressure << " cap=" << max_simultaneous_player_pressure
                                   << " candidates=" << candidate_sites.size() << '\n';
        return false;
    }

    std::sort( candidate_sites.begin(), candidate_sites.end(),
    []( const live_bandit_dispatch_candidate & lhs, const live_bandit_dispatch_candidate & rhs ) {
        return std::tie( lhs.distance, lhs.site_index ) < std::tie( rhs.distance, rhs.site_index );
    } );
    const std::string target_id = live_bandit_player_target_id( u.pos_abs_omt() );
    bool dispatched_any = false;
    for( const live_bandit_dispatch_candidate &candidate_site : candidate_sites ) {
        if( active_player_pressure >= max_simultaneous_player_pressure ) {
            break;
        }

        bandit_live_world::site_record &site = state.sites[candidate_site.site_index];
        if( candidate_site.signal != nullptr ) {
            bandit_live_world::record_live_signal_mark( site, candidate_site.signal->mark );
        }
        if( live_bandit_shakedown_was_paid( site ) ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch rejected: site=" << site.site_id
                                       << " distance=" << candidate_site.distance
                                       << " candidate_reason=" << candidate_site.reason
                                       << " reason=paid_shakedown_cooldown\n";
            continue;
        }
        live_bandit_materialize_abstract_members_for_dispatch( state, site );
        bandit_live_world::camp_map_lead *remembered_lead = candidate_site.remembered_lead_id.empty() ?
                nullptr : site.intelligence_map.find_lead( candidate_site.remembered_lead_id );
        const bandit_live_world::camp_map_dispatch_pressure camp_map_pressure = remembered_lead != nullptr ?
                live_bandit_camp_map_dispatch_pressure( *remembered_lead ) :
                bandit_live_world::camp_map_dispatch_pressure();
        const bandit_live_world::dispatch_plan plan = remembered_lead != nullptr ?
                bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, *remembered_lead,
                        camp_map_pressure ) :
                bandit_live_world::plan_site_dispatch( site, u.pos_abs_omt(), target_id );
        if( !plan.valid ) {
            if( remembered_lead != nullptr && !camp_map_pressure.opening_available ) {
                note_live_bandit_no_opening_result( *remembered_lead );
            }
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch rejected: site=" << site.site_id
                                       << " distance=" << candidate_site.distance
                                       << " candidate_reason=" << candidate_site.reason
                                       << " signal_packet=" << ( candidate_site.signal != nullptr ?
                                               candidate_site.signal->mark.mark_id : "none" )
                                       << " remembered_lead=" << ( candidate_site.remembered_lead_id.empty() ?
                                               "none" : candidate_site.remembered_lead_id )
                                       << " opening_state=" << camp_map_pressure.opening_state
                                       << " opening_available=" << ( camp_map_pressure.opening_available ? "yes" : "no" )
                                       << " cap=" << candidate_site.cap
                                       << " notes=" << join_live_bandit_notes( plan.notes ) << '\n';
            continue;
        }

        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch plan: site=" << site.site_id
                                   << " target=" << plan.target_id
                                   << " candidate_reason=" << candidate_site.reason
                                   << " job=" << bandit_dry_run::to_string( plan.entry.job_type )
                                   << " selected_members=" << plan.member_ids.size()
                                   << " signal_packet=" << ( candidate_site.signal != nullptr ?
                                           candidate_site.signal->mark.mark_id : "none" )
                                   << " remembered_lead=" << ( candidate_site.remembered_lead_id.empty() ?
                                           "none" : candidate_site.remembered_lead_id )
                                   << " opening_state=" << camp_map_pressure.opening_state
                                   << " opening_available=" << ( camp_map_pressure.opening_available ? "yes" : "no" )
                                   << " notes=" << join_live_bandit_notes( plan.notes ) << '\n';

        std::vector<shared_ptr_fast<npc>> dispatched_npcs;
        dispatched_npcs.reserve( plan.member_ids.size() );
        bool missing_member = false;
        for( const character_id &member_id : plan.member_ids ) {
            shared_ptr_fast<npc> bandit = overmap_buffer.find_npc( member_id );
            if( !bandit ) {
                missing_member = true;
                break;
            }
            dispatched_npcs.push_back( bandit );
        }
        if( missing_member || dispatched_npcs.empty() ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch rejected: site=" << site.site_id
                                       << " distance=" << candidate_site.distance
                                       << " candidate_reason=" << candidate_site.reason
                                       << " signal_packet=" << ( candidate_site.signal != nullptr ?
                                               candidate_site.signal->mark.mark_id : "none" )
                                       << " reason=missing_concrete_member\n";
            continue;
        }

        bandit_live_world::site_record gate_site = site;
        if( !bandit_live_world::apply_dispatch_plan( gate_site, plan ) ) {
            continue;
        }
        bandit_live_world::local_gate_input gate_input = live_bandit_make_gate_input( gate_site, u );
        bandit_live_world::local_gate_decision gate_decision =
            bandit_live_world::choose_local_gate_posture( gate_site, gate_input );

        tripoint_abs_omt dispatch_goal = plan.target_omt;
        if( gate_decision.posture == bandit_live_world::local_gate_posture::hold_off ) {
            dispatch_goal = bandit_live_world::choose_hold_off_standoff_goal( gate_site.anchor,
                            plan.target_omt, 2 );
            gate_input.standoff_distance = rl_dist( dispatch_goal, plan.target_omt );
        }

        std::vector<std::vector<tripoint_abs_omt>> dispatch_paths;
        dispatch_paths.reserve( dispatched_npcs.size() );
        bool route_missing = false;
        for( const shared_ptr_fast<npc> &bandit : dispatched_npcs ) {
            std::vector<tripoint_abs_omt> path = overmap_buffer.get_travel_path( bandit->pos_abs_omt(),
                                               dispatch_goal, overmap_path_params::for_npc() ).points;
            if( path.empty() ) {
                route_missing = true;
                break;
            }
            dispatch_paths.push_back( std::move( path ) );
        }
        if( route_missing ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch rejected: site=" << site.site_id
                                       << " distance=" << candidate_site.distance
                                       << " candidate_reason=" << candidate_site.reason
                                       << " signal_packet=" << ( candidate_site.signal != nullptr ?
                                               candidate_site.signal->mark.mark_id : "none" )
                                       << " reason=route_missing\n";
            continue;
        }

        if( !bandit_live_world::apply_dispatch_plan( site, plan ) ) {
            continue;
        }
        if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site ) ) {
            bandit_live_world::note_active_sortie_started(
                site, *cursor, live_bandit_current_minutes() );
        }

        const std::string gate_summary = live_bandit_gate_summary( plan, gate_decision, dispatch_goal );
        for( const character_id &member_id : plan.member_ids ) {
            bandit_live_world::update_member_state( site, member_id,
                                                    bandit_live_world::member_state::outbound, gate_summary );
        }
        DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_local_gate_report( site, gate_input,
                                   gate_decision ) << "- live_dispatch_goal=" << live_bandit_omt_token( dispatch_goal )
                                   << "\n- live_candidate reason=" << candidate_site.reason
                                   << " distance=" << candidate_site.distance
                                   << " cap=" << candidate_site.cap
                                   << " signal_packet=" << ( candidate_site.signal != nullptr ?
                                           candidate_site.signal->mark.mark_id : "none" )
                                   << " remembered_lead=" << ( candidate_site.remembered_lead_id.empty() ?
                                           "none" : candidate_site.remembered_lead_id )
                                   << " signal_distance=" << candidate_site.signal_distance
                                   << '\n';

        for( size_t i = 0; i < dispatched_npcs.size(); ++i ) {
            const shared_ptr_fast<npc> &bandit = dispatched_npcs[i];
            bandit->goal = dispatch_goal;
            bandit->omt_path = std::move( dispatch_paths[i] );
            bandit->set_mission( NPC_MISSION_TRAVELLING );
        }
        active_player_pressure++;
        dispatched_any = true;
    }

    return dispatched_any;
}
} // namespace

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

    // Now, do active NPCs.
    maintain_live_bandit_local_pair_cohesion();
    for( npc &guy : g->all_npcs() ) {
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
            guy.move();
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
    note_live_bandit_structural_deaths_before_cleanup();
    g->cleanup_dead();
}

void overmap_npc_move()
{
    const auto perf_started = std::chrono::steady_clock::now();
    avatar &u = get_avatar();
    bandit_live_world::world_state &bandit_state = overmap_buffer.global_state.bandit_live_world;
    note_live_bandit_aftermath();
    const bool dematerialized_handoffs = dematerialize_live_bandit_structural_handoffs();
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
    if( signal_cadence_due || structural_cadence_due ) {
        bootstrap_live_bandit_abstract_sites_near_player();
    }
    if( signal_cadence_due ) {
        live_signals = observe_live_bandit_field_signals_near_player();
        refresh_live_bandit_signal_marks( live_signals );
        signal_live_hordes_from_light_observations( live_signals );
        signal_live_zombie_riders_from_light_observations( live_signals );
    }
    const auto signal_done = std::chrono::steady_clock::now();
    if( dispatch_cadence_due || structural_cadence_due ) {
        refresh_live_bandit_member_readiness( bandit_state );
    }
    if( dispatch_cadence_due ) {
        steer_live_bandit_dispatch_toward_player( live_signals );
    } else if( signal_cadence_due ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch cadence_skip: reason=30_minute_throttle"
                                   << " signal_packet=" << ( live_signals.empty() ? "no" : "yes" )
                                   << " sites=" << bandit_state.sites.size()
                                   << " dispatch_interval=30_minutes"
                                   << " signal_interval=5_minutes\n";
    }
    if( structural_cadence_due ) {
        maintain_live_bandit_structural_bounty();
    }
    materialize_live_bandit_structural_handoffs();
    const auto dispatch_done = std::chrono::steady_clock::now();
    std::vector<npc *> travelling_npcs;
    static constexpr int move_search_radius = 600;
    for( auto &elem : overmap_buffer.get_npcs_near_player( move_search_radius ) ) {
        if( !elem ) {
            continue;
        }
        npc *npc_to_add = elem.get();
        if( ( !npc_to_add->is_active() || rl_dist( u.pos_bub(), npc_to_add->pos_bub() ) > SEEX * 2 ) &&
            npc_to_add->mission == NPC_MISSION_TRAVELLING ) {
            travelling_npcs.push_back( npc_to_add );
        }
    }
    bool npcs_need_reload = false;
    for( npc *&elem : travelling_npcs ) {
        if( elem->has_omt_destination() ) {
            if( !elem->omt_path.empty() ) {
                if( rl_dist( elem->omt_path.back(), elem->pos_abs_omt() ) > 2 ) {
                    // recalculate path, we got distracted doing something else probably
                    elem->omt_path.clear();
                } else if( elem->omt_path.back() == elem->pos_abs_omt() ) {
                    elem->omt_path.pop_back();
                }
            }
            if( elem->omt_path.empty() ) {
                elem->omt_path = overmap_buffer.get_travel_path( elem->pos_abs_omt(), elem->goal,
                                 overmap_path_params::for_npc() ).points;
                if( elem->omt_path.empty() ) { // goal is unreachable, or already reached goal, reset it
                    elem->goal = npc::no_goal_point;
                }
            } else {
                elem->travel_overmap( elem->omt_path.back() );
                npcs_need_reload = true;
            }
        }
        if( !elem->has_omt_destination() && calendar::once_every( 1_hours ) && one_in( 3 ) ) {
            // travelling destination is reached/not set, try different one
            elem->set_omt_destination();
        }
    }
    if( npcs_need_reload ) {
        g->reload_npcs();
    }
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

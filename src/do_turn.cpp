#include "do_turn.h"

#if defined(EMSCRIPTEN)
#include <emscripten.h>
#endif

#include <algorithm>
#include <chrono>
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
#include <utility>
#include <vector>

#include "action.h"
#include "activity_type.h"
#include "avatar.h"
#include "bandit_live_world.h"
#include "bandit_mark_generation.h"
#include "bionics.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_variant.h"
#include "clzones.h"
#include "coordinates.h"
#include "debug.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "explosion.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "gamemode.h"
#include "help.h"
#include "input.h"
#include "input_context.h"
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
#include "mission.h"
#include "monster.h"
#include "mtype.h"
#include "music.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player_activity.h"
#include "point.h"
#include "popup.h"
#include "rng.h"
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
#include "vpart_position.h"
#include "weather.h"
#include "weather_type.h"
#include "worldfactory.h"

static const activity_id ACT_AUTODRIVE( "ACT_AUTODRIVE" );
static const activity_id ACT_FIRSTAID( "ACT_FIRSTAID" );
static const activity_id ACT_MIGRATION_CANCEL( "ACT_MIGRATION_CANCEL" );
static const activity_id ACT_OPERATION( "ACT_OPERATION" );

static const bionic_id bio_alarm( "bio_alarm" );

static const efftype_id effect_controlled( "controlled" );
static const efftype_id effect_npc_suspend( "npc_suspend" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_sleep( "sleep" );

static const event_statistic_id event_statistic_last_words( "last_words" );

static const json_character_flag json_flag_NO_SCENT( "NO_SCENT" );

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

bool live_bandit_player_at_basecamp( const avatar &u )
{
    return overmap_buffer.find_camp( u.pos_abs_omt().xy() ).has_value();
}

bool live_bandit_player_near_basecamp( const avatar &u )
{
    if( live_bandit_player_at_basecamp( u ) ) {
        return true;
    }

    static constexpr int camp_adjacent_radius_submaps = 24;
    return !overmap_buffer.get_camps_near( u.pos_abs_sm(), camp_adjacent_radius_submaps ).empty();
}

bool live_bandit_player_in_rolling_travel_scene( const avatar &u )
{
    if( u.in_vehicle && u.controlling_vehicle ) {
        return true;
    }

    return overmap_buffer.ter( u.pos_abs_omt() )->is_road();
}

bandit_live_world::local_gate_input live_bandit_make_gate_input(
    const bandit_live_world::site_record &site, const avatar &u )
{
    bandit_live_world::local_gate_input input;
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

    int closest_member_distance = rl_dist( site.anchor, u.pos_abs_omt() );
    for( const character_id &member_id : site.active_member_ids ) {
        const npc *member_npc = g->find_npc( member_id );
        if( member_npc == nullptr ) {
            continue;
        }
        const int distance = rl_dist( member_npc->pos_abs_omt(), u.pos_abs_omt() );
        closest_member_distance = std::min( closest_member_distance, distance );
        input.local_contact_established |= distance <= 1;
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
    static constexpr int basecamp_reach_radius = 30;
    for( const tripoint_bub_ms &pt : here.points_in_radius( u.pos_bub(), basecamp_reach_radius ) ) {
        if( !here.accessible_items( pt ) ) {
            continue;
        }
        for( const item &it : here.i_at( pt ) ) {
            value += live_bandit_item_value( it );
        }
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
        pool.companion_carried_value += live_bandit_character_goods_value( guy );
    }

    if( input.basecamp_or_camp_scene ) {
        pool.reachable_basecamp_value = live_bandit_nearby_ground_goods_value( u );
    } else {
        pool.vehicle_carried_value = live_bandit_current_vehicle_goods_value( u );
    }

    return pool;
}

int live_bandit_surrender_from_character( Character &who, int &remaining_value )
{
    int surrendered_value = 0;
    who.remove_items_with( [&remaining_value, &surrendered_value]( const item & it ) {
        if( remaining_value <= 0 ) {
            return false;
        }
        const int value = live_bandit_item_value( it );
        if( value <= 0 ) {
            return false;
        }
        remaining_value -= value;
        surrendered_value += value;
        return true;
    } );
    return surrendered_value;
}

int live_bandit_surrender_from_nearby_ground( const avatar &u, int &remaining_value )
{
    map &here = get_map();
    int surrendered_value = 0;
    static constexpr int basecamp_reach_radius = 30;
    for( const tripoint_bub_ms &pt : here.points_in_radius( u.pos_bub(), basecamp_reach_radius ) ) {
        if( remaining_value <= 0 ) {
            break;
        }
        if( !here.accessible_items( pt ) ) {
            continue;
        }
        map_stack stack = here.i_at( pt );
        for( map_stack::iterator it = stack.begin(); it != stack.end() && remaining_value > 0; ) {
            const int value = live_bandit_item_value( *it );
            if( value <= 0 ) {
                ++it;
                continue;
            }
            remaining_value -= value;
            surrendered_value += value;
            it = stack.erase( it );
        }
    }
    return surrendered_value;
}

int live_bandit_surrender_from_current_vehicle( const avatar &u, int &remaining_value )
{
    if( !u.in_vehicle ) {
        return 0;
    }

    map &here = get_map();
    const optional_vpart_position player_vehicle = here.veh_at( u.pos_bub() );
    if( !player_vehicle ) {
        return 0;
    }

    int surrendered_value = 0;
    vehicle &veh = player_vehicle->vehicle();
    for( const vpart_reference &part_ref : veh.get_all_parts() ) {
        if( remaining_value <= 0 ) {
            break;
        }
        vehicle_stack stack = veh.get_items( part_ref.part() );
        for( vehicle_stack::iterator it = stack.begin(); it != stack.end() && remaining_value > 0; ) {
            const int value = live_bandit_item_value( *it );
            if( value <= 0 ) {
                ++it;
                continue;
            }
            remaining_value -= value;
            surrendered_value += value;
            it = stack.erase( it );
        }
    }
    return surrendered_value;
}

int live_bandit_surrender_goods( const bandit_live_world::local_gate_input &input,
                                 const bandit_live_world::shakedown_surface &surface,
                                 avatar &u )
{
    int remaining_value = surface.demanded_value;
    int surrendered_value = live_bandit_surrender_from_character( u, remaining_value );

    static constexpr int nearby_companion_radius = 12;
    for( npc &guy : g->all_npcs() ) {
        if( remaining_value <= 0 ) {
            break;
        }
        if( !guy.is_player_ally() || rl_dist( guy.pos_abs(), u.pos_abs() ) > nearby_companion_radius ) {
            continue;
        }
        surrendered_value += live_bandit_surrender_from_character( guy, remaining_value );
    }

    if( remaining_value > 0 ) {
        if( input.basecamp_or_camp_scene ) {
            surrendered_value += live_bandit_surrender_from_nearby_ground( u, remaining_value );
        } else {
            surrendered_value += live_bandit_surrender_from_current_vehicle( u, remaining_value );
        }
    }

    return surrendered_value;
}

bool live_bandit_shakedown_already_opened( const bandit_live_world::site_record &site )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return false;
    }
    for( const character_id &member_id : site.active_member_ids ) {
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

void live_bandit_send_group_home_after_payment( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, const int surrendered_value )
{
    bandit_live_world::shakedown_outcome outcome;
    outcome.paid = true;
    outcome.basecamp_or_camp_scene = surface.includes_basecamp_inventory;
    outcome.demanded_value = surface.demanded_value;
    outcome.surrendered_value = surrendered_value;
    outcome.reachable_goods_value = surface.reachable_goods_value;
    bandit_live_world::apply_shakedown_outcome( site, outcome );

    const std::vector<character_id> member_ids = site.active_member_ids;
    const std::string summary = string_format( "shakedown_surface paid toll=%d demanded=%d reachable=%d",
                                surrendered_value, surface.demanded_value,
                                surface.reachable_goods_value );
    for( const character_id &member_id : member_ids ) {
        bandit_live_world::update_member_state( site, member_id,
                                                bandit_live_world::member_state::at_home, summary );
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
    site.active_group_id.clear();
    site.active_target_id.clear();
    site.active_member_ids.clear();
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
    for( const character_id &member_id : site.active_member_ids ) {
        bandit_live_world::update_member_state( site, member_id,
                                                bandit_live_world::member_state::local_contact, summary );
        if( npc *member_npc = g->find_npc( member_id ) ) {
            member_npc->set_attitude( NPCATT_MUG );
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

    uilist shakedown_menu;
    shakedown_menu.title = _( "Bandit shakedown" );
    shakedown_menu.text = string_format( _( "\"%1$s\"\n\nReachable goods: %2$d\nDemanded toll: %3$d\n\nChoose now: pay or fight." ),
                                         surface.bark, surface.reachable_goods_value,
                                         surface.demanded_value );
    shakedown_menu.addentry( 0, surface.pay_available, 'p', _( "Pay the demanded goods" ) );
    shakedown_menu.addentry( 1, surface.fight_available, 'f', _( "Fight" ) );
    shakedown_menu.allow_cancel = false;
    shakedown_menu.query();

    if( shakedown_menu.ret == 0 ) {
        const int surrendered_value = live_bandit_surrender_goods( input, surface, u );
        if( surrendered_value >= surface.demanded_value ) {
            add_msg( m_bad, _( "You surrender goods worth about %1$d to the bandits." ),
                     surrendered_value );
            live_bandit_send_group_home_after_payment( site, surface, surrendered_value );
            return true;
        }
        add_msg( m_warning, _( "You cannot gather enough reachable goods quickly enough.  The shakedown turns ugly." ) );
    }

    add_msg( m_bad, _( "You refuse the shakedown.  The bandits come at you." ) );
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
    if( !current_exposure && !gate_input.recent_exposure ) {
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
        candidates.push_back( candidate );
    }

    const bandit_live_world::sight_avoid_decision decision =
        bandit_live_world::choose_sight_avoid_reposition( member_npc.pos_abs(), current_exposure,
                gate_input.recent_exposure, candidates );
    if( !decision.repositions ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sight_avoid: still stalking npc="
                                   << member_npc.getID().get_value() << " reason=" << decision.reason
                                   << " current_exposure=" << ( current_exposure ? "yes" : "no" )
                                   << " player_exposure=" << ( current_player_exposure ? "yes" : "no" )
                                   << " camp_exposure=" << ( current_camp_exposure ? "yes" : "no" )
                                   << " recent_exposure=" << ( gate_input.recent_exposure ? "yes" : "no" )
                                   << '\n';
        return false;
    }

    member_npc.move_to( here.get_bub( decision.destination ), true );
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sight_avoid: exposed -> repositioned npc="
                               << member_npc.getID().get_value() << " from="
                               << current.to_string_writable() << " to="
                               << here.get_bub( decision.destination ).to_string_writable()
                               << " reason=" << decision.reason << '\n';
    return true;
}

bool note_live_bandit_aftermath()
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    static constexpr int scout_sortie_limit_minutes = 180;
    bool changed = false;

    for( bandit_live_world::site_record &site : state.sites ) {
        if( site.active_group_id.empty() || site.active_member_ids.empty() ) {
            continue;
        }

        changed |= bandit_live_world::note_active_sortie_started( site, current_minutes );

        std::vector<bandit_live_world::active_member_observation> observations;
        observations.reserve( site.active_member_ids.size() );
        for( const character_id &member_id : site.active_member_ids ) {
            bandit_live_world::active_member_observation observation;
            observation.npc_id = member_id;
            npc *member_npc = g->find_npc( member_id );
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr ) {
                observation.summary = "member record missing";
                observations.push_back( observation );
                continue;
            }
            if( member_npc == nullptr ) {
                observation.state = bandit_live_world::active_member_observation_state::missing;
                observation.summary = "member lookup missing during active outing";
                if( site.last_shakedown_outcome == "fight_unresolved" ) {
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit shakedown aftermath: active member "
                            << member_id.get_value() << " missing from npc lookup for "
                            << site.active_target_id;
                }
                if( member->state != bandit_live_world::member_state::missing ) {
                    changed |= bandit_live_world::update_member_state( site, member_id,
                              bandit_live_world::member_state::missing,
                              "outing member missing near " + site.active_target_id );
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
                            << site.active_target_id;
                }
                if( member->state != bandit_live_world::member_state::dead ) {
                    changed |= bandit_live_world::update_member_state( site, member_id,
                              bandit_live_world::member_state::dead,
                              "local contact loss near " + site.active_target_id );
                }
            } else if( site_contains_omt( site, member_npc->pos_abs_omt() ) ) {
                if( member->state == bandit_live_world::member_state::local_contact ) {
                    observation.state = bandit_live_world::active_member_observation_state::home;
                    observation.summary = "npc back on home footprint";
                } else {
                    observation.summary = "outbound member still on home footprint";
                }
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
                changed |= bandit_live_world::note_active_sortie_local_contact( site, current_minutes );
                if( member->state == bandit_live_world::member_state::outbound ) {
                    changed |= bandit_live_world::update_member_state( site, member_id,
                              bandit_live_world::member_state::local_contact,
                              "local contact near " + site.active_target_id );
                }
                if( live_bandit_member_routing_home( *member_npc, site ) ) {
                    observation.state = bandit_live_world::active_member_observation_state::returning_home;
                    observation.summary = "scout returning home after sortie limit";
                } else if( bandit_live_world::scout_sortie_should_return_home( site, current_minutes,
                           scout_sortie_limit_minutes ) && live_bandit_route_member_home( *member_npc, site ) ) {
                    observation.state = bandit_live_world::active_member_observation_state::returning_home;
                    observation.summary = "scout sortie limit reached; returning home";
                    DebugLog( D_INFO, DC_ALL )
                            << "bandit_live_world scout_sortie: linger limit reached -> return_home"
                            << " site=" << site.site_id
                            << " active_group=" << site.active_group_id
                            << " member=" << member_id.get_value()
                            << " elapsed_minutes=" << ( current_minutes -
                                    ( site.active_sortie_local_contact_minutes >= 0 ?
                                      site.active_sortie_local_contact_minutes :
                                      site.active_sortie_started_minutes ) )
                            << " limit_minutes=" << scout_sortie_limit_minutes << '\n';
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

        const bool active_group_returning_home = !observations.empty() &&
                std::all_of( observations.begin(), observations.end(),
        []( const bandit_live_world::active_member_observation & observation ) {
            return observation.state ==
                   bandit_live_world::active_member_observation_state::returning_home;
        } );
        if( active_group_returning_home ) {
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world scout_sortie: returning_home -> local_gate skipped"
                    << " site=" << site.site_id
                    << " active_group=" << site.active_group_id << '\n';
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
        if( gate_decision.posture == bandit_live_world::local_gate_posture::stalk ||
            gate_decision.posture == bandit_live_world::local_gate_posture::hold_off ) {
            for( const character_id &member_id : site.active_member_ids ) {
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    const bandit_live_world::member_record *member = site.find_member( member_id );
                    if( member != nullptr && member->state == bandit_live_world::member_state::local_contact ) {
                        changed |= live_bandit_try_sight_avoid_reposition( *member_npc, gate_input,
                                   gate_decision );
                    }
                }
            }
        }
        if( gate_decision.opens_shakedown_surface ) {
            changed |= open_live_bandit_shakedown_surface( site, gate_input, gate_decision );
            if( site.active_group_id.empty() || site.active_member_ids.empty() ) {
                continue;
            }
        }

        if( const std::optional<bandit_pursuit_handoff::return_packet> packet =
                bandit_live_world::resolve_active_group_aftermath( site, observations ) ) {
            const std::string site_id = site.site_id;
            const std::string group_id = site.active_group_id;
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
    return !site.active_group_id.empty() && !site.active_member_ids.empty() &&
           string_starts_with( site.active_target_id, "player@" );
}

struct live_bandit_signal_observation {
    bandit_live_world::live_signal_mark mark;
    bandit_mark_generation::signal_input signal;
    tripoint_abs_omt source_omt;
    int range_cap_omt = 0;
    int horde_signal_power = 0;
    std::string weather_summary;
};

struct live_bandit_dispatch_candidate {
    int distance = 0;
    size_t site_index = 0;
    const live_bandit_signal_observation *signal = nullptr;
    std::string reason;
    int signal_distance = 0;
    int cap = 0;
};

static constexpr int live_bandit_system_envelope_omt = 40;
static constexpr int live_bandit_direct_player_range_omt = 10;
static constexpr int live_bandit_local_source_scan_radius_ms = 60;

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

int live_bandit_minimum_concrete_roster_for_scout_dispatch(
    const bandit_live_world::site_record &site )
{
    const bandit_live_world::hostile_site_profile profile = site.profile ==
            bandit_live_world::hostile_site_profile::none ?
            bandit_live_world::profile_for_site_kind( site.site_kind ) : site.profile;
    switch( profile ) {
        case bandit_live_world::hostile_site_profile::camp_style:
            return 2;
        case bandit_live_world::hostile_site_profile::cannibal_camp:
            return 3;
        case bandit_live_world::hostile_site_profile::small_hostile_site:
            return 1;
        case bandit_live_world::hostile_site_profile::none:
            break;
    }
    return 1;
}

int live_bandit_materialize_abstract_members_for_dispatch(
    bandit_live_world::world_state &state, bandit_live_world::site_record &site )
{
    if( site.source_kind != bandit_live_world::anchor_source_kind::overmap_special ||
        site.source_id.empty() || site.headcount <= 0 ) {
        return 0;
    }

    const int materialized_live_members = site.count_live_members();
    const int abstract_members_remaining = site.headcount - materialized_live_members;
    const int at_home_goal = live_bandit_minimum_concrete_roster_for_scout_dispatch( site );
    const int missing_at_home_members = at_home_goal - site.count_members_in_state(
                                            bandit_live_world::member_state::at_home );
    const int members_to_create = std::min( abstract_members_remaining,
                                            std::max( 0, missing_at_home_members ) );
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
                                   << " abstract_headcount=" << site.headcount
                                   << " template=" << template_id.str() << '\n';
    }
    return created_members;
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

std::vector<live_bandit_signal_observation> observe_live_bandit_field_signals_near_player()
{
    avatar &u = get_avatar();
    map &here = get_map();
    struct source_reading {
        int fire_intensity = 0;
        int smoke_intensity = 0;
        bool outside = false;
    };
    std::map<tripoint_abs_omt, source_reading> readings;

    for( const tripoint_bub_ms &p : here.points_in_radius( u.pos_bub(),
            live_bandit_local_source_scan_radius_ms ) ) {
        const int fire_intensity = here.get_field_intensity( p, fd_fire );
        const int smoke_intensity = here.get_field_intensity( p, fd_smoke );
        if( fire_intensity <= 0 && smoke_intensity <= 0 ) {
            continue;
        }
        const tripoint_abs_omt source_omt = coords::project_to<coords::omt>( here.get_abs( p ) );
        source_reading &reading = readings[source_omt];
        reading.fire_intensity = std::max( reading.fire_intensity, fire_intensity );
        reading.smoke_intensity = std::max( reading.smoke_intensity, smoke_intensity );
        reading.outside |= here.is_outside( p );
    }

    std::vector<live_bandit_signal_observation> observations;
    observations.reserve( readings.size() * 2 );
    const bandit_mark_generation::smoke_weather_band weather_band = live_bandit_smoke_weather_band();
    const bandit_mark_generation::light_time_band light_time = live_bandit_light_time_band();
    const bandit_mark_generation::light_weather_band light_weather = live_bandit_light_weather_band();
    const tripoint_abs_omt player_omt = u.pos_abs_omt();
    for( const std::pair<const tripoint_abs_omt, source_reading> &entry : readings ) {
        const tripoint_abs_omt &source_omt = entry.first;
        const source_reading &reading = entry.second;
        bandit_mark_generation::smoke_packet packet;
        packet.id = live_bandit_source_mark_id( "smoke", source_omt );
        packet.envelope_id = live_bandit_player_target_id( player_omt );
        packet.region_id = live_bandit_omt_token( source_omt );
        packet.observed_range_omt = rl_dist( player_omt, source_omt );
        packet.source_strength = std::clamp( reading.fire_intensity + reading.smoke_intensity, 1, 3 );
        packet.persistence = reading.smoke_intensity > 0 ? 1 : 0;
        packet.height_bias = reading.fire_intensity >= 2 ? 1 : 0;
        packet.spread_bias = 0;
        packet.weather = weather_band;
        packet.notes.push_back( "live source hook: fd_fire=" + std::to_string( reading.fire_intensity ) +
                                ", fd_smoke=" + std::to_string( reading.smoke_intensity ) );
        packet.notes.push_back( "live source hook: weather=" +
                                bandit_mark_generation::to_string( weather_band ) );

        const bandit_mark_generation::smoke_projection projection =
            bandit_mark_generation::adapt_smoke_packet( packet );
        if( !projection.viable ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal rejected: packet=" << packet.id
                                       << " kind=smoke reason=below_threshold weather="
                                       << bandit_mark_generation::to_string( weather_band )
                                       << " observed_range_omt=" << packet.observed_range_omt
                                       << " projected_range_omt=" << projection.projected_range_omt
                                       << " visibility_score=" << projection.visibility_score << '\n';
        } else {
            live_bandit_signal_observation observation;
            observation.signal = projection.signal;
            observation.source_omt = source_omt;
            observation.range_cap_omt = projection.projected_range_omt;
            observation.weather_summary = projection.weather_effect.summary;
            observation.mark.mark_id = packet.id;
            observation.mark.kind = "smoke";
            observation.mark.source_omt = source_omt;
            observation.mark.observed_range_omt = packet.observed_range_omt;
            observation.mark.range_cap_omt = projection.projected_range_omt;
            observation.mark.strength = projection.signal.strength;
            observation.mark.confidence = projection.signal.confidence;
            observation.mark.bounty_add = projection.signal.bounty_add;
            observation.mark.threat_add = projection.signal.threat_add;
            observation.mark.notes = projection.signal.notes;
            observations.push_back( observation );
        }

        if( reading.fire_intensity <= 0 ) {
            continue;
        }

        bandit_mark_generation::light_packet light_packet;
        light_packet.id = live_bandit_source_mark_id( "light", source_omt );
        light_packet.envelope_id = live_bandit_player_target_id( player_omt );
        light_packet.region_id = live_bandit_omt_token( source_omt );
        light_packet.observed_range_omt = rl_dist( player_omt, source_omt );
        light_packet.source_strength = std::clamp( reading.fire_intensity, 1, 3 );
        light_packet.persistence = reading.fire_intensity >= 2 ? 1 : 0;
        light_packet.side_leakage = reading.outside ? 1 : 0;
        light_packet.time = light_time;
        light_packet.weather = light_weather;
        light_packet.exposure = reading.outside ?
                                bandit_mark_generation::light_exposure_band::exposed :
                                bandit_mark_generation::light_exposure_band::contained;
        light_packet.source = bandit_mark_generation::light_source_band::ordinary;
        light_packet.terrain = reading.outside ? bandit_mark_generation::light_terrain_band::open :
                               bandit_mark_generation::light_terrain_band::built_cover;
        light_packet.notes.push_back( "live source hook: fd_fire=" +
                                      std::to_string( reading.fire_intensity ) +
                                      ", exposure=" +
                                      bandit_mark_generation::to_string( light_packet.exposure ) );
        light_packet.notes.push_back( "live source hook: time=" +
                                      bandit_mark_generation::to_string( light_time ) +
                                      ", weather=" +
                                      bandit_mark_generation::to_string( light_weather ) );

        const bandit_mark_generation::light_projection light_projection =
            bandit_mark_generation::adapt_light_packet( light_packet );
        if( !light_projection.viable ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal rejected: packet=" << light_packet.id
                                       << " kind=light reason=below_threshold time="
                                       << bandit_mark_generation::to_string( light_time )
                                       << " weather=" << bandit_mark_generation::to_string( light_weather )
                                       << " exposure="
                                       << bandit_mark_generation::to_string( light_packet.exposure )
                                       << " observed_range_omt=" << light_packet.observed_range_omt
                                       << " projected_range_omt=" << light_projection.projected_range_omt
                                       << " visibility_score=" << light_projection.visibility_score << '\n';
            continue;
        }

        live_bandit_signal_observation light_observation;
        light_observation.signal = light_projection.signal;
        light_observation.source_omt = source_omt;
        light_observation.range_cap_omt = light_projection.projected_range_omt;
        light_observation.weather_summary = light_projection.concealment.summary;
        light_observation.mark.mark_id = light_packet.id;
        light_observation.mark.kind = light_projection.signal.kind;
        light_observation.mark.source_omt = source_omt;
        light_observation.mark.observed_range_omt = light_packet.observed_range_omt;
        light_observation.mark.range_cap_omt = light_projection.projected_range_omt;
        light_observation.mark.strength = light_projection.signal.strength;
        light_observation.mark.confidence = light_projection.signal.confidence;
        light_observation.mark.bounty_add = light_projection.signal.bounty_add;
        light_observation.mark.threat_add = light_projection.signal.threat_add;
        light_observation.mark.notes = light_projection.signal.notes;
        light_observation.horde_signal_power =
            bandit_mark_generation::horde_signal_power_from_light_projection( light_projection );
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
                                   << '\n';
    }
    return observations;
}

int bootstrap_live_bandit_abstract_sites_near_player()
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const tripoint_abs_omt center = u.pos_abs_omt();
    int created_sites = 0;
    int recognized_tiles = 0;

    const auto special_lookup = []( const tripoint_abs_omt &candidate ) -> std::optional<std::string> {
        if( const std::optional<overmap_special_id> special =
                overmap_buffer.overmap_special_at_existing( candidate ) ) {
            return special->str();
        }
        return std::nullopt;
    };

    for( int dx = -live_bandit_system_envelope_omt; dx <= live_bandit_system_envelope_omt; ++dx ) {
        for( int dy = -live_bandit_system_envelope_omt; dy <= live_bandit_system_envelope_omt; ++dy ) {
            const tripoint_abs_omt candidate( center.x() + dx, center.y() + dy, center.z() );
            if( rl_dist( center, candidate ) > live_bandit_system_envelope_omt ) {
                continue;
            }
            const std::optional<std::string> source_id = special_lookup( candidate );
            if( !source_id ) {
                continue;
            }
            const std::optional<bandit_live_world::owned_site_kind> site_kind =
                bandit_live_world::classify_tracked_source(
                    bandit_live_world::anchor_source_kind::overmap_special, *source_id );
            if( !site_kind ) {
                continue;
            }
            recognized_tiles++;
            const size_t old_site_count = state.sites.size();
            bandit_live_world::register_abstract_site( state,
                    bandit_live_world::anchor_source_kind::overmap_special, *source_id, candidate,
                    special_lookup, bandit_live_world::abstract_roster_seed_for_site_kind( *site_kind ) );
            if( state.sites.size() > old_site_count ) {
                created_sites++;
            }
        }
    }

    if( created_sites > 0 ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world abstract_bootstrap created_sites="
                                   << created_sites << " recognized_tiles=" << recognized_tiles
                                   << " scan_radius_omt=" << live_bandit_system_envelope_omt
                                   << " total_sites=" << state.sites.size() << '\n';
    }
    return created_sites;
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
    for( bandit_live_world::site_record &site : state.sites ) {
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
                                   << " horde_signal_power=" << signal.horde_signal_power
                                   << " range_cap_omt=" << signal.range_cap_omt
                                   << " weather=" << signal.weather_summary << '\n';
    }
    return signaled_sources;
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
    for( size_t i = 0; i < state.sites.size(); ++i ) {
        const bandit_live_world::site_record &site = state.sites[i];
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
        const bandit_live_world::dispatch_plan plan =
            bandit_live_world::plan_site_dispatch( site, u.pos_abs_omt(), target_id );
        if( !plan.valid ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch rejected: site=" << site.site_id
                                       << " distance=" << candidate_site.distance
                                       << " candidate_reason=" << candidate_site.reason
                                       << " signal_packet=" << ( candidate_site.signal != nullptr ?
                                               candidate_site.signal->mark.mark_id : "none" )
                                       << " cap=" << candidate_site.cap
                                       << " notes=" << join_live_bandit_notes( plan.notes ) << '\n';
            continue;
        }

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

        tripoint_abs_omt dispatch_goal = u.pos_abs_omt();
        if( gate_decision.posture == bandit_live_world::local_gate_posture::hold_off ) {
            dispatch_goal = bandit_live_world::choose_hold_off_standoff_goal( gate_site.anchor,
                            u.pos_abs_omt(), 2 );
            gate_input.standoff_distance = rl_dist( dispatch_goal, u.pos_abs_omt() );
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
        bandit_live_world::note_active_sortie_started( site, live_bandit_current_minutes() );

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
    g->cleanup_dead();
}

void overmap_npc_move()
{
    avatar &u = get_avatar();
    note_live_bandit_aftermath();
    const bool dispatch_cadence_due = calendar::once_every( 30_minutes );
    const bool signal_cadence_due = dispatch_cadence_due || calendar::once_every( 5_minutes );
    std::vector<live_bandit_signal_observation> live_signals;
    if( signal_cadence_due ) {
        bootstrap_live_bandit_abstract_sites_near_player();
        live_signals = observe_live_bandit_field_signals_near_player();
        refresh_live_bandit_signal_marks( live_signals );
        signal_live_hordes_from_light_observations( live_signals );
    }
    if( dispatch_cadence_due ) {
        steer_live_bandit_dispatch_toward_player( live_signals );
    } else if( signal_cadence_due ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world dispatch cadence_skip: reason=30_minute_throttle"
                                   << " signal_packet=" << ( live_signals.empty() ? "no" : "yes" )
                                   << " sites=" << overmap_buffer.global_state.bandit_live_world.sites.size()
                                   << " dispatch_interval=30_minutes"
                                   << " signal_interval=5_minutes\n";
    }
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
}

} // namespace

// MAIN GAME LOOP
// Returns true if game is over (death, saved, quit, etc)
bool do_turn()
{
    if( g->is_game_over() ) {
        return turn_handler::cleanup_at_end();
    }

    weather_manager &weather = get_weather();
    // Actual stuff
    if( g->new_game ) {
        g->new_game = false;
        if( get_option<std::string>( "ETERNAL_WEATHER" ) != "normal" ) {
            weather.weather_override = static_cast<weather_type_id>
                                       ( get_option<std::string>( "ETERNAL_WEATHER" ) );
            weather.set_nextweather( calendar::turn );
        } else {
            weather.weather_override = WEATHER_NULL;
            weather.set_nextweather( calendar::turn );
        }
    } else {
        g->gamemode->per_turn();
        calendar::turn += 1_turns;
    }
    //used for dimension swapping
    if( g->swapping_dimensions ) {
        g->swapping_dimensions = false;
    }
    play_music( music::get_music_id_string() );

    // starting a new turn, clear out temperature cache
    weather.temperature_cache.clear();

    if( g->npcs_dirty ) {
        g->load_npcs();
    }

    timed_event_manager &timed_events = get_timed_events();
    timed_events.process();
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

    g->debug_hour_timer.print_time();

    u.update_body();

    // Auto-save if autosave is enabled
    if( get_option<bool>( "AUTOSAVE" ) &&
        calendar::once_every( 1_turns * get_option<int>( "AUTOSAVE_TURNS" ) ) &&
        !u.is_dead_state() ) {
        g->autosave();
    }

    weather.update_weather();
    g->reset_light_level();

    g->perhaps_add_random_npc( /* ignore_spawn_timers_and_rates = */ false );
    while( u.get_moves() > 0 && u.activity ) {
        u.activity.do_turn( u );
    }

    // Process NPC sound events before they move or they hear themselves talking
    for( npc &guy : g->all_npcs() ) {
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

    if( !u.has_effect( effect_sleep ) || g->uquit == QUIT_WATCH ) {
        if( u.get_moves() > 0 || g->uquit == QUIT_WATCH ) {
            while( u.get_moves() > 0 || g->uquit == QUIT_WATCH ) {
                m.process_falling();
                g->cleanup_dead();
                g->mon_info_update();
                // Process any new sounds the player caused during their turn.
                for( npc &guy : g->all_npcs() ) {
                    if( rl_dist( guy.pos_bub(), u.pos_bub() ) < MAX_VIEW_DISTANCE ) {
                        sounds::process_sound_markers( &guy );
                    }
                }
                explosion_handler::process_explosions();
                sounds::process_sound_markers( &u );
                if( !u.activity && g->uquit != QUIT_WATCH
                    && ( !u.has_distant_destination() || calendar::once_every( 10_seconds ) ) ) {
                    g->wait_popup_reset();
                    ui_manager::redraw();
                }

                if( g->queue_screenshot ) {
                    g->take_screenshot();
                    g->queue_screenshot = false;
                }

                if( g->handle_action() ) {
                    ++g->moves_since_last_save;
                    u.action_taken();
                }

                if( g->is_game_over() ) {
                    return turn_handler::cleanup_at_end();
                }

                if( g->uquit == QUIT_WATCH ) {
                    break;
                }
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

            g->mon_info_update();

            // If player is performing a task, a monster is dangerously close,
            // and monster can reach to the player or it has some sort of a ranged attack,
            // warn them regardless of previous safemode warnings
            if( u.activity ) {
                for( std::pair<const distraction_type, std::string> &dist : u.activity.get_distractions() ) {
                    if( g->cancel_activity_or_ignore_query( dist.first, dist.second ) ) {
                        break;
                    }
                }
            }
        }
    }

    if( g->driving_view_offset.x() != 0 || g->driving_view_offset.y() != 0 ) {
        // Still have a view offset, but might not be driving anymore,
        // or the option has been deactivated,
        // might also happen when someone dives from a moving car.
        // or when using the handbrake.
        vehicle *veh = veh_pointer_or_null( m.veh_at( u.pos_bub() ) );
        g->calc_driving_offset( veh );
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
    monmove();
    if( calendar::once_every( time_between_npc_OM_moves ) ) {
        overmap_npc_move();
    }
    if( calendar::once_every( 10_seconds ) ) {
        for( const tripoint_bub_ms &elem : m.get_furn_field_locations() ) {
            const furn_t &furn = *m.furn( elem );
            for( const emit_id &e : furn.emissions ) {
                m.emit_field( elem, e );
            }
        }
        for( const tripoint_bub_ms &elem : m.get_ter_field_locations() ) {
            const ter_t &ter = *m.ter( elem );
            for( const emit_id &e : ter.emissions ) {
                m.emit_field( elem, e );
            }
        }
    }
    g->mon_info_update();
    u.process_turn();
    if( u.get_moves() < 0 && get_option<bool>( "FORCE_REDRAW" ) ) {
        ui_manager::redraw();
        refresh_display();
    }

    if( levz >= 0 && !u.is_underwater() ) {
        handle_weather_effects( weather.weather_id );
    }

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
        if( g->first_redraw_since_waiting_started ||
            calendar::once_every( std::min( 1_minutes, wait_refresh_rate ) ) ) {
            if( g->first_redraw_since_waiting_started || calendar::once_every( wait_refresh_rate ) ) {
                ui_manager::redraw();
            }

            // Avoid redrawing the main UI every time due to invalidation
            ui_adaptor dummy( ui_adaptor::disable_uis_below {} );
            if( !g->wait_popup ) {
                g->wait_popup = std::make_unique<static_popup>();
            }
            g->wait_popup->on_top( true ).wait_message( "%s", wait_message );
            ui_manager::redraw();
            refresh_display();
            g->first_redraw_since_waiting_started = false;
        }
    } else {
        // Nothing to wait for now
        g->wait_popup_reset();
        g->first_redraw_since_waiting_started = true;
    }

    m.invalidate_visibility_cache();

    u.update_bodytemp();
    u.update_body_wetness( *weather.weather_precise );
    u.apply_wetness_morale( weather.temperature );

    if( calendar::once_every( 1_minutes ) ) {
        u.update_morale();
        for( npc &guy : g->all_npcs() ) {
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

    return false;
}

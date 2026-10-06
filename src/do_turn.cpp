#include "do_turn.h"

#if defined(EMSCRIPTEN)
#include <emscripten.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
#include <type_traits>
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
#include "physical_light.h"
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
#include "debug_menu.h"
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
#include "json.h"
#include "magic_enchantment.h"
#include "map.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "mapbuffer.h"
#include "scout_observation.h"
#include "mapdata.h"
#include "memorial_logger.h"
#include "messages.h"
#include "llm_intent.h"
#include "line.h"
#include "lightmap.h"
#include "live_light.h"
#include "mission.h"
#include "monster.h"
#include "mongroup.h"
#include "mtype.h"
#include "writhing_stalker_ai.h"
#include "music.h"
#include "npc.h"
#include "npctrade.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "pathfinding.h"
#include "raid_decision_trace.h"
#include "pimpl.h"
#include "player_activity.h"
#include "point.h"
#include "popup.h"
#include "rng.h"
#include "regional_settings.h"
#include "scent_map.h"
#include "sdlsound.h"
#include "semantic_surface.h"
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
#include "uistate.h"
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
static const bionic_id bio_sleep_shutdown( "bio_sleep_shutdown" );

static const efftype_id effect_controlled( "controlled" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_npc_flee_player( "npc_flee_player" );
static const efftype_id effect_npc_run_away( "npc_run_away" );
static const efftype_id effect_npc_suspend( "npc_suspend" );
static const efftype_id effect_onfire( "onfire" );
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
static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const species_id species_ZOMBIE( "ZOMBIE" );

static const ter_str_id ter_t_flat_roof( "t_flat_roof" );
static const ter_str_id ter_t_tile_flat_roof( "t_tile_flat_roof" );

static const trait_id trait_DEBUG_CLAIRVOYANCE( "DEBUG_CLAIRVOYANCE" );
static const trait_id trait_HAS_NEMESIS( "HAS_NEMESIS" );
static const trait_id trait_NPC_STATIC_NPC( "NPC_STATIC_NPC" );

static bool openclaw_harness_startup_boundary_trace_enabled()
{
    const char *const trace_enabled = std::getenv( "OPENCLAW_HARNESS_UI_TRACE" );
    const char *const active_run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    return trace_enabled != nullptr && trace_enabled[0] != '\0' && trace_enabled[0] != '0' &&
           active_run_id != nullptr && active_run_id[0] != '\0';
}

// The semantic trace is also the run-bound journal for performance facts.  Do
// not use DebugLog timestamps for this: a delayed flush is transport evidence,
// not the duration of the advancing simulation turn.
static std::string openclaw_harness_turn_trace_quote( const std::string &value )
{
    std::ostringstream quoted;
    for( const char character : value ) {
        switch( character ) {
            case '\\':
                quoted << "\\\\";
                break;
            case '"':
                quoted << "\\\"";
                break;
            case '\n':
                quoted << "\\n";
                break;
            case '\r':
                quoted << "\\r";
                break;
            case '\t':
                quoted << "\\t";
                break;
            default:
                quoted << character;
                break;
        }
    }
    return quoted.str();
}

static bool openclaw_harness_turn_trace_enabled()
{
    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const char *const semantic_run_id = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" );
    const char *const path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" );
    return run_id != nullptr && semantic_run_id != nullptr && path != nullptr && run_id[0] != '\0' &&
           semantic_run_id[0] != '\0' && path[0] != '\0' && std::string( run_id ) == semantic_run_id;
}

static void openclaw_harness_write_turn_trace( const char *event, const char *stage,
        const char *phase, const std::string &turn_id, int game_turn, int game_minutes,
        double simulation_seconds )
{
    if( !openclaw_harness_turn_trace_enabled() ) {
        return;
    }
    const char *const path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" );
    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    static const auto process_instance = std::chrono::system_clock::now().time_since_epoch().count();
    const player_activity &activity = get_avatar().activity;
    const size_t waiting_generation = activity.id().str().compare( 0, 8, "ACT_WAIT" ) == 0 ?
                                      activity.input_generation() : 0;
    static std::uint64_t sequence = 0;
    const auto wall_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                               std::chrono::system_clock::now().time_since_epoch() ).count();
    std::ofstream stream( path, std::ios::app | std::ios::binary );
    if( stream ) {
        // The native wall clock is used to distinguish a newly resumed
        // simulation slice from time spent at an input/save boundary.  The
        // default six significant digits would quantize epoch timestamps by
        // thousands of seconds, so retain a round-trippable double here.
        stream << std::setprecision( std::numeric_limits<double>::max_digits10 );
        stream << "openclaw_harness_semantic_step: {\"event\":\"" << event
               << "\",\"stage\":\"" << stage
               << "\",\"run_id\":\"" << openclaw_harness_turn_trace_quote( run_id )
               << "\",\"process_instance\":\"" << process_instance
               << "\",\"sequence\":" << ++sequence
               << ",\"turn_id\":\"" << turn_id
               << "\",\"game_turn\":" << game_turn
               << ",\"game_minutes\":" << game_minutes
               << ",\"wall_time_seconds\":" << wall_time
               << ",\"simulation_seconds\":" << simulation_seconds
               << ",\"waiting_generation\":" << waiting_generation
               << ",\"phase\":\"" << phase
               << "\",\"owner\":\"game::do_turn\"}\n";
    }
}

class openclaw_harness_turn_trace
{
    public:
        openclaw_harness_turn_trace( bool advancing, int game_turn, int game_minutes ) :
            enabled_( advancing && openclaw_harness_turn_trace_enabled() ), game_turn_( game_turn ),
            game_minutes_( game_minutes ), started_( std::chrono::steady_clock::now() ) {
            if( enabled_ ) {
                static std::uint64_t next_turn_id = 0;
                turn_id_ = std::to_string( ++next_turn_id );
                openclaw_harness_write_turn_trace( "turn", "start", "simulation", turn_id_, game_turn_, game_minutes_, 0.0 );
            }
        }

        ~openclaw_harness_turn_trace() {
            if( enabled_ ) {
                const double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                                           std::chrono::steady_clock::now() - started_ ).count();
                const double open_outside_simulation = outside_simulation_started_ ?
                    std::chrono::duration_cast<std::chrono::duration<double>>(
                        std::chrono::steady_clock::now() - *outside_simulation_started_ ).count() : 0.0;
                openclaw_harness_write_turn_trace( "turn", "end", "simulation", turn_id_,
                        to_turns<int>( calendar::turn - calendar::turn_zero ),
                        to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ),
                        std::max( 0.0, elapsed - outside_simulation_seconds_ - open_outside_simulation ) );
            }
        }

        void input_begin() {
            phase_begin( "input" );
        }

        void input_end() {
            phase_end( "input" );
        }

        void phase_begin( const char *phase ) {
            if( enabled_ && !outside_simulation_started_ ) {
                outside_simulation_started_ = std::chrono::steady_clock::now();
                outside_simulation_phase_ = phase;
                openclaw_harness_write_turn_trace( "turn_phase", "begin", phase, turn_id_, game_turn_, game_minutes_, 0.0 );
            }
        }

        void phase_end( const char *phase ) {
            if( enabled_ && outside_simulation_started_ && outside_simulation_phase_ == phase ) {
                outside_simulation_seconds_ += std::chrono::duration_cast<std::chrono::duration<double>>(
                                      std::chrono::steady_clock::now() - *outside_simulation_started_ ).count();
                outside_simulation_started_.reset();
                outside_simulation_phase_.clear();
                openclaw_harness_write_turn_trace( "turn_phase", "end", "simulation", turn_id_, game_turn_, game_minutes_, 0.0 );
            }
        }

    private:
        bool enabled_ = false;
        int game_turn_ = 0;
        int game_minutes_ = 0;
        std::string turn_id_;
        std::chrono::steady_clock::time_point started_;
        std::optional<std::chrono::steady_clock::time_point> outside_simulation_started_;
        std::string outside_simulation_phase_;
        double outside_simulation_seconds_ = 0.0;
};

static void openclaw_harness_trace_post_hud_pre_input_boundary( const avatar &player,
        bool watching_dead_avatar )
{
    if( !openclaw_harness_startup_boundary_trace_enabled() ) {
        return;
    }

    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    input_context ctxt = get_default_mode_input_context();
    const bool ordinary_input_candidate = !player.is_dead_state() && !watching_dead_avatar &&
                                          !player.activity && !player.has_destination() &&
                                          !player.has_destination_activity() && !uistate.open_menu;
    DebugLog( D_INFO, DC_ALL )
            << "openclaw_harness_startup_boundary: event=post_hud_pre_input"
            << " run_id=\"" << run_id << '\"'
            << " avatar_live=" << ( player.is_dead_state() ? "no" : "yes" )
            << " modal_owner=" << ( uistate.open_menu ? "queued_menu" : "none" )
            << " world_wait_available=" << ( ctxt.first_keyboard_character_for_action( "wait" ) ? "yes" : "no" )
            << " ordinary_input_candidate=" << ( ordinary_input_candidate ? "yes" : "no" );
}

// Keep this at the native action/activity boundary.  A semantic wait can be accepted
// before the activity consumes moves, so the harness needs both sides of each call to
// distinguish a dropped return from an activity that simply did not advance.
static void openclaw_harness_trace_activity_driver( const char *event, const avatar &player,
        std::optional<bool> handle_action_return = std::nullopt )
{
    if( !openclaw_harness_startup_boundary_trace_enabled() ) {
        return;
    }

    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    DebugLog( D_INFO, DC_ALL )
            << "openclaw_harness_activity_driver: event=" << event
            << " run_id=\"" << run_id << '\"'
            << " moves=" << player.get_moves()
            << " activity_present=" << ( player.activity ? "yes" : "no" )
            << " activity_type=" << ( player.activity ? player.activity.id().str() : "none" )
            << " handle_action_return="
            << ( !handle_action_return ? "not_applicable" :
                 ( *handle_action_return ? "true" : "false" ) );
}

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
std::string live_bandit_homeward_boundary_collection_for_test();
bool live_bandit_retained_homeward_resume_blocks_generic_travel(
    const bandit_live_world::active_outing_state &outing, bool materialization_has_pair_slots );

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
bool live_bandit_member_can_take_homeward_step( const npc &member );


bandit_live_world_projection_lease live_bandit_projection_lease(
    const std::string &site_id, const bandit_live_world::active_outing_state &outing )
{
    return { true, site_id, outing.activity_id, "local", outing.generation,
             outing.handoff_epoch, outing.last_advanced_minutes };
}

bandit_live_world_projection_lease live_bandit_projection_lease(
    const std::string &site_id, const bandit_live_world::local_handoff_snapshot &snapshot )
{
    return { true, site_id, snapshot.activity_id, "local", snapshot.activity_generation,
             snapshot.handoff_epoch, snapshot.committed_minutes };
}

bool live_bandit_projection_lease_matches( const bandit_live_world_projection_lease &claim,
        const bandit_live_world_projection_lease &expected )
{
    return claim.present == expected.present && ( !claim.present ||
            ( claim.site_id == expected.site_id && claim.activity_id == expected.activity_id &&
              claim.owner == expected.owner && claim.generation == expected.generation &&
              claim.handoff_epoch == expected.handoff_epoch &&
              claim.last_advanced_minutes == expected.last_advanced_minutes ) );
}

bool live_bandit_projection_copies_match( npc &member,
        const bandit_live_world_projection_lease &expected )
{
    const auto copies = bandit_live_world_projection_lease_copies( member );
    return copies.size() <= 2 && std::all_of( copies.begin(), copies.end(),
    [&expected]( const npc * copy ) {
        return live_bandit_projection_lease_matches(
                   copy->get_bandit_live_world_projection_lease(), expected );
    } );
}

void reset_live_bandit_local_projection( npc &member,
        const bandit_live_world_projection_lease &lease )
{
    member.goal = npc::no_goal_point;
    member.omt_path.clear();
    member.mission = NPC_MISSION_NULL;
    member.previous_mission = NPC_MISSION_NULL;
    member.goto_to_this_pos = std::nullopt;
    member.clear_ai_guard_pos();
    member.path.clear();
    sync_bandit_live_world_projection_lease_copies( member, lease );
}

int live_bandit_current_minutes();
bool live_bandit_confirmed_member_death(
    const bandit_live_world::active_outing_state &reservation,
    character_id member_id, int current_minutes );

// The transfer callbacks own physical bind/quiesce rollback.  This writer owns
// just the persisted claims: validate every copy before changing any of them.
// A local progress commit uses the previous exact cursor, never a <= clock rule.
bool persist_live_bandit_local_projection_leases( const bandit_live_world::site_record &site,
        const bandit_live_world::site_record *previous = nullptr, const bool apply = true,
        const bool restore_previous_cursor = false,
        const bool allow_confirmed_missing_deaths = false,
        const bool allow_deadline_missing_resolution = false,
        const std::optional<character_id> confirmed_cleanup_death = {} )
{
    const auto *next = site.active_external_outing();
    const auto *prior = previous == nullptr ? nullptr : previous->active_external_outing();
    const bool was_local = prior != nullptr &&
                           prior->owner == bandit_live_world::simulation_owner::local;
    const bool is_local = next != nullptr && next->is_active() &&
                          next->owner == bandit_live_world::simulation_owner::local;
    if( !was_local && !is_local ) {
        return true;
    }
    if( is_local && !bandit_live_world::current_external_simulation_cursor( site ) ) {
        return false;
    }
    const bool hostile_owner_transition = next != nullptr && prior != nullptr &&
                                          next->kind == bandit_live_world::outing_kind::hostile_operation &&
                                          previous->site_id == site.site_id && next->activity_id == prior->activity_id &&
                                          next->generation == prior->generation &&
                                          next->last_advanced_minutes >= prior->last_advanced_minutes &&
                                          prior->kind == next->kind && next->member_ids == prior->member_ids &&
                                          next->handoff_epoch == prior->handoff_epoch + 1 && next->owner != prior->owner;
    // A complete native camp arrival terminates local ownership directly.
    // Its per-member physical-return receipts replace an abstract resume crossing.
    const bool completed_structural_return = was_local && next != nullptr &&
            next->kind == bandit_live_world::outing_kind::structural_sortie &&
            next->owner == bandit_live_world::simulation_owner::abstract &&
            next->handoff_epoch == prior->handoff_epoch + 1 &&
            next->member_ids == prior->member_ids &&
    std::all_of( next->member_ids.begin(), next->member_ids.end(), [&]( character_id id ) {
        if( !next->member_is_resolved( id ) ) {
            return false;
        }
        if( prior->member_is_resolved( id ) ||
            std::find( next->casualty_ids.begin(), next->casualty_ids.end(), id ) !=
            next->casualty_ids.end() ) {
            return true;
        }
        const std::string key = bandit_pursuit_handoff::make_operation_component_key(
                                    next->activity_id, next->generation, "return", std::to_string( id.get_value() ) );
        return std::count_if( next->member_return_receipts.begin(), next->member_return_receipts.end(),
        [&]( const auto & receipt ) {
            return receipt.member_id == id && receipt.application_key == key &&
                   receipt.returned_minutes == next->last_advanced_minutes;
        } ) == 1;
    } );
    const bool initial_hostile_contact = prior == nullptr && is_local &&
                                         next->kind == bandit_live_world::outing_kind::hostile_operation &&
                                         site.active_hostile_operation.phase ==
                                         bandit_live_world::hostile_operation_phase::committed_contact;
    if( was_local && ( previous->site_id != site.site_id ||
                       ( next != nullptr && ( next->activity_id != prior->activity_id ||
                               next->generation != prior->generation ||
                               ( next->handoff_epoch != prior->handoff_epoch &&
                                 !hostile_owner_transition && !completed_structural_return &&
                                 !( prior->owner == bandit_live_world::simulation_owner::local &&
                                    next->owner == bandit_live_world::simulation_owner::abstract &&
                                    next->handoff_epoch == prior->handoff_epoch + 1 &&
                                    next->crossing.pending() &&
                                    next->crossing.prior_owner == prior->owner &&
                                    next->crossing.next_owner == next->owner &&
                                    next->crossing.actor_ids == prior->member_ids &&
                                    next->crossing.activity_id == next->activity_id &&
                                    next->crossing.generation == next->generation &&
                                    next->crossing.handoff_epoch == next->handoff_epoch &&
                                    next->crossing.cursor_minutes == next->last_advanced_minutes ) ) ||
                               ( !restore_previous_cursor &&
                                 next->last_advanced_minutes < prior->last_advanced_minutes ) ) ) ) ) {
        return false;
    }
    if( !was_local && !hostile_owner_transition && !initial_hostile_contact &&
        ( !next->crossing.pending() ||
          next->crossing.prior_owner != bandit_live_world::simulation_owner::abstract ||
          next->crossing.next_owner != bandit_live_world::simulation_owner::local ||
          next->crossing.actor_ids != next->member_ids ) ) {
        return false;
    }
    const auto cleanup_actor = confirmed_cleanup_death ?
                               overmap_buffer.find_npc( *confirmed_cleanup_death ) : nullptr;
    // Cleanup cannot defer a witnessed death until an absent partner returns:
    // the native body is about to be erased.  Preserve only this terminal fact;
    // absence does not resolve the partner or admit a load/progress claim.
    const bool committing_cleanup_death = was_local && cleanup_actor && cleanup_actor->is_dead() &&
                                          !prior->member_is_resolved( *confirmed_cleanup_death ) &&
                                          std::find( prior->member_ids.begin(), prior->member_ids.end(),
                                                  *confirmed_cleanup_death ) != prior->member_ids.end() &&
                                          ( !apply || ( next != nullptr && next->member_is_resolved( *confirmed_cleanup_death ) &&
                                                  site.find_member( *confirmed_cleanup_death ) != nullptr &&
                                                  site.find_member( *confirmed_cleanup_death )->state ==
                                                  bandit_live_world::member_state::dead ) );
    const auto &ids = was_local ? prior->member_ids : next->member_ids;
    std::set<character_id> unique_ids;
    struct update {
        npc *copy;
        bandit_live_world_projection_lease before;
        bandit_live_world_projection_lease after;
    };
    std::vector<update> updates;
    for( const character_id id : ids ) {
        if( !unique_ids.insert( id ).second ) {
            return false;
        }
        const bool prior_resolved = was_local && prior->member_is_resolved( id );
        const bool next_resolved = next == nullptr || next->member_is_resolved( id );
        auto actor = overmap_buffer.find_npc( id );
        if( !actor && g ) {
            for( npc &active : g->all_npcs() ) {
                if( active.getID() == id ) {
                    // The persistent save owner is required for a living projection.
                    return false;
                }
            }
        }
        if( !actor ) {
            // Absence is eligible only for the existing native terminal paths.
            // The post-commit pass still requires that candidate to resolve it;
            // ordinary progress and load reconciliation never use this allowance.
            if( prior_resolved || next_resolved || committing_cleanup_death ||
                ( !apply && allow_confirmed_missing_deaths && was_local &&
                  live_bandit_confirmed_member_death( *prior, id, live_bandit_current_minutes() ) ) ||
                ( !apply && allow_deadline_missing_resolution && was_local &&
                  prior->missing_deadline_minutes >= 0 &&
                  live_bandit_current_minutes() >= prior->missing_deadline_minutes ) ) {
                continue;
            }
            return false;
        }
        const auto expected = prior_resolved ? bandit_live_world_projection_lease() :
                              was_local ? live_bandit_projection_lease( site.site_id, *prior ) :
                              bandit_live_world_projection_lease();
        const auto desired = is_local && !next_resolved ?
                             live_bandit_projection_lease( site.site_id, *next ) :
                             bandit_live_world_projection_lease();
        const auto copies = bandit_live_world_projection_lease_copies( *actor );
        if( copies.size() > 2 ) {
            return false;
        }
        for( npc *copy : copies ) {
            const auto &claim = copy->get_bandit_live_world_projection_lease();
            // Initial bind callbacks may already have installed this exact lease.
            if( !live_bandit_projection_lease_matches( claim, expected ) &&
                !( !was_local && live_bandit_projection_lease_matches( claim, desired ) ) ) {
                return false;
            }
            updates.push_back( { copy, claim, desired } );
        }
    }
    if( !apply ) {
        return true;
    }
    try {
        for( const update &entry : updates ) {
            entry.copy->set_bandit_live_world_projection_lease( entry.after );
        }
    } catch( ... ) {
        for( const update &entry : updates ) {
            entry.copy->set_bandit_live_world_projection_lease( entry.before );
        }
        throw;
    }
    return true;
}

// Keep the existing candidate/commit boundary coupled to the actor save owners.
// Rejected progress changes neither the authoritative site nor any actor claim.
template<typename Commit>
auto commit_live_bandit_local_progress( bandit_live_world::site_record &site, Commit commit,
                                        const bool resolve_deadline_missing = false,
const std::optional<character_id> confirmed_cleanup_death = {} )
{
    using result_type = decltype( commit( site ) );
    if( !persist_live_bandit_local_projection_leases( site, &site, false, false, true,
            resolve_deadline_missing, confirmed_cleanup_death ) ) {
        return result_type{};
    }
    bandit_live_world::site_record candidate = site;
    const result_type result = commit( candidate );
    bool applied;
    if constexpr( std::is_same_v<result_type, bool> ) {
        applied = result;
    } else if constexpr( std::is_same_v<result_type, std::optional<int>> ) {
        applied = result.has_value();
    } else if constexpr( std::is_enum_v<result_type> ) {
        if constexpr( std::is_same_v<result_type, bandit_live_world::local_handoff_commit_result> ) {
            applied = result == bandit_live_world::local_handoff_commit_result::applied;
        } else {
            applied = static_cast<int>( result ) > 1;
        }
    } else if constexpr(
        std::is_same_v<result_type, bandit_live_world::covert_scout_egress_failure_effect> ) {
        applied = result.result != bandit_live_world::covert_scout_egress_failure_result::rejected;
    } else if constexpr( std::is_same_v<result_type, bandit_live_world::covert_scout_burn_effect> ) {
        applied = result.result == bandit_live_world::covert_scout_burn_result::applied;
    } else {
        applied = result.changed;
    }
    if( !applied ) {
        return result;
    }
    if( !persist_live_bandit_local_projection_leases( candidate, &site, true, false, false, false,
            confirmed_cleanup_death ) ) {
        return result_type{};
    }
    site = std::move( candidate );
    return result;
}

bool rollback_live_bandit_local_progress( bandit_live_world::site_record &site,
        const bandit_live_world::site_record &before )
{
    if( !persist_live_bandit_local_projection_leases( before, &site, true, true ) ) {
        return false;
    }
    site = before;
    return true;
}

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

static bool openclaw_harness_bandit_owner_trace_enabled()
{
    const char *enabled = std::getenv( "OPENCLAW_HARNESS_UI_TRACE" );
    return enabled != nullptr && enabled[0] != '\0' && enabled[0] != '0';
}

static void openclaw_harness_trace_bandit_owner( const std::string &event,
        const tripoint_abs_omt &player_omt, const tripoint_abs_omt *camp_omt,
        const int distance, const bool eligible, const int result = -1 )
{
    if( !openclaw_harness_bandit_owner_trace_enabled() ) {
        return;
    }
    std::ostringstream trace;
    trace << "openclaw_harness_ui_trace: component=bandit_owner"
          << " event=" << event
          << " player_omt=" << player_omt.x() << ',' << player_omt.y() << ',' << player_omt.z()
          << " camp_omt=";
    if( camp_omt != nullptr ) {
        trace << camp_omt->x() << ',' << camp_omt->y() << ',' << camp_omt->z();
    } else {
        trace << "none";
    }
    trace << " distance=" << distance << " eligible=" << ( eligible ? "true" : "false" )
          << " result=" << result;
    DebugLog( D_INFO, DC_ALL ) << trace.str() << '\n';
}

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
    const tripoint_abs_omt player_omt = u.pos_abs_omt();
    const std::optional<basecamp *> direct_camp = overmap_buffer.find_camp( player_omt.xy() );
    const std::vector<camp_reference> camps_near_player = overmap_buffer.get_camps_near(
                u.pos_abs_sm(), live_bandit_camp_adjacent_radius_submaps );
    if( openclaw_harness_bandit_owner_trace_enabled() ) {
        std::ostringstream trace;
        trace << "openclaw_harness_ui_trace: component=bandit_owner event=registry"
              << " player_omt=" << player_omt.x() << ',' << player_omt.y() << ',' << player_omt.z()
              << " direct=";
        if( direct_camp && *direct_camp != nullptr ) {
            const tripoint_abs_omt camp_omt = ( *direct_camp )->camp_omt_pos();
            trace << camp_omt.x() << ',' << camp_omt.y() << ',' << camp_omt.z();
        } else {
            trace << "none";
        }
        trace << " nearby_count=" << camps_near_player.size() << " nearby_first=";
        if( !camps_near_player.empty() && camps_near_player.front().camp != nullptr ) {
            const tripoint_abs_omt camp_omt = camps_near_player.front().camp->camp_omt_pos();
            trace << camp_omt.x() << ',' << camp_omt.y() << ',' << camp_omt.z();
        } else {
            trace << "none";
        }
        DebugLog( D_INFO, DC_ALL ) << trace.str() << '\n';
    }
    if( direct_camp && *direct_camp != nullptr ) {
        return *direct_camp;
    }
    if( !camps_near_player.empty() ) {
        return camps_near_player.front().camp;
    }

    return nullptr;
}

bool live_bandit_player_near_basecamp( const avatar &u )
{
    basecamp *camp = live_bandit_nearest_basecamp( u );
    const tripoint_abs_omt player_omt = u.pos_abs_omt();
    const std::optional<tripoint_abs_omt> camp_omt_value = camp != nullptr ?
            std::optional<tripoint_abs_omt>( camp->camp_omt_pos() ) : std::nullopt;
    const tripoint_abs_omt *camp_omt = camp_omt_value ? &*camp_omt_value : nullptr;
    const int distance = camp_omt != nullptr ? rl_dist( player_omt, *camp_omt ) : -1;
    const bool eligible = camp != nullptr;
    openclaw_harness_trace_bandit_owner( "near_basecamp", player_omt, camp_omt, distance, eligible );
    return eligible;
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
    }

    map &here = get_map();
    int closest_member_distance = rl_dist( site.anchor, u.pos_abs_omt() );
    bool follower_sight = false;
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
            const bool player_sees_member = get_player_view().sees( here, member_pos );
            const bool member_sees_player = member_npc->sees( here, u );
            const bool follower_sees_member = live_bandit_seen_by_nearby_ally( here, u, member_pos );
            input.player_contact |= member_sees_player;
            input.follower_sight |= follower_sees_member;
            input.current_exposure |= player_sees_member ||
                                      follower_sees_member;
            follower_sight |= follower_sees_member;
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
            input.local_contact_established |= bandit_live_world::normal_shakedown_first_sight_requires_parley(
                                                  distance <= 1 || saved_local_contact,
                                                  follower_sight, input.rolling_travel_scene );
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

int live_bandit_nearby_basecamp_defender_count( const Character &u )
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

bool live_bandit_shakedown_receiver_eligible( const bandit_live_world::site_record &site,
        const Character &receiver, bool require_awake )
{
    const auto &route = site.active_hostile_operation.reservation.shared_route;
    if( route.empty() || receiver.is_dead_state() ||
        receiver.has_effect( effect_narcosis ) || receiver.has_effect( effect_npc_suspend ) ||
        receiver.has_bionic( bio_sleep_shutdown ) || receiver.is_involuntarily_asleep() ||
        ( require_awake && receiver.in_sleep_state() ) ||
        !get_map().inbounds( receiver.pos_bub() ) ) {
        return false;
    }
    const npc *recipient = receiver.as_npc();
    return receiver.is_avatar() || ( recipient != nullptr && recipient->is_active() &&
           recipient->is_player_ally() && recipient->assigned_camp &&
           recipient->assigned_camp->xy() == route.back().xy() );
}

Character *live_bandit_shakedown_receiver( const bandit_live_world::site_record &site );
bool live_bandit_shakedown_speaker_ready( const npc &actor );

npc *live_bandit_shakedown_local_speaker( const bandit_live_world::site_record &site )
{
    const auto *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return nullptr;
    }
    for( const character_id id : outing->member_ids ) {
        if( outing->member_is_resolved( id ) ) {
            continue;
        }
        npc *actor = g->find_npc( id );
        if( actor != nullptr && actor->is_active() && !actor->is_dead() &&
            get_map().inbounds( actor->pos_bub() ) && live_bandit_shakedown_speaker_ready( *actor ) ) {
            return actor;
        }
    }
    return nullptr;
}

bool live_bandit_encounter_reachable( const Character &receiver,
                                    const tripoint_abs_ms &position, const int radius )
{
    map &here = get_map();
    const auto from = receiver.pos_bub( here );
    const auto to = here.get_bub( position );
    return here.inbounds( from ) && here.inbounds( to ) &&
           rl_dist( from, to ) <= radius &&
           ( from == to || here.clear_path( from, to, rl_dist( from, to ), 1, 100 ) );
}

bandit_live_world::shakedown_goods_pool live_bandit_encounter_goods_pool(
    const bandit_live_world::local_gate_input &input, Character &receiver )
{
    bandit_live_world::shakedown_goods_pool pool;
    pool.basecamp_or_camp_scene = input.basecamp_or_camp_scene;
    pool.player_carried_value = receiver.is_avatar() ? live_bandit_character_goods_value( receiver ) : 0;
    pool.companion_carried_value = receiver.is_npc() ? live_bandit_character_goods_value( receiver ) : 0;
    // Match the encounter trade selector: only physically reachable local
    // goods and allies. Neither remote assigned workers nor remote vehicles
    // contribute merely by being registered to the camp.
    for( const npc &ally : g->all_npcs() ) {
        if( &ally != &receiver && ally.is_player_ally() && !ally.is_dead() &&
            ally.is_active() && live_bandit_encounter_reachable( receiver, ally.pos_abs(), 12 ) ) {
            pool.companion_carried_value += live_bandit_character_goods_value( ally );
        }
    }
    const int radius = input.basecamp_or_camp_scene ? live_bandit_basecamp_reach_radius : 1;
    map &here = get_map();
    for( const auto &tile : here.points_in_radius( receiver.pos_bub( here ), radius ) ) {
        if( !live_bandit_encounter_reachable( receiver, here.get_abs( tile ), radius ) ) {
            continue;
        }
        if( here.accessible_items( tile ) ) {
            for( const item &it : here.i_at( tile ) ) {
                pool.reachable_basecamp_value += live_bandit_item_value( it );
            }
        }
        if( const auto cargo = here.veh_at( tile ).cargo() ) {
            for( const item &it : cargo->items() ) {
                pool.reachable_basecamp_value += live_bandit_item_value( it );
            }
        }
    }
    if( !input.basecamp_or_camp_scene ) {
        pool.vehicle_carried_value = pool.reachable_basecamp_value;
        pool.reachable_basecamp_value = 0;
    }
    return pool;
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

int live_bandit_reachable_goods_value( const bandit_live_world::shakedown_goods_pool &pool )
{
    int value = pool.player_carried_value + pool.companion_carried_value;
    value += pool.basecamp_or_camp_scene ? pool.reachable_basecamp_value :
             pool.vehicle_carried_value;
    return std::max( 0, value );
}

int live_bandit_loaded_player_allied_population()
{
    map &here = get_map();
    int population = 1;
    for( const npc &guy : g->all_npcs() ) {
        if( guy.is_player_ally() && !guy.is_dead() && guy.is_active() &&
            here.inbounds( guy.pos_bub( here ) ) ) {
            population++;
        }
    }
    return population;
}

bool live_bandit_active_hostile_receipt_matches(
    const bandit_live_world::world_state &state,
    const bandit_live_world::hostile_target_opportunity_record &receipt )
{
    if( receipt.consumed_generation <= 0 ) {
        return false;
    }
    return std::any_of( state.sites.begin(), state.sites.end(), [&receipt](
    const bandit_live_world::site_record & site ) {
        const bandit_live_world::hostile_operation_state &operation =
            site.active_hostile_operation;
        const bandit_live_world::active_outing_state &reservation = operation.reservation;
        return operation.is_active() && reservation.activity_id == receipt.consumed_operation_id &&
               operation.source_report_application_key == receipt.consumed_report_key &&
               reservation.generation == receipt.consumed_generation;
    } );
}

void observe_live_bandit_player_target_opportunity()
{
    avatar &u = get_avatar();
    const tripoint_abs_omt target_omt = u.pos_abs_omt();
    const std::optional<basecamp *> direct_camp = overmap_buffer.find_camp( target_omt.xy() );
    const std::optional<tripoint_abs_omt> basecamp_omt = direct_camp && *direct_camp != nullptr ?
            std::optional<tripoint_abs_omt>( ( *direct_camp )->camp_omt_pos() ) : std::nullopt;
    if( !bandit_live_world::authoritative_player_target_observation_is_local_to_basecamp(
            target_omt, basecamp_omt ) ) {
        openclaw_harness_trace_bandit_owner( "observation_rejected", u.pos_abs_omt(), nullptr, -1,
                false, 0 );
        return;
    }
    bandit_live_world::local_gate_input input;
    input.basecamp_or_camp_scene = true;
    input.local_opportunity = 2;
    const std::string target_id = "player@" + live_bandit_omt_token( target_omt );
    const bandit_live_world::shakedown_goods_pool pool =
        live_bandit_make_shakedown_goods_pool( input, u );
    const bandit_live_world::hostile_target_opportunity_evidence evidence = {
        live_bandit_reachable_goods_value( pool ),
        live_bandit_loaded_player_allied_population(), input.local_opportunity
    };
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const bandit_live_world::hostile_target_opportunity_record *existing =
        state.find_hostile_target_opportunity( target_id, target_omt );
    if( existing != nullptr && live_bandit_active_hostile_receipt_matches( state, *existing ) &&
        ( existing->goods_value != evidence.reachable_goods_value ||
          existing->population != evidence.loaded_population ||
          existing->activity != evidence.activity ) ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world hostile_target_observation frozen"
                                   << " target=" << target_id
                                   << " receipt=" << existing->consumed_operation_id
                                   << " generation=" << existing->consumed_generation << '\n';
        return;
    }

    // Camp-local scout reports read this world receipt; only this loaded player-scene owner writes it.
    const bool observed = bandit_live_world::observe_authoritative_hostile_target_opportunity( state,
                           target_id, target_omt, evidence );
    openclaw_harness_trace_bandit_owner( "observation_result", target_omt, &*basecamp_omt, 0,
                                         true, observed ? 1 : 0 );
    if( observed ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world hostile_target_observation"
                                   << " target=" << target_id
                                   << " goods=" << evidence.reachable_goods_value
                                   << " population=" << evidence.loaded_population
                                   << " activity=" << evidence.activity << '\n';
    }
}

int live_bandit_select_shakedown_payment( const bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::shakedown_surface &surface, avatar &u )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return 0;
    }
    npc *trader = live_bandit_shakedown_local_speaker( site );
    if( trader == nullptr ) {
        DebugLog( D_INFO, DC_ALL ) << "shakedown_trade_ui result=no_trader demanded="
                                   << surface.demanded_value << " reachable="
                                   << surface.reachable_goods_value << '\n';
        return 0;
    }

    Character *receiver = live_bandit_shakedown_receiver( site );
    if( site.active_hostile_operation.is_active() && receiver == nullptr ) {
        return 0;
    }
    Character &payer = receiver != nullptr ? *receiver : static_cast<Character &>( u );
    const bool encounter_only = receiver != nullptr;
    basecamp *payment_basecamp = !encounter_only && input.basecamp_or_camp_scene ?
                                 live_bandit_nearest_basecamp( u ) : nullptr;
    if( payment_basecamp != nullptr ) {
        live_bandit_refresh_basecamp_storage_tiles( u, *payment_basecamp );
    }
    DebugLog( D_INFO, DC_ALL ) << "shakedown_trade_ui opened demanded="
                               << surface.demanded_value << " reachable=" << surface.reachable_goods_value
                               << " payer=" << payer.getID().get_value()
                               << " encounter_only=" << ( encounter_only ? "true" : "false" )
                               << " trader=" << trader->getID().get_value();
    const bool paid = npc_trading::trade_to_stash( *trader, payer, site.anchor,
                                          surface.demanded_value, _( "Pay:" ),
                                          input.basecamp_or_camp_scene ? live_bandit_basecamp_reach_radius : 1,
                                          12, payment_basecamp, encounter_only );
    DebugLog( D_INFO, DC_ALL ) << "shakedown_trade_ui result=" << ( paid ? "paid" : "cancel_or_short" )
                               << " demanded=" << surface.demanded_value
                               << " payer=" << payer.getID().get_value();
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

int live_bandit_current_minutes();

struct live_bandit_paid_return_travel_order {
    npc *member_npc;
    std::vector<tripoint_abs_omt> route;
    bool local_descent = false;
};

struct live_bandit_paid_return_plan {
    std::vector<live_bandit_paid_return_travel_order> travel_orders;
    bandit_live_world::simulation_advance_cursor cursor;
};

std::optional<live_bandit_paid_return_plan> live_bandit_prepare_paid_return(
    const bandit_live_world::site_record &site, const bool allow_local_descent = false )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( site );
    if( outing == nullptr || outing != &site.active_hostile_operation.reservation ||
        site.active_hostile_operation.phase !=
        bandit_live_world::hostile_operation_phase::committed_contact ||
        outing->owner != bandit_live_world::simulation_owner::local || !cursor ||
        ( !outing->camp_id.empty() && outing->camp_id != site.site_id ) ||
        live_bandit_current_minutes() < cursor->last_advanced_minutes ) {
        return std::nullopt;
    }
    live_bandit_paid_return_plan plan;
    plan.cursor = *cursor;
    for( const character_id member_id : outing->member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( outing->member_is_resolved( member_id ) ) {
            if( member == nullptr ||
                ( member->state != bandit_live_world::member_state::dead &&
                  member->state != bandit_live_world::member_state::at_home ) ) {
                return std::nullopt;
            }
            continue;
        }
        npc *member_npc = g->find_npc( member_id );
        if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ||
            member_npc == nullptr || member_npc->is_dead() ) {
            return std::nullopt;
        }
        const tripoint_abs_omt start = member_npc->pos_abs_omt();
        bool local_descent = false;
        std::vector<tripoint_abs_omt> route = overmap_buffer.get_travel_path(
                    start, site.anchor, overmap_path_params::for_npc() ).points;
        if( route.empty() && allow_local_descent && start.z() > site.anchor.z() && member_npc->is_active() ) {
            // The macro map does not represent local roof ladders/stairs.
            // Compose its ground route only if the ordinary NPC motor can
            // physically reach that route's first onward waypoint, at its
            // real height. Do not order an unreachable indoor OMT centre.
            const tripoint_abs_omt ground( start.xy(), site.anchor.z() );
            route = overmap_buffer.get_travel_path( ground, site.anchor,
                                                  overmap_path_params::for_npc() ).points;
            if( route.size() < 2 || route.front() != site.anchor || route.back() != ground ) {
                return std::nullopt;
            }
            route.pop_back();
            local_descent = true;
            const tripoint_bub_ms next = get_map().get_bub(
                                            project_to<coords::ms>( route.back() ) + point( SEEX, SEEY ) );
            if( !get_map().inbounds( next ) ) {
                return std::nullopt;
            }
            const auto connector = get_map().route( *member_npc,
                                   pathfinding_target::radius( next, 2 ) );
            if( connector.empty() || !pathfinding_target::radius( next, 2 ).contains( connector.back() ) ) {
                return std::nullopt;
            }
        } else if( route.empty() || route.front() != site.anchor || route.back() != start ) {
            return std::nullopt;
        }
        plan.travel_orders.push_back( { member_npc, std::move( route ), local_descent } );
    }
    return plan;
}

void live_bandit_apply_hostile_return_orders( bandit_live_world::site_record &site,
        live_bandit_paid_return_plan &plan )
{
    const bool composed_descent = std::any_of( plan.travel_orders.begin(), plan.travel_orders.end(),
    []( const live_bandit_paid_return_travel_order &order ) { return order.local_descent; } );
    for( live_bandit_paid_return_travel_order &order : plan.travel_orders ) {
        const bool assigned_withdrawal = std::find(
                site.active_hostile_operation.withdrawing_member_ids.begin(),
                site.active_hostile_operation.withdrawing_member_ids.end(),
                order.member_npc->getID() ) !=
            site.active_hostile_operation.withdrawing_member_ids.end();
        const bool frightened_withdrawal = ( assigned_withdrawal ||
                                            site.active_hostile_operation.operation_kind ==
                                            bandit_live_world::hostile_operation_kind::shakedown ) &&
                                           ( order.member_npc->get_attitude() == NPCATT_FLEE ||
                                             order.member_npc->get_attitude() == NPCATT_FLEE_TEMP ||
                                             order.member_npc->has_effect( effect_npc_run_away ) );
        for( const shared_ptr_fast<npc> &persistent_member : overmap_buffer.get_overmap_npcs() ) {
            if( persistent_member && persistent_member->getID() == order.member_npc->getID() &&
                persistent_member.get() != order.member_npc ) {
                if( !frightened_withdrawal ) {
                    persistent_member->set_attitude( NPCATT_NULL );
                }
                if( composed_descent ) {
                    persistent_member->path.clear();
                    persistent_member->goto_to_this_pos.reset();
                    persistent_member->fetching_item = false;
                    persistent_member->wanted_item = {};
                }
                persistent_member->goal = site.anchor;
                persistent_member->omt_path = order.route;
                persistent_member->set_mission( NPC_MISSION_TRAVELLING );
            }
        }
        if( !frightened_withdrawal ) {
            order.member_npc->set_attitude( NPCATT_NULL );
        }
        if( composed_descent ) {
            order.member_npc->path.clear();
            order.member_npc->goto_to_this_pos.reset();
            order.member_npc->fetching_item = false;
            order.member_npc->wanted_item = {};
        }
        order.member_npc->goal = site.anchor;
        order.member_npc->omt_path = std::move( order.route );
        order.member_npc->set_mission( NPC_MISSION_TRAVELLING );
    }
}

bool live_bandit_commit_paid_return( bandit_live_world::site_record &site,
                                     live_bandit_paid_return_plan &plan,
                                     const bandit_live_world::shakedown_surface &surface,
                                     const int surrendered_value )
{
    const std::string summary =
        string_format( "shakedown_surface paid toll=%d demanded=%d reachable=%d",
                       surrendered_value, surface.demanded_value,
                       surface.reachable_goods_value );
    if( commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
    return bandit_live_world::transition_hostile_operation_phase( next, plan.cursor,
            bandit_live_world::hostile_operation_phase::committed_contact,
            bandit_live_world::hostile_operation_phase::returning_home,
            live_bandit_current_minutes(), summary, true );
    } ) !=
    bandit_live_world::hostile_operation_transition_result::applied ) {
        return false;
    }
    site.active_hostile_operation.shakedown_pending_branch = "paid";
    site.active_hostile_operation.shakedown_pending_pay_identity.clear();
    site.active_hostile_operation.shakedown_pending_demanded_value = surface.demanded_value;
    site.active_hostile_operation.shakedown_pending_surrendered_value = surrendered_value;
    site.active_hostile_operation.shakedown_pending_reachable_value = surface.reachable_goods_value;
    site.active_hostile_operation.shakedown_pending_basecamp_scene = surface.includes_basecamp_inventory;
    live_bandit_apply_hostile_return_orders( site, plan );
    DebugLog( D_INFO, DC_ALL ) << summary << " return=physical\n";
    return true;
}

void live_bandit_choose_fight( bandit_live_world::site_record &site,
                               const bandit_live_world::shakedown_surface &surface, const Character &u )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr ) {
        return;
    }
    const std::vector<character_id> member_ids = outing->member_ids;
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    operation.shakedown_pending_branch = "fight";
    operation.shakedown_pending_pay_identity.clear();
    operation.shakedown_pending_demanded_value = surface.demanded_value;
    operation.shakedown_pending_surrendered_value = 0;
    operation.shakedown_pending_reachable_value = surface.reachable_goods_value;
    operation.shakedown_pending_basecamp_scene = surface.includes_basecamp_inventory;
    if( surface.includes_basecamp_inventory ) {
        bandit_live_world::begin_shakedown_basecamp_defender_observation( site,
                live_bandit_nearby_basecamp_defender_count( u ) );
    }

    const std::string summary = string_format( "shakedown_surface fight demanded=%d reachable=%d",
                                surface.demanded_value, surface.reachable_goods_value );
    DebugLog( D_INFO, DC_ALL ) << summary << '\n';
    for( const character_id &member_id : member_ids ) {
        if( outing->member_is_resolved( member_id ) ) {
            continue;
        }
        bandit_live_world::update_member_state( site, member_id,
                                                bandit_live_world::member_state::local_contact, summary );
        if( npc *member_npc = g->find_npc( member_id ) ) {
            if( !member_npc->is_dead() && member_npc->get_attitude() != NPCATT_FLEE &&
                member_npc->get_attitude() != NPCATT_FLEE_TEMP &&
                !member_npc->has_effect( effect_narcosis ) ) {
                member_npc->set_attitude( NPCATT_KILL );
            }
        }
    }
    if( const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( site ) ) {
        bandit_live_world::begin_matching_hostile_shakedown_combat( site, *cursor,
                to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ), summary );
    }
}

std::pair<std::string, nc_color> live_bandit_shakedown_speaker( const bandit_live_world::site_record &site )
{
    if( const npc *speaker = live_bandit_shakedown_local_speaker( site ) ) {
        return { speaker->disp_name(), speaker->basic_symbol_color() };
    }
    return { _( "Bandit" ), c_red };
}

bool live_bandit_shakedown_response_is_current( const bandit_live_world::site_record &site,
        const std::string &activity_id, const int generation, const character_id receiver_id )
{
    const auto &operation = site.active_hostile_operation;
    return operation.is_active() && operation.reservation.activity_id == activity_id &&
           operation.reservation.generation == generation &&
           operation.shakedown_receiver_id == receiver_id &&
           operation.shakedown_pending_branch.empty() && live_bandit_shakedown_receiver( site ) != nullptr;
}

enum class live_bandit_shakedown_response : int {
    pay,
    fight,
    fight_on_backout,
};

bool live_bandit_consume_explicit_fight( bandit_live_world::site_record &site,
                                       const bandit_live_world::shakedown_surface &surface,
                                       const std::string &activity_id, const int generation,
                                       const character_id receiver_id )
{
    if( !live_bandit_shakedown_response_is_current( site, activity_id, generation, receiver_id ) ) {
        return false;
    }
    Character *receiver = live_bandit_shakedown_receiver( site );
    // Commit the existing branch before attempting native vocalization. Its
    // persisted response prevents retries/reload from shouting a second time.
    live_bandit_choose_fight( site, surface, *receiver );
    const int emitted_volume = receiver->shout( "No, fuck you!" );
    const auto scope = raid_decision_trace::environment_selected_npcs();
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    if( scope.includes_turn( turn ) &&
        ( scope.includes( receiver->getID().get_value() ) ||
          std::any_of( site.active_hostile_operation.reservation.member_ids.begin(),
                       site.active_hostile_operation.reservation.member_ids.end(),
    [&scope]( const character_id id ) { return scope.includes( id.get_value() ); } ) ) ) {
        const tripoint_abs_ms position = receiver->pos_abs();
        raid_decision_trace::native_recorder().record_edge( "bandit_shakedown_fight_reply", turn,
                "\"trace_group_id\":" + raid_decision_trace::quote( scope.group_id ) +
                ",\"operation_id\":" + raid_decision_trace::quote( activity_id ) +
                ",\"generation\":" + std::to_string( generation ) +
                ",\"site_id\":" + raid_decision_trace::quote( site.site_id ) +
                ",\"receiver_id\":" + std::to_string( receiver->getID().get_value() ) +
                ",\"receiver_is_avatar\":" + ( receiver->is_avatar() ? "true" : "false" ) +
                ",\"position_abs\":[" + std::to_string( position.x() ) + ',' +
                std::to_string( position.y() ) + ',' + std::to_string( position.z() ) + ']' +
                ",\"attempted\":true,\"emitted\":" + ( emitted_volume > 0 ? "true" : "false" ) +
                ",\"native_volume\":" + std::to_string( emitted_volume ) );
    }
    return true;
}

live_bandit_shakedown_response query_live_bandit_shakedown_dialogue(
    const bandit_live_world::site_record &site,
    const bandit_live_world::shakedown_surface &surface )
{
    semantic_surface_manager *const semantic_manager = active_semantic_surface_manager();
    std::optional<live_bandit_shakedown_response> semantic_response;
    std::optional<semantic_surface_scope> semantic_scope;
    if( semantic_manager != nullptr ) {
        DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_shakedown_semantic: event=scope_construct"
                                   << " run_active=" << ( openclaw_harness_semantic_session_active() ? "true" : "false" )
                                   << " manager=present opening=" << surface.opening_id;
        const std::map<std::string, std::string> semantic_payload = {
            { "opening", surface.opening_id },
            { "responses", "pay/fight" },
            { "payment_surface", "npc_trade_ui" },
            { "operation_id", site.active_hostile_operation.reservation.activity_id },
            { "generation", std::to_string( site.active_hostile_operation.reservation.generation ) },
            { "receiver_id", std::to_string( site.active_hostile_operation.shakedown_receiver_id.get_value() ) },
            { "receiver_is_avatar", site.active_hostile_operation.shakedown_receiver_is_avatar ? "true" : "false" },
            { "camp_response", site.active_hostile_operation.shakedown_contact_established &&
                                !site.active_hostile_operation.shakedown_receiver_is_avatar ? "true" : "false" },
        };
        const std::vector<semantic_action_descriptor> semantic_actions = {
            { "shakedown.pay", "", _( "Pay." ), true },
            { "shakedown.fight", "", _( "Fight." ), true },
        };
        const semantic_action_consumer semantic_consumer = [ &semantic_response ](
        const semantic_action_request &request ) {
            if( request.action_id == "shakedown.pay" ) {
                semantic_response = live_bandit_shakedown_response::pay;
            } else if( request.action_id == "shakedown.fight" ) {
                semantic_response = live_bandit_shakedown_response::fight;
            } else {
                return semantic_action_dispatch_result( false, "unadvertised_action" );
            }
            return semantic_action_dispatch_result( true );
        };
        semantic_scope.emplace( *semantic_manager, "shakedown_demand", "Bandit demand",
                                semantic_payload, semantic_actions, semantic_consumer );
        DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_shakedown_semantic: event=scope_published"
                                   << " owner=shakedown_demand actions=shakedown.pay,shakedown.fight";
    } else {
        DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_shakedown_semantic: event=scope_absent"
                                   << " run_active=" << ( openclaw_harness_semantic_session_active() ? "true" : "false" );
    }
    dialogue_window d_win;
    d_win.is_not_conversation = true;
    const std::pair<std::string, nc_color> speaker = live_bandit_shakedown_speaker( site );
    d_win.add_to_history( surface.bark, speaker.first, speaker.second );
    if( const Character *receiver = live_bandit_shakedown_receiver( site ) ) {
        if( !receiver->is_avatar() ) {
            const auto camp = overmap_buffer.find_camp(
                                  site.active_hostile_operation.reservation.shared_route.back().xy() );
            d_win.add_to_history( string_format( _( "Camp response for %1$s — threat received by %2$s." ),
                                                camp && *camp ? ( *camp )->camp_name() :
                                                string_format( _( "camp at %s" ),
                                                        site.active_hostile_operation.reservation.shared_route.back().to_string() ),
                                                receiver->get_name() ) );
        }
    }
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
        if( semantic_scope ) {
            semantic_scope->consume_request();
        }
        if( semantic_response ) {
            return *semantic_response;
        }
        ui_manager::redraw();
        std::string action = ctxt.handle_input();
        if( semantic_response ) {
            return *semantic_response;
        }
        const input_event evt = ctxt.get_raw_input();
        d_win.handle_scrolling( action, ctxt );
        if( action == "CONFIRM" ) {
            if( d_win.sel_response == 0 ) {
                return live_bandit_shakedown_response::pay;
            }
            return live_bandit_shakedown_response::fight;
        }
        if( action == "QUIT" ) {
            return live_bandit_shakedown_response::fight_on_backout;
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

// A deferred choice is owned by the existing visit branch. Compare its exact
// sender/receiver and reservation identity without reusing a stale UI consumer.
std::string live_bandit_pending_pay_identity( const bandit_live_world::site_record &site )
{
    const auto &operation = site.active_hostile_operation;
    const auto &outing = operation.reservation;
    const npc *speaker = live_bandit_shakedown_local_speaker( site );
    if( speaker == nullptr || live_bandit_shakedown_receiver( site ) == nullptr ||
        ( !outing.camp_id.empty() && outing.camp_id != site.site_id ) ) {
        return {};
    }
    for( character_id id : outing.member_ids ) {
        if( !outing.member_is_resolved( id ) ) {
            const npc *actor = g->find_npc( id );
            if( actor == nullptr || !actor->is_active() || actor->is_dead() ||
                !live_bandit_shakedown_speaker_ready( *actor ) ) {
                return {};
            }
        }
    }
    std::ostringstream result;
    JsonOut json( result );
    json.start_array();
    json.write( site.site_id );
    json.write( outing.activity_id );
    json.write( outing.generation );
    json.write( outing.handoff_epoch );
    json.write( to_string( outing.owner ) );
    json.write( operation.source_report_application_key );
    json.write( speaker->getID() );
    json.write( operation.shakedown_receiver_id );
    json.write( outing.member_ids );
    json.write( outing.resolved_member_ids );
    json.write( outing.casualty_ids );
    json.end_array();
    return result.str();
}

bool live_bandit_cancel_stale_pending_pay( bandit_live_world::site_record &site,
        const bool unrelated_dialogue = false )
{
    auto &operation = site.active_hostile_operation;
    if( operation.shakedown_pending_branch != "pay_waiting_route" ) {
        return false;
    }
    const std::string identity = live_bandit_pending_pay_identity( site );
    const auto *manager = active_semantic_surface_manager();
    if( !unrelated_dialogue && !identity.empty() && identity == operation.shakedown_pending_pay_identity &&
        !( manager != nullptr && manager->top().has_value() && manager->top()->kind == "dialogue" ) ) {
        return false;
    }
    operation.shakedown_pending_branch.clear();
    operation.shakedown_pending_pay_identity.clear();
    add_msg( m_warning, _( "The interrupted payment choice is cancelled.  No goods changed hands." ) );
    return true;
}

bool open_live_bandit_shakedown_surface( bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::local_gate_decision &decision )
{
    auto &operation = site.active_hostile_operation;
    if( operation.shakedown_pending_branch == "paid_pending_return" ) {
        // Goods already changed hands. A recoverable return-lease refusal
        // retries only the physical return commit, never the trade itself.
        const auto plan = live_bandit_prepare_paid_return( site, true );
        if( !plan ) {
            return false;
        }
        auto return_plan = *plan;
        bandit_live_world::shakedown_surface receipt;
        receipt.demanded_value = operation.shakedown_pending_demanded_value;
        receipt.reachable_goods_value = operation.shakedown_pending_reachable_value;
        receipt.includes_basecamp_inventory = operation.shakedown_pending_basecamp_scene;
        return live_bandit_commit_paid_return( site, return_plan, receipt,
                                              operation.shakedown_pending_surrendered_value );
    }
    if( live_bandit_cancel_stale_pending_pay( site ) ) {
        return true;
    }
    const bool pending_pay = operation.shakedown_pending_branch == "pay_waiting_route";
    if( pending_pay ) {
        if( !live_bandit_prepare_paid_return( site, true ) ) {
            return false; // Keep the exact choice, not another demand/menu.
        }
    } else if( operation.is_active() && !operation.shakedown_pending_branch.empty() ) {
        return false;
    }
    if( !pending_pay && live_bandit_shakedown_already_opened( site ) ) {
        return false;
    }

    avatar &u = get_avatar();
    Character *receiver = live_bandit_shakedown_receiver( site );
    if( operation.is_active() && ( receiver == nullptr || live_bandit_shakedown_local_speaker( site ) == nullptr ) ) {
        return false;
    }
    // A real player-directed demand interrupts ordinary work. The existing
    // cancellation hooks retain resumable crafts/sorting in the backlog and
    // require the player to resume them, rather than working through a demand.
    if( receiver == &u && u.activity ) {
        if( !u.activity.is_interruptible() ) {
            return false;
        }
        u.cancel_activity();
        g->wait_popup_reset();
    }
    const bandit_live_world::shakedown_goods_pool pool = receiver != nullptr ?
        live_bandit_encounter_goods_pool( input, *receiver ) :
        live_bandit_make_shakedown_goods_pool( input, u );
    bandit_live_world::shakedown_surface surface =
        bandit_live_world::build_shakedown_surface( site, input, decision, pool );
    if( pending_pay ) {
        surface.demanded_value = operation.shakedown_pending_demanded_value;
    }
    DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_shakedown_surface_report( site,
                               surface );
    if( !surface.valid ) {
        return false;
    }
    // Activity-driven contact has no enclosing World session. Keep the native
    // manager available through the real trade successor, while the demand's
    // own scope still retires normally when its response is consumed.
    std::optional<semantic_surface_manager_session> semantic_session;
    if( openclaw_harness_semantic_session_active() && active_semantic_surface_manager() == nullptr ) {
        semantic_session.emplace( openclaw_harness_semantic_surface_manager() );
    }
    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_shakedown_semantic: event=valid_contact"
                               << " session_active=" << ( openclaw_harness_semantic_session_active() ? "true" : "false" )
                               << " opening=" << surface.opening_id;
    if( !pending_pay ) {
        bandit_live_world::mark_shakedown_reopen_used( site );
        DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_shakedown_semantic: event=reopen_marked";
        DebugLog( D_INFO, DC_ALL ) << "shakedown_surface_dialogue_window opening="
                                   << ( surface.opening_id.empty() ? "none" : surface.opening_id )
                                   << " responses=pay/fight payment_surface=npc_trade_ui\n";
    }
    const bool response_is_visit = operation.is_active();
    const auto response_operation_id = operation.reservation.activity_id;
    const int response_generation = operation.reservation.generation;
    const character_id response_receiver = operation.shakedown_receiver_id;
    const live_bandit_shakedown_response response = pending_pay ?
        live_bandit_shakedown_response::pay : query_live_bandit_shakedown_dialogue( site, surface );
    if( !pending_pay && response_is_visit && !live_bandit_shakedown_response_is_current(
            site, response_operation_id, response_generation, response_receiver ) ) {
        return false;
    }
    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_shakedown_semantic: event="
                               << ( pending_pay ? "pending_pay_resumed" : "dialogue_return" )
                               << " response=" << ( response == live_bandit_shakedown_response::pay ? "pay" : "fight" );

    bool payment_failed = false;
    if( response == live_bandit_shakedown_response::pay ) {
        std::optional<live_bandit_paid_return_plan> return_plan =
            live_bandit_prepare_paid_return( site, true );
        if( !return_plan ) {
            operation.shakedown_pending_branch = "pay_waiting_route";
            operation.shakedown_pending_pay_identity = live_bandit_pending_pay_identity( site );
            operation.shakedown_pending_demanded_value = surface.demanded_value;
            operation.shakedown_pending_reachable_value = surface.reachable_goods_value;
            operation.shakedown_pending_basecamp_scene = surface.includes_basecamp_inventory;
            add_msg( m_warning, _( "The bandits have no reachable return route.  Payment is postponed; no goods changed hands." ) );
            return false;
        } else {
            operation.shakedown_pending_pay_identity.clear();
            const int surrendered_value = live_bandit_select_shakedown_payment( site, input, surface, u );
            if( surrendered_value >= surface.demanded_value ) {
                add_msg( m_bad, _( "You complete the shakedown payment through trade." ) );
                operation.shakedown_pending_branch = "paid_pending_return";
                operation.shakedown_pending_demanded_value = surface.demanded_value;
                operation.shakedown_pending_surrendered_value = surrendered_value;
                operation.shakedown_pending_reachable_value = surface.reachable_goods_value;
                operation.shakedown_pending_basecamp_scene = surface.includes_basecamp_inventory;
                return live_bandit_commit_paid_return(
                           site, *return_plan, surface, surrendered_value );
            }
            payment_failed = true;
            add_msg( m_warning,
                     _( "You do not complete the shakedown payment.  The demand turns into a fight." ) );
        }
    }

    if( response != live_bandit_shakedown_response::pay ) {
        add_msg( m_bad, _( "You choose to fight the shakedown." ) );
    } else if( payment_failed ) {
        add_msg( m_bad, _( "The bandits come at you." ) );
    }
    if( response_is_visit && response == live_bandit_shakedown_response::fight ) {
        return live_bandit_consume_explicit_fight( site, surface, response_operation_id,
                response_generation, response_receiver );
    }
    live_bandit_choose_fight( site, surface, receiver != nullptr ? *receiver : static_cast<Character &>( u ) );
    return true;
}

int live_bandit_current_minutes()
{
    return to_minutes<int>( calendar::turn - calendar::start_of_cataclysm );
}

// Long activities can advance the calendar past one or more five-minute overmap
// callbacks.  A raid which was already gathered must not miss its one night-only
// departure opportunity merely because the next authoritative callback lands after
// dawn.  This intentionally uses the ordinary solar predicate at the cursor
// boundary and the next dusk; it does not give the raid a fixed departure hour.
bool live_bandit_departure_interval_contains_night(
    const bandit_live_world::simulation_advance_cursor &cursor, const int current_minutes )
{
    if( current_minutes <= cursor.last_advanced_minutes ) {
        return false;
    }

    const time_point last_advanced_turn = calendar::start_of_cataclysm +
                                           cursor.last_advanced_minutes * 1_minutes;
    if( is_night( last_advanced_turn ) || is_night( calendar::turn ) ) {
        return true;
    }

    // At least one complete day necessarily contains a night.  This also keeps a
    // stale but otherwise valid operation from scanning an unbounded interval.
    if( current_minutes - cursor.last_advanced_minutes >= to_minutes<int>( 1_days ) ) {
        return true;
    }

    const time_point next_night = night_time( last_advanced_turn );
    return next_night > last_advanced_turn && next_night <= calendar::turn;
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
    const std::optional<int> minimum_distance =
        bandit_live_world::covert_scout_travel_minimum_target_distance( outing );
    if( !minimum_distance || *minimum_distance <= 0 ) {
        exclusions.insert( outing.target_footprint.begin(), outing.target_footprint.end() );
        return exclusions;
    }
    const int radius = *minimum_distance - 1;
    for( const tripoint_abs_omt &target : outing.target_footprint ) {
        for( int dy = -radius; dy <= radius; ++dy ) {
            for( int dx = -radius; dx <= radius; ++dx ) {
                const tripoint_abs_omt candidate( target.x() + dx, target.y() + dy,
                                                  target.z() );
                const std::optional<int> distance =
                    bandit_live_world::target_footprint_watch_distance(
                        candidate, outing.target_footprint );
                if( distance && *distance < *minimum_distance ) {
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
    const std::optional<int> minimum_distance =
        bandit_live_world::covert_scout_travel_minimum_target_distance( outing );
    return minimum_distance &&
           std::all_of( path.begin(), path.end(), [&]( const tripoint_abs_omt &omt ) {
        const std::optional<int> distance =
            bandit_live_world::target_footprint_watch_distance(
                omt, outing.target_footprint );
        return distance && *distance >= *minimum_distance;
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
    bandit_live_world_probe::scoped_loaded_covert_local_path_solve path_solve_probe;
    member_npc.path = get_map().route(
                          member_npc.pos_bub(), destination,
                          member_npc.get_pathfinding_settings( false ),
                          combined_avoid );
    // map::route includes its origin, while npc::move_to_next expects the next tile at
    // the front of the path.  Keep a freshly solved covert route from spending every
    // turn consuming its own current position instead of advancing toward the boundary.
    while( !member_npc.path.empty() &&
            ( member_npc.path.front() == member_npc.pos_bub() ||
              get_map().get_abs( member_npc.path.front() ) == member_npc.pos_abs() ) ) {
        member_npc.path.erase( member_npc.path.begin() );
    }
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
    member_npc.guard_pos.reset();
    member_npc.clear_ai_guard_pos();
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
        member_npc.guard_pos.reset();
        member_npc.clear_ai_guard_pos();
        return true;
    }
    member_npc.omt_path.clear();
    return live_bandit_route_member_to( member_npc, site, site.anchor );
}

bool live_bandit_confirmed_member_death(
    const bandit_live_world::active_outing_state &reservation,
    const character_id member_id, const int current_minutes )
{
    const shared_ptr_fast<npc> actor = overmap_buffer.find_npc( member_id );
    if( actor ) {
        return actor->is_dead();
    }
    // cleanup_dead removes the NPC before the next five-minute approach pass.
    // The saved character_dies statistic retains the numeric identity and turn,
    // including in the pre-repair viewer checkpoint.  Absence alone is not death.
    const auto &deaths = get_stats().get_events( event_type::character_dies ).counts();
    for( const auto &entry : deaths ) {
        const auto character = entry.first.find( "character" );
        if( character == entry.first.end() ||
            character->second.get<character_id>() != member_id ) {
            continue;
        }
        const int death_minutes = to_minutes<int>( entry.second.last -
                                  calendar::start_of_cataclysm );
        return death_minutes >= reservation.started_minutes &&
               death_minutes <= current_minutes;
    }
    return false;
}

bool live_bandit_reconcile_hostile_approach_deaths( bandit_live_world::site_record &site,
        const int current_minutes )
{
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    const bandit_live_world::active_outing_state &reservation = operation.reservation;
    if( !operation.is_active() ||
        ( operation.phase != bandit_live_world::hostile_operation_phase::outbound &&
          operation.phase != bandit_live_world::hostile_operation_phase::rallying &&
          operation.phase != bandit_live_world::hostile_operation_phase::waiting_night &&
          operation.phase != bandit_live_world::hostile_operation_phase::approaching ) ||
        reservation.owner != bandit_live_world::simulation_owner::abstract ||
        current_minutes < reservation.last_advanced_minutes ) {
        return false;
    }
    const std::string activity_id = reservation.activity_id;
    const int generation = reservation.generation;
    std::vector<character_id> confirmed_deaths;
    for( const character_id member_id : reservation.member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member != nullptr && member->state == bandit_live_world::member_state::outbound &&
            !reservation.member_is_resolved( member_id ) &&
            live_bandit_confirmed_member_death( reservation, member_id, current_minutes ) ) {
            confirmed_deaths.push_back( member_id );
        }
    }
    bool changed = false;
    for( const character_id member_id : confirmed_deaths ) {
        changed |= commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::record_matching_external_outing_casualty( next, activity_id, generation,
                    member_id,
                    bandit_live_world::member_state::dead, current_minutes,
                    "authoritative hostile approach death" );
        } );
    }
    if( changed && site.active_hostile_operation.phase ==
        bandit_live_world::hostile_operation_phase::lost ) {
        changed |= commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::release_matching_external_reservation( next, activity_id, generation,
                    "all hostile approach participants died" );
        } ).has_value();
    }
    return changed;
}

bool advance_live_bandit_hostile_rallies()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        changed |= live_bandit_reconcile_hostile_approach_deaths( site, current_minutes );
        bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
        bandit_live_world::active_outing_state &reservation = operation.reservation;
        if( site.retired_empty_site || !operation.is_active() ||
            operation.phase != bandit_live_world::hostile_operation_phase::outbound ||
            reservation.owner != bandit_live_world::simulation_owner::abstract ||
            !operation.has_rally || reservation.shared_route.size() < 3 ||
            reservation.shared_route.front() != site.anchor ||
            std::find( reservation.shared_route.begin() + 1,
                       reservation.shared_route.end() - 1, operation.rally_omt ) ==
            reservation.shared_route.end() - 1 ) {
            continue;
        }
        const tripoint_abs_omt approach = reservation.shared_route.back();
        const int rally_distance = rl_dist( operation.rally_omt, approach );
        if( rally_distance < 2 || rally_distance > 3 ) {
            continue;
        }

        struct hostile_rally_travel_order {
            npc *member_npc;
            std::vector<tripoint_abs_omt> route;
        };
        std::vector<hostile_rally_travel_order> travel_orders;
        bool ready_to_depart = true;
        bool all_at_rally = true;
        int capable_members = 0;
        for( const character_id member_id : reservation.member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( reservation.member_is_resolved( member_id ) ) {
                if( member == nullptr || member->state != bandit_live_world::member_state::dead ) {
                    ready_to_depart = false;
                    break;
                }
                continue;
            }
            ++capable_members;
            npc *member_npc = g->find_npc( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::outbound ||
                member_npc == nullptr || member_npc->is_dead() ) {
                ready_to_depart = false;
                break;
            }
            if( member_npc->pos_abs_omt() == operation.rally_omt ) {
                continue;
            }
            all_at_rally = false;
            if( member_npc->goal == operation.rally_omt && !member_npc->omt_path.empty() &&
                member_npc->is_travelling() ) {
                continue;
            }
            const std::vector<tripoint_abs_omt> route = overmap_buffer.get_travel_path(
                        member_npc->pos_abs_omt(), operation.rally_omt,
                        overmap_path_params::for_npc() ).points;
            if( route.empty() || route.front() != operation.rally_omt ||
                route.back() != member_npc->pos_abs_omt() ) {
                ready_to_depart = false;
                break;
            }
            travel_orders.push_back( { member_npc, route } );
        }
        if( capable_members == 0 || !ready_to_depart || !all_at_rally ) {
            if( ready_to_depart ) {
                for( hostile_rally_travel_order &order : travel_orders ) {
                    order.member_npc->goal = operation.rally_omt;
                    order.member_npc->omt_path = std::move( order.route );
                    order.member_npc->set_mission( NPC_MISSION_TRAVELLING );
                    changed = true;
                }
            }
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor || current_minutes <= cursor->last_advanced_minutes ) {
            if( openclaw_harness_bandit_owner_trace_enabled() ) {
                DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                           << " event=preflight_blocked site=" << site.site_id
                                           << " operation=" << reservation.activity_id
                                           << " generation=" << reservation.generation
                                           << " cursor=" << ( cursor ? "present" : "none" )
                                           << " current_minutes=" << current_minutes
                                           << " last_advanced=" << ( cursor ?
                                               std::to_string( cursor->last_advanced_minutes ) : "none" )
                                           << '\n';
            }
            continue;
        }
        if( openclaw_harness_bandit_owner_trace_enabled() ) {
            DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                       << " event=preflight site=" << site.site_id
                                       << " operation=" << reservation.activity_id
                                       << " generation=" << reservation.generation
                                       << " owner=" << to_string( reservation.owner )
                                       << " member_count=" << reservation.member_ids.size()
                                       << " target=" << reservation.target_id << '\n';
        }
        changed |= commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::transition_hostile_operation_phase( next, *cursor,
                    bandit_live_world::hostile_operation_phase::outbound,
                    bandit_live_world::hostile_operation_phase::rallying, current_minutes,
                    "hostile operation reached persisted rally" );
        } ) ==
        bandit_live_world::hostile_operation_transition_result::applied;
    }
    return changed;
}

void live_bandit_retire_hostile_approach_guard( npc &member_npc )
{
    // Reaching the rally made this actor a guard.  The reserved approach now
    // owns travel, including orders restored from a save while already en route.
    member_npc.guard_pos.reset();
    member_npc.clear_ai_guard_pos();
    member_npc.goto_to_this_pos.reset();
    member_npc.path.clear();
}

std::optional<tripoint_abs_ms> live_bandit_loaded_hostile_entry(
    const npc &member_npc, const tripoint_abs_omt &target,
    const std::vector<tripoint_abs_ms> &reserved_entries )
{
    map &here = get_map();
    const tripoint_bub_ms start = member_npc.pos_bub( here );
    if( !here.inbounds( start ) ) {
        return std::nullopt;
    }
    const tripoint_abs_ms origin = project_to<coords::ms>( target );
    const int edge = coords::map_squares_per( coords::omt ) - 1;
    std::vector<tripoint_abs_ms> candidates;
    for( int y = 0; y <= edge; ++y ) {
        for( int x = 0; x <= edge; ++x ) {
            if( x != 0 && x != edge && y != 0 && y != edge ) {
                continue;
            }
            const tripoint_abs_ms absolute = origin + point( x, y );
            const tripoint_bub_ms local = here.get_bub( absolute );
            if( here.inbounds( local ) && g->is_empty( local ) &&
                std::find( reserved_entries.begin(), reserved_entries.end(), absolute ) ==
                reserved_entries.end() ) {
                candidates.push_back( absolute );
            }
        }
    }
    std::sort( candidates.begin(), candidates.end(), [&member_npc](
    const tripoint_abs_ms & lhs, const tripoint_abs_ms & rhs ) {
        const int lhs_distance = rl_dist( member_npc.pos_abs(), lhs );
        const int rhs_distance = rl_dist( member_npc.pos_abs(), rhs );
        return std::tie( lhs_distance, lhs ) < std::tie( rhs_distance, rhs );
    } );
    for( const tripoint_abs_ms &candidate : candidates ) {
        const tripoint_bub_ms local = here.get_bub( candidate );
        if( !here.route( start, pathfinding_target::point( local ),
                         member_npc.get_pathfinding_settings( false ),
                         member_npc.get_path_avoid() ).empty() ) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool live_bandit_establish_shakedown_communication( bandit_live_world::site_record &site );

bool advance_live_bandit_hostile_approaches()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        changed |= live_bandit_reconcile_hostile_approach_deaths( site, current_minutes );
        changed |= live_bandit_establish_shakedown_communication( site );
        bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
        bandit_live_world::active_outing_state &reservation = operation.reservation;
        if( site.retired_empty_site || !operation.is_active() ||
            ( operation.phase != bandit_live_world::hostile_operation_phase::rallying &&
              operation.phase != bandit_live_world::hostile_operation_phase::waiting_night &&
              operation.phase != bandit_live_world::hostile_operation_phase::approaching ) ||
            reservation.owner != bandit_live_world::simulation_owner::abstract ||
            reservation.member_ids.empty() || reservation.shared_route.empty() ) {
            continue;
        }
        const tripoint_abs_omt approach = reservation.shared_route.back();

        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor || current_minutes <= cursor->last_advanced_minutes ) {
            continue;
        }

        struct hostile_approach_travel_order {
            npc *member_npc;
            std::vector<tripoint_abs_omt> route;
            bool already_travelling;
            std::optional<tripoint_abs_ms> loaded_entry;
        };
        std::vector<hostile_approach_travel_order> travel_orders;
        bool valid_party = true;
        bool all_at_target = true;
        bool approach_progressed = false;
        int capable_members = 0;
        for( const character_id member_id : reservation.member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( reservation.member_is_resolved( member_id ) ) {
                if( member == nullptr || member->state != bandit_live_world::member_state::dead ) {
                    valid_party = false;
                    break;
                }
                continue;
            }
            ++capable_members;
            npc *member_npc = g->find_npc( member_id );
            if( openclaw_harness_bandit_owner_trace_enabled() ) {
                DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                           << " event=member member=" << member_id.get_value()
                                           << " state=" << ( member == nullptr ? "missing" :
                                               std::to_string( static_cast<int>( member->state ) ) )
                                           << " loaded=" << ( member_npc != nullptr ? "true" : "false" )
                                           << " pos=" << ( member_npc != nullptr ?
                                               member_npc->pos_abs_omt().to_string() : "none" )
                                           << " home=" << site.anchor.to_string() << '\n';
            }
            if( member == nullptr || member->state != bandit_live_world::member_state::outbound ||
                member_npc == nullptr || member_npc->is_dead() ) {
                valid_party = false;
                break;
            }
            if( member_npc->pos_abs_omt() == approach ) {
                continue;
            }
            all_at_target = false;
            if( member_npc->goal == approach && !member_npc->omt_path.empty() &&
                member_npc->is_travelling() ) {
                auto next = member_npc->omt_path.rbegin();
                while( next != member_npc->omt_path.rend() && *next == member_npc->pos_abs_omt() ) {
                    ++next;
                }
                if( next != member_npc->omt_path.rend() &&
                    overmap_travel_step_valid( member_npc->pos_abs_omt(), *next,
                                               overmap_path_params::for_npc() ) ) {
                    travel_orders.push_back( { member_npc, {}, true, std::nullopt } );
                    continue;
                }
            }
            const std::vector<tripoint_abs_omt> route = overmap_buffer.get_travel_path(
                        member_npc->pos_abs_omt(), approach,
                        overmap_path_params::for_npc() ).points;
            if( route.empty() || route.front() != approach ||
                route.back() != member_npc->pos_abs_omt() ) {
                valid_party = false;
                break;
            }
            travel_orders.push_back( { member_npc, route, false, std::nullopt } );
        }
        if( !valid_party || capable_members == 0 ) {
            continue;
        }

        if( operation.phase == bandit_live_world::hostile_operation_phase::approaching &&
            !all_at_target ) {
            map &here = get_map();
            const tripoint_bub_ms target_center = here.get_bub(
                    project_to<coords::ms>( approach ) + point( SEEX, SEEY ) );
            if( here.inbounds( target_center ) ) {
                std::vector<tripoint_abs_ms> reserved_entries;
                for( hostile_approach_travel_order &order : travel_orders ) {
                    const std::vector<tripoint_abs_omt> &path = order.already_travelling ?
                            order.member_npc->omt_path : order.route;
                    auto next = path.rbegin();
                    while( next != path.rend() && *next == order.member_npc->pos_abs_omt() ) {
                        ++next;
                    }
                    if( next == path.rend() || *next != approach ) {
                        continue;
                    }
                    order.loaded_entry = live_bandit_loaded_hostile_entry(
                                             *order.member_npc, approach, reserved_entries );
                    if( !order.loaded_entry ) {
                        valid_party = false;
                        break;
                    }
                    reserved_entries.push_back( *order.loaded_entry );
                }
            }
            if( !valid_party ) {
                continue;
            }
        }

        if( operation.phase == bandit_live_world::hostile_operation_phase::rallying ||
            operation.phase == bandit_live_world::hostile_operation_phase::waiting_night ) {
            if( std::any_of( reservation.member_ids.begin(), reservation.member_ids.end(),
            [&operation, &reservation]( const character_id member_id ) {
                if( reservation.member_is_resolved( member_id ) ) {
                    return false;
                }
                npc *member_npc = g->find_npc( member_id );
                return member_npc == nullptr || member_npc->pos_abs_omt() != operation.rally_omt;
            } ) ) {
                continue;
            }
            if( ( operation.operation_kind == bandit_live_world::hostile_operation_kind::raid ||
                  operation.phase == bandit_live_world::hostile_operation_phase::waiting_night ) &&
                !live_bandit_departure_interval_contains_night( *cursor, current_minutes ) ) {
                continue;
            }
            for( hostile_approach_travel_order &order : travel_orders ) {
                live_bandit_retire_hostile_approach_guard( *order.member_npc );
                if( !order.already_travelling ) {
                    order.member_npc->goal = approach;
                    order.member_npc->omt_path = std::move( order.route );
                    order.member_npc->set_mission( NPC_MISSION_TRAVELLING );
                }
                changed = true;
            }
            changed |= commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::transition_hostile_operation_phase( next, *cursor,
                        operation.phase,
                        bandit_live_world::hostile_operation_phase::approaching, current_minutes,
                        "hostile operation openly approached reported site entrance" );
            } ) ==
            bandit_live_world::hostile_operation_transition_result::applied;
            continue;
        }

        if( !all_at_target ) {
            for( hostile_approach_travel_order &order : travel_orders ) {
                live_bandit_retire_hostile_approach_guard( *order.member_npc );
                if( !order.already_travelling ) {
                    order.member_npc->goal = approach;
                    order.member_npc->omt_path = std::move( order.route );
                    order.member_npc->set_mission( NPC_MISSION_TRAVELLING );
                }
                changed = true;
                while( !order.member_npc->omt_path.empty() &&
                       order.member_npc->omt_path.back() == order.member_npc->pos_abs_omt() ) {
                    order.member_npc->omt_path.pop_back();
                }
                if( !order.member_npc->omt_path.empty() ) {
                    if( operation.operation_kind == bandit_live_world::hostile_operation_kind::shakedown &&
                        order.member_npc->is_active() ) {
                        // Once loaded, the ordinary NPC path/action motor owns
                        // every physical step and its survival interruptions.
                        // An abstract reservation must not jump that actor.
                        continue;
                    }
                    const tripoint_abs_omt before = order.member_npc->pos_abs_omt();
                    order.member_npc->travel_overmap( order.member_npc->omt_path.back() );
                    if( order.loaded_entry ) {
                        order.member_npc->spawn_at_precise( *order.loaded_entry );
                        live_bandit_retire_hostile_approach_guard( *order.member_npc );
                    }
                    if( order.member_npc->pos_abs_omt() != before ) {
                        changed = true;
                        approach_progressed = true;
                    }
                }
            }
            if( approach_progressed ) {
                changed |= commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                    return bandit_live_world::record_hostile_operation_approach_progress( next, *cursor,
                            current_minutes );
                } );
            }
            continue;
        }
        if( operation.operation_kind == bandit_live_world::hostile_operation_kind::shakedown ) {
            // Reaching an overmap destination is not a locally simulated
            // encounter. Keep this exact reservation; unloaded time cannot
            // consume search attempts or become refusal.
            const auto destination = get_map().get_bub(
                                         project_to<coords::ms>( approach ) + point( SEEX, SEEY ) );
            if( !get_map().inbounds( destination ) ) {
                changed |= !operation.shakedown_waiting_local;
                operation.shakedown_waiting_local = true;
                continue;
            }
            operation.shakedown_waiting_local = false;
            g->load_npcs();
        }
        // The ownership transition and the physical NPC leases must agree in
        // the next save.  Preflight every persistent actor before changing either.
        std::vector<shared_ptr_fast<npc>> contact_members;
        for( const character_id member_id : reservation.member_ids ) {
            if( reservation.member_is_resolved( member_id ) ) {
                continue;
            }
            shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( !member || member->is_dead() ) {
                valid_party = false;
                break;
            }
            contact_members.push_back( member );
        }
        if( !valid_party ) {
            continue;
        }
        bandit_live_world::site_record contact_site = site;
        if( bandit_live_world::transition_hostile_operation_phase( contact_site, *cursor,
                   bandit_live_world::hostile_operation_phase::approaching,
                   bandit_live_world::hostile_operation_phase::committed_contact, current_minutes,
                   "hostile operation physically reached target contact" ) ==
            bandit_live_world::hostile_operation_transition_result::applied ) {
            if( !persist_live_bandit_local_projection_leases( contact_site, &site ) ) {
                continue;
            }
            site = std::move( contact_site );
            changed = true;
        }
    }
    return changed;
}

bool advance_live_bandit_hostile_returns()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
        bandit_live_world::active_outing_state &reservation = operation.reservation;
        if( site.retired_empty_site || !operation.is_active() ||
            operation.phase != bandit_live_world::hostile_operation_phase::returning_home ||
            reservation.owner != bandit_live_world::simulation_owner::abstract ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor || current_minutes <= cursor->last_advanced_minutes ) {
            continue;
        }
        struct hostile_return_travel_order {
            npc *member_npc;
            std::vector<tripoint_abs_omt> route;
        };
        std::vector<hostile_return_travel_order> travel_orders;
        bool valid_party = true;
        bool all_home = true;
        for( const character_id member_id : reservation.member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( reservation.member_is_resolved( member_id ) ) {
                if( openclaw_harness_bandit_owner_trace_enabled() ) {
                    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                               << " event=member_resolved member=" << member_id.get_value()
                                               << " state=" << ( member == nullptr ? "missing" :
                                                   std::to_string( static_cast<int>( member->state ) ) )
                                               << " casualty=" << ( std::find( reservation.casualty_ids.begin(),
                                                   reservation.casualty_ids.end(), member_id ) !=
                                                   reservation.casualty_ids.end() ? "true" : "false" )
                                               << '\n';
                }
                const bool casualty = std::find( reservation.casualty_ids.begin(),
                                                 reservation.casualty_ids.end(), member_id ) !=
                                      reservation.casualty_ids.end();
                if( member == nullptr || ( casualty ?
                                           member->state != bandit_live_world::member_state::dead :
                                           member->state != bandit_live_world::member_state::at_home ) ) {
                    valid_party = false;
                }
                continue;
            }
            npc *member_npc = g->find_npc( member_id );
            if( openclaw_harness_bandit_owner_trace_enabled() ) {
                DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                           << " event=member_state member=" << member_id.get_value()
                                           << " resolved=false state=" << ( member == nullptr ? "missing" :
                                               std::to_string( static_cast<int>( member->state ) ) )
                                           << " npc_loaded=" << ( member_npc == nullptr ? "false" : "true" )
                                           << " npc_dead=" << ( member_npc != nullptr && member_npc->is_dead() ?
                                               "true" : "false" ) << '\n';
            }
            if( member == nullptr || member->state != bandit_live_world::member_state::outbound ||
                member_npc == nullptr || member_npc->is_dead() ) {
                valid_party = false;
                all_home = false;
                break;
            }
            if( member_npc->pos_abs_omt() == site.anchor ) {
                continue;
            }
            all_home = false;
            if( member_npc->goal == site.anchor && !member_npc->omt_path.empty() &&
                member_npc->is_travelling() ) {
                continue;
            }
            const std::vector<tripoint_abs_omt> route = overmap_buffer.get_travel_path(
                        member_npc->pos_abs_omt(), site.anchor,
                        overmap_path_params::for_npc() ).points;
            if( route.empty() || route.front() != site.anchor ||
                route.back() != member_npc->pos_abs_omt() ) {
                if( openclaw_harness_bandit_owner_trace_enabled() ) {
                    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                               << " event=route_rejected member=" << member_id.get_value()
                                               << " route_size=" << route.size()
                                               << " route_front=" << ( route.empty() ? "none" :
                                                   route.front().to_string() )
                                               << " route_back=" << ( route.empty() ? "none" :
                                                   route.back().to_string() ) << '\n';
                }
                valid_party = false;
                break;
            }
            travel_orders.push_back( { member_npc, route } );
        }
        if( !valid_party ) {
            if( openclaw_harness_bandit_owner_trace_enabled() ) {
                DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                           << " event=handoff_blocked site=" << site.site_id << '\n';
            }
            continue;
        }
        if( !all_home ) {
            for( hostile_return_travel_order &order : travel_orders ) {
                order.member_npc->goal = site.anchor;
                order.member_npc->omt_path = std::move( order.route );
                order.member_npc->set_mission( NPC_MISSION_TRAVELLING );
                changed = true;
            }
            if( openclaw_harness_bandit_owner_trace_enabled() ) {
                DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                           << " event=route_assigned site=" << site.site_id
                                           << " orders=" << travel_orders.size() << '\n';
            }
            continue;
        }
        const std::string activity_id = reservation.activity_id;
        const int generation = reservation.generation;
        if( operation.operation_kind == bandit_live_world::hostile_operation_kind::shakedown &&
            !bandit_live_world::apply_terminal_hostile_shakedown_aftermath( state, site, activity_id,
                    generation ) ) {
            continue;
        }
        const bool released = commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record &
        next ) {
            return bandit_live_world::release_matching_external_reservation( next, activity_id,
                    generation, operation.operation_kind ==
                    bandit_live_world::hostile_operation_kind::raid ?
                    "cannibal raid survivors physically returned home" :
                    "paid shakedown survivors physically returned home" );
        } ).has_value();
        changed |= released;
        if( openclaw_harness_bandit_owner_trace_enabled() ) {
            DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_ui_trace: component=bandit_return"
                                       << " event=terminal_release site=" << site.site_id
                                       << " operation=" << activity_id
                                       << " generation=" << generation
                                       << " released=" << ( released ? "true" : "false" ) << '\n';
        }
    }
    return changed;
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
        commit_live_bandit_local_progress(
    *owner, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::resolve_covert_scout_burned_egress_failure( next, *cursor,
                retry_candidates, live_bandit_current_minutes() );
    } );
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
        commit_live_bandit_local_progress(
    *owner, [&]( bandit_live_world::site_record & next ) {
    return bandit_live_world::abort_local_pair_alternate_watch_reposition( next, *cursor,
            live_bandit_current_minutes(),
            "alternate watch physical route became unavailable" );
    } ) !=
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
    if( member_npc.get_attitude() == NPCATT_FLEE || member_npc.get_attitude() == NPCATT_FLEE_TEMP ||
        member_npc.goto_to_this_pos == decision.destination ) {
        return false;
    }
    member_npc.guard_pos.reset();
    member_npc.clear_ai_guard_pos();
    member_npc.goto_to_this_pos = decision.destination;
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

bool live_bandit_note_combat_intent( npc &member_npc,
                                    const bandit_live_world::site_record &site,
                                    const bandit_live_world::local_gate_input &,
                                    const bandit_live_world::local_gate_decision &gate_decision )
{
    if( !gate_decision.combat_forward || member_npc.get_attitude() == NPCATT_KILL ||
        member_npc.get_attitude() == NPCATT_FLEE || member_npc.get_attitude() == NPCATT_FLEE_TEMP ) {
        return false;
    }
    // The gate can release combat intent; only the normal NPC action loop may
    // select a seen target, attack or move.  Never overwrite individual fear.
    member_npc.set_attitude( NPCATT_KILL );
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world combat intent released"
                               << " site=" << site.site_id
                               << " npc=" << member_npc.getID().get_value() << '\n';
    return true;
}

bool materialize_committed_cannibal_raid( bandit_live_world::site_record &site )
{
    bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( site.retired_empty_site || outing == nullptr || outing != &site.active_hostile_operation.reservation ||
        !site.active_hostile_operation.is_active() ||
        site.active_hostile_operation.operation_kind != bandit_live_world::hostile_operation_kind::raid ||
        site.active_hostile_operation.phase != bandit_live_world::hostile_operation_phase::committed_contact ||
        outing->owner != bandit_live_world::simulation_owner::local ) {
        return false;
    }
    map &here = get_map();
    std::vector<npc *> party;
    party.reserve( outing->member_ids.size() );
    std::set<tripoint_abs_ms> occupied_positions;
    bool needs_load = false;
    for( const character_id member_id : outing->member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( outing->member_is_resolved( member_id ) ) {
            if( member == nullptr ||
                ( member->state != bandit_live_world::member_state::dead &&
                  member->state != bandit_live_world::member_state::at_home ) ) {
                return false;
            }
            continue;
        }
        if( std::find( site.active_hostile_operation.withdrawing_member_ids.begin(),
                       site.active_hostile_operation.withdrawing_member_ids.end(), member_id ) !=
            site.active_hostile_operation.withdrawing_member_ids.end() ) {
            continue;
        }
        npc *member_npc = g->find_npc( member_id );
        if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ||
            member_npc == nullptr || member_npc->is_dead() ) {
            return false;
        }
        if( !member_npc->is_active() ) {
            if( outing->shared_route.empty() ||
                member_npc->pos_abs_omt() != outing->shared_route.back() ||
                !occupied_positions.insert( member_npc->pos_abs() ).second ) {
                return false;
            }
            const tripoint_bub_ms position = member_npc->pos_bub( here );
            if( !here.inbounds( position ) || !g->is_empty( position ) ) {
                return false;
            }
            needs_load = true;
        }
        party.push_back( member_npc );
    }
    if( party.empty() || !needs_load ) {
        return false;
    }
    // Approach already placed these exact actors.  Loading their physical
    // positions must not relocate them across a wall toward the OMT center.
    g->load_npcs();
    return std::all_of( party.begin(), party.end(), []( const npc * member_npc ) {
        return member_npc->is_active();
    } );
}

bool active_local_contact_member( const bandit_live_world::site_record &site,
                                  const character_id member_id,
                                  const shared_ptr_fast<npc> &member_npc )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( member_npc == nullptr || site.retired_empty_site || outing == nullptr ||
        outing->owner != bandit_live_world::simulation_owner::local ||
        std::find( outing->member_ids.begin(), outing->member_ids.end(), member_id ) ==
        outing->member_ids.end() ) {
        return false;
    }
    const bandit_live_world::member_record *member = site.find_member( member_id );
    if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ||
        member_npc->is_dead() || overmap_buffer.find_npc( member_id ) != member_npc ) {
        return false;
    }

    map &here = get_map();
    if( !member_npc->is_active() || !here.inbounds( member_npc->pos_bub( here ) ) ) {
        return false;
    }
    // all_npcs is the active tracker view.  Count the exact object rather than
    // accepting an ID lookup, which can resolve an inactive overmap record.
    int tracker_entries = 0;
    for( npc &active_npc : g->all_npcs() ) {
        if( &active_npc == member_npc.get() ) {
            ++tracker_entries;
        }
    }
    return tracker_entries == 1;
}

bool active_committed_bandit_shakedown_member( const bandit_live_world::site_record &site,
        const character_id member_id, const shared_ptr_fast<npc> &member_npc )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    return outing == &site.active_hostile_operation.reservation &&
           site.active_hostile_operation.is_active() &&
           site.active_hostile_operation.operation_kind ==
           bandit_live_world::hostile_operation_kind::shakedown &&
           site.active_hostile_operation.phase ==
           bandit_live_world::hostile_operation_phase::committed_contact &&
           active_local_contact_member( site, member_id, member_npc );
}

bool active_committed_bandit_shakedown_member( const bandit_live_world::site_record &site,
        const character_id member_id )
{
    return active_committed_bandit_shakedown_member( site, member_id,
            overmap_buffer.find_npc( member_id ) );
}

bool materialize_committed_bandit_shakedown( bandit_live_world::site_record &site )
{
    bandit_live_world::active_outing_state *outing = site.active_external_outing();
    const auto reject = [&site]( const std::string_view reason, const std::string_view observed ) {
        // This is run-bound admission instrumentation, not gameplay evidence or state mutation.
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world shakedown_materialization_rejected"
                                   << " reason=" << reason
                                   << " site=" << site.site_id
                                   << " observed=" << observed
                                   << " outcome_credit=none";
        return false;
    };
    if( site.retired_empty_site ) {
        return reject( "retired_empty_site", "site_retired=yes" );
    }
    if( outing == nullptr ) {
        return reject( "no_active_external_outing", "outing=none" );
    }
    if( outing != &site.active_hostile_operation.reservation ) {
        return reject( "outing_not_hostile_reservation", "reservation_identity=mismatch" );
    }
    if( !site.active_hostile_operation.is_active() ) {
        return reject( "hostile_operation_inactive", "operation_active=no" );
    }
    if( site.active_hostile_operation.operation_kind !=
        bandit_live_world::hostile_operation_kind::shakedown ) {
        return reject( "operation_kind_not_shakedown", "operation_kind=mismatch" );
    }
    if( site.active_hostile_operation.phase !=
        bandit_live_world::hostile_operation_phase::committed_contact ) {
        return reject( "operation_phase_not_committed_contact", "phase=mismatch" );
    }
    if( outing->owner != bandit_live_world::simulation_owner::local ) {
        return reject( "outing_not_locally_owned", "owner=nonlocal" );
    }

    map &here = get_map();
    const tripoint_bub_ms avatar_pos = get_avatar().pos_bub( here );
    // Admission owns a *loaded* local scene.  A test or map transition can
    // temporarily leave the avatar coordinate rebased against a different
    // bubble; placing persistent NPCs into that map would strand them at an
    // unrelated absolute location.  Leave the committed reservation intact
    // until the player and the loaded map agree.
    if( here.get_abs( avatar_pos ) != get_avatar().pos_abs() ) {
        return reject( "avatar_not_in_loaded_map", "avatar_map_coordinates_mismatch" );
    }
    if( outing->shared_route.empty() ) {
        return reject( "approach_route_missing", "shared_route=empty" );
    }
    const tripoint_bub_ms target_pos = here.get_bub(
                                           project_to<coords::ms>( outing->shared_route.back() ) +
                                           point( SEEX, SEEY ) );
    if( !here.inbounds( target_pos ) ) {
        return reject( "target_not_in_loaded_map", "target_site_outside_local_bubble" );
    }
    std::vector<shared_ptr_fast<npc>> party;
    party.reserve( outing->member_ids.size() );
    for( std::size_t member_index = 0; member_index < outing->member_ids.size(); ++member_index ) {
        const character_id member_id = outing->member_ids[member_index];
        const bandit_live_world::member_record *member = site.find_member( member_id );
        const shared_ptr_fast<npc> member_npc = overmap_buffer.find_npc( member_id );
        if( member == nullptr ) {
            return reject( "member_record_missing", "member_index=" + std::to_string( member_index ) );
        }
        if( member->state != bandit_live_world::member_state::local_contact ) {
            return reject( "member_not_local_contact", "member_index=" + std::to_string( member_index ) );
        }
        if( !member_npc ) {
            return reject( "overmap_npc_missing", "member_index=" + std::to_string( member_index ) );
        }
        if( member_npc->is_dead() ) {
            return reject( "member_dead", "member_index=" + std::to_string( member_index ) );
        }
        party.push_back( member_npc );
    }

    const bool any_active = std::any_of( party.begin(), party.end(), []( const shared_ptr_fast<npc> & member ) {
        return member->is_active();
    } );
    if( any_active ) {
        const bool complete_admission = std::all_of( outing->member_ids.begin(), outing->member_ids.end(),
        [&site]( const character_id member_id ) {
            return active_committed_bandit_shakedown_member( site, member_id );
        } );
        if( complete_admission && persist_live_bandit_local_projection_leases( site, &site, false ) ) {
            // An already admitted party retains its local posture and receives
            // neither a second placement nor an attitude reset.
            return true;
        }
        return reject( "partial_or_invalid_existing_admission",
                       "party_size=" + std::to_string( party.size() ) );
    }

    std::vector<tripoint_bub_ms> placements;
    placements.reserve( party.size() );
    for( const tripoint_bub_ms &candidate : closest_points_first( target_pos, 2 ) ) {
        if( candidate == target_pos || !here.inbounds( candidate ) || !g->is_empty( candidate ) ) {
            continue;
        }
        placements.push_back( candidate );
        if( placements.size() == party.size() ) {
            break;
        }
    }
    if( placements.size() != party.size() ) {
        return reject( "insufficient_empty_local_placements",
                       "party_size=" + std::to_string( party.size() ) +
                       ",placement_count=" + std::to_string( placements.size() ) );
    }

    const auto lease = live_bandit_projection_lease( site.site_id, *outing );
    const bool all_unclaimed = std::all_of( party.begin(), party.end(), []( const auto & member ) {
        return live_bandit_projection_copies_match( *member, bandit_live_world_projection_lease() );
    } );
    const bool all_current = std::all_of( party.begin(), party.end(), [&lease]( const auto & member ) {
        return live_bandit_projection_copies_match( *member, lease );
    } );
    if( ( !all_unclaimed && !all_current ) || !persist_live_bandit_local_projection_leases( site ) ) {
        return reject( "projection_claims_do_not_match_admission",
                       "complete_current_or_unclaimed_party_required" );
    }

    for( std::size_t index = 0; index < party.size(); ++index ) {
        party[index]->setpos( here, placements[index] );
    }
    // setpos only relocates the persistent NPC.  The canonical loader owns
    // tracker insertion and on_load; invoke it once after the whole party has
    // passed placement validation so one member cannot affect another's slot.
    // on_load may restore a template's faction-derived hostile attitude, so
    // establish the pre-dialogue neutral posture only after that canonical
    // lifecycle hook has completed.
    g->load_npcs();
    for( const shared_ptr_fast<npc> &member_npc : party ) {
        if( !member_npc->hit_by_player &&
            member_npc->get_attitude() != NPCATT_FLEE &&
            member_npc->get_attitude() != NPCATT_FLEE_TEMP &&
            bandit_live_world::is_active_shakedown_parley_member(
                overmap_buffer.global_state.bandit_live_world, member_npc->getID() ) ) {
            member_npc->set_attitude( NPCATT_NULL );
        }
    }
    const bool complete_admission = std::all_of( outing->member_ids.begin(), outing->member_ids.end(),
    [&site]( const character_id member_id ) {
        return active_committed_bandit_shakedown_member( site, member_id );
    } );
    if( !complete_admission ) {
        return reject( "canonical_admission_incomplete",
                       "party_size=" + std::to_string( party.size() ) );
    }
    return true;
}

bool materialize_live_bandit_committed_shakedown_contacts()
{
    bool changed = false;
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    for( bandit_live_world::site_record &site : state.sites ) {
        bandit_live_world::active_outing_state *outing = site.active_external_outing();
        if( site.retired_empty_site || outing == nullptr ||
            outing != &site.active_hostile_operation.reservation ||
            !site.active_hostile_operation.is_active() ||
            site.active_hostile_operation.operation_kind !=
            bandit_live_world::hostile_operation_kind::shakedown ||
            site.active_hostile_operation.phase !=
            bandit_live_world::hostile_operation_phase::committed_contact ||
            outing->owner != bandit_live_world::simulation_owner::local ) {
            continue;
        }
        changed |= materialize_committed_bandit_shakedown( site );
    }
    return changed;
}

bool live_bandit_reconcile_hostile_shakedown_combat( bandit_live_world::site_record &site )
{
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    bandit_live_world::active_outing_state &reservation = operation.reservation;
    if( !operation.is_active() || operation.operation_kind !=
        bandit_live_world::hostile_operation_kind::shakedown ||
        operation.phase != bandit_live_world::hostile_operation_phase::committed_contact ||
        reservation.owner != bandit_live_world::simulation_owner::local ||
        site.last_shakedown_outcome != "fight_unresolved" ) {
        return false;
    }

    const int current_minutes = live_bandit_current_minutes();
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( site );
    if( !cursor || current_minutes <= cursor->last_advanced_minutes ) {
        return false;
    }
    std::vector<character_id> dead_member_ids;
    for( const character_id member_id : reservation.member_ids ) {
        if( reservation.member_is_resolved( member_id ) ) {
            continue;
        }
        if( live_bandit_confirmed_member_death( reservation, member_id,
                                                current_minutes ) ) {
            dead_member_ids.push_back( member_id );
        }
    }
    if( !dead_member_ids.empty() ) {
        const std::string activity_id = reservation.activity_id;
        const int generation = reservation.generation;
        if( !commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::reconcile_matching_hostile_operation_deaths( next, *cursor,
                dead_member_ids, current_minutes,
                "authoritative shakedown combat death" );
        } ) ) {
            return false;
        }
        if( site.active_hostile_operation.phase ==
            bandit_live_world::hostile_operation_phase::lost ) {
            if( !bandit_live_world::apply_terminal_hostile_shakedown_aftermath(
                    overmap_buffer.global_state.bandit_live_world, site,
                    activity_id, generation ) ) {
                return false;
            }
            return commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::release_matching_external_reservation( next,
                        activity_id, generation,
                        "all shakedown combat participants died" );
            } ).has_value();
        }
        return true;
    }
    // Choosing Fight commits the survivors to native combat.  A single loss
    // does not silently turn the remaining attackers into a returning party.
    return false;
}

bool live_cannibal_raid_reconcile_combat( bandit_live_world::site_record &site )
{
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    bandit_live_world::active_outing_state &reservation = operation.reservation;
    if( !operation.is_active() || operation.operation_kind !=
        bandit_live_world::hostile_operation_kind::raid ||
        operation.phase != bandit_live_world::hostile_operation_phase::committed_contact ||
        reservation.owner != bandit_live_world::simulation_owner::local ) {
        return false;
    }
    const int current_minutes = live_bandit_current_minutes();
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( site );
    if( !cursor || current_minutes <= cursor->last_advanced_minutes ) {
        return false;
    }
    std::vector<character_id> dead_member_ids;
    for( const character_id member_id : reservation.member_ids ) {
        if( reservation.member_is_resolved( member_id ) ) {
            continue;
        }
        if( live_bandit_confirmed_member_death( reservation, member_id,
                                                current_minutes ) ) {
            dead_member_ids.push_back( member_id );
        }
    }
    if( !dead_member_ids.empty() ) {
        const std::string activity_id = reservation.activity_id;
        const int generation = reservation.generation;
        if( !commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::reconcile_matching_hostile_operation_deaths( next, *cursor,
                dead_member_ids, current_minutes,
                "authoritative cannibal raid combat death" );
        } ) ) {
            return false;
        }
        if( site.active_hostile_operation.phase ==
            bandit_live_world::hostile_operation_phase::lost ) {
            return commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::release_matching_external_reservation( next,
                        activity_id, generation,
                        "all cannibal raid combat participants died" );
            } ).has_value();
        }
        return true;
    }
    // Surviving raiders remain in committed contact for their own NPC attack
    // decisions; only an entirely dead party resolves this operation as lost.
    return false;
}

constexpr std::array<point, 5> live_bandit_site_search_waypoints = {
    point( SEEX, SEEY ), point( SEEX / 2, SEEY / 2 ),
    point( SEEX + SEEX / 2, SEEY / 2 ),
    point( SEEX + SEEX / 2, SEEY + SEEY / 2 ),
    point( SEEX / 2, SEEY + SEEY / 2 )
};

bool live_cannibal_raid_member_is_fleeing( const npc &member )
{
    return member.get_attitude() == NPCATT_FLEE ||
           member.get_attitude() == NPCATT_FLEE_TEMP ||
           member.has_effect( effect_npc_flee_player ) ||
           member.has_effect( effect_npc_run_away );
}

bool live_cannibal_raid_advance_withdrawals( bandit_live_world::site_record &site )
{
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    bandit_live_world::active_outing_state &reservation = operation.reservation;
    const tripoint_abs_omt ground_approach = reservation.shared_route.empty() ?
                                             tripoint_abs_omt::invalid : reservation.shared_route.back();
    if( ground_approach == tripoint_abs_omt::invalid ) {
        return false;
    }
    map &here = get_map();
    bool changed = false;
    for( const character_id member_id : reservation.member_ids ) {
        if( reservation.member_is_resolved( member_id ) ) {
            continue;
        }
        bandit_live_world::member_record *record = site.find_member( member_id );
        const shared_ptr_fast<npc> actor = overmap_buffer.find_npc( member_id );
        if( record == nullptr || actor == nullptr || actor->is_dead() ) {
            continue;
        }
        const bool withdrawing = std::find( operation.withdrawing_member_ids.begin(),
                                            operation.withdrawing_member_ids.end(), member_id ) !=
                                 operation.withdrawing_member_ids.end();
        if( !withdrawing && record->state == bandit_live_world::member_state::local_contact &&
            active_local_contact_member( site, member_id, actor ) &&
            live_cannibal_raid_member_is_fleeing( *actor ) ) {
            operation.withdrawing_member_ids.push_back( member_id );
            actor->goto_to_this_pos.reset();
            actor->path.clear();
            actor->guard_pos.reset();
            actor->clear_ai_guard_pos();
            actor->goal = npc::no_goal_point;
            actor->omt_path.clear();
            actor->set_mission( NPC_MISSION_NULL );
            changed = true;
        }
        if( std::find( operation.withdrawing_member_ids.begin(),
                       operation.withdrawing_member_ids.end(), member_id ) ==
            operation.withdrawing_member_ids.end() ) {
            continue;
        }
        if( actor->pos_abs_omt() == site.anchor ) {
            if( !bandit_live_world::update_member_state( site, member_id,
                    bandit_live_world::member_state::at_home,
                    "frightened hostile member physically returned home" ) ) {
                continue;
            }
            reservation.resolved_member_ids.push_back( member_id );
            sync_bandit_live_world_projection_lease_copies( *actor,
                    bandit_live_world_projection_lease() );
            actor->goto_to_this_pos.reset();
            actor->path.clear();
            actor->omt_path.clear();
            actor->goal = npc::no_goal_point;
            actor->set_mission( NPC_MISSION_NULL );
            actor->set_attitude( NPCATT_NULL );
            changed = true;
            continue;
        }
        if( actor->pos_abs_omt().z() != ground_approach.z() ) {
            if( !actor->is_active() || !here.inbounds( actor->pos_bub( here ) ) ) {
                continue;
            }
            std::optional<tripoint_abs_ms> best_goal;
            std::vector<tripoint_bub_ms> best_path;
            for( const point &offset : live_bandit_site_search_waypoints ) {
                const tripoint_abs_ms goal = project_to<coords::ms>( ground_approach ) + offset;
                const tripoint_bub_ms local_goal = here.get_bub( goal );
                if( !here.inbounds( local_goal ) || !here.passable( local_goal ) ) {
                    continue;
                }
                std::vector<tripoint_bub_ms> route = here.route(
                            actor->pos_bub( here ), pathfinding_target::point( local_goal ),
                            actor->get_pathfinding_settings( false ), actor->get_path_avoid() );
                if( !route.empty() && ( best_path.empty() || route.size() < best_path.size() ) ) {
                    best_goal = goal;
                    best_path = std::move( route );
                }
            }
            if( best_goal && actor->goto_to_this_pos != best_goal ) {
                actor->goto_to_this_pos = best_goal;
                actor->path = std::move( best_path );
                actor->omt_path.clear();
                actor->goal = npc::no_goal_point;
                actor->set_mission( NPC_MISSION_NULL );
                changed = true;
            } else if( !best_goal ) {
                actor->goto_to_this_pos.reset();
                actor->path.clear();
            }
            continue;
        }
        if( actor->goal == site.anchor && !actor->omt_path.empty() && actor->is_travelling() ) {
            continue;
        }
        std::vector<tripoint_abs_omt> route = overmap_buffer.get_travel_path(
                    actor->pos_abs_omt(), site.anchor, overmap_path_params::for_npc() ).points;
        if( route.empty() || route.front() != site.anchor ||
            route.back() != actor->pos_abs_omt() ) {
            continue;
        }
        actor->goto_to_this_pos.reset();
        actor->path.clear();
        actor->goal = site.anchor;
        actor->omt_path = std::move( route );
        actor->set_mission( NPC_MISSION_TRAVELLING );
        changed = true;
    }
    return changed;
}

bool live_cannibal_raid_advance_site_search( bandit_live_world::site_record &site )
{
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    const bandit_live_world::active_outing_state &outing = operation.reservation;
    const bool fight_shakedown = operation.operation_kind ==
                                 bandit_live_world::hostile_operation_kind::shakedown &&
                                 site.last_shakedown_outcome == "fight_unresolved";
    if( ( operation.operation_kind != bandit_live_world::hostile_operation_kind::raid &&
          !fight_shakedown ) ||
        operation.phase != bandit_live_world::hostile_operation_phase::committed_contact ||
        outing.owner != bandit_live_world::simulation_owner::local ) {
        return false;
    }
    const bool withdrawal_changed = operation.operation_kind ==
                                    bandit_live_world::hostile_operation_kind::raid &&
                                    live_cannibal_raid_advance_withdrawals( site );
    if( std::all_of( outing.member_ids.begin(), outing.member_ids.end(),
    [&outing]( const character_id id ) {
    return outing.member_is_resolved( id );
    } ) ) {
        return commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::release_matching_external_reservation( next,
                    outing.activity_id, outing.generation,
                    "all raid participants died or physically returned home" );
        } ).has_value();
    }
    std::vector<npc *> party;
    for( const character_id member_id : outing.member_ids ) {
        if( std::find( operation.withdrawing_member_ids.begin(),
                       operation.withdrawing_member_ids.end(), member_id ) !=
            operation.withdrawing_member_ids.end() ) {
            continue;
        }
        const shared_ptr_fast<npc> member_npc = overmap_buffer.find_npc( member_id );
        if( active_local_contact_member( site, member_id, member_npc ) ) {
            party.push_back( member_npc.get() );
        }
    }
    const auto trace_search = [&]( const std::string &reason, const npc *trigger,
                                   const Creature *seen, const std::vector<npc *> &recipients ) {
        raid_decision_trace::recorder &trace = raid_decision_trace::native_recorder();
        if( !trace.enabled() ) {
            return;
        }
        const auto q = raid_decision_trace::quote;
        std::ostringstream payload;
        payload << "\"operation_id\":" << q( outing.activity_id )
                << ",\"generation\":" << outing.generation
                << ",\"site_id\":" << q( site.site_id )
                << ",\"reason\":" << q( reason )
                << ",\"waypoint\":" << operation.site_search_waypoint
                << ",\"approach_omt\":" << q( outing.shared_route.empty() ?
                    outing.target_omt.to_string() : outing.shared_route.back().to_string() )
                << ",\"report_target_omt\":" << q( outing.target_omt.to_string() )
                << ",\"waypoint_attempted\":" << ( operation.site_search_waypoint_attempted ?
                    "true" : "false" )
                << ",\"trigger_actor_id\":";
        if( trigger != nullptr ) {
            payload << trigger->getID().get_value();
        } else {
            payload << "null";
        }
        payload << ",\"seen_target\":";
        if( seen != nullptr ) {
            payload << "{\"type\":" << q( seen->is_avatar() ? "avatar" :
                        dynamic_cast<const npc *>( seen ) != nullptr ? "npc" : "monster" )
                    << ",\"id\":";
            if( const Character *person = dynamic_cast<const Character *>( seen ) ) {
                payload << person->getID().get_value();
            } else {
                payload << "null";
            }
            payload << '}';
        } else {
            payload << "null";
        }
        payload << ",\"recipients\":[";
        bool first = true;
        for( const npc *member : recipients ) {
            if( !first ) {
                payload << ',';
            }
            first = false;
            payload << "{\"id\":" << member->getID().get_value() << ",\"goto_abs\":";
            if( member->goto_to_this_pos ) {
                payload << '[' << member->goto_to_this_pos->x() << ','
                        << member->goto_to_this_pos->y() << ','
                        << member->goto_to_this_pos->z() << ']';
            } else {
                payload << "null";
            }
            payload << '}';
        }
        payload << ']';
        trace.record( outing.activity_id + '#' + std::to_string( outing.generation ) + ":search",
                      "raid_site_search",
                      to_turns<int>( calendar::turn - calendar::turn_zero ), payload.str() );
    };
    if( party.empty() ) {
        const bool all_survivors_withdrawing = std::all_of(
                outing.member_ids.begin(), outing.member_ids.end(),
        [&outing, &operation]( const character_id id ) {
            return outing.member_is_resolved( id ) ||
                   std::find( operation.withdrawing_member_ids.begin(),
                              operation.withdrawing_member_ids.end(), id ) !=
                   operation.withdrawing_member_ids.end();
        } );
        std::optional<live_bandit_paid_return_plan> return_plan =
            all_survivors_withdrawing ? live_bandit_prepare_paid_return( site ) : std::nullopt;
        if( return_plan &&
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::transition_hostile_operation_phase( next, return_plan->cursor,
                bandit_live_world::hostile_operation_phase::committed_contact,
                bandit_live_world::hostile_operation_phase::returning_home,
                live_bandit_current_minutes(), "raid members physically withdrawing",
                true );
        } ) == bandit_live_world::hostile_operation_transition_result::applied ) {
            trace_search( "physical_withdrawal_returning_home", nullptr, nullptr, {} );
            live_bandit_apply_hostile_return_orders( site, *return_plan );
            return true;
        }
        trace_search( "no_active_member", nullptr, nullptr, {} );
        return withdrawal_changed;
    }
    if( operation.site_search_initial_goods_value < 0 ) {
        operation.site_search_initial_goods_value = 0;
        for( npc *member : party ) {
            member->goal = member->pos_abs_omt();
            member->omt_path.clear();
            member->set_mission( NPC_MISSION_NULL );
        }
    }

    map &here = get_map();
    std::vector<npc *> engaged;
    const Creature *first_combat_target = nullptr;
    for( npc *member : party ) {
        // Sight alone is not a combat choice.  The NPC may see a distant or
        // obstructed foe but select no target, and still needs its physical
        // search order.  A member that has selected a visible target fights
        // through npc::move while the rest of the party continues searching.
        const Creature *target = member->current_target();
        if( member->get_attitude() == NPCATT_KILL && target != nullptr &&
            !target->is_dead_state() && member->sees( here, *target ) ) {
            engaged.push_back( member );
            if( first_combat_target == nullptr ) {
                first_combat_target = target;
            }
        }
    }
    if( engaged.size() == party.size() ) {
        trace_search( "all_members_have_combat_target", engaged.front(), first_combat_target, {} );
        return false;
    }

    const tripoint_abs_omt approach = outing.shared_route.empty() ? outing.target_omt :
                                      outing.shared_route.back();
    const int floor_step = outing.target_omt.z() >= approach.z() ? 1 : -1;
    const int floor_count = std::abs( outing.target_omt.z() - approach.z() ) + 1;
    const int total_waypoints = floor_count * static_cast<int>( live_bandit_site_search_waypoints.size() );
    const int current_turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    raid_decision_trace::recorder &decision_trace = raid_decision_trace::native_recorder();
    const raid_decision_trace::selected_npc_scope selected_trace = decision_trace.enabled() ?
            raid_decision_trace::environment_selected_npcs() : raid_decision_trace::selected_npc_scope{};
    if( operation.site_search_retry_after_turn > current_turn ) {
        return false;
    }
    const auto waypoint_goal = [&]( const int waypoint_index, const std::size_t member_index ) {
        const int floor_z = approach.z() + floor_step *
                            ( waypoint_index / static_cast<int>( live_bandit_site_search_waypoints.size() ) );
        const tripoint_abs_omt floor_source( outing.target_omt.x(), outing.target_omt.y(), floor_z );
        const std::size_t waypoint = ( static_cast<std::size_t>( waypoint_index ) + member_index ) %
                                     live_bandit_site_search_waypoints.size();
        return project_to<coords::ms>( floor_source ) + live_bandit_site_search_waypoints[waypoint];
    };
    const auto path_to_goal = [&]( const npc &member, const tripoint_abs_ms &goal,
                                   const int candidate, const std::size_t member_index,
                                   const std::string_view purpose ) {
        const int actor_id = member.getID().get_value();
        const bool capture = selected_trace.includes( actor_id ) &&
                             selected_trace.includes_turn( current_turn );
        const bool engaged_member = std::find( engaged.begin(), engaged.end(), &member ) != engaged.end();
        const bool already_at_goal = member.pos_abs() == goal;
        const tripoint_bub_ms local_goal = here.get_bub( goal );
        const bool goal_inbounds = here.inbounds( local_goal );
        const bool goal_passable = goal_inbounds && here.passable( local_goal );
        std::vector<tripoint_bub_ms> route;
        struct avoid_sample {
            tripoint_bub_ms local;
            std::string reason;
            const Creature *occupant = nullptr;
        };
        std::vector<avoid_sample> avoid_samples;
        bool avoid_samples_truncated = false;
        int avoid_checks = 0;
        int occupied_checks = 0;
        int other_avoid_checks = 0;
        bool route_evaluated = false;
        bool relaxed_route_evaluated = false;
        std::vector<tripoint_bub_ms> relaxed_route;

        if( !already_at_goal && goal_passable ) {
            route_evaluated = true;
            const pathfinding_settings &settings = member.get_pathfinding_settings( false );
            if( capture ) {
                const npc::path_avoid_diagnostic observe_avoid = [&]( const tripoint_bub_ms &p,
                const std::string_view reason, const Creature *occupant ) {
                    ++avoid_checks;
                    if( reason == "occupied" ) {
                        ++occupied_checks;
                    } else {
                        ++other_avoid_checks;
                    }
                    if( std::none_of( avoid_samples.begin(), avoid_samples.end(),
                    [&p]( const avoid_sample &sample ) {
                        return sample.local == p;
                    } ) ) {
                        if( avoid_samples.size() < 12 ) {
                            avoid_samples.push_back( { p, std::string( reason ), occupant } );
                        } else {
                            avoid_samples_truncated = true;
                        }
                    }
                };
                route = here.route( member.pos_bub( here ), pathfinding_target::point( local_goal ),
                                    settings, member.get_path_avoid( observe_avoid ) );
                if( route.empty() ) {
                    // Diagnostic comparison only: retry with the member-specific avoid predicate
                    // disabled to distinguish its dynamic blockers from an otherwise absent route.
                    relaxed_route_evaluated = true;
                    relaxed_route = here.route( member.pos_bub( here ),
                                                pathfinding_target::point( local_goal ),
                                                settings,
                    []( const tripoint_bub_ms & ) {
                        return false;
                    } );
                }
            } else {
                route = here.route( member.pos_bub( here ), pathfinding_target::point( local_goal ),
                                    settings, member.get_path_avoid() );
            }
        }

        if( capture ) {
            const std::string_view diagnosis = already_at_goal ? "already_at_goal" :
                                               !goal_inbounds ? "goal_out_of_bounds" :
                                               !goal_passable ? "goal_not_passable" :
                                               !route.empty() ? "route_found" :
                                               !relaxed_route.empty() ? "member_avoid_blocks_route" :
                                               "no_route_without_member_avoid";
            std::ostringstream payload;
            const auto append_abs = [&payload]( const tripoint_abs_ms &p ) {
                payload << '[' << p.x() << ',' << p.y() << ',' << p.z() << ']';
            };
            const auto append_local = [&payload]( const tripoint_bub_ms &p ) {
                payload << '[' << p.x() << ',' << p.y() << ',' << p.z() << ']';
            };
            payload << "\"trace_group_id\":" << raid_decision_trace::quote( selected_trace.group_id )
                    << ",\"operation_id\":" << raid_decision_trace::quote( outing.activity_id )
                    << ",\"generation\":" << outing.generation
                    << ",\"site_id\":" << raid_decision_trace::quote( site.site_id )
                    << ",\"npc_id\":" << actor_id
                    << ",\"member_index\":" << member_index
                    << ",\"candidate\":" << candidate
                    << ",\"purpose\":" << raid_decision_trace::quote( purpose )
                    << ",\"engaged\":" << ( engaged_member ? "true" : "false" )
                    << ",\"position_abs\":";
            append_abs( member.pos_abs() );
            payload << ",\"goal_abs\":";
            append_abs( goal );
            payload << ",\"goal_local\":";
            append_local( local_goal );
            payload << ",\"already_at_goal\":" << ( already_at_goal ? "true" : "false" )
                    << ",\"goal_inbounds\":" << ( goal_inbounds ? "true" : "false" )
                    << ",\"goal_passable\":" << ( goal_passable ? "true" : "false" )
                    << ",\"route_evaluated\":" << ( route_evaluated ? "true" : "false" )
                    << ",\"route_found\":" << ( already_at_goal || !route.empty() ? "true" : "false" )
                    << ",\"route_length\":" << ( already_at_goal ? 0 :
                            route_evaluated ? static_cast<int>( route.size() ) : -1 )
                    << ",\"diagnosis\":" << raid_decision_trace::quote( diagnosis )
                    << ",\"avoid_rejected_checks\":" << avoid_checks
                    << ",\"occupied_rejected_checks\":" << occupied_checks
                    << ",\"other_avoid_rejected_checks\":" << other_avoid_checks
                    << ",\"avoid_samples_truncated\":" <<
                    ( avoid_samples_truncated ? "true" : "false" )
                    << ",\"relaxed_route_evaluated\":" << ( relaxed_route_evaluated ? "true" : "false" )
                    << ",\"relaxed_route_found\":";
            if( relaxed_route_evaluated ) {
                payload << ( relaxed_route.empty() ? "false" : "true" );
            } else {
                payload << "null";
            }
            payload << ",\"relaxed_route_basis\":\"same_settings_without_member_avoid\""
                    << ",\"relaxed_route_length\":";
            if( relaxed_route_evaluated ) {
                payload << relaxed_route.size();
            } else {
                payload << "null";
            }
            payload << ",\"avoid_samples\":[";
            for( std::size_t index = 0; index < avoid_samples.size(); ++index ) {
                if( index > 0 ) {
                    payload << ',';
                }
                const avoid_sample &sample = avoid_samples[index];
                const tripoint_abs_ms absolute = here.get_abs( sample.local );
                payload << "{\"local\":";
                append_local( sample.local );
                payload << ",\"absolute\":";
                append_abs( absolute );
                payload << ",\"reason\":" << raid_decision_trace::quote( sample.reason )
                        << ",\"occupant\":";
                if( sample.occupant == nullptr ) {
                    payload << "null";
                } else {
                    payload << "{\"type\":" << raid_decision_trace::quote(
                                   sample.occupant->is_avatar() ? "avatar" :
                                   sample.occupant->is_npc() ? "npc" :
                                   sample.occupant->is_monster() ? "monster" : "other" )
                            << ",\"id\":";
                    if( const Character *person = sample.occupant->as_character() ) {
                        payload << person->getID().get_value();
                    } else {
                        payload << "null";
                    }
                    payload << ",\"monster_type\":";
                    if( const monster *mon = sample.occupant->as_monster() ) {
                        payload << raid_decision_trace::quote( mon->type->id.str() );
                    } else {
                        payload << "null";
                    }
                    payload << '}';
                }
                payload << '}';
            }
            payload << ']';
            decision_trace.record( outing.activity_id + '#' + std::to_string( outing.generation ) +
                                   ':' + std::to_string( actor_id ), "raid_site_route_probe", current_turn,
                                   payload.str() );
        }
        return already_at_goal || !route.empty();
    };
    while( operation.site_search_waypoint < total_waypoints ) {
        const int floor_end = std::min( total_waypoints,
                                       ( operation.site_search_waypoint /
                                         static_cast<int>( live_bandit_site_search_waypoints.size() ) + 1 ) *
                                       static_cast<int>( live_bandit_site_search_waypoints.size() ) );
        bool floor_accessible = false;
        for( int candidate = operation.site_search_waypoint; candidate < floor_end && !floor_accessible;
             ++candidate ) {
            for( std::size_t index = 0; index < party.size(); ++index ) {
                if( std::find( engaged.begin(), engaged.end(), party[index] ) == engaged.end() &&
                    path_to_goal( *party[index], waypoint_goal( candidate, index ), candidate, index,
                                  "floor_access" ) ) {
                    floor_accessible = true;
                    break;
                }
            }
        }
        if( !floor_accessible ) {
            operation.site_search_retry_after_turn = current_turn + 60;
            trace_search( "floor_access_unavailable", nullptr, nullptr, {} );
            return false;
        }
        operation.site_search_retry_after_turn = 0;
        // The operation remembers the attempted waypoint, but an NPC's local
        // goto order may be lost across load.  Reissue only a physically
        // reachable goal the member has not reached.  A failed path does not
        // count an inaccessible upper floor as searched.
        bool missing_order = false;
        for( std::size_t index = 0; index < party.size(); ++index ) {
            const npc *member = party[index];
            const tripoint_abs_ms goal = waypoint_goal( operation.site_search_waypoint, index );
            if( std::find( engaged.begin(), engaged.end(), member ) == engaged.end() &&
                !member->goto_to_this_pos && member->pos_abs() != goal &&
                path_to_goal( *member, goal, operation.site_search_waypoint, index,
                              "order_recovery" ) ) {
                missing_order = true;
                break;
            }
        }
        if( !operation.site_search_waypoint_attempted || missing_order ) {
            bool assigned = false;
            std::vector<npc *> recipients;
            for( std::size_t index = 0; index < party.size(); ++index ) {
                if( std::find( engaged.begin(), engaged.end(), party[index] ) != engaged.end() ||
                    party[index]->goto_to_this_pos ) {
                    continue;
                }
                const tripoint_abs_ms goal = waypoint_goal( operation.site_search_waypoint, index );
                if( party[index]->pos_abs() != goal &&
                    path_to_goal( *party[index], goal, operation.site_search_waypoint, index,
                                  "waypoint_assignment" ) ) {
                    party[index]->goto_to_this_pos = goal;
                    assigned = true;
                    recipients.push_back( party[index] );
                }
            }
            operation.site_search_waypoint_attempted = true;
            if( assigned ) {
                trace_search( "waypoint_assigned", nullptr, nullptr, recipients );
                return true;
            }
            trace_search( "waypoint_unreachable", nullptr, nullptr, {} );
        }
        bool travelling = false;
        std::vector<npc *> travelling_members;
        for( std::size_t index = 0; index < party.size(); ++index ) {
            npc *member = party[index];
            if( std::find( engaged.begin(), engaged.end(), member ) == engaged.end() &&
                member->goto_to_this_pos && member->pos_abs() != *member->goto_to_this_pos ) {
                if( !member->path.empty() ||
                    path_to_goal( *member, *member->goto_to_this_pos,
                                  operation.site_search_waypoint, index, "existing_order_validation" ) ) {
                    travelling = true;
                    travelling_members.push_back( member );
                } else {
                    member->goto_to_this_pos.reset();
                }
            }
        }
        if( travelling ) {
            trace_search( "waiting_for_movement", nullptr, nullptr, travelling_members );
            return false;
        }
        if( !engaged.empty() && std::any_of( party.begin(), party.end(), [&]( const npc * member ) {
            return std::find( engaged.begin(), engaged.end(), member ) == engaged.end() &&
                   member->goto_to_this_pos.has_value();
        } ) ) {
            trace_search( "combat_target_search_hold", engaged.front(), first_combat_target, {} );
            return false;
        }
        for( npc *member : party ) {
            member->goto_to_this_pos.reset();
        }
        operation.site_search_waypoint++;
        operation.site_search_waypoint_attempted = false;
    }

    if( !engaged.empty() ) {
        trace_search( "combat_target_search_hold", engaged.front(), first_combat_target, {} );
        return false;
    }
    if( fight_shakedown ) {
        trace_search( "fight_site_search_exhausted", nullptr, nullptr, {} );
        return false;
    }
    for( npc *member : party ) {
        if( std::find( operation.withdrawing_member_ids.begin(),
                       operation.withdrawing_member_ids.end(), member->getID() ) ==
            operation.withdrawing_member_ids.end() ) {
            operation.withdrawing_member_ids.push_back( member->getID() );
        }
        member->goto_to_this_pos.reset();
        member->path.clear();
    }
    live_cannibal_raid_advance_withdrawals( site );
    std::optional<live_bandit_paid_return_plan> return_plan = live_bandit_prepare_paid_return( site );
    if( !return_plan ) {
        trace_search( "exhausted_home_route_unavailable", nullptr, nullptr, {} );
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world raid site search retained: physical home route unavailable"
                                   << " site=" << site.site_id;
        return false;
    }
    if( commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
    return bandit_live_world::transition_hostile_operation_phase( next, return_plan->cursor,
            bandit_live_world::hostile_operation_phase::committed_contact,
            bandit_live_world::hostile_operation_phase::returning_home,
            live_bandit_current_minutes(), "cannibal raid completed physical site search", true );
    } ) !=
    bandit_live_world::hostile_operation_transition_result::applied ) {
        trace_search( "exhausted_transition_rejected", nullptr, nullptr, {} );
        return false;
    }
    trace_search( "exhausted_returning_home", nullptr, nullptr, {} );
    live_bandit_apply_hostile_return_orders( site, *return_plan );
    return true;
}

int live_bandit_shakedown_party_goods_value( const bandit_live_world::site_record &site )
{
    int value = 0;
    for( const character_id member_id : site.active_hostile_operation.reservation.member_ids ) {
        const npc *member_npc = g->find_npc( member_id );
        if( member_npc != nullptr && !member_npc->is_dead() ) {
            value += live_bandit_character_goods_value( *member_npc );
        }
    }
    return value;
}

bool live_bandit_advance_hidden_shakedown_search( bandit_live_world::site_record &site )
{
    bandit_live_world::hostile_operation_state &operation = site.active_hostile_operation;
    const bandit_live_world::active_outing_state &outing = operation.reservation;
    npc *searcher = nullptr;
    for( const character_id member_id : outing.member_ids ) {
        const shared_ptr_fast<npc> member_npc = overmap_buffer.find_npc( member_id );
        if( active_committed_bandit_shakedown_member( site, member_id, member_npc ) ) {
            searcher = member_npc.get();
            break;
        }
    }
    if( searcher == nullptr ) {
        return false;
    }
    if( operation.site_search_initial_goods_value < 0 ) {
        operation.site_search_initial_goods_value = live_bandit_shakedown_party_goods_value( site );
        for( const character_id member_id : outing.member_ids ) {
            npc *member_npc = g->find_npc( member_id );
            if( member_npc != nullptr && !member_npc->is_dead() ) {
                member_npc->goal = member_npc->pos_abs_omt();
                member_npc->omt_path.clear();
                member_npc->set_mission( NPC_MISSION_NULL );
            }
        }
    }

    // The native NPC action loop searches visible items while walking.  Visit
    // the center and four quarters of the actual target OMT; no remote stash
    // or unseen inventory is queried or transferred here.
    map &here = get_map();
    while( operation.site_search_waypoint < static_cast<int>( live_bandit_site_search_waypoints.size() ) ) {
        const tripoint_abs_ms goal = project_to<coords::ms>( outing.target_omt ) +
                                     live_bandit_site_search_waypoints[static_cast<std::size_t>(
                                             operation.site_search_waypoint )];
        const tripoint_bub_ms local_goal = here.get_bub( goal );
        if( !here.inbounds( local_goal ) || !here.passable( local_goal ) ) {
            operation.site_search_waypoint++;
            operation.site_search_waypoint_attempted = false;
            continue;
        }
        if( searcher->fetching_item ) {
            return false;
        }
        if( operation.site_search_waypoint_attempted ) {
            if( searcher->pos_abs() == goal || searcher->goto_to_this_pos != goal ||
                searcher->path.empty() ) {
                searcher->goto_to_this_pos.reset();
                // Ordered movement takes priority over the normal idle-item scan.
                // Probe from the reached waypoint before moving on; the NPC's
                // ordinary pickup action handles the selected physical item.
                searcher->find_item();
                if( searcher->fetching_item ) {
                    return false;
                }
                operation.site_search_waypoint++;
                operation.site_search_waypoint_attempted = false;
                continue;
            }
            return false;
        }
        searcher->goto_to_this_pos = goal;
        operation.site_search_waypoint_attempted = true;
        return true;
    }

    std::optional<live_bandit_paid_return_plan> return_plan = live_bandit_prepare_paid_return( site );
    if( !return_plan ) {
        return false;
    }
    const int taken_value = std::max( 0, live_bandit_shakedown_party_goods_value( site ) -
                                     operation.site_search_initial_goods_value );
    const std::string summary = string_format( "bandit site search completed physical loot=%d",
                                taken_value );
    if( commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
    return bandit_live_world::transition_hostile_operation_phase( next, return_plan->cursor,
            bandit_live_world::hostile_operation_phase::committed_contact,
            bandit_live_world::hostile_operation_phase::returning_home,
            live_bandit_current_minutes(), summary, true );
    } ) !=
    bandit_live_world::hostile_operation_transition_result::applied ) {
        return false;
    }
    bandit_live_world::hostile_operation_state &returned_operation = site.active_hostile_operation;
    returned_operation.shakedown_pending_branch = taken_value > 0 ? "robbed" : "searched_empty";
    returned_operation.shakedown_pending_demanded_value = 0;
    returned_operation.shakedown_pending_surrendered_value = taken_value;
    returned_operation.shakedown_pending_reachable_value = taken_value;
    returned_operation.shakedown_pending_basecamp_scene = true;
    live_bandit_apply_hostile_return_orders( site, *return_plan );
    DebugLog( D_INFO, DC_ALL ) << summary << " return=physical\n";
    return true;
}

struct shakedown_speaker_readiness {
    int shout_volume;
    bool sleeping;
    bool narcosis;
    bool suspended;
    bool fleeing;
    bool fleeing_temporary;
    bool runaway;
    bool dangerous_field;
    const Creature *target;
    bool hostile_target;

    bool ready() const {
        return shout_volume > 2 && !sleeping && !narcosis && !suspended && !fleeing &&
               !fleeing_temporary && !runaway && !dangerous_field && !hostile_target;
    }
};

shakedown_speaker_readiness live_bandit_shakedown_speaker_readiness( const npc &actor )
{
    // Character::shout emits no words for a faceless speaker and only an
    // indistinct voice at its minimum volume of two. Neither delivers a demand.
    const Creature *const target = actor.current_target();
    return { actor.get_shout_volume(), actor.in_sleep_state(), actor.has_effect( effect_narcosis ),
             actor.has_effect( effect_npc_suspend ), actor.get_attitude() == NPCATT_FLEE,
             actor.get_attitude() == NPCATT_FLEE_TEMP,
             actor.has_effect( effect_npc_run_away ), get_map().dangerous_field_at( actor.pos_bub() ),
             target, target != nullptr && actor.attitude_to( *target ) == Creature::Attitude::HOSTILE };
}

bool live_bandit_shakedown_speaker_ready( const npc &actor )
{
    return live_bandit_shakedown_speaker_readiness( actor ).ready();
}

Character *live_bandit_shakedown_receiver( const bandit_live_world::site_record &site )
{
    const auto &operation = site.active_hostile_operation;
    const auto &outing = operation.reservation;
    if( !operation.is_active() || !operation.shakedown_contact_established ||
        operation.operation_kind != bandit_live_world::hostile_operation_kind::shakedown ||
        operation.phase != bandit_live_world::hostile_operation_phase::committed_contact ||
        outing.owner != bandit_live_world::simulation_owner::local ) {
        return nullptr;
    }
    Character *receiver = operation.shakedown_receiver_is_avatar ?
                          static_cast<Character *>( &get_avatar() ) :
                          static_cast<Character *>( g->find_npc( operation.shakedown_receiver_id ) );
    if( receiver == nullptr || receiver->getID() != operation.shakedown_receiver_id ||
        !live_bandit_shakedown_receiver_eligible( site, *receiver, true ) ) {
        return nullptr;
    }
    return receiver;
}

bool live_bandit_establish_shakedown_communication( bandit_live_world::site_record &site )
{
    using namespace bandit_live_world;
    auto &operation = site.active_hostile_operation;
    auto &outing = operation.reservation;
    if( site.retired_empty_site || !operation.is_active() ||
        ( site.profile == hostile_site_profile::none ? profile_for_site_kind( site.site_kind ) :
          site.profile ) == hostile_site_profile::cannibal_camp ||
        ( !outing.camp_id.empty() && outing.camp_id != site.site_id ) ||
        operation.operation_kind != hostile_operation_kind::shakedown ||
        ( operation.phase != hostile_operation_phase::approaching &&
          operation.phase != hostile_operation_phase::committed_contact ) ||
        operation.shakedown_contact_established || !operation.shakedown_pending_branch.empty() ||
        outing.shared_route.empty() || !site.last_shakedown_outcome.empty() ) {
        return false;
    }
    map &here = get_map();
    const auto destination = here.get_bub( project_to<coords::ms>( outing.shared_route.back() ) +
                                         point( SEEX, SEEY ) );
    if( !here.inbounds( destination ) ) {
        return false;
    }
    // The entire surviving reservation must really be present before a local
    // owner handover. No menu, placement or exact doorstep substitutes for it.
    std::vector<npc *> party;
    for( const character_id id : outing.member_ids ) {
        if( outing.member_is_resolved( id ) ) {
            continue;
        }
        const auto *member = site.find_member( id );
        npc *actor = g->find_npc( id );
        if( member == nullptr || actor == nullptr || actor->is_dead() || !actor->is_active() ||
            !here.inbounds( actor->pos_bub() ) ||
            ( member->state != member_state::outbound && member->state != member_state::local_contact ) ) {
            return false;
        }
        party.push_back( actor );
    }
    npc *speaker = nullptr;
    for( npc *actor : party ) {
        if( live_bandit_shakedown_speaker_ready( *actor ) &&
            actor->getID() == operation.shakedown_demand_speaker_id ) {
            speaker = actor;
            break;
        }
    }
    if( speaker == nullptr ) {
        for( npc *actor : party ) {
            if( live_bandit_shakedown_speaker_ready( *actor ) ) {
                speaker = actor;
                break;
            }
        }
    }
    if( speaker == nullptr ) {
        return false;
    }
    Character *receiver = operation.shakedown_receiver_is_avatar ?
                          static_cast<Character *>( &get_avatar() ) :
                          static_cast<Character *>( g->find_npc( operation.shakedown_receiver_id ) );
    const bool heard_current_speaker = operation.shakedown_demand_emitted_turn >= 0 &&
                                       speaker->getID() == operation.shakedown_demand_speaker_id;
    const bool current_receipt = heard_current_speaker && receiver != nullptr &&
                                 receiver->getID() == operation.shakedown_receiver_id &&
                                 live_bandit_shakedown_receiver_eligible( site, *receiver, false ) &&
                                 sounds::can_hear_local_sound( *receiver, speaker->pos_bub(),
                                         operation.shakedown_demand_volume );
    if( current_receipt && receiver->has_effect( effect_sleep ) &&
        receiver->get_effect( effect_sleep ).get_duration() <= 0_turns ) {
        // Actual hearing already requested wake. Wait for normal effect expiry,
        // rather than repeating the voice or bypassing scheduler exclusion.
        return false;
    }
    if( !current_receipt || !live_bandit_shakedown_receiver_eligible( site, *receiver, true ) ) {
        const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
        const bool emitted = operation.shakedown_demand_emitted_turn >= 0;
        const sounds::robbery_demand previous{ site.site_id, outing.activity_id, outing.generation,
                  operation.shakedown_demand_speaker_id, operation.shakedown_demand_emitted_turn };
        if( emitted && ( turn <= operation.shakedown_demand_emitted_turn ||
                         sounds::robbery_demand_pending( previous ) ) ) {
            return false;
        }
        // An undelivered/unheard/invalidated receipt is not a permanent lock.
        // Retry only at an actual new audible opportunity, not each callback
        // while nobody can hear. This predicate grants no hearing or contact.
        const auto can_receive = [&]( Character &recipient ) {
            return live_bandit_shakedown_receiver_eligible( site, recipient, false ) &&
                   sounds::can_hear_local_sound( recipient, speaker->pos_bub(),
                                                speaker->get_shout_volume() );
        };
        if( emitted && !can_receive( get_avatar() ) ) {
            bool audible_recipient = false;
            for( npc &recipient : g->all_npcs() ) {
                if( can_receive( recipient ) ) {
                    audible_recipient = true;
                    break;
                }
            }
            if( !audible_recipient ) {
                return false;
            }
        }
        const sounds::robbery_demand demand{ site.site_id, outing.activity_id,
                  outing.generation, speaker->getID(), turn };
        const int volume = speaker->shout( _( "We want payment. Pay us, or fight!" ), false, &demand );
        if( volume <= 2 ) {
            return false;
        }
        // Persist the last actual emission on the same reservation. It cannot
        // be rolled back by a later contact refusal. A new event must earn its
        // own actual hearing; an old candidate never authenticates this voice.
        operation.shakedown_demand_emitted_turn = turn;
        operation.shakedown_demand_volume = volume;
        operation.shakedown_demand_speaker_id = speaker->getID();
        operation.shakedown_receiver_id = character_id();
        operation.shakedown_receiver_is_avatar = false;
        return false;
    }
    const bool by_shout = !speaker->sees( here, *receiver );
    site_record candidate = site;
    if( operation.phase == hostile_operation_phase::approaching ) {
        const auto cursor = current_external_simulation_cursor( site );
        if( !cursor || transition_hostile_operation_phase( candidate, *cursor,
                hostile_operation_phase::approaching, hostile_operation_phase::committed_contact,
                live_bandit_current_minutes(), "shakedown reached real communication" ) !=
            hostile_operation_transition_result::applied ) {
            return false;
        }
    }
    auto &contact = candidate.active_hostile_operation;
    contact.shakedown_contact_established = true;
    contact.shakedown_receiver_id = receiver->getID();
    contact.shakedown_receiver_is_avatar = receiver->is_avatar();
    contact.shakedown_contact_by_shout = by_shout;
    contact.shakedown_waiting_local = false;
    if( !persist_live_bandit_local_projection_leases( candidate, &site ) ) {
        return false;
    }
    site = std::move( candidate );
    const auto &committed_contact = site.active_hostile_operation;
    const auto scope = raid_decision_trace::environment_selected_npcs();
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    if( scope.includes_turn( turn ) &&
        ( scope.includes( speaker->getID().get_value() ) || scope.includes( receiver->getID().get_value() ) ) ) {
        raid_decision_trace::native_recorder().record_edge( "bandit_shakedown_contact", turn,
                "\"trace_group_id\":" + raid_decision_trace::quote( scope.group_id ) +
                ",\"operation_id\":" + raid_decision_trace::quote( committed_contact.reservation.activity_id ) +
                ",\"generation\":" + std::to_string( committed_contact.reservation.generation ) +
                ",\"site_id\":" + raid_decision_trace::quote( site.site_id ) +
                ",\"speaker_id\":" + std::to_string( speaker->getID().get_value() ) +
                ",\"receiver_id\":" + std::to_string( receiver->getID().get_value() ) +
                ",\"receiver_is_avatar\":" + ( receiver->is_avatar() ? "true" : "false" ) +
                ",\"audible_shout\":true,\"unseen_receiver\":" + ( by_shout ? "true" : "false" ) );
    }
    return true;
}

bool live_bandit_handle_hostile_shakedown_contact( bandit_live_world::site_record &site,
        const avatar &u )
{
    bandit_live_world::active_outing_state *outing = site.active_external_outing();
    if( site.retired_empty_site || outing == nullptr || !outing->is_active() ||
        outing != &site.active_hostile_operation.reservation ||
        site.active_hostile_operation.operation_kind !=
        bandit_live_world::hostile_operation_kind::shakedown ||
        site.active_hostile_operation.phase !=
        bandit_live_world::hostile_operation_phase::committed_contact ||
        outing->owner != bandit_live_world::simulation_owner::local ) {
        return false;
    }

    // Once a parley has been released into combat, its already-admitted
    // members must be reconciled before admission validation.  A casualty is
    // expected in this state and must not turn a recoverable aftermath into a
    // placement failure.
    if( !site.last_shakedown_outcome.empty() ) {
        bool changed = live_bandit_reconcile_hostile_shakedown_combat( site );
        outing = site.active_external_outing();
        if( site.last_shakedown_outcome == "fight_unresolved" &&
            site.active_hostile_operation.phase ==
            bandit_live_world::hostile_operation_phase::committed_contact && outing != nullptr ) {
            for( const character_id member_id : outing->member_ids ) {
                const shared_ptr_fast<npc> member_npc = overmap_buffer.find_npc( member_id );
                if( active_committed_bandit_shakedown_member( site, member_id, member_npc ) &&
                    member_npc->get_attitude() != NPCATT_KILL &&
                    member_npc->get_attitude() != NPCATT_FLEE &&
                    member_npc->get_attitude() != NPCATT_FLEE_TEMP ) {
                    member_npc->set_attitude( NPCATT_KILL );
                    changed = true;
                }
            }
        }
        if( site.last_shakedown_outcome == "fight_unresolved" ) {
            changed |= live_cannibal_raid_advance_site_search( site );
        }
        return changed;
    }
    // Invalidate the deferred response before a stale reservation can be
    // rejected by admission. It can never revive as trade after later loading.
    if( live_bandit_cancel_stale_pending_pay( site ) ) {
        return true;
    }
    if( !materialize_committed_bandit_shakedown( site ) ) {
        return false;
    }
    live_bandit_establish_shakedown_communication( site );

    if( site.last_shakedown_outcome.empty() ) {
        const bool player_attack_escalated =
            site.active_hostile_operation.shakedown_pending_branch == "fight";
        if( player_attack_escalated ) {
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( cursor ) {
                bandit_live_world::begin_matching_hostile_shakedown_combat( site, *cursor,
                        live_bandit_current_minutes(), "player attacked shakedown participant" );
            }
        }
    }
    outing = site.active_external_outing();
    if( outing == nullptr || outing != &site.active_hostile_operation.reservation ) {
        return false;
    }
    if( live_bandit_reconcile_hostile_shakedown_combat( site ) ) {
        return true;
    }

    const bool has_loaded_local_contact = std::any_of(
            outing->member_ids.begin(), outing->member_ids.end(), [&site]( const character_id member_id ) {
        return active_committed_bandit_shakedown_member( site, member_id );
    } );
    if( !has_loaded_local_contact ) {
        return false;
    }

    bandit_live_world::local_gate_input gate_input = live_bandit_make_gate_input( site, u );
    gate_input.local_contact_established = true;
    // This reserved visit remains a shakedown even if the unrelated avatar
    // happens to be travelling on a road. No unheard ambush releases its intent.
    gate_input.rolling_travel_scene = false;
    if( site.active_hostile_operation.shakedown_contact_established ) {
        gate_input.player_contact = site.active_hostile_operation.shakedown_receiver_is_avatar;
        gate_input.follower_sight = !gate_input.player_contact;
        gate_input.basecamp_or_camp_scene = !gate_input.player_contact || gate_input.basecamp_or_camp_scene;
    }
    const bandit_live_world::local_gate_decision gate_decision =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    bandit_live_world::record_local_gate_semantic_event( site, gate_input, gate_decision );
    if( gate_decision.combat_forward ) {
        bool changed = false;
        for( const character_id member_id : outing->member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                continue;
            }
            const shared_ptr_fast<npc> member_npc = overmap_buffer.find_npc( member_id );
            if( active_committed_bandit_shakedown_member( site, member_id, member_npc ) ) {
                changed |= live_bandit_note_combat_intent( *member_npc, site, gate_input,
                           gate_decision );
            }
        }
        return changed;
    }
    if( site.active_hostile_operation.shakedown_contact_established &&
        gate_decision.opens_shakedown_surface ) {
        return open_live_bandit_shakedown_surface( site, gate_input, gate_decision );
    }
    if( !site.active_hostile_operation.shakedown_contact_established ) {
        return live_bandit_advance_hidden_shakedown_search( site );
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
            outing->kind == bandit_live_world::outing_kind::structural_sortie ||
            bandit_live_world::active_outing_requires_homeward_routing( *outing ) ) {
            continue;
        }
        if( outing == &site.active_hostile_operation.reservation &&
            ( site.active_hostile_operation.operation_kind ==
              bandit_live_world::hostile_operation_kind::shakedown ||
              site.active_hostile_operation.phase !=
              bandit_live_world::hostile_operation_phase::committed_contact ) ) {
            continue;
        }

        bool has_loaded_local_contact_member = false;
        for( const character_id &member_id : outing->member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                continue;
            }
            if( active_local_contact_member( site, member_id,
                                             overmap_buffer.find_npc( member_id ) ) ) {
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
        bandit_live_world::record_local_gate_semantic_event( site, gate_input, gate_decision );
        DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_local_gate_report( site, gate_input,
                                   gate_decision )
                                   << "- live_existing_active_group=yes\n";
        if( gate_decision.combat_forward ) {
            for( const character_id &member_id : outing->member_ids ) {
                const bandit_live_world::member_record *member = site.find_member( member_id );
                if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ) {
                    continue;
                }
                const shared_ptr_fast<npc> member_npc = overmap_buffer.find_npc( member_id );
                if( active_local_contact_member( site, member_id, member_npc ) ) {
                    changed |= live_bandit_note_combat_intent( *member_npc, site, gate_input,
                               gate_decision );
                }
            }
            continue;
        }
        if( gate_decision.opens_shakedown_surface && gate_input.player_contact ) {
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
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
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
                commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                    return bandit_live_world::commit_local_structural_watch_exit( next, exit_plan, prepare_member,
                            restore_member );
                } );
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
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::apply_covert_scout_burn( next, *cursor, reads, egress_candidates,
                    current_minutes, danger_read );
        } );
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
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
        if( !bandit_live_world::claim_local_pair_site_ownership( site, claimed_members ) ) {
            return 0;
        }
    }

    avatar &u = get_avatar();
    map &here = get_map();
    std::vector<target_character> target_characters = { { &u, "avatar" } };
    for( const npc &defender : g->all_npcs() ) {
        if( !defender.is_dead() && defender.is_active() &&
            here.inbounds( defender.pos_bub( here ) ) ) {
            target_characters.push_back( { &defender, "npc:" +
                                           std::to_string( defender.getID().get_value() ) } );
        }
    }

    const int current_minutes = live_bandit_current_minutes();
    int recorded = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || outing.schema_version < 8 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            ( outing.owner == bandit_live_world::simulation_owner::local &&
              ( !outing.local_handoff.is_active() || !outing.local_handoff.cohesion_assembled ||
                outing.local_handoff.cohesion_abort_return ) ) || outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<target_character> bounded_targets = target_characters;
        // Existing resident NPCs remain physical targets outside the player's
        // active list. Query only already loaded overmaps near the watched site;
        // never load terrain, materialize a roster, or use player reveal flags.
        std::set<const overmap *> resident_maps;
        std::set<std::string> included_ids;
        for( const auto &target : bounded_targets ) {
            included_ids.insert( target.stable_id );
        }
        for( const auto &tile : outing.target_footprint ) {
            for( const overmap *om : overmap_buffer.get_loaded_overmaps_near(
                     project_to<coords::sm>( tile ).xy(), 4 ) ) {
                if( !resident_maps.insert( om ).second ) {
                    continue;
                }
                for( const auto &actor : om->get_npcs() ) {
                    const std::string id = "npc:" + std::to_string( actor->getID().get_value() );
                    if( !actor->is_dead_state() && !actor->is_fake() && included_ids.insert( id ).second ) {
                        bounded_targets.push_back( { actor.get(), id } );
                    }
                }
            }
        }
        bounded_targets.erase( std::remove_if( bounded_targets.begin(), bounded_targets.end(),
        [&]( const target_character & target ) {
            std::vector<tripoint_abs_omt> previous;
            for( const auto &observation : outing.observations ) {
                if( observation.observed_site_id == outing.target_id &&
                    observation.target_revision == outing.target_lead_revision &&
                    observation.observed_minutes <= current_minutes &&
                    observation.expiry_minutes >= current_minutes &&
                    std::find( observation.defender_ids.begin(), observation.defender_ids.end(),
                               target.stable_id ) != observation.defender_ids.end() ) {
                    previous.push_back( observation.source_omt );
                }
            }
            return !scout_observation::associated_with_site( target.actor->pos_abs_omt(),
                    outing.target_footprint, previous );
        } ), bounded_targets.end() );
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

        scout_observation::site_reader optical_geometry;
        std::vector<shared_ptr_fast<npc>> observers;
        for( const character_id member_id : outing.member_ids ) {
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( member != nullptr && optical_geometry.watching_member( site, *member ) &&
                member->pos_abs_omt() == outing.selected_watch_omt &&
                ( outing.owner == bandit_live_world::simulation_owner::abstract ||
                  ( member->is_active() && here.inbounds( member->pos_bub( here ) ) ) ) &&
                live_bandit_can_make_ordinary_visual_observation( *member ) ) {
                observers.push_back( member );
            }
        }
        std::sort( observers.begin(), observers.end(), [&outing]( const shared_ptr_fast<npc> &lhs,
        const shared_ptr_fast<npc> &rhs ) {
            return std::make_tuple( lhs->getID() == outing.leader_id ? 0 : 1,
                                    lhs->getID().get_value() ) <
                   std::make_tuple( rhs->getID() == outing.leader_id ? 0 : 1,
                                    rhs->getID().get_value() );
        } );

        // Reuse only physical geometry and native illumination within this
        // site's one observation decision; actor eligibility/identity/visibility
        // are read again for every target, and nothing survives another turn.
        bool site_changed = false;
        bool site_observed = false;
        for( const shared_ptr_fast<npc> &observer : observers ) {
            const bool scout_has_gun = observer->get_wielded_item() &&
                                       observer->get_wielded_item()->is_gun();
            std::vector<bandit_live_world::covert_scout_burn_read::visible_defender_read>
            visible_defenders;
            bool physical_pair_can_share = observers.size() == 2 &&
                                           rl_dist( observers[0]->pos_abs(), observers[1]->pos_abs() ) <= 6;
            for( const target_character &defender : bounded_targets ) {
                if( defender.actor == observer.get() || defender.actor->is_dead_state() ) {
                    continue;
                }
                const auto visibility = optical_geometry.read_character( *observer, *defender.actor,
                                        outing.target_footprint );
                if( bandit_live_world_probe::transition_events_enabled() ) {
                    bandit_live_world_probe::transition_event event;
                    event.game_minutes = current_minutes;
                    event.domain = "scout_observation";
                    event.transition = "binocular_site_actor";
                    event.observer_id = std::to_string( observer->getID().get_value() );
                    event.site_id = site.site_id;
                    event.operation_id = outing.activity_id;
                    event.reason = defender.stable_id + " observer=" + std::to_string( observer->getID().get_value() ) +
                                   " observer_abs_ms=" + observer->pos_abs().to_string() +
                                   " target_abs_ms=" + defender.actor->pos_abs().to_string() +
                                   " bucket=" + std::to_string( current_minutes - current_minutes % 30 ) +
                                   " result=" + ( visibility.visible ? "observed" : visibility.known ? "rejected" : "unknown" ) +
                                   " reason=" + visibility.reason;
                    bandit_live_world_probe::record_live_transition_event( std::move( event ) );
                }
                if( !visibility.visible ) {
                    continue;
                }
                // A nearby capable partner may still have different sight
                // limits or target-local cover. Share this actor group only
                // when both actual watchers see every person it contains.
                if( physical_pair_can_share ) {
                    const auto &partner = observers[0] == observer ? observers[1] : observers[0];
                    physical_pair_can_share = optical_geometry.read_character(
                                                  *partner, *defender.actor, outing.target_footprint ).visible;
                }
                bandit_live_world::covert_scout_burn_read::visible_defender_read read;
                read.stable_id = defender.stable_id;
                read.position = defender.actor->pos_abs_omt();
                read.actual_position = defender.actor->pos_abs();
                const bool detailed = here.inbounds( observer->pos_abs() ) &&
                                      observer->sees_without_clairvoyance( here, *defender.actor );
                read.normalized_power = detailed ?
                                        bandit_live_world::normalize_hostile_camp_character_power(
                                            observer->evaluate_character_threat_without_perception_fuzz(
                                                *defender.actor, scout_has_gun, true ) ) : 3;
                const item_location defender_weapon = detailed ? defender.actor->get_wielded_item() :
                                                      item_location();
                read.equipment_detail = !defender_weapon ? 0 : defender_weapon->is_gun() ? 2 : 1;
                visible_defenders.push_back( std::move( read ) );
            }
            std::sort( visible_defenders.begin(), visible_defenders.end(),
            []( const auto & lhs, const auto & rhs ) {
                return lhs.stable_id < rhs.stable_id;
            } );
            if( visible_defenders.empty() ) {
                continue;
            }
            const auto observer_cursor = bandit_live_world::current_external_simulation_cursor( site );
            if( !observer_cursor ) {
                break;
            }

            const auto record = [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::record_covert_visible_defender_observations( next, *observer_cursor,
                        observer->getID(), observer->pos_abs_omt(), visible_defenders, current_minutes,
                        physical_pair_can_share );
            };
            const bandit_live_world::sortie_observation_effect effect =
                outing.owner == bandit_live_world::simulation_owner::local ?
                commit_live_bandit_local_progress( site, record ) : record( site );
            if( effect.valid ) {
                site_observed = true;
                site_changed = site_changed || effect.changed;
            }
        }
        if( site_changed ) {
            recorded++;
        }
        if( site_observed ) {
            // Assess after both actual watchers have contributed. The finite
            // watch may finish now, without dropping the second member's sight.
            if( site.active_outing.owner == bandit_live_world::simulation_owner::abstract &&
                site.active_outing.schema_version >= 10 ) {
                const std::string activity_id = site.active_outing.activity_id;
                const int generation = site.active_outing.generation;
                const int revision = site.active_outing.target_lead_revision;
                bandit_live_world::advance_structural_scout_assessment(
                    site, activity_id, generation, revision, current_minutes );
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
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
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
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::record_covert_vehicle_wealth_observations( next, *cursor,
                        observer->getID(), observer->pos_abs_omt(), reads,
                        current_minutes );
            } );
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
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
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
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::record_covert_generation_infrastructure_observations( next, *cursor,
                        observer->getID(), observer->pos_abs_omt(), reads,
                        current_minutes );
            } );
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
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
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
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::record_covert_cargo_handling_observations( next, *cursor,
                        observer->getID(), observer->pos_abs_omt(), reads,
                        current_minutes );
            } );
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

    // A terminal shakedown receipt deliberately outlives its released operation.  Keep this
    // observer outside the aftermath and reservation writers: it only classifies the persisted
    // receipt after release, including the stale identity control, and records whether those
    // reads left the serialized world unchanged.
    static std::set<std::string> observed_terminal_replay_keys;
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( site.active_hostile_operation.is_active() ||
            site.last_hostile_shakedown_operation_id.empty() ||
            site.last_hostile_shakedown_report_key.empty() ||
            site.last_hostile_shakedown_generation <= 0 ) {
            continue;
        }
        const bandit_live_world::hostile_target_opportunity_record *receipt = nullptr;
        for( const bandit_live_world::hostile_target_opportunity_record &candidate :
             state.hostile_target_opportunities ) {
            if( candidate.consumed_operation_id == site.last_hostile_shakedown_operation_id &&
                candidate.consumed_report_key == site.last_hostile_shakedown_report_key &&
                candidate.consumed_generation == site.last_hostile_shakedown_generation ) {
                receipt = &candidate;
                break;
            }
        }
        if( receipt == nullptr || receipt->revision <= 0 ) {
            continue;
        }
        const std::string replay_key = site.site_id + "|" + receipt->target_id + "|" +
                                       receipt->consumed_operation_id + "|" +
                                       receipt->consumed_report_key + "|" +
                                       std::to_string( receipt->consumed_generation );
        if( !observed_terminal_replay_keys.insert( replay_key ).second ) {
            continue;
        }
        bandit_live_world::terminal_hostile_shakedown_replay_identity exact = {
            receipt->target_id, receipt->target_omt, receipt->revision,
            receipt->consumed_operation_id, receipt->consumed_report_key,
            receipt->consumed_generation
        };
        bandit_live_world::terminal_hostile_shakedown_replay_identity stale = exact;
        stale.generation = stale.generation == std::numeric_limits<int>::max() ?
                           stale.generation - 1 : stale.generation + 1;
        const auto serialized_state = []() {
            std::ostringstream serialized;
            JsonOut json( serialized );
            state.serialize( json );
            return serialized.str();
        };
        const std::string before = serialized_state();
        const bandit_live_world::terminal_hostile_shakedown_replay_disposition exact_disposition =
            bandit_live_world::observe_terminal_hostile_shakedown_replay( state, site, exact );
        const bandit_live_world::terminal_hostile_shakedown_replay_disposition stale_disposition =
            bandit_live_world::observe_terminal_hostile_shakedown_replay( state, site, stale );
        const bool byte_stable = serialized_state() == before;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world terminal_shakedown_replay_observer"
                                   << " site=" << site.site_id
                                   << " target=" << exact.target_id
                                   << " operation=" << exact.operation_id
                                   << " report_key=" << exact.report_key
                                   << " generation=" << exact.generation
                                   << " active_operation=absent"
                                   << " exact=" << ( exact_disposition ==
                                           bandit_live_world::terminal_hostile_shakedown_replay_disposition::exact_duplicate ?
                                           "exact_duplicate" : "rejected" )
                                   << " stale=" << ( stale_disposition ==
                                           bandit_live_world::terminal_hostile_shakedown_replay_disposition::stale_replay ?
                                           "stale_replay" : "rejected" )
                                   << " byte_stable=" << ( byte_stable ? "yes" : "no" )
                                   << " outcome_credit=none\n";
    }

    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &assault_reservation =
            site.active_hostile_operation.reservation;
        for( const character_id member_id : assault_reservation.member_ids ) {
            if( bandit_live_world::active_local_assault_site_for( state, member_id ) != &site ) {
                continue;
            }
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( member == nullptr || member->is_dead() || g->find_npc( member_id ) != member.get() ) {
                continue;
            }
            member->reconcile_active_assault_routine( site.site_id + ":" +
                    assault_reservation.activity_id + "#" +
                    std::to_string( assault_reservation.generation ) );
        }
        changed |= live_bandit_establish_shakedown_communication( site );
        changed |= live_bandit_apply_shakedown_defender_aftermath( site, u );
        bandit_live_world::active_outing_state *external_outing = site.active_external_outing();
        if( external_outing != nullptr && external_outing != &site.active_outing ) {
            if( site.active_hostile_operation.operation_kind ==
                bandit_live_world::hostile_operation_kind::raid ) {
                if( live_cannibal_raid_reconcile_combat( site ) ) {
                    changed = true;
                } else {
                    changed |= materialize_committed_cannibal_raid( site );
                    changed |= live_cannibal_raid_advance_site_search( site );
                }
            } else {
                changed |= live_bandit_handle_hostile_shakedown_contact( site, u );
            }
            continue;
        }
        if( site.retired_empty_site || !site.active_outing.is_active() || site.active_outing.member_ids.empty() ) {
            continue;
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
                if( cursor &&
                commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::reconcile_local_pair_casualties( next, *cursor, casualty_reads,
                        current_minutes );
                }, true ) ) {
                    changed = true;
                }
            }
        }

        // Dedicated local casualty and overdue-missing reconciliation above remains structural
        // authority.  Structural bounty maintenance owns every other structural transition;
        // the generic observation, phase, and route writer below is for modern scouts and
        // hostile operations only.
        if( site.active_outing.kind == bandit_live_world::outing_kind::structural_sortie ) {
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
            // A native save/reload can retain an inactive persistent projection
            // beside a non-null active lookup.  Route the durable projection so
            // the generic exact-pair handoff reads the same homeward motor.
            const shared_ptr_fast<npc> persistent_member = overmap_buffer.find_npc( member_id );
            if( persistent_member && !persistent_member->is_active() ) {
                member_npc = persistent_member.get();
            }
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
                    const bool local_contact_committed =
                    commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                        return bandit_live_world::observe_active_sortie_local_contact( next, *cursor, member_id,
                                current_minutes );
                    } );
                    changed |= local_contact_committed;
                }
                if( live_bandit_member_routing_home( *member_npc, site ) ) {
                    observation.state = bandit_live_world::active_member_observation_state::returning_home;
                    observation.summary = "scout returning home after sortie limit";
                }
            } else if( member->state == bandit_live_world::member_state::local_contact ) {
                // A persisted local projection can be inactive immediately after reload.
                // That alone is not a return decision: issuing a home route here bypasses
                // the sortie's source-owned observation window and splits the exact pair.
                // Keep the local owner stationary until the normal sortie limit authorizes
                // the homeward motor (or a distinct withdrawal phase already requires it).
                const bool homeward_route_due =
                    bandit_live_world::scout_sortie_should_return_home(
                        site, current_minutes, scout_sortie_limit_minutes ) ||
                    bandit_live_world::scout_phase_requires_homeward_only(
                        site.active_outing.phase );
                if( homeward_route_due && !member_npc->is_active() &&
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
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::apply_active_scout_observations( next, *cursor, observations,
                        current_minutes );
            }, true ) :
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
            if( packet &&
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::apply_return_packet( next, *packet );
            }, true ) ) {
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
            if( cursor &&
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::complete_covert_scout_burned_egress( next, *cursor, acquire_reads,
                    current_minutes );
            } ) ) {
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
                    commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                        return bandit_live_world::transition_active_scout_phase( next, *cursor, site.active_outing.phase,
                                bandit_live_world::scout_phase::returning_home, current_minutes,
                                "live party returning home" );
                    } );
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
                        if( !persist_live_bandit_local_projection_leases( candidate, &site ) ) {
                            continue;
                        }
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
                changed |= commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                    return bandit_live_world::release_covert_cohesion_abort_after_target_clear( next, *cursor,
                            acquire_reads );
                } );
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
        bandit_live_world::record_local_gate_semantic_event( site, gate_input, gate_decision );
        DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_local_gate_report( site, gate_input,
                                   gate_decision )
                                   << "- live_existing_active_group=yes\n";
        if( gate_decision.combat_forward ) {
            for( const character_id &member_id : site.active_outing.member_ids ) {
                if( npc *member_npc = g->find_npc( member_id ) ) {
                    const bandit_live_world::member_record *member = site.find_member( member_id );
                    if( member != nullptr && member->state == bandit_live_world::member_state::local_contact ) {
                        changed |= live_bandit_note_combat_intent( *member_npc, site, gate_input,
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
            const bool applied_return = commit_live_bandit_local_progress( site, [&](
            bandit_live_world::site_record & next ) {
                return bandit_live_world::apply_return_packet( next, *packet );
            } );
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

struct live_bandit_sound_observation {
    tripoint_abs_omt source_omt;
    int volume = 0;
    sounds::significant_sound_t kind = sounds::significant_sound_t::none;
    int emitted_minutes = -1;
};

struct r008_channel_stream {
    std::string path;
    std::string run_id;
    std::uint64_t sequence = 0;
    std::uint64_t scan_sequence = 0;
    bool initialized = false;
    bool writable = false;
};

r008_channel_stream r008_channels;

bool r008_channel_stream_enabled()
{
    const char *const path_value = std::getenv( "OPENCLAW_HARNESS_R008_CHANNEL_PATH" );
    const char *const run_value = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const char *const scenario_value = std::getenv( "OPENCLAW_HARNESS_SCENARIO" );
    const char *const source_value = std::getenv( "OPENCLAW_HARNESS_RUNTIME_SOURCE_SHA256" );
    const char *const executable_value = std::getenv( "OPENCLAW_HARNESS_EXECUTABLE_SHA256" );
    const char *const binding_value = std::getenv( "OPENCLAW_HARNESS_BINDING_ID" );
    if( path_value == nullptr || path_value[0] == '\0' || run_value == nullptr ||
        run_value[0] == '\0' || scenario_value == nullptr || scenario_value[0] == '\0' ||
        source_value == nullptr || source_value[0] == '\0' || executable_value == nullptr ||
        executable_value[0] == '\0' || binding_value == nullptr || binding_value[0] == '\0' ) {
        return false;
    }
    const std::string path( path_value );
    const std::string run_id( run_value );
    if( r008_channels.initialized ) {
        return r008_channels.writable && r008_channels.path == path &&
               r008_channels.run_id == run_id;
    }
    r008_channels.initialized = true;
    r008_channels.path = path;
    r008_channels.run_id = run_id;
    std::error_code error;
    r008_channels.writable = !std::filesystem::exists( path, error ) && !error;
    return r008_channels.writable;
}

void append_r008_channel_record( const std::string_view channel,
                                 const std::string_view signal_origin,
                                 const std::string_view consumer,
                                 const bool observed, const int game_minutes,
                                 const std::string_view source_omt,
                                 const std::string_view scan_id,
                                 const std::string_view detail )
{
    if( !r008_channel_stream_enabled() || game_minutes < 0 || channel.empty() ||
        scan_id.empty() ||
        signal_origin.empty() || consumer.empty() ) {
        return;
    }
    const char *const scenario = std::getenv( "OPENCLAW_HARNESS_SCENARIO" );
    const char *const source = std::getenv( "OPENCLAW_HARNESS_RUNTIME_SOURCE_SHA256" );
    const char *const executable = std::getenv( "OPENCLAW_HARNESS_EXECUTABLE_SHA256" );
    const char *const binding = std::getenv( "OPENCLAW_HARNESS_BINDING_ID" );
    if( scenario == nullptr || source == nullptr || executable == nullptr || binding == nullptr ||
        r008_channels.sequence == std::numeric_limits<std::uint64_t>::max() ) {
        return;
    }
    std::ofstream output( r008_channels.path, std::ios::app );
    if( !output ) {
        r008_channels.writable = false;
        return;
    }
    const std::uint64_t sequence = ++r008_channels.sequence;
    JsonOut json( output );
    json.start_object();
    json.member( "schema", "caol-r008-production-channel-v1" );
    json.member( "sequence", sequence );
    json.member( "run_id", r008_channels.run_id );
    json.member( "scan_id", scan_id );
    json.member( "binding" );
    json.start_object();
    json.member( "binding_id", binding );
    json.member( "runtime_source_sha256", source );
    json.member( "executable_sha256", executable );
    json.member( "scenario_id", scenario );
    json.end_object();
    json.member( "scan" );
    json.start_object();
    json.member( "game_minutes", game_minutes );
    json.member( "fresh", true );
    json.member( "isolated", true );
    json.end_object();
    json.member( "channel", channel );
    json.member( "signal_origin", signal_origin );
    json.member( "consumer", consumer );
    json.member( "observed", observed );
    json.member( "isolated", true );
    if( !source_omt.empty() ) {
        json.member( "source_omt", source_omt );
    }
    if( !detail.empty() ) {
        json.member( "detail", detail );
    }
    json.end_object();
    output << '\n';
}

void record_r008_production_channel_scan(
    const std::vector<live_bandit_signal_observation> &signals,
    const std::vector<live_bandit_sound_observation> &sounds )
{
    if( !r008_channel_stream_enabled() ) {
        return;
    }
    const int now_minutes = live_bandit_current_minutes();
    if( r008_channels.scan_sequence == std::numeric_limits<std::uint64_t>::max() ) {
        return;
    }
    const std::string scan_id = r008_channels.run_id + ":" +
                                std::to_string( ++r008_channels.scan_sequence );
    bool smoke = false;
    bool light = false;
    bool sound_observed = false;
    for( const live_bandit_signal_observation &signal : signals ) {
        const std::string source = signal.source_omt.to_string();
        if( signal.mark.kind == "smoke" ) {
            smoke = true;
            append_r008_channel_record( "smoke", "local_field", "bandit_live_world.signal_scan",
                                         true, now_minutes, source, scan_id, signal.mark.mark_id );
        } else if( signal.mark.kind == "light" || signal.mark.kind == "searchlight" ) {
            light = true;
            append_r008_channel_record( "light", "local_field", "bandit_live_world.signal_scan",
                                         true, now_minutes, source, scan_id, signal.mark.mark_id );
        }
    }
    for( const live_bandit_sound_observation &sound : sounds ) {
        sound_observed = true;
        append_r008_channel_record( "sound", "significant_sound", "bandit_live_world.sound_adapter",
                                     true, now_minutes, sound.source_omt.to_string(),
                                     scan_id, std::to_string( sound.volume ) );
    }
    if( !sound_observed ) {
        append_r008_channel_record( "sound", "significant_sound", "bandit_live_world.sound_adapter",
                                     false, now_minutes, "", scan_id, "absent" );
    }
    if( !smoke ) {
        append_r008_channel_record( "smoke", "local_field", "bandit_live_world.signal_scan",
                                     false, now_minutes, "", scan_id, "absent" );
    }
    if( !light ) {
        append_r008_channel_record( "light", "local_field", "bandit_live_world.signal_scan",
                                     false, now_minutes, "", scan_id, "absent" );
    }
    append_r008_channel_record( "scent", "none", "bandit_live_world.signal_scan", false,
                                 now_minutes, "", scan_id, "not_consumed" );
    append_r008_channel_record( "prior_knowledge", "none", "bandit_live_world.signal_scan", false,
                                 now_minutes, "", scan_id, "not_consumed" );
    append_r008_channel_record( "incidental_contact", "none", "bandit_live_world.signal_scan", false,
                                 now_minutes, "", scan_id, "not_consumed" );
}

static constexpr int live_bandit_system_envelope_omt = 40;
static constexpr int live_bandit_local_source_scan_radius_ms = 60;
static constexpr int live_bandit_structural_scan_budget = 12;
static constexpr int live_bandit_structural_dispatch_cap = 2;
static constexpr int live_bandit_structural_watch_path_budget = 8;

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

int live_bandit_materialize_abstract_members(
    bandit_live_world::world_state &state, bandit_live_world::site_record &site,
    const int members_to_create, const std::string &purpose,
    std::string *failure_reason = nullptr )
{
    const auto fail = [failure_reason]( const std::string &reason ) {
        if( failure_reason != nullptr ) {
            *failure_reason = reason;
        }
        return 0;
    };
    if( site.source_kind != bandit_live_world::anchor_source_kind::overmap_special ||
        site.source_id.empty() || site.living_total <= 0 || members_to_create <= 0 ) {
        return fail( "unsupported_home_allocation_source" );
    }
    const bandit_live_world::roster_view roster = site.roster();
    if( !roster.valid || members_to_create > roster.unmaterialized_home_total ) {
        return fail( "invalid_home_allocation_roster" );
    }

    const npc_template_id template_id = live_bandit_template_for_site( site.site_kind );
    if( template_id.is_null() || !template_id.is_valid() ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world lazy materialization skipped: site="
                                   << site.site_id << " reason=invalid_template template="
                                   << template_id.str() << '\n';
        return fail( "invalid_home_allocation_template" );
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
        // Claim through the existing population owner, but publish neither an NPC nor
        // a changed roster unless it consumes exactly one slot of this exact site.
        // A mismatched source/footprint must remain retryable, not create a new camp.
        bandit_live_world::world_state candidate = state;
        const bool claimed = bandit_live_world::claim_tracked_spawn( candidate, template_id.str(),
                             bandit->getID(), bandit->pos_abs(), site.source_id, std::nullopt, special_lookup );
        const bandit_live_world::site_record *allocated = candidate.find_site( site.site_id );
        if( claimed && candidate.sites.size() == state.sites.size() && allocated != nullptr &&
            allocated->has_member( bandit->getID() ) && allocated->roster().valid &&
            allocated->living_total == site.living_total &&
            allocated->roster().unmaterialized_home_total == site.roster().unmaterialized_home_total - 1 ) {
            overmap_buffer.insert_npc( bandit );
            site = *allocated;
            created_members++;
        } else {
            fail( "home_allocation_claim_failed" );
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world lazy materialization skipped member: site="
                                       << site.site_id << " reason=claim_failed template="
                                       << template_id.str() << '\n';
        }
    }

    if( created_members > 0 ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world lazy materialized " << purpose << ": site="
                                   << site.site_id << " created_members=" << created_members
                                   << " concrete_live_members=" << site.count_live_members()
                                   << " living_total=" << site.living_total
                                   << " template=" << template_id.str() << '\n';
    }
    return created_members;
}

// Loaded bodies own their current senses and location.  The persistent overmap copy
// is the sensor only when the actor is not loaded; never borrow an outing projection.
npc *live_bandit_staffed_observer( const character_id id )
{
    for( npc &guy : g->all_npcs() ) {
        if( guy.getID() == id ) {
            return &guy;
        }
    }
    return overmap_buffer.find_npc( id ).get();
}

bandit_live_world::camp_signal_observer_resolution resolve_live_bandit_staffed_observer(
    bandit_live_world::world_state &state, const std::size_t site_index )
{
    bandit_live_world::site_record &site = state.sites[site_index];
    bandit_live_world::camp_signal_observer_resolution result;
    if( site.retired_empty_site || !site.roster().valid ) {
        result.exclusion_reason = site.retired_empty_site ? "retired_empty_site" : "invalid_roster";
        return result;
    }
    if( site.roster().physically_present_total == 0 ) {
        result.exclusion_reason = "no_living_home_population";
        return result;
    }
    const auto resolve_concrete = [&site, &result]() {
        const bandit_live_world::roster_view roster = site.roster();
        for( const character_id id : roster.physically_present_ids ) {
            if( std::binary_search( roster.reserved_unresolved_ids.begin(),
                                    roster.reserved_unresolved_ids.end(), id ) ) {
                continue;
            }
            result.request.observer_id = id;
            result.request.camp_omt = site.anchor;
            npc *observer = live_bandit_staffed_observer( id );
            if( observer == nullptr ) {
                result.exclusion_reason = "home_sensor_missing";
            } else if( observer->is_dead() ) {
                result.exclusion_reason = "home_sensor_dead";
            } else if( observer->pos_abs_omt() != site.anchor &&
                       !site_contains_omt( site, observer->pos_abs_omt() ) ) {
                result.exclusion_reason = "home_sensor_outside_camp";
            } else if( !live_bandit_can_make_ordinary_visual_observation( *observer ) &&
                       ( observer->is_deaf() || observer->has_effect( effect_narcosis ) ) ) {
                result.exclusion_reason = "home_sensor_no_usable_senses";
            } else {
                result.request.camp_omt = observer->pos_abs_omt();
                result.request.owner = observer->is_active() ? bandit_live_world::simulation_owner::local :
                                       bandit_live_world::simulation_owner::abstract;
                result.exclusion_reason.clear();
                return true;
            }
        }
        return false;
    };
    if( resolve_concrete() ) {
        return result;
    }
    if( site.roster().unmaterialized_home_total > 0 ) {
        std::string failure;
        if( live_bandit_materialize_abstract_members( state, site, 1,
                "abstract home observer", &failure ) == 1 && resolve_concrete() ) {
            return result;
        }
        if( !failure.empty() ) {
            result.exclusion_reason = failure;
        }
    }
    if( result.exclusion_reason.empty() ) {
        result.exclusion_reason = "no_unreserved_home_sensor";
    }
    return result;
}

int live_bandit_materialize_abstract_members_for_routine(
    bandit_live_world::world_state &state, bandit_live_world::site_record &site,
    const bool admitted_cannibal_signal_response = false )
{
    const bandit_live_world::hostile_site_profile profile = site.profile ==
            bandit_live_world::hostile_site_profile::none ?
            bandit_live_world::profile_for_site_kind( site.site_kind ) : site.profile;
    // Cannibal camps remain abstract until a production signal admits their
    // response.  Structural routine maintenance otherwise consumes an NPC id
    // before the source-owned signal path can materialize its exact pair.
    if( profile == bandit_live_world::hostile_site_profile::cannibal_camp &&
        !admitted_cannibal_signal_response ) {
        return 0;
    }
    int members_to_create = bandit_live_world::routine_scout_materialization_count( site );
    if( profile == bandit_live_world::hostile_site_profile::small_hostile_site &&
        !site.retired_empty_site && !site.has_active_outside_pressure() ) {
        const bandit_live_world::roster_view roster = site.roster();
        if( roster.valid ) {
            members_to_create = std::min( roster.unmaterialized_home_total,
                                          std::max( 0, 1 - roster.ready_concrete_total ) );
        }
    }
    return live_bandit_materialize_abstract_members( state, site, members_to_create,
            "abstract routine roster" );
}

int live_bandit_materialize_abstract_members_for_response(
    bandit_live_world::world_state &state, bandit_live_world::site_record &site )
{
    if( site.camp_decision.state !=
        bandit_live_world::camp_decision_state::report_awaiting_assessment ) {
        return 0;
    }
    const std::vector<bandit_live_world::response_member_power_read> reads =
        live_bandit_response_member_power_reads_impl( site );
    const int ready_concrete_source_members = static_cast<int>( std::count_if(
            reads.begin(), reads.end(), []( const bandit_live_world::response_member_power_read & read ) {
        return read.authoritative_present && read.at_source_camp && read.ready;
    } ) );
    return live_bandit_materialize_abstract_members( state, site,
            bandit_live_world::hostile_response_materialization_count( site,
                    ready_concrete_source_members ), "abstract response roster" );
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
    bandit_live_world_projection_lease projection_lease;
};

void restore_live_bandit_local_projection( const live_bandit_local_handoff_member_backup &backup )
{
    npc &member = *backup.member;
    member.goal = backup.goal;
    member.omt_path = backup.omt_path;
    member.mission = backup.mission;
    member.previous_mission = backup.previous_mission;
    member.goto_to_this_pos = backup.ordered_position;
    if( backup.ai_guard_position ) {
        member.set_ai_guard_pos( *backup.ai_guard_position );
    } else {
        member.clear_ai_guard_pos();
    }
    member.path = backup.local_path;
    sync_bandit_live_world_projection_lease_copies( member, backup.projection_lease );
}

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
                                 observer->sees_without_clairvoyance( here, critter );
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
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::record_structural_local_zombie_observation( next, *cursor, *read,
                    now_minutes );
        } );
        if( effect.valid ) {
            recorded++;
            std::ostringstream threat_ids;
            for( std::size_t index = 0; index < read->stable_threat_ids.size(); ++index ) {
                if( index > 0 ) {
                    threat_ids << ',';
                }
                threat_ids << read->stable_threat_ids[index];
            }
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_zombie_observation accepted"
                                       << " site=" << site.site_id
                                       << " activity=" << site.active_outing.activity_id
                                       << " generation=" << site.active_outing.generation
                                       << " observer=" << read->observer_id
                                       << " source_omt=" << read->source_omt.to_string()
                                       << " stable_threat_ids=" << threat_ids.str()
                                       << " danger_low=" << read->danger_low
                                       << " danger_high=" << read->danger_high
                                       << " accepted_count=" << effect.inserted + effect.replaced
                                       << '\n';
        }
    }
    return recorded;
}

std::map<character_id, tripoint_abs_ms> maintain_live_bandit_local_pair_cohesion()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    const auto forward_destinations =
        bandit_live_world::local_pair_ingress_travel_destinations( state );
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
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
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
        // Once the common forward owner accepts an assembled pair, staging must
        // not recapture it while it approaches its watch.  Signal height does not
        // change this ownership boundary.  Retain the schema-9 saved-watch migration.
        if( bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) ||
            forward_destinations.count( outing.leader_id ) > 0 ) {
            continue;
        }
        if( bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
            !outing.casualty_ids.empty() ) {
            // A confirmed casualty resolves the pair cohesion obligation.  The bounded
            // homeward selector owns the remaining survivor's exit; do not overwrite its
            // physical route with a two-member staging plan.
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor || !persist_live_bandit_local_projection_leases( site, &site, false ) ) {
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
        struct homeward_route_backup {
            npc *member = nullptr;
            npc_mission mission = NPC_MISSION_NULL;
            npc_mission previous_mission = NPC_MISSION_NULL;
            tripoint_abs_omt goal = npc::no_goal_point;
            std::vector<tripoint_abs_omt> omt_path;
        };
        std::vector<homeward_route_backup> homeward_backups;
        const auto selected_watch_waypoint = std::find(
                    site.active_outing.shared_route.begin(), site.active_outing.shared_route.end(),
                    site.active_outing.selected_watch_omt );
        const bool forward_route_release =
            plan.snapshot.cohesion_assembled &&
            plan.snapshot.phase == bandit_live_world::scout_phase::observing &&
            site.active_outing.schema_version >= 10 &&
            site.active_outing.waypoint_index >= 0 &&
            selected_watch_waypoint != site.active_outing.shared_route.end() &&
            std::distance( site.active_outing.shared_route.begin(), selected_watch_waypoint ) >
            site.active_outing.waypoint_index &&
            plan.snapshot.waypoint_index == site.active_outing.waypoint_index &&
            plan.snapshot.route_position == site.active_outing.shared_route[
                static_cast<std::size_t>( site.active_outing.waypoint_index )] &&
            plan.snapshot.egress_omt == site.active_outing.shared_route[
                static_cast<std::size_t>( site.active_outing.waypoint_index + 1 )] &&
            *selected_watch_waypoint == site.active_outing.selected_watch_omt;
        if( forward_route_release ) {
            bandit_live_world::site_record route_site = site;
            route_site.active_outing.local_handoff = plan.snapshot;
            // Ingress validates its native mission against the final watch, not merely the
            // next shared-route waypoint.  This is the same destination when adjacent and
            // preserves the required mission identity for intermediate-waypoint routes.
            bool route_preparation_failed = false;
            for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
                 plan.snapshot.members ) {
                if( snapshot.dead ) {
                    continue;
                }
                npc *member = g->find_npc( snapshot.npc_id );
                if( member == nullptr || member->is_dead() ) {
                    route_preparation_failed = true;
                    break;
                }
                if( member->is_travelling() &&
                    member->goal == site.active_outing.selected_watch_omt &&
                    !member->omt_path.empty() ) {
                    continue;
                }
                homeward_backups.push_back( { member, member->mission,
                                              member->previous_mission, member->goal,
                                              member->omt_path } );
                if( !live_bandit_route_member_to( *member, route_site,
                                                   site.active_outing.selected_watch_omt ) ) {
                    route_preparation_failed = true;
                    break;
                }
            }
            if( route_preparation_failed ) {
                for( const homeward_route_backup &backup : homeward_backups ) {
                    backup.member->mission = backup.mission;
                    backup.member->previous_mission = backup.previous_mission;
                    backup.member->goal = backup.goal;
                    backup.member->omt_path = backup.omt_path;
                }
                append_assembly_orders( site.active_outing );
                continue;
            }
        }
        const bool restaging_homeward =
            plan.snapshot.route_position != site.active_outing.local_handoff.route_position &&
            bandit_live_world::scout_phase_requires_homeward_only( plan.snapshot.phase );
        if( restaging_homeward ) {
            bandit_live_world::site_record route_site = site;
            route_site.active_outing.local_handoff = plan.snapshot;
            bool route_preparation_failed = false;
            for( const bandit_live_world::local_handoff_member_snapshot &snapshot :
                 plan.snapshot.members ) {
                if( snapshot.dead ) {
                    continue;
                }
                npc *member = g->find_npc( snapshot.npc_id );
                if( member == nullptr || member->is_dead() ) {
                    route_preparation_failed = true;
                    break;
                }
                std::vector<tripoint_abs_omt> route = live_bandit_member_route_to(
                    *member, route_site, site.anchor );
                const tripoint_abs_omt current_omt = member->pos_abs_omt();
                if( route.empty() ||
                    !std::any_of( route.begin(), route.end(), [&current_omt](
                                      const tripoint_abs_omt & omt ) {
                    return omt != current_omt;
                } ) || !live_bandit_route_respects_covert_ring(
                    route_site.active_outing, route ) ) {
                    route_preparation_failed = true;
                    break;
                }
                homeward_backups.push_back( { member, member->mission,
                                              member->previous_mission, member->goal,
                                              member->omt_path } );
                member->goal = site.anchor;
                member->omt_path = std::move( route );
                member->set_mission( NPC_MISSION_TRAVELLING );
            }
            const std::size_t living_member_count = static_cast<std::size_t>( std::count_if(
                plan.snapshot.members.begin(), plan.snapshot.members.end(),
                []( const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
                    return !snapshot.dead;
                } ) );
            if( route_preparation_failed || homeward_backups.size() != living_member_count ) {
                for( const homeward_route_backup &backup : homeward_backups ) {
                    backup.member->mission = backup.mission;
                    backup.member->previous_mission = backup.previous_mission;
                    backup.member->goal = backup.goal;
                    backup.member->omt_path = backup.omt_path;
                }
                append_assembly_orders( site.active_outing );
                continue;
            }
        }
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

        if( !commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::commit_local_pair_cohesion( next, plan, route_attempted, route_failed );
        } ) ) {
            if( forward_route_release ) {
                // A saved, already assembled local pair has no further cohesion-state delta
                // to commit, but may still need its native forward route rebound after load.
                // The route was prepared transactionally for both members above; retaining it
                // here makes the existing ingress preflight admit that durable local state.
                append_assembly_orders( site.active_outing );
                continue;
            }
            for( const homeward_route_backup &backup : homeward_backups ) {
                backup.member->mission = backup.mission;
                backup.member->previous_mission = backup.previous_mission;
                backup.member->goal = backup.goal;
                backup.member->omt_path = backup.omt_path;
            }
            for( const order_backup &backup : backups ) {
                backup.member->goto_to_this_pos = backup.ordered_position;
                if( backup.ai_guard_position ) {
                    backup.member->set_ai_guard_pos( *backup.ai_guard_position );
                } else {
                    backup.member->clear_ai_guard_pos();
                }
                backup.member->path = backup.path;
            }
            // A same-minute cohesion re-read may have no persisted delta, but the local owner
            // must retain motor priority for the rest of that minute.  Rebuild the motor view
            // from the current authoritative outing even when this pass attempted a route.
            append_assembly_orders( site.active_outing );
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
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::commit_loaded_local_pair_alternate_watch_reposition( next, plan );
        } ) !=
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
        // Schema 11 seals the local-return eligibility receipt at handoff.
        // It remains the same local structural outing, so the exact-pair
        // arrival motor must admit it rather than stranding the assembled
        // pair before its outcome and physical return path.
        // commit_scout_pair_watch_arrival validates that both actors are physically at the
        // selected watch.  A direct egress to that watch is therefore an admission case.
        const bool frontier = bandit_live_world::structural_outing_uses_frontier_route( outing );
        const tripoint_abs_omt destination = frontier ? outing.target_omt : outing.selected_watch_omt;
        if( ( outing.schema_version < 10 && !frontier ) ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            !outing.local_handoff.is_active() ||
            !outing.local_handoff.cohesion_assembled ||
            outing.waypoint_index + 1 >= static_cast<int>( outing.shared_route.size() ) ) {
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
            read.route_confirmed = member->goal == destination ||
                                   member->pos_abs_omt() == destination;
            read.hp_percent = member->hp_percentage();
            read.current_position = member->pos_abs();
            reads.push_back( read );
            members.push_back( member );
        }
        if( ( !any_loaded && !frontier ) || reads.size() != 2 ||
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return frontier ? bandit_live_world::complete_structural_frontier_arrival(
                   next, *cursor, live_bandit_current_minutes(), reads ) :
               bandit_live_world::commit_scout_pair_watch_arrival( next, *cursor,
                       live_bandit_current_minutes(), reads );
        } ) !=
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
            if( frontier ) {
                live_bandit_route_member_home( *member, site );
            }
        }
        changed = true;
    }
    return changed;
}

bool handoff_live_bandit_generic_scout_returns()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::scout_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ||
            outing.crossing.pending() || outing.member_ids.size() != 2 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }

        std::vector<shared_ptr_fast<npc>> returned_members;
        returned_members.reserve( outing.member_ids.size() );
        bool complete_return = true;
        for( const character_id member_id : outing.member_ids ) {
            const bandit_live_world::member_record *member = site.find_member( member_id );
            const shared_ptr_fast<npc> persistent_member = overmap_buffer.find_npc( member_id );
            if( member == nullptr || member->state != bandit_live_world::member_state::local_contact ||
                !persistent_member || persistent_member->is_dead() ||
                persistent_member->is_active() || here.inbounds( persistent_member->pos_abs() ) ||
                !live_bandit_member_routing_home( *persistent_member, site ) ) {
                complete_return = false;
                break;
            }
            returned_members.push_back( persistent_member );
        }
        if( !complete_return || returned_members.size() != outing.member_ids.size() ) {
            continue;
        }

        const bandit_live_world::simulation_owner_transition_result result =
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::transition_external_simulation_owner( next, cursor->activity_id,
                    cursor->generation,
                    bandit_live_world::simulation_owner::local,
                    bandit_live_world::simulation_owner::abstract, cursor->handoff_epoch,
                    cursor->last_advanced_minutes, live_bandit_current_minutes() );
        } );
        if( result != bandit_live_world::simulation_owner_transition_result::applied ) {
            continue;
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world generic scout return handoff committed"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " generation=" << site.active_outing.generation
                                   << " epoch=" << site.active_outing.handoff_epoch
                                   << " members=" << returned_members.size() << '\n';
        changed = true;
    }
    return changed;
}

namespace
{
bandit_live_world_probe::scout_homeward_member_read read_live_bandit_homeward_actor(
    const character_id id, const npc *actual_member = nullptr )
{
    bandit_live_world_probe::scout_homeward_member_read read;
    read.npc_id = id.get_value();
    npc *loaded = nullptr;
    for( npc &candidate : g->all_npcs() ) {
        if( candidate.getID() == id ) {
            loaded = &candidate;
            break;
        }
    }
    const auto stored = overmap_buffer.find_npc( id );
    const npc *member = actual_member != nullptr ? actual_member :
                        loaded != nullptr ? loaded : stored.get();
    read.found = member != nullptr;
    if( member != nullptr ) {
        read.loaded = loaded == member;
        read.active = member->is_active();
        read.in_bounds = get_map().inbounds( member->pos_abs() );
        read.dead = member->is_dead();
        read.sleeping = member->in_sleep_state();
        read.cannot_move = member->has_flag( json_flag_CANNOT_MOVE );
        read.movement_impaired = member->has_effect( effect_downed ) ||
                                 member->has_effect( effect_stunned ) ||
                                 member->has_effect( effect_psi_stunned ) ||
                                 member->has_effect( effect_narcosis );
        read.travelling = member->is_travelling();
        read.has_destination = member->has_omt_destination();
        read.mission = static_cast<int>( member->mission );
        read.moves = member->get_moves();
        read.position_ms = member->pos_abs().to_string();
        read.position_omt = member->pos_abs_omt().to_string();
        read.goal_omt = member->goal.to_string();
        read.local_path_size = member->path.size();
        read.omt_path_size = member->omt_path.size();
        if( !member->omt_path.empty() ) {
            read.next_omt = member->omt_path.back().to_string();
        }
    }
    return read;
}

std::optional<bandit_live_world_probe::transition_event> make_live_bandit_homeward_event(
    const bandit_live_world::site_record &site, const std::string_view transition,
    const std::string_view entrypoint )
{
    const auto &outing = site.active_outing;
    if( !bandit_live_world_probe::transition_events_enabled() || !outing.is_active() ||
        !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
        return std::nullopt;
    }
    bandit_live_world_probe::transition_event event;
    event.domain = "bandit_live_world";
    event.transition = transition;
    event.site_id = site.site_id;
    event.operation_id = outing.activity_id;
    event.generation = outing.generation;
    event.handoff_epoch = outing.handoff_epoch;
    event.simulation_owner = bandit_live_world::to_string( outing.owner );
    event.previous_phase = bandit_live_world::to_string( outing.phase );
    event.new_phase = event.previous_phase;
    event.turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    event.game_minutes = live_bandit_current_minutes();
    event.at_minutes = event.game_minutes;
    event.scout_homeward.emplace();
    auto &read = *event.scout_homeward;
    read.entrypoint = entrypoint;
    read.outing_kind = bandit_live_world::to_string( outing.kind );
    read.owner_before = event.simulation_owner;
    read.handoff_epoch_before = outing.handoff_epoch;
    read.outing_member_count = outing.member_ids.size();
    read.cursor_present = bandit_live_world::current_external_simulation_cursor( site ).has_value();
    read.handoff_active = outing.local_handoff.is_active();
    read.crossing_pending = outing.crossing.pending();
    read.handoff_member_count = outing.local_handoff.members.size();
    // Both production transfer adapters consume an exact pair. Keep invalid
    // larger records bounded too; the actual handoff count remains explicit.
    for( const character_id id : outing.member_ids ) {
        if( read.members.size() == 2 ) {
            break;
        }
        event.actor_ids.push_back( id.get_value() );
        read.members.push_back( read_live_bandit_homeward_actor( id ) );
    }
    return event;
}

void record_live_bandit_homeward_transfer(
    std::optional<bandit_live_world_probe::transition_event> &event,
    const bandit_live_world::site_record &site, const std::string_view outcome,
    const std::string &reason, const bool plan_invoked,
    const std::optional<bool> plan_valid = std::nullopt,
    const std::string_view commit_result = {} )
{
    if( !event ) {
        return;
    }
    event->outcome = outcome;
    event->reason = reason;
    event->new_phase = bandit_live_world::to_string( site.active_outing.phase );
    event->simulation_owner = bandit_live_world::to_string( site.active_outing.owner );
    event->handoff_epoch = site.active_outing.handoff_epoch;
    event->scout_homeward->plan_invoked = plan_invoked;
    event->scout_homeward->plan_valid = plan_valid;
    event->scout_homeward->commit_result = commit_result;
    bandit_live_world_probe::record_scout_homeward_observation( std::move( *event ) );
    event.reset();
}

std::string_view live_bandit_homeward_commit_result(
    const bandit_live_world::local_handoff_commit_result result )
{
    switch( result ) {
        case bandit_live_world::local_handoff_commit_result::rejected:
            return "rejected";
        case bandit_live_world::local_handoff_commit_result::unchanged:
            return "unchanged";
        case bandit_live_world::local_handoff_commit_result::applied:
            return "applied";
        case bandit_live_world::local_handoff_commit_result::rolled_back:
            return "rolled_back";
    }
    return "unknown";
}

class live_bandit_homeward_motor_trace
{
    public:
        explicit live_bandit_homeward_motor_trace( npc &member, const bool selected ) :
            member_( member ) {
            if( !selected || !bandit_live_world_probe::transition_events_enabled() ) {
                return;
            }
            for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
                const auto &outing = site.active_outing;
                if( outing.owner == bandit_live_world::simulation_owner::local &&
                    bandit_live_world::current_external_simulation_cursor( site ) &&
                    std::find( outing.member_ids.begin(), outing.member_ids.end(), member.getID() ) !=
                    outing.member_ids.end() ) {
                    event_ = make_live_bandit_homeward_event(
                                 site, "scout_homeward_motor", "local_npc_motor" );
                    if( event_ ) {
                        event_->actor_ids = { member.getID().get_value() };
                        event_->scout_homeward->members = {
                            read_live_bandit_homeward_actor( member.getID(), &member )
                        };
                        event_->reason = "selected homeward actor reached the NPC action loop";
                        event_->scout_homeward->members.front().selected = true;
                        break;
                    }
                }
            }
        }

        ~live_bandit_homeward_motor_trace() {
            if( event_ ) {
                auto &read = event_->scout_homeward->members.front();
                read.position_after_ms = member_.pos_abs().to_string();
                read.moves_after = member_.get_moves();
                if( event_->outcome.empty() ) {
                    event_->outcome = read.position_ms != read.position_after_ms ? "moved" : "stationary";
                }
                bandit_live_world_probe::record_scout_homeward_observation( std::move( *event_ ) );
            }
        }

        void action( const std::string_view action, const std::string_view reason,
                     const std::string_view outcome = {} ) {
            if( event_ ) {
                event_->scout_homeward->action = action;
                event_->reason = reason;
                event_->outcome = outcome;
            }
        }

        bandit_live_world_probe::scout_homeward_observation *read() {
            return event_ ? &*event_->scout_homeward : nullptr;
        }

    private:
        npc &member_;
        std::optional<bandit_live_world_probe::transition_event> event_;
};
} // namespace

bool dematerialize_live_bandit_structural_handoffs()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        auto homeward_event = make_live_bandit_homeward_event(
                                  site, "scout_homeward_dematerialization", "stable_unloaded" );
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.schema_version < 7 ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "dematerialization eligibility gate did not pass", false );
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "current_external_simulation_cursor returned no cursor", false );
            continue;
        }

        // A complete homeward pair already inside its own camp must be consumed by the
        // physical-return recorder below.  Turning that exact arrival into another abstract
        // resume drops its return receipts and lets a later materialization rewind the persisted
        // homeward cursor.
        if( !persist_live_bandit_local_projection_leases( site, &site, false ) ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                  "actor projection copies do not match the current local cursor", false );
            continue;
        }
        const bool complete_physical_return_ready =
            bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
            std::all_of( outing.member_ids.begin(), outing.member_ids.end(),
        [&site, &outing]( const character_id member_id ) {
            if( outing.member_is_resolved( member_id ) ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                outing.casualty_ids.end() ) {
                return true;
            }
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            return member && !member->is_dead() &&
                   site_contains_omt( site, member->pos_abs_omt() );
        } );
        if( complete_physical_return_ready ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "complete_physical_return_ready", false );
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
                backups.push_back( { overmap_buffer.find_npc( member->getID() ), member->pos_abs(), member->goal, member->omt_path,
                                 member->mission, member->previous_mission,
                                 member->goto_to_this_pos, member->get_ai_guard_pos(), member->path, {} } );
            backups.back().projection_lease = member->get_bandit_live_world_projection_lease();
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
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "dematerialization member preflight did not pass", false );
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
            if( !live_bandit_projection_copies_match( *backup->member,
                    live_bandit_projection_lease( site.site_id, site.active_outing ) ) ) {
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
            reset_live_bandit_local_projection(
                *backup->member, bandit_live_world_projection_lease() );
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
            restore_live_bandit_local_projection( *backup );
            if( here.inbounds( backup->position ) && !backup->member->is_active() ) {
                g->load_npcs();
            }
        };

        if( outing.alternate_watch_reposition_pending ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "alternate-watch reposition adapter selected", false );
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
        if( homeward_event ) {
            for( auto &member : homeward_event->scout_homeward->members ) {
                const auto input = std::find_if( reads.begin(), reads.end(),
                [&member]( const auto &read ) {
                    return read.npc_id.get_value() == member.npc_id;
                } );
                if( input != reads.end() ) {
                    member.transfer_position_ms = input->current_position.to_string();
                }
            }
        }
        if( !plan.valid ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "rejected",
                                                 plan.notes.empty() ? "invalid dematerialization plan returned without a note" :
                                                 plan.notes.front(), true, false );
            continue;
        }
        const bandit_live_world::local_handoff_commit_result result =
            bandit_live_world::commit_local_pair_dematerialization(
                site, plan, quiesce_member, rollback_member );
        record_live_bandit_homeward_transfer( homeward_event, site,
                                             result == bandit_live_world::local_handoff_commit_result::applied ? "committed" :
                                             live_bandit_homeward_commit_result( result ),
                                             "commit_local_pair_dematerialization returned " +
                                             std::string( live_bandit_homeward_commit_result( result ) ), true, true,
                                             live_bandit_homeward_commit_result( result ) );
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

namespace
{
void record_live_bandit_scout_travel_trace( const bandit_live_world::site_record &site,
        const bandit_live_world::active_outing_state &outing, const character_id member_id,
        const std::string_view outcome, std::string reason,
        const std::string_view transition = "scout_travel_prepare" )
{
    if( !bandit_live_world_probe::transition_events_enabled() ) {
        return;
    }

    struct trace_cache {
        std::string run_id;
        std::map<std::string, std::string> last_signature;
        std::size_t emitted = 0;
    };
    static trace_cache cache;
    const char *const run_id_value = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const std::string run_id = run_id_value == nullptr ? std::string() : run_id_value;
    if( !run_id.empty() ) {
        if( cache.run_id != run_id ) {
            cache = {};
            cache.run_id = run_id;
        }
        const std::string key = site.site_id + "#" + outing.activity_id + "#" +
                                 std::to_string( outing.generation ) + "#" +
                                 std::to_string( member_id.get_value() ) + "#" + std::string( transition );
        const std::string signature = std::string( outcome ) + '|' + reason;
        const auto previous = cache.last_signature.find( key );
        if( ( previous != cache.last_signature.end() && previous->second == signature ) ||
            cache.emitted >= bandit_live_world_probe::max_transition_events ) {
            return;
        }
        cache.last_signature[key] = signature;
        cache.emitted++;
    }

    if( reason.size() > bandit_live_world_probe::max_transition_event_reason_length ) {
        reason.resize( bandit_live_world_probe::max_transition_event_reason_length );
    }
    bandit_live_world_probe::transition_event event;
    event.domain = "bandit_live_world";
    event.transition = transition;
    event.outcome = outcome;
    event.site_id = site.site_id;
    event.operation_id = outing.activity_id;
    event.generation = outing.generation;
    event.simulation_owner = bandit_live_world::to_string( outing.owner );
    event.previous_phase = bandit_live_world::to_string( outing.phase );
    event.new_phase = event.previous_phase;
    event.reason = std::move( reason );
    event.actor_ids = { member_id.get_value() };
    event.at_minutes = live_bandit_current_minutes();
    event.game_minutes = event.at_minutes;
    bandit_live_world_probe::record_live_transition_event( event );
    bandit_live_world_probe::record_transition_event( std::move( event ) );
}

std::string live_bandit_scout_member_state( const npc *member )
{
    if( member == nullptr ) {
        return "found=0";
    }
    std::ostringstream out;
    out << "f=1 a=" << member->is_active()
        << " d=" << member->is_dead()
        << " cm=" << member->has_flag( json_flag_CANNOT_MOVE )
        << " ms=" << member->pos_abs().to_string()
        << " omt=" << member->pos_abs_omt().to_string()
        << " m=" << static_cast<int>( member->mission )
        << " g=" << ( member->has_omt_destination() ? member->goal.to_string() : "none" )
        << " pn=" << member->omt_path.size();
    if( !member->omt_path.empty() ) {
        out << " pf=" << member->omt_path.front().to_string()
            << " pl=" << member->omt_path.back().to_string();
    }
    return out.str();
}
} // namespace

void record_live_bandit_scout_motor_service( const npc &member,
        const std::string_view outcome, const tripoint_abs_ms &before )
{
    if( !bandit_live_world_probe::transition_events_enabled() ) {
        return;
    }
    for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
        const auto &outing = site.active_outing;
        if( !outing.is_active() || outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            std::find( outing.member_ids.begin(), outing.member_ids.end(), member.getID() ) ==
            outing.member_ids.end() ) {
            continue;
        }
        std::ostringstream reason;
        reason << "caller=overmap_npc_move result=" << outcome
               << " before=" << before.to_string() << " after=" << member.pos_abs().to_string()
               << " moved=" << ( before != member.pos_abs() ) << ' '
               << live_bandit_scout_member_state( &member );
        record_live_bandit_scout_travel_trace( site, outing, member.getID(), outcome,
                                             reason.str(), "scout_motor_service" );
    }
}

// Only the current unique abstract watch assignment can own this completed
// destination. Position or an empty path cannot create an assignment.
bool pending_abstract_scout_watch_order( const npc &member )
{
    if( !member.is_travelling() || member.is_active() || member.is_dead() ||
        member.goal != member.pos_abs_omt() ||
        overmap_buffer.find_npc( member.getID() ).get() != &member ) {
        return false;
    }
    const bandit_live_world::site_record *owner = nullptr;
    for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
        const auto &outing = site.active_outing;
        const auto &hostile = site.active_hostile_operation;
        const auto contains = [&member]( const auto &ids ) {
            return std::find( ids.begin(), ids.end(), member.getID() ) != ids.end();
        };
        if( hostile.is_active() && contains( hostile.reservation.member_ids ) ) {
            return false;
        }
        if( !outing.is_active() || !contains( outing.member_ids ) ) {
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
    const auto &outing = owner->active_outing;
    if( owner->retired_empty_site || outing.local_projection_reconciliation_rejected ||
        outing.kind != bandit_live_world::outing_kind::structural_sortie ||
        outing.owner != bandit_live_world::simulation_owner::abstract || outing.schema_version < 10 ||
        bandit_live_world::structural_outing_uses_frontier_route( outing ) ||
        outing.phase != bandit_live_world::scout_phase::observing ||
        outing.alternate_watch_reposition_pending || outing.crossing.pending() ||
        outing.local_handoff.is_active() || outing.local_handoff.is_abstract_resume() ||
        !outing.actor_departure_observed || outing.member_ids.size() != 2 ||
        outing.member_ids[0] == outing.member_ids[1] ||
        !outing.casualty_ids.empty() || !outing.resolved_member_ids.empty() ||
        outing.waypoint_index != 1 || outing.actor_route_waypoint != 1 ||
        outing.assessment.observation_started_minutes >= 0 ||
        outing.selected_watch_kind == bandit_live_world::structural_watch_kind::none ||
        outing.shared_route.size() != 5 || outing.shared_route[2] != member.goal ||
        outing.selected_watch_omt != member.goal ||
        !bandit_live_world::structural_watch_shared_route_is_canonical(
            outing.shared_route, owner->anchor, outing.selected_watch_omt, outing.target_footprint ) ||
        !bandit_live_world::current_external_simulation_cursor( *owner ) ||
        live_bandit_current_minutes() < outing.last_advanced_minutes ) {
        return false;
    }
    const auto cursor = bandit_live_world::current_external_simulation_cursor( *owner );
    if( !cursor || cursor->activity_id != outing.activity_id ||
        cursor->generation != outing.generation || cursor->owner != outing.owner ||
        cursor->handoff_epoch != outing.handoff_epoch ||
        cursor->last_advanced_minutes != outing.last_advanced_minutes ||
        cursor->covert_egress_revision != outing.covert_egress_revision ) {
        return false;
    }
    for( const character_id id : outing.member_ids ) {
        const auto actor = overmap_buffer.find_npc( id );
        const auto *record = owner->find_member( id );
        if( !actor || actor->is_active() || actor->is_dead() || !actor->is_travelling() ||
            actor->goal != outing.selected_watch_omt || record == nullptr ||
            record->state != bandit_live_world::member_state::outbound ) {
            return false;
        }
        // A different loaded/persistent representation must not lend this
        // abstract order its identity or lease.
        for( const npc *copy : bandit_live_world_projection_lease_copies( *actor ) ) {
            if( copy != actor.get() || copy->get_bandit_live_world_projection_lease().present ) {
                return false;
            }
        }
    }
    return true;
}

std::set<character_id> prepare_live_bandit_abstract_scout_travel()
{
    std::set<character_id> held;
    for( auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
        auto &outing = site.active_outing;
        if( !outing.is_active() || outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::abstract ||
            outing.local_handoff.is_abstract_resume() || outing.crossing.pending() ||
            ( outing.schema_version < 10 &&
              !bandit_live_world::structural_outing_uses_frontier_route( outing ) ) ||
            ( outing.selected_watch_kind == bandit_live_world::structural_watch_kind::none &&
              !bandit_live_world::structural_outing_uses_frontier_route( outing ) ) ||
            ( bandit_live_world::structural_outing_uses_frontier_route( outing ) ?
              outing.shared_route.size() < 3 : outing.shared_route.size() != 5 ) ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        // Absence is not death.  Use the existing generation-bound casualty
        // writer, and its missing deadline, before releasing a surviving member.
        const auto assigned_ids = outing.member_ids;
        for( const character_id id : assigned_ids ) {
            if( outing.member_is_resolved( id ) ) {
                continue;
            }
            const auto member = overmap_buffer.find_npc( id );
            if( member && !member->is_dead() ) {
                continue;
            }
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::record_matching_external_outing_casualty( next, outing.activity_id,
                        outing.generation, id,
                        member ? bandit_live_world::member_state::dead : bandit_live_world::member_state::missing,
                        live_bandit_current_minutes(), member ? "assigned abstract scout confirmed dead" :
                        "assigned abstract scout absent after missing deadline",
                        member ? std::optional<tripoint_abs_ms>( member->pos_abs() ) : std::nullopt );
            } );
        }
        std::vector<npc *> members;
        std::size_t unresolved = 0;
        for( const character_id id : outing.member_ids ) {
            if( outing.member_is_resolved( id ) ) {
                record_live_bandit_scout_travel_trace( site, outing, id, "resolved",
                        "member already resolved" );
                continue;
            }
            ++unresolved;
            const auto member = overmap_buffer.find_npc( id );
            const bool frontier_ready =
                ( !bandit_live_world::structural_outing_uses_frontier_route( outing ) &&
                  outing.phase != bandit_live_world::scout_phase::observing ) ||
                                        ( member && live_bandit_member_can_take_homeward_step( *member ) &&
                                          member->get_attitude() != NPCATT_FLEE &&
                                          member->get_attitude() != NPCATT_FLEE_TEMP &&
                                          !member->has_active_faction_alarm() );
            const bool eligible = member && !member->is_active() && !member->is_dead() &&
                                  !member->has_flag( json_flag_CANNOT_MOVE ) && frontier_ready;
            if( eligible ) {
                members.push_back( member.get() );
            }
        }
        if( members.empty() || members.size() != unresolved ) {
            for( const character_id id : outing.member_ids ) {
                if( outing.member_is_resolved( id ) ) {
                    continue;
                }
                const auto member = overmap_buffer.find_npc( id );
                std::ostringstream reason;
                reason << "eligibility=";
                if( !member ) {
                    reason << "missing";
                } else if( member->is_active() ) {
                    reason << "active";
                } else if( member->is_dead() ) {
                    reason << "dead";
                } else if( member->has_flag( json_flag_CANNOT_MOVE ) ) {
                    reason << "cannot_move";
                } else {
                    reason << "eligible_pair_incomplete";
                }
                reason << " wp=" << outing.actor_route_waypoint
                       << " depart=" << outing.actor_departure_observed << ' '
                       << live_bandit_scout_member_state( member.get() );
                record_live_bandit_scout_travel_trace( site, outing, id, "held",
                        reason.str() );
            }
            held.insert( outing.member_ids.begin(), outing.member_ids.end() );
            continue;
        }
        if( outing.actor_route_waypoint < 0 ) {
            // Bind a new departure only.  A legacy homeward record with no
            // movement history cannot turn camp-parked NPCs into return proof.
            if( outing.phase != bandit_live_world::scout_phase::outbound ||
                outing.waypoint_index != 0 || outing.handoff_epoch != 0 ||
                !std::all_of( members.begin(), members.end(), [&site]( const npc *member ) {
                    return site_contains_omt( site, member->pos_abs_omt() );
                } ) ) {
                for( const npc *member : members ) {
                    std::ostringstream reason;
                    reason << "departure_origin_invalid waypoint=" << outing.waypoint_index
                           << " epoch=" << outing.handoff_epoch << ' '
                           << live_bandit_scout_member_state( member );
                    record_live_bandit_scout_travel_trace( site, outing, member->getID(), "held",
                            reason.str() );
                }
                held.insert( outing.member_ids.begin(), outing.member_ids.end() );
                continue;
            }
            outing.actor_route_waypoint = 0;
        }
        if( std::all_of( members.begin(), members.end(), [&site]( const npc *member ) {
            return !site_contains_omt( site, member->pos_abs_omt() );
        } ) ) {
            outing.actor_departure_observed = true;
        }
        const bool homeward = bandit_live_world::scout_phase_requires_homeward_only( outing.phase );
        // The return half is independent of outbound progress.  In particular,
        // an early withdrawal must never finish the outbound watch leg first.
        const int home_index = static_cast<int>( outing.shared_route.size() ) - 1;
        const int destination_index = homeward ?
                ( outing.actor_route_waypoint < 2 || outing.actor_route_waypoint >= home_index - 1 ?
                  home_index : home_index - 1 ) :
                outing.phase == bandit_live_world::scout_phase::outbound ? 1 :
                bandit_live_world::structural_outing_uses_frontier_route( outing ) ? home_index - 1 : 2;
        const tripoint_abs_omt destination = outing.shared_route[destination_index];
        const bool arrived = std::all_of( members.begin(), members.end(),
        [&destination]( const npc *member ) {
            return member->pos_abs_omt() == destination;
        } );
        if( arrived ) {
            if( homeward && !outing.actor_departure_observed ) {
                // Cancel a bound departure that never became a complete field
                // party.  This releases the actual home identities without a
                // return receipt, assessment, or scout report.
                held.insert( outing.member_ids.begin(), outing.member_ids.end() );
                bandit_live_world::release_structural_outing_reservation(
                    site, outing.activity_id, outing.generation,
                    "assigned scout departure canceled at home without field report" );
                continue;
            }
            if( !homeward && outing.phase == bandit_live_world::scout_phase::observing &&
                outing.waypoint_index == destination_index &&
                outing.actor_route_waypoint == destination_index &&
                outing.assessment.observation_started_minutes >= 0 ) {
                // The pair already consumed its orders. An empty generic goal
                // after normal cleanup does not require a second arrival.
                held.insert( outing.member_ids.begin(), outing.member_ids.end() );
                continue;
            }
            if( !homeward && outing.phase == bandit_live_world::scout_phase::observing ) {
                const auto cursor = bandit_live_world::current_external_simulation_cursor( site );
                auto arrival_result = bandit_live_world::local_handoff_commit_result::rejected;
                if( cursor ) {
                    std::vector<bandit_live_world::local_route_arrival_member_read> reads;
                    for( const npc *member : members ) {
                        const auto &lease = member->get_bandit_live_world_projection_lease();
                        reads.push_back( { member->getID(), true, false,
                            member->goal == destination && !lease.present &&
                            live_bandit_member_can_take_homeward_step( *member ) &&
                            member->get_attitude() != NPCATT_FLEE &&
                            member->get_attitude() != NPCATT_FLEE_TEMP,
                            member->hp_percentage(), member->pos_abs() } );
                    }
                    if( bandit_live_world::structural_outing_uses_frontier_route( outing ) ) {
                        arrival_result = bandit_live_world::complete_structural_frontier_arrival(
                                             site, *cursor, live_bandit_current_minutes(), reads );
                    } else {
                        arrival_result = bandit_live_world::commit_scout_pair_watch_arrival(
                                             site, *cursor, live_bandit_current_minutes(), reads );
                    }
                }
                for( const npc *member : members ) {
                    std::ostringstream reason;
                    reason << "attempted=" << bool( cursor ) << " result="
                           << ( cursor ? live_bandit_homeward_commit_result( arrival_result ) : "cursor_unavailable" )
                           << " object=persistent_overmap goal_matches=" << ( member->goal == destination )
                           << " lease_present=" << member->get_bandit_live_world_projection_lease().present
                           << " wp=" << outing.waypoint_index << " actor_wp=" << outing.actor_route_waypoint
                           << " dest=" << destination.to_string() << ' '
                           << live_bandit_scout_member_state( member );
                    record_live_bandit_scout_travel_trace( site, outing, member->getID(),
                                                         cursor ? live_bandit_homeward_commit_result( arrival_result ) : "unavailable",
                                                         reason.str(), "scout_watch_arrival_service" );
                }
                const bool survival_refusal = std::any_of( members.begin(), members.end(),
                []( const npc *member ) {
                    return !live_bandit_member_can_take_homeward_step( *member ) ||
                           member->get_attitude() == NPCATT_FLEE ||
                           member->get_attitude() == NPCATT_FLEE_TEMP;
                } );
                for( npc *member : members ) {
                    if( member->is_travelling() && member->goal == destination &&
                        member->omt_path.empty() &&
                        ( arrival_result == bandit_live_world::local_handoff_commit_result::applied ||
                          arrival_result == bandit_live_world::local_handoff_commit_result::unchanged ||
                          !survival_refusal || !pending_abstract_scout_watch_order( *member ) ) ) {
                        // Consume successful orders; invalidated ownership also
                        // releases generic travel. Temporary survival refusal
                        // leaves the still-valid pair order pending.
                        member->reach_omt_destination();
                    }
                }
            } else {
                outing.actor_route_waypoint = destination_index;
            }
            held.insert( outing.member_ids.begin(), outing.member_ids.end() );
            continue;
        }
        if( !homeward && outing.phase == bandit_live_world::scout_phase::observing &&
            outing.actor_route_waypoint >= destination_index ) {
            // A persisted alternate watch is a new physical destination.
            outing.actor_route_waypoint = destination_index - 1;
        }
        std::vector<std::vector<tripoint_abs_omt>> routes;
        std::vector<std::string> route_diagnostics;
        const std::unordered_set<tripoint_abs_omt> route_exclusions =
            live_bandit_covert_route_exclusions( outing );
        for( const npc *member : members ) {
            if( member->pos_abs_omt() == destination ) {
                routes.emplace_back();
                route_diagnostics.push_back( "route=at_destination covert=not_needed" );
                continue;
            }
            if( member->goal == destination && member->is_travelling() && !member->omt_path.empty() ) {
                const bool endpoints_ok = member->omt_path.front() == destination &&
                                          member->omt_path.back() == member->pos_abs_omt();
                const bool covert_ok = live_bandit_route_respects_covert_ring( outing,
                                       member->omt_path );
                if( endpoints_ok && covert_ok ) {
                    routes.push_back( member->omt_path );
                    std::ostringstream detail;
                    detail << "route=reused len=" << member->omt_path.size()
                           << " front=" << member->omt_path.front().to_string()
                           << " back=" << member->omt_path.back().to_string()
                           << " endpoints=1 covert=1";
                    route_diagnostics.push_back( detail.str() );
                    continue;
                }
            }
            auto route = overmap_buffer.get_travel_path( member->pos_abs_omt(), destination,
                         overmap_path_params::for_npc(), route_exclusions ).points;
            const bool endpoints_ok = !route.empty() && route.front() == destination &&
                                      route.back() == member->pos_abs_omt();
            const bool covert_ok = !route.empty() &&
                                   live_bandit_route_respects_covert_ring( outing, route );
            std::ostringstream detail;
            detail << "route=" << ( route.empty() ? "empty" : "computed" )
                   << " len=" << route.size();
            if( !route.empty() ) {
                detail << " front=" << route.front().to_string()
                       << " back=" << route.back().to_string();
            }
            detail << " endpoints=" << endpoints_ok << " covert=" << covert_ok;
            route_diagnostics.push_back( detail.str() );
            if( !endpoints_ok || !covert_ok ) {
                break;
            }
            routes.push_back( std::move( route ) );
        }
        if( routes.size() != members.size() ) {
            for( std::size_t index = 0; index < members.size(); ++index ) {
                const npc *member = members[index];
                std::ostringstream reason;
                reason << "eligibility=eligible dest=" << destination.to_string()
                       << " wp=" << outing.actor_route_waypoint
                       << " depart=" << outing.actor_departure_observed << ' ';
                if( index < route_diagnostics.size() ) {
                    reason << route_diagnostics[index];
                } else {
                    reason << "route=not_evaluated prior_member_route_rejected=1";
                }
                reason << " result=pair_held " << live_bandit_scout_member_state( member );
                record_live_bandit_scout_travel_trace( site, outing, member->getID(), "held",
                        reason.str() );
            }
            held.insert( outing.member_ids.begin(), outing.member_ids.end() );
            continue;
        }
        for( std::size_t index = 0; index < members.size(); ++index ) {
            npc &member = *members[index];
            if( member.pos_abs_omt() == destination ) {
                std::ostringstream reason;
                reason << "eligibility=eligible dest=" << destination.to_string()
                       << " wp=" << outing.actor_route_waypoint
                       << " depart=" << outing.actor_departure_observed
                       << " route=at_destination result=no_assignment "
                       << live_bandit_scout_member_state( &member );
                record_live_bandit_scout_travel_trace( site, outing, member.getID(), "at_destination",
                        reason.str() );
                held.insert( member.getID() );
                continue;
            }
            if( member.goal != destination || !member.is_travelling() ||
                member.omt_path != routes[index] ) {
                member.goal = destination;
                member.omt_path = std::move( routes[index] );
                member.guard_pos.reset();
                member.clear_ai_guard_pos();
                member.set_mission( NPC_MISSION_TRAVELLING );
            }
            std::ostringstream reason;
            reason << "eligibility=eligible dest=" << destination.to_string()
                   << " wp=" << outing.actor_route_waypoint
                   << " depart=" << outing.actor_departure_observed << ' '
                   << route_diagnostics[index] << " result=assigned "
                   << live_bandit_scout_member_state( &member );
            record_live_bandit_scout_travel_trace( site, outing, member.getID(), "prepared",
                    reason.str() );
        }
    }
    return held;
}

bool record_live_bandit_structural_member_returns()
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            ( outing.owner != bandit_live_world::simulation_owner::local &&
              !( outing.owner == bandit_live_world::simulation_owner::abstract &&
                 ( outing.local_handoff.is_abstract_resume() ||
                   ( outing.actor_departure_observed && outing.actor_route_waypoint > 0 &&
                     outing.actor_route_waypoint ==
                     static_cast<int>( outing.shared_route.size() ) - 1 ) ) &&
                 !outing.crossing.pending() ) ) ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        // A loaded pair owns one physical arrival transaction.  Do not unload the first
        // member that reaches the camp OMT while its exact partner is still travelling;
        // both identity-bound receipts must be preflighted and committed together.
        std::vector<character_id> arriving_ids;
        arriving_ids.reserve( outing.member_ids.size() );
        bool complete_arrival = true;
        for( const character_id member_id : outing.member_ids ) {
            if( outing.member_is_resolved( member_id ) ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                outing.casualty_ids.end() ) {
                continue;
            }
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( !member || member->is_dead() || !site_contains_omt( site, member->pos_abs_omt() ) ) {
                complete_arrival = false;
                break;
            }
            arriving_ids.push_back( member_id );
        }
        if( !complete_arrival || arriving_ids.empty() ) {
            continue;
        }

        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor || !persist_live_bandit_local_projection_leases( site, &site, false ) ) {
            continue;
        }
        const int current_minutes = live_bandit_current_minutes();
        bandit_live_world::site_record candidate = site;
        bool committed = true;
        for( const character_id member_id : arriving_ids ) {
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            const auto member_cursor = bandit_live_world::current_external_simulation_cursor( candidate );
            if( !member || !member_cursor || !bandit_live_world::record_structural_member_physical_return(
                    candidate, *member_cursor, member_id, member->pos_abs_omt(), current_minutes ) ) {
                committed = false;
                break;
            }
        }
        if( !committed || !persist_live_bandit_local_projection_leases( candidate, &site ) ) {
            continue;
        }
        site = std::move( candidate );
        for( const character_id member_id : arriving_ids ) {
            shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( !member ) {
                continue;
            }
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local physical return receipt committed"
                                       << " site=" << site.site_id
                                       << " activity=" << site.active_outing.activity_id
                                       << " generation=" << site.active_outing.generation
                                       << " member=" << member_id
                                       << " receipts="
                                       << site.active_outing.member_return_receipts.size()
                                       << " owner=" << bandit_live_world::to_string(
                                           site.active_outing.owner ) << '\n';
            member->on_unload();
            g->remove_npc( member_id );
            changed = true;
        }
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
    map &here = get_map();
    bool changed = false;
    for( bandit_live_world::site_record &site : state.sites ) {
        const bandit_live_world::active_outing_state &pre_handoff = site.active_outing;
        if( !site.retired_empty_site && pre_handoff.is_active() &&
            pre_handoff.kind == bandit_live_world::outing_kind::structural_sortie &&
            pre_handoff.owner == bandit_live_world::simulation_owner::abstract &&
            bandit_live_world::scout_phase_requires_homeward_only( pre_handoff.phase ) &&
            !pre_handoff.local_handoff.is_abstract_resume() &&
            pre_handoff.member_ids.size() == 2 &&
            pre_handoff.actor_route_waypoint > pre_handoff.waypoint_index ) {
            const auto cursor = bandit_live_world::current_external_simulation_cursor( site );
            std::vector<bandit_live_world::local_abstract_resume_progress_read> reads;
            for( const character_id id : pre_handoff.member_ids ) {
                const auto member = overmap_buffer.find_npc( id );
                if( !member || member->is_active() || here.inbounds( member->pos_abs() ) ) {
                    break;
                }
                reads.push_back( { id, true, member->is_dead(),
                                   member->is_dead() ? 0 : member->hp_percentage(),
                                   member->pos_abs() } );
            }
            if( cursor && reads.size() == pre_handoff.member_ids.size() &&
                bandit_live_world::begin_abstract_pair_homeward_resume(
                    site, *cursor, live_bandit_current_minutes(), reads ) ) {
                changed = true;
                continue;
            }
        }
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        const bool homeward_candidate = outing.is_active() &&
                                        outing.kind == bandit_live_world::outing_kind::structural_sortie &&
                                        outing.owner == bandit_live_world::simulation_owner::abstract &&
                                        bandit_live_world::active_outing_requires_homeward_routing( outing );
        const auto log_homeward_rejection = [&site, &outing, homeward_candidate](
        const std::string_view reason ) {
            if( !homeward_candidate && outing.phase != bandit_live_world::scout_phase::observing ) {
                return;
            }
            DebugLog( D_INFO, DC_ALL )
                    << "bandit_live_world materialization rejected"
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
        // A bound abstract party must reach the handoff OMT with these same
        // identities before staging can transfer ownership.  The phase cursor
        // alone is not permission to move camp-parked actors to the watch.
        if( outing.actor_route_waypoint >= 0 && !outing.local_handoff.is_abstract_resume() ) {
            const tripoint_abs_omt handoff_omt = outing.shared_route[outing.waypoint_index];
            if( surviving_member_ids.empty() ||
                !std::all_of( surviving_member_ids.begin(), surviving_member_ids.end(),
            [&handoff_omt]( const character_id id ) {
                const auto member = overmap_buffer.find_npc( id );
                return member && !member->is_active() && !member->is_dead() &&
                       member->pos_abs_omt() == handoff_omt;
            } ) ) {
                continue;
            }
        }
        const bool resumes_physical_homeward_cursor = outing.local_handoff.is_abstract_resume();
        // A failed local rendezvous records an explicit abort-return resume.  It has no
        // assembled local pair to retain, but the complete persistent pair may be rebound
        // transactionally at the next safe interior edge.
        const bool resumes_recenterable_homeward_pair = resumes_physical_homeward_cursor &&
                ( outing.local_handoff.cohesion_assembled ||
                  outing.local_handoff.cohesion_abort_return ) &&
                bandit_live_world::scout_phase_requires_homeward_only( outing.phase );
        tripoint_abs_omt route_position = resumes_physical_homeward_cursor ?
                                          outing.local_handoff.route_position :
                                          outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
        const bool retains_accepted_homeward_route_edge =
            resumes_physical_homeward_cursor && route_position !=
            outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )] &&
            !outing.local_handoff.cohesion_abort_return;
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
        const std::vector<tripoint_abs_ms> initial_entry_positions = entry_positions;
        const std::vector<tripoint_abs_ms> initial_staging_positions = staging_positions;
        const std::size_t initial_entry_count = entry_positions.size();
        const std::size_t initial_staging_count = staging_positions.size();
        const auto position_list = []( const std::vector<tripoint_abs_ms> &positions ) {
            std::ostringstream result;
            for( std::size_t index = 0; index < positions.size(); ++index ) {
                if( index > 0 ) {
                    result << ',';
                }
                result << positions[index].to_string();
            }
            return result.str();
        };
        const auto survivor_list = [&surviving_member_ids]() {
            std::ostringstream result;
            for( std::size_t index = 0; index < surviving_member_ids.size(); ++index ) {
                if( index > 0 ) {
                    result << ',';
                }
                result << surviving_member_ids[index];
            }
            return result.str();
        };
        const std::optional<int> current_target_distance =
            bandit_live_world::target_footprint_watch_distance(
                outing.local_handoff.route_position, outing.target_footprint );
        const bool assessed_homeward_cursor = resumes_physical_homeward_cursor &&
                outing.local_handoff.route_position ==
                outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
        const bool explicit_abort_return_recenter =
            resumes_recenterable_homeward_pair && outing.local_handoff.cohesion_abort_return;
        std::optional<bandit_live_world::site_record> advanced_homeward_site;
        if( resumes_recenterable_homeward_pair && !retains_accepted_homeward_route_edge &&
            surviving_member_ids.size() == 2 &&
            ( explicit_abort_return_recenter ||
              entry_positions.size() != surviving_member_ids.size() ||
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
                    if( explicit_abort_return_recenter &&
                        ( candidate_route == outing.local_handoff.route_position ||
                          !current_target_distance || !candidate_target_distance ||
                          *candidate_target_distance < *current_target_distance ) ) {
                        continue;
                    }
                    if( derived_route &&
                        ( ( !assessed_homeward_cursor &&
                            !outing.local_handoff.cohesion_abort_return ) ||
                          !current_target_distance ||
                          !candidate_target_distance ||
                          *candidate_target_distance < *current_target_distance ) ) {
                        continue;
                    }
                    // Geometry alone is not enough for a local handoff.  The
                    // constructor will spawn each survivor at this route edge
                    // and immediately give it camp ownership; reject a
                    // candidate whose post-spawn home route cannot be solved,
                    // so selection can continue to the next valid edge while
                    // the abstract owner remains untouched.
                    if( bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
                        const std::vector<tripoint_abs_omt> candidate_home_route =
                            overmap_buffer.get_travel_path(
                                candidate_route, site.anchor,
                                overmap_path_params::for_npc() ).points;
                        if( candidate_home_route.empty() ||
                            !live_bandit_route_respects_covert_ring(
                                outing, candidate_home_route ) ) {
                            DebugLog( D_INFO, DC_ALL )
                                    << "bandit_live_world homeward materialization candidate rejected"
                                    << " site=" << site.site_id
                                    << " candidate_route=" << candidate_route.to_string()
                                    << " reason=camp route unavailable after spawn\n";
                            continue;
                        }
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
                                << " candidate_approach=" << candidate_approach.to_string()
                                << " candidate_facing=" << candidate_facing.to_string()
                                << " entry_count=" << candidate_entries.size()
                                << " entries=" << position_list( candidate_entries )
                                << " staging_count=" << candidate_staging.size()
                                << " staging=" << position_list( candidate_staging )
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
                map &here = get_map();
                const int loaded_width = SEEX * here.getmapsize();
                const int loaded_height = SEEY * here.getmapsize();
                const tripoint_abs_ms loaded_minimum = here.get_abs(
                            tripoint_bub_ms( 0, 0, route_position.z() ) );
                const tripoint_abs_ms loaded_maximum = here.get_abs(
                            tripoint_bub_ms( loaded_width - 1, loaded_height - 1,
                                             route_position.z() ) );
                const avatar &player = get_avatar();
                DebugLog( D_INFO, DC_ALL )
                        << "bandit_live_world homeward materialization rejected"
                        << " site=" << site.site_id
                        << " activity=" << outing.activity_id
                        << " generation=" << outing.generation
                        << " phase=" << bandit_live_world::to_string( outing.phase )
                        << " waypoint=" << outing.waypoint_index
                        << " route_size=" << outing.shared_route.size()
                        << " route_position=" << route_position.to_string()
                        << " persisted_waypoint=" << outing.shared_route[static_cast<std::size_t>(
                                                    outing.waypoint_index )].to_string()
                        << " persisted_resume_route=" << outing.local_handoff.route_position.to_string()
                        << " approach=" << approach_from.to_string()
                        << " facing=" << staging_facing_omt.to_string()
                        << " egress=" << egress_omt.to_string()
                        << " player_omt=" << player.pos_abs_omt().to_string()
                        << " player_sm=" << player.pos_abs_sm().to_string()
                        << " loaded_min_ms=" << loaded_minimum.to_string()
                        << " loaded_max_ms=" << loaded_maximum.to_string()
                        << " abstract_resume=" << ( resumes_physical_homeward_cursor ? "yes" : "no" )
                        << " cohesion_assembled=" <<
                           ( outing.local_handoff.cohesion_assembled ? "yes" : "no" )
                        << " homeward_phase=" <<
                           ( bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ?
                             "yes" : "no" )
                        << " survivor_pair=" << ( surviving_member_ids.size() == 2 ? "yes" : "no" )
                        << " recenter_gate=" << ( resumes_recenterable_homeward_pair ? "yes" : "no" )
                        << " assessed_cursor=" << ( assessed_homeward_cursor ? "yes" : "no" )
                        << " target_distance_known=" << ( current_target_distance ? "yes" : "no" )
                        << " initial_entry_count=" << initial_entry_count
                        << " initial_entries=" << position_list( initial_entry_positions )
                        << " initial_staging_count=" << initial_staging_count
                        << " initial_staging=" << position_list( initial_staging_positions )
                        << " final_entry_count=" << entry_positions.size()
                        << " final_entries=" << position_list( entry_positions )
                        << " final_staging_count=" << staging_positions.size()
                        << " final_staging=" << position_list( staging_positions )
                        << " required_survivors=" << surviving_member_ids.size()
                        << " survivor_ids=" << survivor_list()
                        << " reason=loaded bubble lacks paired entry or staging positions\n";
            }
            continue;
        }

        std::vector<live_bandit_local_handoff_member_backup> backups;
        std::vector<bandit_live_world::local_handoff_member_read> reads;
        std::set<character_id> auto_loaded_boundary_members;
        backups.reserve( surviving_member_ids.size() );
        reads.reserve( surviving_member_ids.size() );
        bool preflight_failed = false;
        for( std::size_t index = 0; index < surviving_member_ids.size(); ++index ) {
            const character_id member_id = surviving_member_ids[index];
            shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            // Native map loading can activate the exact persisted pair before this owner
            // receives its materialization turn.  Rebind only that complete retained-resume
            // identity through the normal transaction; a different active NPC remains an
            // ownership conflict and must fail closed.
            const bool exact_auto_loaded_abort_resume =
                resumes_recenterable_homeward_pair &&
                outing.local_handoff.cohesion_abort_return && member && member->is_active() &&
                g->find_npc( member_id ) == member.get() && here.inbounds( member->pos_abs() );
            const bool exact_auto_loaded_resume =
                resumes_physical_homeward_cursor && member && member->is_active() &&
                g->find_npc( member_id ) == member.get() && here.inbounds( member->pos_abs() ) &&
                ( member->pos_abs_omt() == route_position || exact_auto_loaded_abort_resume );
            // When the player bubble first reaches a newly dispatched route, native map
            // loading may activate the exact pair at its authoritative home anchor before
            // the abstract owner gets its first departure handoff.  That is a legal source
            // boundary: only the complete persisted pair, at waypoint one and at this
            // site's own anchor, may be rebound to the route cursor.  Any other active NPC
            // still denotes a competing local owner and must remain fail-closed.
            const bool exact_auto_loaded_outbound_departure =
                !resumes_physical_homeward_cursor &&
                outing.phase == bandit_live_world::scout_phase::observing &&
                outing.waypoint_index == 1 && outing.shared_route.front() == site.anchor &&
                member && member->is_active() && g->find_npc( member_id ) == member.get() &&
                here.inbounds( member->pos_abs() ) && member->pos_abs_omt() == site.anchor;
            if( !member || member->is_dead() ||
                ( member->is_active() && !exact_auto_loaded_resume &&
                  !exact_auto_loaded_outbound_departure ) ) {
                preflight_failed = true;
                log_homeward_rejection( !member ? "member unavailable" :
                                        member->is_dead() ? "member dead" :
                                        string_format( "member already active id=%d active_omt=%s route_position=%s "
                                                       "abort_return=%s",
                                                       member_id.get_value(),
                                                       member->pos_abs_omt().to_string(),
                                                       route_position.to_string(),
                                                       outing.local_handoff.cohesion_abort_return ? "yes" : "no" ) );
                break;
            }
            if( !live_bandit_projection_copies_match( *member, bandit_live_world_projection_lease() ) ) {
                preflight_failed = true;
                log_homeward_rejection( "previous owner has a conflicting projection lease" );
                break;
            }
            if( exact_auto_loaded_resume || exact_auto_loaded_outbound_departure ) {
                auto_loaded_boundary_members.insert( member_id );
            }
            // The persisted abstract-resume cursor is the route authority here.
            // A concrete NPC's transient goal/path can be stale after the pair was
            // dematerialized at a retained edge; bind_member replaces it with a
            // freshly solved camp route after spawning at that exact edge.
            backups.push_back( { member, member->pos_abs(), member->goal, member->omt_path,
                                 member->mission, member->previous_mission,
                                 member->goto_to_this_pos, member->get_ai_guard_pos(), member->path, {} } );
            backups.back().projection_lease = member->get_bandit_live_world_projection_lease();
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

        const bandit_live_world::site_record &preflight_site =
            advanced_homeward_site ? *advanced_homeward_site : site;
        // A freshly loaded abstract owner may legitimately carry a persisted
        // cursor newer than the process clock (for example after a staged
        // overmap bubble is restored).  Materialization must not rewind that
        // cursor; use its monotonic floor for the shared constructor while
        // retaining the preflight's fail-closed checks for genuinely stale
        // state.
        const int handoff_minutes = std::max( live_bandit_current_minutes(),
                                              preflight_site.active_outing.last_advanced_minutes );
        const bandit_live_world::local_handoff_preflight preflight =
            bandit_live_world::preflight_local_pair_handoff(
                preflight_site, *cursor, handoff_minutes, reads );
        if( !preflight.valid ) {
            for( const std::string &prerequisite : preflight.failed_prerequisites ) {
                log_homeward_rejection( "local handoff " + prerequisite +
                                        " prerequisite failed" );
            }
            continue;
        }
        const bandit_live_world::local_handoff_plan plan =
            bandit_live_world::plan_local_pair_handoff( preflight_site,
                    *cursor, handoff_minutes, reads );
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
        const auto bind_member = [&find_backup, &backups, &site, &plan, homeward_handoff,
                                  &bind_failure_reason, &auto_loaded_boundary_members](
        const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
            if( snapshot.dead ) {
                return true;
            }
            const auto backup = find_backup( snapshot.npc_id );
            if( backup == backups.end() ) {
                bind_failure_reason = "member backup unavailable";
                return false;
            }
            if( !live_bandit_projection_copies_match( *backup->member,
                    bandit_live_world_projection_lease() ) ) {
                bind_failure_reason = "previous owner has a conflicting projection lease";
                return false;
            }
            if( auto_loaded_boundary_members.count( snapshot.npc_id ) > 0 ) {
                npc *loaded_member = g->find_npc( snapshot.npc_id );
                if( loaded_member != backup->member.get() || !loaded_member->is_active() ) {
                    bind_failure_reason = "loaded retained resume identity changed";
                    return false;
                }
                loaded_member->on_unload();
                g->remove_npc( snapshot.npc_id );
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
            sync_bandit_live_world_projection_lease_copies( *member,
                    live_bandit_projection_lease( site.site_id, plan.snapshot ) );
            overmap_buffer.insert_npc( member );
            return true;
        };
        const auto rollback_member = [&find_backup, &backups, &here,
                                      &auto_loaded_boundary_members](
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
            restore_live_bandit_local_projection( *backup );
            overmap_buffer.insert_npc( member );
            if( auto_loaded_boundary_members.count( snapshot.npc_id ) > 0 &&
                here.inbounds( backup->position ) ) {
                g->load_npcs();
            }
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
                                   << " recenter_gate=" <<
                                   ( resumes_recenterable_homeward_pair ? "yes" : "no" )
                                   << " recentered=" <<
                                   ( advanced_homeward_site ? "yes" : "no" )
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

std::string live_bandit_source_mark_id( const std::string &kind, const tripoint_abs_ms &pos )
{
    std::ostringstream out;
    out << "live_" << kind << '@' << pos.x() << ',' << pos.y() << ',' << pos.z();
    return out.str();
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

void live_bandit_note_light_geometry( live_bandit_local_source_reading &reading, map &here,
                                      const tripoint_bub_ms &p )
{
    // map::sees is a local transparency query; unlike build_seen_cache it
    // neither reads nor overwrites the avatar visibility cache.
    const physical_light::escape escape = physical_light::evaluate_escape( here, p );
    reading.outside = escape.exposed_to_sky;
    reading.side_leakage = escape.side_leakage;
    reading.elevated_roof_exposed = escape.elevated_exposed;
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

physical_light::loaded_source_sampler live_source_sampler;

std::vector<live_bandit_signal_observation> observe_loaded_z_light_sources()
{
    avatar &u = get_avatar();
    map &here = get_map();
    // Absolute-map-square identity deliberately survives until after local
    // escape is evaluated.  A window lamp may therefore not lend its opening
    // (or its representative position) to a sealed lamp in the same OMT.
    std::map<tripoint_abs_ms, live_bandit_local_source_reading> readings;
    const physical_light::loaded_z_source_index source_index =
        live_source_sampler.sample( u, here, to_turns<int>( calendar::turn - calendar::turn_zero ) );

    for( const physical_light::loaded_z_source_index::field_source &source :
         source_index.field_sources ) {
        live_bandit_local_source_reading &reading = readings[source.position];
        reading.fire_intensity = source.fire_intensity;
        reading.smoke_intensity = source.smoke_intensity;
        live_bandit_note_light_geometry( reading, here, here.get_bub( source.position ) );
    }

    // Item emitters are collected as individual records so their power and
    // provenance are resolved before the OMT reading combines them.
    for( const physical_light::emitter &emitter : source_index.item_emitters ) {
        const tripoint_bub_ms p = here.get_bub( emitter.position );
        if( !here.inbounds( p ) ) {
            continue;
        }
        const int intensity = live_bandit_light_intensity_from_luminance( emitter.luminance );
        const bandit_mark_generation::light_source_band source = emitter.directional ?
                bandit_mark_generation::light_source_band::searchlight :
                bandit_mark_generation::light_source_band::ordinary;
        live_bandit_local_source_reading &reading = readings[emitter.position];
        live_bandit_note_light_source( reading, intensity, source, emitter.position );
        live_bandit_note_light_geometry( reading, here, p );
    }

    for( const physical_light::emitter &emitter : source_index.stationary_emitters ) {
        const tripoint_bub_ms p = here.get_bub( emitter.position );
        if( !here.inbounds( p ) ) {
            continue;
        }
        const int intensity = live_bandit_light_intensity_from_luminance( emitter.luminance );
        const bandit_mark_generation::light_source_band source = emitter.directional ?
                bandit_mark_generation::light_source_band::searchlight :
                bandit_mark_generation::light_source_band::ordinary;
        live_bandit_local_source_reading &reading = readings[emitter.position];
        live_bandit_note_light_source( reading, intensity, source, emitter.position );
        live_bandit_note_light_geometry( reading, here, p );
    }

    std::vector<live_bandit_signal_observation> observations;
    observations.reserve( readings.size() * 2 );
    const bandit_mark_generation::smoke_weather_band weather_band = live_bandit_smoke_weather_band();
    const bandit_mark_generation::light_time_band light_time = live_bandit_light_time_band();
    const bandit_mark_generation::light_weather_band light_weather = live_bandit_light_weather_band();
    const weather_manager &weather = get_weather_const();
    const int observed_turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    const int observed_minutes = live_bandit_current_minutes();
    for( const std::pair<const tripoint_abs_ms, live_bandit_local_source_reading> &entry : readings ) {
        const tripoint_abs_ms &source_pos = entry.first;
        const tripoint_abs_omt source_omt = coords::project_to<coords::omt>( source_pos );
        const live_bandit_local_source_reading &reading = entry.second;
        bandit_mark_generation::local_field_signal_reading adapter_reading;
        adapter_reading.smoke_id = live_bandit_source_mark_id( "smoke", source_pos );
        adapter_reading.light_id = live_bandit_source_mark_id( "light", source_pos );
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
            observation.source_ms = source_pos;
            observation.range_cap_omt = projection.projected_range_omt;
            observation.weather_summary = projection.weather_effect.summary;
            observation.smoke_source_exposed_to_sky = reading.outside;
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
            observation.observed_turn = observed_turn;
            observation.observed_minutes = observed_minutes;
            observation.sample_id = observation.mark.mark_id + "#" +
                                    std::to_string( observed_turn );
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
        light_observation.source_ms = source_pos;
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
        light_observation.observed_turn = observed_turn;
        light_observation.observed_minutes = observed_minutes;
        light_observation.sample_id = light_observation.mark.mark_id + "#" +
                                      std::to_string( observed_turn );
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
                                   << " now_minutes=" << live_bandit_current_minutes()
                                   << " tiles_examined=" << source_index.tiles_examined
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
                                   << " now_minutes=" << live_bandit_current_minutes()
                                   << " tiles_examined=" << source_index.tiles_examined
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

bandit_live_world::structural_route_read live_bandit_structural_route_read(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan, int &watch_path_budget );
std::vector<bandit_live_world::structural_signal_read> live_bandit_staffed_camp_signal_reads(
    const std::vector<live_bandit_signal_observation> &signals,
    const std::vector<live_bandit_sound_observation> &sound_events,
    const bandit_live_world::site_record &site,
    const bandit_live_world::camp_signal_observer_request &request );

int observe_live_hostile_signal_sources(
    const std::vector<live_bandit_signal_observation> &signals )
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    std::vector<const live_bandit_signal_observation *> ordered_signals;
    ordered_signals.reserve( signals.size() );
    for( const live_bandit_signal_observation &signal : signals ) {
        if( signal.range_cap_omt > 0 && !signal.mark.mark_id.empty() ) {
            ordered_signals.push_back( &signal );
        }
    }
    std::sort( ordered_signals.begin(), ordered_signals.end(), [](
    const live_bandit_signal_observation *lhs,
    const live_bandit_signal_observation *rhs ) {
        return std::make_tuple( lhs->mark.kind == "smoke" ? 0 : 1,
                                lhs->source_omt.z(), lhs->source_omt.y(), lhs->source_omt.x(),
                                lhs->mark.mark_id ) <
               std::make_tuple( rhs->mark.kind == "smoke" ? 0 : 1,
                                rhs->source_omt.z(), rhs->source_omt.y(), rhs->source_omt.x(),
                                rhs->mark.mark_id );
    } );

    int observations = 0;
    for( bandit_live_world::site_record &site : state.sites ) {
        const auto receipt_signal_admission = [&site]( const std::string &outcome,
        const std::string &reason, const live_bandit_signal_observation *signal = nullptr ) {
            bandit_live_world_probe::transition_event event;
            event.game_minutes = live_bandit_current_minutes();
            event.domain = "hostile_source";
            event.transition = "production_signal_dispatch_admission";
            event.outcome = outcome;
            event.site_id = site.site_id;
            event.operation_id = signal == nullptr ? "" : signal->mark.mark_id;
            event.simulation_owner = "abstract";
            event.previous_phase = site.has_active_outside_pressure() ? "outside_pressure" : "candidate_ready";
            event.new_phase = event.previous_phase;
            event.reason = reason;
            if( const bandit_live_world::active_outing_state *outing = site.active_external_outing() ) {
                event.operation_id = outing->activity_id;
                event.generation = outing->generation;
                event.handoff_epoch = outing->handoff_epoch;
                event.simulation_owner = bandit_live_world::to_string( outing->owner );
                event.new_phase = bandit_live_world::to_string( outing->phase );
                for( const character_id id : outing->member_ids ) {
                    event.actor_ids.push_back( id.get_value() );
                }
            } else if( site.has_active_outside_pressure() ) {
                // A conflicting or member-only reservation has no unique outing
                // cursor.  Keep its actual changed failure context diagnosable.
                event.new_phase = "outside_pressure_without_unique_outing";
                const auto describe = []( const bandit_live_world::active_outing_state &outing ) {
                    return outing.activity_id + ":" + std::to_string( outing.generation ) + ":" +
                           bandit_live_world::to_string( outing.owner ) + ":" +
                           std::to_string( outing.handoff_epoch ) + ":" +
                           bandit_live_world::to_string( outing.phase );
                };
                event.reason += " outing=" + describe( site.active_outing ) +
                                " operation=" + describe( site.active_hostile_operation.reservation );
                for( const bandit_live_world::member_record &member : site.members ) {
                    if( member.state == bandit_live_world::member_state::outbound ||
                        member.state == bandit_live_world::member_state::local_contact ) {
                        event.actor_ids.push_back( member.npc_id.get_value() );
                        event.reason += " member=" + std::to_string( member.npc_id.get_value() ) +
                                        ":" + bandit_live_world::to_string( member.state );
                    }
                }
            }
            event.at_minutes = event.game_minutes;
            bandit_live_world_probe::record_signal_dispatch_admission( std::move( event ) );
        };
        if( ( site.profile != bandit_live_world::hostile_site_profile::cannibal_camp &&
              site.profile != bandit_live_world::hostile_site_profile::camp_style ) ||
            site.retired_empty_site ) {
            continue;
        }
        if( site.has_active_outside_pressure() ) {
            receipt_signal_admission( "rejected", "active_outside_pressure" );
            continue;
        }
        const auto signal = std::find_if( ordered_signals.begin(), ordered_signals.end(),
        [&site]( const live_bandit_signal_observation * candidate ) {
            return rl_dist( site.anchor, candidate->source_omt ) <= candidate->range_cap_omt;
        } );
        if( signal == ordered_signals.end() ) {
            std::string reason = "no_in_range_production_signal";
            if( !ordered_signals.empty() ) {
                const auto nearest = std::min_element( ordered_signals.begin(), ordered_signals.end(),
                [&site]( const live_bandit_signal_observation * lhs,
                const live_bandit_signal_observation * rhs ) {
                    return rl_dist( site.anchor, lhs->source_omt ) <
                           rl_dist( site.anchor, rhs->source_omt );
                } );
                const live_bandit_signal_observation &candidate = **nearest;
                reason += " nearest_kind=" + candidate.mark.kind +
                          " nearest_source=" + candidate.source_omt.to_string() +
                          " distance_omt=" + std::to_string( rl_dist( site.anchor, candidate.source_omt ) ) +
                          " range_cap_omt=" + std::to_string( candidate.range_cap_omt );
            }
            receipt_signal_admission( "rejected", reason );
            continue;
        }
        std::set<character_id> members_before_materialization;
        for( const bandit_live_world::member_record &member : site.members ) {
            members_before_materialization.insert( member.npc_id );
        }
        const int materialized_members =
            live_bandit_materialize_abstract_members_for_routine( state, site, true );
        if( materialized_members > 0 ) {
            bandit_live_world_probe::transition_event event;
            event.game_minutes = live_bandit_current_minutes();
            event.domain = "hostile_source";
            event.transition = "production_signal_materialization";
            event.outcome = "committed";
            event.site_id = site.site_id;
            event.operation_id = ( *signal )->mark.mark_id;
            event.simulation_owner = "abstract";
            event.previous_phase = "candidate_ready";
            event.new_phase = "concrete_roster_ready";
            event.reason = "signal=" + ( *signal )->mark.kind + " range_cap_omt=" +
                           std::to_string( ( *signal )->range_cap_omt ) +
                           " production_lazy_materialization=" +
                           std::to_string( materialized_members );
            event.at_minutes = event.game_minutes;
            for( const bandit_live_world::member_record &member : site.members ) {
                if( members_before_materialization.count( member.npc_id ) == 0 ) {
                    event.actor_ids.push_back( member.npc_id.get_value() );
                }
            }
            bandit_live_world_probe::record_live_transition_event( std::move( event ) );
        }
        // Both factions learn through the staffed physical observer, then use the
        // same durable paired watch/search/return lifecycle.  Assignment and
        // identity-bound travel remain with structural maintenance and its motor.
        const bandit_live_world::camp_signal_observer_resolution observer =
            resolve_live_bandit_staffed_observer( state, static_cast<std::size_t>( &site - state.sites.data() ) );
        if( !observer.exclusion_reason.empty() ) {
            receipt_signal_admission( "rejected", observer.exclusion_reason, *signal );
            continue;
        }
        const auto reads = live_bandit_staffed_camp_signal_reads(
                               signals, {}, site, observer.request );
        if( std::none_of( reads.begin(), reads.end(), []( const auto &read ) {
            return !read.rejected;
        } ) ) {
            receipt_signal_admission( "rejected", reads.empty() ? "no_physical_signal_read" :
                                      reads.front().rejection_reason, *signal );
            continue;
        }
        bandit_live_world::world_state observation;
        observation.sites.push_back( site );
        const auto observation_result = bandit_live_world::record_staffed_camp_signal_observations(
            observation, live_bandit_current_minutes(),
        [&reads]( const bandit_live_world::site_record &,
        const bandit_live_world::camp_signal_observer_request & ) {
            return reads;
        }, [&observer]( bandit_live_world::world_state &, std::size_t ) {
            return observer;
        } );
        site = std::move( observation.sites.front() );
        // Discovery owns the physical read and durable lead only.  The normal
        // structural maintenance pass is the sole assignment/report owner.
        observations += observation_result.leads_created + observation_result.leads_refreshed;

    }
    return observations;
}

int bootstrap_live_bandit_abstract_sites_near_player()
{
    avatar &u = get_avatar();
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const tripoint_abs_omt center = u.pos_abs_omt();
    std::set<std::string> known_site_ids;
    for( const bandit_live_world::site_record &site : state.sites ) {
        known_site_ids.insert( site.site_id );
    }

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

    // Mapgen and the lazy materializer already register their concrete NPCs
    // through claim_tracked_spawn.  A persisted hostile NPC can instead be
    // loaded from the overmap before either producer runs; reconcile that
    // exact source-footprint actor through the same ownership boundary rather
    // than treating an abstract headcount as a concrete roster.
    int reconciled_persisted_members = 0;
    // This API takes submaps, not map squares.  Three submaps include the
    // adjacent-OMT fixture actor at its 24-map-square offset while excluding
    // the regional hostile roster.
    for( const shared_ptr_fast<npc> &candidate :
         overmap_buffer.get_npcs_near_player( 3 ) ) {
        if( candidate == nullptr || candidate->is_dead() ) {
            continue;
        }
        const tripoint_abs_omt candidate_omt = candidate->pos_abs_omt();
        const std::optional<std::string> source_id = special_lookup( candidate_omt );
        if( !source_id ) {
            continue;
        }
        const std::optional<bandit_live_world::owned_site_kind> site_kind =
            bandit_live_world::classify_tracked_source(
                bandit_live_world::anchor_source_kind::overmap_special, *source_id );
        if( !site_kind ) {
            continue;
        }
        const bool expected_faction =
            ( *site_kind == bandit_live_world::owned_site_kind::cannibal_camp &&
              candidate->get_fac_id().str() == "cannibal_camp" ) ||
            ( *site_kind != bandit_live_world::owned_site_kind::cannibal_camp &&
              candidate->get_fac_id().str() == "hells_raiders" );
        const npc_template_id template_id = live_bandit_template_for_site( *site_kind );
        if( !expected_faction || template_id.is_null() || !template_id.is_valid() ) {
            continue;
        }
        if( bandit_live_world::claim_tracked_spawn( state, template_id.str(), candidate->getID(),
                candidate->pos_abs(), source_id, std::nullopt, special_lookup ) ) {
            reconciled_persisted_members++;
        }
    }

    // The fixture may establish an overmap-special footprint, but its roster is
    // deliberately absent.  Receipt the production registration boundary before
    // any routine materializes an NPC or considers a signal, target, or contact.
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( known_site_ids.count( site.site_id ) != 0 ||
            site.source_kind != bandit_live_world::anchor_source_kind::overmap_special ||
            site.profile != bandit_live_world::hostile_site_profile::cannibal_camp ||
            site.living_total <= 0 ) {
            continue;
        }
        bandit_live_world_probe::transition_event event;
        event.game_minutes = live_bandit_current_minutes();
        event.domain = "hostile_source";
        event.transition = "production_candidate_registered";
        event.outcome = "committed";
        event.site_id = site.site_id;
        event.operation_id = "abstract_bootstrap";
        event.simulation_owner = "abstract";
        event.previous_phase = "unregistered";
        event.new_phase = "candidate_ready";
        event.reason = "overmap_special=cannibal_camp abstract_roster=" +
                       std::to_string( site.living_total ) +
                       " concrete_actors=0 target_knowledge=0 dispatch=0 contact=0";
        event.at_minutes = event.game_minutes;
        bandit_live_world_probe::record_live_transition_event( std::move( event ) );
    }

    if( result.created_sites > 0 ) {
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world abstract_bootstrap created_sites="
                                   << result.created_sites << " recognized_tiles=" << result.recognized_tiles
                                   << " scan_radius_omt=" << live_bandit_system_envelope_omt
                                   << " total_sites=" << state.sites.size()
                                   << " reconciled_persisted_members=" << reconciled_persisted_members << '\n';
    }
    return result.created_sites;
}

int attract_live_hordes_from_light_observations(
    const std::vector<live_bandit_signal_observation> &signals )
{
    int signaled_sources = 0;
    for( const live_bandit_signal_observation &signal : signals ) {
        if( signal.horde_signal_power <= 0 ) {
            continue;
        }
        const tripoint_abs_ms source_ms = signal.source_ms.value_or(
                                            coords::project_to<coords::ms>( signal.source_omt ) );
        int candidates = 0;
        const bool source_exposed = signal.light_projection.packet.exposure !=
                                    bandit_mark_generation::light_exposure_band::contained;
        const int attracted = overmap_buffer.attract_hordes_to_light( source_ms,
                              signal.horde_signal_power, signal.horde_signal_power,
                              signal.range_cap_omt, source_exposed, signal.sample_id,
                              calendar::turn, &candidates );
        signaled_sources += attracted > 0 ? 1 : 0;
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world horde light interest: packet="
                                   << signal.mark.mark_id << " kind=" << signal.mark.kind
                                   << " source_omt=" << signal.source_omt.to_string()
                                   << " source_ms=" << source_ms.to_string()
                                   << " current_light=yes"
                                   << " light_interest=" << signal.horde_signal_power
                                   << " source_envelope_sm=" << signal.horde_signal_power
                                   << " candidate_entities=" << candidates
                                   << " attracted_entities=" << attracted
                                   << " range_cap_omt=" << signal.range_cap_omt
                                   << " weather=" << signal.weather_summary << '\n';
    }
    return signaled_sources;
}

int signal_live_writhing_stalkers_from_light_observations(
    const std::vector<live_bandit_signal_observation> &signals )
{
    map &here = get_map();
    int observations = 0;
    for( const live_bandit_signal_observation &signal : signals ) {
        if( !signal.has_light_projection || signal.sample_id.empty() || signal.observed_turn < 0 ) {
            continue;
        }
        const tripoint_abs_ms source = signal.source_ms.value_or(
                                            coords::project_to<coords::ms>( signal.source_omt ) );
        const tripoint_bub_ms source_bub = here.get_bub( source );
        if( !here.inbounds( source_bub ) ) {
            continue;
        }
        const int duration = std::clamp( 90 + signal.horde_signal_power * 4, 90, 300 );
        for( monster &critter : g->all_monsters() ) {
            if( critter.type->id != mon_writhing_stalker || critter.is_dead() || critter.friendly != 0 ||
                !critter.sees( here, source_bub ) ) {
                continue;
            }
            writhing_stalker::persistent_state &state = critter.writhing_stalker_state();
            if( state.observe_light_interest( source, signal.sample_id, signal.observed_turn, duration ) ) {
                observations++;
            }
        }
    }
    return observations;
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
        const int durable_band_members = overmap_buffer.global_state.zombie_rider_bands.living_members(
                                            rider_id );
        pressure_input.rider_count = durable_band_members;
        pressure_input.band_formed = durable_band_members >=
                                    zombie_rider_overmap_ai::rider_band_minimum_size;
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
        // This ID is allocated with the biological actor and remains valid
        // through unload/reload.  A coordinate is evidence of an encounter,
        // never an identity or a band membership key.
        rider.rider_id = critter.predator_state().actor_id;
        rider.pos = critter.pos_abs_omt();
        rider.available = true;
        const std::optional<zombie_rider_overmap_ai::rider_camp_pressure_intent> existing_intent =
            zombie_rider_overmap_ai::get_camp_pressure_intent( critter );
        rider.already_in_band = existing_intent.has_value() ||
                                overmap_buffer.global_state.zombie_rider_bands.living_members(
                                    rider.rider_id ) >= zombie_rider_overmap_ai::rider_band_minimum_size;
        rider.cooldown_turns = critter.has_effect( effect_run ) ? 1 : 0;
        riders.push_back( rider );
        live_riders_by_id.emplace( rider.rider_id, &critter );
        overmap_buffer.global_state.zombie_rider_bands.reconcile_cache( rider.rider_id,
                critter.predator_state() );
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
        const bool fresh_detection = zombie_rider_overmap_ai::refresh_light_memory(
                                        memory, interest, signal.sample_id, signal.observed_turn );
        // A caller can safely replay its cached packet while doing ordinary
        // scheduling.  Only the real source sample above may change this
        // memory's observation time or finite expiry.
        if( !fresh_detection ) {
            continue;
        }
        const zombie_rider_overmap_ai::rider_convergence_result convergence =
            zombie_rider_overmap_ai::evaluate_rider_convergence( memory, signal.source_omt, riders );
        const tripoint_abs_ms light_source = signal.source_ms.value_or(
                                                coords::project_to<coords::ms>( signal.source_omt ) );
        const live_zombie_rider_pressure_summary pressure = command_live_zombie_riders_to_light(
                    convergence, live_riders_by_id, light_source, memory.turns_remaining );
        // Reservation only prevents this finite light sample from issuing two
        // commands.  It must not become a durable membership write.
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

bool live_bandit_overmap_smoke_los_from( const tripoint_abs_omt &origin,
                                        const live_bandit_signal_observation &smoke )
{
    if( smoke.source_omt.z() == origin.z() ) {
        return live_bandit_overmap_los_from( origin, smoke.source_omt, smoke.range_cap_omt );
    }
    // An open-air plume can be viewed across levels in either direction.
    // The generic terrain/actor LOS remains same-level; this smoke ray still
    // pays every intervening terrain cost.
    if( !smoke.smoke_source_exposed_to_sky || smoke.range_cap_omt <= 0 ||
        rl_dist( origin.xy(), smoke.source_omt.xy() ) > smoke.range_cap_omt ) {
        return false;
    }
    std::vector<int> terrain_see_costs;
    for( const tripoint_abs_omt &omt : line_to( origin, smoke.source_omt ) ) {
        terrain_see_costs.push_back(
            static_cast<int>( overmap_buffer.ter( omt )->get_see_cost() ) );
    }
    return bandit_live_world::structural_observer_route_is_visible(
               smoke.range_cap_omt, terrain_see_costs );
}

bool live_bandit_overmap_light_los_from( const tripoint_abs_omt &origin,
        const live_bandit_signal_observation &signal )
{
    if( !signal.has_light_projection ) {
        return false;
    }
    const bandit_mark_generation::light_packet &packet = signal.light_projection.packet;
    const point_rel_omt offset = signal.source_omt.xy() - origin.xy();
    const int brightness_range = signal.range_cap_omt;
    if( brightness_range <= 0 || offset.x() < -brightness_range ||
        offset.x() > brightness_range || offset.y() < -brightness_range ||
        offset.y() > brightness_range ) {
        return false;
    }
    std::vector<int> terrain_see_costs;
    // This reads static overmap terrain only.  It intentionally does not call
    // map::build_seen_cache or share an NPC/avatar recognition cache.
    for( const tripoint_abs_omt &omt : line_to( origin, signal.source_omt ) ) {
        terrain_see_costs.push_back( static_cast<int>( overmap_buffer.ter( omt )->get_see_cost() ) );
    }
    physical_light::route route;
    route.distance_omt = rl_dist( origin, signal.source_omt );
    route.vertical_offset = signal.source_omt.z() - origin.z();
    route.source_exposed = packet.exposure != bandit_mark_generation::light_exposure_band::contained;
    // A source that already escaped through a local aperture can be seen
    // across levels.  A sealed upper floor cannot manufacture this flag.
    route.vertical_sightline = route.vertical_offset == 0 || route.source_exposed;
    route.brightness_range_omt = brightness_range;
    route.terrain_see_costs = std::move( terrain_see_costs );
    const physical_light::detection detection = physical_light::detect( route );
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world light_los"
                               << " origin=" << origin
                               << " source=" << signal.source_omt
                               << " distance=" << route.distance_omt
                               << " vertical=" << route.vertical_offset
                               << " exposed=" << ( route.source_exposed ? "yes" : "no" )
                               << " vertical_sightline=" << ( route.vertical_sightline ? "yes" : "no" )
                               << " range=" << route.brightness_range_omt
                               << " terrain_costs=";
    for( const int see_cost : route.terrain_see_costs ) {
        DebugLog( D_INFO, DC_ALL ) << see_cost << ',';
    }
    DebugLog( D_INFO, DC_ALL ) << " result=" << ( detection.visible ? "visible" : "blocked" )
                               << " remaining=" << detection.remaining_brightness << '\n';
    return detection.visible;
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
    const bool has_optic = bandit_live_world::live_structural_observer_has_optic( observer );
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
    const int route_budget = bandit_live_world::hostile_camp_routine_route_budget_omt(
                                 site, live_bandit_current_minutes() );
    const tripoint_abs_omt route_target = plan.shared_route.size() >= 3 ?
                                          plan.shared_route[plan.shared_route.size() - 2] :
                                          plan.target_omt;
    const int source_node_cost = npc_route.get_cost(
                                     overmap_buffer.ter_existing( site.anchor )->get_travel_cost_type() );
    const std::optional<tripoint_abs_omt> physical_watch_target =
        bandit_live_world::structural_watch_physical_target( site, plan );
    const bool watch_required = physical_watch_target.has_value();
    if( !watch_required ) {
        const auto path = overmap_buffer.get_travel_path(
                              site.anchor, route_target, npc_route );
        if( path.points.empty() || path.cost < 0 ) {
            read.summary = "live structural route solve found no passable route";
            return read;
        }
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
        read.reachable = read.complete_route_cost <= route_budget;
        read.summary = string_format(
                           "live structural target route %s raw_cost=%d source_cost=%d diagonal=%s round_trip_omt=%d budget=%d",
                           read.reachable ? "accepted" : "exceeded complete-route cap", path.cost,
                           source_node_cost, diagonal_departure ? "yes" : "no",
                           read.complete_route_cost, route_budget );
        if( route_target != plan.target_omt ) {
            read.summary += "; elevated uncertain source uses physical approach waypoint " +
                            route_target.to_string();
        }
        return read;
    }
    read.summary = "live structural route evaluated against watch endpoint";

    std::vector<tripoint_abs_omt> target_footprint = { *physical_watch_target };
    if( plan.target_omt.z() == site.anchor.z() ) {
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
    }

    const std::unordered_set<tripoint_abs_omt> target_footprint_exclusions(
        target_footprint.begin(), target_footprint.end() );
    std::vector<std::pair<tripoint_abs_omt, std::vector<tripoint_abs_omt>>> watch_paths;
    // Route scoring precedes materialization and exact member assignment. This
    // prospective human optical model only scores physical lookout geometry;
    // it never publishes an observation. Actual acquisition rechecks each assigned NPC.
    npc optical_reference;
    optical_reference.set_fake( true );
    optical_reference.normalize();
    optical_reference.recalc_sight_limits();
    scout_observation::site_reader optical_geometry;
    const auto terrain_lookup = [&optical_reference, &optical_geometry]( const tripoint_abs_omt & candidate,
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
        terrain.intervening_omts_clear = true;
        bool checked = false;
        bool useful = false;
        {
            const tripoint_abs_ms origin = project_to<coords::ms>( candidate ) + tripoint_rel_ms( SEEX, SEEY,
                                           0 );
            const tripoint_abs_ms target = project_to<coords::ms>( *nearest );
            if( scout_observation::associated_with_site( get_avatar().pos_abs_omt(), footprint ) ) {
                const auto view = optical_geometry.read( optical_reference, origin,
                                  get_avatar().pos_abs(), footprint );
                checked = view.known;
                useful = view.visible;
            }
            int actor_reads = 0;
            for( const npc &actor : g->all_npcs() ) {
                if( useful || actor_reads >= bandit_live_world::covert_visible_defender_read_cap() ) {
                    break;
                }
                if( actor.is_dead() || !actor.is_active() ||
                    !scout_observation::associated_with_site( actor.pos_abs_omt(), footprint ) ) {
                    continue;
                }
                ++actor_reads;
                const auto view = optical_geometry.read( optical_reference, origin, actor.pos_abs(), footprint );
                checked = checked || view.known;
                useful = useful || view.visible;
            }
            for( const point &offset : {
                     point( 0, 0 ), point( 23, 0 ), point( 0, 23 ), point( 23, 23 )
                 } ) {
                if( useful ) {
                    break;
                }
                const auto view = optical_geometry.read( optical_reference, origin,
                                  target + tripoint_rel_ms( offset.x, offset.y, 0 ), footprint );
                checked = checked || view.known;
                useful = useful || view.visible;
                if( useful ) {
                    break;
                }
            }
        }
        // Unavailable local geometry is not an affirmative watch view.
        terrain.intervening_omts_clear = terrain.intervening_omts_clear && checked && useful;
        return terrain;
    };
    const auto watch_route_lookup = [&site, &npc_route, &source_node_cost, &target_footprint,
                                    &target_footprint_exclusions,
                                    &watch_path_budget, route_budget,
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
        route.reachable = route.route_cost >= 0 && route.route_cost <= route_budget;
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
        read.max_segment_risk = 0;
        for( const tripoint_abs_omt &omt : selected_path->second ) {
            const std::string terrain_id = live_bandit_structural_terrain_id( omt ).value_or(
                                               std::string() );
            const bandit_live_world::structural_bounty_read terrain =
                bandit_live_world::classify_structural_bounty_terrain( terrain_id );
            read.max_segment_risk = std::max( read.max_segment_risk,
                                             bandit_live_world::structural_terrain_static_risk(
                                                 terrain.terrain_fit_class ) );
        }
        read.reachable = true;
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

std::optional<bandit_live_world::canonical_hostile_operation_route>
live_bandit_hostile_operation_route_read( const bandit_live_world::site_record &site )
{
    const tripoint_abs_omt reported = site.camp_decision.target_omt;
    // The report names the physical source; the overmap journey ends at a
    // reachable entrance/footprint.  Prefer its own horizontal tile and the
    // ordinary ground entrance, then bounded adjacent access if the tile is
    // impassable.  Neither the source z nor the camp's starting z is flattened.
    static constexpr std::array<point, 9> offsets = {
        point( 0, 0 ), point( 0, -1 ), point( 1, 0 ), point( 0, 1 ),
        point( -1, 0 ), point( 1, -1 ), point( 1, 1 ),
        point( -1, 1 ), point( -1, -1 )
    };
    // Try every site level between the ordinary entrance, the party's real
    // starting level and the reported source.  A high source may have its
    // first reachable doorway below the source floor.
    std::vector<int> heights = { 0 };
    if( site.anchor.z() != 0 ) {
        heights.push_back( site.anchor.z() );
    }
    const int low = std::min( { 0, site.anchor.z(), reported.z() } );
    const int high = std::max( { 0, site.anchor.z(), reported.z() } );
    for( int height = low; height <= high; ++height ) {
        if( std::find( heights.begin(), heights.end(), height ) == heights.end() ) {
            heights.push_back( height );
        }
    }
    const overmap_path_params params = overmap_path_params::for_npc();
    for( const int height : heights ) {
        for( const bool exact_footprint : { true, false } ) {
            std::optional<bandit_live_world::canonical_hostile_operation_route> best;
            int best_cost = std::numeric_limits<int>::max();
            for( const point &offset : offsets ) {
                if( ( offset == point::zero ) != exact_footprint ) {
                    continue;
                }
                const tripoint_abs_omt approach( reported.x() + offset.x,
                                                 reported.y() + offset.y, height );
                const auto path = overmap_buffer.get_travel_path( site.anchor, approach, params );
                if( path.cost < 0 || path.cost >= best_cost ) {
                    continue;
                }
                auto canonical = bandit_live_world::canonicalize_hostile_operation_route(
                                     path.points, site.anchor, approach, reported );
                if( canonical ) {
                    best_cost = path.cost;
                    best = std::move( canonical );
                }
            }
            if( best ) {
                return best;
            }
        }
    }
    return std::nullopt;
}

bool live_bandit_player_opportunity_route_available(
    const bandit_live_world::site_record &site,
    const bandit_live_world::hostile_target_opportunity_record &opportunity )
{
    const auto path = overmap_buffer.get_travel_path( site.anchor, opportunity.target_omt,
                      overmap_path_params::for_npc() );
    return path.cost >= 0 && path.points.size() >= 2 &&
           path.points.front() == opportunity.target_omt && path.points.back() == site.anchor;
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
        std::ostringstream corridor;
        for( std::size_t index = 0; index < read.watch_shared_route.size(); ++index ) {
            if( index > 0 ) {
                corridor << '>';
            }
            corridor << read.watch_shared_route[index].to_string();
        }
        selected_fields = " watch=" + selected_watch->omt.to_string() +
                          " route_cost=" + std::to_string( selected_watch->route_cost ) +
                          " corridor=" + corridor.str();
    }
    return "bandit_live_world structural_route_analyzer site=" + site.site_id +
           " lead=" + plan.lead_id + " target=" + plan.target_omt.to_string() +
           " selector=" + selector + " outcome=" + ( selected ? "selected" : "rejected" ) +
           selected_fields + " summary=" + read.summary;
}

std::string live_bandit_structural_signal_request_diagnostic(
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request )
{
    // The footprint renderer is bounded by the authoritative canonical footprint cap.
    constexpr std::size_t max_rendered_omts = 64;
    const auto render_omts = [max_rendered_omts]( const std::vector<tripoint_abs_omt> &omts ) {
        std::ostringstream rendered;
        rendered << '[';
        const std::size_t rendered_count = std::min( omts.size(), max_rendered_omts );
        for( std::size_t index = 0; index < rendered_count; ++index ) {
            if( index > 0 ) {
                rendered << ',';
            }
            rendered << omts[index].to_string();
        }
        if( rendered_count < omts.size() ) {
            if( rendered_count > 0 ) {
                rendered << ',';
            }
            rendered << "...";
        }
        rendered << ']';
        return rendered.str();
    };
    std::ostringstream diagnostic;
    diagnostic << " selected_watch_kind=" << bandit_live_world::to_string( outing.selected_watch_kind )
               << " selected_watch_omt=" << outing.selected_watch_omt.to_string()
               << " waypoint_index=" << outing.waypoint_index
               << " phase=" << bandit_live_world::to_string( outing.phase )
               << " target_footprint_count=" << outing.target_footprint.size()
               << " target_footprint=" << render_omts( outing.target_footprint )
               << " visible_forward_omts_count=" << request.visible_forward_omts.size()
               << " visible_forward_omts=" << render_omts( request.visible_forward_omts );
    return diagnostic.str();
}

std::vector<bandit_live_world::structural_signal_read> live_bandit_structural_signal_reads(
    const std::vector<live_bandit_signal_observation> &signals,
    const std::vector<live_bandit_sound_observation> &sound_events,
    const bandit_live_world::site_record &site,
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request )
{
    std::vector<bandit_live_world::structural_signal_read> result;
    const bool request_current_inbounds = get_map().inbounds( request.current_omt );
    // This opt-in explanation belongs to this acquisition only. It retains
    // results from the actual predicates/readers below, never rereads geometry.
    const bool diagnostic_watch = openclaw_harness_bandit_owner_trace_enabled() &&
                                  outing.kind == bandit_live_world::outing_kind::structural_sortie &&
                                  outing.phase == bandit_live_world::scout_phase::observing;
    std::vector<std::pair<character_id, std::string>> watcher_diagnostics;
    std::vector<std::string> candidate_rejections( diagnostic_watch ? signals.size() : 0 );
    std::vector<std::vector<std::pair<character_id, scout_observation::visibility_read>>>
            candidate_readers( diagnostic_watch ? signals.size() : 0 );
    std::string request_rejection;
    bool watcher_eligibility_evaluated = false;
    on_out_of_scope log_request( [&] {
        // Retain the existing compact request row. Empty opt-in watches also
        // report zero candidates, rather than making missing emission ambiguous.
        if( signals.empty() && sound_events.empty() && !diagnostic_watch ) {
            return;
        }
        std::ostringstream detail;
        if( diagnostic_watch ) {
            JsonOut json( detail );
            json.start_object();
            const char *run = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
            json.member( "run_id", run && run[0] ? std::optional<std::string>( run ) : std::nullopt );
            json.member( "game_turn", to_turns<int>( calendar::turn - calendar::turn_zero ) );
            json.member( "service_minutes", live_bandit_current_minutes() );
            json.member( "site_id", site.site_id );
            json.member( "activity_id", outing.activity_id );
            json.member( "generation", outing.generation );
            json.member( "target_lead_id", outing.target_lead_id );
            json.member( "target_lead_revision", outing.target_lead_revision );
            json.member( "assessment_pinned_revision", outing.assessment.pinned_target_revision );
            json.member( "request_rejection", request_rejection );
            json.member( "watcher_eligibility_evaluated", watcher_eligibility_evaluated );
            json.member( "watcher_object_lookup", "overmap_buffer.find_npc" );
            json.member( "watchers" );
            json.start_array();
            for( const auto &entry : watcher_diagnostics ) {
                json.start_object();
                json.member( "member_id", entry.first.get_value() );
                json.member( "eligible", entry.second == "eligible" );
                json.member( "reason", entry.second );
                json.end_object();
            }
            json.end_array();
            json.member( "candidates" );
            json.start_array();
            for( size_t index = 0; index < signals.size(); ++index ) {
                const auto &signal = signals[index];
                json.start_object();
                json.member( "source_id", signal.sample_id.empty() ? signal.mark.mark_id : signal.sample_id );
                json.member( "source_ms", signal.source_ms );
                json.member( "source_omt", signal.source_omt );
                json.member( "channel", signal.mark.kind );
                json.member( "observed_turn", signal.observed_turn );
                json.member( "observed_minutes", signal.observed_minutes );
                json.member( "range_cap_omt", signal.range_cap_omt );
                json.member( "range_omt", signal.mark.kind == "smoke" ?
                             rl_dist( request.current_omt.xy(), signal.source_omt.xy() ) :
                             rl_dist( request.current_omt, signal.source_omt ) );
                json.member( "associated_with_target_site", scout_observation::associated_with_site(
                                 signal.source_omt, outing.target_footprint ) );
                json.member( "smoke_exposed_to_sky", signal.smoke_source_exposed_to_sky );
                json.member( "light_exposure", signal.has_light_projection ?
                             std::optional<std::string>( bandit_mark_generation::to_string(
                                     signal.light_projection.packet.exposure ) ) : std::nullopt );
                json.member( "adapter_rejection", request_rejection.empty() ?
                             candidate_rejections[index] : request_rejection );
                json.member( "reader_evaluated", !candidate_readers[index].empty() );
                json.member( "readers" );
                json.start_array();
                for( const auto &entry : candidate_readers[index] ) {
                    json.start_object();
                    json.member( "member_id", entry.first.get_value() );
                    json.member( "known", entry.second.known );
                    json.member( "visible", entry.second.visible );
                    json.member( "reason", entry.second.reason );
                    json.end_object();
                }
                json.end_array();
                json.end_object();
            }
            json.end_array();
            json.end_object();
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world signal_adapter request"
                                   << " current_omt=" << request.current_omt
                                   << " map_origin_omt="
                                   << coords::project_to<coords::omt>( get_map().get_abs_sub() )
                                   << " current_inbounds=" << ( request_current_inbounds ? "yes" : "no" )
                                   << " field_signals=" << signals.size()
                                   << " sound_events=" << sound_events.size()
                                   << live_bandit_structural_signal_request_diagnostic( outing, request )
                                   << ( diagnostic_watch ? " diagnostic=" + detail.str() : "" ) << '\n';
    } );
    const bool local_observer = outing.owner == bandit_live_world::simulation_owner::local &&
                                outing.phase == bandit_live_world::scout_phase::observing &&
                                outing.local_handoff.is_active() &&
                                outing.local_handoff.cohesion_assembled &&
                                !outing.local_handoff.cohesion_abort_return &&
                                outing.local_handoff.route_position == request.current_omt;
    const bool exact_site_watch = outing.schema_version >= 8 &&
                            outing.phase == bandit_live_world::scout_phase::observing &&
                            outing.selected_watch_kind == bandit_live_world::structural_watch_kind::exact &&
                            bandit_live_world::target_footprint_watch_distance(
                                outing.selected_watch_omt, outing.target_footprint ) == 3 &&
                            outing.selected_watch_omt == request.current_omt;
    if( outing.kind != bandit_live_world::outing_kind::structural_sortie ) {
        request_rejection = "wrong_outing_kind";
    } else if( outing.owner != bandit_live_world::simulation_owner::abstract && !local_observer ) {
        request_rejection = "owner_not_available";
    } else if( request.party_power <= 0 ) {
        request_rejection = "no_party_power";
    } else if( request.visible_forward_omts.size() > 3 ) {
        request_rejection = "forward_footprint_limit";
    } else if( request_current_inbounds && !local_observer && !exact_site_watch ) {
        // An established exact watch checks its capable members below; ordinary
        // abstract travel cannot read a loaded local projection.
        request_rejection = "loaded_abstract_travel";
    }
    if( !request_rejection.empty() ) {
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
    scout_observation::site_reader optical_geometry;
    std::vector<shared_ptr_fast<npc>> watchers;
    if( exact_site_watch ) {
        watcher_eligibility_evaluated = true;
        for( const auto id : outing.member_ids ) {
            const auto member = overmap_buffer.find_npc( id );
            std::string reason;
            const bool eligible = member && optical_geometry.watching_member( site, *member,
                                  diagnostic_watch ? &reason : nullptr );
            if( diagnostic_watch ) {
                watcher_diagnostics.emplace_back( id, member ? reason : "actor_unavailable" );
            }
            if( eligible ) {
                watchers.push_back( member );
            }
        }
        std::sort( watchers.begin(), watchers.end(), [&outing]( const auto &lhs, const auto &rhs ) {
            return std::make_tuple( lhs->getID() != outing.leader_id, lhs->getID().get_value() ) <
                   std::make_tuple( rhs->getID() != outing.leader_id, rhs->getID().get_value() );
        } );
    } else if( !observer || observer->is_dead() || observer->is_blind() ) {
        return result;
    }
    const bool site_watch = exact_site_watch && !watchers.empty();
    const auto signal_distance = [&request]( const live_bandit_signal_observation &signal ) {
        return signal.mark.kind == "smoke" ?
               rl_dist( request.current_omt.xy(), signal.source_omt.xy() ) :
               rl_dist( request.current_omt, signal.source_omt );
    };
    struct optical_candidate {
        const live_bandit_signal_observation *signal = nullptr;
        character_id observer_id;
        bool physical_pair_can_share = false;
    };
    std::vector<optical_candidate> candidates;
    for( size_t signal_index = 0; signal_index < signals.size(); ++signal_index ) {
        const auto &signal = signals[signal_index];
        const auto refuse_candidate = [&]( const char *reason ) {
            if( diagnostic_watch ) {
                candidate_rejections[signal_index] = reason;
            }
        };
        // An interrupted exact watch cannot acquire optical evidence through
        // the ordinary travel/legacy fallback. No optical mode is persisted.
        if( exact_site_watch && !site_watch ) {
            refuse_candidate( "no_eligible_watcher" );
            continue;
        }
        const bool supported_kind = signal.mark.kind == "smoke" ||
                                    signal.mark.kind == "light" ||
                                    signal.mark.kind == "searchlight";
        const bool smoke_signal = signal.mark.kind == "smoke";
        const bool light_signal = signal.mark.kind == "light" || signal.mark.kind == "searchlight";
        const tripoint_abs_omt source_at_scout_level( signal.source_omt.x(),
                signal.source_omt.y(), request.current_omt.z() );
        if( site_watch ) {
            const char *rejection = nullptr;
            if( !supported_kind ) {
                rejection = "unsupported_channel";
            } else if( !signal.source_ms ) {
                rejection = "source_ms_unavailable";
            } else if( project_to<coords::omt>( *signal.source_ms ) != signal.source_omt ) {
                rejection = "source_omt_mismatch";
            } else if( signal.range_cap_omt <= 0 || signal.range_cap_omt > 40 ) {
                rejection = "invalid_range_cap";
            } else if( signal_distance( signal ) > signal.range_cap_omt ) {
                rejection = "outside_packet_range";
            } else if( !scout_observation::associated_with_site( signal.source_omt, outing.target_footprint ) ) {
                rejection = "outside_target_site";
            }
            if( rejection ) {
                refuse_candidate( rejection );
                continue;
            }
        }
        std::vector<character_id> actual_observers;
        bool physical_pair_can_share = false;
        bool line_of_sight = true;
        if( site_watch ) {
            if( light_signal && ( !signal.has_light_projection ||
                signal.light_projection.packet.exposure == bandit_mark_generation::light_exposure_band::contained ) ) {
                line_of_sight = false;
                refuse_candidate( "light_projection_not_observable" );
            } else if( smoke_signal && signal.source_omt.z() != request.current_omt.z() &&
                       !signal.smoke_source_exposed_to_sky ) {
                line_of_sight = false;
                refuse_candidate( "cross_level_smoke_not_exposed" );
            }
        } else {
            line_of_sight = signal.source_omt == request.current_omt ||
                            ( light_signal ? live_bandit_overmap_light_los_from( request.current_omt, signal ) :
                              live_bandit_overmap_smoke_los_from( request.current_omt, signal ) );
        }
        if( site_watch && line_of_sight ) {
            size_t visible_watchers = 0;
            for( const auto &member : watchers ) {
                const auto read = optical_geometry.read_signal( *member, member->pos_abs(),
                                  *signal.source_ms, outing.target_footprint );
                if( diagnostic_watch ) {
                    candidate_readers[signal_index].emplace_back( member->getID(), read );
                }
                if( read.visible ) {
                    actual_observers.push_back( member->getID() );
                    ++visible_watchers;
                }
            }
            line_of_sight = !actual_observers.empty();
            physical_pair_can_share = visible_watchers == 2 && watchers.size() == 2 &&
                                      rl_dist( watchers[0]->pos_abs(), watchers[1]->pos_abs() ) <= 6;
        }
        if( site_watch && !line_of_sight && diagnostic_watch &&
            !candidate_readers[signal_index].empty() ) {
            // Preserve each actual first reader refusal; pre-reader projection
            // refusals have no invented optical result.
            refuse_candidate( "no_visible_watcher" );
        }
        // Preserve the source's real height.  Only a physically visible
        // cross-level signal above a permitted ground route tile can use its
        // projected footprint for route eligibility.
        const bool cross_level_signal_over_route = ( smoke_signal || light_signal ) &&
                signal.source_omt.z() != request.current_omt.z() && line_of_sight &&
                ( source_at_scout_level == request.current_omt ||
                  std::find( request.visible_forward_omts.begin(),
                             request.visible_forward_omts.end(), source_at_scout_level ) !=
                  request.visible_forward_omts.end() );
        const bool source_is_permitted = ( site_watch &&
                scout_observation::associated_with_site( signal.source_omt, outing.target_footprint ) ) ||
                                         signal.source_omt == request.current_omt ||
                                         std::find( request.visible_forward_omts.begin(),
                                                 request.visible_forward_omts.end(),
                                                 signal.source_omt ) !=
                                         request.visible_forward_omts.end() ||
                                         cross_level_signal_over_route;
        const int scout_range = signal_distance( signal );
        if( supported_kind && source_is_permitted && signal.range_cap_omt > 0 &&
            signal.range_cap_omt <= 40 && scout_range <= signal.range_cap_omt &&
            line_of_sight ) {
            if( site_watch && !physical_pair_can_share ) {
                for( const auto id : actual_observers ) {
                    candidates.push_back( { &signal, id, false } );
                }
            } else {
                candidates.push_back( { &signal, actual_observers.empty() ? character_id() :
                                        actual_observers.front(), physical_pair_can_share } );
            }
        }
    }
    std::sort( candidates.begin(), candidates.end(), [&signal_distance](
    const optical_candidate &left, const optical_candidate &right ) {
        const auto *lhs = left.signal;
        const auto *rhs = right.signal;
        return std::make_tuple( signal_distance( *lhs ),
                                lhs->source_omt.z(), lhs->source_omt.y(), lhs->source_omt.x(),
                                lhs->mark.kind, lhs->mark.mark_id, left.observer_id.get_value() ) <
               std::make_tuple( signal_distance( *rhs ),
                                rhs->source_omt.z(), rhs->source_omt.y(), rhs->source_omt.x(),
                                rhs->mark.kind, rhs->mark.mark_id, right.observer_id.get_value() );
    } );

    for( const bool select_smoke : { true, false } ) {
        const auto found = std::find_if( candidates.begin(), candidates.end(), [select_smoke](
        const optical_candidate &candidate ) {
            return ( candidate.signal->mark.kind == "smoke" ) == select_smoke;
        } );
        if( found == candidates.end() ) {
            continue;
        }
        // Shared sight needs one receipt; separated witnesses each keep their
        // actual private receipt for this selected source and channel.
        for( const auto &candidate : candidates ) {
            if( candidate.signal != found->signal ) {
                continue;
            }
            const live_bandit_signal_observation &signal = *found->signal;
            const int scout_range = signal_distance( signal );
            bandit_live_world::structural_signal_read read;
            read.sense = select_smoke ? bandit_live_world::sortie_observation_sense::smoke :
                         bandit_live_world::sortie_observation_sense::light;
            read.source_omt = signal.source_omt;
            read.observer_id = candidate.observer_id;
            read.physical_pair_can_share = candidate.physical_pair_can_share;
            read.source_id = signal.sample_id.empty() ? signal.mark.mark_id : signal.sample_id;
            read.observed_minutes = signal.observed_minutes;
            read.range_cap_omt = signal.range_cap_omt;
            read.strength = std::clamp( signal.mark.strength, 1, 6 );
            read.confidence = std::clamp( 40 + 10 * signal.mark.confidence, 0, 60 );
            read.observer_sight_points = signal.range_cap_omt;
            read.line_of_sight = true;
            read.uncertainty_radius_omt = std::clamp( std::max( 1, ( scout_range + 2 ) / 3 ) +
                                                    ( select_smoke ? 1 : 0 ), 1, 40 );
            read.summary = signal.weather_summary;
            result.push_back( std::move( read ) );
        }
    }

    // Ordinary hearing retains its original leader basis, independent of which
    // capable member made the site-directed optical acquisition.
    if( !observer || observer->is_dead() || observer->is_blind() ) {
        return result;
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

std::vector<bandit_live_world::structural_signal_read> live_bandit_staffed_camp_signal_reads(
    const std::vector<live_bandit_signal_observation> &signals,
    const std::vector<live_bandit_sound_observation> &sound_events,
    const bandit_live_world::site_record &site,
    const bandit_live_world::camp_signal_observer_request &request )
{
    std::vector<bandit_live_world::structural_signal_read> result;
    if( request.camp_omt != site.anchor && !site_contains_omt( site, request.camp_omt ) ) {
        return result;
    }
    const npc *observer = live_bandit_staffed_observer( request.observer_id );
    if( observer == nullptr || observer->is_dead() || observer->pos_abs_omt() != request.camp_omt ) {
        return result;
    }
    const int sight_points = live_bandit_structural_observer_sight( *observer,
                             request.camp_omt ).sight_points;
    const auto signal_distance = [&request]( const live_bandit_signal_observation &signal ) {
        return signal.mark.kind == "smoke" ?
               rl_dist( request.camp_omt.xy(), signal.source_omt.xy() ) :
               rl_dist( request.camp_omt, signal.source_omt );
    };
    for( const bool select_smoke : { true, false } ) {
        if( !live_bandit_can_make_ordinary_visual_observation( *observer ) ) {
            const auto source = std::find_if( signals.begin(), signals.end(),
            [select_smoke]( const live_bandit_signal_observation &signal ) {
                return select_smoke ? signal.mark.kind == "smoke" :
                       signal.mark.kind == "light" || signal.mark.kind == "searchlight";
            } );
            if( source != signals.end() ) {
                bandit_live_world::structural_signal_read read;
                read.sense = select_smoke ? bandit_live_world::sortie_observation_sense::smoke :
                             bandit_live_world::sortie_observation_sense::light;
                read.source_omt = source->source_omt;
                read.range_cap_omt = source->range_cap_omt;
                read.rejected = true;
                read.rejection_reason = "home_sensor_no_visual_senses";
                result.push_back( std::move( read ) );
            }
            continue;
        }
        const auto found = std::find_if( signals.begin(), signals.end(),
        [&request, &signal_distance, select_smoke]( const live_bandit_signal_observation & signal ) {
            const bool smoke = signal.mark.kind == "smoke";
            const bool light = signal.mark.kind == "light" || signal.mark.kind == "searchlight";
            const bool line_of_sight = light ?
                                       live_bandit_overmap_light_los_from( request.camp_omt, signal ) :
                                       live_bandit_overmap_smoke_los_from( request.camp_omt, signal );
            return ( select_smoke ? smoke : light ) && signal.range_cap_omt >= 1 &&
                   signal.range_cap_omt <= 40 &&
                   signal_distance( signal ) <= signal.range_cap_omt &&
                   line_of_sight;
        } );
        if( found == signals.end() ) {
            const auto blocked = std::find_if( signals.begin(), signals.end(),
            [&signal_distance, select_smoke]( const live_bandit_signal_observation & signal ) {
                const bool smoke = signal.mark.kind == "smoke";
                const bool light = signal.mark.kind == "light" || signal.mark.kind == "searchlight";
                return ( select_smoke ? smoke : light ) && signal.range_cap_omt >= 1 &&
                       signal.range_cap_omt <= 40 &&
                       signal_distance( signal ) <= signal.range_cap_omt;
            } );
            if( blocked != signals.end() ) {
                bandit_live_world::structural_signal_read read;
                read.sense = select_smoke ? bandit_live_world::sortie_observation_sense::smoke :
                             bandit_live_world::sortie_observation_sense::light;
                read.source_omt = blocked->source_omt;
                read.range_cap_omt = blocked->range_cap_omt;
                read.observer_sight_points = sight_points;
                read.line_of_sight = false;
                read.rejected = true;
                read.rejection_reason = "blocked_line_of_sight";
                read.summary = blocked->weather_summary;
                result.push_back( std::move( read ) );
            }
            if( blocked != signals.end() ) {
                continue;
            }
            const auto out_of_range = std::min_element( signals.begin(), signals.end(),
            [&signal_distance, select_smoke]( const live_bandit_signal_observation & lhs,
            const live_bandit_signal_observation & rhs ) {
                const auto candidate_key = [&signal_distance, select_smoke](
                const live_bandit_signal_observation & signal ) {
                    const bool smoke = signal.mark.kind == "smoke";
                    const bool light = signal.mark.kind == "light" || signal.mark.kind == "searchlight";
                    const bool eligible_channel = select_smoke ? smoke : light;
                    const bool beyond_cap = eligible_channel && signal.range_cap_omt >= 1 &&
                                            signal.range_cap_omt <= 40 &&
                                            signal_distance( signal ) > signal.range_cap_omt;
                    return std::make_tuple( !beyond_cap, signal_distance( signal ),
                                            signal.source_omt.z(), signal.source_omt.y(), signal.source_omt.x() );
                };
                return candidate_key( lhs ) < candidate_key( rhs );
            } );
            if( out_of_range != signals.end() ) {
                const bool smoke = out_of_range->mark.kind == "smoke";
                const bool light = out_of_range->mark.kind == "light" ||
                                   out_of_range->mark.kind == "searchlight";
                const bool eligible_channel = select_smoke ? smoke : light;
                if( eligible_channel && out_of_range->range_cap_omt >= 1 &&
                    out_of_range->range_cap_omt <= 40 &&
                    signal_distance( *out_of_range ) > out_of_range->range_cap_omt ) {
                    bandit_live_world::structural_signal_read read;
                    read.sense = select_smoke ? bandit_live_world::sortie_observation_sense::smoke :
                                 bandit_live_world::sortie_observation_sense::light;
                    read.source_omt = out_of_range->source_omt;
                    read.range_cap_omt = out_of_range->range_cap_omt;
                    read.strength = std::clamp( out_of_range->mark.strength, 1, 6 );
                    read.observer_sight_points = sight_points;
                    read.line_of_sight = light ?
                                         live_bandit_overmap_light_los_from( request.camp_omt,
                                                 *out_of_range ) :
                                         live_bandit_overmap_smoke_los_from( request.camp_omt,
                                                 *out_of_range );
                    read.rejected = true;
                    read.rejection_reason = "out_of_range";
                    read.summary = out_of_range->weather_summary;
                    result.push_back( std::move( read ) );
                }
            }
            continue;
        }
        const int range = signal_distance( *found );
        bandit_live_world::structural_signal_read read;
        read.sense = select_smoke ? bandit_live_world::sortie_observation_sense::smoke :
                     bandit_live_world::sortie_observation_sense::light;
        read.source_omt = found->source_omt;
        // Preserve the immutable physical sample identity through the staffed
        // observer handoff.  Legacy callers may not provide one, so retain the
        // stable mark id as a bounded fallback.
        read.source_id = found->sample_id.empty() ? found->mark.mark_id : found->sample_id;
        read.observed_minutes = found->observed_minutes;
        read.range_cap_omt = found->range_cap_omt;
        read.observer_sight_points = sight_points;
        read.line_of_sight = true;
        read.strength = std::clamp( found->mark.strength, 1, 6 );
        read.confidence = std::clamp( 40 + 10 * found->mark.confidence, 0, 60 );
        read.uncertainty_radius_omt = std::clamp( std::max( 1, ( range + 2 ) / 3 ) +
                                                  ( select_smoke ? 1 : 0 ), 1, 40 );
        read.summary = found->weather_summary;
        result.push_back( std::move( read ) );
    }
    const int now_minutes = live_bandit_current_minutes();
    const int weather_attenuation = live_bandit_remote_weather_at( request.camp_omt )->sound_attn;
    const auto audible = std::find_if( sound_events.begin(), sound_events.end(),
    [&request, &observer, now_minutes, weather_attenuation]( const live_bandit_sound_observation & event ) {
        const bool supported = event.kind == sounds::significant_sound_t::gunfire ||
                               event.kind == sounds::significant_sound_t::alarm ||
                               event.kind == sounds::significant_sound_t::explosion;
        const int distance = rl_dist( request.camp_omt, event.source_omt );
        const int vertical = std::abs( request.camp_omt.z() - event.source_omt.z() );
        const int required = ( distance + 1 ) * 2 * SEEX - 1 + vertical * 10 * SEEX;
        const int effective = static_cast<int>( std::floor( std::max( 0, event.volume -
                                      weather_attenuation ) * observer->hearing_ability() ) );
        return supported && event.volume >= 24 && event.emitted_minutes >= 0 &&
               event.emitted_minutes <= now_minutes && now_minutes - event.emitted_minutes <= 180 &&
               !observer->is_deaf() && !observer->has_effect( effect_narcosis ) && effective >= required;
    } );
    if( audible != sound_events.end() ) {
        bandit_live_world::structural_signal_read read;
        read.sense = bandit_live_world::sortie_observation_sense::sound;
        read.sound_kind = audible->kind == sounds::significant_sound_t::gunfire ?
                          bandit_live_world::structural_sound_kind::gunfire :
                          audible->kind == sounds::significant_sound_t::alarm ?
                          bandit_live_world::structural_sound_kind::alarm :
                          bandit_live_world::structural_sound_kind::explosion;
        read.source_omt = audible->source_omt;
        read.emitted_minutes = audible->emitted_minutes;
        read.range_cap_omt = 40;
        read.observer_sight_points = static_cast<int>( observer->hearing_ability() * 100 );
        read.line_of_sight = true;
        read.strength = std::clamp( audible->volume / ( 2 * SEEX ), 1, 6 );
        read.confidence = 2;
        read.uncertainty_radius_omt = std::clamp( 1 + rl_dist( request.camp_omt,
                                                   audible->source_omt ) / 2, 1, 40 );
        read.summary = "significant sound heard from staffed camp";
        result.push_back( std::move( read ) );
    }
    return result;
}

bandit_live_world::camp_signal_observation_result record_live_bandit_staffed_camp_signals(
    bandit_live_world::world_state &state,
    const std::vector<live_bandit_signal_observation> &signals,
    const std::vector<live_bandit_sound_observation> &sounds )
{
    return bandit_live_world::record_staffed_camp_signal_observations( state,
           live_bandit_current_minutes(),
    [&signals, &sounds]( const bandit_live_world::site_record &site,
                        const bandit_live_world::camp_signal_observer_request &request ) {
        return live_bandit_staffed_camp_signal_reads( signals, sounds, site, request );
    }, resolve_live_bandit_staffed_observer );
}

void sample_and_deliver_live_light_for_advancing_turn()
{
    live_light::callbacks callbacks;
    callbacks.reconcile_riders = []() {
        overmap_buffer.reconcile_rider_band_encounters();
    };
    callbacks.age_rider_memory = []() {
        advance_zombie_rider_light_memories();
    };
    callbacks.discover = []() {
        return observe_loaded_z_light_sources();
    };
    callbacks.deliver_hordes = []( const std::vector<live_bandit_signal_observation> &samples ) {
        attract_live_hordes_from_light_observations( samples );
    };
    callbacks.deliver_stalkers = []( const std::vector<live_bandit_signal_observation> &samples ) {
        signal_live_writhing_stalkers_from_light_observations( samples );
    };
    callbacks.deliver_riders = []( const std::vector<live_bandit_signal_observation> &samples ) {
        signal_live_zombie_riders_from_light_observations( samples );
    };
    callbacks.deliver_cannibals = []( const std::vector<live_bandit_signal_observation> &samples ) {
        observe_live_hostile_signal_sources( samples );
    };
    live_light::run_advancing_turn( calendar::turn, callbacks );
}

bandit_live_world::structural_bounty_maintenance_result maintain_live_bandit_structural_bounty(
    const std::vector<live_bandit_signal_observation> &live_signals,
    const std::vector<live_bandit_sound_observation> &live_sounds )
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int current_minutes = live_bandit_current_minutes();
    int watch_paths_remaining = live_bandit_structural_watch_path_budget;
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
                live_bandit_structural_scan_budget, live_bandit_structural_dispatch_cap,
                live_bandit_structural_terrain_id,
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
    }, []( bandit_live_world::world_state & world, const std::size_t site_index ) {
        if( site_index >= world.sites.size() ) {
            return 0;
        }
        return live_bandit_materialize_abstract_members_for_response(
                   world, world.sites[site_index] );
    }, []( const bandit_live_world::site_record & site ) {
        return live_bandit_hostile_operation_route_read( site );
    }, {}, []( const bandit_live_world::site_record & before,
               const bandit_live_world::site_record & after ) {
        return persist_live_bandit_local_projection_leases( after, &before );
    } );
    DebugLog( D_INFO, DC_ALL ) << bandit_live_world::render_structural_bounty_maintenance_report(
                                   result );
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

bandit_live_world::structural_signal_record_result record_live_bandit_local_structural_sounds(
    bandit_live_world::world_state &state,
    const std::vector<live_bandit_sound_observation> &live_sounds )
{
    const std::vector<live_bandit_signal_observation> no_field_signals;
    return bandit_live_world::record_local_structural_signal_observations(
               state, live_bandit_current_minutes(),
               [&no_field_signals, &live_sounds]( const bandit_live_world::site_record & site,
    const bandit_live_world::active_outing_state & outing,
    const bandit_live_world::structural_threat_observer_request & request ) {
        return live_bandit_structural_signal_reads(
                   no_field_signals, live_sounds, site, outing, request );
    }, []( const bandit_live_world::site_record & before,
           const bandit_live_world::site_record & after ) {
        return persist_live_bandit_local_projection_leases( after, &before );
    } );
}

int record_live_bandit_stationary_watch_signals( bandit_live_world::world_state &state,
        const std::vector<live_bandit_signal_observation> &signals, const bool cadence_due )
{
    if( !cadence_due || signals.empty() ) {
        return 0;
    }
    const auto lookup = [&signals]( const bandit_live_world::site_record &site,
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request ) {
        if( outing.phase != bandit_live_world::scout_phase::observing ||
            outing.selected_watch_kind != bandit_live_world::structural_watch_kind::exact ||
            request.current_omt != outing.selected_watch_omt ||
            bandit_live_world::target_footprint_watch_distance(
                outing.selected_watch_omt, outing.target_footprint ) != 3 ) {
            return std::vector<bandit_live_world::structural_signal_read>();
        }
        return live_bandit_structural_signal_reads( signals, {}, site, outing, request );
    };
    const auto abstract = bandit_live_world::record_structural_signal_observations(
                              state, live_bandit_current_minutes(), lookup );
    const auto local = bandit_live_world::record_local_structural_signal_observations(
                           state, live_bandit_current_minutes(), lookup,
    []( const bandit_live_world::site_record &before, const bandit_live_world::site_record &after ) {
        return persist_live_bandit_local_projection_leases( after, &before );
    } );
    return abstract.facts_recorded + local.facts_recorded;
}

std::vector<live_bandit_sound_observation> take_live_bandit_sounds()
{
    std::vector<live_bandit_sound_observation> events;
    sounds::consume_significant_sounds( [&events]( const tripoint_abs_omt & source_omt,
    const int volume, const sounds::significant_sound_t kind, const int emitted_minutes ) {
        events.push_back( { source_omt, volume, kind, emitted_minutes } );
    } );
    return events;
}

int observe_live_bandit_sounds()
{
    const std::vector<live_bandit_sound_observation> events = take_live_bandit_sounds();
    if( events.empty() ) {
        return 0;
    }
    const std::vector<live_bandit_signal_observation> no_visual_signals;
    record_r008_production_channel_scan( no_visual_signals, events );
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    record_live_bandit_structural_sounds( state, events );
    record_live_bandit_local_structural_sounds( state, events );
    record_live_bandit_staffed_camp_signals( state, no_visual_signals, events );
    // Store what was heard now; ordinary overmap maintenance owns the later
    // decision to act.  A gunshot cannot advance every camp's travel/dispatch.
    return static_cast<int>( events.size() );
}

int advance_live_bandit_local_scout_assessments()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    observe_live_bandit_player_target_opportunity();
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
        avatar &u = get_avatar();
        const tripoint_abs_omt player_omt = u.pos_abs_omt();
        const std::string player_target_id = "player@" + live_bandit_omt_token( player_omt );
        const bandit_live_world::hostile_target_opportunity_record *opportunity =
            state.find_hostile_target_opportunity( player_target_id, player_omt );
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( opportunity != nullptr && opportunity->revision > 0 && opportunity->goods_value > 0 &&
            opportunity->population > 0 && opportunity->activity > 0 && cursor ) {
            map &here = get_map();
            for( const character_id &member_id : outing.member_ids ) {
                const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
                if( !member || member->is_dead() ||
                    member->pos_abs_omt() != outing.selected_watch_omt || !member->sees( here, u ) ) {
                    continue;
                }
                const bandit_live_world::sortie_observation_effect observed =
                commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                    return bandit_live_world::record_physically_observed_player_opportunity( next, *cursor,
                            member_id, player_target_id, member->pos_abs_omt(), player_omt,
                            opportunity->revision, current_minutes );
                } );
                if( observed.valid && observed.progress ) {
                    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world physical_player_observation"
                                               << " site=" << site.site_id
                                               << " scout=" << member_id.get_value()
                                               << " target=" << player_target_id
                                               << " revision=" << opportunity->revision << '\n';
                }
                break;
            }
        }
        const std::string activity_id = outing.activity_id;
        const int generation = outing.generation;
        const int target_revision = outing.target_lead_revision;
        const bandit_live_world::site_record before_assessment = site;
        const bandit_live_world::scout_assessment_result result =
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::advance_structural_scout_assessment( next, activity_id, generation,
                    target_revision, current_minutes );
        } );
        if( result == bandit_live_world::scout_assessment_result::normal_success ||
            result == bandit_live_world::scout_assessment_result::inconclusive ) {
            for( const character_id member_id : site.active_outing.member_ids ) {
                npc *member = g->find_npc( member_id );
                if( member == nullptr || member->is_dead() ) {
                    continue;
                }
                const bool travelling_before = member->is_travelling();
                const tripoint_abs_omt goal_before = member->goal;
                const std::size_t omt_path_before = member->omt_path.size();
                const std::size_t local_path_before = member->path.size();
                const bool route_assigned = live_bandit_route_member_home( *member, site );
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward motor"
                                           << " site=" << site.site_id
                                           << " activity=" << site.active_outing.activity_id
                                           << " generation=" << site.active_outing.generation
                                           << " minute=" << current_minutes
                                           << " phase=" << bandit_live_world::to_string(
                                               site.active_outing.phase )
                                           << " owner=" << bandit_live_world::to_string(
                                               site.active_outing.owner )
                                           << " member=" << member_id
                                           << " is_travelling_before=" <<
                                           ( travelling_before ? "yes" : "no" )
                                           << " goal_before=" << goal_before.to_string()
                                           << " omt_path_before=" << omt_path_before
                                           << " local_path_before=" << local_path_before
                                           << " route_assigned=" <<
                                           ( route_assigned ? "yes" : "no" )
                                           << " is_travelling_after=" <<
                                           ( member->is_travelling() ? "yes" : "no" )
                                           << " goal_after=" << member->goal.to_string()
                                           << " omt_path_after=" << member->omt_path.size()
                                           << " local_path_after=" << member->path.size() << '\n';
            }
            completed++;
        } else if( result ==
                   bandit_live_world::scout_assessment_result::alternate_watch_reposition_required ) {
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( !cursor ) {
                rollback_live_bandit_local_progress( site, before_assessment );
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
                                     member->get_ai_guard_pos(), member->path, {} } );
                routes.emplace( member_id, std::move( route ) );
            }
            if( preflight_failed || backups.size() != 2 || routes.size() != 2 ) {
                if( commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::abort_local_pair_alternate_watch_reposition( next, *cursor,
                        current_minutes,
                        "alternate watch route unavailable at departure" );
                } ) ==
                bandit_live_world::local_handoff_commit_result::applied ) {
                    for( const live_bandit_local_handoff_member_backup &backup : backups ) {
                        backup.member->omt_path.clear();
                        live_bandit_route_member_home( *backup.member, site );
                    }
                    completed++;
                } else {
                    rollback_live_bandit_local_progress( site, before_assessment );
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
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::start_local_pair_alternate_watch_reposition( next, *cursor,
                    current_minutes );
            } ) !=
            bandit_live_world::local_handoff_commit_result::applied ) {
                restore_routes();
                rollback_live_bandit_local_progress( site, before_assessment );
                continue;
            }
            completed++;
        }
    }
    return completed;
}

struct live_bandit_elevated_recovery_order {
    tripoint_abs_omt approach;
    tripoint_abs_omt physical_target;
    tripoint_abs_omt watch;
};

using live_bandit_elevated_recovery_orders =
    std::map<character_id, live_bandit_elevated_recovery_order>;

live_bandit_elevated_recovery_orders recover_live_bandit_local_elevated_watches()
{
    live_bandit_elevated_recovery_orders orders;
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        if( !bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) ) {
            continue;
        }
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        bandit_live_world::structural_outing_plan plan;
        plan.lead_id = outing.target_lead_id;
        plan.target_omt = outing.target_omt;
        plan.job = bandit_dry_run::job_template::scout;
        plan.shared_route = outing.shared_route;
        int watch_budget = live_bandit_structural_watch_path_budget;
        const bandit_live_world::structural_route_read read =
            live_bandit_structural_route_read( site, plan, watch_budget );
        const bandit_live_world::watch_selection_result selected =
            bandit_live_world::select_watch_ring_candidate(
                read.target_footprint, read.watch_candidates );
        if( !read.reachable || !read.watch_geography_supplied ||
            !selected.valid ||
            selected.outcome != bandit_live_world::watch_selection_outcome::selected_exact ||
            !bandit_live_world::structural_watch_shared_route_is_canonical(
                read.watch_shared_route, site.anchor, selected.omt,
                read.target_footprint ) ) {
            continue;
        }
        const tripoint_abs_omt approach = read.watch_shared_route[1];
        const tripoint_abs_omt physical_target(
            outing.target_omt.x(), outing.target_omt.y(), site.anchor.z() );
        std::vector<npc *> members;
        std::vector<std::vector<tripoint_abs_omt>> routes;
        std::vector<bandit_live_world::local_route_arrival_member_read> member_reads;
        bool all_at_approach = true;
        bool eligible = true;
        for( const character_id member_id : outing.member_ids ) {
            npc *member = g->find_npc( member_id );
            if( member == nullptr ) {
                const shared_ptr_fast<npc> persistent = overmap_buffer.find_npc( member_id );
                member = persistent.get();
            }
            if( member == nullptr || member->is_dead() ||
                member->has_flag( json_flag_CANNOT_MOVE ) ||
                member->get_attitude() == NPCATT_FLEE ||
                member->get_attitude() == NPCATT_FLEE_TEMP ||
                ( member->current_target() != nullptr && member->get_ai_danger() > 0 ) ) {
                eligible = false;
                break;
            }
            members.push_back( member );
            member_reads.push_back( { member_id, true, false, false,
                                      member->hp_percentage(), member->pos_abs() } );
            if( member->pos_abs_omt() == approach ) {
                routes.emplace_back();
                continue;
            }
            all_at_approach = false;
            std::vector<tripoint_abs_omt> route = live_bandit_member_route_to(
                        *member, site, approach );
            const int start_distance = std::max( 1, rl_dist(
                                                member->pos_abs_omt(), physical_target ) );
            if( route.empty() || !std::all_of( route.begin(), route.end(),
            [physical_target, start_distance]( const tripoint_abs_omt & omt ) {
                return omt.z() == physical_target.z() &&
                       rl_dist( omt, physical_target ) >= start_distance;
            } ) ) {
                eligible = false;
                break;
            }
            routes.push_back( std::move( route ) );
        }
        if( !eligible || members.size() != 2 ) {
            continue;
        }
        if( all_at_approach ) {
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( cursor &&
            commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
            return bandit_live_world::recover_local_elevated_signal_watch( next, *cursor, read, member_reads,
                    live_bandit_current_minutes() );
            } ) ==
            bandit_live_world::structural_watch_route_apply_result::applied ) {
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world roof_watch_recovered"
                                           << " site=" << site.site_id
                                           << " generation=" << site.active_outing.generation
                                           << " watch=" << site.active_outing.selected_watch_omt
                                           << " minute=" << live_bandit_current_minutes() << '\n';
            }
            continue;
        }
        for( std::size_t index = 0; index < members.size(); ++index ) {
            npc &member = *members[index];
            if( routes[index].empty() ||
                ( member.is_travelling() && member.goal == approach &&
                  !member.omt_path.empty() ) ) {
                continue;
            }
            member.guard_pos.reset();
            member.clear_ai_guard_pos();
            member.goal = approach;
            member.omt_path = std::move( routes[index] );
            member.set_mission( NPC_MISSION_TRAVELLING );
        }
        for( const character_id id : outing.member_ids ) {
            orders.emplace( id, live_bandit_elevated_recovery_order{
                approach, physical_target, selected.omt
            } );
        }
    }
    return orders;
}

} // namespace

namespace
{
bandit_live_world::camp_signal_observation_result record_live_light_staffed_observer_at_cadence(
    bandit_live_world::world_state &bandit_state,
    const std::vector<live_bandit_signal_observation> &,
    const std::vector<live_bandit_sound_observation> &live_sounds,
    bool signal_cadence_due );
}

void reset_live_light_sample_cache()
{
    live_light::reset();
    live_source_sampler.reset();
}

int observe_live_hostile_signal_sources_for_test(
    const std::vector<live_bandit_signal_observation> &signals )
{
    return observe_live_hostile_signal_sources( signals );
}

void maintain_live_bandit_structural_bounty_for_test(
    const std::vector<live_bandit_signal_observation> &signals )
{
    maintain_live_bandit_structural_bounty( signals, {} );
    prepare_live_bandit_abstract_scout_travel();
}

bool live_bandit_scout_watch_order_pending( const npc &member )
{
    return pending_abstract_scout_watch_order( member );
}

void prepare_live_bandit_abstract_scout_travel_for_test()
{
    prepare_live_bandit_abstract_scout_travel();
}

void run_live_light_delivery_for_test()
{
    sample_and_deliver_live_light_for_advancing_turn();
}

int observe_live_bandit_sounds_for_test()
{
    return observe_live_bandit_sounds();
}

void run_live_light_staffed_observer_for_test()
{
    const std::vector<live_bandit_sound_observation> no_sounds;
    record_live_light_staffed_observer_at_cadence(
        overmap_buffer.global_state.bandit_live_world,
        live_light::samples_for_turn( calendar::turn ), no_sounds, true );
}

bool live_light_sample_is_current_for_test()
{
    return live_light::sample_is_current( calendar::turn );
}

std::vector<live_light_delivery_stage> live_light_delivery_order_for_test()
{
    return live_light::delivery_order();
}

std::vector<live_light_delivery_stage> live_light_delivery_trace_for_test()
{
    return live_light::delivery_trace();
}

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

std::string live_bandit_structural_signal_request_diagnostic_for_test(
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request )
{
    return live_bandit_structural_signal_request_diagnostic( outing, request );
}

namespace
{
void write_harness_omt( JsonOut &json, const tripoint_abs_omt &omt )
{
    json.start_object();
    json.member( "x", omt.x() );
    json.member( "y", omt.y() );
    json.member( "z", omt.z() );
    json.end_object();
}

void write_harness_route_read( JsonOut &json,
                               const bandit_live_world::structural_route_read &read )
{
    json.start_object();
    json.member( "reachable", read.reachable );
    json.member( "complete_route_cost", read.complete_route_cost );
    json.member( "max_segment_risk", read.max_segment_risk );
    json.member( "summary", read.summary );
    json.member( "watch_geography_supplied", read.watch_geography_supplied );
    json.member( "watch_candidates" );
    json.start_array();
    for( const bandit_live_world::watch_selection_candidate &candidate : read.watch_candidates ) {
        json.start_object();
        json.member( "omt" );
        write_harness_omt( json, candidate.omt );
        json.member( "route_cost", candidate.route_cost );
        json.end_object();
    }
    json.end_array();
    json.member( "watch_shared_route" );
    json.start_array();
    for( const tripoint_abs_omt &omt : read.watch_shared_route ) {
        write_harness_omt( json, omt );
    }
    json.end_array();
    json.end_object();
}

std::string canonical_harness_ecology_state(
    const bandit_live_world::world_state &state )
{
    return state.canonical_snapshot();
}

bool openclaw_harness_r027_snapshot_request_is_authorized()
{
    const char *const request = std::getenv( "OPENCLAW_HARNESS_R027_WORLD_STATE_REQUEST" );
    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const char *const scenario = std::getenv( "OPENCLAW_HARNESS_SCENARIO" );
    const char *const source = std::getenv( "OPENCLAW_HARNESS_RUNTIME_SOURCE_SHA256" );
    const char *const executable = std::getenv( "OPENCLAW_HARNESS_EXECUTABLE_SHA256" );
    const char *const binding = std::getenv( "OPENCLAW_HARNESS_BINDING_ID" );
    return request != nullptr && std::string_view( request ) == "read" &&
           run_id != nullptr && run_id[0] != '\0' && scenario != nullptr &&
           scenario[0] != '\0' && source != nullptr && source[0] != '\0' &&
           executable != nullptr && executable[0] != '\0' && binding != nullptr &&
           binding[0] != '\0';
}

const char *openclaw_harness_attitude_name( const Creature::Attitude attitude )
{
    switch( attitude ) {
        case Creature::Attitude::HOSTILE:
            return "hostile";
        case Creature::Attitude::FRIENDLY:
            return "friendly";
        case Creature::Attitude::NEUTRAL:
            return "neutral";
        case Creature::Attitude::ANY:
            return "any";
    }
    return "unknown";
}

bool write_openclaw_harness_r027_world_state_snapshot_to( const avatar &u, const map &here,
        const char *const path_value )
{
    if( path_value == nullptr || path_value[0] == '\0' ) {
        return false;
    }

    const std::filesystem::path path( path_value );
    std::error_code error;
    if( std::filesystem::exists( path, error ) || error ) {
        DebugLog( D_WARNING, DC_ALL ) << "openclaw_harness r027 world-state snapshot rejected"
                                      << " reason=destination_exists_or_unreadable";
        return false;
    }

    std::ofstream output( path, std::ios::out | std::ios::app );
    if( !output ) {
        DebugLog( D_WARNING, DC_ALL ) << "openclaw_harness r027 world-state snapshot rejected"
                                      << " reason=destination_unwritable";
        return false;
    }

    const tripoint_bub_ms avatar_tile = u.pos_bub();
    const tripoint_bub_ms source_tile = avatar_tile + tripoint_rel_ms::south;
    // R-027's saved physical source is deliberately independent of the
    // avatar's current footing.  Retain its fixed absolute read so a farther
    // zero-credit footing cannot make the audit silently inspect a new tile.
    const tripoint_abs_ms saved_source( 3648, 1187, 0 );
    const tripoint_bub_ms saved_source_tile = here.get_bub( saved_source );
    JsonOut json( output );
    json.start_object();
    json.member( "schema", "caol-r027-world-state-snapshot-v1" );
    json.member( "run_id", std::getenv( "OPENCLAW_HARNESS_RUN_ID" ) );
    json.member( "scenario_id", std::getenv( "OPENCLAW_HARNESS_SCENARIO" ) );
    json.member( "binding_id", std::getenv( "OPENCLAW_HARNESS_BINDING_ID" ) );
    json.member( "runtime_source_sha256", std::getenv( "OPENCLAW_HARNESS_RUNTIME_SOURCE_SHA256" ) );
    json.member( "executable_sha256", std::getenv( "OPENCLAW_HARNESS_EXECUTABLE_SHA256" ) );
    json.member( "game_minutes", to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) );
    json.member( "avatar" );
    json.start_object();
    json.member( "abs_ms", u.pos_abs().to_string() );
    json.member( "abs_omt", u.pos_abs_omt().to_string() );
    json.member( "fire_intensity", here.get_field_intensity( avatar_tile, fd_fire ) );
    json.member( "effects" );
    json.start_array();
    for( const std::reference_wrapper<const effect> &effect_ref : u.get_effects() ) {
        const effect &avatar_effect = effect_ref.get();
        json.start_object();
        json.member( "id", avatar_effect.get_id().str() );
        json.member( "intensity", avatar_effect.get_intensity() );
        json.member( "duration_turns", to_turns<int>( avatar_effect.get_duration() ) );
        json.end_object();
    }
    json.end_array();
    json.member( "body_parts" );
    json.start_array();
    for( const bodypart_id &body_part : u.get_all_body_parts( get_body_part_flags::only_main ) ) {
        json.start_object();
        json.member( "id", body_part.id().str() );
        json.member( "hp_current", u.get_part_hp_cur( body_part ) );
        json.member( "hp_max", u.get_part_hp_max( body_part ) );
        json.end_object();
    }
    json.end_array();
    json.end_object();
    // This is the existing production signal scan radius, not a new diagnostic
    // envelope.  The read-only fixture audit must see threats and fields that
    // can participate in the same loaded-bubble neighborhood as the signal.
    json.member( "nearby_entities" );
    json.start_array();
    creature_tracker &creatures = get_creature_tracker();
    for( const tripoint_bub_ms &tile : here.points_in_radius( avatar_tile, 60 ) ) {
        Creature *const critter = creatures.creature_at( tile );
        if( critter == nullptr || critter == &u ) {
            continue;
        }
        json.start_object();
        json.member( "identity", critter->disp_name() );
        json.member( "kind", critter->is_monster() ? "monster" :
                     critter->is_npc() ? "npc" : "other" );
        json.member( "attitude", openclaw_harness_attitude_name( critter->attitude_to( u ) ) );
        json.member( "abs_ms", critter->pos_abs().to_string() );
        json.end_object();
    }
    json.end_array();
    json.member( "damaging_fields" );
    json.start_array();
    for( const tripoint_bub_ms &tile : here.points_in_radius( avatar_tile, 60 ) ) {
        for( const std::pair<const field_type_id, field_entry> &field : here.field_at( tile ) ) {
            if( !u.is_dangerous_field( field.second ) ) {
                continue;
            }
            json.start_object();
            json.member( "field", field.first->id.str() );
            json.member( "intensity", field.second.get_field_intensity() );
            json.member( "abs_ms", here.get_abs( tile ).to_string() );
            json.end_object();
        }
    }
    json.end_array();
    json.member( "saved_source_south_of_avatar" );
    json.start_object();
    json.member( "abs_ms", here.get_abs( source_tile ).to_string() );
    json.member( "fire_intensity", here.get_field_intensity( source_tile, fd_fire ) );
    json.end_object();
    json.member( "fixed_saved_source" );
    json.start_object();
    json.member( "abs_ms", saved_source.to_string() );
    json.member( "fire_intensity", here.get_field_intensity( saved_source_tile, fd_fire ) );
    json.end_object();
    json.member( "bandit_live_world" );
    overmap_buffer.global_state.bandit_live_world.serialize( json );
    json.end_object();
    output << '\n';
    return static_cast<bool>( output );
}

void write_openclaw_harness_r027_world_state_snapshot( const avatar &u, const map &here )
{
    const char *const path_value = std::getenv( "OPENCLAW_HARNESS_R027_WORLD_STATE_PATH" );
    if( !openclaw_harness_r027_snapshot_request_is_authorized() ) {
        return;
    }
    write_openclaw_harness_r027_world_state_snapshot_to( u, here, path_value );
}

bool openclaw_harness_r027_onfire_cleanup_is_authorized()
{
    const char *const request = std::getenv( "OPENCLAW_HARNESS_R027_ONFIRE_CLEANUP_REQUEST" );
    const char *const scenario = std::getenv( "OPENCLAW_HARNESS_SCENARIO" );
    return openclaw_harness_r027_snapshot_request_is_authorized() && request != nullptr &&
           std::string_view( request ) == "remove_onfire" && scenario != nullptr &&
           std::string_view( scenario ) ==
           "bandit.r027_avatar_onfire_cleanup_bootstrap_v001_mcw";
}

void remove_openclaw_harness_r027_avatar_onfire( avatar &u, const map &here )
{
    const char *const before_path = std::getenv( "OPENCLAW_HARNESS_R027_ONFIRE_CLEANUP_BEFORE_PATH" );
    const char *const after_path = std::getenv( "OPENCLAW_HARNESS_R027_ONFIRE_CLEANUP_AFTER_PATH" );
    if( !openclaw_harness_r027_onfire_cleanup_is_authorized() || before_path == nullptr ||
        after_path == nullptr || before_path[0] == '\0' || after_path[0] == '\0' ||
        std::string_view( before_path ) == after_path || !u.has_effect( effect_onfire ) ) {
        return;
    }
    if( !write_openclaw_harness_r027_world_state_snapshot_to( u, here, before_path ) ) {
        return;
    }
    u.remove_effect( effect_onfire );
    if( !write_openclaw_harness_r027_world_state_snapshot_to( u, here, after_path ) ) {
        DebugLog( D_ERROR, DC_ALL ) << "openclaw_harness r027 onfire cleanup after snapshot failed";
    }
}
} // namespace

bool write_harness_new_world_feasibility_artifact()
{
    const char *const feasibility_requested =
        std::getenv( "OPENCLAW_HARNESS_BANDIT_FEASIBILITY" );
    if( feasibility_requested == nullptr ||
        std::string_view( feasibility_requested ) != "1" ) {
        return true;
    }
    const char *const run_dir_value = std::getenv( "OPENCLAW_HARNESS_RUN_DIR" );
    if( run_dir_value == nullptr || run_dir_value[0] == '\0' ) {
        return false;
    }

    const bandit_live_world::world_state persistent_before =
        overmap_buffer.global_state.bandit_live_world;
    const std::string ecology_before = canonical_harness_ecology_state( persistent_before );
    bandit_live_world::world_state analysis_state = persistent_before;
    const tripoint_abs_omt player_omt = get_avatar().pos_abs_omt();
    const auto special_lookup = []( const tripoint_abs_omt &candidate ) -> std::optional<std::string> {
        if( const std::optional<overmap_special_id> special =
                overmap_buffer.overmap_special_at_existing( candidate ) ) {
            return special->str();
        }
        return std::nullopt;
    };
    const int bootstrap_radius_omt = live_bandit_system_envelope_omt;
    const bandit_live_world::abstract_bootstrap_result bootstrap =
        bandit_live_world::register_abstract_sites_near( analysis_state, player_omt,
                bootstrap_radius_omt, special_lookup );

    const bandit_live_world::site_record *natural_bandit_site = nullptr;
    double natural_bandit_distance = std::numeric_limits<double>::infinity();
    tripoint_abs_omt natural_bandit_nearest_omt;
    for( const bandit_live_world::site_record &site : analysis_state.sites ) {
        if( site.site_kind != bandit_live_world::owned_site_kind::bandit_camp ) {
            continue;
        }
        const auto nearest = std::min_element( site.footprint.begin(), site.footprint.end(),
        [&player_omt]( const tripoint_abs_omt & lhs, const tripoint_abs_omt & rhs ) {
            const auto squared_distance = [&player_omt]( const tripoint_abs_omt & omt ) {
                const double dx = static_cast<double>( omt.x() - player_omt.x() );
                const double dy = static_cast<double>( omt.y() - player_omt.y() );
                return dx * dx + dy * dy;
            };
            return std::make_tuple( squared_distance( lhs ), lhs.z(), lhs.y(), lhs.x() ) <
                   std::make_tuple( squared_distance( rhs ), rhs.z(), rhs.y(), rhs.x() );
        } );
        const tripoint_abs_omt nearest_omt = nearest == site.footprint.end() ? site.anchor : *nearest;
        const double dx = static_cast<double>( nearest_omt.x() - player_omt.x() );
        const double dy = static_cast<double>( nearest_omt.y() - player_omt.y() );
        const double distance = std::hypot( dx, dy );
        if( distance <= bootstrap_radius_omt && distance < natural_bandit_distance ) {
            natural_bandit_site = &site;
            natural_bandit_distance = distance;
            natural_bandit_nearest_omt = nearest_omt;
        }
    }

    const int now_minutes = live_bandit_current_minutes();
    const bandit_live_world::structural_bounty_scan_result scan =
        bandit_live_world::advance_structural_bounty_scan( analysis_state, now_minutes,
                live_bandit_structural_scan_budget,
                live_bandit_structural_terrain_id );
    const bandit_live_world::site_record *analyzed_site = natural_bandit_site == nullptr ? nullptr :
            analysis_state.find_site( natural_bandit_site->site_id );
    const std::vector<bandit_live_world::structural_outing_plan> all_candidates =
        analyzed_site == nullptr ? std::vector<bandit_live_world::structural_outing_plan>() :
        bandit_live_world::plan_structural_bounty_outing_candidates( *analyzed_site, now_minutes,
                false );
    // The production planner has already applied its bounded remembered-ground inventory.
    // Preserve every measured candidate in this diagnostic instead of recreating a shorter prefix.
    const std::vector<bandit_live_world::structural_outing_plan> &candidates = all_candidates;
    const int watch_budget_before = live_bandit_structural_watch_path_budget;
    int watch_budget_after = watch_budget_before;
    std::vector<bandit_live_world::structural_route_read> reads;
    if( analyzed_site != nullptr ) {
        reads = live_bandit_structural_route_analyzer_reads_for_test( *analyzed_site, candidates,
                watch_budget_after );
    }

    const std::string ecology_after = canonical_harness_ecology_state(
                                          overmap_buffer.global_state.bandit_live_world );
    const bool ecology_unchanged = ecology_before == ecology_after;
    const std::filesystem::path artifact_path = std::filesystem::path( run_dir_value ) /
            "harness_new_world_feasibility.json";
    std::ofstream artifact( artifact_path );
    if( !artifact ) {
        return false;
    }
    JsonOut json( artifact );
    json.start_object();
    json.member( "artifact_kind", "harness_new_world_feasibility" );
    json.member( "artifact_version", 1 );
    json.member( "raw_seed", g->get_seed() );
    json.member( "player_omt" );
    write_harness_omt( json, player_omt );
    json.member( "persistent_ecology_state_before", ecology_before );
    json.member( "persistent_ecology_state_after", ecology_after );
    json.member( "persistent_ecology_unchanged", ecology_unchanged );
    json.member( "bootstrap_radius_omt", bootstrap_radius_omt );
    json.member( "bootstrap_created_sites", bootstrap.created_sites );
    json.member( "bootstrap_recognized_tiles", bootstrap.recognized_tiles );
    json.member( "natural_bandit_site_found", natural_bandit_site != nullptr );
    if( natural_bandit_site != nullptr ) {
        json.member( "natural_bandit_site_id", natural_bandit_site->site_id );
        json.member( "natural_bandit_site_anchor" );
        write_harness_omt( json, natural_bandit_site->anchor );
        json.member( "natural_bandit_site_nearest_omt" );
        write_harness_omt( json, natural_bandit_nearest_omt );
        json.member( "natural_bandit_site_distance_omt", natural_bandit_distance );
    }
    json.member( "structural_scan_budget", scan.scan_budget );
    json.member( "structural_scan_budget_used", scan.budget_used );
    json.member( "structural_scan_sites_considered", scan.sites_considered );
    json.member( "structural_scan_candidates_sampled", scan.candidates_sampled );
    json.member( "structural_scan_notes", scan.notes );
    json.member( "candidate_prefix_limit", candidates.size() );
    json.member( "candidate_rows" );
    json.start_array();
    for( std::size_t index = 0; index < candidates.size(); ++index ) {
        const bandit_live_world::structural_outing_plan &plan = candidates[index];
        const bandit_live_world::structural_route_read &read = reads[index];
        json.start_object();
        json.member( "lead_id", plan.lead_id );
        json.member( "target_omt" );
        write_harness_omt( json, plan.target_omt );
        json.member( "selector", "non_frontier" );
        json.member( "watch_budget_before", watch_budget_before );
        json.member( "watch_budget_after", watch_budget_after );
        const auto selected_watch = std::find_if( read.watch_candidates.begin(),
                read.watch_candidates.end(), [&read](
            const bandit_live_world::watch_selection_candidate &candidate ) {
            return read.watch_shared_route.size() > 2 &&
                   read.watch_shared_route[2] == candidate.omt;
        } );
        const bool selected = read.reachable && selected_watch != read.watch_candidates.end() &&
                              !read.watch_shared_route.empty();
        json.member( "outcome", selected ? "selected" : "rejected" );
        json.member( "watch" );
        if( selected ) {
            write_harness_omt( json, selected_watch->omt );
        } else {
            json.write_null();
        }
        json.member( "watch_route_cost", selected ? selected_watch->route_cost : -1 );
        json.member( "analyzer_record", live_bandit_structural_route_analyzer_record_for_test(
                         *analyzed_site, plan, "non_frontier", read ) );
        json.member( "read" );
        write_harness_route_read( json, read );
        json.end_object();
    }
    json.end_array();
    json.member( "route_watch_budget_before", watch_budget_before );
    json.member( "route_watch_budget_after", watch_budget_after );
    json.member( "route_watch_budget_used", watch_budget_before - watch_budget_after );
    json.end_object();
    return static_cast<bool>( artifact );
}

void run_live_bandit_structural_route_analyzer_for_debug()
{
    if( !get_avatar().has_trait( trait_DEBUG_CLAIRVOYANCE ) ) {
        return;
    }

    const bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const int now_minutes = live_bandit_current_minutes();
    int watch_paths_remaining = live_bandit_structural_watch_path_budget;
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

    map &here = get_map();
    avatar &observer = get_avatar();
    std::vector<std::string> records;
    bool unsafe = false;
    for( monster &critter : g->all_monsters() ) {
        const std::string record = live_bandit_local_reality_safety_record_for_test( here, observer,
                                   critter );
        unsafe = unsafe || record.find( " verdict=unsafe" ) != std::string::npos;
        records.push_back( record );
    }
    std::sort( records.begin(), records.end() );
    DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local_reality_safety_preflight"
                               << " observer=" << observer.pos_abs().to_string()
                               << " resident_monsters=" << records.size()
                               << " outcome=" << ( unsafe ? "unsafe" : "safe" ) << '\n';
    for( const std::string &record : records ) {
        DebugLog( D_INFO, DC_ALL ) << record << '\n';
    }
}

std::string live_bandit_local_reality_safety_record_for_test( const map &here,
        const avatar &observer, monster &critter )
{
    const bool hostile = critter.attitude_to( observer ) == Creature::Attitude::HOSTILE;
    const bool infrared = critter.has_flag( mon_flag_INFRARED_VISION );
    const bool visible = critter.sees_without_clairvoyance( here, observer );
    const bool contact = rl_dist( critter.pos_abs(), observer.pos_abs() ) <= 1;
    const bool same_z_possible_reach = critter.can_reach_to( observer.pos_bub( here ) );
    const Creature *const attack_target = critter.attack_target();
    const bool targets_observer = attack_target == &observer;
    const bool unsafe = hostile && ( contact || targets_observer ||
                                     ( visible && same_z_possible_reach ) );
    return "bandit_live_world local_reality_safety_resident"
           " identity=" + critter.type->id.str() + "@" + critter.pos_abs().to_string() +
           " type=" + critter.type->id.str() +
           " position=" + critter.pos_abs().to_string() +
           " senses=infrared:" + ( infrared ? "yes" : "no" ) +
           " hostile=" + ( hostile ? "yes" : "no" ) +
           " visible=" + ( visible ? "yes" : "no" ) +
           " contact=" + ( contact ? "yes" : "no" ) +
           " same_z_possible_reach=" + ( same_z_possible_reach ? "yes" : "no" ) +
           " attack_target=" + ( targets_observer ? "avatar" : "other_or_none" ) +
           " goal=" + ( critter.has_dest() ? critter.get_dest().to_string() : "none" ) +
           " verdict=" + ( unsafe ? "unsafe" : "safe" );
}

namespace bandit_live_world
{
bool record_live_local_pair_member_death( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const character_id member_id,
        const tripoint_abs_ms &death_position,
        const int current_minutes )
{
    const auto actor = overmap_buffer.find_npc( member_id );
    if( !actor || !actor->is_dead() || actor->pos_abs() != death_position ) {
        return false;
    }
    return commit_live_bandit_local_progress( site, [&]( site_record & next ) {
        return record_local_pair_member_death( next, expected_cursor, member_id,
                                               death_position, current_minutes );
    }, false, member_id );
}

bool live_structural_observer_has_optic( const npc &observer )
{
    static const json_character_flag enhanced_vision( "ENHANCED_VISION" );
    return observer.cache_has_item_with( flag_ZOOM ) ||
           observer.has_flag( enhanced_vision ) ||
           observer.cache_has_item_with( "is_gun", &item::is_gun,
    []( const item & gun ) {
        const std::vector<const item *> mods = gun.gunmods();
        return std::any_of( mods.begin(), mods.end(),
        []( const item * mod ) {
            return mod != nullptr && mod->has_flag( flag_ZOOM );
        } );
    } );
}

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
    // Keep post-death viewers and prompts inside the native harness lifetime.
    // An unsupported viewer must publish its own owner, not leave the final
    // death-camera question and its deferred receipt visible after it closed.
    std::optional<semantic_surface_manager_session> cleanup_semantic_session;
    if( active_semantic_surface_manager() == nullptr && openclaw_harness_semantic_session_active() ) {
        cleanup_semantic_session.emplace( openclaw_harness_semantic_surface_manager() );
    }
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
        // A long-running activity owns the turn outside World::handle_action.
        // Its native input loop still accepts the ordinary pause action, which
        // opens the game's own activity-cancellation query.  Give that exact
        // native owner a semantic surface instead of leaving an actionless
        // activity-resumed marker until unrelated physical input arrives.
        std::optional<semantic_surface_manager_session> semantic_session;
        if( active_semantic_surface_manager() == nullptr && openclaw_harness_semantic_session_active() ) {
            semantic_session.emplace( openclaw_harness_semantic_surface_manager() );
        }
        semantic_surface_manager *const semantic_manager = active_semantic_surface_manager();
        std::optional<semantic_surface_scope> semantic_scope;
        std::string semantic_action;
        if( semantic_manager != nullptr ) {
            semantic_scope.emplace( *semantic_manager, "activity_wait", "Activity in progress",
                                    std::map<std::string, std::string>{
                { "native_owner", "DEFAULTMODE" },
                { "native_action", "pause" },
                { "activity_generation", std::to_string( u.activity.input_generation() ) },
                { "activity_type", u.activity.id().str() },
            }, std::vector<semantic_action_descriptor>{
                { "activity.pause", "", _( "Pause activity" ), u.activity.is_interruptible_with_kb() },
            }, [ &semantic_action, semantic_manager ]( const semantic_action_request &request ) {
                if( request.action_id != "activity.pause" ) {
                    return semantic_action_dispatch_result{ false, "unadvertised_action", "" };
                }
                // The following native cancellation query owns any next
                // player decision.  The pause request itself has a durable
                // native owner receipt; the cancellation query is a separate
                // prompt owner and must not be used as a deferred receipt
                // successor.  Deferring here strands the accepted request
                // when native cancellation exits without recreating an
                // activity surface.
                semantic_manager->withhold_parent_authority_until_recreated( request.surface_id );
                semantic_action = "pause";
                return semantic_action_dispatch_result{ true, "", "", false, false };
            } );
            semantic_scope->consume_request();
        }
        std::string action = semantic_action;
        if( action.empty() ) {
            action = ctxt.handle_input( 0 );
            // A transport wake is consumed inside input_context.  Its
            // semantic consumer selected the real native pause action above,
            // while input_context correctly returns CATA_ERROR rather than
            // fabricating a physical key event.
            if( !semantic_action.empty() ) {
                action = semantic_action;
            }
        }
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

bool live_bandit_member_can_take_homeward_step( const npc &member )
{
    return !member.is_dead() && !member.in_sleep_state() &&
           !member.has_flag( json_flag_CANNOT_MOVE ) && !member.has_effect( effect_downed ) &&
           !member.has_effect( effect_stunned ) && !member.has_effect( effect_psi_stunned ) &&
           !member.has_effect( effect_narcosis );
}

bool live_bandit_local_step_has_known_dangerous_trap(
    const npc &member, map &here, const tripoint_bub_ms &step )
{
    const trap &candidate = here.tr_at( step );
    return candidate.can_see( step, member ) && !candidate.is_benign();
}

// A separated homeward pair needs a reachable rendezvous, not a greedy decrease
// in straight-line distance.  Keep one actor stationary while the other follows
// a complete native route; this permits safe detours without releasing cohesion.
struct live_bandit_homeward_reunion_route {
    character_id mover;
    std::vector<tripoint_bub_ms> path;
};

live_bandit_homeward_reunion_route live_bandit_homeward_reunion_path(
    npc &member, npc &partner, const bandit_live_world::site_record &site, map &here )
{
    if( site.active_outing.leader_id != member.getID() &&
        site.active_outing.leader_id != partner.getID() ) {
        return {};
    }
    npc *leader = member.getID() == site.active_outing.leader_id ? &member : &partner;
    npc *follower = leader == &member ? &partner : &member;
    for( npc *mover : {
             follower, leader
         } ) {
        npc &anchor = mover == leader ? *follower : *leader;
        if( !live_bandit_member_can_take_homeward_step( *mover ) ||
            !here.inbounds( mover->pos_abs() ) || !here.inbounds( anchor.pos_abs() ) ) {
            continue;
        }
        const auto relationship = bandit_live_world::read_active_covert_scout_homeward_member(
                                      overmap_buffer.global_state.bandit_live_world, mover->getID() );
        if( !relationship ) {
            continue;
        }
        const auto npc_avoid = mover->get_path_avoid();
        const auto avoid = [&]( const tripoint_bub_ms & step ) {
            return npc_avoid( step ) ||
                   live_bandit_local_step_has_known_dangerous_trap( *mover, here, step ) ||
                   !live_bandit_local_step_respects_nonreentry(
                       *mover, *relationship, here, step );
        };
        bandit_live_world_probe::scoped_loaded_covert_local_path_solve path_solve_probe;
        const auto valid_reunion_path = [&]( std::vector<tripoint_bub_ms> &path ) {
            while( !path.empty() && path.front() == mover->pos_bub( here ) ) {
                path.erase( path.begin() );
            }
            if( path.empty() || project_to<coords::omt>( here.get_abs( path.back() ) ) !=
                anchor.pos_abs_omt() || !std::all_of( path.begin(), path.end(), [&]( const auto & step ) {
                return !avoid( step );
            } ) ) {
                return false;
            }
            if( relationship->phase == bandit_live_world::scout_phase::burned_withdrawal ) {
                std::vector<tripoint_abs_omt> route{ mover->pos_abs_omt() };
                for( const auto &step : path ) {
                    const tripoint_abs_omt omt = project_to<coords::omt>( here.get_abs( step ) );
                    if( omt != route.back() ) {
                        route.push_back( omt );
                    }
                }
                std::reverse( route.begin(), route.end() );
                return bandit_live_world::covert_scout_egress_route_respects_retry_memory(
                           site.active_outing, mover->pos_abs_omt(), route, false );
            }
            return true;
        };
        const int radius = bandit_live_world::local_pair_cohesion_radius();
        auto path = here.route( mover->pos_bub( here ),
                                pathfinding_target::radius( anchor.pos_bub( here ), radius ),
                                mover->get_pathfinding_settings( false ), avoid );
        if( valid_reunion_path( path ) ) {
            return { mover->getID(), std::move( path ) };
        }
        // A radius target can end on an occupied/avoided tile or across an OMT edge.
        // Try the legal endpoints of that same rendezvous, without relaxing the route.
        std::vector<tripoint_bub_ms> endpoints;
        for( const auto &point : here.points_in_radius( anchor.pos_bub( here ), radius ) ) {
            if( here.inbounds( point ) && !avoid( point ) &&
                project_to<coords::omt>( here.get_abs( point ) ) == anchor.pos_abs_omt() ) {
                endpoints.push_back( point );
            }
        }
        std::sort( endpoints.begin(), endpoints.end(), [&]( const auto & a, const auto & b ) {
            return std::make_tuple( rl_dist( a, mover->pos_bub( here ) ), a.z(), a.y(), a.x() ) <
                   std::make_tuple( rl_dist( b, mover->pos_bub( here ) ), b.z(), b.y(), b.x() );
        } );
        for( const auto &endpoint : endpoints ) {
            path = here.route( mover->pos_bub( here ), pathfinding_target::point( endpoint ),
                               mover->get_pathfinding_settings( false ), avoid );
            if( valid_reunion_path( path ) ) {
                return { mover->getID(), std::move( path ) };
            }
        }
    }
    return {};
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
               live_bandit_local_step_has_known_dangerous_trap( member, here, step ) ||
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
    [&member, &relationship, &here, &npc_avoid]( const tripoint_bub_ms & step ) {
        // Native point targets may bypass avoidance at the endpoint.  A handoff
        // needs two genuinely reachable slots, without pushing the other actor.
        return ( step == member.pos_bub( here ) || !npc_avoid( step ) ) &&
               !live_bandit_local_step_has_known_dangerous_trap( member, here, step ) &&
               live_bandit_local_step_respects_nonreentry( member, relationship, here, step );
    } );
    return read;
}

// Visit each loaded edge tile once.  An off-bubble step can only start on
// this perimeter; the interior adds area-sized work before route selection.
template<typename Func>
void for_each_live_bandit_loaded_perimeter( map &here, int z, Func &&visit )
{
    const int width = SEEX * here.getmapsize();
    const int height = SEEY * here.getmapsize();
    if( width <= 0 || height <= 0 ) {
        return;
    }
    for( int x = 0; x < width; ++x ) {
        visit( tripoint_bub_ms( x, 0, z ) );
        if( height > 1 ) {
            visit( tripoint_bub_ms( x, height - 1, z ) );
        }
    }
    for( int y = 1; y < height - 1; ++y ) {
        visit( tripoint_bub_ms( 0, y, z ) );
        if( width > 1 ) {
            visit( tripoint_bub_ms( width - 1, y, z ) );
        }
    }
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
        // The ingress consumer commits a watch arrival, not an intermediate
        // boundary crossing.  A closer exit in the current OMT would be rolled
        // back by that consumer forever, leaving the pair parked at its slots.
        const bool watch_arrival_crossing =
            outing.owner == bandit_live_world::simulation_owner::local &&
            outing.phase == bandit_live_world::scout_phase::observing &&
            !bandit_live_world::structural_outing_uses_frontier_route( outing ) &&
            destination == outing.selected_watch_omt;
        // Frontier and homeward crossings commit one complete pair in one OMT.
        // Selecting adjacent exits on opposite sides of an OMT boundary leaves
        // both local motors paused at slots the crossing consumer must refuse.
        const bool same_omt_exit_required = require_safe_routes ||
                                           bandit_live_world::structural_outing_uses_frontier_route( outing );
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
                    rl_dist( first_candidate.exit, second_candidate.exit ) > 1 ||
                    ( watch_arrival_crossing &&
                      ( project_to<coords::omt>( first_candidate.exit ) != destination ||
                        project_to<coords::omt>( second_candidate.exit ) != destination ) ) ||
                    ( same_omt_exit_required && project_to<coords::omt>( first_candidate.exit ) !=
                      project_to<coords::omt>( second_candidate.exit ) ) ) {
                    // Keep the existing complete-pair crossing contract.
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
            // Occupancy can rule out an endpoint which was previously accepted.
            // Rank safe pairs by the same route/proximity score as the ordinary
            // selector, so two actors already at legal slots do not swap forever.
            std::stable_sort( complete_pairs.begin(), complete_pairs.end(),
            [&route_rank, first, second]( const auto &lhs, const auto &rhs ) {
                const auto score = [&route_rank, first, second]( const auto &pair ) {
                    const auto first_rank = *route_rank( *first, project_to<coords::omt>( pair.first.exit ) );
                    const auto second_rank = *route_rank( *second, project_to<coords::omt>( pair.second.exit ) );
                    const int first_distance = rl_dist( first->pos_abs(), pair.first.departure );
                    const int second_distance = rl_dist( second->pos_abs(), pair.second.departure );
                    return std::make_tuple( std::max( first_rank, second_rank ), first_rank + second_rank,
                                           std::max( first_distance, second_distance ), first_distance + second_distance,
                                           pair.first.departure, pair.second.departure );
                };
                return score( lhs ) < score( rhs );
            } );
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

// The saved elevated-source outing has no selected watch yet.  Its approach can
// be beyond the fixed player bubble, so choose a *reachable adjacent* OMT for
// the complete pair rather than asking the ordinary NPC route for an off-map
// center.  The watch remains uncredited until the two persisted actors really
// reach the approach and then the selected watch.
std::map<character_id, live_bandit_pair_boundary_step>
live_bandit_elevated_recovery_boundary_steps(
    const live_bandit_elevated_recovery_orders &orders )
{
    std::map<character_id, live_bandit_pair_boundary_step> result;
    map &here = get_map();
    for( const bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        const auto &outing = site.active_outing;
        if( !bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        const auto first_order = orders.find( outing.member_ids[0] );
        const auto second_order = orders.find( outing.member_ids[1] );
        if( first_order == orders.end() || second_order == orders.end() ||
            first_order->second.approach != second_order->second.approach ) {
            continue;
        }
        const live_bandit_elevated_recovery_order &order = first_order->second;
        if( here.inbounds( project_to<coords::ms>( order.approach ) + point( SEEX, SEEY ) ) ) {
            continue;
        }
        npc *first = g->find_npc( outing.member_ids[0] );
        npc *second = g->find_npc( outing.member_ids[1] );
        if( first == nullptr || second == nullptr || first->is_dead() || second->is_dead() ||
            !first->is_active() || !second->is_active() ) {
            continue;
        }
        const int first_distance = rl_dist( first->pos_abs_omt(), order.physical_target );
        const int second_distance = rl_dist( second->pos_abs_omt(), order.physical_target );
        if( first_distance < 2 || second_distance < 2 ) {
            continue;
        }
        struct candidate {
            tripoint_abs_ms departure;
            tripoint_abs_ms exit;
            tripoint_abs_omt exit_omt;
            std::size_t overmap_steps;
        };
        std::vector<candidate> candidates;
        std::map<tripoint_abs_omt, std::optional<std::size_t>> route_lengths;
        for_each_live_bandit_loaded_perimeter( here, order.approach.z(),
        [&]( const tripoint_bub_ms &point ) {
            if( !here.passable( point ) ||
                ( !g->is_empty( point ) && point != first->pos_bub() &&
                  point != second->pos_bub() ) ) {
                return;
            }
            const tripoint_abs_ms departure = here.get_abs( point );
            for( int dy = -1; dy <= 1; ++dy ) {
                for( int dx = -1; dx <= 1; ++dx ) {
                    if( dx == 0 && dy == 0 ) {
                        continue;
                    }
                    const tripoint_abs_ms exit = departure + point_rel_ms( dx, dy );
                    if( here.inbounds( exit ) ) {
                        continue;
                    }
                    const tripoint_abs_omt exit_omt = project_to<coords::omt>( exit );
                    if( exit_omt == first->pos_abs_omt() ||
                        rl_dist( exit_omt, order.physical_target ) <
                        std::min( first_distance, second_distance ) ) {
                        continue;
                    }
                    auto found = route_lengths.find( exit_omt );
                    if( found == route_lengths.end() ) {
                        const auto path = overmap_buffer.get_travel_path(
                                              exit_omt, order.approach,
                                              overmap_path_params::for_npc() ).points;
                        const bool valid = !path.empty() && path.front() == order.approach &&
                                           path.back() == exit_omt &&
                                           std::all_of( path.begin(), path.end(), [&order, &first_distance,
                        &second_distance]( const tripoint_abs_omt & omt ) {
                            return omt.z() == order.approach.z() &&
                                   rl_dist( omt, order.physical_target ) >=
                                   std::min( first_distance, second_distance );
                        } );
                        found = route_lengths.emplace( exit_omt,
                                                       valid ? std::optional<std::size_t>( path.size() ) :
                                                       std::nullopt ).first;
                    }
                    if( found->second ) {
                        candidates.push_back( { departure, exit, exit_omt, *found->second } );
                    }
                }
            }
        } );
        std::sort( candidates.begin(), candidates.end(),
        [&order, first, second]( const candidate &lhs, const candidate &rhs ) {
            const auto score = [&order, first, second]( const candidate &value ) {
                return std::make_tuple( value.exit_omt == order.watch,
                                        value.overmap_steps,
                                        std::min( rl_dist( first->pos_abs(), value.departure ),
                                                  rl_dist( second->pos_abs(), value.departure ) ),
                                        value.departure, value.exit );
            };
            return score( lhs ) < score( rhs );
        } );
        const auto relationship_for = [&order]( const int distance ) {
            return bandit_live_world::covert_scout_relationship_read{
                bandit_live_world::scout_phase::observing, { order.physical_target },
                order.approach, distance, {}, false
            };
        };
        const auto first_relationship = relationship_for( first_distance );
        const auto second_relationship = relationship_for( second_distance );
        // Preserve the sorted first-safe-pair order while joining only exits
        // that can be adjacent.  At most the eight neighboring coordinates
        // need inspection for each candidate, regardless of perimeter size.
        std::map<tripoint_abs_ms, std::vector<std::size_t>> candidates_by_exit;
        for( std::size_t index = 0; index < candidates.size(); ++index ) {
            candidates_by_exit[candidates[index].exit].push_back( index );
        }
        std::map<tripoint_abs_ms, live_bandit_safe_local_route_read> first_routes;
        std::map<tripoint_abs_ms, live_bandit_safe_local_route_read> second_routes;
        const auto safe_route = [&here]( npc &member,
        const bandit_live_world::covert_scout_relationship_read &relationship,
        const tripoint_abs_ms &departure,
        std::map<tripoint_abs_ms, live_bandit_safe_local_route_read> &cache ) {
            const auto found = cache.find( departure );
            if( found != cache.end() ) {
                return found->second.safe;
            }
            return cache.emplace( departure, live_bandit_safe_local_route_to(
                                      member, relationship, here, departure ) ).first->second.safe;
        };
        for( const candidate &a : candidates ) {
            if( !safe_route( *first, first_relationship, a.departure, first_routes ) ) {
                continue;
            }
            std::vector<std::size_t> neighbors;
            for( int dy = -1; dy <= 1; ++dy ) {
                for( int dx = -1; dx <= 1; ++dx ) {
                    if( dx == 0 && dy == 0 ) {
                        continue;
                    }
                    const tripoint_abs_ms adjacent = a.exit + point_rel_ms( dx, dy );
                    const auto found = candidates_by_exit.find( adjacent );
                    if( found != candidates_by_exit.end() ) {
                        neighbors.insert( neighbors.end(), found->second.begin(), found->second.end() );
                    }
                }
            }
            std::sort( neighbors.begin(), neighbors.end() );
            for( const std::size_t index : neighbors ) {
                const candidate &b = candidates[index];
                if( a.exit_omt != b.exit_omt || a.departure == b.departure ||
                    a.exit == b.exit || rl_dist( a.exit, b.exit ) > 1 ||
                    !safe_route( *second, second_relationship, b.departure, second_routes ) ) {
                    continue;
                }
                result.emplace( first->getID(), live_bandit_pair_boundary_step{
                    a.departure, a.exit
                } );
                result.emplace( second->getID(), live_bandit_pair_boundary_step{
                    b.departure, b.exit
                } );
                break;
            }
            if( result.count( first->getID() ) != 0 ) {
                break;
            }
        }
    }
    return result;
}

std::map<character_id, live_bandit_pair_boundary_step>
live_bandit_single_homeward_boundary_steps(
    const std::map<character_id, tripoint_abs_omt> &destinations )
{
    std::map<character_id, live_bandit_pair_boundary_step> result;
    map &here = get_map();
    for( const bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        std::vector<character_id> survivors;
        for( const character_id member_id : outing.member_ids ) {
            if( outing.member_is_resolved( member_id ) ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                outing.casualty_ids.end() ) {
                continue;
            }
            survivors.push_back( member_id );
        }
        if( survivors.size() != 1 || outing.casualty_ids.size() != 1 ||
            !outing.member_is_resolved( outing.casualty_ids.front() ) ) {
            continue;
        }
        const auto destination = destinations.find( survivors.front() );
        npc *survivor = g->find_npc( survivors.front() );
        if( destination == destinations.end() || survivor == nullptr || survivor->is_dead() ||
            !survivor->is_active() ) {
            continue;
        }
        const tripoint_abs_ms destination_center =
            project_to<coords::ms>( destination->second ) + point( SEEX, SEEY );
        if( here.inbounds( destination_center ) ) {
            continue;
        }
        const std::optional<bandit_live_world::covert_scout_relationship_read> relationship =
            bandit_live_world::read_active_covert_scout_homeward_member(
                overmap_buffer.global_state.bandit_live_world, survivor->getID() );
        if( !relationship ) {
            continue;
        }
        struct candidate {
            tripoint_abs_ms departure;
            tripoint_abs_ms exit;
        };
        std::optional<candidate> selected;
        std::tuple<std::size_t, int, tripoint_abs_ms> selected_score;
        for( const tripoint_bub_ms &point : here.points_on_zlevel( destination->second.z() ) ) {
            const tripoint_abs_ms departure = here.get_abs( point );
            if( !here.passable( point ) ||
                ( !g->is_empty( point ) && departure != survivor->pos_abs() ) ) {
                continue;
            }
            for( int dy = -1; dy <= 1; ++dy ) {
                for( int dx = -1; dx <= 1; ++dx ) {
                    if( dx == 0 && dy == 0 ) {
                        continue;
                    }
                    const tripoint_abs_ms exit = departure + point_rel_ms( dx, dy );
                    if( here.inbounds( exit ) ) {
                        continue;
                    }
                    const tripoint_abs_omt exit_omt = project_to<coords::omt>( exit );
                    std::optional<std::size_t> route_rank;
                    std::size_t rank = 0;
                    for( auto route = survivor->omt_path.rbegin();
                         route != survivor->omt_path.rend(); ++route ) {
                        if( *route == survivor->pos_abs_omt() ) {
                            continue;
                        }
                        ++rank;
                        if( *route == exit_omt ) {
                            route_rank = rank;
                            break;
                        }
                    }
                    if( !route_rank && exit_omt == survivor->pos_abs_omt() ) {
                        route_rank = 0;
                    }
                    if( !route_rank || !live_bandit_safe_local_route_to(
                            *survivor, *relationship, here, departure ).safe ) {
                        continue;
                    }
                    const auto score = std::make_tuple( *route_rank,
                                       rl_dist( survivor->pos_abs(), departure ), departure );
                    if( !selected || score < selected_score ) {
                        selected = { departure, exit };
                        selected_score = score;
                    }
                }
            }
        }
        if( selected ) {
            result.emplace( survivor->getID(), live_bandit_pair_boundary_step{
                selected->departure, selected->exit
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
        [&homeward_destinations]() {
            std::map<character_id, live_bandit_pair_boundary_step> result =
                live_bandit_pair_boundary_steps( homeward_destinations, nullptr, true );
            const std::map<character_id, live_bandit_pair_boundary_step> single_steps =
                live_bandit_single_homeward_boundary_steps( homeward_destinations );
            result.insert( single_steps.begin(), single_steps.end() );
            return result;
        }();
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
        if( outing.member_ids.size() != 2 ||
            bandit_live_world::structural_outing_uses_frontier_route( outing ) ) {
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
        // Reaching a slot through native flight/combat is not permission to
        // consume the scout order or unload a surviving actor. Match the
        // local motor's survival priority before applying the pair crossing.
        const std::array<const npc *, 2> members = { first, second };
        if( std::any_of( members.begin(), members.end(), []( const npc *member ) {
            return !live_bandit_member_can_take_homeward_step( *member ) ||
                   member->get_attitude() == NPCATT_FLEE ||
                   member->get_attitude() == NPCATT_FLEE_TEMP ||
                   member->has_active_faction_alarm() ||
                   ( member->current_target() != nullptr && member->get_ai_danger() > 0 );
        } ) ) {
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
        if( commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::commit_scout_pair_watch_arrival( next, *cursor,
                live_bandit_current_minutes(), reads );
        } ) !=
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

std::set<character_id> complete_live_bandit_outward_boundary_steps(
    const std::map<character_id, live_bandit_pair_boundary_step> &steps )
{
    std::set<character_id> crossed;
    map &here = get_map();
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        const auto &outing = site.active_outing;
        const bool frontier = bandit_live_world::structural_outing_uses_frontier_route( outing );
        if( ( !bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) &&
              !bandit_live_world::structural_outing_uses_frontier_route( outing ) ) ||
            outing.member_ids.size() != 2 ) {
            continue;
        }
        const auto a = steps.find( outing.member_ids[0] );
        const auto b = steps.find( outing.member_ids[1] );
        npc *first = g->find_npc( outing.member_ids[0] );
        npc *second = g->find_npc( outing.member_ids[1] );
        if( a == steps.end() || b == steps.end() || first == nullptr || second == nullptr ||
            first->is_dead() || second->is_dead() ||
            ( frontier && ( first->in_sleep_state() || second->in_sleep_state() ||
                            first->has_active_faction_alarm() || second->has_active_faction_alarm() ) ) ||
            first->get_attitude() == NPCATT_FLEE || second->get_attitude() == NPCATT_FLEE ||
            first->get_attitude() == NPCATT_FLEE_TEMP ||
            second->get_attitude() == NPCATT_FLEE_TEMP ||
            first->has_flag( json_flag_CANNOT_MOVE ) ||
            second->has_flag( json_flag_CANNOT_MOVE ) ||
            first->pos_abs() != a->second.departure ||
            second->pos_abs() != b->second.departure ||
            here.inbounds( a->second.exit ) || here.inbounds( b->second.exit ) ||
            a->second.exit == b->second.exit ||
            project_to<coords::omt>( a->second.exit ) !=
            project_to<coords::omt>( b->second.exit ) ) {
            continue;
        }
        const auto cursor = bandit_live_world::current_external_simulation_cursor( site );
        shared_ptr_fast<npc> stored_first = overmap_buffer.find_npc( first->getID() );
        shared_ptr_fast<npc> stored_second = overmap_buffer.find_npc( second->getID() );
        if( !cursor || !stored_first || !stored_second || stored_first->is_dead() ||
            stored_second->is_dead() ||
            rl_dist( a->second.exit, b->second.exit ) >
            bandit_live_world::local_pair_cohesion_radius() ) {
            continue;
        }
        const character_id first_id = first->getID();
        const character_id second_id = second->getID();
        stored_first = overmap_buffer.remove_npc( first_id );
        stored_second = overmap_buffer.remove_npc( second_id );
        if( !stored_first || !stored_second ) {
            if( stored_first ) {
                overmap_buffer.insert_npc( stored_first );
            }
            if( stored_second ) {
                overmap_buffer.insert_npc( stored_second );
            }
            continue;
        }
        const auto unload = [&here, frontier]( npc &active, npc &stored,
        const live_bandit_pair_boundary_step & step ) {
            if( active.in_vehicle ) {
                here.unboard_vehicle( active.pos_bub( here ) );
            }
            active.setpos( step.exit, false );
            if( frontier ) {
                // The physical one-tile crossing consumed the route up to this OMT.
                // Retaining the departure OMT at the back sends the pair into the
                // loaded map again before its outer visit can progress.
                const auto reached = std::find( active.omt_path.begin(), active.omt_path.end(),
                                               active.pos_abs_omt() );
                if( reached != active.omt_path.end() ) {
                    active.omt_path.erase( reached + 1, active.omt_path.end() );
                }
            }
            active.on_unload();
            if( &stored != &active ) {
                stored.spawn_at_precise( step.exit );
                stored.goal = active.goal;
                stored.omt_path = active.omt_path;
                stored.mission = active.mission;
                stored.previous_mission = active.previous_mission;
                stored.goto_to_this_pos = active.goto_to_this_pos;
                stored.path = active.path;
                stored.on_unload();
            }
            g->remove_npc( active.getID() );
        };
        unload( *first, *stored_first, a->second );
        unload( *second, *stored_second, b->second );
        overmap_buffer.insert_npc( stored_first );
        overmap_buffer.insert_npc( stored_second );
        crossed.insert( first_id );
        crossed.insert( second_id );
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world elevated recovery pair crossed"
                                   << " site=" << site.site_id
                                   << " generation=" << outing.generation
                                   << " members=" << first_id.get_value() << ','
                                   << second_id.get_value()
                                   << " exit_omt=" << project_to<coords::omt>( a->second.exit )
                                   << '\n';
    }
    return crossed;
}

void record_live_bandit_persistent_elevated_watch_arrivals()
{
    map &here = get_map();
    for( bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        const auto &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.schema_version < 10 ||
            outing.owner != bandit_live_world::simulation_owner::local ||
            outing.phase != bandit_live_world::scout_phase::observing ||
            outing.target_omt.z() <= site.anchor.z() || outing.waypoint_index != 1 ||
            outing.member_ids.size() != 2 || !outing.casualty_ids.empty() ||
            !outing.local_handoff.is_active() ||
            outing.local_handoff.route_position != outing.shared_route[1] ||
            outing.local_handoff.egress_omt != outing.selected_watch_omt ) {
            continue;
        }
        std::vector<bandit_live_world::local_route_arrival_member_read> reads;
        for( const character_id id : outing.member_ids ) {
            const auto member = overmap_buffer.find_npc( id );
            if( !member || member->is_active() || member->is_dead() ||
                here.inbounds( member->pos_abs() ) ||
                member->pos_abs_omt() != outing.selected_watch_omt ) {
                break;
            }
            reads.push_back( { id, true, false, true,
                               member->hp_percentage(), member->pos_abs() } );
        }
        const auto cursor = bandit_live_world::current_external_simulation_cursor( site );
        if( reads.size() != 2 || !cursor ||
        commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
        return bandit_live_world::commit_scout_pair_watch_arrival( next, *cursor,
                live_bandit_current_minutes(), reads );
        } ) !=
        bandit_live_world::local_handoff_commit_result::applied ) {
            continue;
        }
        site.active_outing.actor_route_waypoint = site.active_outing.waypoint_index;
        site.active_outing.actor_departure_observed = true;
    }
}

std::set<character_id> complete_live_bandit_homeward_boundary_steps(
        const std::map<character_id, live_bandit_pair_boundary_step> &steps )
{
    std::set<character_id> committed_member_ids;
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    map &here = get_map();
    for( bandit_live_world::site_record &site : state.sites ) {
        bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        auto homeward_event = make_live_bandit_homeward_event(
                                  site, "scout_homeward_dematerialization", "selected_boundary" );
        std::vector<std::pair<npc *, live_bandit_pair_boundary_step>> crossings;
        std::string collection_reason = "complete";
        for( const character_id member_id : outing.member_ids ) {
            if( outing.member_is_resolved( member_id ) ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                outing.casualty_ids.end() ) {
                continue;
            }
            const auto step = steps.find( member_id );
            npc *member = g->find_npc( member_id );
            if( member == nullptr ) {
                const shared_ptr_fast<npc> stored_member = overmap_buffer.find_npc( member_id );
                member = stored_member ? stored_member.get() : nullptr;
            }
            if( step == steps.end() || member == nullptr ||
                member->pos_abs() != step->second.departure ) {
                const tripoint_abs_ms destination_center =
                    project_to<coords::ms>( site.anchor ) + point( SEEX, SEEY );
                collection_reason = step == steps.end() ?
                                    ( member != nullptr && here.inbounds( destination_center ) ?
                                      "destination_in_loaded_bubble" : "step_missing" ) :
                                    member == nullptr ? "npc_missing" : "not_at_departure";
                crossings.clear();
                break;
            }
            if( !live_bandit_member_can_take_homeward_step( *member ) ) {
                collection_reason = "member_incapacitated";
                crossings.clear();
                break;
            }
            crossings.emplace_back( member, step->second );
        }
        if( homeward_event ) {
            for( auto &member : homeward_event->scout_homeward->members ) {
                const auto step = steps.find( character_id( member.npc_id ) );
                member.boundary_selected = step != steps.end();
                if( step != steps.end() ) {
                    member.boundary_departure_ms = step->second.departure.to_string();
                    member.transfer_position_ms = step->second.exit.to_string();
                }
            }
        }
        if( crossings.empty() || crossings.size() + outing.casualty_ids.size() !=
            outing.member_ids.size() ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward boundary collection"
                                       << " site=" << site.site_id
                                       << " activity=" << outing.activity_id
                                       << " generation=" << outing.generation
                                       << " steps=" << steps.size()
                                       << " crossings=" << crossings.size()
                                       << " casualties=" << outing.casualty_ids.size()
                                       << " members=" << outing.member_ids.size()
                                       << " reason=" << ( crossings.empty() ? collection_reason :
                                               "incomplete_pair" ) << '\n';
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                  crossings.empty() ? collection_reason : "incomplete_pair", false );
            continue;
        }
        if( !persist_live_bandit_local_projection_leases( site, &site, false ) ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                  "actor projection copies do not match the current local cursor", false );
            continue;
        }
        const tripoint_abs_omt boundary_omt = project_to<coords::omt>( crossings.front().second.exit );
        const int current_minutes = live_bandit_current_minutes();
        std::set<character_id> site_committed_member_ids;
        if( outing.kind == bandit_live_world::outing_kind::scout_sortie ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "legacy generic owner transaction selected", false );
            // A legacy scout owns the same physical exit, but has no structural
            // local-handoff snapshot.  Cross the selected adjacent boundary for
            // the complete pair, then use its existing generic owner transaction.
            // Leaving the bubble is not a receipt of arrival at home.
            const bool ready = outing.owner == bandit_live_world::simulation_owner::local &&
                               !outing.crossing.pending() && crossings.size() == 2 &&
                               std::all_of( crossings.begin(), crossings.end(),
            [&site, &here]( const auto &crossing ) {
                const auto stored = overmap_buffer.find_npc( crossing.first->getID() );
                return stored && stored.get() == crossing.first && !stored->is_dead() &&
                       !here.inbounds( crossing.second.exit ) &&
                       live_bandit_member_routing_home( *stored, site );
            } );
            if( !ready ) {
                continue;
            }
            for( const auto &crossing : crossings ) {
                const character_id id = crossing.first->getID();
                auto member = overmap_buffer.remove_npc( id );
                if( member->in_vehicle ) {
                    here.unboard_vehicle( member->pos_bub( here ) );
                }
                member->setpos( crossing.second.exit, false );
                member->guard_pos.reset();
                member->clear_ai_guard_pos();
                member->goto_to_this_pos.reset();
                member->path.clear();
                member->on_unload();
                g->remove_npc( id );
                overmap_buffer.insert_npc( member );
                site_committed_member_ids.insert( id );
            }
            handoff_live_bandit_generic_scout_returns();
            committed_member_ids.insert( site_committed_member_ids.begin(), site_committed_member_ids.end() );
            continue;
        }
        if( site_contains_omt( site, boundary_omt ) ) {
            record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                 "physical camp-return recorder selected", false );
            for( const std::pair<npc *, live_bandit_pair_boundary_step> &crossing : crossings ) {
                npc *member = crossing.first;
                member->setpos( crossing.second.exit, false );
                const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                    bandit_live_world::current_external_simulation_cursor( site );
                if( !cursor ||
                !commit_live_bandit_local_progress( site, [&]( bandit_live_world::site_record & next ) {
                return bandit_live_world::record_structural_member_physical_return( next, *cursor, member->getID(),
                        boundary_omt, current_minutes );
                } ) ) {
                    continue;
                }
                // The active NPC and the overmap record are separate instances.
                // A completed in-site return must normalize the persisted record
                // too, otherwise its old local lease survives the save after the
                // global outing has already been finalized.
                shared_ptr_fast<npc> persistent_member = overmap_buffer.remove_npc( member->getID() );
                if( persistent_member ) {
                    persistent_member->spawn_at_precise( member->pos_abs() );
                    sync_bandit_live_world_projection_lease_copies(
                        *persistent_member, bandit_live_world_projection_lease() );
                    persistent_member->on_unload();
                    overmap_buffer.insert_npc( persistent_member );
                } else {
                    DebugLog( D_ERROR, DC_ALL ) << "bandit_live_world local return lost persistent member"
                                               << " member=" << member->getID().get_value();
                }
                member->on_unload();
                g->remove_npc( member->getID() );
                site_committed_member_ids.insert( member->getID() );
            }
        } else {
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            if( !cursor ) {
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward boundary rejected"
                                           << " site=" << site.site_id
                                           << " activity=" << outing.activity_id
                                           << " generation=" << outing.generation
                                           << " reason=missing_cursor state=unchanged\n";
                record_live_bandit_homeward_transfer( homeward_event, site, "not_invoked",
                                                     "missing_cursor", false );
                continue;
            }
            std::vector<bandit_live_world::local_dematerialization_member_read> reads;
            std::vector<live_bandit_local_handoff_member_backup> backups;
            reads.reserve( crossings.size() );
            backups.reserve( crossings.size() );
            for( const std::pair<npc *, live_bandit_pair_boundary_step> &crossing : crossings ) {
                npc *member = crossing.first;
                reads.push_back( { member->getID(), true, false, true, member->hp_percentage(),
                                   crossing.second.exit } );
                backups.push_back( { overmap_buffer.find_npc( member->getID() ), member->pos_abs(),
                                     member->goal, member->omt_path, member->mission,
                                     member->previous_mission, member->goto_to_this_pos,
                                     member->get_ai_guard_pos(), member->path, {} } );
                if( backups.back().member ) {
                    backups.back().projection_lease =
                        backups.back().member->get_bandit_live_world_projection_lease();
                }
            }
            // The dead member's retained position completes the same stable pair
            // transaction; only the surviving actor crosses this actual boundary.
            for( const auto &snapshot : outing.local_handoff.members ) {
                if( snapshot.dead ) {
                    reads.push_back( { snapshot.npc_id, true, true, true, 0, snapshot.exit_position } );
                }
            }
            const bandit_live_world::local_dematerialization_plan plan =
                bandit_live_world::plan_local_pair_dematerialization(
                    site, *cursor, current_minutes, reads, outing.cargo );
            if( !plan.valid ) {
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward boundary rejected"
                                           << " site=" << site.site_id
                                           << " activity=" << outing.activity_id
                                           << " generation=" << outing.generation
                                           << " reason=dematerialization_plan state=unchanged"
                                           << " detail=" << ( plan.notes.empty() ? "no-diagnostic" :
                                                   plan.notes.front() ) << '\n';
                record_live_bandit_homeward_transfer( homeward_event, site, "rejected",
                                                     plan.notes.empty() ? "invalid dematerialization plan returned without a note" :
                                                     plan.notes.front(), true, false );
                continue;
            }
            const auto find_backup = [&backups]( const character_id member_id ) {
                return std::find_if( backups.begin(), backups.end(),
                [&member_id]( const live_bandit_local_handoff_member_backup & backup ) {
                    return backup.member && backup.member->getID() == member_id;
                } );
            };
            const auto find_crossing = [&crossings]( const character_id member_id ) {
                return std::find_if( crossings.begin(), crossings.end(),
                [&member_id]( const std::pair<npc *, live_bandit_pair_boundary_step> & crossing ) {
                    return crossing.first->getID() == member_id;
                } );
            };
            const auto clear_vehicle_state = [&here]( npc & member ) {
                if( member.in_vehicle ) {
                    const optional_vpart_position vehicle_part = here.veh_at( member.pos_bub( here ) );
                    const std::optional<vpart_reference> boardable_part = vehicle_part ?
                            vehicle_part.part_with_feature( VPFLAG_BOARDABLE, false ) : std::nullopt;
                    if( boardable_part && boardable_part->get_passenger() == &member ) {
                        here.unboard_vehicle( *boardable_part, &member );
                        return;
                    }
                }
                member.in_vehicle = false;
                member.controlling_vehicle = false;
            };
            const auto quiesce_member = [&find_backup, &find_crossing, &backups, &crossings,
                                                       &clear_vehicle_state, &site](
            const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
                if( snapshot.dead ) {
                    return true;
                }
                const auto backup = find_backup( snapshot.npc_id );
                const auto crossing = find_crossing( snapshot.npc_id );
                if( backup == backups.end() || crossing == crossings.end() ||
                    backup->member->is_dead() ||
                    backup->member->pos_abs() != crossing->second.departure ||
                    snapshot.exit_position != crossing->second.exit ) {
                    return false;
                }
                // The loaded reality-bubble NPC and the persistent overmap handle can be
                // distinct objects.  The persistent actor's owning overmap is determined only
                // when it is inserted, so remove it before relocating and reinsert it at the
                // committed exit.  Moving the pointer while it remains indexed leaves its
                // identity owned by the departure overmap.
                shared_ptr_fast<npc> persistent_member = overmap_buffer.remove_npc( snapshot.npc_id );
                if( !persistent_member || persistent_member != backup->member ) {
                    if( persistent_member ) {
                        overmap_buffer.insert_npc( persistent_member );
                    }
                    return false;
                }
                if( !live_bandit_projection_copies_match( *persistent_member,
                        live_bandit_projection_lease( site.site_id, site.active_outing ) ) ) {
                    overmap_buffer.insert_npc( persistent_member );
                    return false;
                }
                clear_vehicle_state( *crossing->first );
                crossing->first->setpos( crossing->second.exit, false );
                clear_vehicle_state( *persistent_member );
                persistent_member->spawn_at_precise( crossing->second.exit );
                // The reality-bubble and overmap instances are distinct objects.  Preserve
                // the active instance's authoritative homeward motor when handing identity
                // back to the overmap store; otherwise the persisted copy loses its travel
                // mission and the abstract cursor stalls after a rejected materialization.
                persistent_member->goal = crossing->first->goal;
                persistent_member->omt_path = crossing->first->omt_path;
                persistent_member->mission = crossing->first->mission;
                persistent_member->previous_mission = crossing->first->previous_mission;
                persistent_member->goto_to_this_pos = crossing->first->goto_to_this_pos;
                if( crossing->first->get_ai_guard_pos() ) {
                    persistent_member->set_ai_guard_pos( *crossing->first->get_ai_guard_pos() );
                } else {
                    persistent_member->clear_ai_guard_pos();
                }
                persistent_member->path = crossing->first->path;
                sync_bandit_live_world_projection_lease_copies(
                    *persistent_member, bandit_live_world_projection_lease() );
                persistent_member->on_unload();
                // The active reality-bubble NPC is distinct from the persistent
                // overmap record.  Clear the completed local lease on both before
                // removing the active instance, so a later persistence pass cannot
                // resurrect an ownership claim for an outing that is now abstract.
                g->remove_npc( snapshot.npc_id );
                for( const npc &remaining : g->all_npcs() ) {
                    if( remaining.getID() == snapshot.npc_id ) {
                        return false;
                    }
                }
                overmap_buffer.insert_npc( persistent_member );
                const shared_ptr_fast<npc> indexed_member = overmap_buffer.find_npc( snapshot.npc_id );
                return indexed_member == persistent_member && !indexed_member->is_active() &&
                       indexed_member->pos_abs() == crossing->second.exit;
            };
            const auto rollback_member = [&find_backup, &backups](
            const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
                const auto backup = find_backup( snapshot.npc_id );
                if( backup == backups.end() ) {
                    return;
                }
                shared_ptr_fast<npc> persistent_member = overmap_buffer.remove_npc( snapshot.npc_id );
                if( !persistent_member || persistent_member != backup->member ) {
                    if( persistent_member ) {
                        overmap_buffer.insert_npc( persistent_member );
                    }
                    return;
                }
                persistent_member->spawn_at_precise( backup->position );
                restore_live_bandit_local_projection( *backup );
                overmap_buffer.insert_npc( persistent_member );
                if( get_map().inbounds( backup->position ) && !persistent_member->is_active() ) {
                    g->load_npcs();
                }
            };
            const bandit_live_world::local_handoff_commit_result commit_result =
                bandit_live_world::commit_local_pair_dematerialization(
                    site, plan, quiesce_member, rollback_member );
            record_live_bandit_homeward_transfer( homeward_event, site,
                                                 commit_result == bandit_live_world::local_handoff_commit_result::applied ? "committed" :
                                                 live_bandit_homeward_commit_result( commit_result ),
                                                 "commit_local_pair_dematerialization returned " +
                                                 std::string( live_bandit_homeward_commit_result( commit_result ) ), true, true,
                                                 live_bandit_homeward_commit_result( commit_result ) );
            if( commit_result != bandit_live_world::local_handoff_commit_result::applied ) {
                DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward boundary rejected"
                                           << " site=" << site.site_id
                                           << " activity=" << outing.activity_id
                                           << " generation=" << outing.generation
                                           << " reason=dematerialization_commit"
                                           << " result=" << static_cast<int>( commit_result )
                                           << " state=unchanged\n";
                continue;
            }
            for( const std::pair<npc *, live_bandit_pair_boundary_step> &crossing : crossings ) {
                site_committed_member_ids.insert( crossing.first->getID() );
            }
        }
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world local homeward boundary committed"
                                   << " site=" << site.site_id
                                   << " activity=" << site.active_outing.activity_id
                                   << " generation=" << site.active_outing.generation
                                   << " route_position=" << boundary_omt.to_string()
                                   << " destination=" << site.anchor.to_string()
                                   << " members=" << crossings.size() << '\n';
        committed_member_ids.insert( site_committed_member_ids.begin(),
                                     site_committed_member_ids.end() );
    }
    return committed_member_ids;
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
            bandit_live_world_probe::record_fixture_monster_lifecycle( critter, "monmove_before", "local" );
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
            bandit_live_world_probe::record_fixture_monster_lifecycle( critter, "move_after", "local" );
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
        bandit_live_world_probe::record_fixture_monster_lifecycle( critter,
                "post_monmove_visibility", "local" );
    }

    g->cleanup_dead();

    // The remaining monsters are all alive, but may be outside of the reality bubble.
    // If so, despawn them. This is not the same as dying, they will be stored for later and the
    // monster::die function is not called.
    g->despawn_nonlocal_monsters();

    // Wake owned ordinary sleep before either pair selection or scheduler
    // exclusion. Forced incapacity and flight remain native survival facts.
    for( npc &guy : g->all_npcs() ) {
        guy.reconcile_active_operation_sleep();
    }

    // Now, do active NPCs.  Cohesion owns the first local cursor advance so
    // evidence recording can never delay an incomplete pair's safety update.
    std::map<character_id, tripoint_abs_ms> pair_assembly_orders;
    std::set<character_id> pair_homeward_travel_ids;
    std::map<character_id, character_id> pair_homeward_partner_ids;
    std::map<character_id, std::string> pair_homeward_site_ids;
    std::map<character_id, tripoint_abs_omt> pair_ingress_travel_destinations;
    std::map<character_id, live_bandit_pair_boundary_step> pair_ingress_boundary_steps;
    std::map<character_id, live_bandit_pair_boundary_step> pair_standard_boundary_steps;
    std::map<character_id, live_bandit_pair_boundary_step> pair_recovery_boundary_steps;
    live_bandit_elevated_recovery_orders elevated_recovery_orders;
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
        elevated_recovery_orders = recover_live_bandit_local_elevated_watches();
        record_live_bandit_persistent_elevated_watch_arrivals();
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
                pair_assembly_orders = maintain_live_bandit_local_pair_cohesion();
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
        pair_standard_boundary_steps = live_bandit_pair_boundary_steps(
                                           pair_ingress_travel_destinations );
        pair_ingress_boundary_steps = pair_standard_boundary_steps;
        pair_recovery_boundary_steps = live_bandit_elevated_recovery_boundary_steps(
                                           elevated_recovery_orders );
        for( const auto &order : elevated_recovery_orders ) {
            pair_ingress_travel_destinations.emplace( order.first, order.second.approach );
        }
        pair_ingress_boundary_steps.insert( pair_recovery_boundary_steps.begin(),
                                            pair_recovery_boundary_steps.end() );
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
                for( const character_id id : site.active_outing.member_ids ) {
                    pair_homeward_site_ids.emplace( id, site.site_id );
                }
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
        const std::map<character_id, live_bandit_pair_boundary_step> single_homeward_steps =
            live_bandit_single_homeward_boundary_steps( pair_homeward_destinations );
        pair_homeward_boundary_steps.insert( single_homeward_steps.begin(),
                                             single_homeward_steps.end() );
        if( bandit_live_world_probe::transition_events_enabled() ) {
            for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
                for( const character_id id : site.active_outing.member_ids ) {
                    auto event = make_live_bandit_homeward_event(
                                     site, "scout_homeward_selection", "local_npc_prepass" );
                    if( !event ) {
                        break;
                    }
                    const bool selected = pair_homeward_travel_ids.count( id ) > 0;
                    event->actor_ids = { id.get_value() };
                    event->outcome = selected ? "selected" : "not_selected";
                    event->reason = selected ? "local_pair_homeward_travel_ids selected actor" :
                                    "local_pair_homeward_travel_ids did not select actor";
                    auto member = read_live_bandit_homeward_actor( id );
                    member.selected = selected;
                    member.assembly_selected = pair_assembly_orders.count( id ) > 0;
                    member.ingress_selected = pair_ingress_travel_destinations.count( id ) > 0;
                    const auto step = pair_homeward_boundary_steps.find( id );
                    member.boundary_selected = step != pair_homeward_boundary_steps.end();
                    if( step != pair_homeward_boundary_steps.end() ) {
                        member.boundary_departure_ms = step->second.departure.to_string();
                        member.transfer_position_ms = step->second.exit.to_string();
                    }
                    event->scout_homeward->members = { std::move( member ) };
                    bandit_live_world_probe::record_scout_homeward_observation( std::move( *event ) );
                }
            }
        }
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
    // A selected homeward boundary is a physical ownership handoff, not an NPC
    // movement goal.  Commit it before ordinary NPC processing can consume or
    // replace the selected member's OMT route in this same turn.
    complete_live_bandit_homeward_boundary_steps( pair_homeward_boundary_steps );
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
        guy.reconcile_alarm_response();
        bool entered_action_loop = false;
        while( !guy.is_dead() && ( !guy.in_sleep_state() ||
                                   guy.activity.id() == ACT_OPERATION || guy.activity.id() == ACT_MIGRATION_CANCEL ) &&
               guy.get_moves() > 0 && turns < 10 ) {
            entered_action_loop = true;
            live_bandit_homeward_motor_trace homeward_trace(
                guy, pair_homeward_travel_ids.count( guy.getID() ) > 0 );
            const int moves = guy.get_moves();
            const bool has_destination = guy.has_destination_activity();
            const auto assembly_order = pair_assembly_orders.find( guy.getID() );
            const auto ingress_destination = pair_ingress_travel_destinations.find(
                                                 guy.getID() );
            const auto ingress_boundary = pair_ingress_boundary_steps.find( guy.getID() );
            const auto homeward_boundary = pair_homeward_boundary_steps.find( guy.getID() );
            const bool pair_motor = assembly_order != pair_assembly_orders.end() ||
                                    ingress_destination != pair_ingress_travel_destinations.end() ||
                                    pair_homeward_travel_ids.count( guy.getID() ) != 0;
            if( pair_motor ) {
                // Route ownership never replaces this individual's perception,
                // fear or combat decision.  Cache refresh spends no action.
                guy.regen_ai_cache();
            }
            const bool native_survival = pair_motor &&
                                        ( guy.get_attitude() == NPCATT_FLEE ||
                                          guy.get_attitude() == NPCATT_FLEE_TEMP ||
                                          guy.has_active_faction_alarm() ||
                                          ( guy.current_target() != nullptr && guy.get_ai_danger() > 0 ) );
            if( native_survival ) {
                homeward_trace.action( "npc_move", "native_survival branch selected" );
                guy.move();
            } else if( assembly_order != pair_assembly_orders.end() ) {
                homeward_trace.action( "assembly_order", "assembly order precedes homeward motor" );
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
                homeward_trace.action( "ingress_order", "ingress order precedes homeward motor" );
                std::optional<bandit_live_world::covert_scout_relationship_read>
                relationship = bandit_live_world::read_active_covert_scout_member(
                                   overmap_buffer.global_state.bandit_live_world, guy.getID() );
                const auto recovery = elevated_recovery_orders.find( guy.getID() );
                if( !relationship && recovery != elevated_recovery_orders.end() ) {
                    relationship = bandit_live_world::covert_scout_relationship_read{
                        bandit_live_world::scout_phase::observing,
                        { recovery->second.physical_target }, recovery->second.approach,
                        rl_dist( guy.pos_abs_omt(), recovery->second.physical_target ), {}, false
                    };
                }
                const auto local_path_respects_watch_ring = [relationship, &m, &guy](
                const std::vector<tripoint_bub_ms> &candidate_path ) {
                    const std::optional<int> start_distance = relationship ?
                            bandit_live_world::target_footprint_watch_distance(
                                guy.pos_abs_omt(), relationship->target_footprint ) :
                            std::nullopt;
                    const int minimum_distance = relationship && start_distance ?
                                                 std::min( relationship->minimum_target_distance,
                                                           *start_distance ) : 0;
                    return relationship && start_distance && std::all_of(
                               candidate_path.begin(), candidate_path.end(),
                    [relationship, &m, &guy, minimum_distance]( const tripoint_bub_ms & step ) {
                        const tripoint_abs_omt step_omt =
                            project_to<coords::omt>( m.get_abs( step ) );
                        const std::optional<int> distance =
                            bandit_live_world::target_footprint_watch_distance(
                                step_omt, relationship->target_footprint );
                        return distance && *distance >= minimum_distance &&
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
                    } else {
                        const bool route_found = live_bandit_update_local_path(
                                                     guy, m.get_bub(
                                                         ingress_boundary->second.departure ) );
                        const bool route_safe = route_found &&
                                                local_path_respects_watch_ring( guy.path );
                        if( route_safe ) {
                            guy.move_to_next();
                        } else {
                            guy.path.clear();
                            guy.move_pause();
                        }
                    }
                } else {
                    const auto avoid_watch_ring = [&local_path_respects_watch_ring](
                    const tripoint_bub_ms &step ) {
                        return !local_path_respects_watch_ring( { step } );
                    };
                    // The pair can materialize one OMT ring-unit inside the selected watch,
                    // and a tile-level boundary crossing may briefly remain at that starting
                    // distance.  Keep that non-inward corridor available while excluding every
                    // step closer to the target; the covert mover retains the native OMT mission
                    // and validates the prospective route before the arrival commit.
                    live_bandit_move_to_omt_destination_avoiding(
                        guy, local_path_respects_watch_ring, avoid_watch_ring );
                }
            } else if( pair_homeward_travel_ids.count( guy.getID() ) > 0 ) {
                const std::optional<bandit_live_world::covert_scout_relationship_read>
                relationship = bandit_live_world::read_active_covert_scout_homeward_member(
                                   overmap_buffer.global_state.bandit_live_world, guy.getID() );
                if( homeward_trace.read() ) {
                    homeward_trace.read()->relationship_present = relationship.has_value();
                }
                const auto partner_id = pair_homeward_partner_ids.find( guy.getID() );
                npc *partner = partner_id == pair_homeward_partner_ids.end() ? nullptr :
                               g->find_npc( partner_id->second );
                const auto homeward_site_id = pair_homeward_site_ids.find( guy.getID() );
                const auto *homeward_site = homeward_site_id == pair_homeward_site_ids.end() ? nullptr :
                                            overmap_buffer.global_state.bandit_live_world.find_site( homeward_site_id->second );
                const auto step_preserves_pair_cohesion = [&m, &guy, partner](
                const tripoint_bub_ms & step ) {
                    if( partner == nullptr ) {
                        return true;
                    }
                    const int current_distance = rl_dist( guy.pos_abs(), partner->pos_abs() );
                    const int next_distance = rl_dist( m.get_abs( step ), partner->pos_abs() );
                    // Ordinary homeward travel keeps the pair inside the hard radius.
                    // The complete reunion route below handles an earlier separation.
                    return next_distance <= bandit_live_world::local_pair_cohesion_radius() ||
                           next_distance < current_distance;
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
                    homeward_trace.action( "npc_move", "homeward relationship read returned no relationship" );
                    guy.move();
                } else if( guy.has_flag( json_flag_CANNOT_MOVE ) ) {
                    homeward_trace.action( "move_pause", "CANNOT_MOVE flag", "blocked" );
                    if( immediate_field_hazard && relationship->phase ==
                        bandit_live_world::scout_phase::burned_withdrawal ) {
                        bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                    }
                    if( adjacent_non_camp_threat &&
                        !guy.has_flag( json_flag_CANNOT_ATTACK ) ) {
                        homeward_trace.action( "melee_attack", "adjacent non-camp threat while movement prohibited" );
                        guy.melee_attack( *threat, true );
                    } else {
                        guy.move_pause();
                    }
                } else if( field_escape ) {
                    homeward_trace.action( "move_to", "immediate field hazard selected a clear escape" );
                    guy.move_to( *field_escape, true );
                } else if( forced_field_escape ) {
                    homeward_trace.action( "move_to", "immediate field hazard selected a forced escape" );
                    guy.move_to( *forced_field_escape, true, nullptr, true );
                } else if( adjacent_non_camp_threat &&
                           !guy.has_flag( json_flag_CANNOT_ATTACK ) ) {
                    // Defense follows immediate field survival but still precedes squad routing.
                    homeward_trace.action( "melee_attack", "adjacent non-camp threat selected" );
                    guy.melee_attack( *threat, true );
                } else if( partner != nullptr && !partner->is_dead() &&
                           ( rl_dist( guy.pos_abs(), partner->pos_abs() ) >
                             bandit_live_world::local_pair_cohesion_radius() ||
                             ( homeward_site != nullptr &&
                               homeward_site->active_outing.local_handoff.cohesion_abort_return &&
                               guy.pos_abs_omt() != partner->pos_abs_omt() ) ) ) {
                    const auto reunion = homeward_site ? live_bandit_homeward_reunion_path(
                                             guy, *partner, *homeward_site, m ) : live_bandit_homeward_reunion_route{};
                    if( reunion.mover != guy.getID() ) {
                        guy.path.clear();
                        homeward_trace.action( "move_pause", reunion.path.empty() ?
                                               "safe homeward reunion route unavailable" :
                                               "holding homeward rendezvous anchor", reunion.path.empty() ? "blocked" : "waiting" );
                        guy.move_pause();
                    } else {
                        guy.path = reunion.path;
                        if( auto *read = homeward_trace.read() ) {
                            read->route_found = true;
                            read->route_safe = true;
                            read->next_step_cohesive = step_preserves_pair_cohesion( guy.path.front() );
                            read->next_step_ms = m.get_abs( guy.path.front() ).to_string();
                            read->next_step_passable = m.passable_through( guy.path.front() );
                            read->next_step_occupied = get_creature_tracker().creature_at( guy.path.front() ) != nullptr;
                            read->next_step_dangerous = guy.sees_dangerous_field( guy.path.front() );
                        }
                        homeward_trace.action( "move_to_next", "safe homeward reunion route" );
                        guy.move_to_next();
                        if( rl_dist( guy.pos_abs(), partner->pos_abs() ) <=
                            bandit_live_world::local_pair_cohesion_radius() &&
                            guy.pos_abs_omt() == partner->pos_abs_omt() ) {
                            // Reunion may cross an OMT outside the actor's old homeward path.
                            // Rebind both routes from their real positions through the existing
                            // covert planner, without changing ownership or crediting a return.
                            // The relationship retains a required burned egress until
                            // its physical completion authorizes travel to camp.
                            const tripoint_abs_omt destination = relationship->egress_omt;
                            auto member_route = live_bandit_member_route_to( guy, *homeward_site, destination );
                            auto partner_route = live_bandit_member_route_to( *partner, *homeward_site, destination );
                            if( !member_route.empty() && !partner_route.empty() ) {
                                guy.goal = destination;
                                partner->goal = destination;
                                guy.omt_path = std::move( member_route );
                                partner->omt_path = std::move( partner_route );
                                guy.path.clear();
                                partner->path.clear();
                            }
                        }
                    }
                } else if( homeward_boundary != pair_homeward_boundary_steps.end() ) {
                    if( guy.pos_abs() == homeward_boundary->second.departure ) {
                        homeward_trace.action( "move_pause", "at selected boundary departure", "blocked" );
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
                        // The active local map owns this coordinate frame.  Discard every
                        // origin entry against that frame before evaluating or advancing the
                        // route, because npc::move_to_next otherwise spends this turn on the
                        // actor's current tile when a freshly solved route retains it.
                        while( !guy.path.empty() &&
                                ( guy.path.front() == guy.pos_bub( m ) ||
                                  m.get_abs( guy.path.front() ) == guy.pos_abs() ) ) {
                            guy.path.erase( guy.path.begin() );
                        }
                        const bool route_safe = route_found && !guy.path.empty() &&
                                                local_path_respects_nonreentry( guy.path );
                        const bool next_step_cohesive = partner == nullptr || guy.path.empty() ||
                                                        step_preserves_pair_cohesion(
                                                            guy.path.front() );
                        const std::size_t path_size_before_movement = guy.path.size();
                        const tripoint_abs_ms position_before_movement = guy.pos_abs();
                        const int moves_before_movement = guy.get_moves();
                        const std::optional<tripoint_bub_ms> next_step = guy.path.empty() ?
                                std::nullopt : std::optional<tripoint_bub_ms>( guy.path.front() );
                        Creature *const next_occupant = next_step ?
                                get_creature_tracker().creature_at( *next_step ) : nullptr;
                        const bool next_step_passable = next_step && m.passable_through( *next_step );
                        const bool next_step_moveable = next_step &&
                                                        guy.can_move_to_ignoring_danger(
                                                            *next_step, false );
                        const bool next_step_dangerous = next_step &&
                                                         guy.sees_dangerous_field( *next_step );
                        const bool movement_impaired = guy.has_effect( effect_downed ) ||
                                                      guy.has_effect( effect_stunned ) ||
                                                      guy.has_effect( effect_psi_stunned ) ||
                                                      guy.has_effect( effect_narcosis );
                        if( auto *read = homeward_trace.read() ) {
                            read->route_found = route_found;
                            read->route_safe = route_safe;
                            read->next_step_cohesive = next_step_cohesive;
                            if( next_step ) {
                                read->next_step_ms = m.get_abs( *next_step ).to_string();
                                read->next_step_passable = next_step_passable;
                                read->next_step_occupied = next_occupant != nullptr;
                                read->next_step_dangerous = next_step_dangerous;
                            }
                        }
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
                            homeward_trace.action( "move_to_next", "safe boundary route and cohesive next step" );
                            guy.move_to_next();
                        } else if( route_safe ) {
                            homeward_trace.action( "move_pause", "pair_cohesion", "blocked" );
                            guy.move_pause();
                        } else {
                            homeward_trace.action( "omt_fallback", "boundary route did not pass safety preflight" );
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
                                    << " next=" << ( next_step ? next_step->to_string() : "none" )
                                    << " next_passable=" << ( next_step_passable ? "yes" : "no" )
                                    << " next_moveable=" << ( next_step_moveable ? "yes" : "no" )
                                    << " next_dangerous=" << ( next_step_dangerous ? "yes" : "no" )
                                    << " next_occupied=" << ( next_occupant ? "yes" : "no" )
                                    << " movement_impaired=" << ( movement_impaired ? "yes" : "no" )
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
                        homeward_trace.action( "none", "homeward route binding gate did not pass", "blocked" );
                        break;
                    }
                    const tripoint_abs_omt current_omt = guy.pos_abs_omt();
                    auto next_omt = guy.omt_path.rbegin();
                    while( next_omt != guy.omt_path.rend() && *next_omt == current_omt ) {
                        ++next_omt;
                    }
                    if( next_omt == guy.omt_path.rend() ) {
                        homeward_trace.action( "none", "no remaining OMT waypoint", "blocked" );
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
                    const bool allow_ordinary_homeward_local_reentry =
                        relationship->ordinary_homeward_local_reentry && m.inbounds( next_center );
                    const auto combined_avoid = [&npc_avoid, &avoid_nonreentry](
                    const tripoint_bub_ms & step ) {
                        return npc_avoid( step ) || avoid_nonreentry( step );
                    };
                    const std::function<bool( const tripoint_bub_ms & )> local_homeward_avoid =
                        allow_ordinary_homeward_local_reentry ? npc_avoid : combined_avoid;
                    bandit_live_world_probe::scoped_loaded_covert_local_path_solve path_solve_probe;
                    std::vector<tripoint_bub_ms> preflight_path =
                        m.inbounds( next_center ) ? m.route(
                            guy.pos_bub(), next_target, guy.get_pathfinding_settings( false ),
                            local_homeward_avoid ) : std::vector<tripoint_bub_ms>();
                    // The retained-edge materialization has just assigned this member a
                    // homeward OMT route.  Normalize that route in the active local-map
                    // frame before checking cohesion or consuming it: map::route includes
                    // the current tile but npc::move_to_next requires an actual next tile.
                    while( !preflight_path.empty() &&
                           preflight_path.front() == guy.pos_bub( m ) ) {
                        preflight_path.erase( preflight_path.begin() );
                    }
                    const bool preflight_route_safe = !preflight_path.empty() &&
                                                      ( allow_ordinary_homeward_local_reentry ||
                                                        local_path_respects_nonreentry(
                                                            preflight_path ) );
                    if( auto *read = homeward_trace.read() ) {
                        read->next_center_in_bounds = m.inbounds( next_center );
                        read->ordinary_local_reentry = allow_ordinary_homeward_local_reentry;
                        read->route_found = !preflight_path.empty();
                        read->route_safe = preflight_route_safe;
                        if( !preflight_path.empty() ) {
                            const auto &next = preflight_path.front();
                            read->next_step_ms = m.get_abs( next ).to_string();
                            read->next_step_passable = m.passable_through( next );
                            read->next_step_occupied = get_creature_tracker().creature_at( next ) != nullptr;
                            read->next_step_dangerous = guy.sees_dangerous_field( next );
                        }
                    }
                    if( !preflight_route_safe ) {
                        homeward_trace.action( "none", "local_route_unavailable", "blocked" );
                        const bool emit_local_rejection = log_homeward_route_result &&
                                                          logged_homeward_route_result_ids.insert(
                                                              guy.getID() ).second;
                        if( emit_local_rejection ) {
                            const auto avoid_none = []( const tripoint_bub_ms & ) {
                                return false;
                            };
                            const std::vector<tripoint_bub_ms> base_path = m.route(
                                        guy.pos_bub(), next_target,
                                        guy.get_pathfinding_settings( false ), avoid_none );
                            const std::vector<tripoint_bub_ms> npc_avoid_path = m.route(
                                        guy.pos_bub(), next_target,
                                        guy.get_pathfinding_settings( false ), npc_avoid );
                            const std::vector<tripoint_bub_ms> nonreentry_path = m.route(
                                        guy.pos_bub(), next_target,
                                        guy.get_pathfinding_settings( false ), avoid_nonreentry );
                            DebugLog( D_INFO, DC_ALL )
                                    << "bandit_live_world loaded homeward local movement rejected"
                                    << " member=" << guy.getID().get_value()
                                    << " phase=" << bandit_live_world::to_string(
                                        relationship->phase )
                                    << " owner=local"
                                    << " position=" << guy.pos_abs().to_string()
                                    << " goal=" << guy.goal.to_string()
                                    << " next_omt=" << next_omt->to_string()
                                    << " route_found=" << ( preflight_path.empty() ? "no" : "yes" )
                                    << " route_safe=" << ( preflight_route_safe ? "yes" : "no" )
                                    << " base_route_found=" <<
                                    ( base_path.empty() ? "no" : "yes" )
                                    << " npc_avoid_route_found=" <<
                                    ( npc_avoid_path.empty() ? "no" : "yes" )
                                    << " nonreentry_route_found=" <<
                                    ( nonreentry_path.empty() ? "no" : "yes" )
                                    << " reason=local_route_unavailable\n";
                        }
                        break;
                    }
                    const bool next_step_cohesive = step_preserves_pair_cohesion( preflight_path.front() );
                    if( homeward_trace.read() ) {
                        homeward_trace.read()->next_step_cohesive = next_step_cohesive;
                    }
                    if( !next_step_cohesive ) {
                        homeward_trace.action( "move_pause", "pair_cohesion", "blocked" );
                        DebugLog( D_INFO, DC_ALL )
                                << "bandit_live_world loaded homeward local movement deferred"
                                << " member=" << guy.getID().get_value()
                                << " phase=" << bandit_live_world::to_string(
                                    relationship->phase )
                                << " owner=local"
                                << " position=" << guy.pos_abs().to_string()
                                << " goal=" << guy.goal.to_string()
                                << " next_omt=" << next_omt->to_string()
                                << " next_step=" << preflight_path.front().to_string()
                                << " reason=pair_cohesion\n";
                        guy.move_pause();
                        continue;
                    }
                    while( !guy.omt_path.empty() && guy.omt_path.back() == current_omt ) {
                        guy.omt_path.pop_back();
                    }
                    guy.path = std::move( preflight_path );
                    homeward_trace.action( "move_to_next", "safe local homeward route and cohesive next step" );
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
                    homeward_trace.action( "omt_fallback", local_route_failed ?
                                           "homeward OMT fallback returned false" : "homeward OMT fallback returned true" );
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
                        homeward_trace.action( "move_to_next", "burned egress local route accepted" );
                        guy.move_to_next();
                    } else {
                        guy.path.clear();
                        const std::optional<tripoint_bub_ms> survival_step =
                            choose_noninward_step( false );
                        if( survival_step ) {
                            homeward_trace.action( "move_to", "burned egress selected a survival step" );
                            guy.move_to( *survival_step, true, nullptr, true );
                            bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                        } else {
                            homeward_trace.action( "move_pause", "burned egress has no survival step", "blocked" );
                            bandit_live_world::fail_live_covert_scout_burned_egress( guy.getID() );
                            guy.move_pause();
                        }
                    }
                } else {
                    homeward_trace.action( "move_pause", "homeward branch has no movement order", "blocked" );
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
        if( !entered_action_loop ) {
            live_bandit_homeward_motor_trace homeward_trace(
                guy, pair_homeward_travel_ids.count( guy.getID() ) > 0 );
            homeward_trace.action( "none", "NPC action loop not entered", "not_invoked" );
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
    complete_live_bandit_ingress_boundary_steps( pair_standard_boundary_steps );
    pair_recovery_boundary_steps.insert( pair_standard_boundary_steps.begin(),
                                         pair_standard_boundary_steps.end() );
    complete_live_bandit_outward_boundary_steps( pair_recovery_boundary_steps );
    g->cleanup_dead();
    // A homeward pair whose camp OMT is still inside the loaded bubble has no boundary
    // crossing to consume.  Once both exact members are physically home, commit their
    // generation-bound receipts through the same recorder before the next generic cadence.
    record_live_bandit_structural_member_returns();
}

bandit_live_world::camp_signal_observation_result record_live_light_staffed_observer_at_cadence(
    bandit_live_world::world_state &bandit_state,
    const std::vector<live_bandit_signal_observation> &,
    const std::vector<live_bandit_sound_observation> &live_sounds,
    const bool signal_cadence_due )
{
    if( !signal_cadence_due ) {
        return {};
    }
    bandit_live_world::camp_signal_observation_result result;
    live_light::record_staffed_observer( calendar::turn,
    [&result, &bandit_state, &live_sounds]( const std::vector<live_bandit_signal_observation> &samples ) {
        result = record_live_bandit_staffed_camp_signals( bandit_state, samples, live_sounds );
    } );
    return result;
}

void overmap_npc_move()
{
    const auto perf_started = std::chrono::steady_clock::now();
    avatar &u = get_avatar();
    bandit_live_world::world_state &bandit_state = overmap_buffer.global_state.bandit_live_world;
    const live_bandit_elevated_recovery_orders elevated_recovery_orders =
        recover_live_bandit_local_elevated_watches();
    record_live_bandit_persistent_elevated_watch_arrivals();
    std::set<character_id> blocked_elevated_recovery_members;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        if( !bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) ) {
            continue;
        }
        for( const character_id id : site.active_outing.member_ids ) {
            if( elevated_recovery_orders.count( id ) == 0 ) {
                blocked_elevated_recovery_members.insert( id );
            }
        }
    }
    prepare_live_bandit_abstract_scout_travel();
    note_live_bandit_aftermath();
    complete_loaded_live_bandit_route_arrivals();
    complete_loaded_live_bandit_alternate_watch_repositions();
    record_live_bandit_structural_member_returns();
    // The overmap travel owner runs before monmove.  Preserve a sole confirmed
    // survivor's selected physical exit while its identity-bound route is still
    // present, rather than letting ordinary travel replace that route first.
    const std::set<character_id> homeward_member_ids =
        bandit_live_world::local_pair_homeward_travel_ids( bandit_state );
    std::map<character_id, tripoint_abs_omt> homeward_destinations;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        for( const character_id member_id : site.active_outing.member_ids ) {
            if( homeward_member_ids.count( member_id ) > 0 ) {
                homeward_destinations.emplace( member_id, site.anchor );
            }
        }
    }
    // A loaded pair that reached its selected departures during monmove must cross before the
    // generic overmap owner normalizes or pops either member's OMT path.  Sole survivors use the
    // same boundary consumer below; stale or mismatched identities remain absent from both maps.
    std::map<character_id, live_bandit_pair_boundary_step> homeward_boundary_steps =
        live_bandit_pair_boundary_steps( homeward_destinations, nullptr, true );
    const std::map<character_id, live_bandit_pair_boundary_step> single_homeward_steps =
        live_bandit_single_homeward_boundary_steps( homeward_destinations );
    homeward_boundary_steps.insert( single_homeward_steps.begin(), single_homeward_steps.end() );
    const std::set<character_id> committed_homeward_member_ids =
        complete_live_bandit_homeward_boundary_steps( homeward_boundary_steps );
    bool dematerialized_handoffs = handoff_live_bandit_generic_scout_returns();
    dematerialized_handoffs |= dematerialize_live_bandit_structural_handoffs();
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
    const std::vector<live_bandit_sound_observation> live_sounds = take_live_bandit_sounds();
    int bootstrapped_sites = 0;
    if( signal_cadence_due || structural_cadence_due ) {
        bootstrapped_sites = bootstrap_live_bandit_abstract_sites_near_player();
    }
    if( signal_cadence_due ) {
        live_signals = live_light::samples_for_turn( calendar::turn );
    }
    if( signal_cadence_due || !live_sounds.empty() ) {
        record_r008_production_channel_scan( live_signals, live_sounds );
    }
    const auto signal_done = std::chrono::steady_clock::now();
    if( dispatch_cadence_due || structural_cadence_due ) {
        refresh_live_bandit_member_readiness( bandit_state );
    }
    // Home rosters receive the same bounded signal packet on the existing
    // five-minute cadence.  This is discovery and memory only: the normal
    // structural/drive scheduler remains the sole dispatch decision owner.
    // Registration and observation are independent ownership boundaries.  A cadence may
    // discover an unrelated abstract source while an already staffed site is eligible for
    // its production read; suppressing the observer here made that valid callback depend on
    // whether registration happened to run first. Counted abstract home slots resolve
    // through the tracked-spawn owner; invalid and empty rosters remain excluded.
    if( signal_cadence_due ) {
        const bandit_live_world::camp_signal_observation_result camp_signals =
            record_live_light_staffed_observer_at_cadence(
                bandit_state, live_signals, live_sounds, signal_cadence_due );
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world staffed_camp_signal_observation"
                                   << " sites=" << camp_signals.sites_considered
                                   << " eligible=" << camp_signals.eligible_camps
                                   << " callbacks=" << camp_signals.callbacks_invoked
                                   << " created=" << camp_signals.leads_created
                                   << " refreshed=" << camp_signals.leads_refreshed
                                   << " unchanged=" << camp_signals.unchanged_reads << '\n';
    } else if( !live_sounds.empty() ) {
        // A consumed one-shot sound cannot wait for the next optical cadence.
        record_live_bandit_staffed_camp_signals( bandit_state, {}, live_sounds );
    }
    // The loaded player scene owns observation.  The camp may adopt that durable
    // opportunity on the signal cadence, but structural maintenance remains on
    // its own cadence so adoption alone cannot dispatch an outing.
    if( signal_cadence_due && bootstrapped_sites == 0 ) {
        observe_live_bandit_player_target_opportunity();
        const int adopted_opportunities =
            bandit_live_world::adopt_observed_hostile_player_opportunities(
                bandit_state, live_bandit_current_minutes(),
        []( const bandit_live_world::site_record & site,
        const bandit_live_world::hostile_target_opportunity_record & opportunity ) {
            return live_bandit_player_opportunity_route_available( site, opportunity );
        } );
        const std::optional<basecamp *> direct_camp = overmap_buffer.find_camp( u.pos_abs_omt().xy() );
        const std::optional<tripoint_abs_omt> camp_omt = direct_camp && *direct_camp != nullptr ?
                std::optional<tripoint_abs_omt>( ( *direct_camp )->camp_omt_pos() ) : std::nullopt;
        const tripoint_abs_omt *camp_omt_ptr = camp_omt ? &*camp_omt : nullptr;
        const int camp_distance = camp_omt ? rl_dist( u.pos_abs_omt(), *camp_omt ) : -1;
        openclaw_harness_trace_bandit_owner( "adoption_result", u.pos_abs_omt(), camp_omt_ptr,
                camp_distance, camp_omt.has_value(), adopted_opportunities );
        if( adopted_opportunities > 0 ) {
            DebugLog( D_INFO, DC_ALL ) << "bandit_live_world player_opportunity_adoption adopted="
                                       << adopted_opportunities << '\n';
        }
    }
    // A camp created on this structural cadence has not yet had an ordinary
    // loaded player scene in which to observe a target opportunity.  Defer
    // its first structural maintenance pass until the next cadence, so the
    // structural scheduler does not consume that initial lead first.
    if( structural_cadence_due && bootstrapped_sites == 0 ) {
        maintain_live_bandit_structural_bounty( live_signals, live_sounds );
    } else if( !live_sounds.empty() ) {
        const bandit_live_world::structural_signal_record_result recorded =
            record_live_bandit_structural_sounds( bandit_state, live_sounds );
        const bandit_live_world::structural_signal_record_result local_recorded =
            record_live_bandit_local_structural_sounds( bandit_state, live_sounds );
        DebugLog( D_INFO, DC_ALL ) << "bandit_live_world sound_observation sites="
                                   << recorded.sites_considered
                                   << " active=" << recorded.active_outings_considered
                                   << " callbacks=" << recorded.callbacks_invoked
                                   << " recorded=" << recorded.sites_recorded
                                   << " facts=" << recorded.facts_recorded
                                   << " local_callbacks=" << local_recorded.callbacks_invoked
                                   << " local_recorded=" << local_recorded.sites_recorded
                                   << " local_facts=" << local_recorded.facts_recorded << '\n';
    }
    // Scheduled signals and one-shot sounds read their own packet before visual evidence
    // consumes this minute's abstract cursor. Typed batches deduplicate independently;
    // movement and owner transitions retain their single-advance clock.
    record_live_bandit_stationary_watch_signals( bandit_state, live_signals, signal_cadence_due );
    advance_live_bandit_hostile_rallies();
    advance_live_bandit_hostile_approaches();
    // The approach owner can commit contact in this overmap pass, after the
    // normal aftermath pass above has already inspected it.  Admit the exact
    // local party now, but leave parley/combat progression to its regular
    // aftermath owner on a later player turn.
    materialize_live_bandit_committed_shakedown_contacts();
    advance_live_bandit_hostile_returns();
    const std::set<character_id> identity_held_members = prepare_live_bandit_abstract_scout_travel();
    materialize_live_bandit_structural_handoffs();
    // A retained non-canonical resume is a single two-member admission boundary while that
    // exact OMT can host the pair.  At an unstageable edge, holding the pair here prevents both
    // the abstract route and the physical actors from progressing; release it to the paired
    // overmap travel owner until a later OMT can be materialized.
    std::set<character_id> blocked_retained_resume_members;
    std::set<character_id> released_retained_resume_members;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || outing.member_ids.size() != 2 ||
            outing.owner != bandit_live_world::simulation_owner::abstract ||
            !outing.local_handoff.is_abstract_resume() ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ||
            outing.shared_route.empty() || outing.waypoint_index < 0 ||
            outing.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ||
            ( outing.local_handoff.route_position ==
              outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )] &&
              !bandit_live_world::structural_outing_uses_frontier_route( outing ) ) ) {
            continue;
        }
        std::vector<tripoint_abs_ms> entry_positions =
            live_bandit_local_handoff_entry_positions( outing.local_handoff.route_position,
                    outing.local_handoff.approach_from, outing.member_ids.size() );
        std::vector<tripoint_abs_ms> staging_positions =
            live_bandit_local_handoff_entry_positions( outing.local_handoff.route_position,
                    outing.shared_route.back(), outing.member_ids.size(), entry_positions );
        if( entry_positions.size() == outing.member_ids.size() &&
            staging_positions.size() != outing.member_ids.size() ) {
            staging_positions = entry_positions;
        }
        const bool materialization_has_pair_slots =
            entry_positions.size() == outing.member_ids.size() &&
            staging_positions.size() == outing.member_ids.size();
        if( live_bandit_retained_homeward_resume_blocks_generic_travel(
                outing, materialization_has_pair_slots ) ) {
            blocked_retained_resume_members.insert( outing.member_ids.begin(),
                                                   outing.member_ids.end() );
        } else {
            released_retained_resume_members.insert( outing.member_ids.begin(),
                                                     outing.member_ids.end() );
        }
    }
    const auto dispatch_done = std::chrono::steady_clock::now();
    std::set<character_id> hostile_approach_member_ids;
    std::set<character_id> committed_local_hostile_member_ids;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        const bandit_live_world::hostile_operation_state &operation =
            site.active_hostile_operation;
        const bandit_live_world::active_outing_state &reservation = operation.reservation;
        if( !site.retired_empty_site && operation.is_active() &&
            operation.phase == bandit_live_world::hostile_operation_phase::approaching &&
            reservation.owner == bandit_live_world::simulation_owner::abstract ) {
            hostile_approach_member_ids.insert( reservation.member_ids.begin(),
                                                reservation.member_ids.end() );
        }
        // Tactical committed contact owns these exact reservation members.  Do
        // not erase an inherited strategic order: the authoritative return or
        // terminal handoff will consume it.  This only prevents the generic
        // travel motor from advancing it while local admission/stalking/combat
        // owns the actor.
        if( !site.retired_empty_site && operation.is_active() &&
            operation.phase == bandit_live_world::hostile_operation_phase::committed_contact &&
            reservation.owner == bandit_live_world::simulation_owner::local ) {
            for( const character_id member_id : reservation.member_ids ) {
                if( std::find( operation.withdrawing_member_ids.begin(),
                               operation.withdrawing_member_ids.end(), member_id ) ==
                    operation.withdrawing_member_ids.end() ) {
                    committed_local_hostile_member_ids.insert( member_id );
                }
            }
        }
    }
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
    for( const auto &order : elevated_recovery_orders ) {
        local_pair_forward_destinations.emplace( order.first, order.second.approach );
    }
    // A locally owned pair that has not released staging ownership must not fall through to
    // ordinary overmap travel.  In particular, a stale goal/path from before materialization can
    // otherwise advance one member (or rewrite the pair's route) while the loaded cohesion motor
    // is still assembling it.  Build this view only after the ownership preflight so malformed or
    // aliased outings fail closed with the same owner used by the local motor.
    std::map<character_id, tripoint_abs_ms> local_pair_assembly_orders;
    std::set<character_id> assembly_claimed_members;
    bool assembly_ownership_preflight_failed = false;
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        if( site.active_outing.local_projection_reconciliation_rejected ) {
            continue;
        }
        if( !bandit_live_world::claim_local_pair_site_ownership(
                site, assembly_claimed_members ) ) {
            assembly_ownership_preflight_failed = true;
            break;
        }
    }
    if( !assembly_ownership_preflight_failed ) {
        for( const bandit_live_world::site_record &site : bandit_state.sites ) {
            if( bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) ) {
                // Compatibility migration for the old schema-9 saved watch.
                continue;
            }
            // The common order producer releases assembled forward pairs at any
            // source height and retains every new transfer until assembly completes.
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
                   ( outing.kind == bandit_live_world::outing_kind::structural_sortie ||
                     outing.kind == bandit_live_world::outing_kind::scout_sortie ) &&
                   outing.owner == bandit_live_world::simulation_owner::local &&
                   bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
                   std::find( outing.member_ids.begin(), outing.member_ids.end(), member_id ) !=
                   outing.member_ids.end() &&
                   std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) ==
                   outing.casualty_ids.end() && site_contains_omt( site, position );
        } );
    };
    // A retained abstract resume owns a physical pair that has already crossed a loaded
    // boundary.  Ordinary NPC travel can move those two instances, but has no authority to
    // update the retained handoff cursor; it therefore leaves materialization looking at the
    // old OMT forever.  Advance the complete pair and its persisted cursor as one transaction
    // until a later OMT can bind the local owner again.
    std::set<character_id> progressed_retained_resume_members;
    for( bandit_live_world::site_record &site : bandit_state.sites ) {
        bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || outing.member_ids.size() != 2 ||
            outing.owner != bandit_live_world::simulation_owner::abstract ||
            !outing.local_handoff.is_abstract_resume() ||
            outing.local_handoff.cohesion_abort_return ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ||
            released_retained_resume_members.count( outing.member_ids[0] ) == 0 ||
            released_retained_resume_members.count( outing.member_ids[1] ) == 0 ) {
            continue;
        }
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }
        std::vector<shared_ptr_fast<npc>> members;
        bool complete_party = true;
        for( const auto &snapshot : outing.local_handoff.members ) {
            if( snapshot.dead ) {
                const auto *record = site.find_member( snapshot.npc_id );
                complete_party &= record && record->state == bandit_live_world::member_state::dead &&
                                  outing.member_is_resolved( snapshot.npc_id );
                continue;
            }
            const auto member = overmap_buffer.find_npc( snapshot.npc_id );
            if( !member || member->is_active() || member->is_dead() ||
                member->has_flag( json_flag_CANNOT_MOVE ) ||
                ( bandit_live_world::structural_outing_uses_frontier_route( outing ) &&
                  ( !live_bandit_member_can_take_homeward_step( *member ) ||
                    member->get_attitude() == NPCATT_FLEE ||
                    member->get_attitude() == NPCATT_FLEE_TEMP ||
                    member->has_active_faction_alarm() ) ) ||
                member->pos_abs_omt() != outing.local_handoff.route_position ) {
                complete_party = false;
                break;
            }
            members.push_back( member );
        }
        if( !complete_party || members.empty() ) {
            continue;
        }
        std::vector<std::vector<tripoint_abs_omt>> routes;
        std::optional<tripoint_abs_omt> next;
        for( const auto &member : members ) {
            auto path = overmap_buffer.get_travel_path( member->pos_abs_omt(), site.anchor,
                        overmap_path_params::for_npc() ).points;
            if( path.empty() || path.front() != site.anchor ||
                path.back() != member->pos_abs_omt() ||
                !live_bandit_route_respects_covert_ring( outing, path ) ) {
                break;
            }
            while( !path.empty() && path.back() == member->pos_abs_omt() ) {
                path.pop_back();
            }
            if( path.empty() || ( next && *next != path.back() ) ) {
                break;
            }
            next = path.back();
            routes.push_back( std::move( path ) );
        }
        if( routes.size() != members.size() || !next ) {
            continue;
        }
        // One return owner advances the complete surviving party.  A confirmed
        // casualty is not a second travelling actor, and must not strand the
        // survivor with an already-consumed local boundary goal.
        for( std::size_t index = 0; index < members.size(); ++index ) {
            auto &member = members[index];
            member->goal = site.anchor;
            member->omt_path = std::move( routes[index] );
            member->guard_pos.reset();
            member->clear_ai_guard_pos();
            member->set_mission( NPC_MISSION_TRAVELLING );
            const tripoint_rel_ms offset = member->pos_abs() -
                                           project_to<coords::ms>( member->pos_abs_omt() );
            member->travel_overmap( *next );
            if( bandit_live_world::structural_outing_uses_frontier_route( outing ) ) {
                member->spawn_at_precise( project_to<coords::ms>( *next ) + offset );
            }
        }
        std::vector<bandit_live_world::local_abstract_resume_progress_read> reads;
        for( const auto &snapshot : outing.local_handoff.members ) {
            if( snapshot.dead ) {
                reads.push_back( { snapshot.npc_id, true, true, 0, snapshot.exit_position } );
            } else {
                const auto member = overmap_buffer.find_npc( snapshot.npc_id );
                reads.push_back( { snapshot.npc_id, true, false,
                                   member->hp_percentage(), member->pos_abs() } );
            }
        }
        if( !bandit_live_world::record_local_pair_abstract_resume_progress(
                site, *cursor, live_bandit_current_minutes(), reads ) ) {
            continue;
        }
        for( const auto &member : members ) {
            progressed_retained_resume_members.insert( member->getID() );
        }
        DebugLog( D_INFO, DC_ALL )
                << "bandit_live_world retained homeward resume advanced"
                << " site=" << site.site_id
                << " activity=" << site.active_outing.activity_id
                << " generation=" << site.active_outing.generation
                << " prior_route=" << outing.local_handoff.approach_from.to_string()
                << " route_position=" << site.active_outing.local_handoff.route_position.to_string()
                << " surviving_members=" << members.size()
                << '\n';
    }
    std::vector<npc *> travelling_npcs;
    bool local_pair_needs_reload = false;
    // Ingress ownership is transactional.  Do this check before assigning or
    // repairing either member's forward route: otherwise the first member can
    // be advanced while its partner is unavailable, even though the later
    // travelling-npc pass correctly refuses a partial pair.
    std::set<character_id> preflight_blocked_ingress_members;
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
        const bool frontier_forward = bandit_live_world::structural_outing_uses_frontier_route( outing );
        if( ( frontier_forward ||
              ( outing.target_omt.z() > site.anchor.z() && outing.schema_version >= 10 ) ) &&
            outing.phase == bandit_live_world::scout_phase::observing &&
            outing.waypoint_index == 1 && outing.casualty_ids.empty() &&
            outing.assessment.observation_started_minutes < 0 &&
            first_destination->second == ( frontier_forward ? outing.target_omt : outing.selected_watch_omt ) ) {
            // Reaching the physical approach consumes the previous OMT goal
            // and sets an ordinary guard mission.  Bind both watch routes
            // together before the ingress preflight checks their missions.
            npc *first = g->find_npc( outing.member_ids[0] );
            npc *second = g->find_npc( outing.member_ids[1] );
            const auto ready = [&outing, frontier_forward]( const npc *member ) {
                return member != nullptr && ( frontier_forward || !member->is_active() ) &&
                       !member->is_dead() && !member->in_sleep_state() &&
                       ( !frontier_forward || ( live_bandit_member_can_take_homeward_step( *member ) &&
                                                !member->has_active_faction_alarm() ) ) &&
                       !member->has_flag( json_flag_CANNOT_MOVE ) &&
                       member->get_attitude() != NPCATT_FLEE &&
                       member->get_attitude() != NPCATT_FLEE_TEMP &&
                       member->pos_abs_omt() == outing.shared_route[1];
            };
            if( ready( first ) && ready( second ) &&
                ( !frontier_forward || persist_live_bandit_local_projection_leases( site, &site, false ) ) ) {
                const std::vector<tripoint_abs_omt> first_route =
                    live_bandit_member_route_to( *first, site, first_destination->second );
                const std::vector<tripoint_abs_omt> second_route =
                    live_bandit_member_route_to( *second, site, second_destination->second );
                if( !first_route.empty() && !second_route.empty() ) {
                    for( npc *member : { first, second } ) {
                        member->guard_pos.reset();
                        member->clear_ai_guard_pos();
                        member->goal = first_destination->second;
                        member->set_mission( NPC_MISSION_TRAVELLING );
                    }
                    first->omt_path = first_route;
                    second->omt_path = second_route;
                }
            }
        }
        const auto member_can_enter = []( const character_id member_id ) {
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            return member && !member->is_dead() && !member->has_flag( json_flag_CANNOT_MOVE ) &&
                   member->is_travelling();
        };
        if( !member_can_enter( outing.member_ids[0] ) ||
            !member_can_enter( outing.member_ids[1] ) ) {
            preflight_blocked_ingress_members.insert( outing.member_ids.begin(),
                    outing.member_ids.end() );
        }
    }
    static constexpr int move_search_radius = 600;
    for( auto &elem : overmap_buffer.get_npcs_near_player( move_search_radius ) ) {
        if( !elem ) {
            continue;
        }
        npc *npc_to_add = elem.get();
        if( progressed_retained_resume_members.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
        if( blocked_retained_resume_members.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
        if( committed_homeward_member_ids.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
        if( blocked_elevated_recovery_members.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
        if( hostile_approach_member_ids.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
        if( committed_local_hostile_member_ids.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
        if( preflight_blocked_ingress_members.count( npc_to_add->getID() ) > 0 ) {
            continue;
        }
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
        const bandit_live_world::site_record *homeward_owner = nullptr;
        if( local_pair_homeward_member_ids.count( npc_to_add->getID() ) > 0 ) {
            for( const bandit_live_world::site_record &site : bandit_state.sites ) {
                if( !site.retired_empty_site &&
                    ( site.active_outing.kind == bandit_live_world::outing_kind::structural_sortie ||
                      site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie ) &&
                    site.active_outing.owner == bandit_live_world::simulation_owner::local &&
                    bandit_live_world::scout_phase_requires_homeward_only(
                        site.active_outing.phase ) &&
                    std::find( site.active_outing.member_ids.begin(),
                               site.active_outing.member_ids.end(),
                               npc_to_add->getID() ) != site.active_outing.member_ids.end() ) {
                    homeward_owner = &site;
                    break;
                }
            }
        }
        const bool unloaded_structural_homeward = homeward_owner != nullptr &&
                homeward_owner->active_outing.kind == bandit_live_world::outing_kind::structural_sortie;
        if( locally_owned_travel_member &&
            ( npc_to_add->is_active() || unloaded_structural_homeward ) ) {
            if( local_pair_homeward_member_ids.count( npc_to_add->getID() ) > 0 ) {
                if( homeward_owner != nullptr ) {
                    // The same local operation owns unloaded survivors too.  A
                    // destination-less inactive member cannot enter the travel pass
                    // until this owner supplies a real route from its actual position.
                    // Validate the complete copy set before assigning either member;
                    // forced incapacity and individual escape still stop unloaded travel.
                    if( !npc_to_add->is_active() &&
                        ( !persist_live_bandit_local_projection_leases(
                              *homeward_owner, homeward_owner, false ) ||
                          npc_to_add->is_dead() || npc_to_add->in_sleep_state() ||
                          npc_to_add->has_flag( json_flag_CANNOT_MOVE ) ||
                          npc_to_add->get_attitude() == NPCATT_FLEE ||
                          npc_to_add->get_attitude() == NPCATT_FLEE_TEMP ) ) {
                        continue;
                    }
                    const bool was_routing_home = live_bandit_member_routing_home(
                                                      *npc_to_add, *homeward_owner );
                    const bool route_assigned = live_bandit_route_member_home(
                                                    *npc_to_add, *homeward_owner );
                    if( !was_routing_home || !route_assigned ) {
                        DebugLog( D_INFO, DC_ALL )
                                << "bandit_live_world overmap homeward motor"
                                << " site=" << homeward_owner->site_id
                                << " activity=" << homeward_owner->active_outing.activity_id
                                << " generation=" << homeward_owner->active_outing.generation
                                << " member=" << npc_to_add->getID().get_value()
                                << " route_assigned=" << ( route_assigned ? "yes" : "no" )
                                << " goal=" << npc_to_add->goal.to_string()
                                << " omt_path=" << npc_to_add->omt_path.size() << '\n';
                    }
                    if( !npc_to_add->is_active() && !route_assigned ) {
                        continue;
                    }
                }
            }
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
            // The loaded local motor owns an in-bounds member.  Once its assigned homeward
            // route carries it beyond this map, however, it must re-enter the paired overmap
            // owner below: retaining this unconditional continue left the selected boundary
            // empty forever and prevented either the paired crossing or transactional unload.
            // Keep the reload request, but do not hide an out-of-bounds member from that owner.
            if( get_map().inbounds( npc_to_add->pos_abs() ) ) {
                local_pair_needs_reload |= !npc_to_add->is_active();
                continue;
            }
            local_pair_needs_reload |= npc_to_add->is_active();
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
    // Abstract structural members remain the authoritative owners of their persisted
    // overmap route after a loaded-bubble preflight rejects materialization.  They may
    // be outside the generic near-player enumeration, but must still make progress on
    // the exact persisted route.  Admit only those concrete outbound members carrying
    // an explicit travel mission; local-owner members remain handled above.
    for( const bandit_live_world::site_record &site : bandit_state.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != bandit_live_world::outing_kind::structural_sortie ||
            outing.owner != bandit_live_world::simulation_owner::abstract ) {
            continue;
        }
        for( const character_id member_id : outing.member_ids ) {
            if( progressed_retained_resume_members.count( member_id ) > 0 ) {
                continue;
            }
            if( blocked_retained_resume_members.count( member_id ) > 0 ) {
                continue;
            }
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( member_id );
            if( !member || member->is_active() || member->mission != NPC_MISSION_TRAVELLING ||
                std::find( travelling_npcs.begin(), travelling_npcs.end(), member.get() ) !=
                travelling_npcs.end() ) {
                continue;
            }
            travelling_npcs.push_back( member.get() );
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
                                        &local_pair_ingress_destinations, &outing](
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
                   ( !bandit_live_world::structural_outing_uses_frontier_route( outing ) ||
                     ( live_bandit_member_can_take_homeward_step( *candidate ) &&
                       candidate->get_attitude() != NPCATT_FLEE &&
                       candidate->get_attitude() != NPCATT_FLEE_TEMP &&
                       !candidate->has_active_faction_alarm() ) ) &&
                   candidate->has_omt_destination() && candidate->goal == destination->second &&
                   !candidate->omt_path.empty();
        };
        if( !member_is_eligible( outing.member_ids[0] ) ||
            !member_is_eligible( outing.member_ids[1] ) ) {
            blocked_ingress_members.insert( outing.member_ids.begin(), outing.member_ids.end() );
        }
    }
    for( npc *&elem : travelling_npcs ) {
        const bool locally_reserved = std::any_of( bandit_state.sites.begin(), bandit_state.sites.end(),
        [elem]( const bandit_live_world::site_record &site ) {
            const auto &outing = site.active_outing;
            const auto &hostile = site.active_hostile_operation;
            return ( outing.is_active() && outing.owner == bandit_live_world::simulation_owner::local &&
                     std::find( outing.member_ids.begin(), outing.member_ids.end(), elem->getID() ) !=
                     outing.member_ids.end() ) ||
                   ( hostile.is_active() &&
                     std::find( hostile.reservation.member_ids.begin(), hostile.reservation.member_ids.end(),
                                elem->getID() ) != hostile.reservation.member_ids.end() );
        } );
        if( !elem->is_active() && !locally_reserved && elem->is_travelling() &&
            elem->goal == elem->pos_abs_omt() && elem->omt_path.empty() &&
            !pending_abstract_scout_watch_order( *elem ) &&
            std::any_of( bandit_state.sites.begin(), bandit_state.sites.end(),
        [elem]( const bandit_live_world::site_record &site ) {
            return site.find_member( elem->getID() ) != nullptr;
        } ) ) {
            // Release/abort/casualty/foreign ownership cannot retain a completed
            // pending watch order. This performs no assessment or return write.
            elem->reach_omt_destination();
        }
        if( identity_held_members.count( elem->getID() ) > 0 ) {
            record_live_bandit_scout_motor_service( *elem, "identity_held", elem->pos_abs() );
            continue;
        }
        if( committed_homeward_member_ids.count( elem->getID() ) > 0 ) {
            continue;
        }
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
                    ( outing.selected_watch_omt == forward_destination->second ||
                      ( bandit_live_world::structural_outing_uses_frontier_route( outing ) &&
                        outing.target_omt == forward_destination->second ) ||
                      ( bandit_live_world::local_elevated_signal_watch_recovery_needed( site ) &&
                        elevated_recovery_orders.count( elem->getID() ) != 0 ) );
                if( !site.retired_empty_site && outing.is_active() &&
                    ( outing.kind == bandit_live_world::outing_kind::structural_sortie ||
                      outing.kind == bandit_live_world::outing_kind::scout_sortie ) &&
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
                    const bool outward_pair = local_owner != nullptr &&
                            !elem->is_active() &&
                            ( local_owner->active_outing.target_omt.z() > local_owner->anchor.z() ||
                              bandit_live_world::structural_outing_uses_frontier_route( local_owner->active_outing ) ) &&
                            local_owner->active_outing.phase == bandit_live_world::scout_phase::observing &&
                            local_owner->active_outing.assessment.observation_started_minutes < 0 &&
                            ( bandit_live_world::local_elevated_signal_watch_recovery_needed(
                                  *local_owner ) ||
                              ( ( local_owner->active_outing.schema_version >= 10 ||
                                  bandit_live_world::structural_outing_uses_frontier_route( local_owner->active_outing ) ) &&
                                local_owner->active_outing.waypoint_index == 1 ) );
                    const tripoint_rel_ms former_offset = elem->pos_abs() -
                                                           project_to<coords::ms>( elem->pos_abs_omt() );
                    const tripoint_abs_ms motor_before = elem->pos_abs();
                    elem->travel_overmap( elem->omt_path.back() );
                    if( outward_pair ) {
                        // Generic OMT travel randomizes each NPC's square.  Carry
                        // this pair's distinct exit offsets into the next OMT so
                        // a random same-square landing cannot invalidate the
                        // two-actor physical watch arrival.
                        elem->spawn_at_precise(
                            project_to<coords::ms>( elem->pos_abs_omt() ) + former_offset );
                    }
                    record_live_bandit_scout_motor_service( *elem, "travel_overmap_returned", motor_before );
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
    if( npcs_need_reload || local_pair_needs_reload ) {
        g->reload_npcs();
        for( const character_id member_id : committed_homeward_member_ids ) {
            npc *committed_member = g->find_npc( member_id );
            if( committed_member != nullptr ) {
                committed_member->on_unload();
                g->remove_npc( member_id );
            }
        }
    }
    // Travel-overmap can leave one half of a generic scout pair pending the
    // normal NPC reload.  Settle that production boundary before demanding
    // both durable identities for its local-to-abstract handoff.
    dematerialized_handoffs |= handoff_live_bandit_generic_scout_returns();
    record_live_bandit_persistent_elevated_watch_arrivals();
    dematerialized_handoffs |= dematerialize_live_bandit_structural_handoffs();
    // Observe the same physical actors after generic overmap travel as well as
    // retained-pair travel.  Canonical resume positions and a confirmed survivor
    // must not leave their persisted route cursor behind their actual movement.
    for( bandit_live_world::site_record &site : bandit_state.sites ) {
        const auto &outing = site.active_outing;
        if( outing.owner != bandit_live_world::simulation_owner::abstract ||
            !outing.local_handoff.is_abstract_resume() ) {
            continue;
        }
        const auto cursor = bandit_live_world::current_external_simulation_cursor( site );
        if( !cursor ) {
            continue;
        }
        std::vector<bandit_live_world::local_abstract_resume_progress_read> reads;
        for( const auto &snapshot : outing.local_handoff.members ) {
            if( snapshot.dead ) {
                reads.push_back( { snapshot.npc_id, true, true, 0, snapshot.exit_position } );
                continue;
            }
            const auto member = overmap_buffer.find_npc( snapshot.npc_id );
            if( !member || member->is_active() ) {
                break;
            }
            reads.push_back( { snapshot.npc_id, true, member->is_dead(),
                               member->hp_percentage(), member->pos_abs() } );
        }
        bandit_live_world::record_local_pair_abstract_resume_progress(
            site, *cursor, live_bandit_current_minutes(), reads );
    }
    prepare_live_bandit_abstract_scout_travel();
    complete_loaded_live_bandit_route_arrivals();
    complete_loaded_live_bandit_alternate_watch_repositions();
    record_live_bandit_structural_member_returns();
    dematerialized_handoffs |= handoff_live_bandit_generic_scout_returns();
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

std::map<character_id, tripoint_abs_omt> live_bandit_elevated_recovery_orders_for_test()
{
    std::map<character_id, tripoint_abs_omt> result;
    for( const auto &order : recover_live_bandit_local_elevated_watches() ) {
        result.emplace( order.first, order.second.approach );
    }
    return result;
}

std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>>
live_bandit_elevated_recovery_boundary_for_test()
{
    std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>> result;
    for( const auto &step : live_bandit_elevated_recovery_boundary_steps(
             recover_live_bandit_local_elevated_watches() ) ) {
        result.emplace( step.first,
                        std::make_pair( step.second.departure, step.second.exit ) );
    }
    return result;
}

std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>>
live_bandit_ingress_boundary_steps_for_test()
{
    std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>> result;
    for( const auto &step : live_bandit_pair_boundary_steps(
             bandit_live_world::local_pair_ingress_travel_destinations(
                 overmap_buffer.global_state.bandit_live_world ) ) ) {
        result.emplace( step.first, std::make_pair( step.second.departure, step.second.exit ) );
    }
    return result;
}

void live_bandit_elevated_recovery_npc_turn_for_test( const bool overmap_step )
{
    monmove();
    if( overmap_step ) {
        overmap_npc_move();
    }
}

bool live_bandit_retained_homeward_resume_blocks_generic_travel(
    const bandit_live_world::active_outing_state &outing,
    const bool materialization_has_pair_slots )
{
    return materialization_has_pair_slots && outing.member_ids.size() == 2 &&
           outing.owner == bandit_live_world::simulation_owner::abstract &&
           outing.local_handoff.is_abstract_resume() &&
           bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) &&
           !outing.shared_route.empty() && outing.waypoint_index >= 0 &&
           outing.waypoint_index < static_cast<int>( outing.shared_route.size() ) &&
           ( outing.local_handoff.route_position !=
             outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )] ||
             bandit_live_world::structural_outing_uses_frontier_route( outing ) );
}

std::string live_bandit_homeward_boundary_discriminator_for_test()
{
    bandit_live_world::world_state &state =
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
    std::map<character_id, live_bandit_pair_boundary_step> all_steps = steps;
    const std::map<character_id, live_bandit_pair_boundary_step> single_steps =
        live_bandit_single_homeward_boundary_steps( homeward_destinations );
    all_steps.insert( single_steps.begin(), single_steps.end() );
    std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>> result;
    for( const auto &step : all_steps ) {
        result.emplace( step.first,
                        std::make_pair( step.second.departure, step.second.exit ) );
    }
    return result;
}

std::string live_bandit_homeward_boundary_collection_for_test()
{
    const std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>> steps =
        live_bandit_homeward_boundary_steps_for_test();
    for( const bandit_live_world::site_record &site :
         overmap_buffer.global_state.bandit_live_world.sites ) {
        const bandit_live_world::active_outing_state &outing = site.active_outing;
        if( outing.member_ids.size() != 2 ||
            !bandit_live_world::scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        std::ostringstream report;
        report << "site=" << site.site_id << " steps=" << steps.size();
        int crossings = 0;
        std::string reason = "complete";
        for( const character_id member_id : outing.member_ids ) {
            const auto step = steps.find( member_id );
            npc *member = g->find_npc( member_id );
            if( member == nullptr ) {
                const shared_ptr_fast<npc> stored_member = overmap_buffer.find_npc( member_id );
                member = stored_member ? stored_member.get() : nullptr;
            }
            const bool at_departure = step != steps.end() && member != nullptr &&
                                      member->pos_abs() == step->second.first;
            crossings += at_departure ? 1 : 0;
            if( !at_departure && reason == "complete" ) {
                const tripoint_abs_ms destination_center =
                    project_to<coords::ms>( site.anchor ) + point( SEEX, SEEY );
                reason = step == steps.end() ?
                         ( member != nullptr && get_map().inbounds( destination_center ) ?
                           "destination_in_loaded_bubble" : "step_missing" ) :
                         member == nullptr ? "npc_missing" : "not_at_departure";
            }
            report << " member=" << member_id.get_value()
                   << ":step=" << ( step != steps.end() ? "yes" : "no" )
                   << ":npc=" << ( member != nullptr ? "yes" : "no" )
                   << ":at_departure=" << ( at_departure ? "yes" : "no" );
        }
        report << " crossings=" << crossings << " casualties=" << outing.casualty_ids.size()
               << " reason=" << ( crossings + outing.casualty_ids.size() == outing.member_ids.size() &&
                                   crossings > 0 ? "complete" : reason );
        return report.str();
    }
    return "available=no";
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

std::vector<bandit_live_world::structural_signal_read>
live_bandit_structural_signal_reads_for_test(
    const std::vector<live_bandit_signal_observation> &signals,
    const bandit_live_world::site_record &site,
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request )
{
    return live_bandit_structural_signal_reads( signals, {}, site, outing, request );
}

int record_live_bandit_stationary_watch_signals_for_test( bandit_live_world::world_state &state,
        const std::vector<live_bandit_signal_observation> &signals, const bool cadence_due )
{
    return record_live_bandit_stationary_watch_signals( state, signals, cadence_due );
}

std::vector<bandit_live_world::structural_signal_read>
live_bandit_staffed_camp_signal_reads_for_test(
    const std::vector<live_bandit_signal_observation> &signals,
    const bandit_live_world::site_record &site, const character_id observer_id )
{
    return live_bandit_staffed_camp_signal_reads( signals, {}, site, { observer_id, site.anchor } );
}

bandit_live_world::camp_signal_observer_resolution resolve_live_bandit_staffed_observer_for_test(
    bandit_live_world::world_state &state, const std::size_t site_index )
{
    return resolve_live_bandit_staffed_observer( state, site_index );
}

bandit_live_world::camp_signal_observation_result record_live_bandit_staffed_camp_signals_for_test(
    bandit_live_world::world_state &state,
    const std::vector<live_bandit_signal_observation> &signals )
{
    return record_live_bandit_staffed_camp_signals( state, signals, {} );
}

bool live_bandit_overmap_los_from_for_test( const tripoint_abs_omt &origin,
        const tripoint_abs_omt &target, const int sight_points )
{
    return live_bandit_overmap_los_from( origin, target, sight_points );
}

bool materialize_live_bandit_structural_handoffs_for_test()
{
    return materialize_live_bandit_structural_handoffs();
}

int materialize_live_bandit_response_members_for_test( const std::string &site_id )
{
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    bandit_live_world::site_record *site = state.find_site( site_id );
    return site == nullptr ? 0 : live_bandit_materialize_abstract_members_for_response(
               state, *site );
}

std::vector<bandit_live_world::response_member_power_read>
live_bandit_response_member_power_reads_for_test( const bandit_live_world::site_record &site )
{
    return live_bandit_response_member_power_reads_impl( site );
}

std::size_t maintain_live_bandit_local_pair_cohesion_for_test()
{
    return maintain_live_bandit_local_pair_cohesion().size();
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

bool advance_live_bandit_hostile_returns_for_test()
{
    return advance_live_bandit_hostile_returns();
}

bool materialize_committed_bandit_shakedown_for_test( bandit_live_world::site_record &site )
{
    return materialize_committed_bandit_shakedown( site );
}

bool complete_live_bandit_homeward_boundary_for_test()
{
    bandit_live_world::world_state &state =
        overmap_buffer.global_state.bandit_live_world;
    const std::set<character_id> homeward_member_ids =
        bandit_live_world::local_pair_homeward_travel_ids( state );
    std::map<character_id, tripoint_abs_omt> destinations;
    for( const bandit_live_world::site_record &site : state.sites ) {
        for( const character_id member_id : site.active_outing.member_ids ) {
            if( homeward_member_ids.count( member_id ) > 0 ) {
                destinations.emplace( member_id, site.anchor );
            }
        }
    }
    const std::map<character_id, live_bandit_pair_boundary_step> steps =
        live_bandit_pair_boundary_steps( destinations, nullptr, true );
    if( steps.size() != 2 ) {
        return false;
    }
    for( const auto &[member_id, step] : steps ) {
        npc *member = g->find_npc( member_id );
        if( member == nullptr || !member->is_active() ) {
            return false;
        }
        member->setpos( get_map(), get_map().get_bub( step.departure ) );
        member->path = { get_map().get_bub( step.departure ) };
    }
    return complete_live_bandit_homeward_boundary_steps( steps ).size() == 2;
}

void note_live_bandit_aftermath_for_test()
{
    note_live_bandit_aftermath();
}

bool live_cannibal_raid_advance_site_search_for_test( bandit_live_world::site_record &site )
{
    return live_cannibal_raid_advance_site_search( site );
}

void note_live_bandit_local_turn_sight_avoid_for_test()
{
    note_live_bandit_local_turn_sight_avoid();
}

bool commit_bandit_shakedown_payment_for_test( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, const int value )
{
    auto plan = live_bandit_prepare_paid_return( site, true );
    return plan && live_bandit_commit_paid_return( site, *plan, surface, value );
}

bool bandit_shakedown_response_matches_for_test( const bandit_live_world::site_record &site,
        const std::string &activity_id, const int generation, const character_id receiver_id )
{
    return live_bandit_shakedown_response_is_current( site, activity_id, generation, receiver_id );
}

bool advance_bandit_hidden_shakedown_search_for_test( bandit_live_world::site_record &site )
{
    return live_bandit_advance_hidden_shakedown_search( site );
}

void choose_bandit_shakedown_fight_for_test( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, const Character &receiver )
{
    live_bandit_choose_fight( site, surface, receiver );
}

bool consume_explicit_bandit_shakedown_fight_for_test( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, const std::string &activity_id,
        const int generation, const character_id receiver_id )
{
    return live_bandit_consume_explicit_fight( site, surface, activity_id, generation, receiver_id );
}

void hear_bandit_shakedown_demand( const sounds::robbery_demand &demand, Character &hearer )
{
    using namespace bandit_live_world;
    site_record *site = overmap_buffer.global_state.bandit_live_world.find_site( demand.site_id );
    if( site == nullptr ) {
        return;
    }
    auto &operation = site->active_hostile_operation;
    const auto &outing = operation.reservation;
    if( !operation.is_active() || operation.operation_kind != hostile_operation_kind::shakedown ||
        ( operation.phase != hostile_operation_phase::approaching &&
          operation.phase != hostile_operation_phase::committed_contact ) ||
        operation.shakedown_contact_established || !operation.shakedown_pending_branch.empty() ||
        outing.activity_id != demand.operation_id || outing.generation != demand.generation ||
        operation.shakedown_demand_emitted_turn != demand.emitted_turn ||
        operation.shakedown_demand_speaker_id != demand.speaker_id ||
        operation.shakedown_demand_volume <= 2 || outing.member_is_resolved( demand.speaker_id ) ||
        std::find( outing.member_ids.begin(), outing.member_ids.end(), demand.speaker_id ) ==
        outing.member_ids.end() || !live_bandit_shakedown_receiver_eligible( *site, hearer, false ) ) {
        return;
    }
    // Preserve the existing avatar-first preference. Other audible NPCs still
    // wake without becoming receivers, targets or operation members.
    if( operation.shakedown_receiver_id.is_valid() && !hearer.is_avatar() ) {
        return;
    }
    operation.shakedown_receiver_id = hearer.getID();
    operation.shakedown_receiver_is_avatar = hearer.is_avatar();
}

bool establish_bandit_shakedown_communication_for_test( bandit_live_world::site_record &site )
{
    return live_bandit_establish_shakedown_communication( site );
}

void cancel_bandit_shakedown_pending_pay_on_dialogue()
{
    for( auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
        if( site.active_hostile_operation.is_active() &&
            site.active_hostile_operation.operation_kind ==
            bandit_live_world::hostile_operation_kind::shakedown ) {
            live_bandit_cancel_stale_pending_pay( site, true );
        }
    }
}

std::string bandit_shakedown_communication_diagnostic( const npc &actor )
{
    const bandit_live_world::site_record *site = nullptr;
    for( const auto &candidate : overmap_buffer.global_state.bandit_live_world.sites ) {
        const auto &operation = candidate.active_hostile_operation;
        const auto &members = operation.reservation.member_ids;
        if( operation.is_active() &&
            operation.operation_kind == bandit_live_world::hostile_operation_kind::shakedown &&
            std::find( members.begin(), members.end(), actor.getID() ) != members.end() ) {
            site = &candidate;
            break;
        }
    }
    std::ostringstream stream;
    JsonOut json( stream );
    json.start_object();
    json.member( "actor_id", actor.getID().get_value() );
    json.member( "current_turn", to_turns<int>( calendar::turn - calendar::turn_zero ) );
    json.member( "provenance", "Current pure predicates and persisted contact; eligibility is not an emitted or delivered shout." );
    const bool loaded = actor.is_active() && get_map().inbounds( actor.pos_bub() );
    json.member( "actor_loaded", loaded );
    json.member( "speaker" );
    json.start_object();
    json.member( "available", loaded );
    if( loaded ) {
        const auto readiness = live_bandit_shakedown_speaker_readiness( actor );
        json.member( "eligible", readiness.ready() );
        json.member( "shout_volume", readiness.shout_volume );
        json.member( "sleep", readiness.sleeping );
        json.member( "narcosis", readiness.narcosis );
        json.member( "suspended", readiness.suspended );
        json.member( "flee", readiness.fleeing );
        json.member( "flee_temp", readiness.fleeing_temporary );
        json.member( "runaway", readiness.runaway );
        json.member( "dangerous_field", readiness.dangerous_field );
        json.member( "hostile_target", readiness.hostile_target );
        json.member( "current_target" );
        json.start_object();
        const bool target_loaded = readiness.target != nullptr &&
                                   get_map().inbounds( readiness.target->pos_bub() ) &&
                                   ( !readiness.target->is_npc() || readiness.target->as_npc()->is_active() );
        json.member( "available", target_loaded );
        if( target_loaded ) {
            json.member( "name", readiness.target->get_name() );
            json.member( "absolute_position", readiness.target->pos_abs() );
            if( readiness.target->as_character() != nullptr ) {
                json.member( "actor_id", readiness.target->as_character()->getID().get_value() );
            }
        } else {
            json.member( "reason", readiness.target == nullptr ? "no_current_target" : "target_unloaded" );
        }
        json.end_object();
    } else {
        json.member( "reason", "actor_unloaded" );
    }
    json.end_object();
    json.member( "operation_available", site != nullptr );
    json.member( "receiver" );
    json.start_object();
    Character *receiver = site != nullptr ? live_bandit_shakedown_receiver( *site ) : nullptr;
    json.member( "available", receiver != nullptr );
    if( receiver != nullptr ) {
        json.member( "actor_id", receiver->getID().get_value() );
        json.member( "is_avatar", receiver->is_avatar() );
        json.member( "hearing_available", loaded );
        if( loaded ) {
            json.member( "can_hear", receiver->can_hear( actor.pos_bub(), actor.get_shout_volume() ) );
        }
    } else {
        const Character *recorded = nullptr;
        if( site != nullptr && site->active_hostile_operation.shakedown_receiver_id.is_valid() ) {
            const auto &operation = site->active_hostile_operation;
            recorded = operation.shakedown_receiver_is_avatar ? static_cast<const Character *>( &get_avatar() ) :
                       static_cast<const Character *>( g->find_npc( operation.shakedown_receiver_id ) );
            if( recorded != nullptr && recorded->getID() != operation.shakedown_receiver_id ) {
                recorded = nullptr;
            }
        }
        const bool receiver_unloaded = recorded != nullptr &&
                                       ( !get_map().inbounds( recorded->pos_bub() ) ||
                                         ( recorded->is_npc() && !recorded->as_npc()->is_active() ) );
        json.member( "reason", receiver_unloaded ? "receiver_unloaded" :
                     recorded == nullptr ? "receiver_absent" :
                     !site->active_hostile_operation.shakedown_contact_established ?
                     "contact_not_established" : "receiver_not_eligible" );
    }
    json.end_object();
    if( site != nullptr ) {
        const auto &operation = site->active_hostile_operation;
        json.member( "site_id", site->site_id );
        json.member( "operation_id", operation.reservation.activity_id );
        json.member( "generation", operation.reservation.generation );
        json.member( "demand_emitted_turn", operation.shakedown_demand_emitted_turn );
        json.member( "demand_volume", operation.shakedown_demand_volume );
        json.member( "demand_speaker_id", operation.shakedown_demand_speaker_id.get_value() );
        json.member( "contact_established", operation.shakedown_contact_established );
        json.member( "contact_by_shout", operation.shakedown_contact_by_shout );
        json.member( "recorded_receiver_id", operation.shakedown_receiver_id.get_value() );
    }
    json.end_object();
    return stream.str();
}

bool handle_bandit_shakedown_contact_for_test( bandit_live_world::site_record &site )
{
    return live_bandit_handle_hostile_shakedown_contact( site, get_avatar() );
}

std::optional<character_id> bandit_shakedown_speaker_id_for_test( const bandit_live_world::site_record &site )
{
    const npc *speaker = live_bandit_shakedown_local_speaker( site );
    return speaker != nullptr ? std::optional<character_id>( speaker->getID() ) : std::nullopt;
}

bandit_live_world::shakedown_goods_pool bandit_encounter_goods_pool_for_test(
    const bandit_live_world::local_gate_input &input, Character &receiver )
{
    return live_bandit_encounter_goods_pool( input, receiver );
}

bool advance_live_bandit_hostile_approaches_for_test()
{
    return advance_live_bandit_hostile_approaches();
}

std::optional<bandit_live_world::canonical_hostile_operation_route>
live_bandit_hostile_operation_route_read_for_test( const bandit_live_world::site_record &site )
{
    return live_bandit_hostile_operation_route_read( site );
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
    const bool advancing_game_turn = !new_game;
    if( new_game ) {
        new_game = false;
        weather.on_game_start();
    } else {
        gamemode->per_turn();
        calendar::turn += 1_turns;
    }
    openclaw_harness_turn_trace turn_trace( advancing_game_turn,
                                            to_turns<int>( calendar::turn - calendar::turn_zero ),
                                            to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) );
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
    debug_menu::process_harness_item_setup( u.pos_bub() );
    map &m = get_map();
    // This audit hook is deliberately before the normal turn pipeline: it reads
    // the loaded save without allowing cadence, scheduler, or response work to
    // become an unacknowledged part of the comparison.
    remove_openclaw_harness_r027_avatar_onfire( u, m );
    write_openclaw_harness_r027_world_state_snapshot( u, m );
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
        turn_trace.phase_begin( "save" );
        autosave();
        turn_trace.phase_end( "save" );
    }

    weather.update_weather();

    reset_light_level();
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
        m.set_lightmap_cache_dirty( z );
    }

    perhaps_add_random_npc( /* ignore_spawn_timers_and_rates = */ false );

    // process avatar activities (ignoring user input)
    while( u.get_moves() > 0 && u.activity ) {
        openclaw_harness_trace_activity_driver( "pre_input_activity_do_turn_enter", u );
        u.activity.do_turn( u );
        openclaw_harness_trace_activity_driver( "pre_input_activity_do_turn_exit", u );
    }

    // Process NPC sound events before they move or they hear themselves talking
    for( npc &guy : all_npcs() ) {
        if( rl_dist( guy.pos_bub(), u.pos_bub() ) < MAX_VIEW_DISTANCE ) {
            sounds::process_sound_markers( &guy );
        } else {
            sounds::process_sound_markers( &guy, true );
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
                    // This trace observes the turn-loop boundary only.  The
                    // initial semantic producer lives at the render boundary,
                    // which can execute before this loop is entered.
                    openclaw_harness_trace_post_hud_pre_input_boundary( u,
                            uquit == QUIT_WATCH );
                }

                if( queue_screenshot ) {
                    take_screenshot();
                    queue_screenshot = false;
                }

                openclaw_harness_trace_activity_driver( "handle_action_enter", u );
                turn_trace.input_begin();
                const bool handle_action_returned = handle_action();
                turn_trace.input_end();
                openclaw_harness_trace_activity_driver( "handle_action_exit", u, handle_action_returned );
                if( handle_action_returned ) {
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
                    openclaw_harness_trace_activity_driver( "post_handle_action_activity_do_turn_enter", u );
                    u.activity.do_turn( u );
                    openclaw_harness_trace_activity_driver( "post_handle_action_activity_do_turn_exit", u );
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
                turn_trace.input_begin();
                handle_key_blocking_activity();
                turn_trace.input_end();
                start = now;
            }

            mon_info_update();

            // If player is performing a task, a monster is dangerously close,
            // and monster can reach to the player or it has some sort of a ranged attack,
            // warn them regardless of previous safemode warnings
            if( u.activity ) {
                for( std::pair<const distraction_type, std::string> &dist : u.activity.get_distractions() ) {
                    // This query owns a real blocking input prompt while an
                    // activity is advancing; do not charge it to simulation.
                    turn_trace.input_begin();
                    const bool activity_cancelled = cancel_activity_or_ignore_query( dist.first, dist.second );
                    turn_trace.input_end();
                    if( activity_cancelled ) {
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

    // Rider reconciliation, memory aging, loaded-z discovery, and immediate
    // recipients run once per distinct advancing turn.  Staffed observation
    // remains owned by its separate five-minute cadence.
    if( advancing_game_turn ) {
        sample_and_deliver_live_light_for_advancing_turn();
    }

    // A normal shakedown has to establish its persisted parley relationship before any
    // local NPC can classify or target the player during monmove.  The later overmap
    // pass remains responsible for travel and is idempotent at this contact boundary.
    note_live_bandit_aftermath();

    // process monster and npc turn
    monmove();
    note_live_bandit_local_turn_sight_avoid();
    // One-shot sounds are observed without waking the entire overmap update.
    // Their durable coarse memories survive until ordinary AI considers them.
    if( calendar::once_every( time_between_npc_OM_moves ) ) {
        overmap_npc_move();
    } else if( sounds::has_significant_sounds() ) {
        observe_live_bandit_sounds();
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

int advance_live_bandit_local_scout_assessments_for_test()
{
    return advance_live_bandit_local_scout_assessments();
}

bool persist_live_bandit_local_projection_leases_for_test( const bandit_live_world::site_record
        &site )
{
    return persist_live_bandit_local_projection_leases( site );
}

bool persist_live_bandit_local_progress_for_test( const bandit_live_world::site_record &before,
        const bandit_live_world::site_record &after )
{
    return persist_live_bandit_local_projection_leases( after, &before );
}

bool record_live_bandit_structural_member_returns_for_test()
{
    return record_live_bandit_structural_member_returns();
}

bool complete_loaded_live_bandit_route_arrivals_for_test()
{
    return complete_loaded_live_bandit_route_arrivals();
}

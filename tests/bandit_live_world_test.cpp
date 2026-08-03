#include "bandit_live_world.h"
#include "bandit_live_world_probe.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cata_catch.h"
#include "game_constants.h"
#include "json.h"
#include "json_loader.h"
#include "lightmap.h"
#include "npc.h"
#include "omdata.h"
#include "weather_type.h"

namespace
{
std::optional<std::string> special_lookup( const tripoint_abs_omt &omt )
{
    if( omt.z() != 0 ) {
        return std::nullopt;
    }

    if( omt.x() >= 10 && omt.x() <= 11 && omt.y() >= 20 && omt.y() <= 21 ) {
        return std::string( "bandit_camp" );
    }

    if( omt.x() >= 40 && omt.x() <= 42 && omt.y() >= 50 && omt.y() <= 52 ) {
        return std::string( "bandit_work_camp" );
    }

    if( omt.x() >= 70 && omt.x() <= 71 && omt.y() >= 80 && omt.y() <= 81 ) {
        return std::string( "cannibal_camp" );
    }

    return std::nullopt;
}

std::optional<std::string> multi_z_special_lookup( const tripoint_abs_omt &omt )
{
    const bool tracked_z = omt.z() == 0 || omt.z() == 1 || omt.z() == 5;
    if( tracked_z && omt.x() >= 10 && omt.x() <= 11 && omt.y() >= 20 && omt.y() <= 21 ) {
        return std::string( "bandit_camp" );
    }

    return std::nullopt;
}
void add_bandit_camp_member( bandit_live_world::world_state &world, int index, int id_base = 11000 )
{
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( id_base + index ),
             tripoint_abs_ms( 240 + index, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
}

void add_bandit_work_camp_member( bandit_live_world::world_state &world, int index,
                                  int id_base = 12000 )
{
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( id_base + index ),
             tripoint_abs_ms( 984 + index, 1224, 0 ), std::string( "bandit_work_camp" ),
             std::nullopt, special_lookup ) );
}

void add_cannibal_camp_member( bandit_live_world::world_state &world, int index,
                               int id_base = 12500 )
{
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter",
             character_id( id_base + index ), tripoint_abs_ms( 1680 + index, 1920, 0 ),
             std::string( "cannibal_camp" ), std::nullopt, special_lookup ) );
}

void add_scheduler_test_site( bandit_live_world::world_state &world, const int index,
                              const bool cannibal, const int id_base = 300000 )
{
    bandit_live_world::site_record site;
    site.site_id = "scheduler-site-" + std::to_string( index );
    site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
    site.site_kind = cannibal ? bandit_live_world::owned_site_kind::cannibal_camp :
                     bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = cannibal ? bandit_live_world::hostile_site_profile::cannibal_camp :
                   bandit_live_world::hostile_site_profile::camp_style;
    site.source_id = cannibal ? "cannibal_camp" : "bandit_camp";
    site.anchor = tripoint_abs_omt( index * 20, index * 20, 0 );
    site.footprint.push_back( site.anchor );
    site.living_total = 3;
    site.supply_accounted_living_total = 3;
    const tripoint_abs_ms home = project_to<coords::ms>( site.anchor );
    for( int member_index = 0; member_index < 3; ++member_index ) {
        const tripoint_abs_ms tile( home.x() + member_index, home.y(), home.z() );
        site.members.push_back( { character_id( id_base + index * 3 + member_index ),
                                  cannibal ? "cannibal_hunter" : "bandit", tile,
                                  bandit_live_world::member_state::at_home, false, "" } );
        site.spawn_tiles.push_back( { tile, 1 } );
    }
    world.sites.push_back( std::move( site ) );
}

std::optional<std::string> lookup_test_terrain(
    const std::vector<std::pair<tripoint_abs_omt, std::string>> &terrain,
    const tripoint_abs_omt &omt )
{
    for( const std::pair<tripoint_abs_omt, std::string> &entry : terrain ) {
        if( entry.first == omt ) {
            return entry.second;
        }
    }
    return std::nullopt;
}

bandit_live_world::world_state round_trip_world( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    return loaded;
}

std::string serialize_world( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    return out.str();
}

std::string serialize_camp_map_lead( const bandit_live_world::camp_map_lead &lead )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    lead.serialize( jsout );
    return out.str();
}

std::string serialize_sortie_observation(
    const bandit_live_world::sortie_observation &observation )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    observation.serialize( jsout );
    return out.str();
}

bandit_live_world::sortie_observation make_typed_visual_observation(
    const character_id observer_id, const int target_revision, const int observed_minutes,
    const std::string &fact_key,
    const bandit_live_world::sortie_observation_share_state share_state )
{
    bandit_live_world::sortie_observation observation;
    observation.fact_key = fact_key;
    observation.summary = "physically observed defenders";
    observation.confidence = 80;
    observation.observed_minutes = observed_minutes;
    observation.kind = bandit_live_world::sortie_observation_kind::certainty;
    observation.state_key = "defenders-present";
    observation.record_schema_version = 1;
    observation.source_id = "physical-source:" + fact_key;
    observation.sense = bandit_live_world::sortie_observation_sense::visual;
    observation.observer_id = observer_id;
    observation.source_omt = tripoint_abs_omt( 18, 20, 0 );
    observation.receiver_omt = tripoint_abs_omt( 10, 20, 0 );
    observation.bucket_start_minutes = observed_minutes - observed_minutes % 30;
    observation.strength = 4;
    observation.visual_quality = 3;
    observation.defender_ids = { "defender:1", "defender:2" };
    observation.simultaneity_start_minutes = std::max(
            observation.bucket_start_minutes, observed_minutes - 1 );
    observation.simultaneity_end_minutes = std::min(
            observation.bucket_start_minutes + 29, observed_minutes + 1 );
    observation.observed_power_low = 5;
    observation.observed_power_high = 8;
    observation.equipment_detail = 2;
    observation.target_revision = target_revision;
    observation.uncertainty_radius_omt = 1;
    observation.expiry_minutes = observed_minutes + 180;
    observation.share_state = share_state;
    return observation;
}

bandit_live_world::world_state make_abstract_threat_test_world( const bool cannibal,
        const int id_base, const int target_distance_omt = 4 )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        if( cannibal ) {
            add_cannibal_camp_member( world, index, id_base );
        } else {
            add_bandit_camp_member( world, index, id_base );
        }
    }
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt target( site.anchor.x() + target_distance_omt,
                                   site.anchor.y(), site.anchor.z() );
    const bandit_live_world::structural_bounty_read read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, target, read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                    site.site_id, target, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );
    REQUIRE( site.active_outing.member_ids.size() == 2 );
    return world;
}

constexpr int abstract_threat_test_stalking_minutes = 160;

bandit_live_world::abstract_threat_read make_abstract_threat_read(
    const tripoint_abs_omt &omt, const int danger, const bool overlap = true,
    const bool local_reality = false )
{
    bandit_live_world::abstract_threat_read read;
    read.observed = true;
    read.overlap = overlap;
    read.local_reality = local_reality;
    read.threat_omt = omt;
    read.danger_low = danger;
    read.danger_high = danger;
    read.stable_threat_ids = { "horde:test" };
    read.summary = "focused abstract threat fixture";
    return read;
}

bandit_live_world::structural_signal_read make_structural_signal_read(
    const bandit_live_world::sortie_observation_sense sense,
    const tripoint_abs_omt &source_omt, const int strength,
    const int confidence, const int uncertainty_radius_omt,
    const bool local_reality = false )
{
    bandit_live_world::structural_signal_read read;
    read.sense = sense;
    read.source_omt = source_omt;
    read.range_cap_omt = 40;
    read.strength = strength;
    read.confidence = confidence;
    read.uncertainty_radius_omt = uncertainty_radius_omt;
    read.local_reality = local_reality;
    read.summary = sense == bandit_live_world::sortie_observation_sense::smoke ?
                   "uncertain smoke along the committed route" :
                   "uncertain light along the committed route";
    return read;
}

bandit_live_world::world_state make_structural_signal_test_world( const bool cannibal,
        const int id_base )
{
    bandit_live_world::world_state world = make_abstract_threat_test_world(
                cannibal, id_base, 8 );
    bandit_live_world::advance_structural_bounty_outings( world, 220, {} );
    REQUIRE( world.sites.front().active_outing.phase ==
             bandit_live_world::scout_phase::observing );
    REQUIRE( world.sites.front().active_outing.observations.empty() );
    return world;
}

std::string serialize_abstract_encounter(
    const bandit_live_world::abstract_encounter_state &encounter )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    encounter.serialize( jsout );
    return out.str();
}

void erase_pretty_json_member_line( std::string &bytes, const std::string &member_name )
{
    const std::size_t member = bytes.find( "\"" + member_name + "\"" );
    REQUIRE( member != std::string::npos );
    const std::size_t colon = bytes.find( ':', member );
    REQUIRE( colon != std::string::npos );
    std::size_t value = colon + 1;
    while( value < bytes.size() && ( bytes[value] == ' ' || bytes[value] == '\t' ) ) {
        value++;
    }
    std::size_t value_end = value;
    if( value < bytes.size() && bytes[value] == '"' ) {
        value_end++;
        bool escaped = false;
        for( ; value_end < bytes.size(); value_end++ ) {
            const char current = bytes[value_end];
            if( current == '"' && !escaped ) {
                value_end++;
                break;
            }
            escaped = current == '\\' && !escaped;
            if( current != '\\' ) {
                escaped = false;
            }
        }
    } else {
        value_end = bytes.find_first_of( ",}\n", value );
    }
    REQUIRE( value_end != std::string::npos );
    while( value_end < bytes.size() && ( bytes[value_end] == ' ' || bytes[value_end] == '\t' ) ) {
        value_end++;
    }
    REQUIRE( ( value_end < bytes.size() && bytes[value_end] == ',' ) );
    value_end++;
    std::size_t member_begin = member;
    while( member_begin > 0 &&
           ( bytes[member_begin - 1] == ' ' || bytes[member_begin - 1] == '\t' ) ) {
        member_begin--;
    }
    bytes.erase( member_begin, value_end - member_begin );
}

bandit_live_world::world_state round_trip_legacy_site_world(
    const bandit_live_world::world_state &world )
{
    std::string bytes = serialize_world( world );
    const std::string current_schema = "\"schema_version\": 12";
    const std::string current_roster = "\"living_total\"";
    REQUIRE( bytes.find( current_schema ) != std::string::npos );
    REQUIRE( bytes.find( current_roster ) != std::string::npos );
    bytes.replace( bytes.find( current_schema ), current_schema.size(),
                   "\"schema_version\": 9" );
    bytes.replace( bytes.find( current_roster ), current_roster.size(), "\"headcount\"" );
    erase_pretty_json_member_line( bytes, "origin_disposition" );
    erase_pretty_json_member_line( bytes, "origin_changed_minutes" );
    erase_pretty_json_member_line( bytes, "origin_summary" );
    erase_pretty_json_member_line( bytes, "routine_activated_minutes" );
    erase_pretty_json_member_line( bytes, "last_routine_resolved_minutes" );
    erase_pretty_json_member_line( bytes, "next_routine_dispatch_eligible_minutes" );
    erase_pretty_json_member_line( bytes, "routine_no_candidate_streak" );

    JsonValue jsin = json_loader::from_string( bytes );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    return loaded;
}

void issue_test_resource_operation( bandit_live_world::world_state &world,
                                    const tripoint_abs_omt &omt,
                                    const std::string &operation_id,
                                    const int operation_generation )
{
    if( world.sites.empty() ) {
        bandit_live_world::site_record claimant;
        claimant.site_id = "resource-test-camp";
        world.sites.push_back( std::move( claimant ) );
    }
    const std::string &claimant_site_id = world.sites.front().site_id;
    bandit_live_world::site_record &claimant = world.sites.front();
    const character_id member_id( 990001 );
    if( claimant.find_member( member_id ) == nullptr ) {
        claimant.members.push_back( { member_id, "bandit", tripoint_abs_ms( 0, 0, 0 ),
                                      bandit_live_world::member_state::outbound, false,
                                      "test resource operation" } );
        claimant.spawn_tiles.push_back( { tripoint_abs_ms( 0, 0, 0 ), 1 } );
        claimant.living_total = std::max( claimant.living_total, 1 );
        claimant.supply_accounted_living_total = 1;
    } else {
        claimant.find_member( member_id )->state = bandit_live_world::member_state::outbound;
    }
    claimant.active_outing.clear();
    claimant.active_outing.kind = bandit_live_world::outing_kind::scout_sortie;
    claimant.active_outing.activity_id = operation_id;
    claimant.active_outing.camp_id = claimant_site_id;
    claimant.active_outing.generation = operation_generation;
    claimant.active_outing.member_ids = { member_id };
    claimant.active_outing.leader_id = member_id;
    claimant.active_outing.target_omt = omt;
    claimant.active_outing.job_type = "scavenge";
    claimant.active_outing.owner = bandit_live_world::simulation_owner::abstract;
    claimant.active_outing.return_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            operation_id, operation_generation, "return" );
    claimant.active_outing.report_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            operation_id, operation_generation, "report" );
    claimant.active_outing.cargo_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            operation_id, operation_generation, "cargo" );
    claimant.next_outing_generation = std::max( claimant.next_outing_generation,
                                     operation_generation + 1 );
}

bandit_live_world::finite_resource_claim_result claim_test_resource(
    bandit_live_world::world_state &world, const tripoint_abs_omt &omt,
    const bandit_live_world::finite_resource_record &expected, const int requested_units,
    const std::string &operation_id = "",
    const int operation_generation = 1 )
{
    if( world.sites.empty() ) {
        bandit_live_world::site_record claimant;
        claimant.site_id = "resource-test-camp";
        world.sites.push_back( std::move( claimant ) );
    }
    const std::string &claimant_site_id = world.sites.front().site_id;
    const std::string effective_operation_id = operation_id.empty() ?
            claimant_site_id + "#resource" : operation_id;
    const bandit_live_world::active_outing_state *issued =
        world.sites.front().active_external_outing();
    if( issued == nullptr || operation_generation > issued->generation ) {
        issue_test_resource_operation( world, omt, effective_operation_id, operation_generation );
    }
    const std::string application_key =
        bandit_live_world::finite_resource_claim_application_key(
            effective_operation_id, operation_generation, omt );
    return bandit_live_world::claim_finite_resource_units(
               world, claimant_site_id, omt, expected, requested_units,
               effective_operation_id,
               operation_generation, application_key );
}

bandit_live_world::camp_map_lead make_retention_test_lead( const int index )
{
    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "retention-lead-" + std::to_string( index );
    lead.revision = 1;
    lead.kind = bandit_live_world::camp_lead_kind::structural_bounty;
    lead.status = bandit_live_world::camp_lead_status::suspected;
    lead.target_id = "retention-target-" + std::to_string( index );
    lead.omt = tripoint_abs_omt( 100 + index, 200 + index, 0 );
    lead.first_seen_minutes = index;
    lead.last_seen_minutes = index;
    lead.bounty = 1;
    lead.confidence = 1;
    lead.source_summary = "retention fixture " + std::to_string( index );
    return lead;
}

void set_test_active_outing( bandit_live_world::site_record &site, const std::string &activity_id,
                             bandit_live_world::outing_kind kind =
                                 bandit_live_world::outing_kind::scout_sortie )
{
    site.active_outing.kind = kind;
    site.active_outing.activity_id = activity_id;
    site.active_outing.camp_id = site.site_id;
    site.active_outing.generation = site.next_outing_generation++;
    if( kind == bandit_live_world::outing_kind::scout_sortie ) {
        site.active_outing.job_type = "scout";
    }
    site.active_outing.return_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            activity_id, site.active_outing.generation, "return" );
    site.active_outing.report_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            activity_id, site.active_outing.generation, "report" );
    site.active_outing.cargo_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            activity_id, site.active_outing.generation, "cargo" );
}

std::pair<std::string, bandit_live_world::structural_outing_plan>
start_test_structural_bounty_outing( bandit_live_world::world_state &world,
                                     const std::size_t site_index,
                                     const tripoint_abs_omt &target,
                                     const int now_minutes )
{
    REQUIRE( site_index < world.sites.size() );
    bandit_live_world::site_record &site = world.sites[site_index];
    bandit_live_world::structural_bounty_read read;
    read.terrain_class = "town";
    read.terrain_fit_class = "building";
    read.bounty = 3;
    read.confidence = 1;
    read.eligible = true;
    read.summary = "three-unit structural test bounty";
    REQUIRE( read.eligible );
    REQUIRE( read.bounty == 3 );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                 site, target, read, now_minutes ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                    site.site_id, target, read.terrain_class );
    const bandit_live_world::camp_map_lead *lead =
        site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, now_minutes );
    REQUIRE( plan.valid );
    REQUIRE( plan.job == bandit_dry_run::job_template::scout );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan(
                 site, plan, now_minutes ) );
    return { lead_id, std::move( plan ) };
}

bandit_live_world::simulation_advance_cursor require_current_simulation_cursor(
    const bandit_live_world::site_record &site )
{
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( site );
    REQUIRE( cursor.has_value() );
    return *cursor;
}

void prepare_hostile_follow_on( bandit_live_world::site_record &site, const int report_revision,
                                const int scout_generation, const std::string &target_id,
                                const tripoint_abs_omt &target_omt, const int delivered_minutes,
                                const std::string &target_lead_id = "" )
{
    site.current_scout_report.revision = report_revision;
    site.current_scout_report.action_policy =
        site.profile == bandit_live_world::hostile_site_profile::cannibal_camp ?
        bandit_live_world::camp_report_policy::cannibal_night_raid :
        bandit_live_world::camp_report_policy::bandit_shakedown;
    site.current_scout_report.source_activity_id = site.site_id + "#scout:" +
            std::to_string( scout_generation );
    site.current_scout_report.source_generation = scout_generation;
    site.current_scout_report.source_job_type = "scout";
    site.current_scout_report.target_id = target_id;
    site.current_scout_report.target_omt = target_omt;
    site.current_scout_report.target_lead_id = target_lead_id;
    site.current_scout_report.target_lead_revision = 3;
    site.current_scout_report.application_key =
        site.current_scout_report.source_activity_id + ":report:" +
        std::to_string( scout_generation );
    site.current_scout_report.delivered_minutes = delivered_minutes;
    site.current_scout_report.provisional = false;
    site.next_outing_generation = std::max( site.next_outing_generation, scout_generation + 1 );
    REQUIRE( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
             bandit_live_world::camp_decision_transition_result::applied );
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 site, bandit_live_world::camp_decision_state::report_awaiting_assessment,
                 bandit_live_world::camp_decision_state::preparing_follow_on,
                 report_revision, scout_generation, delivered_minutes + 1, -1,
                 "test follow-on approved" ) ==
             bandit_live_world::camp_decision_transition_result::applied );
}

void apply_test_hostile_dispatch( bandit_live_world::site_record &site,
                                  const bandit_live_world::dispatch_plan &dispatch,
                                  const int current_minutes )
{
    const bandit_live_world::hostile_operation_kind operation_kind =
        site.profile == bandit_live_world::hostile_site_profile::cannibal_camp ?
        bandit_live_world::hostile_operation_kind::raid :
        bandit_live_world::hostile_operation_kind::shakedown;
    const bandit_live_world::hostile_operation_plan operation =
        bandit_live_world::plan_hostile_operation(
            site, operation_kind, { site.anchor, dispatch.target_omt },
            site.anchor, current_minutes );
    REQUIRE( operation.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, operation ) );
    REQUIRE( bandit_live_world::transition_hostile_operation_phase(
                 site, require_current_simulation_cursor( site ),
                 bandit_live_world::hostile_operation_phase::assembling,
                 bandit_live_world::hostile_operation_phase::outbound,
                 current_minutes + 1, "test hostile party departed" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
}

bandit_live_world::hostile_operation_transition_result transition_test_hostile_operation(
    bandit_live_world::site_record &site,
    const bandit_live_world::hostile_operation_phase expected_phase,
    const bandit_live_world::hostile_operation_phase next_phase,
    const int current_minutes, const std::string &reason )
{
    return bandit_live_world::transition_hostile_operation_phase(
               site, require_current_simulation_cursor( site ), expected_phase, next_phase,
               current_minutes, reason );
}
} // namespace

TEST_CASE( "bandit_live_world_claims_one_bounded_special_backed_site_ledger", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 101 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 102 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 103 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    REQUIRE( world.owner_id == "hells_raiders_live_owner_v0" );
    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_camp@10,20,0" );
    CHECK( site.source_kind == bandit_live_world::anchor_source_kind::overmap_special );
    CHECK( site.site_kind == bandit_live_world::owned_site_kind::bandit_camp );
    CHECK( site.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( site.anchor == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.living_total == 3 );
    REQUIRE( site.footprint.size() == 4 );
    CHECK( site.footprint.front() == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.footprint.back() == tripoint_abs_omt( 11, 21, 0 ) );
    REQUIRE( site.members.size() == 3 );
    CHECK( site.members.front().npc_template_id == "bandit" );
    REQUIRE( site.spawn_tiles.size() == 3 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->assigned_living_total == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) )->assigned_living_total == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 264, 504, 0 ) )->assigned_living_total == 1 );
}

TEST_CASE( "bandit_live_world_collapses_multi_z_special_camp_into_one_site",
           "[bandit][live_world][multi_z]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 151 ),
             tripoint_abs_ms( 240, 480, 1 ), std::string( "bandit_camp" ), std::nullopt,
             multi_z_special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 152 ),
             tripoint_abs_ms( 241, 480, 5 ), std::string( "bandit_camp" ), std::nullopt,
             multi_z_special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 153 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             multi_z_special_lookup ) );

    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_camp@10,20,0" );
    CHECK( site.anchor == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.living_total == 3 );
    REQUIRE( site.footprint.size() == 12 );
    CHECK( site.footprint.front() == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.footprint.back() == tripoint_abs_omt( 11, 21, 5 ) );
    CHECK( std::any_of( site.footprint.begin(), site.footprint.end(), []( const tripoint_abs_omt &omt ) {
        return omt == tripoint_abs_omt( 10, 20, 1 );
    } ) );
    CHECK( std::any_of( site.footprint.begin(), site.footprint.end(), []( const tripoint_abs_omt &omt ) {
        return omt == tripoint_abs_omt( 10, 20, 5 );
    } ) );
    REQUIRE( site.members.size() == 3 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 240, 480, 1 ) )->assigned_living_total == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 5 ) )->assigned_living_total == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 264, 504, 0 ) )->assigned_living_total == 1 );
}

TEST_CASE( "bandit_live_world_keeps_map_extra_hostile_spawns_as_micro_sites", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 201 ),
             tripoint_abs_ms( 120, 96, 0 ), std::nullopt, std::string( "mx_looters" ),
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 202 ),
             tripoint_abs_ms( 168, 120, 0 ), std::nullopt, std::string( "mx_bandits_block" ),
             special_lookup ) );

    REQUIRE( world.sites.size() == 2 );
    const bandit_live_world::site_record *looters = world.find_site( "map_extra:mx_looters@5,4,0" );
    REQUIRE( looters != nullptr );
    CHECK( looters->site_kind == bandit_live_world::owned_site_kind::looters );
    CHECK( looters->profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( looters->living_total == 1 );
    REQUIRE( looters->footprint.size() == 1 );
    CHECK( looters->footprint.front() == tripoint_abs_omt( 5, 4, 0 ) );

    const bandit_live_world::site_record *roadblock = world.find_site( "map_extra:mx_bandits_block@7,5,0" );
    REQUIRE( roadblock != nullptr );
    CHECK( roadblock->site_kind == bandit_live_world::owned_site_kind::bandits_block );
    CHECK( roadblock->profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( roadblock->living_total == 1 );
    REQUIRE( roadblock->footprint.size() == 1 );
    CHECK( roadblock->footprint.front() == tripoint_abs_omt( 7, 5, 0 ) );
}

TEST_CASE( "bandit_live_world_registers_abstract_special_before_npc_materialization",
           "[bandit][live_world][abstract_bootstrap]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::register_abstract_site( world,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_work_camp",
             tripoint_abs_omt( 41, 51, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_work_camp ) ) );

    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_work_camp@40,50,0" );
    CHECK( site.source_kind == bandit_live_world::anchor_source_kind::overmap_special );
    CHECK( site.site_kind == bandit_live_world::owned_site_kind::bandit_work_camp );
    CHECK( site.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( site.anchor == tripoint_abs_omt( 40, 50, 0 ) );
    CHECK( site.living_total == 6 );
    CHECK( site.members.empty() );
    REQUIRE( site.footprint.size() == 9 );
    CHECK( site.footprint.front() == tripoint_abs_omt( 40, 50, 0 ) );
    CHECK( site.footprint.back() == tripoint_abs_omt( 42, 52, 0 ) );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.site_id == site.site_id );
    CHECK( loaded_site.living_total == 6 );
    CHECK( loaded_site.members.empty() );
    REQUIRE( loaded_site.footprint.size() == 9 );
}

TEST_CASE( "bandit_live_world_reconciles_materialized_spawns_with_abstract_specials",
           "[bandit][live_world][abstract_bootstrap]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::register_abstract_site( world,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_camp ) ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 203 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 204 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_camp@10,20,0" );
    CHECK( site.living_total == 6 );
    REQUIRE( site.members.size() == 2 );
    CHECK( site.members.front().npc_id == character_id( 203 ) );
    CHECK( site.members.back().npc_id == character_id( 204 ) );
    REQUIRE( site.spawn_tiles.size() == 2 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->assigned_living_total == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 264, 504, 0 ) )->assigned_living_total == 1 );
}

TEST_CASE( "bandit_live_world_derives_disjoint_roster_authorities",
           "[bandit][live_world][roster]" )
{
    bandit_live_world::world_state partial_world;
    REQUIRE( bandit_live_world::register_abstract_site(
                 partial_world, bandit_live_world::anchor_source_kind::overmap_special,
                 "bandit_camp", tripoint_abs_omt( 10, 20, 0 ), special_lookup, 6 ) );
    bandit_live_world::site_record &partial_site = partial_world.sites.front();
    bandit_live_world::roster_view roster = partial_site.roster();
    REQUIRE( roster.valid );
    CHECK( roster.living_total == 6 );
    CHECK( roster.materialized_living_total == 0 );
    CHECK( roster.unmaterialized_home_total == 6 );
    CHECK( roster.physically_present_total == 6 );
    CHECK( roster.ready_total == 6 );
    CHECK( roster.physically_present_ids.empty() );
    CHECK( roster.away_ids.empty() );
    CHECK( roster.reserved_unresolved_ids.empty() );

    for( int index = 0; index < 3; ++index ) {
        add_bandit_camp_member( partial_world, index, 47100 );
    }
    partial_site.find_member( character_id( 47100 ) )->wounded_or_unready = true;
    roster = partial_site.roster();
    REQUIRE( roster.valid );
    CHECK( roster.living_total == 6 );
    CHECK( roster.materialized_living_total == 3 );
    CHECK( roster.unmaterialized_home_total == 3 );
    CHECK( roster.physically_present_total == 6 );
    CHECK( roster.ready_concrete_total == 2 );
    CHECK( roster.ready_total == 5 );
    REQUIRE( roster.physically_present_ids.size() == 3 );

    bandit_live_world::world_state pair_world;
    add_bandit_camp_member( pair_world, 0, 47200 );
    add_bandit_camp_member( pair_world, 1, 47200 );
    bandit_live_world::site_record &pair_site = pair_world.sites.front();
    set_test_active_outing( pair_site, pair_site.site_id + "#scout:pair" );
    pair_site.active_outing.member_ids = { character_id( 47200 ), character_id( 47201 ) };
    pair_site.active_outing.leader_id = character_id( 47200 );
    REQUIRE( bandit_live_world::update_member_state(
                 pair_site, character_id( 47200 ), bandit_live_world::member_state::outbound,
                 "paired roster test" ) );
    REQUIRE( bandit_live_world::update_member_state(
                 pair_site, character_id( 47201 ), bandit_live_world::member_state::outbound,
                 "paired roster test" ) );
    roster = pair_site.roster();
    REQUIRE( roster.valid );
    CHECK( roster.living_total == 2 );
    CHECK( roster.physically_present_total == 0 );
    CHECK( roster.ready_total == 0 );
    CHECK( roster.away_ids == pair_site.active_outing.member_ids );
    CHECK( roster.reserved_unresolved_ids == pair_site.active_outing.member_ids );
    CHECK_FALSE( pair_site.eligible_for_empty_site_retirement() );

    bandit_live_world::world_state hostile_world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( hostile_world, index, 47300 );
    }
    bandit_live_world::site_record &hostile_site = hostile_world.sites.front();
    prepare_hostile_follow_on( hostile_site, 2, 1, "roster-target",
                               tripoint_abs_omt( 18, 20, 0 ), 100 );
    const bandit_live_world::hostile_operation_plan hostile_plan =
        bandit_live_world::plan_hostile_operation(
            hostile_site, bandit_live_world::hostile_operation_kind::shakedown,
            { hostile_site.anchor, tripoint_abs_omt( 18, 20, 0 ) },
            hostile_site.anchor, 102 );
    REQUIRE( hostile_plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( hostile_site, hostile_plan ) );
    roster = hostile_site.roster();
    REQUIRE( roster.valid );
    CHECK( roster.physically_present_total == 6 );
    CHECK( roster.away_ids.empty() );
    CHECK( roster.reserved_unresolved_ids == hostile_plan.operation.reservation.member_ids );
    CHECK( roster.ready_concrete_total == 4 );
}

TEST_CASE( "bandit_live_world_routine_policy_sizes_exact_pairs_and_materialization",
           "[bandit][live_world][routine_policy]" )
{
    const auto make_site = []( const bandit_live_world::hostile_site_profile profile,
    const int living_total ) {
        bandit_live_world::site_record site;
        site.site_id = profile == bandit_live_world::hostile_site_profile::cannibal_camp ?
                       "routine-cannibal" : "routine-bandit";
        site.profile = profile;
        site.site_kind = profile == bandit_live_world::hostile_site_profile::cannibal_camp ?
                         bandit_live_world::owned_site_kind::cannibal_camp :
                         bandit_live_world::owned_site_kind::bandit_camp;
        site.living_total = living_total;
        for( int index = 0; index < living_total; ++index ) {
            site.members.push_back( { character_id( 47600 + index ),
                                      profile == bandit_live_world::hostile_site_profile::cannibal_camp ?
                                      "cannibal_hunter" : "bandit",
                                      tripoint_abs_ms( 240 + index, 480, 0 ),
                                      bandit_live_world::member_state::at_home, false, "" } );
        }
        return site;
    };

    const std::vector<bandit_live_world::hostile_site_profile> camp_profiles = {
        bandit_live_world::hostile_site_profile::camp_style,
        bandit_live_world::hostile_site_profile::cannibal_camp,
    };
    std::vector<std::pair<std::string, bandit_live_world::routine_member_readiness_snapshot>>
    unready_variants;
    bandit_live_world::routine_member_readiness_snapshot wounded;
    wounded.hp_percent = 50;
    unready_variants.emplace_back( "wounded", wounded );
    bandit_live_world::routine_member_readiness_snapshot sleeping;
    sleeping.sleeping = true;
    unready_variants.emplace_back( "sleeping", sleeping );
    bandit_live_world::routine_member_readiness_snapshot incapacitated;
    incapacitated.incapacitated = true;
    unready_variants.emplace_back( "incapacitated", incapacitated );
    for( const bandit_live_world::hostile_site_profile profile : camp_profiles ) {
        for( int population = 0; population <= 10; ++population ) {
            const bandit_live_world::site_record site = make_site( profile, population );
            const bandit_live_world::routine_scout_policy_result policy =
                bandit_live_world::routine_scout_policy( site );
            const bandit_live_world::roster_view roster = site.roster();
            CAPTURE( static_cast<int>( profile ), population );
            REQUIRE( roster.valid );
            CHECK( roster.living_total == population );
            CHECK( roster.ready_total == population );
            CHECK( policy.applies );
            CHECK( policy.eligible == ( population >= 2 ) );
            CHECK( policy.party_size == ( population >= 2 ? 2 : 0 ) );
            CHECK( policy.required_local_reserve == ( population >= 3 ? 1 : 0 ) );
            CHECK( policy.concrete_ready_goal == ( population >= 3 ? 3 :
                                                   population == 2 ? 2 : 0 ) );

            for( const auto &unready_variant : unready_variants ) {
                bandit_live_world::site_record unready_site = make_site( profile, population );
                if( population > 0 ) {
                    unready_site.members.front().wounded_or_unready =
                        bandit_live_world::routine_member_is_unready( unready_variant.second );
                }
                const bool expected_eligible = population >= 4;
                const bandit_live_world::routine_scout_policy_result unready_policy =
                    bandit_live_world::routine_scout_policy( unready_site );
                const bandit_live_world::routine_scout_pair_selection_result unready_pair =
                    bandit_live_world::select_routine_scout_pair( unready_site );
                const bandit_live_world::roster_view unready_roster = unready_site.roster();
                CAPTURE( unready_variant.first );
                REQUIRE( unready_roster.valid );
                CHECK( unready_roster.living_total == population );
                CHECK( unready_roster.ready_total == std::max( 0, population - 1 ) );
                CHECK( unready_policy.eligible == expected_eligible );
                CHECK( unready_pair.eligible == expected_eligible );
                if( unready_pair.eligible ) {
                    CHECK( std::find( unready_pair.member_ids.begin(),
                                      unready_pair.member_ids.end(),
                                      unready_site.members.front().npc_id ) ==
                           unready_pair.member_ids.end() );
                }
            }

            bandit_live_world::site_record missing_site = make_site( profile, population );
            if( population > 0 ) {
                REQUIRE( bandit_live_world::update_member_state(
                             missing_site, missing_site.members.front().npc_id,
                             bandit_live_world::member_state::missing,
                             "population matrix missing member" ) );
            }
            CHECK( missing_site.living_total == std::max( 0, population - 1 ) );
            REQUIRE( missing_site.roster().valid );
            CHECK( missing_site.roster().ready_total == std::max( 0, population - 1 ) );
            CHECK( bandit_live_world::routine_scout_policy( missing_site ).eligible ==
                   ( population >= 3 ) );
            CHECK( bandit_live_world::select_routine_scout_pair( missing_site ).eligible ==
                   ( population >= 3 ) );

            if( population >= 2 ) {
                bandit_live_world::site_record reserved_site = make_site( profile, population );
                set_test_active_outing( reserved_site, reserved_site.site_id + "#matrix-active" );
                reserved_site.active_outing.member_ids = {
                    reserved_site.members[0].npc_id, reserved_site.members[1].npc_id
                };
                reserved_site.active_outing.leader_id = reserved_site.members[0].npc_id;
                for( const character_id &member_id : reserved_site.active_outing.member_ids ) {
                    REQUIRE( bandit_live_world::update_member_state(
                                 reserved_site, member_id,
                                 bandit_live_world::member_state::outbound,
                                 "population matrix active reservation" ) );
                }
                const bandit_live_world::roster_view reserved_roster = reserved_site.roster();
                REQUIRE( reserved_roster.valid );
                CHECK( reserved_roster.living_total == population );
                CHECK( reserved_roster.ready_total == population - 2 );
                CHECK( reserved_roster.away_ids == reserved_site.active_outing.member_ids );
                const bandit_live_world::routine_scout_policy_result reserved_policy =
                    bandit_live_world::routine_scout_policy( reserved_site );
                CHECK_FALSE( reserved_policy.eligible );
                CHECK( reserved_policy.party_size == 0 );
                CHECK( reserved_policy.required_local_reserve == 0 );
                CHECK( reserved_policy.concrete_ready_goal == 0 );
                CHECK_FALSE( bandit_live_world::select_routine_scout_pair( reserved_site ).eligible );
            }
        }
    }

    bandit_live_world::site_record abstract_six = make_site(
                bandit_live_world::hostile_site_profile::camp_style, 0 );
    abstract_six.living_total = 6;
    CHECK( bandit_live_world::routine_scout_materialization_count( abstract_six ) == 3 );
    abstract_six.members = make_site(
                               bandit_live_world::hostile_site_profile::camp_style, 3 ).members;
    CHECK( bandit_live_world::routine_scout_materialization_count( abstract_six ) == 0 );

    bandit_live_world::site_record abstract_pair = make_site(
                bandit_live_world::hostile_site_profile::cannibal_camp, 0 );
    abstract_pair.living_total = 2;
    CHECK( bandit_live_world::routine_scout_materialization_count( abstract_pair ) == 2 );

    bandit_live_world::site_record wounded_concrete = make_site(
                bandit_live_world::hostile_site_profile::camp_style, 3 );
    wounded_concrete.living_total = 6;
    wounded_concrete.members.front().wounded_or_unready = true;
    REQUIRE( bandit_live_world::routine_scout_policy( wounded_concrete ).eligible );
    CHECK( bandit_live_world::routine_scout_materialization_count( wounded_concrete ) == 1 );

    bandit_live_world::site_record under_ready = make_site(
                bandit_live_world::hostile_site_profile::cannibal_camp, 3 );
    under_ready.members.front().wounded_or_unready = true;
    const bandit_live_world::routine_scout_policy_result under_ready_policy =
        bandit_live_world::routine_scout_policy( under_ready );
    CHECK_FALSE( under_ready_policy.eligible );
    CHECK( under_ready_policy.party_size == 2 );

    bandit_live_world::site_record active_pair = make_site(
                bandit_live_world::hostile_site_profile::camp_style, 3 );
    set_test_active_outing( active_pair, active_pair.site_id + "#scout:1" );
    active_pair.active_outing.member_ids = { character_id( 47600 ), character_id( 47601 ) };
    active_pair.active_outing.leader_id = character_id( 47600 );
    REQUIRE( bandit_live_world::update_member_state(
                 active_pair, character_id( 47600 ), bandit_live_world::member_state::outbound,
                 "routine policy active pair" ) );
    REQUIRE( bandit_live_world::update_member_state(
                 active_pair, character_id( 47601 ), bandit_live_world::member_state::outbound,
                 "routine policy active pair" ) );
    CHECK_FALSE( bandit_live_world::routine_scout_policy( active_pair ).eligible );
    CHECK( bandit_live_world::routine_scout_materialization_count( active_pair ) == 0 );

    bandit_live_world::site_record large_bandit = make_site(
                bandit_live_world::hostile_site_profile::camp_style, 10 );
    CHECK( bandit_live_world::routine_scout_policy( large_bandit ).party_size == 2 );
    const bandit_live_world::response_party_policy_result bandit_response =
        bandit_live_world::response_party_policy(
            large_bandit, bandit_dry_run::job_template::raid );
    REQUIRE( bandit_response.eligible );
    CHECK( bandit_response.party_size == 2 );
    CHECK( bandit_response.required_local_reserve >= 3 );

    bandit_live_world::site_record large_cannibal = make_site(
                bandit_live_world::hostile_site_profile::cannibal_camp, 10 );
    CHECK( bandit_live_world::routine_scout_policy( large_cannibal ).party_size == 2 );
    const bandit_live_world::response_party_policy_result cannibal_response =
        bandit_live_world::response_party_policy(
            large_cannibal, bandit_dry_run::job_template::raid );
    REQUIRE( cannibal_response.eligible );
    CHECK( cannibal_response.party_size == 3 );
    CHECK( cannibal_response.required_local_reserve == 2 );
    const bandit_live_world::response_party_policy_result undersized_cannibal_response =
        bandit_live_world::response_party_policy(
            large_cannibal, bandit_dry_run::job_template::raid, 1 );
    CHECK_FALSE( undersized_cannibal_response.eligible );

    bandit_live_world::camp_map_lead rich_stale_lead;
    rich_stale_lead.status = bandit_live_world::camp_lead_status::stale;
    rich_stale_lead.bounty = 1000;
    rich_stale_lead.threat = 0;
    rich_stale_lead.confidence = 1;
    for( const bandit_live_world::site_record *site : { &large_bandit, &large_cannibal } ) {
        const bandit_live_world::camp_map_dispatch_decision decision =
            bandit_live_world::choose_camp_map_dispatch( *site, rich_stale_lead );
        CHECK( decision.intent == bandit_dry_run::job_template::scout );
        CHECK( decision.selected_member_count == 2 );
    }

    bandit_live_world::camp_map_lead dangerous_stale_lead = rich_stale_lead;
    dangerous_stale_lead.bounty = 1;
    dangerous_stale_lead.threat = 4;
    for( const bandit_live_world::site_record *site : { &large_bandit, &large_cannibal } ) {
        const bandit_live_world::camp_map_dispatch_decision decision =
            bandit_live_world::choose_camp_map_dispatch( *site, dangerous_stale_lead );
        CHECK( decision.intent == bandit_dry_run::job_template::hold_chill );
        CHECK( decision.selected_member_count == 0 );
    }

    bandit_live_world::site_record micro_site = make_site(
                bandit_live_world::hostile_site_profile::small_hostile_site, 1 );
    CHECK_FALSE( bandit_live_world::routine_scout_policy( micro_site ).applies );
    const bandit_live_world::response_party_policy_result micro_response =
        bandit_live_world::response_party_policy(
            micro_site, bandit_dry_run::job_template::stalk );
    REQUIRE( micro_response.eligible );
    CHECK( micro_response.party_size == 1 );
}

TEST_CASE( "bandit_live_world_selects_ready_capable_observer_and_lightest_safe_escort",
           "[bandit][live_world][routine_policy][capability]" )
{
    bandit_live_world::routine_member_readiness_snapshot readiness;
    CHECK_FALSE( bandit_live_world::routine_member_is_unready( readiness ) );
    readiness.hp_percent = 51;
    CHECK_FALSE( bandit_live_world::routine_member_is_unready( readiness ) );
    readiness.hp_percent = 50;
    CHECK( bandit_live_world::routine_member_is_unready( readiness ) );
    readiness = {};
    readiness.present = false;
    CHECK( bandit_live_world::routine_member_is_unready( readiness ) );
    readiness = {};
    readiness.dead = true;
    CHECK( bandit_live_world::routine_member_is_unready( readiness ) );
    readiness = {};
    readiness.sleeping = true;
    CHECK( bandit_live_world::routine_member_is_unready( readiness ) );
    readiness = {};
    readiness.incapacitated = true;
    CHECK( bandit_live_world::routine_member_is_unready( readiness ) );

    bandit_live_world::world_state bandit_world;
    const std::vector<std::string> bandit_templates = {
        "hells_raiders_boss", "bandit_trader", "thug", "bandit", "bandit_mechanic"
    };
    for( int index = 0; index < static_cast<int>( bandit_templates.size() ); ++index ) {
        REQUIRE( bandit_live_world::claim_tracked_spawn(
                     bandit_world, bandit_templates[index], character_id( 47700 + index ),
                     tripoint_abs_ms( 240 + index, 480, 0 ), std::string( "bandit_camp" ),
                     std::nullopt, special_lookup ) );
    }
    bandit_live_world::site_record &bandit_site = bandit_world.sites.front();
    const bandit_live_world::routine_scout_pair_selection_result bandit_pair =
        bandit_live_world::select_routine_scout_pair( bandit_site );
    REQUIRE( bandit_pair.eligible );
    CHECK( bandit_pair.observer_id == character_id( 47703 ) );
    CHECK( bandit_pair.escort_id == character_id( 47704 ) );
    CHECK( bandit_pair.observer_capability == 3 );
    CHECK( bandit_pair.escort_capability == 2 );
    CHECK( bandit_pair.return_safe_escort );
    const std::vector<character_id> expected_bandit_pair = {
        character_id( 47703 ), character_id( 47704 )
    };
    CHECK( bandit_pair.member_ids == expected_bandit_pair );

    bandit_live_world::site_record reordered_bandit_site = bandit_site;
    std::reverse( reordered_bandit_site.members.begin(), reordered_bandit_site.members.end() );
    CHECK( bandit_live_world::select_routine_scout_pair(
               reordered_bandit_site ).member_ids == expected_bandit_pair );

    bandit_site.find_member( character_id( 47704 ) )->wounded_or_unready = true;
    const bandit_live_world::routine_scout_pair_selection_result fallback_escort =
        bandit_live_world::select_routine_scout_pair( bandit_site );
    REQUIRE( fallback_escort.eligible );
    CHECK( fallback_escort.observer_id == character_id( 47703 ) );
    CHECK( fallback_escort.escort_id == character_id( 47702 ) );
    bandit_site.find_member( character_id( 47704 ) )->wounded_or_unready = false;

    bandit_live_world::site_record unsafe_escort_site = bandit_site;
    for( bandit_live_world::member_record &member : unsafe_escort_site.members ) {
        if( member.npc_id != character_id( 47703 ) ) {
            member.npc_template_id = "bandit_trader";
        }
    }
    const bandit_live_world::routine_scout_pair_selection_result unsafe_pair =
        bandit_live_world::select_routine_scout_pair( unsafe_escort_site );
    CHECK_FALSE( unsafe_pair.eligible );
    CHECK( unsafe_pair.rejection_reason ==
           "no second ready member has return-safe escort capability" );

    bandit_live_world::camp_map_lead stale_lead;
    stale_lead.lead_id = "capability-scout-lead";
    stale_lead.status = bandit_live_world::camp_lead_status::stale;
    stale_lead.target_id = "capability-target";
    stale_lead.omt = tripoint_abs_omt( 18, 20, 0 );
    stale_lead.bounty = 8;
    stale_lead.confidence = 1;
    bandit_site.intelligence_map.leads.push_back( stale_lead );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( bandit_site, stale_lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
    CHECK( plan.member_ids == expected_bandit_pair );

    bandit_site.find_member( bandit_pair.observer_id )->wounded_or_unready = true;
    const std::string before_stale_apply = serialize_world( bandit_world );
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( bandit_site, plan ) );
    CHECK( serialize_world( bandit_world ) == before_stale_apply );
    bandit_site.find_member( bandit_pair.observer_id )->wounded_or_unready = false;
    REQUIRE( bandit_live_world::apply_dispatch_plan( bandit_site, plan ) );
    CHECK( bandit_site.active_outing.member_ids == expected_bandit_pair );
    CHECK( bandit_site.active_outing.leader_id == bandit_pair.observer_id );

    bandit_live_world::world_state cannibal_world;
    const std::vector<std::string> cannibal_templates = {
        "cannibal_camp_leader", "cannibal_butcher", "cannibal_hunter", "cannibal_hunter"
    };
    for( int index = 0; index < static_cast<int>( cannibal_templates.size() ); ++index ) {
        REQUIRE( bandit_live_world::claim_tracked_spawn(
                     cannibal_world, cannibal_templates[index], character_id( 47800 + index ),
                     tripoint_abs_ms( 1680 + index, 1920, 0 ), std::string( "cannibal_camp" ),
                     std::nullopt, special_lookup ) );
    }
    bandit_live_world::site_record &cannibal_site = cannibal_world.sites.front();
    const bandit_live_world::routine_scout_pair_selection_result cannibal_pair =
        bandit_live_world::select_routine_scout_pair( cannibal_site );
    REQUIRE( cannibal_pair.eligible );
    CHECK( cannibal_pair.observer_id == character_id( 47802 ) );
    CHECK( cannibal_pair.escort_id == character_id( 47803 ) );
    CHECK( cannibal_pair.return_safe_escort );
    cannibal_site.find_member( character_id( 47803 ) )->wounded_or_unready = true;
    const bandit_live_world::routine_scout_pair_selection_result cannibal_fallback =
        bandit_live_world::select_routine_scout_pair( cannibal_site );
    REQUIRE( cannibal_fallback.eligible );
    CHECK( cannibal_fallback.observer_id == character_id( 47802 ) );
    CHECK( cannibal_fallback.escort_id == character_id( 47801 ) );
}

TEST_CASE( "bandit_live_world_migrates_and_strictly_validates_roster_authority",
           "[bandit][live_world][roster][migration]" )
{
    JsonValue legacy_json = json_loader::from_string(
        R"({"schema_version":5,"site_id":"legacy-roster","headcount":1,"members":[{"npc_id":47400,"state":"at_home"},{"npc_id":47401,"state":"at_home"}]})" );
    bandit_live_world::site_record migrated;
    migrated.deserialize( legacy_json.get_object() );
    CHECK( migrated.schema_version == 12 );
    CHECK( migrated.living_total == 2 );
    REQUIRE( migrated.roster().valid );
    CHECK( migrated.roster().physically_present_total == 2 );
    REQUIRE( migrated.spawn_tiles.size() == 1 );
    CHECK( migrated.spawn_tiles.front().assigned_living_total == 2 );
    CHECK( migrated.members.front().npc_template_id == "bandit" );
    CHECK( migrated.members.back().npc_template_id == "bandit" );

    bandit_live_world::world_state migrated_world;
    migrated_world.sites.push_back( migrated );
    const std::string migrated_bytes = serialize_world( migrated_world );
    CHECK( migrated_bytes.find( "\"living_total\": 2" ) != std::string::npos );
    CHECK( migrated_bytes.find( "\"headcount\"" ) == std::string::npos );

    JsonValue legacy_tile_json = json_loader::from_string(
        R"({"schema_version":5,"site_id":"legacy-tile-roster","headcount":0,"spawn_tiles":[{"tile":[240,480,0],"headcount":2}]})" );
    bandit_live_world::site_record migrated_tile_site;
    migrated_tile_site.deserialize( legacy_tile_json.get_object() );
    CHECK( migrated_tile_site.living_total == 2 );
    REQUIRE( migrated_tile_site.roster().valid );
    CHECK( migrated_tile_site.roster().physically_present_total == 2 );

    JsonValue duplicate_legacy_tiles_json = json_loader::from_string(
        R"({"schema_version":5,"site_id":"legacy-duplicate-tiles","headcount":0,"spawn_tiles":[{"tile":[240,480,0],"headcount":1},{"tile":[240,480,0],"headcount":2}]})" );
    bandit_live_world::site_record merged_legacy_tiles;
    merged_legacy_tiles.deserialize( duplicate_legacy_tiles_json.get_object() );
    CHECK( merged_legacy_tiles.living_total == 2 );
    REQUIRE( merged_legacy_tiles.spawn_tiles.size() == 1 );
    CHECK( merged_legacy_tiles.spawn_tiles.front().assigned_living_total == 2 );

    bandit_live_world::world_state materialized_tile_world;
    REQUIRE( bandit_live_world::register_abstract_site(
                 materialized_tile_world, bandit_live_world::anchor_source_kind::overmap_special,
                 "bandit_camp", tripoint_abs_omt( 10, 20, 0 ), special_lookup, 2 ) );
    bandit_live_world::site_record &materialized_tile_site =
        materialized_tile_world.sites.front();
    const tripoint_abs_ms assigned_tile( 240, 480, 0 );
    const tripoint_abs_ms materialized_elsewhere( 241, 480, 0 );
    materialized_tile_site.spawn_tiles.push_back( { assigned_tile, 2 } );
    REQUIRE( bandit_live_world::claim_tracked_spawn(
                 materialized_tile_world, "bandit", character_id( 47420 ), materialized_elsewhere,
                 std::string( "bandit_camp" ), std::nullopt, special_lookup ) );
    CHECK( materialized_tile_site.living_total == 2 );
    CHECK( materialized_tile_site.find_spawn_tile( assigned_tile )->assigned_living_total == 1 );
    CHECK( materialized_tile_site.find_spawn_tile(
               materialized_elsewhere )->assigned_living_total == 1 );
    REQUIRE( bandit_live_world::claim_tracked_spawn(
                 materialized_tile_world, "bandit", character_id( 47421 ), assigned_tile,
                 std::string( "bandit_camp" ), std::nullopt, special_lookup ) );
    CHECK( materialized_tile_site.living_total == 2 );
    CHECK( materialized_tile_site.find_spawn_tile( assigned_tile )->assigned_living_total == 1 );
    CHECK( materialized_tile_site.find_spawn_tile(
               materialized_elsewhere )->assigned_living_total == 1 );
    CHECK( serialize_world( round_trip_world( materialized_tile_world ) ) ==
           serialize_world( materialized_tile_world ) );

    JsonValue legacy_orphan_json = json_loader::from_string(
        R"({"schema_version":5,"site_id":"legacy-orphan-away","headcount":1,"members":[{"npc_id":47410,"state":"outbound"}]})" );
    bandit_live_world::site_record migrated_orphan_site;
    migrated_orphan_site.deserialize( legacy_orphan_json.get_object() );
    REQUIRE( migrated_orphan_site.roster().valid );
    REQUIRE( migrated_orphan_site.members.size() == 1 );
    CHECK( migrated_orphan_site.members.front().state ==
           bandit_live_world::member_state::at_home );
    CHECK( migrated_orphan_site.members.front().last_writeback_summary ==
           "returned orphaned legacy away member home" );

    bandit_live_world::world_state protected_world;
    add_bandit_camp_member( protected_world, 0, 47500 );
    add_bandit_camp_member( protected_world, 1, 47500 );
    const std::string protected_bytes = serialize_world( protected_world );
    const auto require_atomic_rejection = [&]( std::string malformed_bytes ) {
        JsonValue malformed_json = json_loader::from_string( malformed_bytes );
        CHECK_THROWS( protected_world.deserialize( malformed_json.get_object() ) );
        CHECK( serialize_world( protected_world ) == protected_bytes );
    };

    std::string too_small = protected_bytes;
    const std::string living_two = "\"living_total\": 2";
    REQUIRE( too_small.find( living_two ) != std::string::npos );
    too_small.replace( too_small.find( living_two ), living_two.size(), "\"living_total\": 1" );
    require_atomic_rejection( too_small );

    std::string duplicate_id = protected_bytes;
    const std::string second_id = "\"npc_id\": 47501";
    REQUIRE( duplicate_id.find( second_id ) != std::string::npos );
    duplicate_id.replace( duplicate_id.find( second_id ), second_id.size(),
                          "\"npc_id\": 47500" );
    require_atomic_rejection( duplicate_id );

    std::string orphan_away = protected_bytes;
    const std::string at_home = "\"state\": \"at_home\"";
    REQUIRE( orphan_away.find( at_home ) != std::string::npos );
    orphan_away.replace( orphan_away.find( at_home ), at_home.size(),
                         "\"state\": \"outbound\"" );
    require_atomic_rejection( orphan_away );

    std::string undersized_tile = protected_bytes;
    const std::string assigned_one = "\"assigned_living_total\": 1";
    REQUIRE( undersized_tile.find( assigned_one ) != std::string::npos );
    undersized_tile.replace( undersized_tile.find( assigned_one ), assigned_one.size(),
                             "\"assigned_living_total\": 0" );
    require_atomic_rejection( undersized_tile );

    std::string oversized_assignments = protected_bytes;
    REQUIRE( oversized_assignments.find( assigned_one ) != std::string::npos );
    oversized_assignments.replace( oversized_assignments.find( assigned_one ),
                                   assigned_one.size(), "\"assigned_living_total\": 2" );
    require_atomic_rejection( oversized_assignments );

    std::string duplicate_current_tile = protected_bytes;
    const std::string second_tile = "\"tile\": [ 241, 480, 0 ]";
    REQUIRE( duplicate_current_tile.find( second_tile ) != std::string::npos );
    duplicate_current_tile.replace( duplicate_current_tile.find( second_tile ),
                                    second_tile.size(), "\"tile\": [ 240, 480, 0 ]" );
    require_atomic_rejection( duplicate_current_tile );

    std::string missing_concrete_tile = protected_bytes;
    REQUIRE( missing_concrete_tile.find( second_tile ) != std::string::npos );
    missing_concrete_tile.replace( missing_concrete_tile.find( second_tile ),
                                   second_tile.size(), "\"tile\": [ 242, 480, 0 ]" );
    require_atomic_rejection( missing_concrete_tile );

    std::string incomplete_supply = protected_bytes;
    const std::string supply_key = "\"supply_units\"";
    REQUIRE( incomplete_supply.find( supply_key ) != std::string::npos );
    incomplete_supply.replace( incomplete_supply.find( supply_key ), supply_key.size(),
                               "\"missing_supply_units\"" );
    require_atomic_rejection( incomplete_supply );

    bandit_live_world::world_state active_world;
    add_bandit_camp_member( active_world, 0, 47600 );
    add_bandit_camp_member( active_world, 1, 47600 );
    bandit_live_world::site_record &active_site = active_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state(
                 active_site, character_id( 47600 ), bandit_live_world::member_state::outbound,
                 "strict owner fixture" ) );
    set_test_active_outing( active_site, active_site.site_id + "#scout:strict-owner" );
    active_site.active_outing.member_ids = { character_id( 47600 ) };
    active_site.active_outing.leader_id = character_id( 47600 );
    const std::string active_bytes = serialize_world( active_world );
    std::string wrong_owner = active_bytes;
    const std::string correct_camp = "\"camp_id\": \"" + active_site.site_id + "\"";
    REQUIRE( wrong_owner.find( correct_camp ) != std::string::npos );
    wrong_owner.replace( wrong_owner.find( correct_camp ), correct_camp.size(),
                         "\"camp_id\": \"wrong-camp\"" );
    JsonValue wrong_owner_json = json_loader::from_string( wrong_owner );
    CHECK_THROWS( active_world.deserialize( wrong_owner_json.get_object() ) );
    CHECK( serialize_world( active_world ) == active_bytes );

    std::string downgraded_nested_owner = active_bytes;
    const std::string active_schema = "\"active_outing\": { \"schema_version\": 5";
    REQUIRE( downgraded_nested_owner.find( active_schema ) != std::string::npos );
    downgraded_nested_owner.replace( downgraded_nested_owner.find( active_schema ),
                                     active_schema.size(),
                                     "\"active_outing\": { \"schema_version\": 3" );
    JsonValue downgraded_owner_json = json_loader::from_string( downgraded_nested_owner );
    CHECK_THROWS( active_world.deserialize( downgraded_owner_json.get_object() ) );
    CHECK( serialize_world( active_world ) == active_bytes );

    bandit_live_world::world_state hostile_kind_world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( hostile_kind_world, index, 47700 );
    }
    bandit_live_world::site_record &hostile_kind_site = hostile_kind_world.sites.front();
    prepare_hostile_follow_on( hostile_kind_site, 2, 1, "kind-target",
                               tripoint_abs_omt( 18, 20, 0 ), 100 );
    const bandit_live_world::hostile_operation_plan hostile_kind_plan =
        bandit_live_world::plan_hostile_operation(
            hostile_kind_site, bandit_live_world::hostile_operation_kind::shakedown,
            { hostile_kind_site.anchor, tripoint_abs_omt( 18, 20, 0 ) },
            hostile_kind_site.anchor, 102 );
    REQUIRE( hostile_kind_plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan(
                 hostile_kind_site, hostile_kind_plan ) );
    const std::string hostile_kind_bytes = serialize_world( hostile_kind_world );
    const std::string hostile_kind = "\"kind\": \"hostile_operation\"";
    std::string malformed_current_hostile_kind = hostile_kind_bytes;
    REQUIRE( malformed_current_hostile_kind.find( hostile_kind ) != std::string::npos );
    malformed_current_hostile_kind.replace(
        malformed_current_hostile_kind.find( hostile_kind ), hostile_kind.size(),
        "\"kind\": \"scout_sortie\"" );
    JsonValue malformed_current_hostile_json =
        json_loader::from_string( malformed_current_hostile_kind );
    CHECK_THROWS( hostile_kind_world.deserialize( malformed_current_hostile_json.get_object() ) );
    CHECK( serialize_world( hostile_kind_world ) == hostile_kind_bytes );

    std::string future_hostile_wrapper = hostile_kind_bytes;
    const std::string hostile_wrapper_schema =
        "\"active_hostile_operation\": { \"schema_version\": 1";
    REQUIRE( future_hostile_wrapper.find( hostile_wrapper_schema ) != std::string::npos );
    future_hostile_wrapper.replace( future_hostile_wrapper.find( hostile_wrapper_schema ),
                                    hostile_wrapper_schema.size(),
                                    "\"active_hostile_operation\": { \"schema_version\": 2" );
    JsonValue future_hostile_wrapper_json = json_loader::from_string( future_hostile_wrapper );
    CHECK_THROWS( hostile_kind_world.deserialize( future_hostile_wrapper_json.get_object() ) );
    CHECK( serialize_world( hostile_kind_world ) == hostile_kind_bytes );

    JsonValue future_nested_wrapper_json = json_loader::from_string(
            R"({"schema_version":2,"operation_kind":"none"})" );
    bandit_live_world::hostile_operation_state future_nested_wrapper;
    CHECK_THROWS( future_nested_wrapper.deserialize( future_nested_wrapper_json.get_object() ) );

    std::string legacy_hostile_kind = malformed_current_hostile_kind;
    const std::string current_site_schema = "\"schema_version\": 12";
    REQUIRE( legacy_hostile_kind.find( current_site_schema ) != std::string::npos );
    legacy_hostile_kind.replace( legacy_hostile_kind.find( current_site_schema ),
                                 current_site_schema.size(), "\"schema_version\": 9" );
    const std::string current_roster_key = "\"living_total\"";
    REQUIRE( legacy_hostile_kind.find( current_roster_key ) != std::string::npos );
    legacy_hostile_kind.replace( legacy_hostile_kind.find( current_roster_key ),
                                 current_roster_key.size(), "\"headcount\"" );
    erase_pretty_json_member_line( legacy_hostile_kind, "origin_disposition" );
    erase_pretty_json_member_line( legacy_hostile_kind, "origin_changed_minutes" );
    erase_pretty_json_member_line( legacy_hostile_kind, "origin_summary" );
    erase_pretty_json_member_line( legacy_hostile_kind, "routine_activated_minutes" );
    erase_pretty_json_member_line( legacy_hostile_kind, "last_routine_resolved_minutes" );
    erase_pretty_json_member_line( legacy_hostile_kind,
                                   "next_routine_dispatch_eligible_minutes" );
    erase_pretty_json_member_line( legacy_hostile_kind, "routine_no_candidate_streak" );
    JsonValue legacy_hostile_json = json_loader::from_string( legacy_hostile_kind );
    bandit_live_world::world_state repaired_hostile_kind_world;
    repaired_hostile_kind_world.deserialize( legacy_hostile_json.get_object() );
    REQUIRE( repaired_hostile_kind_world.sites.size() == 1 );
    CHECK( repaired_hostile_kind_world.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_hostile_kind_world.sites.front().active_hostile_operation.reservation.kind ==
           bandit_live_world::outing_kind::hostile_operation );
}

TEST_CASE( "bandit_live_world_materialized_routine_pair_keeps_concrete_reserve",
           "[bandit][live_world][routine_policy][materialization]" )
{
    const auto check_materialized_dispatch = []( const bool cannibal ) {
        bandit_live_world::world_state world;
        const std::string source_id = cannibal ? "cannibal_camp" : "bandit_camp";
        const tripoint_abs_omt anchor = cannibal ? tripoint_abs_omt( 70, 80, 0 ) :
                                         tripoint_abs_omt( 10, 20, 0 );
        REQUIRE( bandit_live_world::register_abstract_site(
                     world, bandit_live_world::anchor_source_kind::overmap_special,
                     source_id, anchor, special_lookup, 6 ) );
        REQUIRE( bandit_live_world::routine_scout_materialization_count(
                     world.sites.front() ) == 3 );

        for( int index = 0; index < 3; ++index ) {
            if( cannibal ) {
                add_cannibal_camp_member( world, index, 47700 );
            } else {
                add_bandit_camp_member( world, index, 47800 );
            }
        }

        bandit_live_world::site_record &site = world.sites.front();
        REQUIRE( site.roster().valid );
        CHECK( site.roster().materialized_living_total == 3 );
        CHECK( site.roster().unmaterialized_home_total == 3 );
        CHECK( bandit_live_world::routine_scout_materialization_count( site ) == 0 );

        const bandit_live_world::dispatch_plan plan = bandit_live_world::plan_site_dispatch(
                    site, tripoint_abs_omt( anchor.x() + 1, anchor.y(), anchor.z() ),
                    "materialized-routine-target" );
        REQUIRE( plan.valid );
        CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
        REQUIRE( plan.member_ids.size() == 2 );
        REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
        CHECK( site.count_members_in_state( bandit_live_world::member_state::outbound ) == 2 );
        CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 1 );
        CHECK( site.living_total == 6 );
        REQUIRE( site.roster().valid );

        const std::string dispatched_bytes = serialize_world( world );
        const bandit_live_world::world_state loaded = round_trip_world( world );
        CHECK( serialize_world( loaded ) == dispatched_bytes );
        REQUIRE( loaded.sites.size() == 1 );
        CHECK( loaded.sites.front().active_outing.member_ids.size() == 2 );
        CHECK( loaded.sites.front().count_members_in_state(
                   bandit_live_world::member_state::at_home ) == 1 );
    };

    check_materialized_dispatch( false );
    check_materialized_dispatch( true );
}

TEST_CASE( "bandit_live_world_records_bounded_live_signal_marks_on_owned_sites",
           "[bandit][live_world][live_signal]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::register_abstract_site( world,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_camp ) ) );
    REQUIRE( world.sites.size() == 1 );
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::live_signal_mark smoke_mark;
    smoke_mark.mark_id = "live_smoke@18,20,0";
    smoke_mark.kind = "smoke";
    smoke_mark.source_omt = tripoint_abs_omt( 18, 20, 0 );
    smoke_mark.observed_range_omt = 0;
    smoke_mark.range_cap_omt = 15;
    smoke_mark.strength = 1;
    smoke_mark.confidence = 1;
    smoke_mark.bounty_add = 1;
    smoke_mark.threat_add = 0;
    smoke_mark.notes.push_back( "live source hook: fd_fire=2, fd_smoke=1" );

    REQUIRE( bandit_live_world::record_live_signal_mark( site, smoke_mark ) );
    CHECK( site.remembered_target_or_mark == "live_smoke@18,20,0" );
    CHECK( site.remembered_bounty_estimate == 1 );
    CHECK( site.remembered_threat_estimate == 0 );
    REQUIRE( site.known_recent_marks.size() == 1 );
    CHECK( site.known_recent_marks.front() == "live_smoke@18,20,0" );
    REQUIRE( site.intelligence_map.leads.size() == 1 );
    CHECK( site.intelligence_map.leads.front().kind == bandit_live_world::camp_lead_kind::smoke_signal );
    CHECK( site.intelligence_map.leads.front().origin ==
           bandit_live_world::camp_lead_origin::signal );
    CHECK( site.intelligence_map.leads.front().source_summary.find( "obscured/uncertain" ) !=
           std::string::npos );

    const std::string before_duplicate_signal = serialize_world( world );
    CHECK_FALSE( bandit_live_world::record_live_signal_mark( site, smoke_mark ) );
    CHECK( serialize_world( world ) == before_duplicate_signal );
    CHECK( site.known_recent_marks.size() == 1 );

    for( int i = 0; i < 9; ++i ) {
        bandit_live_world::live_signal_mark extra_mark = smoke_mark;
        extra_mark.mark_id = "live_smoke@" + std::to_string( 30 + i ) + ",20,0";
        CHECK( bandit_live_world::record_live_signal_mark( site, extra_mark ) );
    }
    CHECK( site.known_recent_marks.size() == 8 );
    CHECK( site.known_recent_marks.front() == "live_smoke@31,20,0" );
    CHECK( site.known_recent_marks.back() == "live_smoke@38,20,0" );
}

TEST_CASE( "bandit_live_world_survives_a_save_style_round_trip", "[bandit][live_world]" )
{
    bandit_live_world::world_state original;
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit_mechanic", character_id( 301 ),
             tripoint_abs_ms( 960, 1200, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 302 ),
             tripoint_abs_ms( 984, 1224, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &original_site = original.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( original_site, tripoint_abs_omt( 48, 50, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    CHECK( plan.group.handoff_epoch == 0 );
    CHECK( plan.entry.handoff_epoch == 0 );
    const std::string before_epoch_rejection = serialize_world( original );
    bandit_live_world::dispatch_plan mismatched_epoch = plan;
    mismatched_epoch.entry.handoff_epoch = 1;
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( original_site, mismatched_epoch ) );
    CHECK( serialize_world( original ) == before_epoch_rejection );
    bandit_live_world::dispatch_plan noninitial_epoch = plan;
    noninitial_epoch.group.handoff_epoch = 2;
    noninitial_epoch.entry.handoff_epoch = 2;
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( original_site, noninitial_epoch ) );
    CHECK( serialize_world( original ) == before_epoch_rejection );
    REQUIRE( bandit_live_world::apply_dispatch_plan( original_site, plan ) );

    std::ostringstream out;
    JsonOut jsout( out, true );
    original.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    CHECK( loaded.owner_id == original.owner_id );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &site = loaded.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_work_camp@40,50,0" );
    CHECK( site.site_kind == bandit_live_world::owned_site_kind::bandit_work_camp );
    CHECK( site.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( site.living_total == 2 );
    REQUIRE( site.footprint.size() == 9 );
    REQUIRE( site.members.size() == 2 );
    CHECK( site.members.front().npc_id == character_id( 301 ) );
    CHECK( site.members.front().npc_template_id == "bandit_mechanic" );
    CHECK( site.members.back().npc_id == character_id( 302 ) );
    REQUIRE( site.spawn_tiles.size() == 2 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 960, 1200, 0 ) )->assigned_living_total == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 984, 1224, 0 ) )->assigned_living_total == 1 );
    CHECK( site.active_outing.activity_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    CHECK( site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie );
    CHECK( site.active_outing.generation == 1 );
    CHECK( site.next_outing_generation == 2 );
    CHECK( site.active_outing.return_application_key ==
           "overmap_special:bandit_work_camp@40,50,0#dispatch:return:1" );
    CHECK( site.active_outing.target_id == "player_basecamp_nearby" );
    REQUIRE( site.active_outing.member_ids ==
             std::vector<character_id>( { character_id( 302 ), character_id( 301 ) } ) );
    CHECK( site.active_outing.leader_id == character_id( 302 ) );
}

TEST_CASE( "bandit_live_world_deserialize_commits_only_after_the_packet_is_valid",
           "[bandit][live_world][save]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 303 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    const std::string before = serialize_world( world );

    JsonValue malformed = json_loader::from_string(
                              R"({"owner_id":"replacement","sites":"not-an-array"})" );
    CHECK_THROWS( world.deserialize( malformed.get_object() ) );
    CHECK( serialize_world( world ) == before );
}

TEST_CASE( "bandit_live_world_finite_resource_claim_is_global_atomic_and_idempotent",
           "[bandit][live_world][resource]" )
{
    bandit_live_world::world_state world;
    bandit_live_world::site_record claimant;
    claimant.site_id = "resource-test-camp";
    world.sites.push_back( claimant );
    const tripoint_abs_omt resource_omt( 120, -45, 0 );

    const bandit_live_world::finite_resource_record first_snapshot =
        bandit_live_world::finite_resource_snapshot( world, resource_omt, 3 );
    CHECK( first_snapshot.remaining_units == 3 );
    CHECK( first_snapshot.revision == 0 );
    CHECK( world.finite_resources.empty() );

    const std::string before_first_claim = serialize_world( world );
    CHECK( bandit_live_world::claim_finite_resource_units(
               world, claimant.site_id, resource_omt, first_snapshot, 1,
               claimant.site_id + "#resource", 1, "forged-resource-key" ).status ==
           bandit_live_world::finite_resource_claim_status::rejected );
    CHECK( serialize_world( world ) == before_first_claim );

    bandit_live_world::world_state terminal_claim_world;
    const std::string terminal_operation_id = "resource-test-camp#terminal-resource";
    issue_test_resource_operation( terminal_claim_world, resource_omt,
                                   terminal_operation_id,
                                   std::numeric_limits<int>::max() - 1 );
    const bandit_live_world::finite_resource_record terminal_snapshot =
        bandit_live_world::finite_resource_snapshot( terminal_claim_world, resource_omt, 3 );
    const std::string terminal_claim_before = serialize_world( terminal_claim_world );
    CHECK( bandit_live_world::claim_finite_resource_units(
               terminal_claim_world, terminal_claim_world.sites.front().site_id,
               resource_omt, terminal_snapshot, 1, terminal_operation_id,
               std::numeric_limits<int>::max() - 1,
               bandit_live_world::finite_resource_claim_application_key(
                   terminal_operation_id, std::numeric_limits<int>::max() - 1,
                   resource_omt ) ).status ==
           bandit_live_world::finite_resource_claim_status::rejected );
    CHECK( serialize_world( terminal_claim_world ) == terminal_claim_before );
    CHECK( bandit_live_world::claim_finite_resource_units(
               world, claimant.site_id, resource_omt, first_snapshot, 1,
               claimant.site_id + "#resource", std::numeric_limits<int>::max() - 1,
               bandit_live_world::finite_resource_claim_application_key(
                   claimant.site_id + "#resource", std::numeric_limits<int>::max() - 1,
                   resource_omt ) ).status ==
           bandit_live_world::finite_resource_claim_status::rejected );
    CHECK( serialize_world( world ) == before_first_claim );

    const bandit_live_world::finite_resource_claim_result first_claim =
        claim_test_resource( world, resource_omt, first_snapshot, 1 );
    CHECK( first_claim.status == bandit_live_world::finite_resource_claim_status::applied );
    CHECK( first_claim.claimed_units == 1 );
    CHECK( first_claim.remaining_units == 2 );
    CHECK( first_claim.revision == 1 );
    CHECK( first_claim.application_key ==
           bandit_live_world::finite_resource_claim_application_key(
               claimant.site_id + "#resource", 1, resource_omt ) );
    REQUIRE( world.finite_resources.size() == 1 );

    const std::string after_first_claim = serialize_world( world );
    const bandit_live_world::finite_resource_claim_result replay =
        claim_test_resource( world, resource_omt, first_snapshot, 1 );
    CHECK( replay.status == bandit_live_world::finite_resource_claim_status::already_applied );
    CHECK( replay.claimed_units == 1 );
    CHECK( replay.remaining_units == 2 );
    CHECK( replay.revision == 1 );
    CHECK( serialize_world( world ) == after_first_claim );
    const bandit_live_world::finite_resource_claim_result competing_operation =
        claim_test_resource( world, resource_omt, first_snapshot, 1,
                             claimant.site_id + "#competing", 1 );
    CHECK( competing_operation.status ==
           bandit_live_world::finite_resource_claim_status::rejected );
    CHECK( competing_operation.application_key != replay.application_key );
    CHECK( serialize_world( world ) == after_first_claim );

    const bandit_live_world::finite_resource_record second_snapshot =
        bandit_live_world::finite_resource_snapshot( world, resource_omt, 3 );
    const bandit_live_world::finite_resource_claim_result second_claim =
        claim_test_resource( world, resource_omt, second_snapshot, 2,
                             claimant.site_id + "#resource-second", 2 );
    CHECK( second_claim.status == bandit_live_world::finite_resource_claim_status::applied );
    CHECK( second_claim.claimed_units == 2 );
    CHECK( second_claim.remaining_units == 0 );
    CHECK( second_claim.revision == 2 );
    REQUIRE( world.finite_resources.size() == 1 );

    issue_test_resource_operation( world, resource_omt,
                                   claimant.site_id + "#resource-empty", 3 );
    const std::string depleted_bytes = serialize_world( world );
    const bandit_live_world::finite_resource_record depleted_snapshot =
        bandit_live_world::finite_resource_snapshot( world, resource_omt, 3 );
    const bandit_live_world::finite_resource_claim_result depleted_claim =
        claim_test_resource( world, resource_omt, depleted_snapshot, 1,
                             claimant.site_id + "#resource-empty", 3 );
    CHECK( depleted_claim.status == bandit_live_world::finite_resource_claim_status::depleted );
    CHECK( serialize_world( world ) == depleted_bytes );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.finite_resources.size() == 1 );
    CHECK( serialize_world( loaded ) == depleted_bytes );
    bandit_live_world::world_state replay_loaded = loaded;
    CHECK( claim_test_resource( replay_loaded, resource_omt,
            second_snapshot, 2, claimant.site_id + "#resource-second", 2 ).status ==
           bandit_live_world::finite_resource_claim_status::already_applied );
    CHECK( serialize_world( replay_loaded ) == depleted_bytes );

    const tripoint_abs_omt absent_omt( 121, -45, 0 );
    issue_test_resource_operation( world, absent_omt, claimant.site_id + "#empty", 4 );
    const std::string absent_operation_bytes = serialize_world( world );
    const bandit_live_world::finite_resource_record empty_snapshot =
        bandit_live_world::finite_resource_snapshot( world, absent_omt, 0 );
    CHECK( claim_test_resource( world, absent_omt,
            empty_snapshot, 1, claimant.site_id + "#empty", 4 ).status ==
           bandit_live_world::finite_resource_claim_status::depleted );
    CHECK( world.finite_resources.size() == 1 );
    CHECK( claim_test_resource( world, absent_omt,
            bandit_live_world::finite_resource_record { 1, 1 }, 1,
            claimant.site_id + "#empty", 4 ).status ==
           bandit_live_world::finite_resource_claim_status::stale );
    CHECK( claim_test_resource( world, absent_omt,
            bandit_live_world::finite_resource_record { 1, 0 }, 0,
            claimant.site_id + "#empty", 4 ).status ==
           bandit_live_world::finite_resource_claim_status::rejected );
    CHECK( serialize_world( world ) == absent_operation_bytes );
}

TEST_CASE( "bandit_live_world_finite_resource_save_contract_is_canonical_and_fail_closed",
           "[bandit][live_world][resource][save][migration]" )
{
    const std::array<tripoint_abs_omt, 3> omts = {
        tripoint_abs_omt( -8, 4, -1 ),
        tripoint_abs_omt( 3, 7, 0 ),
        tripoint_abs_omt( 11, -2, 1 ),
    };
    bandit_live_world::world_state forward;
    bandit_live_world::world_state reverse;
    int operation_generation = 1;
    for( const tripoint_abs_omt &omt : omts ) {
        const bandit_live_world::finite_resource_record expected =
            bandit_live_world::finite_resource_snapshot( forward, omt, 1 );
        REQUIRE( claim_test_resource( forward, omt, expected, 1, "",
                                      operation_generation++ ).status ==
                 bandit_live_world::finite_resource_claim_status::applied );
    }
    operation_generation = 1;
    for( auto iter = omts.rbegin(); iter != omts.rend(); ++iter ) {
        const bandit_live_world::finite_resource_record expected =
            bandit_live_world::finite_resource_snapshot( reverse, *iter, 1 );
        REQUIRE( claim_test_resource( reverse, *iter, expected, 1, "",
                                      operation_generation++ ).status ==
                 bandit_live_world::finite_resource_claim_status::applied );
    }
    REQUIRE( forward.finite_resources.size() == reverse.finite_resources.size() );
    for( const auto &entry : forward.finite_resources ) {
        const bandit_live_world::finite_resource_record *reverse_record =
            reverse.find_finite_resource( entry.first );
        REQUIRE( reverse_record != nullptr );
        CHECK( reverse_record->remaining_units == entry.second.remaining_units );
        CHECK( reverse_record->revision == entry.second.revision );
    }
    CHECK( serialize_world( round_trip_world( forward ) ) == serialize_world( forward ) );
    CHECK( serialize_world( round_trip_world( reverse ) ) == serialize_world( reverse ) );

    bandit_live_world::world_state terminal_receipt_target = forward;
    const std::string terminal_receipt_before = serialize_world( terminal_receipt_target );
    std::string terminal_receipt_packet = terminal_receipt_before;
    const std::string resource_generation_field = "\"applied_resource_generation\": 3";
    const std::size_t resource_generation_at = terminal_receipt_packet.find(
            resource_generation_field );
    REQUIRE( resource_generation_at != std::string::npos );
    terminal_receipt_packet.replace( resource_generation_at,
                                     resource_generation_field.size(),
                                     "\"applied_resource_generation\": " +
                                     std::to_string( std::numeric_limits<int>::max() - 1 ) );
    JsonValue terminal_receipt_json = json_loader::from_string( terminal_receipt_packet );
    CHECK_THROWS( terminal_receipt_target.deserialize( terminal_receipt_json.get_object() ) );
    CHECK( serialize_world( terminal_receipt_target ) == terminal_receipt_before );

    bandit_live_world::world_state forged_receipt_target = forward;
    const std::string forged_receipt_before = serialize_world( forged_receipt_target );
    std::string forged_receipt_packet = forged_receipt_before;
    const std::string next_generation_field = "\"next_outing_generation\": 4";
    const std::size_t next_generation_at = forged_receipt_packet.find( next_generation_field );
    REQUIRE( next_generation_at != std::string::npos );
    forged_receipt_packet.replace( next_generation_at, next_generation_field.size(),
                                   "\"next_outing_generation\": 5" );
    const std::size_t forged_resource_generation_at = forged_receipt_packet.find(
                resource_generation_field );
    REQUIRE( forged_resource_generation_at != std::string::npos );
    forged_receipt_packet.replace( forged_resource_generation_at,
                                   resource_generation_field.size(),
                                   "\"applied_resource_generation\": 4" );
    JsonValue forged_receipt_json = json_loader::from_string( forged_receipt_packet );
    CHECK_THROWS( forged_receipt_target.deserialize( forged_receipt_json.get_object() ) );
    CHECK( serialize_world( forged_receipt_target ) == forged_receipt_before );

    bandit_live_world::world_state legacy;
    legacy.schema_version = 3;
    bandit_live_world::site_record legacy_site;
    legacy_site.site_id = "legacy-resource-camp";
    bandit_live_world::camp_map_lead harvested;
    harvested.lead_id = "legacy-harvested";
    harvested.kind = bandit_live_world::camp_lead_kind::structural_bounty;
    harvested.status = bandit_live_world::camp_lead_status::harvested;
    harvested.omt = tripoint_abs_omt( 40, 50, 0 );
    legacy_site.intelligence_map.leads.push_back( harvested );
    bandit_live_world::camp_map_lead unharvested = harvested;
    unharvested.lead_id = "legacy-unharvested";
    unharvested.status = bandit_live_world::camp_lead_status::suspected;
    unharvested.omt = tripoint_abs_omt( 41, 50, 0 );
    legacy_site.intelligence_map.leads.push_back( unharvested );
    legacy.sites.push_back( legacy_site );

    const bandit_live_world::world_state migrated = round_trip_world( legacy );
    CHECK( migrated.schema_version == 6 );
    REQUIRE( migrated.finite_resources.size() == 1 );
    const bandit_live_world::finite_resource_record *migrated_resource =
        migrated.find_finite_resource( harvested.omt );
    REQUIRE( migrated_resource != nullptr );
    CHECK( migrated_resource->remaining_units == 0 );
    CHECK( migrated_resource->revision == 1 );
    CHECK( migrated.find_finite_resource( unharvested.omt ) == nullptr );

    bandit_live_world::world_state protected_world = forward;
    const std::string protected_bytes = serialize_world( protected_world );
    const std::array<std::string, 4> malformed_packets = {
        R"({"schema_version":4,"sites":[],"finite_resources":[[1,2,0,2,1],[1,2,0,1,2]]})",
        R"({"schema_version":4,"sites":[],"finite_resources":[[1,2,0,-1,1]]})",
        R"({"schema_version":4,"sites":[],"finite_resources":[[1,2,0,2,0]]})",
        R"({"schema_version":4,"sites":[],"finite_resources":[[1,2,0,1]]})",
    };
    for( const std::string &packet : malformed_packets ) {
        JsonValue malformed = json_loader::from_string( packet );
        CHECK_THROWS( protected_world.deserialize( malformed.get_object() ) );
        CHECK( serialize_world( protected_world ) == protected_bytes );
    }

    std::string hybrid_legacy_packet = serialize_world( legacy );
    const std::size_t object_end = hybrid_legacy_packet.rfind( '}' );
    REQUIRE( object_end != std::string::npos );
    hybrid_legacy_packet.insert( object_end,
                                 ",\n  \"finite_resources\": [[40,50,0,2,1]]\n" );
    JsonValue hybrid_legacy = json_loader::from_string( hybrid_legacy_packet );
    CHECK_THROWS( protected_world.deserialize( hybrid_legacy.get_object() ) );
    CHECK( serialize_world( protected_world ) == protected_bytes );
}

TEST_CASE( "bandit_live_world_finite_resource_serialization_has_a_bounded_slope",
           "[bandit][live_world][resource][save]" )
{
    bandit_live_world::world_state world;
    std::size_t bytes_at_500 = 0;
    for( int index = 0; index < 1000; ++index ) {
        const tripoint_abs_omt omt( 100000 + index, 200000 + index, 0 );
        bandit_live_world::finite_resource_record expected =
            bandit_live_world::finite_resource_snapshot( world, omt, 3 );
        REQUIRE( claim_test_resource( world, omt, expected, 1, "",
                                      index * 2 + 1 ).status ==
                 bandit_live_world::finite_resource_claim_status::applied );
        expected = bandit_live_world::finite_resource_snapshot( world, omt, 3 );
        REQUIRE( claim_test_resource( world, omt, expected, 2, "",
                                      index * 2 + 2 ).status ==
                 bandit_live_world::finite_resource_claim_status::applied );
        if( index == 499 ) {
            bytes_at_500 = serialize_world( world ).size();
        }
    }
    const std::size_t bytes_at_1000 = serialize_world( world ).size();
    CAPTURE( bytes_at_500, bytes_at_1000 );
    REQUIRE( bytes_at_1000 > bytes_at_500 );
    CHECK( bytes_at_1000 - bytes_at_500 <= 32 * 500 );
    CHECK( serialize_world( round_trip_world( world ) ) == serialize_world( world ) );
}

TEST_CASE( "bandit_live_world_camp_supply_has_bounded_capacity_and_safe_migration",
           "[bandit][live_world][supply][migration]" )
{
    bandit_live_world::site_record capacity_site;
    capacity_site.living_total = 0;
    CHECK( bandit_live_world::camp_supply_cap( capacity_site ) == 14 );
    capacity_site.living_total = 1;
    CHECK( bandit_live_world::camp_supply_cap( capacity_site ) == 14 );
    capacity_site.living_total = 18;
    CHECK( bandit_live_world::camp_supply_cap( capacity_site ) == 252 );
    capacity_site.living_total = 19;
    CHECK( bandit_live_world::camp_supply_cap( capacity_site ) == 256 );

    bandit_live_world::world_state registered;
    REQUIRE( bandit_live_world::register_abstract_site( registered,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 10, 20, 0 ), special_lookup, 6 ) );
    REQUIRE( registered.sites.size() == 1 );
    CHECK( registered.sites.front().supply_units == 42 );
    CHECK( registered.sites.front().supply_accounted_living_total == 6 );
    CHECK( registered.sites.front().supply_last_update_minutes == -1 );
    for( int index = 0; index < 14; ++index ) {
        add_bandit_camp_member( registered, index, 47000 );
    }
    CHECK( registered.sites.front().living_total == 14 );
    CHECK( registered.sites.front().supply_units == 98 );
    CHECK( registered.sites.front().supply_accounted_living_total == 14 );

    JsonValue legacy_json = json_loader::from_string(
                                R"({"schema_version":5,"site_id":"legacy-supply","headcount":6})" );
    bandit_live_world::site_record legacy;
    legacy.deserialize( legacy_json.get_object() );
    CHECK( legacy.schema_version == 12 );
    CHECK( legacy.supply_units == 42 );
    CHECK( legacy.supply_accounted_living_total == 6 );
    CHECK( legacy.supply_member_minute_remainder == 0 );
    CHECK( legacy.supply_last_update_minutes == -1 );

    bandit_live_world::world_state migrated_world;
    migrated_world.sites.push_back( legacy );
    const bandit_live_world::world_state migrated_round_trip = round_trip_world( migrated_world );
    REQUIRE( migrated_round_trip.sites.size() == 1 );
    CHECK( migrated_round_trip.sites.front().schema_version == 12 );
    CHECK( migrated_round_trip.sites.front().supply_units == 42 );

    JsonValue incomplete_current_json = json_loader::from_string(
                                            R"({"schema_version":6,"site_id":"incomplete-supply","headcount":6,"supply_units":42})" );
    bandit_live_world::site_record incomplete_current;
    incomplete_current.deserialize( incomplete_current_json.get_object() );
    CHECK( incomplete_current.supply_units == 0 );
    CHECK( incomplete_current.supply_accounted_living_total == 6 );
    CHECK( incomplete_current.supply_last_update_minutes == -1 );
}

TEST_CASE( "bandit_live_world_camp_supply_catch_up_is_bounded_and_stepwise_stable",
           "[bandit][live_world][supply][time]" )
{
    bandit_live_world::site_record fractional;
    fractional.living_total = 3;
    fractional.supply_units = 42;
    fractional.supply_accounted_living_total = 3;
    REQUIRE( bandit_live_world::advance_camp_supply( fractional, 100 ) );
    CHECK( fractional.supply_units == 42 );
    CHECK( fractional.supply_member_minute_remainder == 0 );
    REQUIRE( bandit_live_world::advance_camp_supply( fractional, 820 ) );
    CHECK( fractional.supply_units == 41 );
    CHECK( fractional.supply_member_minute_remainder == 720 );

    bandit_live_world::world_state fractional_world;
    fractional_world.sites.push_back( fractional );
    bandit_live_world::world_state loaded_fractional = round_trip_world( fractional_world );
    REQUIRE( loaded_fractional.sites.size() == 1 );
    REQUIRE( bandit_live_world::advance_camp_supply( fractional, 1540 ) );
    REQUIRE( bandit_live_world::advance_camp_supply( loaded_fractional.sites.front(), 1540 ) );
    CHECK( fractional.supply_units == 39 );
    CHECK( fractional.supply_member_minute_remainder == 0 );
    CHECK( loaded_fractional.sites.front().supply_units == fractional.supply_units );
    CHECK( loaded_fractional.sites.front().supply_member_minute_remainder == 0 );

    bandit_live_world::site_record one_jump;
    one_jump.living_total = 18;
    one_jump.supply_units = 252;
    one_jump.supply_last_update_minutes = 0;
    one_jump.supply_accounted_living_total = 18;
    bandit_live_world::site_record daily = one_jump;
    REQUIRE( bandit_live_world::advance_camp_supply( one_jump, 10 * 24 * 60 ) );
    for( int day = 1; day <= 10; ++day ) {
        REQUIRE( bandit_live_world::advance_camp_supply( daily, day * 24 * 60 ) );
    }
    CHECK( one_jump.supply_units == 72 );
    CHECK( daily.supply_units == one_jump.supply_units );
    CHECK( daily.supply_member_minute_remainder == one_jump.supply_member_minute_remainder );

    const std::string before_backward = [&one_jump]() {
        bandit_live_world::world_state snapshot;
        snapshot.sites.push_back( one_jump );
        return serialize_world( snapshot );
    }();
    CHECK_FALSE( bandit_live_world::advance_camp_supply( one_jump, one_jump.supply_last_update_minutes - 1 ) );
    bandit_live_world::world_state after_backward;
    after_backward.sites.push_back( one_jump );
    CHECK( serialize_world( after_backward ) == before_backward );

    REQUIRE( bandit_live_world::advance_camp_supply( one_jump, 730 * 24 * 60 ) );
    CHECK( one_jump.supply_units == 0 );
    CHECK( one_jump.supply_last_update_minutes == 730 * 24 * 60 );

    bandit_live_world::world_state zero_living_world;
    bandit_live_world::site_record zero_living;
    zero_living.supply_units = 14;
    zero_living.supply_last_update_minutes = 0;
    zero_living_world.sites.push_back( zero_living );
    CHECK( bandit_live_world::advance_world_camp_supplies( zero_living_world,
            730 * 24 * 60 ) == 1 );
    CHECK( zero_living_world.sites.front().supply_units == 14 );
    CHECK( bandit_live_world::advance_world_camp_supplies( zero_living_world,
            730 * 24 * 60 ) == 0 );

    bandit_live_world::world_state roster_world;
    add_bandit_camp_member( roster_world, 0, 48000 );
    add_bandit_camp_member( roster_world, 1, 48000 );
    bandit_live_world::site_record &roster_site = roster_world.sites.front();
    roster_site.supply_units = 28;
    REQUIRE( bandit_live_world::update_member_state( roster_site, character_id( 48001 ),
             bandit_live_world::member_state::dead, "supply cap roster shrink" ) );
    CHECK( bandit_live_world::camp_supply_living_total( roster_site ) == 1 );
    CHECK( roster_site.supply_units == 14 );
}

TEST_CASE( "bandit_live_world_resource_estimates_remain_private_camp_knowledge",
           "[bandit][live_world][resource][knowledge]" )
{
    bandit_live_world::world_state world;
    bandit_live_world::site_record camp_a;
    camp_a.site_id = "resource-estimate-camp-a";
    bandit_live_world::site_record camp_b;
    camp_b.site_id = "resource-estimate-camp-b";
    const tripoint_abs_omt resource_omt( 25, 30, 0 );
    bandit_live_world::structural_bounty_read read;
    read.terrain_class = "town";
    read.bounty = 3;
    read.confidence = 1;
    read.eligible = true;
    read.summary = "private structural estimate";
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( camp_a, resource_omt, read, 100 ) );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( camp_b, resource_omt, read, 100 ) );
    world.sites = { camp_a, camp_b };

    const std::string camp_a_lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                           world.sites[0].site_id, resource_omt, read.terrain_class );
    const std::string camp_b_lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                           world.sites[1].site_id, resource_omt, read.terrain_class );
    bandit_live_world::finite_resource_record snapshot =
        bandit_live_world::finite_resource_snapshot( world, resource_omt, 3 );
    REQUIRE( claim_test_resource( world, resource_omt, snapshot, 1 ).status ==
             bandit_live_world::finite_resource_claim_status::applied );
    REQUIRE( world.sites[0].intelligence_map.find_lead( camp_a_lead_id ) != nullptr );
    REQUIRE( world.sites[1].intelligence_map.find_lead( camp_b_lead_id ) != nullptr );
    CHECK( world.sites[0].intelligence_map.find_lead( camp_a_lead_id )->bounty == 3 );
    CHECK( world.sites[1].intelligence_map.find_lead( camp_b_lead_id )->bounty == 3 );

    REQUIRE( bandit_live_world::record_camp_resource_estimate( world.sites[0], camp_a_lead_id,
             2, 3, 200 ) );
    const bandit_live_world::camp_map_lead *estimate_a =
        world.sites[0].intelligence_map.find_lead( camp_a_lead_id );
    const bandit_live_world::camp_map_lead *estimate_b =
        world.sites[1].intelligence_map.find_lead( camp_b_lead_id );
    REQUIRE( estimate_a != nullptr );
    REQUIRE( estimate_b != nullptr );
    CHECK( estimate_a->bounty == 2 );
    CHECK( estimate_a->confidence == 3 );
    CHECK( estimate_a->last_checked_minutes == 200 );
    CHECK( estimate_b->bounty == 3 );
    CHECK( estimate_b->confidence == 1 );
    CHECK( estimate_b->last_checked_minutes == -1 );

    const std::string before_stale_estimate = serialize_world( world );
    CHECK_FALSE( bandit_live_world::record_camp_resource_estimate( world.sites[0], camp_a_lead_id,
                 1, 3, 199 ) );
    CHECK_FALSE( bandit_live_world::record_camp_resource_estimate( world.sites[0], camp_a_lead_id,
                 4, 3, 201 ) );
    CHECK( serialize_world( world ) == before_stale_estimate );

    snapshot = bandit_live_world::finite_resource_snapshot( world, resource_omt, 3 );
    REQUIRE( claim_test_resource( world, resource_omt, snapshot, 2, "", 2 ).status ==
             bandit_live_world::finite_resource_claim_status::applied );
    CHECK( world.sites[0].intelligence_map.find_lead( camp_a_lead_id )->bounty == 2 );
    CHECK( world.sites[1].intelligence_map.find_lead( camp_b_lead_id )->bounty == 3 );

    REQUIRE( bandit_live_world::record_camp_resource_estimate( world.sites[0], camp_a_lead_id,
             0, 3, 300 ) );
    estimate_a = world.sites[0].intelligence_map.find_lead( camp_a_lead_id );
    REQUIRE( estimate_a != nullptr );
    CHECK( estimate_a->bounty == 0 );
    CHECK( estimate_a->confidence == 3 );
    CHECK( estimate_a->last_checked_minutes == 300 );
    CHECK( estimate_a->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( world.sites[1].intelligence_map.find_lead( camp_b_lead_id )->bounty == 3 );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 2 );
    CHECK( loaded.sites[0].intelligence_map.find_lead( camp_a_lead_id )->bounty == 0 );
    CHECK( loaded.sites[0].intelligence_map.find_lead( camp_a_lead_id )->last_checked_minutes == 300 );
    CHECK( loaded.sites[1].intelligence_map.find_lead( camp_b_lead_id )->bounty == 3 );
}

TEST_CASE( "bandit_live_world_normalizes_intelligence_deterministically_and_pins_active_leads",
           "[bandit][live_world][intelligence][capacity]" )
{
    bandit_live_world::site_record forward;
    forward.site_id = "retention-camp";
    bandit_live_world::site_record reverse = forward;
    for( int index = 0; index < 70; ++index ) {
        forward.intelligence_map.leads.push_back( make_retention_test_lead( index ) );
    }
    for( int index = 69; index >= 0; --index ) {
        reverse.intelligence_map.leads.push_back( make_retention_test_lead( index ) );
    }

    bandit_live_world::normalize_camp_intelligence( forward );
    bandit_live_world::normalize_camp_intelligence( reverse );
    REQUIRE( forward.intelligence_map.leads.size() == 64 );
    REQUIRE( reverse.intelligence_map.leads.size() == 64 );
    bandit_live_world::world_state forward_world;
    forward_world.sites.push_back( forward );
    bandit_live_world::world_state reverse_world;
    reverse_world.sites.push_back( reverse );
    CHECK( serialize_world( forward_world ) == serialize_world( reverse_world ) );
    CHECK( forward.intelligence_map.find_lead( "retention-lead-0" ) == nullptr );
    CHECK( forward.intelligence_map.find_lead( "retention-lead-69" ) != nullptr );

    bandit_live_world::site_record duplicate_site;
    duplicate_site.site_id = "duplicate-camp";
    bandit_live_world::camp_map_lead older = make_retention_test_lead( 80 );
    older.lead_id = "duplicate-lead";
    older.revision = 2;
    older.last_seen_minutes = 10;
    older.source_summary = "older duplicate";
    bandit_live_world::camp_map_lead newer = older;
    newer.revision = 3;
    newer.last_seen_minutes = 20;
    newer.source_summary = "newer duplicate";
    duplicate_site.intelligence_map.leads = { older, newer };
    bandit_live_world::normalize_camp_intelligence( duplicate_site );
    REQUIRE( duplicate_site.intelligence_map.leads.size() == 1 );
    CHECK( duplicate_site.intelligence_map.leads.front().revision == 3 );
    CHECK( duplicate_site.intelligence_map.leads.front().source_summary == "newer duplicate" );

    bandit_live_world::site_record delimiter_forward;
    delimiter_forward.site_id = "delimiter-camp";
    bandit_live_world::camp_map_lead delimiter_a = make_retention_test_lead( 81 );
    delimiter_a.lead_id = "delimiter-duplicate";
    delimiter_a.source_key = "a|b";
    delimiter_a.source_summary = "c";
    bandit_live_world::camp_map_lead delimiter_b = delimiter_a;
    delimiter_b.source_key = "a";
    delimiter_b.source_summary = "b|c";
    delimiter_forward.intelligence_map.leads = { delimiter_a, delimiter_b };
    bandit_live_world::site_record delimiter_reverse = delimiter_forward;
    std::reverse( delimiter_reverse.intelligence_map.leads.begin(),
                  delimiter_reverse.intelligence_map.leads.end() );
    bandit_live_world::normalize_camp_intelligence( delimiter_forward );
    bandit_live_world::normalize_camp_intelligence( delimiter_reverse );
    bandit_live_world::world_state delimiter_forward_world;
    delimiter_forward_world.sites.push_back( delimiter_forward );
    bandit_live_world::world_state delimiter_reverse_world;
    delimiter_reverse_world.sites.push_back( delimiter_reverse );
    CHECK( serialize_world( delimiter_forward_world ) ==
           serialize_world( delimiter_reverse_world ) );

    bandit_live_world::site_record pinned_site;
    pinned_site.site_id = "pinned-camp";
    for( int index = 0; index < 65; ++index ) {
        pinned_site.intelligence_map.leads.push_back( make_retention_test_lead( index ) );
    }
    const bandit_live_world::camp_map_lead pinned = pinned_site.intelligence_map.leads.front();
    set_test_active_outing( pinned_site, "pinned-camp#scout:1" );
    pinned_site.active_outing.target_id = pinned.target_id;
    pinned_site.active_outing.target_omt = pinned.omt;
    pinned_site.active_outing.target_lead_id = pinned.lead_id;
    pinned_site.active_outing.target_lead_revision = pinned.revision;
    bandit_live_world::normalize_camp_intelligence( pinned_site );
    REQUIRE( pinned_site.intelligence_map.leads.size() == 64 );
    CHECK( pinned_site.intelligence_map.find_lead( pinned.lead_id ) != nullptr );
    CHECK( pinned_site.intelligence_map.find_lead( "retention-lead-1" ) == nullptr );

    bandit_live_world::site_record legacy_revision_site;
    legacy_revision_site.site_id = "legacy-revision-camp";
    legacy_revision_site.intelligence_map.leads.push_back( make_retention_test_lead( 90 ) );
    const bandit_live_world::camp_map_lead legacy_lead =
        legacy_revision_site.intelligence_map.leads.front();
    set_test_active_outing( legacy_revision_site, "legacy-revision-camp#scout:7" );
    legacy_revision_site.active_outing.target_id = legacy_lead.target_id;
    legacy_revision_site.active_outing.target_omt = legacy_lead.omt;
    legacy_revision_site.active_outing.target_lead_revision = 7;
    bandit_live_world::normalize_camp_intelligence( legacy_revision_site );
    REQUIRE( legacy_revision_site.intelligence_map.find_lead( legacy_lead.lead_id ) != nullptr );
    CHECK( legacy_revision_site.intelligence_map.find_lead( legacy_lead.lead_id )->revision == 7 );
    CHECK( legacy_revision_site.active_outing.target_lead_id == legacy_lead.lead_id );
    CHECK( legacy_revision_site.active_outing.target_lead_revision == 7 );

    bandit_live_world::site_record bounded_strings_site;
    bounded_strings_site.site_id = "bounded-strings-camp";
    bandit_live_world::camp_map_lead oversized = make_retention_test_lead( 91 );
    oversized.lead_id = std::string( 4096, 'i' );
    oversized.target_id = std::string( 4096, 't' );
    oversized.source_key = std::string( 4096, 'k' );
    oversized.source_summary = std::string( 4096, 's' );
    oversized.last_outcome = std::string( 4096, 'o' );
    bounded_strings_site.intelligence_map.leads.push_back( oversized );
    bounded_strings_site.known_recent_marks.push_back( std::string( 4096, 'm' ) );
    bandit_live_world::normalize_camp_intelligence( bounded_strings_site );
    REQUIRE( bounded_strings_site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &bounded =
        bounded_strings_site.intelligence_map.leads.front();
    CHECK( bounded.lead_id.size() == 192 );
    CHECK( bounded.target_id.size() == 192 );
    CHECK( bounded.source_key.size() == 192 );
    CHECK( bounded.source_summary.size() == 256 );
    CHECK( bounded.last_outcome.size() == 128 );
    REQUIRE( bounded_strings_site.known_recent_marks.size() == 1 );
    CHECK( bounded_strings_site.known_recent_marks.front().size() == 192 );
}

TEST_CASE( "bandit_live_world_rejects_a_structural_plan_after_its_lead_revision_changes",
           "[bandit][live_world][intelligence][revision]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 48100 );
    add_bandit_camp_member( world, 1, 48100 );
    bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::structural_bounty_read read;
    read.terrain_class = "town";
    read.bounty = 3;
    read.confidence = 1;
    read.eligible = true;
    read.summary = "revision fixture";
    const tripoint_abs_omt omt( 15, 20, 0 );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, omt, read, 100 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                    site.site_id, omt, read.terrain_class );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    const bandit_live_world::structural_outing_plan stale_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 1000 );
    REQUIRE( stale_plan.valid );
    CHECK( stale_plan.lead_revision == lead->revision );
    REQUIRE( bandit_live_world::record_camp_resource_estimate( site, lead_id, 2, 2, 500 ) );
    REQUIRE( site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( lead_id )->revision > stale_plan.lead_revision );
    const std::string before = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan( site, stale_plan, 1000 ) );
    CHECK( serialize_world( world ) == before );

    bandit_live_world::camp_map_lead *terminal = site.intelligence_map.find_lead( lead_id );
    REQUIRE( terminal != nullptr );
    terminal->revision = std::numeric_limits<int>::max();
    const std::string before_terminal_update = serialize_world( world );
    CHECK_FALSE( bandit_live_world::record_camp_resource_estimate( site, lead_id, 1, 3, 600 ) );
    CHECK( serialize_world( world ) == before_terminal_update );
}

TEST_CASE( "bandit_live_world_load_caps_intelligence_and_preserves_current_references",
           "[bandit][live_world][intelligence][capacity][save]" )
{
    bandit_live_world::world_state world;
    bandit_live_world::site_record site;
    site.site_id = "oversized-intelligence-camp";
    for( int index = 0; index < 70; ++index ) {
        site.intelligence_map.leads.push_back( make_retention_test_lead( index ) );
    }
    for( int index = 0; index < 10; ++index ) {
        site.known_recent_marks.push_back( "recent-mark-" + std::to_string( index ) );
    }
    const bandit_live_world::camp_map_lead &pinned = site.intelligence_map.leads.front();
    site.current_scout_report.revision = 1;
    site.current_scout_report.action_policy =
        bandit_live_world::camp_report_policy::bandit_shakedown;
    site.current_scout_report.source_activity_id = site.site_id + "#scout:1";
    site.current_scout_report.source_generation = 1;
    site.current_scout_report.source_job_type = "scout";
    site.current_scout_report.target_id = pinned.target_id;
    site.current_scout_report.target_omt = pinned.omt;
    site.current_scout_report.target_lead_id = pinned.lead_id;
    site.current_scout_report.target_lead_revision = pinned.revision;
    site.current_scout_report.application_key = site.current_scout_report.source_activity_id +
            ":report:1";
    site.current_scout_report.delivered_minutes = 100;
    world.sites.push_back( site );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    REQUIRE( loaded_site.intelligence_map.leads.size() == 64 );
    CHECK( loaded_site.intelligence_map.schema_version == 5 );
    CHECK( loaded_site.intelligence_map.terrain_scan_cursor == 0 );
    CHECK( loaded_site.intelligence_map.frontier_sector_cursor == 0 );
    CHECK( loaded_site.intelligence_map.frontier_last_resolved_minutes ==
           std::vector<int>( 8, -1 ) );
    CHECK( loaded_site.intelligence_map.find_lead( pinned.lead_id ) != nullptr );
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK( loaded_site.current_scout_report.target_lead_id == pinned.lead_id );
    CHECK( loaded_site.current_scout_report.target_lead_revision == pinned.revision );
    REQUIRE( loaded_site.known_recent_marks.size() == 8 );
    CHECK( loaded_site.known_recent_marks.front() == "recent-mark-2" );
    CHECK( loaded_site.known_recent_marks.back() == "recent-mark-9" );
}

TEST_CASE( "bandit_live_world_migrates_and_normalizes_legacy_active_outing_identity",
           "[bandit][live_world][migration]" )
{
    SECTION( "valid legacy active group gains stable identity and generation" ) {
        JsonValue legacy = json_loader::from_string( R"({
            "site_id": "legacy_camp",
            "members": [ { "npc_id": 42, "state": "outbound" } ],
            "active_group_id": "legacy_camp#dispatch",
            "active_target_id": "legacy_target",
            "active_job_type": "scout",
            "active_member_ids": [ 42 ],
            "active_sortie_started_minutes": 120
        })" );
        bandit_live_world::site_record site;
        site.deserialize( legacy.get_object() );

        CHECK( site.schema_version == 12 );
        CHECK( site.active_outing.is_active() );
        CHECK( site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie );
        CHECK( site.active_outing.activity_id == "legacy_camp#dispatch" );
        CHECK( site.active_outing.camp_id == "legacy_camp" );
        CHECK( site.active_outing.generation == 1 );
        CHECK( site.active_outing.last_advanced_minutes == 120 );
        CHECK( site.active_outing.return_application_key == "legacy_camp#dispatch:return:1" );
        CHECK( site.next_outing_generation == 2 );

        bandit_live_world::world_state world;
        world.sites.push_back( site );
        const bandit_live_world::world_state loaded = round_trip_world( world );
        REQUIRE( loaded.sites.size() == 1 );
        CHECK( loaded.sites.front().active_outing.activity_id == "legacy_camp#dispatch" );
        CHECK( loaded.sites.front().active_outing.generation == 1 );
        const std::string modern_json = serialize_world( loaded );
        CHECK( modern_json.find( "active_group_id" ) == std::string::npos );
        CHECK( modern_json.find( "active_target_id" ) == std::string::npos );
        CHECK( modern_json.find( "active_target_omt" ) == std::string::npos );
        CHECK( modern_json.find( "active_job_type" ) == std::string::npos );
        CHECK( modern_json.find( "active_member_ids" ) == std::string::npos );
        CHECK( modern_json.find( "active_sortie_started_minutes" ) == std::string::npos );
        CHECK( modern_json.find( "active_sortie_local_contact_minutes" ) == std::string::npos );
    }

    SECTION( "duplicate legacy reservations close safely instead of wedging the camp" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "site_id": "legacy_camp",
            "members": [ { "npc_id": 43, "state": "outbound" } ],
            "active_group_id": "legacy_camp#dispatch",
            "active_target_id": "legacy_target",
            "active_job_type": "scout",
            "active_member_ids": [ 43, 43 ]
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        CHECK( site.active_outing.target_id.empty() );
        REQUIRE( site.find_member( character_id( 43 ) ) != nullptr );
        CHECK( site.find_member( character_id( 43 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( site.next_outing_generation == 2 );
    }

    SECTION( "legacy scalar local contact migrates to local ownership" ) {
        JsonValue legacy = json_loader::from_string( R"({
            "site_id": "legacy_contact_camp",
            "members": [ { "npc_id": 47, "state": "local_contact" } ],
            "active_group_id": "legacy_contact_camp#dispatch",
            "active_target_id": "legacy_contact_target",
            "active_job_type": "scout",
            "active_member_ids": [ 47 ],
            "active_sortie_started_minutes": 120,
            "active_sortie_local_contact_minutes": 150
        })" );
        bandit_live_world::site_record site;
        site.deserialize( legacy.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( site.active_outing.handoff_epoch == 1 );
        CHECK( site.active_outing.last_advanced_minutes == 150 );
        CHECK( bandit_live_world::current_external_simulation_cursor( site ).has_value() );
    }

    SECTION( "transitional nested identity imports its former site payload" ) {
        JsonValue transitional = json_loader::from_string( R"({
            "schema_version": 2,
            "site_id": "transitional_camp",
            "members": [ { "npc_id": 44, "state": "local_contact" } ],
            "active_outing": {
                "schema_version": 1,
                "kind": "scout_sortie",
                "activity_id": "transitional_camp#dispatch",
                "camp_id": "transitional_camp",
                "generation": 4,
                "simulation_owner": "local",
                "handoff_epoch": 1,
                "return_application_key": "transitional_camp#dispatch:return:4"
            },
            "active_target_id": "legacy_nested_target",
            "active_target_omt": [ 9, 10, 0 ],
            "active_job_type": "scout",
            "active_member_ids": [ 44 ],
            "active_sortie_started_minutes": 200,
            "active_sortie_local_contact_minutes": 240
        })" );
        bandit_live_world::site_record site;
        site.deserialize( transitional.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.schema_version == 12 );
        CHECK( site.active_outing.schema_version == 5 );
        CHECK( site.active_outing.member_ids == std::vector<character_id> { character_id( 44 ) } );
        CHECK( site.active_outing.leader_id == character_id( 44 ) );
        CHECK( site.active_outing.target_id == "legacy_nested_target" );
        CHECK( site.active_outing.target_omt == tripoint_abs_omt( 9, 10, 0 ) );
        CHECK( site.active_outing.job_type == "scout" );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::observing );
        CHECK( site.active_outing.started_minutes == 200 );
        CHECK( site.active_outing.local_contact_minutes == 240 );
        CHECK( site.active_outing.last_progress_minutes == 240 );
        CHECK( site.active_outing.last_advanced_minutes == 240 );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( site.active_outing.handoff_epoch == 1 );
        CHECK( site.active_outing.expected_return_minutes == 1080 );
        CHECK( site.active_outing.missing_deadline_minutes == 2520 );
        CHECK( site.active_outing.report_application_key ==
               "transitional_camp#dispatch:report:4" );
        CHECK( site.active_outing.cargo_application_key ==
               "transitional_camp#dispatch:cargo:4" );
        CHECK( site.next_outing_generation == 5 );
    }

    SECTION( "schema-three local owner repairs its legacy even handoff epoch" ) {
        JsonValue transitional = json_loader::from_string( R"({
            "schema_version": 4,
            "site_id": "owner_epoch_camp",
            "members": [ { "npc_id": 46, "state": "local_contact" } ],
            "active_outing": {
                "schema_version": 3,
                "kind": "scout_sortie",
                "activity_id": "owner_epoch_camp#dispatch",
                "camp_id": "owner_epoch_camp",
                "generation": 2,
                "member_ids": [ 46 ],
                "leader_id": 46,
                "target_id": "legacy_contact",
                "job_type": "scout",
                "phase": "observing",
                "simulation_owner": "local",
                "handoff_epoch": 2,
                "started_minutes": 100,
                "local_contact_minutes": 150,
                "last_progress_minutes": 150,
                "last_advanced_minutes": 150,
                "return_application_key": "owner_epoch_camp#dispatch:return:2",
                "report_application_key": "owner_epoch_camp#dispatch:report:2",
                "cargo_application_key": "owner_epoch_camp#dispatch:cargo:2"
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( transitional.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( site.active_outing.handoff_epoch == 3 );
        CHECK( site.active_outing.last_advanced_minutes == 150 );
        CHECK( bandit_live_world::current_external_simulation_cursor( site ).has_value() );
    }

    SECTION( "current nested identity rejects a forged component key" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 5,
            "kind": "scout_sortie",
            "activity_id": "current-key-camp#dispatch",
            "camp_id": "current-key-camp",
            "generation": 2,
            "member_ids": [ 46 ],
            "leader_id": 46,
            "job_type": "scout",
            "return_application_key": "current-key-camp#dispatch:return:2",
            "report_application_key": "forged-report-key",
            "cargo_application_key": "current-key-camp#dispatch:cargo:2"
        })" );
        bandit_live_world::active_outing_state outing;
        CHECK_THROWS( outing.deserialize( malformed.get_object() ) );
    }

    SECTION( "schema-four torn advancement cursor fails closed" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 5,
            "site_id": "torn_cursor_camp",
            "members": [ { "npc_id": 48, "state": "outbound" } ],
            "active_outing": {
                "schema_version": 4,
                "kind": "scout_sortie",
                "activity_id": "torn_cursor_camp#dispatch",
                "camp_id": "torn_cursor_camp",
                "generation": 1,
                "member_ids": [ 48 ],
                "leader_id": 48,
                "target_id": "torn_target",
                "job_type": "scout",
                "phase": "searching",
                "simulation_owner": "abstract",
                "handoff_epoch": 0,
                "started_minutes": 100,
                "last_progress_minutes": 150,
                "last_advanced_minutes": 140,
                "return_application_key": "torn_cursor_camp#dispatch:return:1",
                "report_application_key": "torn_cursor_camp#dispatch:report:1",
                "cargo_application_key": "torn_cursor_camp#dispatch:cargo:1"
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 48 ) ) != nullptr );
        CHECK( site.find_member( character_id( 48 ) )->state ==
               bandit_live_world::member_state::at_home );
    }

    SECTION( "incomplete schema-four cursor never falls back to legacy scalars" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 5,
            "site_id": "incomplete_cursor_camp",
            "members": [ { "npc_id": 49, "state": "local_contact" } ],
            "active_outing": {
                "schema_version": 4,
                "kind": "scout_sortie",
                "activity_id": "incomplete_cursor_camp#dispatch",
                "camp_id": "incomplete_cursor_camp",
                "generation": 1,
                "simulation_owner": "local",
                "handoff_epoch": 0,
                "last_advanced_minutes": 140
            },
            "active_hostile_operation": {},
            "active_target_id": "legacy_fallback_target",
            "active_job_type": "scout",
            "active_member_ids": [ 49 ],
            "active_sortie_started_minutes": 100,
            "active_sortie_local_contact_minutes": 150
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 49 ) ) != nullptr );
        CHECK( site.find_member( character_id( 49 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK_FALSE( bandit_live_world::current_external_simulation_cursor( site ).has_value() );
    }

    SECTION( "invalid modern identity still releases its embedded reservations" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 3,
            "site_id": "malformed_modern_camp",
            "members": [ { "npc_id": 45, "state": "outbound" } ],
            "active_outing": {
                "schema_version": 2,
                "kind": "none",
                "activity_id": "malformed_modern_camp#dispatch",
                "camp_id": "malformed_modern_camp",
                "generation": 5,
                "member_ids": [ 45 ]
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 45 ) ) != nullptr );
        CHECK( site.find_member( character_id( 45 ) )->state ==
               bandit_live_world::member_state::at_home );
    }
}

TEST_CASE( "bandit_live_world_migrates_or_closes_persisted_hostile_operation_owners",
           "[bandit][live_world][hostile_operation][migration][save]" )
{
    using bandit_live_world::hostile_operation_kind;
    using bandit_live_world::hostile_operation_phase;

    SECTION( "unpinned legacy hostile outing retains its party only for withdrawal" ) {
        JsonValue legacy = json_loader::from_string( R"({
            "schema_version":4,
            "site_id":"legacy_hostile_camp",
            "members":[{"npc_id":54,"state":"outbound"}],
            "current_scout_report":{
                "revision":2,
                "source_activity_id":"legacy_hostile_camp#scout:3",
                "source_generation":3,
                "source_job_type":"scout",
                "target_id":"legacy_target",
                "target_omt":[3,0,0],
                "target_lead_revision":0,
                "application_key":"legacy_hostile_camp#scout:3:report:3",
                "delivered_minutes":90,
                "provisional":false
            },
            "camp_decision":{
                "state":"preparing_follow_on",
                "source_report_revision":2,
                "source_report_generation":3,
                "source_report_activity_id":"legacy_hostile_camp#scout:3",
                "source_report_application_key":"legacy_hostile_camp#scout:3:report:3",
                "target_id":"legacy_target",
                "target_omt":[3,0,0],
                "target_lead_revision":0,
                "last_transition_minutes":95
            },
            "active_outing":{
                "schema_version":3,
                "kind":"hostile_operation",
                "activity_id":"legacy_hostile_camp#dispatch",
                "camp_id":"legacy_hostile_camp",
                "generation":4,
                "member_ids":[54],
                "leader_id":54,
                "shared_route":[[0,0,0],[3,0,0]],
                "waypoint_index":0,
                "target_id":"legacy_target",
                "target_omt":[3,0,0],
                "job_type":"toll",
                "phase":"outbound",
                "started_minutes":100,
                "last_progress_minutes":120,
                "last_advanced_minutes":120,
                "return_application_key":"legacy_hostile_camp#dispatch:return:4",
                "cargo_application_key":"legacy_hostile_camp#dispatch:cargo:4"
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( legacy.get_object() );

        CHECK( site.schema_version == 12 );
        CHECK_FALSE( site.active_outing.is_active() );
        REQUIRE( site.active_hostile_operation.is_active() );
        CHECK( site.active_hostile_operation.operation_kind ==
               hostile_operation_kind::shakedown );
        CHECK( site.active_hostile_operation.phase ==
               hostile_operation_phase::returning_home );
        CHECK( site.active_hostile_operation.legacy_unpinned );
        CHECK( site.active_hostile_operation.has_rally );
        CHECK( site.active_hostile_operation.reservation.activity_id ==
               "legacy_hostile_camp#dispatch" );
        CHECK( site.active_hostile_operation.reservation.generation == 4 );
        CHECK( site.active_hostile_operation.reservation.member_ids ==
               std::vector<character_id> { character_id( 54 ) } );
        CHECK( site.find_member( character_id( 54 ) )->state ==
               bandit_live_world::member_state::outbound );
        CHECK( site.next_outing_generation == 5 );
        const std::string before_escalation = serialize_world(
                                                  bandit_live_world::world_state { 6,
                                                      "hells_raiders_live_owner_v0", 0, 0, -1,
                                                      { site }, {} } );
        CHECK( transition_test_hostile_operation(
                   site, hostile_operation_phase::returning_home,
                   hostile_operation_phase::approaching, 130, "unsafe legacy escalation" ) ==
               bandit_live_world::hostile_operation_transition_result::rejected );
        CHECK( serialize_world( bandit_live_world::world_state { 6,
                                "hells_raiders_live_owner_v0", 0, 0, -1,
                                { site }, {} } ) == before_escalation );
        CHECK( transition_test_hostile_operation(
                   site, hostile_operation_phase::returning_home,
                   hostile_operation_phase::lost, 130, "legacy party lost" ) ==
               bandit_live_world::hostile_operation_transition_result::applied );
        CHECK( serialize_world( bandit_live_world::world_state { 6,
                                "hells_raiders_live_owner_v0", 0, 0, -1,
                                { site }, {} } ) != before_escalation );
    }

    SECTION( "malformed new operation releases every known in-flight reservation" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version":5,
            "site_id":"malformed_hostile_camp",
            "members":[{"npc_id":55,"state":"outbound"}],
            "active_hostile_operation":{
                "operation_kind":"shakedown",
                "phase":"returning_home",
                "legacy_unpinned":true,
                "reservation":{
                    "kind":"hostile_operation",
                    "activity_id":"malformed_hostile_camp#hostile:5",
                    "camp_id":"malformed_hostile_camp",
                    "generation":5,
                    "member_ids":[55,999],
                    "leader_id":55,
                    "job_type":"toll",
                    "phase":"returning_home",
                    "return_application_key":"malformed_hostile_camp#hostile:5:return:5",
                    "cargo_application_key":"malformed_hostile_camp#hostile:5:cargo:5"
                }
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_hostile_operation.is_active() );
        CHECK( site.active_hostile_operation.reservation.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 55 ) ) != nullptr );
        CHECK( site.find_member( character_id( 55 ) )->state ==
               bandit_live_world::member_state::at_home );
    }

    SECTION( "a valid scout owner wins over a duplicate hostile owner" ) {
        JsonValue duplicate = json_loader::from_string( R"({
            "schema_version":5,
            "site_id":"duplicate_owner_camp",
            "members":[
                {"npc_id":60,"state":"outbound"},
                {"npc_id":61,"state":"at_home"}
            ],
            "active_outing":{
                "kind":"scout_sortie",
                "activity_id":"duplicate_owner_camp#scout:2",
                "camp_id":"duplicate_owner_camp",
                "generation":2,
                "member_ids":[60],
                "leader_id":60,
                "job_type":"scout",
                "phase":"outbound",
                "return_application_key":"duplicate_owner_camp#scout:2:return:2",
                "report_application_key":"duplicate_owner_camp#scout:2:report:2",
                "cargo_application_key":"duplicate_owner_camp#scout:2:cargo:2"
            },
            "active_hostile_operation":{
                "operation_kind":"shakedown",
                "phase":"assembling",
                "has_rally":true,
                "rally_omt":[1,0,0],
                "legacy_unpinned":true,
                "reservation":{
                    "kind":"hostile_operation",
                    "activity_id":"duplicate_owner_camp#hostile:3",
                    "camp_id":"duplicate_owner_camp",
                    "generation":3,
                    "member_ids":[61],
                    "leader_id":61,
                    "shared_route":[[0,0,0],[1,0,0],[2,0,0]],
                    "target_omt":[2,0,0],
                    "job_type":"toll",
                    "phase":"assembling"
                }
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( duplicate.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie );
        CHECK_FALSE( site.active_hostile_operation.is_active() );
        CHECK( site.active_hostile_operation.reservation.member_ids.empty() );
        CHECK( site.find_member( character_id( 60 ) )->state ==
               bandit_live_world::member_state::outbound );
        CHECK( site.find_member( character_id( 61 ) )->state ==
               bandit_live_world::member_state::at_home );
    }
}

TEST_CASE( "bandit_live_world_round_trips_bounded_authoritative_scout_state",
           "[bandit][live_world][scout_state][save]" )
{
    bandit_live_world::world_state original;
    add_bandit_camp_member( original, 0, 45000 );
    add_bandit_camp_member( original, 1, 45000 );
    bandit_live_world::site_record &site = original.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45000 ),
             bandit_live_world::member_state::local_contact, "test scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45001 ),
             bandit_live_world::member_state::dead, "test recorded casualty" ) );
    set_test_active_outing( site, site.site_id + "#scout:full" );
    site.active_outing.member_ids = { character_id( 45000 ), character_id( 45001 ) };
    site.active_outing.leader_id = character_id( 45000 );
    site.active_outing.shared_route = { site.anchor, tripoint_abs_omt( 12, 20, 0 ),
                                       tripoint_abs_omt( 14, 20, 0 ) };
    site.active_outing.waypoint_index = 1;
    site.active_outing.target_id = "resource-lead-7";
    site.active_outing.target_omt = tripoint_abs_omt( 14, 20, 0 );
    site.active_outing.job_type = "scout";
    site.active_outing.target_lead_revision = 7;
    site.active_outing.phase = bandit_live_world::scout_phase::burned_withdrawal;
    site.active_outing.observations.push_back( { "burn@14,20,0", "two observers exposed", 83,
                                                 330, true,
                                                 bandit_live_world::sortie_observation_kind::routine, "" } );
    site.active_outing.cargo.supply_units = 3;
    site.active_outing.cargo.trade_value = 90;
    site.active_outing.casualty_ids = { character_id( 45001 ) };
    site.active_outing.started_minutes = 100;
    site.active_outing.local_contact_minutes = 300;
    site.active_outing.last_progress_minutes = 330;
    site.active_outing.expected_return_minutes = 940;
    site.active_outing.missing_deadline_minutes = 2380;
    site.active_outing.owner = bandit_live_world::simulation_owner::local;
    site.active_outing.handoff_epoch = 1;
    site.active_outing.last_advanced_minutes = 331;

    const std::vector<bandit_live_world::scout_phase> phases = {
        bandit_live_world::scout_phase::assembling,
        bandit_live_world::scout_phase::outbound,
        bandit_live_world::scout_phase::searching,
        bandit_live_world::scout_phase::observing,
        bandit_live_world::scout_phase::harvesting,
        bandit_live_world::scout_phase::burned_withdrawal,
        bandit_live_world::scout_phase::returning_exposed,
        bandit_live_world::scout_phase::returning_report,
        bandit_live_world::scout_phase::returning_home,
        bandit_live_world::scout_phase::lost,
    };
    for( const bandit_live_world::scout_phase phase : phases ) {
        original.sites.front().active_outing.phase = phase;
        const bandit_live_world::world_state loaded = round_trip_world( original );
        REQUIRE( loaded.schema_version == 6 );
        REQUIRE( loaded.sites.size() == 1 );
        const bandit_live_world::active_outing_state &outing = loaded.sites.front().active_outing;
        CHECK( outing.schema_version == 5 );
        CHECK( outing.phase == phase );
        CHECK( outing.member_ids == original.sites.front().active_outing.member_ids );
        CHECK( outing.leader_id == character_id( 45000 ) );
        CHECK( outing.shared_route == original.sites.front().active_outing.shared_route );
        CHECK( outing.waypoint_index == 1 );
        CHECK( outing.target_id == "resource-lead-7" );
        CHECK( outing.target_omt == tripoint_abs_omt( 14, 20, 0 ) );
        CHECK( outing.target_lead_revision == 7 );
        REQUIRE( outing.observations.size() == 1 );
        CHECK( outing.observations.front().fact_key == "burn@14,20,0" );
        CHECK( outing.observations.front().confidence == 83 );
        CHECK( outing.observations.front().critical );
        CHECK( outing.cargo.supply_units == 3 );
        CHECK( outing.cargo.trade_value == 90 );
        CHECK( outing.casualty_ids == std::vector<character_id> { character_id( 45001 ) } );
        CHECK( outing.started_minutes == 100 );
        CHECK( outing.local_contact_minutes == 300 );
        CHECK( outing.last_progress_minutes == 330 );
        CHECK( outing.expected_return_minutes == 940 );
        CHECK( outing.missing_deadline_minutes == 2380 );
        CHECK( outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( outing.handoff_epoch == 1 );
        CHECK( outing.last_advanced_minutes == 331 );
        CHECK_FALSE( outing.return_application_key.empty() );
        CHECK_FALSE( outing.report_application_key.empty() );
        CHECK_FALSE( outing.cargo_application_key.empty() );
    }
}

TEST_CASE( "bandit_live_world_simulation_owner_handoff_is_shared_and_token_checked",
           "[bandit][live_world][handoff][owner]" )
{
    using bandit_live_world::simulation_owner;
    using bandit_live_world::simulation_owner_transition_result;

    bandit_live_world::world_state scout_world;
    add_bandit_camp_member( scout_world, 0, 45500 );
    add_bandit_camp_member( scout_world, 1, 45500 );
    bandit_live_world::site_record &scout_site = scout_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state(
                 scout_site, character_id( 45500 ), bandit_live_world::member_state::outbound,
                 "test scout departed" ) );
    set_test_active_outing( scout_site, scout_site.site_id + "#scout:owner" );
    scout_site.active_outing.member_ids = { character_id( 45500 ) };
    scout_site.active_outing.leader_id = character_id( 45500 );
    scout_site.active_outing.phase = bandit_live_world::scout_phase::outbound;
    scout_site.active_outing.started_minutes = 10;
    scout_site.active_outing.last_progress_minutes = 10;
    scout_site.active_outing.last_advanced_minutes = 10;
    const std::string scout_id = scout_site.active_outing.activity_id;
    const int scout_generation = scout_site.active_outing.generation;
    const std::string initial_scout = serialize_world( scout_world );

    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 9, 11 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( bandit_live_world::transition_external_simulation_owner(
               scout_site, scout_id + ":stale", scout_generation,
               simulation_owner::abstract, simulation_owner::local, 0, 10, 11 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( serialize_world( scout_world ) == initial_scout );

    REQUIRE( bandit_live_world::advance_external_simulation(
                 scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 10, 11 ) ==
             simulation_owner_transition_result::applied );
    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 10, 12 ) ==
           simulation_owner_transition_result::rejected );
    const std::string after_advance = serialize_world( scout_world );
    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 11, 11 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( serialize_world( scout_world ) == after_advance );

    REQUIRE( bandit_live_world::transition_external_simulation_owner(
                 scout_site, scout_id, scout_generation, simulation_owner::abstract,
                 simulation_owner::local, 0, 11, 12 ) ==
             simulation_owner_transition_result::applied );
    CHECK( scout_site.active_outing.owner == simulation_owner::local );
    CHECK( scout_site.active_outing.handoff_epoch == 1 );
    CHECK( scout_site.active_outing.last_advanced_minutes == 12 );
    const std::string local_scout = serialize_world( scout_world );
    CHECK( bandit_live_world::transition_external_simulation_owner(
               scout_site, scout_id, scout_generation, simulation_owner::abstract,
               simulation_owner::local, 0, 11, 12 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 11, 13 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( serialize_world( scout_world ) == local_scout );

    REQUIRE( bandit_live_world::advance_external_simulation(
                 scout_site, scout_id, scout_generation, simulation_owner::local, 1, 12, 13 ) ==
             simulation_owner_transition_result::applied );
    REQUIRE( bandit_live_world::transition_external_simulation_owner(
                 scout_site, scout_id, scout_generation, simulation_owner::local,
                 simulation_owner::abstract, 1, 13, 14 ) ==
             simulation_owner_transition_result::applied );
    const bandit_live_world::world_state loaded_scout = round_trip_world( scout_world );
    REQUIRE( loaded_scout.sites.size() == 1 );
    CHECK( loaded_scout.sites.front().active_outing.owner == simulation_owner::abstract );
    CHECK( loaded_scout.sites.front().active_outing.handoff_epoch == 2 );
    CHECK( loaded_scout.sites.front().active_outing.last_advanced_minutes == 14 );

    bandit_live_world::world_state hostile_world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( hostile_world, index, 45600 );
    }
    bandit_live_world::site_record &hostile_site = hostile_world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    prepare_hostile_follow_on( hostile_site, 5, 4, "owner-target", target, 700 );
    const bandit_live_world::hostile_operation_plan hostile_plan =
        bandit_live_world::plan_hostile_operation(
            hostile_site, bandit_live_world::hostile_operation_kind::shakedown,
            { hostile_site.anchor, rally, target }, rally, 702 );
    REQUIRE( hostile_plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( hostile_site, hostile_plan ) );
    REQUIRE( transition_test_hostile_operation(
                 hostile_site, bandit_live_world::hostile_operation_phase::assembling,
                 bandit_live_world::hostile_operation_phase::outbound, 703,
                 "test hostile departed" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
    bandit_live_world::active_outing_state &hostile_reservation =
        hostile_site.active_hostile_operation.reservation;
    REQUIRE( bandit_live_world::transition_external_simulation_owner(
                 hostile_site, hostile_reservation.activity_id,
                 hostile_reservation.generation, simulation_owner::abstract,
                 simulation_owner::local, 0, 703, 704 ) ==
             simulation_owner_transition_result::applied );
    const bandit_live_world::world_state loaded_hostile = round_trip_world( hostile_world );
    REQUIRE( loaded_hostile.sites.size() == 1 );
    REQUIRE( loaded_hostile.sites.front().active_hostile_operation.is_active() );
    CHECK( loaded_hostile.sites.front().active_hostile_operation.reservation.owner ==
           simulation_owner::local );
    CHECK( loaded_hostile.sites.front().active_hostile_operation.reservation.handoff_epoch == 1 );
    CHECK( loaded_hostile.sites.front().active_hostile_operation.reservation.last_advanced_minutes == 704 );

    bandit_live_world::world_state repaired_hostile = hostile_world;
    repaired_hostile.sites.front().active_hostile_operation.reservation.owner =
        simulation_owner::local;
    repaired_hostile.sites.front().active_hostile_operation.reservation.handoff_epoch = 0;
    repaired_hostile.sites.front().active_hostile_operation.reservation.last_advanced_minutes = -1;
    repaired_hostile = round_trip_legacy_site_world( repaired_hostile );
    CHECK_FALSE( repaired_hostile.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_hostile.sites.front().active_hostile_operation.reservation.member_ids.empty() );
}

TEST_CASE( "bandit_live_world_scout_phase_transitions_are_one_way_and_atomic",
           "[bandit][live_world][scout_state][phase]" )
{
    using bandit_live_world::scout_phase;
    using bandit_live_world::scout_phase_transition_result;

    const std::vector<scout_phase> phases = {
        scout_phase::assembling,
        scout_phase::outbound,
        scout_phase::searching,
        scout_phase::observing,
        scout_phase::harvesting,
        scout_phase::burned_withdrawal,
        scout_phase::returning_exposed,
        scout_phase::returning_report,
        scout_phase::returning_home,
        scout_phase::lost,
    };
    const std::vector<std::pair<scout_phase, scout_phase>> forward_edges = {
        { scout_phase::assembling, scout_phase::outbound },
        { scout_phase::outbound, scout_phase::searching },
        { scout_phase::outbound, scout_phase::observing },
        { scout_phase::outbound, scout_phase::returning_report },
        { scout_phase::outbound, scout_phase::returning_home },
        { scout_phase::searching, scout_phase::observing },
        { scout_phase::searching, scout_phase::returning_report },
        { scout_phase::searching, scout_phase::returning_home },
        { scout_phase::observing, scout_phase::harvesting },
        { scout_phase::observing, scout_phase::burned_withdrawal },
        { scout_phase::observing, scout_phase::returning_report },
        { scout_phase::observing, scout_phase::returning_home },
        { scout_phase::harvesting, scout_phase::returning_report },
        { scout_phase::harvesting, scout_phase::returning_home },
        { scout_phase::burned_withdrawal, scout_phase::returning_exposed },
        { scout_phase::burned_withdrawal, scout_phase::returning_report },
        { scout_phase::burned_withdrawal, scout_phase::returning_home },
        { scout_phase::returning_exposed, scout_phase::returning_report },
        { scout_phase::returning_exposed, scout_phase::returning_home },
        { scout_phase::returning_report, scout_phase::returning_home },
    };
    for( const scout_phase previous : phases ) {
        for( const scout_phase next : phases ) {
            const bool same_phase = previous == next;
            const bool terminal_loss = next == scout_phase::lost &&
                                       previous != scout_phase::lost;
            const bool forward = std::find( forward_edges.begin(), forward_edges.end(),
                                            std::make_pair( previous, next ) ) !=
                                 forward_edges.end();
            CHECK( bandit_live_world::is_valid_scout_phase_transition( previous, next ) ==
                   ( same_phase || terminal_loss || forward ) );
        }
    }

    CHECK( bandit_live_world::scout_phase_after_burned_evacuation( true ) ==
           scout_phase::returning_report );
    CHECK( bandit_live_world::scout_phase_after_burned_evacuation( false ) ==
           scout_phase::returning_exposed );
    CHECK( bandit_live_world::scout_phase_requires_homeward_only(
               scout_phase::burned_withdrawal ) );
    CHECK( bandit_live_world::scout_phase_requires_homeward_only(
               scout_phase::returning_exposed ) );
    CHECK_FALSE( bandit_live_world::scout_phase_requires_homeward_only(
                     scout_phase::observing ) );

    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45100 );
    bandit_live_world::site_record &site = world.sites.front();
    set_test_active_outing( site, site.site_id + "#scout:phase" );
    site.active_outing.member_ids = { character_id( 45100 ) };
    site.active_outing.leader_id = character_id( 45100 );
    site.active_outing.job_type = "scout";
    site.active_outing.phase = scout_phase::outbound;
    site.active_outing.started_minutes = 100;
    site.active_outing.last_progress_minutes = 100;
    site.active_outing.last_advanced_minutes = 100;

    const bandit_live_world::simulation_advance_cursor initial_cursor =
        require_current_simulation_cursor( site );
    const std::string before_stale = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::assembling, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == before_stale );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 99 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == before_stale );

    site.active_outing.kind = bandit_live_world::outing_kind::hostile_operation;
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::rejected );
    site.active_outing.kind = bandit_live_world::outing_kind::scout_sortie;
    site.active_outing.job_type = "raid";
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::rejected );
    site.active_outing.job_type = "scout";
    CHECK( serialize_world( world ) == before_stale );

    bandit_live_world::site_record legacy_scavenge = site;
    legacy_scavenge.active_outing.job_type = "scavenge";
    CHECK( bandit_live_world::transition_active_scout_phase(
               legacy_scavenge, require_current_simulation_cursor( legacy_scavenge ),
               scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::applied );

    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::applied );
    CHECK( site.active_outing.phase == scout_phase::searching );
    CHECK( site.active_outing.last_progress_minutes == 110 );
    CHECK( site.active_outing.last_advanced_minutes == 110 );
    const std::string after_first_advance = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::searching, scout_phase::observing, 120 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == after_first_advance );
    const std::string before_same = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::searching,
               scout_phase::searching, 120 ) ==
           scout_phase_transition_result::unchanged );
    CHECK( serialize_world( world ) == before_same );

    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::searching,
               scout_phase::observing, 120 ) ==
           scout_phase_transition_result::applied );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::observing,
               scout_phase::burned_withdrawal, 130 ) ==
           scout_phase_transition_result::applied );
    const std::string burned_state = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::burned_withdrawal,
               scout_phase::observing, 140 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == burned_state );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::burned_withdrawal,
               scout_phase::returning_exposed, 140 ) ==
           scout_phase_transition_result::applied );
    const std::string exposed_state = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_exposed,
               scout_phase::harvesting, 150 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == exposed_state );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_exposed,
               scout_phase::returning_report, 150 ) ==
           scout_phase_transition_result::applied );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_report,
               scout_phase::returning_home, 160 ) ==
           scout_phase_transition_result::applied );
    const std::string home_state = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_home,
               scout_phase::outbound, 170 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == home_state );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_home,
               scout_phase::lost, 170 ) ==
           scout_phase_transition_result::applied );

    bandit_live_world::world_state returning_world;
    add_bandit_camp_member( returning_world, 0, 45110 );
    bandit_live_world::site_record &returning_site = returning_world.sites.front();
    set_test_active_outing( returning_site, returning_site.site_id + "#scout:returning" );
    returning_site.active_outing.member_ids = { character_id( 45110 ) };
    returning_site.active_outing.leader_id = character_id( 45110 );
    returning_site.active_outing.job_type = "scout";
    returning_site.active_outing.phase = scout_phase::returning_exposed;
    returning_site.active_outing.started_minutes = 100;
    returning_site.active_outing.last_progress_minutes = 200;
    returning_site.active_outing.last_advanced_minutes = 200;
    const std::string before_contact = serialize_world( returning_world );
    CHECK_FALSE( bandit_live_world::note_active_sortie_local_contact(
                     returning_site, require_current_simulation_cursor( returning_site ),
                     character_id( 45110 ), 210 ) );
    CHECK( serialize_world( returning_world ) == before_contact );

    returning_site.active_outing.started_minutes = -1;
    returning_site.active_outing.phase = scout_phase::burned_withdrawal;
    const std::string before_restart = serialize_world( returning_world );
    CHECK_FALSE( bandit_live_world::note_active_sortie_started(
                     returning_site, require_current_simulation_cursor( returning_site ), 220 ) );
    CHECK( serialize_world( returning_world ) == before_restart );

    JsonValue future_phase_json = json_loader::from_string(
        R"({"kind":"scout_sortie","activity_id":"future-phase","generation":1,"job_type":"scout","phase":"future_phase"})" );
    bandit_live_world::active_outing_state future_phase;
    future_phase.deserialize( future_phase_json.get_object() );
    CHECK( future_phase.phase == scout_phase::lost );

    JsonValue legacy_phase_json = json_loader::from_string(
        R"({"kind":"scout_sortie","activity_id":"legacy-phase","generation":1,"job_type":"scout"})" );
    bandit_live_world::active_outing_state legacy_phase;
    legacy_phase.deserialize( legacy_phase_json.get_object() );
    CHECK( legacy_phase.phase == scout_phase::assembling );

    bandit_live_world::world_state malformed_job_world;
    add_bandit_camp_member( malformed_job_world, 0, 45120 );
    bandit_live_world::site_record &malformed_job_site = malformed_job_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( malformed_job_site, character_id( 45120 ),
             bandit_live_world::member_state::outbound, "malformed scout reservation" ) );
    set_test_active_outing( malformed_job_site,
                            malformed_job_site.site_id + "#scout:malformed-job" );
    malformed_job_site.active_outing.member_ids = { character_id( 45120 ) };
    malformed_job_site.active_outing.leader_id = character_id( 45120 );
    malformed_job_site.active_outing.job_type = "raid";
    const bandit_live_world::world_state repaired_job =
        round_trip_legacy_site_world( malformed_job_world );
    REQUIRE( repaired_job.sites.size() == 1 );
    CHECK_FALSE( repaired_job.sites.front().active_outing.is_active() );
    CHECK( repaired_job.sites.front().find_member( character_id( 45120 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_camp_decision_pins_only_final_scout_reports_and_transitions_once",
           "[bandit][live_world][camp_decision][save][replay]" )
{
    using bandit_live_world::camp_decision_state;
    using bandit_live_world::camp_decision_transition_result;

    const std::vector<camp_decision_state> states = {
        camp_decision_state::idle,
        camp_decision_state::report_awaiting_assessment,
        camp_decision_state::preparing_follow_on,
        camp_decision_state::cooldown,
        camp_decision_state::abandoned,
    };
    const std::vector<std::pair<camp_decision_state, camp_decision_state>> forward_edges = {
        { camp_decision_state::idle, camp_decision_state::report_awaiting_assessment },
        { camp_decision_state::report_awaiting_assessment,
          camp_decision_state::preparing_follow_on },
        { camp_decision_state::report_awaiting_assessment, camp_decision_state::cooldown },
        { camp_decision_state::preparing_follow_on, camp_decision_state::cooldown },
        { camp_decision_state::cooldown, camp_decision_state::idle },
        { camp_decision_state::cooldown,
          camp_decision_state::report_awaiting_assessment },
        { camp_decision_state::abandoned,
          camp_decision_state::report_awaiting_assessment },
    };
    for( const camp_decision_state previous : states ) {
        for( const camp_decision_state next : states ) {
            const bool same_state = previous == next;
            const bool fail_closed = next == camp_decision_state::abandoned &&
                                     previous != camp_decision_state::abandoned;
            const bool forward = std::find( forward_edges.begin(), forward_edges.end(),
                                            std::make_pair( previous, next ) ) !=
                                 forward_edges.end();
            CHECK( bandit_live_world::is_valid_camp_decision_transition( previous, next ) ==
                   ( same_state || fail_closed || forward ) );
        }
    }

    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        add_bandit_camp_member( world, index, 45130 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan stale_idle_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "decision-target" );
    REQUIRE( stale_idle_plan.valid );
    site.current_scout_report.revision = 7;
    site.current_scout_report.action_policy =
        bandit_live_world::camp_report_policy::bandit_shakedown;
    site.current_scout_report.source_activity_id = site.site_id + "#scout:decision";
    site.current_scout_report.source_generation = 4;
    site.current_scout_report.source_job_type = "scout";
    site.current_scout_report.target_id = "decision-target";
    site.current_scout_report.target_omt = tripoint_abs_omt( 18, 20, 0 );
    site.current_scout_report.target_lead_revision = 3;
    site.current_scout_report.application_key =
        site.current_scout_report.source_activity_id + ":report:4";
    site.current_scout_report.delivered_minutes = 600;
    site.current_scout_report.provisional = true;

    const std::string before_provisional = serialize_world( world );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_provisional );
    site.current_scout_report.provisional = false;
    site.current_scout_report.source_job_type = "scavenge";
    const std::string before_scavenge = serialize_world( world );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_scavenge );
    site.current_scout_report.source_job_type = "scout";
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::applied );
    CHECK( site.camp_decision.state == camp_decision_state::report_awaiting_assessment );
    CHECK( site.camp_decision.source_report_revision == 7 );
    CHECK( site.camp_decision.source_report_generation == 4 );
    CHECK( site.camp_decision.source_report_activity_id ==
           site.current_scout_report.source_activity_id );
    CHECK( site.camp_decision.source_report_application_key ==
           site.current_scout_report.application_key );
    CHECK_FALSE( bandit_live_world::plan_site_dispatch(
                     site, tripoint_abs_omt( 18, 20, 0 ), "decision-target" ).valid );
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( site, stale_idle_plan ) );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::unchanged );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().camp_decision.state ==
           camp_decision_state::report_awaiting_assessment );
    CHECK( loaded.sites.front().camp_decision.source_report_generation == 4 );
    CHECK( loaded.sites.front().camp_decision.target_id == "decision-target" );
    CHECK( loaded.sites.front().current_scout_report.action_policy ==
           bandit_live_world::camp_report_policy::bandit_shakedown );
    CHECK( loaded.sites.front().camp_decision.report_policy ==
           bandit_live_world::camp_report_policy::bandit_shakedown );
    REQUIRE( loaded.sites.front().acted_reports.size() == 1 );
    CHECK( loaded.sites.front().acted_reports.front().report_revision == 7 );

    const std::string before_stale = serialize_world( world );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 6, 4, 700, -1, "wrong report" ) ==
           camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 3, 700, -1, "wrong generation" ) ==
           camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 4, 599, -1, "stale time" ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_stale );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::report_awaiting_assessment, 7, 4, 700, -1, "same" ) ==
           camp_decision_transition_result::unchanged );
    CHECK( serialize_world( world ) == before_stale );

    site.active_outing.kind = bandit_live_world::outing_kind::structural_sortie;
    site.active_outing.activity_id = site.site_id + "#structural";
    site.active_outing.generation = 8;
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 4, 700, -1, "slot busy" ) ==
           camp_decision_transition_result::rejected );
    site.active_outing.clear();
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 4, 700, -1, "assessment passed" ) ==
           camp_decision_transition_result::applied );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::preparing_follow_on,
               camp_decision_state::cooldown, 7, 4, 800, 799, "invalid cooldown" ) ==
           camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::preparing_follow_on,
               camp_decision_state::cooldown, 7, 4, 800, 900, "bounded cooldown" ) ==
           camp_decision_transition_result::applied );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::cooldown, camp_decision_state::idle,
               7, 4, 899, -1, "too early" ) == camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::cooldown, camp_decision_state::idle,
               7, 4, 900, -1, "cooldown elapsed" ) == camp_decision_transition_result::applied );
    CHECK( site.camp_decision.state == camp_decision_state::idle );
    CHECK( site.camp_decision.source_report_revision == 7 );
    CHECK( site.camp_decision.source_report_generation == 4 );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );

    bandit_live_world::world_state abandoned_world = loaded;
    bandit_live_world::site_record &abandoned_site = abandoned_world.sites.front();
    CHECK( bandit_live_world::transition_camp_decision_state(
               abandoned_site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::abandoned, 7, 4, 700, -1, "assessment rejected" ) ==
           camp_decision_transition_result::applied );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( abandoned_site ) ==
           camp_decision_transition_result::rejected );
    abandoned_site.current_scout_report.revision = 8;
    abandoned_site.current_scout_report.source_generation = 5;
    abandoned_site.current_scout_report.source_activity_id = abandoned_site.site_id +
            "#scout:new-decision";
    abandoned_site.current_scout_report.application_key =
        abandoned_site.current_scout_report.source_activity_id + ":report:5";
    abandoned_site.current_scout_report.delivered_minutes = 800;
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( abandoned_site ) ==
           camp_decision_transition_result::applied );

    JsonValue unknown_state_json = json_loader::from_string(
                                       R"({"state":"future_state"})" );
    bandit_live_world::camp_decision_record unknown_state;
    unknown_state.deserialize( unknown_state_json.get_object() );
    CHECK( unknown_state.state == camp_decision_state::abandoned );
    CHECK( unknown_state.transition_reason == "unknown persisted camp decision state" );

    JsonValue missing_state_json = json_loader::from_string( R"({"schema_version":0})" );
    bandit_live_world::camp_decision_record missing_state;
    missing_state.deserialize( missing_state_json.get_object() );
    CHECK( missing_state.state == camp_decision_state::idle );

    JsonValue unknown_policy_json = json_loader::from_string(
                                        R"({"schema_version":2,"state":"report_awaiting_assessment","report_policy":"future_policy"})" );
    bandit_live_world::camp_decision_record unknown_policy;
    unknown_policy.deserialize( unknown_policy_json.get_object() );
    CHECK( unknown_policy.state == camp_decision_state::abandoned );
    CHECK( unknown_policy.report_policy == bandit_live_world::camp_report_policy::none );
    CHECK( unknown_policy.transition_reason == "unknown persisted camp report policy" );

    JsonValue legacy_final_report_json = json_loader::from_string( R"({
        "site_id":"legacy-final-report",
        "current_scout_report":{
            "revision":2,
            "source_activity_id":"legacy-final-report#scout:2",
            "source_generation":2,
            "source_job_type":"scout",
            "target_id":"legacy-target",
            "target_omt":[4,5,0],
            "target_lead_revision":1,
            "application_key":"legacy-final-report#scout:2:report:2",
            "delivered_minutes":500,
            "provisional":false
        }
    })" );
    bandit_live_world::site_record migrated_final_report;
    migrated_final_report.deserialize( legacy_final_report_json.get_object() );
    CHECK( migrated_final_report.camp_decision.state ==
           camp_decision_state::report_awaiting_assessment );
    CHECK( migrated_final_report.camp_decision.source_report_generation == 2 );
    CHECK( migrated_final_report.camp_decision.target_id == "legacy-target" );
    CHECK( migrated_final_report.current_scout_report.action_policy ==
           bandit_live_world::camp_report_policy::bandit_shakedown );
    CHECK( migrated_final_report.camp_decision.report_policy ==
           bandit_live_world::camp_report_policy::bandit_shakedown );
    REQUIRE( migrated_final_report.acted_reports.size() == 1 );
    CHECK( migrated_final_report.acted_reports.front().report_revision == 2 );

    JsonValue legacy_unknown_policy_json = json_loader::from_string( R"({
        "schema_version":7,
        "site_kind":"bandit_camp",
        "current_scout_report":{
            "revision":2,
            "action_policy":"future_policy",
            "source_activity_id":"legacy-unknown#scout:2",
            "source_generation":2,
            "source_job_type":"scout",
            "target_id":"legacy-target",
            "target_omt":[4,5,0],
            "application_key":"legacy-unknown#scout:2:report:2",
            "delivered_minutes":500,
            "provisional":false
        }
    })" );
    bandit_live_world::site_record legacy_unknown_policy;
    legacy_unknown_policy.deserialize( legacy_unknown_policy_json.get_object() );
    CHECK( legacy_unknown_policy.current_scout_report.action_policy ==
           bandit_live_world::camp_report_policy::none );
    CHECK( legacy_unknown_policy.camp_decision.state == camp_decision_state::idle );
    CHECK( legacy_unknown_policy.acted_reports.empty() );

    bandit_live_world::world_state malformed_world = loaded;
    malformed_world.sites.front().camp_decision.source_report_application_key = "forged-key";
    const bandit_live_world::world_state repaired = round_trip_world( malformed_world );
    REQUIRE( repaired.sites.size() == 1 );
    CHECK( repaired.sites.front().camp_decision.state == camp_decision_state::abandoned );
    CHECK( repaired.sites.front().camp_decision.transition_reason ==
           "repaired inconsistent persisted camp decision" );

    bandit_live_world::world_state malformed_policy_world = loaded;
    malformed_policy_world.sites.front().current_scout_report.action_policy =
        bandit_live_world::camp_report_policy::none;
    const bandit_live_world::world_state repaired_policy = round_trip_world(
                malformed_policy_world );
    REQUIRE( repaired_policy.sites.size() == 1 );
    CHECK( repaired_policy.sites.front().camp_decision.state ==
           camp_decision_state::abandoned );
}

TEST_CASE( "bandit_live_world_acts_once_per_target_and_report_policy",
           "[bandit][live_world][camp_decision][report_policy][replay][save]" )
{
    using bandit_live_world::camp_decision_state;
    using bandit_live_world::camp_decision_transition_result;
    using bandit_live_world::camp_report_policy;

    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        add_bandit_camp_member( world, index, 45160 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const auto install_report = [&site]( const std::string &target_id,
    const tripoint_abs_omt & target_omt, const int generation, const int revision,
    const int delivered_minutes, const camp_report_policy policy ) {
        site.current_scout_report.clear();
        site.current_scout_report.revision = revision;
        site.current_scout_report.action_policy = policy;
        site.current_scout_report.source_activity_id = site.site_id + "#scout:" +
                target_id + ":" + std::to_string( generation );
        site.current_scout_report.source_generation = generation;
        site.current_scout_report.source_job_type = "scout";
        site.current_scout_report.target_id = target_id;
        site.current_scout_report.target_omt = target_omt;
        site.current_scout_report.application_key =
            site.current_scout_report.source_activity_id + ":report:" +
            std::to_string( revision );
        site.current_scout_report.delivered_minutes = delivered_minutes;
        site.applied_report_generation = std::max( site.applied_report_generation, generation );
        site.next_outing_generation = std::max( site.next_outing_generation, generation + 1 );
    };
    const auto finish_assessment = [&site]( const int current_minutes ) {
        const int revision = site.camp_decision.source_report_revision;
        const int generation = site.camp_decision.source_report_generation;
        REQUIRE( bandit_live_world::transition_camp_decision_state(
                     site, camp_decision_state::report_awaiting_assessment,
                     camp_decision_state::cooldown, revision, generation,
                     current_minutes, current_minutes + 1, "assessment complete" ) ==
                 camp_decision_transition_result::applied );
        REQUIRE( bandit_live_world::transition_camp_decision_state(
                     site, camp_decision_state::cooldown, camp_decision_state::idle,
                     revision, generation, current_minutes + 1, -1, "cooldown elapsed" ) ==
                 camp_decision_transition_result::applied );
    };

    const tripoint_abs_omt target_a( 18, 20, 0 );
    const tripoint_abs_omt target_b( 19, 20, 0 );
    install_report( "target-a", target_a, 4, 7, 600,
                    camp_report_policy::bandit_shakedown );
    REQUIRE( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
             camp_decision_transition_result::applied );
    finish_assessment( 610 );

    install_report( "target-b", target_b, 1, 1, 700,
                    camp_report_policy::bandit_shakedown );
    REQUIRE( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
             camp_decision_transition_result::applied );
    finish_assessment( 710 );

    install_report( "target-a", target_a, 4, 7, 800,
                    camp_report_policy::bandit_shakedown );
    const std::string before_replay = serialize_world( world );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_replay );

    install_report( "target-a", target_a, 4, 8, 800,
                    camp_report_policy::bandit_shakedown );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::applied );
    CHECK( site.camp_decision.report_policy == camp_report_policy::bandit_shakedown );
    REQUIRE( site.acted_reports.size() == 2 );
    const auto target_a_summary = std::find_if( site.acted_reports.begin(),
    site.acted_reports.end(), []( const bandit_live_world::acted_report_summary & summary ) {
        return summary.target_id == "target-a";
    } );
    REQUIRE( target_a_summary != site.acted_reports.end() );
    CHECK( target_a_summary->report_revision == 8 );

    finish_assessment( 810 );
    site.profile = bandit_live_world::hostile_site_profile::cannibal_camp;
    install_report( "target-a", target_a, 1, 1, 900,
                    camp_report_policy::cannibal_night_raid );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::applied );
    CHECK( site.camp_decision.report_policy == camp_report_policy::cannibal_night_raid );
    REQUIRE( site.acted_reports.size() == 3 );

    const bandit_live_world::world_state loaded_world = round_trip_world( world );
    REQUIRE( loaded_world.sites.size() == 1 );
    CHECK( loaded_world.sites.front().acted_reports.size() == 3 );
    CHECK( serialize_world( loaded_world ) == serialize_world( world ) );
}

TEST_CASE( "bandit_live_world_bounds_acted_report_watermarks_canonically",
           "[bandit][live_world][camp_decision][report_policy][capacity][save]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        add_bandit_camp_member( world, index, 45180 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    prepare_hostile_follow_on( site, 1, 1, "pinned-target",
                               tripoint_abs_omt( 18, 20, 0 ), 100 );
    for( int index = 0; index < 65; ++index ) {
        bandit_live_world::acted_report_summary summary;
        summary.target_id = "target-" + std::to_string( index );
        summary.target_omt = tripoint_abs_omt( 20 + index, 20, 0 );
        summary.policy = bandit_live_world::camp_report_policy::bandit_shakedown;
        summary.source_generation = index + 1;
        summary.report_revision = index + 1;
        summary.acted_minutes = 200 + index;
        site.acted_reports.push_back( summary );
    }
    bandit_live_world::world_state reversed_world = world;
    std::reverse( reversed_world.sites.front().acted_reports.begin(),
                  reversed_world.sites.front().acted_reports.end() );
    CHECK( serialize_world( world ) == serialize_world( reversed_world ) );

    const bandit_live_world::world_state loaded = round_trip_world( reversed_world );
    REQUIRE( loaded.sites.size() == 1 );
    const std::vector<bandit_live_world::acted_report_summary> &acted =
        loaded.sites.front().acted_reports;
    REQUIRE( acted.size() == 64 );
    CHECK( std::any_of( acted.begin(), acted.end(),
    []( const bandit_live_world::acted_report_summary & summary ) {
        return summary.target_id == "pinned-target";
    } ) );
    CHECK_FALSE( std::any_of( acted.begin(), acted.end(),
    []( const bandit_live_world::acted_report_summary & summary ) {
        return summary.target_id == "target-0" || summary.target_id == "target-1";
    } ) );
}

TEST_CASE( "bandit_live_world_plans_and_applies_a_fresh_report_pinned_hostile_operation",
           "[bandit][live_world][hostile_operation][plan][save][replay]" )
{
    using bandit_live_world::hostile_operation_kind;
    using bandit_live_world::hostile_operation_phase;

    bandit_live_world::world_state world;
    for( int index = 0; index < 7; ++index ) {
        add_bandit_camp_member( world, index, 45200 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    const std::vector<tripoint_abs_omt> route = { site.anchor, rally, target };
    const std::vector<character_id> members = {
        character_id( 45200 ), character_id( 45201 )
    };
    prepare_hostile_follow_on( site, 9, 4, "report-target", target, 600 );

    site.find_member( character_id( 45200 ) )->wounded_or_unready = true;
    const bandit_live_world::response_party_selection_result shifted_selection =
        bandit_live_world::select_fresh_response_party(
            site, hostile_operation_kind::shakedown );
    REQUIRE( shifted_selection.eligible );
    CHECK_FALSE( shifted_selection.threat_derived );
    CHECK( shifted_selection.party_size == 2 );
    const std::vector<character_id> shifted_expected = {
        character_id( 45201 ), character_id( 45202 )
    };
    CHECK( shifted_selection.member_ids == shifted_expected );
    site.find_member( character_id( 45200 ) )->wounded_or_unready = false;

    const std::string before_rejections = serialize_world( world );
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     site, hostile_operation_kind::raid, route, rally, 602 ).valid );
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     site, hostile_operation_kind::shakedown,
                     { site.anchor, target }, rally, 602 ).valid );
    for( int member_id = 45203; member_id <= 45206; ++member_id ) {
        site.find_member( character_id( member_id ) )->wounded_or_unready = true;
    }
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     site, hostile_operation_kind::shakedown, route, rally, 602 ).valid );
    for( int member_id = 45203; member_id <= 45206; ++member_id ) {
        site.find_member( character_id( member_id ) )->wounded_or_unready = false;
    }
    CHECK( serialize_world( world ) == before_rejections );

    const bandit_live_world::hostile_operation_plan plan =
        bandit_live_world::plan_hostile_operation(
            site, hostile_operation_kind::shakedown, route, rally, 602 );
    REQUIRE( plan.valid );
    CHECK( plan.operation.operation_kind == hostile_operation_kind::shakedown );
    CHECK( plan.operation.phase == hostile_operation_phase::assembling );
    CHECK( plan.operation.source_report_revision == 9 );
    CHECK( plan.operation.source_report_generation == 4 );
    CHECK( plan.operation.source_report_activity_id ==
           site.current_scout_report.source_activity_id );
    CHECK( plan.operation.source_report_application_key ==
           site.current_scout_report.application_key );
    CHECK( plan.operation.has_rally );
    CHECK( plan.operation.rally_omt == rally );
    CHECK_FALSE( plan.operation.legacy_unpinned );
    CHECK( plan.operation.reservation.activity_id == site.site_id + "#hostile:5" );
    CHECK( plan.operation.reservation.generation == 5 );
    CHECK( plan.operation.reservation.kind ==
           bandit_live_world::outing_kind::hostile_operation );
    CHECK( plan.operation.reservation.member_ids == members );
    CHECK( plan.operation.reservation.shared_route == route );
    CHECK( plan.operation.reservation.target_id == "report-target" );
    CHECK( plan.operation.reservation.job_type == "toll" );

    site.find_member( members.front() )->wounded_or_unready = true;
    const std::string changed_roster = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
    CHECK( serialize_world( world ) == changed_roster );
    site.find_member( members.front() )->wounded_or_unready = false;

    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
    CHECK_FALSE( site.active_outing.is_active() );
    CHECK( site.active_hostile_operation.is_active() );
    CHECK( site.active_external_outing() == &site.active_hostile_operation.reservation );
    CHECK( site.next_outing_generation == 6 );
    for( const character_id &member_id : members ) {
        REQUIRE( site.find_member( member_id ) != nullptr );
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::at_home );
    }
    const std::string after_apply = serialize_world( world );
    CHECK( after_apply.find( "\"has_rally\": true" ) != std::string::npos );
    CHECK_FALSE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
    CHECK( serialize_world( world ) == after_apply );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.schema_version == 12 );
    CHECK( loaded_site.active_hostile_operation.is_active() );
    CHECK( loaded_site.active_hostile_operation.operation_kind ==
           hostile_operation_kind::shakedown );
    CHECK( loaded_site.active_hostile_operation.phase ==
           hostile_operation_phase::assembling );
    CHECK( loaded_site.active_hostile_operation.has_rally );
    CHECK( loaded_site.active_hostile_operation.rally_omt == rally );
    CHECK( loaded_site.active_hostile_operation.reservation.member_ids == members );
    CHECK( loaded_site.active_hostile_operation.reservation.shared_route == route );
    CHECK( loaded_site.active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::abstract );
    CHECK( loaded_site.active_external_outing() ==
           &loaded_site.active_hostile_operation.reservation );

    bandit_live_world::world_state malformed_party = world;
    malformed_party.sites.front().active_hostile_operation.reservation.member_ids.resize( 1 );
    malformed_party.sites.front().active_hostile_operation.reservation.leader_id =
        malformed_party.sites.front().active_hostile_operation.reservation.member_ids.front();
    const bandit_live_world::world_state repaired_party =
        round_trip_legacy_site_world( malformed_party );
    REQUIRE( repaired_party.sites.size() == 1 );
    CHECK_FALSE( repaired_party.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_party.sites.front().active_hostile_operation.reservation.member_ids.empty() );

    bandit_live_world::world_state broken_reserve = world;
    for( int member_id = 45203; member_id <= 45206; ++member_id ) {
        broken_reserve.sites.front().find_member( character_id( member_id ) )->wounded_or_unready = true;
    }
    const bandit_live_world::world_state repaired_reserve =
        round_trip_legacy_site_world( broken_reserve );
    REQUIRE( repaired_reserve.sites.size() == 1 );
    CHECK_FALSE( repaired_reserve.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_reserve.sites.front().active_hostile_operation.reservation.member_ids.empty() );

    bandit_live_world::world_state cannibal_world;
    for( int index = 0; index < 5; ++index ) {
        add_cannibal_camp_member( cannibal_world, index, 45300 );
    }
    bandit_live_world::site_record &cannibal_site = cannibal_world.sites.front();
    const tripoint_abs_omt cannibal_rally( 72, 80, 0 );
    const tripoint_abs_omt cannibal_target( 75, 80, 0 );
    prepare_hostile_follow_on( cannibal_site, 3, 2, "night-target",
                               cannibal_target, 700 );
    CHECK( cannibal_site.current_scout_report.action_policy ==
           bandit_live_world::camp_report_policy::cannibal_night_raid );
    CHECK( cannibal_site.camp_decision.report_policy ==
           bandit_live_world::camp_report_policy::cannibal_night_raid );
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     cannibal_site, hostile_operation_kind::shakedown,
                     { cannibal_site.anchor, cannibal_rally, cannibal_target },
                     cannibal_rally, 702 ).valid );
    const bandit_live_world::hostile_operation_plan raid_plan =
        bandit_live_world::plan_hostile_operation(
            cannibal_site, hostile_operation_kind::raid,
            { cannibal_site.anchor, cannibal_rally, cannibal_target },
            cannibal_rally, 702 );
    REQUIRE( raid_plan.valid );
    CHECK( raid_plan.operation.reservation.job_type == "raid" );
    bandit_live_world::site_record drifted_profile = cannibal_site;
    drifted_profile.profile = bandit_live_world::hostile_site_profile::camp_style;
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     drifted_profile, hostile_operation_kind::shakedown,
                     { drifted_profile.anchor, cannibal_rally, cannibal_target },
                     cannibal_rally, 702 ).valid );
}

TEST_CASE( "bandit_live_world_hostile_operation_phases_are_one_way_and_atomic",
           "[bandit][live_world][hostile_operation][phase][cas]" )
{
    using bandit_live_world::hostile_operation_phase;
    using bandit_live_world::hostile_operation_transition_result;

    const std::vector<hostile_operation_phase> phases = {
        hostile_operation_phase::assembling,
        hostile_operation_phase::outbound,
        hostile_operation_phase::rallying,
        hostile_operation_phase::waiting_night,
        hostile_operation_phase::approaching,
        hostile_operation_phase::committed_contact,
        hostile_operation_phase::returning_home,
        hostile_operation_phase::lost,
    };
    const std::vector<std::pair<hostile_operation_phase, hostile_operation_phase>> forward_edges = {
        { hostile_operation_phase::assembling, hostile_operation_phase::outbound },
        { hostile_operation_phase::outbound, hostile_operation_phase::rallying },
        { hostile_operation_phase::outbound, hostile_operation_phase::returning_home },
        { hostile_operation_phase::rallying, hostile_operation_phase::waiting_night },
        { hostile_operation_phase::rallying, hostile_operation_phase::approaching },
        { hostile_operation_phase::rallying, hostile_operation_phase::returning_home },
        { hostile_operation_phase::waiting_night, hostile_operation_phase::approaching },
        { hostile_operation_phase::waiting_night, hostile_operation_phase::returning_home },
        { hostile_operation_phase::approaching, hostile_operation_phase::committed_contact },
        { hostile_operation_phase::approaching, hostile_operation_phase::returning_home },
        { hostile_operation_phase::committed_contact, hostile_operation_phase::returning_home },
    };
    for( const hostile_operation_phase previous : phases ) {
        for( const hostile_operation_phase next : phases ) {
            const bool same = previous == next;
            const bool lost = next == hostile_operation_phase::lost &&
                              previous != hostile_operation_phase::lost;
            const bool forward = std::find( forward_edges.begin(), forward_edges.end(),
                                            std::make_pair( previous, next ) ) !=
                                 forward_edges.end();
            CHECK( bandit_live_world::is_valid_hostile_operation_phase_transition(
                       previous, next ) == ( same || lost || forward ) );
        }
    }

    for( const hostile_operation_phase phase : phases ) {
        bandit_live_world::hostile_operation_state saved;
        saved.operation_kind = bandit_live_world::hostile_operation_kind::shakedown;
        saved.phase = phase;
        saved.reservation.kind = bandit_live_world::outing_kind::hostile_operation;
        saved.reservation.activity_id = "phase-round-trip";
        saved.reservation.generation = 1;
        saved.reservation.return_application_key =
            bandit_pursuit_handoff::make_operation_component_key(
                saved.reservation.activity_id, saved.reservation.generation, "return" );
        saved.reservation.report_application_key =
            bandit_pursuit_handoff::make_operation_component_key(
                saved.reservation.activity_id, saved.reservation.generation, "report" );
        saved.reservation.cargo_application_key =
            bandit_pursuit_handoff::make_operation_component_key(
                saved.reservation.activity_id, saved.reservation.generation, "cargo" );
        std::ostringstream out;
        JsonOut jsout( out, true );
        saved.serialize( jsout );
        JsonValue jsin = json_loader::from_string( out.str() );
        bandit_live_world::hostile_operation_state loaded;
        loaded.deserialize( jsin.get_object() );
        CHECK( loaded.phase == phase );
    }

    bandit_live_world::world_state world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( world, index, 45400 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    const std::vector<character_id> members = {
        character_id( 45400 ), character_id( 45401 )
    };
    prepare_hostile_follow_on( site, 5, 4, "phase-target", target, 800 );
    const bandit_live_world::hostile_operation_plan plan =
        bandit_live_world::plan_hostile_operation(
            site, bandit_live_world::hostile_operation_kind::shakedown,
            { site.anchor, rally, target }, rally, 802 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );

    const std::string assembled = serialize_world( world );
    const bandit_live_world::simulation_advance_cursor assembled_cursor =
        require_current_simulation_cursor( site );
    bandit_live_world::simulation_advance_cursor wrong_identity = assembled_cursor;
    wrong_identity.activity_id += ":stale";
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, wrong_identity,
               hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               803, "wrong identity" ) == hostile_operation_transition_result::rejected );
    bandit_live_world::simulation_advance_cursor wrong_generation = assembled_cursor;
    wrong_generation.generation++;
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, wrong_generation,
               hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               803, "wrong generation" ) == hostile_operation_transition_result::rejected );
    for( int member_id = 45402; member_id <= 45404; ++member_id ) {
        site.find_member( character_id( member_id ) )->wounded_or_unready = true;
    }
    const std::string insufficient_reserve = serialize_world( world );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               803, "reserve vanished" ) == hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == insufficient_reserve );
    for( int member_id = 45402; member_id <= 45404; ++member_id ) {
        site.find_member( character_id( member_id ) )->wounded_or_unready = false;
    }
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::outbound, hostile_operation_phase::rallying,
               803, "wrong expected" ) == hostile_operation_transition_result::rejected );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               801, "stale time" ) == hostile_operation_transition_result::rejected );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::assembling, hostile_operation_phase::assembling,
               803, "same" ) == hostile_operation_transition_result::unchanged );
    CHECK( serialize_world( world ) == assembled );

    bandit_live_world::world_state lost_before_departure = world;
    bandit_live_world::site_record &lost_site = lost_before_departure.sites.front();
    REQUIRE( transition_test_hostile_operation(
                 lost_site, hostile_operation_phase::assembling,
                 hostile_operation_phase::lost, 803, "lost before departure" ) ==
             hostile_operation_transition_result::applied );
    const bandit_live_world::world_state loaded_lost = round_trip_world( lost_before_departure );
    REQUIRE( loaded_lost.sites.size() == 1 );
    REQUIRE( loaded_lost.sites.front().active_hostile_operation.is_active() );
    CHECK( loaded_lost.sites.front().active_hostile_operation.phase ==
           hostile_operation_phase::lost );
    CHECK( loaded_lost.sites.front().active_hostile_operation.reservation.resolved_member_ids ==
           members );
    for( const character_id &member_id : members ) {
        CHECK( loaded_lost.sites.front().find_member( member_id )->state ==
               bandit_live_world::member_state::at_home );
    }

    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::assembling, hostile_operation_phase::outbound,
                 803, "party departed" ) == hostile_operation_transition_result::applied );
    for( const character_id &member_id : members ) {
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::outbound );
    }
    CHECK( site.active_hostile_operation.reservation.last_progress_minutes == 803 );
    CHECK( site.active_hostile_operation.reservation.last_advanced_minutes == 803 );
    CHECK( site.active_hostile_operation.last_transition_reason == "party departed" );
    const std::string departed = serialize_world( world );
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, assembled_cursor, hostile_operation_phase::outbound,
               hostile_operation_phase::rallying, 810, "stale pre-departure cursor" ) ==
           hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == departed );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::outbound, hostile_operation_phase::rallying,
                 810, "rally reached" ) == hostile_operation_transition_result::applied );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::rallying, hostile_operation_phase::waiting_night,
                 820, "waiting for night" ) == hostile_operation_transition_result::applied );
    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().active_hostile_operation.phase ==
           hostile_operation_phase::waiting_night );
    CHECK( loaded.sites.front().active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::abstract );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::waiting_night, hostile_operation_phase::approaching,
                 830, "night approach" ) == hostile_operation_transition_result::applied );
    const bandit_live_world::simulation_advance_cursor pre_contact_cursor =
        require_current_simulation_cursor( site );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::approaching,
                 hostile_operation_phase::committed_contact, 840, "contact committed" ) ==
             hostile_operation_transition_result::applied );
    CHECK( site.active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::local );
    CHECK( site.active_hostile_operation.reservation.handoff_epoch == 1 );
    for( const character_id &member_id : members ) {
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::local_contact );
    }
    const std::string committed = serialize_world( world );
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, pre_contact_cursor, hostile_operation_phase::committed_contact,
               hostile_operation_phase::returning_home, 850, "stale pre-contact owner" ) ==
           hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == committed );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::committed_contact,
                 hostile_operation_phase::returning_home, 850, "withdraw" ) ==
             hostile_operation_transition_result::applied );
    CHECK( site.active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::abstract );
    CHECK( site.active_hostile_operation.reservation.handoff_epoch == 2 );
    for( const character_id &member_id : members ) {
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::outbound );
    }
    const std::string returning = serialize_world( world );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::returning_home,
               hostile_operation_phase::approaching, 860, "illegal reversal" ) ==
           hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == returning );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::returning_home,
                 hostile_operation_phase::lost, 860, "party lost" ) ==
             hostile_operation_transition_result::applied );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::lost, hostile_operation_phase::lost,
               870, "same terminal" ) == hostile_operation_transition_result::unchanged );

    JsonValue future_phase_json = json_loader::from_string(
        R"({"operation_kind":"shakedown","phase":"future_phase","reservation":{"kind":"hostile_operation","activity_id":"future-op","generation":1}})" );
    bandit_live_world::hostile_operation_state future_phase;
    future_phase.deserialize( future_phase_json.get_object() );
    CHECK( future_phase.phase == hostile_operation_phase::lost );
}

TEST_CASE( "bandit_live_world_round_trips_every_active_hostile_operation_phase",
           "[bandit][live_world][hostile_operation][phase][save][transition_event]" )
{
    using bandit_live_world::hostile_operation_phase;
    using bandit_live_world::hostile_operation_transition_result;
    using bandit_live_world::member_state;
    using bandit_live_world::simulation_owner;

    bandit_live_world::world_state world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( world, index, 45500 );
    }
    bandit_live_world::site_record &planning_site = world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    const std::vector<character_id> operation_members = {
        character_id( 45500 ), character_id( 45501 )
    };
    prepare_hostile_follow_on( planning_site, 8, 7, "all-phase-target", target, 900 );
    const bandit_live_world::hostile_operation_plan plan =
        bandit_live_world::plan_hostile_operation(
            planning_site, bandit_live_world::hostile_operation_kind::shakedown,
            { planning_site.anchor, rally, target }, rally, 902 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( planning_site, plan ) );

    const std::string operation_id =
        planning_site.active_hostile_operation.reservation.activity_id;
    const int operation_generation =
        planning_site.active_hostile_operation.reservation.generation;
    const std::string source_report_application_key =
        planning_site.active_hostile_operation.source_report_application_key;
    const std::string return_application_key =
        planning_site.active_hostile_operation.reservation.return_application_key;
    const std::string report_application_key =
        planning_site.active_hostile_operation.reservation.report_application_key;
    const std::string cargo_application_key =
        planning_site.active_hostile_operation.reservation.cargo_application_key;

    const auto require_canonical_round_trip = [&]( bandit_live_world::world_state &candidate,
    const hostile_operation_phase expected_phase,
    const simulation_owner expected_owner,
    const member_state expected_operation_member_state ) {
        REQUIRE( candidate.sites.size() == 1 );
        const bandit_live_world::site_record &before_site = candidate.sites.front();
        REQUIRE( before_site.active_hostile_operation.is_active() );
        const bandit_live_world::hostile_operation_state &before_operation =
            before_site.active_hostile_operation;
        REQUIRE( before_operation.phase == expected_phase );
        bandit_live_world::world_state loaded;
        bandit_live_world::world_state canonical_loaded;
        bandit_live_world_probe::snapshot event_snapshot;
        {
            bandit_live_world_probe::session event_session(
                bandit_live_world_probe::collection_mode::transition_events );
            loaded = round_trip_world( candidate );
            canonical_loaded = round_trip_world( loaded );
            event_snapshot = event_session.result();
        }
        CHECK( event_snapshot.transition_events.empty() );
        CHECK( event_snapshot.dropped_transition_events == 0 );
        CHECK( serialize_world( canonical_loaded ) == serialize_world( loaded ) );
        loaded = std::move( canonical_loaded );

        REQUIRE( loaded.sites.size() == 1 );
        const bandit_live_world::site_record &loaded_site = loaded.sites.front();
        REQUIRE( loaded_site.active_hostile_operation.is_active() );
        CHECK_FALSE( loaded_site.active_outing.is_active() );
        CHECK( loaded_site.active_external_outing() ==
               &loaded_site.active_hostile_operation.reservation );
        const bandit_live_world::hostile_operation_state &loaded_operation =
            loaded_site.active_hostile_operation;
        const bandit_live_world::active_outing_state &before_reservation =
            before_operation.reservation;
        const bandit_live_world::active_outing_state &loaded_reservation =
            loaded_operation.reservation;

        CHECK( loaded_operation.operation_kind == before_operation.operation_kind );
        CHECK( loaded_operation.phase == expected_phase );
        CHECK( loaded_operation.source_report_revision ==
               before_operation.source_report_revision );
        CHECK( loaded_operation.source_report_generation ==
               before_operation.source_report_generation );
        CHECK( loaded_operation.source_report_activity_id ==
               before_operation.source_report_activity_id );
        CHECK( loaded_operation.source_report_application_key ==
               source_report_application_key );
        CHECK( loaded_operation.source_report_application_key ==
               before_operation.source_report_application_key );
        CHECK( loaded_operation.has_rally == before_operation.has_rally );
        CHECK( loaded_operation.rally_omt == before_operation.rally_omt );
        CHECK( loaded_operation.last_transition_reason ==
               before_operation.last_transition_reason );
        CHECK( loaded_operation.legacy_unpinned == before_operation.legacy_unpinned );

        CHECK( loaded_reservation.activity_id == operation_id );
        CHECK( loaded_reservation.activity_id == before_reservation.activity_id );
        CHECK( loaded_reservation.generation == operation_generation );
        CHECK( loaded_reservation.generation == before_reservation.generation );
        CHECK( loaded_reservation.kind == before_reservation.kind );
        CHECK( loaded_reservation.camp_id == before_reservation.camp_id );
        CHECK( loaded_reservation.member_ids == operation_members );
        CHECK( loaded_reservation.member_ids == before_reservation.member_ids );
        CHECK( loaded_reservation.leader_id == before_reservation.leader_id );
        CHECK( loaded_reservation.shared_route == before_reservation.shared_route );
        CHECK( loaded_reservation.waypoint_index == before_reservation.waypoint_index );
        CHECK( loaded_reservation.target_id == before_reservation.target_id );
        CHECK( loaded_reservation.target_omt == before_reservation.target_omt );
        CHECK( loaded_reservation.job_type == before_reservation.job_type );
        CHECK( loaded_reservation.target_lead_id == before_reservation.target_lead_id );
        CHECK( loaded_reservation.target_lead_revision ==
               before_reservation.target_lead_revision );
        CHECK( loaded_reservation.casualty_ids == before_reservation.casualty_ids );
        CHECK( loaded_reservation.resolved_member_ids ==
               before_reservation.resolved_member_ids );
        CHECK( loaded_reservation.owner == expected_owner );
        CHECK( loaded_reservation.owner == before_reservation.owner );
        CHECK( loaded_reservation.handoff_epoch == before_reservation.handoff_epoch );
        CHECK( loaded_reservation.last_advanced_minutes ==
               before_reservation.last_advanced_minutes );
        CHECK( loaded_reservation.return_application_key == return_application_key );
        CHECK( loaded_reservation.return_application_key ==
               before_reservation.return_application_key );
        CHECK( loaded_reservation.report_application_key == report_application_key );
        CHECK( loaded_reservation.report_application_key ==
               before_reservation.report_application_key );
        CHECK( loaded_reservation.cargo_application_key == cargo_application_key );
        CHECK( loaded_reservation.cargo_application_key ==
               before_reservation.cargo_application_key );
        CHECK( loaded_site.current_scout_report.application_key ==
               before_site.current_scout_report.application_key );
        CHECK( loaded_site.current_scout_report.source_activity_id ==
               before_site.current_scout_report.source_activity_id );
        CHECK( loaded_site.current_scout_report.source_generation ==
               before_site.current_scout_report.source_generation );

        REQUIRE( loaded_site.members.size() == before_site.members.size() );
        for( const bandit_live_world::member_record &before_member : before_site.members ) {
            const bandit_live_world::member_record *loaded_member =
                loaded_site.find_member( before_member.npc_id );
            REQUIRE( loaded_member != nullptr );
            CHECK( loaded_member->npc_template_id == before_member.npc_template_id );
            CHECK( loaded_member->home_spawn_tile == before_member.home_spawn_tile );
            CHECK( loaded_member->state == before_member.state );
            CHECK( loaded_member->wounded_or_unready == before_member.wounded_or_unready );
            CHECK( loaded_member->last_writeback_summary ==
                   before_member.last_writeback_summary );
            CHECK( std::count_if( loaded_site.members.begin(), loaded_site.members.end(),
            [&]( const bandit_live_world::member_record &candidate_member ) {
                return candidate_member.npc_id == before_member.npc_id;
            } ) == 1 );
        }
        for( const character_id &member_id : operation_members ) {
            CHECK( std::count( loaded_reservation.member_ids.begin(),
                               loaded_reservation.member_ids.end(), member_id ) == 1 );
            REQUIRE( loaded_site.find_member( member_id ) != nullptr );
            CHECK( loaded_site.find_member( member_id )->state ==
                   expected_operation_member_state );
        }

        candidate = std::move( loaded );
    };

    bandit_live_world::world_state lost_world = world;
    require_canonical_round_trip( world, hostile_operation_phase::assembling,
                                  simulation_owner::abstract, member_state::at_home );

    REQUIRE( transition_test_hostile_operation(
                 world.sites.front(), hostile_operation_phase::assembling,
                 hostile_operation_phase::outbound, 903, "all-phase departure" ) ==
             hostile_operation_transition_result::applied );
    require_canonical_round_trip( world, hostile_operation_phase::outbound,
                                  simulation_owner::abstract, member_state::outbound );
    REQUIRE( transition_test_hostile_operation(
                 world.sites.front(), hostile_operation_phase::outbound,
                 hostile_operation_phase::rallying, 910, "all-phase rally" ) ==
             hostile_operation_transition_result::applied );
    require_canonical_round_trip( world, hostile_operation_phase::rallying,
                                  simulation_owner::abstract, member_state::outbound );
    REQUIRE( transition_test_hostile_operation(
                 world.sites.front(), hostile_operation_phase::rallying,
                 hostile_operation_phase::waiting_night, 920, "all-phase wait" ) ==
             hostile_operation_transition_result::applied );
    require_canonical_round_trip( world, hostile_operation_phase::waiting_night,
                                  simulation_owner::abstract, member_state::outbound );
    REQUIRE( transition_test_hostile_operation(
                 world.sites.front(), hostile_operation_phase::waiting_night,
                 hostile_operation_phase::approaching, 930, "all-phase approach" ) ==
             hostile_operation_transition_result::applied );
    require_canonical_round_trip( world, hostile_operation_phase::approaching,
                                  simulation_owner::abstract, member_state::outbound );
    REQUIRE( transition_test_hostile_operation(
                 world.sites.front(), hostile_operation_phase::approaching,
                 hostile_operation_phase::committed_contact, 940, "all-phase contact" ) ==
             hostile_operation_transition_result::applied );
    require_canonical_round_trip( world, hostile_operation_phase::committed_contact,
                                  simulation_owner::local, member_state::local_contact );
    REQUIRE( transition_test_hostile_operation(
                 world.sites.front(), hostile_operation_phase::committed_contact,
                 hostile_operation_phase::returning_home, 950, "all-phase withdrawal" ) ==
             hostile_operation_transition_result::applied );
    require_canonical_round_trip( world, hostile_operation_phase::returning_home,
                                  simulation_owner::abstract, member_state::outbound );

    REQUIRE( transition_test_hostile_operation(
                 lost_world.sites.front(), hostile_operation_phase::assembling,
                 hostile_operation_phase::lost, 903, "all-phase lost copy" ) ==
             hostile_operation_transition_result::applied );
    REQUIRE( lost_world.sites.front().active_hostile_operation.reservation.resolved_member_ids ==
             operation_members );
    require_canonical_round_trip( lost_world, hostile_operation_phase::lost,
                                  simulation_owner::abstract, member_state::at_home );
}

TEST_CASE( "bandit_live_world_transition_events_report_only_committed_scout_changes",
           "[bandit][live_world][transition_event][scout]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45500 );
    add_bandit_camp_member( world, 1, 45500 );
    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "transition-event-target" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    site.active_outing.phase = bandit_live_world::scout_phase::assembling;
    const std::string activity_id = site.active_outing.activity_id;
    const int generation = site.active_outing.generation;

    bandit_live_world_probe::snapshot departure_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        bandit_live_world::simulation_advance_cursor wrong_cursor =
            require_current_simulation_cursor( site );
        wrong_cursor.activity_id += ":stale";
        CHECK_FALSE( bandit_live_world::note_active_sortie_started( site, wrong_cursor, 100 ) );
        REQUIRE( event_session.result().transition_events.empty() );

        REQUIRE( bandit_live_world::note_active_sortie_started(
                     site, require_current_simulation_cursor( site ), 100 ) );
        CHECK( bandit_live_world::transition_active_scout_phase(
                   site, require_current_simulation_cursor( site ),
                   bandit_live_world::scout_phase::outbound,
                   bandit_live_world::scout_phase::outbound, 101 ) ==
               bandit_live_world::scout_phase_transition_result::unchanged );
        CHECK( bandit_live_world::transition_active_scout_phase(
                   site, require_current_simulation_cursor( site ),
                   bandit_live_world::scout_phase::outbound,
                   bandit_live_world::scout_phase::searching, 101, "" ) ==
               bandit_live_world::scout_phase_transition_result::rejected );
        departure_snapshot = event_session.result();
    }
    REQUIRE( departure_snapshot.transition_events.size() == 1 );
    const bandit_live_world_probe::transition_event &departure =
        departure_snapshot.transition_events.front();
    CHECK( departure.operation_id == activity_id );
    CHECK( departure.generation == generation );
    CHECK( departure.simulation_owner == "abstract" );
    CHECK( departure.previous_phase == "assembling" );
    CHECK( departure.new_phase == "outbound" );
    CHECK( departure.reason == "sortie departed" );
    CHECK( departure.at_minutes == 100 );
    CHECK( departure_snapshot.dropped_transition_events == 0 );

    bandit_live_world_probe::snapshot contact_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        REQUIRE( bandit_live_world::note_active_sortie_local_contact(
                     site, require_current_simulation_cursor( site ),
                     site.active_outing.member_ids.front(), 130 ) );
        contact_snapshot = event_session.result();
    }
    REQUIRE( contact_snapshot.transition_events.size() == 1 );
    const bandit_live_world_probe::transition_event &contact =
        contact_snapshot.transition_events.front();
    CHECK( contact.operation_id == activity_id );
    CHECK( contact.generation == generation );
    CHECK( contact.simulation_owner == "local" );
    CHECK( contact.previous_phase == "outbound" );
    CHECK( contact.new_phase == "observing" );
    CHECK( contact.reason == "first local contact" );
    CHECK( contact.at_minutes == 130 );

    const std::string before_round_trip = serialize_world( world );
    bandit_live_world::world_state loaded;
    bandit_live_world_probe::snapshot round_trip_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        loaded = round_trip_world( world );
        round_trip_snapshot = event_session.result();
    }
    CHECK( round_trip_snapshot.transition_events.empty() );
    CHECK( round_trip_snapshot.dropped_transition_events == 0 );
    CHECK( serialize_world( world ) == before_round_trip );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().active_outing.phase ==
           bandit_live_world::scout_phase::observing );
    CHECK( loaded.sites.front().active_outing.owner ==
           bandit_live_world::simulation_owner::local );

    bandit_live_world_probe::snapshot first_casualty_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        REQUIRE( bandit_live_world::record_active_outing_casualty(
                     site, require_current_simulation_cursor( site ),
                     site.active_outing.member_ids.front(),
                     bandit_live_world::member_state::dead, 150,
                     "lead scout confirmed dead" ) );
        first_casualty_snapshot = event_session.result();
    }
    CHECK( first_casualty_snapshot.transition_events.empty() );

    bandit_live_world_probe::snapshot casualty_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        REQUIRE( bandit_live_world::record_active_outing_casualty(
                     site, require_current_simulation_cursor( site ),
                     site.active_outing.member_ids.back(),
                     bandit_live_world::member_state::dead, 151,
                     "escort confirmed dead" ) );
        casualty_snapshot = event_session.result();
    }
    REQUIRE( casualty_snapshot.transition_events.size() == 1 );
    const bandit_live_world_probe::transition_event &casualty =
        casualty_snapshot.transition_events.front();
    CHECK( casualty.operation_id == activity_id );
    CHECK( casualty.generation == generation );
    CHECK( casualty.simulation_owner == "local" );
    CHECK( casualty.previous_phase == "observing" );
    CHECK( casualty.new_phase == "lost" );
    CHECK( casualty.reason == "all scout members resolved as casualties" );
    CHECK( casualty.at_minutes == 151 );
}

TEST_CASE( "bandit_live_world_transition_events_capture_final_hostile_owner",
           "[bandit][live_world][transition_event][hostile_operation]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( world, index, 45520 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    prepare_hostile_follow_on( site, 6, 5, "event-hostile-target", target, 700 );
    const bandit_live_world::hostile_operation_plan plan =
        bandit_live_world::plan_hostile_operation(
            site, bandit_live_world::hostile_operation_kind::shakedown,
            { site.anchor, rally, target }, rally, 702 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
    REQUIRE( transition_test_hostile_operation(
                 site, bandit_live_world::hostile_operation_phase::assembling,
                 bandit_live_world::hostile_operation_phase::outbound,
                 703, "party departed" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
    REQUIRE( transition_test_hostile_operation(
                 site, bandit_live_world::hostile_operation_phase::outbound,
                 bandit_live_world::hostile_operation_phase::rallying,
                 710, "rally reached" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
    REQUIRE( transition_test_hostile_operation(
                 site, bandit_live_world::hostile_operation_phase::rallying,
                 bandit_live_world::hostile_operation_phase::approaching,
                 720, "approach begun" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
    const std::string activity_id = site.active_hostile_operation.reservation.activity_id;
    const int generation = site.active_hostile_operation.reservation.generation;

    bandit_live_world_probe::snapshot event_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        REQUIRE( transition_test_hostile_operation(
                     site, bandit_live_world::hostile_operation_phase::approaching,
                     bandit_live_world::hostile_operation_phase::committed_contact,
                     730, "contact committed" ) ==
                 bandit_live_world::hostile_operation_transition_result::applied );
        event_snapshot = event_session.result();
    }
    REQUIRE( event_snapshot.transition_events.size() == 1 );
    const bandit_live_world_probe::transition_event &event =
        event_snapshot.transition_events.front();
    CHECK( event.operation_id == activity_id );
    CHECK( event.generation == generation );
    CHECK( event.simulation_owner == "local" );
    CHECK( event.previous_phase == "approaching" );
    CHECK( event.new_phase == "committed_contact" );
    CHECK( event.reason == "contact committed" );
    CHECK( event.at_minutes == 730 );
}

TEST_CASE( "bandit_live_world_transition_events_are_bounded",
           "[bandit][live_world][transition_event][bounded]" )
{
    bandit_live_world_probe::snapshot event_snapshot;
    {
        bandit_live_world_probe::session event_session(
            bandit_live_world_probe::collection_mode::transition_events );
        const std::string long_reason( 300, 'r' );
        const std::string bounded_reason = "bounded transition";
        for( int index = 0; index < 65; ++index ) {
            bandit_live_world_probe::record_transition_event(
                "bounded-operation", index + 1, "abstract", "outbound", "searching",
                index == 0 ? long_reason : bounded_reason, 800 + index );
        }
        const std::string oversized_field(
            bandit_live_world_probe::max_transition_event_field_length + 1, 'x' );
        bandit_live_world_probe::record_transition_event(
            oversized_field, 66, "abstract", "outbound", "searching",
            bounded_reason, 866 );
        bandit_live_world_probe::record_transition_event(
            "bounded-operation", 67, oversized_field, "outbound", "searching",
            bounded_reason, 867 );
        bandit_live_world_probe::record_transition_event(
            "bounded-operation", 68, "abstract", oversized_field, "searching",
            bounded_reason, 868 );
        bandit_live_world_probe::record_transition_event(
            "bounded-operation", 69, "abstract", "outbound", oversized_field,
            bounded_reason, 869 );
        bandit_live_world_probe::record_transition_event(
            "bounded-operation", 70, "abstract", "outbound", "searching",
            bounded_reason, 870 );
        event_snapshot = event_session.result();
    }
    REQUIRE( event_snapshot.transition_events.size() ==
             bandit_live_world_probe::max_transition_events );
    CHECK( event_snapshot.dropped_transition_events == 6 );
    CHECK( event_snapshot.transition_events.front().generation == 2 );
    CHECK( event_snapshot.transition_events.front().reason == "bounded transition" );
    CHECK( event_snapshot.transition_events.back().generation == 65 );
}

TEST_CASE( "bandit_live_world_caps_scout_routes_observations_and_reservations_on_load",
           "[bandit][live_world][scout_state][save]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45100 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45100 ),
             bandit_live_world::member_state::outbound, "test scout active" ) );
    set_test_active_outing( site, site.site_id + "#scout:caps" );
    site.active_outing.member_ids = { character_id( 45100 ) };
    site.active_outing.leader_id = character_id( 45100 );
    site.active_outing.waypoint_index = 280;
    for( int index = 0; index < 300; ++index ) {
        site.active_outing.shared_route.emplace_back( index, 20, 0 );
    }
    for( int index = 0; index < 20; ++index ) {
        site.active_outing.observations.push_back( { "fact-" + std::to_string( index ),
                                                     "bounded observation", 50, index, false,
                                                     bandit_live_world::sortie_observation_kind::routine, "" } );
    }
    site.active_outing.observations.push_back( { "critical-after-cap", "burned withdrawal",
                                                 90, 21, true,
                                                 bandit_live_world::sortie_observation_kind::routine, "" } );
    site.active_outing.observations.back().kind =
        bandit_live_world::sortie_observation_kind::burn;
    site.active_outing.observations.back().state_key = "burned";

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    REQUIRE( loaded.sites.front().active_outing.shared_route.size() == 20 );
    CHECK( loaded.sites.front().active_outing.shared_route.front() == tripoint_abs_omt( 280, 20, 0 ) );
    CHECK( loaded.sites.front().active_outing.shared_route.back() == tripoint_abs_omt( 299, 20, 0 ) );
    CHECK( loaded.sites.front().active_outing.waypoint_index == 0 );
    CHECK( loaded.sites.front().active_outing.observations.size() == 16 );
    CHECK( std::any_of( loaded.sites.front().active_outing.observations.begin(),
                        loaded.sites.front().active_outing.observations.end(),
    []( const bandit_live_world::sortie_observation & observation ) {
        return observation.fact_key == "critical-after-cap" && observation.critical;
    } ) );
    const auto burned = std::find_if( loaded.sites.front().active_outing.observations.begin(),
                                      loaded.sites.front().active_outing.observations.end(),
    []( const bandit_live_world::sortie_observation & observation ) {
        return observation.fact_key == "critical-after-cap";
    } );
    REQUIRE( burned != loaded.sites.front().active_outing.observations.end() );
    CHECK( burned->kind == bandit_live_world::sortie_observation_kind::burn );
    CHECK( burned->state_key == "burned" );

    world.sites.front().active_outing.waypoint_index = 0;
    const bandit_live_world::world_state capped_from_start = round_trip_world( world );
    REQUIRE( capped_from_start.sites.front().active_outing.shared_route.size() == 256 );
    CHECK( capped_from_start.sites.front().active_outing.shared_route.front() ==
           tripoint_abs_omt( 0, 20, 0 ) );
    CHECK( capped_from_start.sites.front().active_outing.shared_route.back() ==
           tripoint_abs_omt( 299, 20, 0 ) );
    CHECK( capped_from_start.sites.front().active_outing.waypoint_index == 0 );

    bandit_live_world::world_state oversized;
    REQUIRE( bandit_live_world::register_abstract_site( oversized,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup, 17 ) );
    bandit_live_world::site_record &oversized_site = oversized.sites.front();
    set_test_active_outing( oversized_site, oversized_site.site_id + "#scout:oversized" );
    for( int index = 0; index < 17; ++index ) {
        const character_id member_id( 45200 + index );
        oversized_site.members.push_back( { member_id, "bandit", tripoint_abs_ms(),
                                            bandit_live_world::member_state::outbound, false, "" } );
        oversized_site.active_outing.member_ids.push_back( member_id );
    }
    oversized_site.active_outing.leader_id = oversized_site.active_outing.member_ids.front();
    const bandit_live_world::world_state closed = round_trip_legacy_site_world( oversized );
    REQUIRE( closed.sites.size() == 1 );
    CHECK_FALSE( closed.sites.front().active_outing.is_active() );
    CHECK( closed.sites.front().active_outing.member_ids.empty() );
    CHECK( closed.sites.front().count_members_in_state( bandit_live_world::member_state::outbound ) == 0 );
}

TEST_CASE( "bandit_live_world_compacts_scout_facts_and_only_semantic_change_progresses",
           "[bandit][live_world][scout_state][observation]" )
{
    using bandit_live_world::sortie_observation;
    using bandit_live_world::sortie_observation_kind;

    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45200 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45200 ),
             bandit_live_world::member_state::outbound, "test scout active" ) );
    set_test_active_outing( site, site.site_id + "#scout:observation" );
    site.active_outing.member_ids = { character_id( 45200 ) };
    site.active_outing.leader_id = character_id( 45200 );
    site.active_outing.phase = bandit_live_world::scout_phase::observing;
    site.active_outing.started_minutes = 10;
    site.active_outing.last_progress_minutes = 100;
    site.active_outing.last_advanced_minutes = 100;
    for( int index = 0; index < 16; ++index ) {
        sortie_observation routine;
        routine.fact_key = "routine-" + std::to_string( index );
        routine.state_key = "unchanged";
        routine.summary = "routine poll";
        routine.confidence = 10;
        routine.observed_minutes = 20 + index;
        site.active_outing.observations.push_back( routine );
    }

    const std::vector<std::pair<sortie_observation_kind, std::string>> protected_facts = {
        { sortie_observation_kind::burn, "burn" },
        { sortie_observation_kind::casualty, "casualty" },
        { sortie_observation_kind::contradiction, "contradiction" },
        { sortie_observation_kind::hard_danger, "hard-danger" },
        { sortie_observation_kind::target_revision, "target-revision" },
    };
    std::vector<sortie_observation> protected_inputs;
    for( const auto &entry : protected_facts ) {
        sortie_observation observation;
        observation.fact_key = entry.second;
        observation.state_key = "observed";
        observation.summary = entry.second + " fact";
        observation.confidence = 80;
        observation.observed_minutes = 110;
        observation.kind = entry.first;
        protected_inputs.push_back( observation );
    }

    const bandit_live_world::simulation_advance_cursor stale_cursor =
        require_current_simulation_cursor( site );
    const bandit_live_world::sortie_observation_effect protected_effect =
        bandit_live_world::record_active_sortie_observations( site, stale_cursor,
                protected_inputs, 110 );
    REQUIRE( protected_effect.valid );
    CHECK( protected_effect.changed );
    CHECK( protected_effect.progress );
    CHECK( protected_effect.inserted == 5 );
    CHECK( protected_effect.evicted == 5 );
    CHECK( site.active_outing.observations.size() == 16 );
    CHECK( site.active_outing.last_progress_minutes == 110 );
    CHECK( site.active_outing.last_advanced_minutes == 110 );
    for( const auto &entry : protected_facts ) {
        CHECK( std::any_of( site.active_outing.observations.begin(),
                            site.active_outing.observations.end(),
        [&entry]( const sortie_observation & observation ) {
            return observation.fact_key == entry.second && observation.kind == entry.first;
        } ) );
    }

    const std::string after_protected = serialize_world( world );
    CHECK_FALSE( bandit_live_world::record_active_sortie_observations(
                     site, stale_cursor, protected_inputs, 120 ).valid );
    CHECK( serialize_world( world ) == after_protected );

    sortie_observation stronger_duplicate = protected_inputs.front();
    stronger_duplicate.confidence = 95;
    stronger_duplicate.summary = "stronger duplicate burn";
    stronger_duplicate.observed_minutes = 120;
    const bandit_live_world::sortie_observation_effect duplicate_effect =
        bandit_live_world::record_active_sortie_observations(
            site, require_current_simulation_cursor( site ), { stronger_duplicate }, 120 );
    REQUIRE( duplicate_effect.valid );
    CHECK( duplicate_effect.replaced == 1 );
    CHECK_FALSE( duplicate_effect.progress );
    CHECK( site.active_outing.last_progress_minutes == 110 );
    const auto retained_burn = std::find_if( site.active_outing.observations.begin(),
                                             site.active_outing.observations.end(),
    []( const sortie_observation & observation ) {
        return observation.fact_key == "burn";
    } );
    REQUIRE( retained_burn != site.active_outing.observations.end() );
    CHECK( retained_burn->observed_minutes == 110 );
    CHECK( retained_burn->confidence == 95 );

    sortie_observation poll;
    poll.fact_key = "routine-new";
    poll.state_key = "unchanged";
    poll.summary = "new polling sample";
    poll.confidence = 20;
    poll.observed_minutes = 130;
    const bandit_live_world::sortie_observation_effect poll_effect =
        bandit_live_world::record_active_sortie_observations(
            site, require_current_simulation_cursor( site ), { poll }, 130 );
    REQUIRE( poll_effect.valid );
    CHECK_FALSE( poll_effect.progress );
    CHECK( site.active_outing.last_progress_minutes == 110 );
    CHECK( site.active_outing.last_advanced_minutes == 130 );

    sortie_observation certainty;
    certainty.fact_key = "defender-certainty";
    certainty.state_key = "confirmed";
    certainty.summary = "presence confirmed";
    certainty.confidence = 60;
    certainty.observed_minutes = 140;
    certainty.kind = sortie_observation_kind::certainty;
    REQUIRE( bandit_live_world::record_active_sortie_observations(
                 site, require_current_simulation_cursor( site ), { certainty }, 140 ).progress );
    CHECK( site.active_outing.last_progress_minutes == 140 );

    certainty.confidence = 80;
    certainty.summary = "stronger confirmation";
    certainty.observed_minutes = 150;
    const bandit_live_world::sortie_observation_effect strength_effect =
        bandit_live_world::record_active_sortie_observations(
            site, require_current_simulation_cursor( site ), { certainty }, 150 );
    REQUIRE( strength_effect.valid );
    CHECK_FALSE( strength_effect.progress );
    CHECK( site.active_outing.last_progress_minutes == 140 );

    certainty.state_key = "uncertain";
    certainty.summary = "legitimate contradictory visibility changed certainty";
    certainty.observed_minutes = 160;
    const bandit_live_world::sortie_observation_effect state_change_effect =
        bandit_live_world::record_active_sortie_observations(
            site, require_current_simulation_cursor( site ), { certainty }, 160 );
    REQUIRE( state_change_effect.valid );
    CHECK( state_change_effect.progress );
    CHECK( state_change_effect.replaced == 1 );
    CHECK( site.active_outing.last_progress_minutes == 160 );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    CHECK( serialize_world( loaded ) == serialize_world( world ) );
    const auto loaded_certainty = std::find_if(
                                      loaded.sites.front().active_outing.observations.begin(),
                                      loaded.sites.front().active_outing.observations.end(),
    []( const sortie_observation & observation ) {
        return observation.fact_key == "defender-certainty";
    } );
    REQUIRE( loaded_certainty != loaded.sites.front().active_outing.observations.end() );
    CHECK( loaded_certainty->kind == sortie_observation_kind::certainty );
    CHECK( loaded_certainty->state_key == "uncertain" );
}

TEST_CASE( "bandit_live_world_round_trips_typed_physical_observations_for_both_camps",
           "[bandit][live_world][typed_observation][save]" )
{
    for( const bool cannibal : { false, true } ) {
        bandit_live_world::world_state world;
        const int id_base = cannibal ? 45220 : 45210;
        if( cannibal ) {
            add_cannibal_camp_member( world, 0, id_base );
            add_cannibal_camp_member( world, 1, id_base );
        } else {
            add_bandit_camp_member( world, 0, id_base );
            add_bandit_camp_member( world, 1, id_base );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const character_id observer_id( id_base );
        REQUIRE( bandit_live_world::update_member_state(
                     site, observer_id, bandit_live_world::member_state::outbound,
                     "typed observation fixture" ) );
        set_test_active_outing( site, site.site_id + "#scout:typed-round-trip" );
        site.active_outing.member_ids = { observer_id };
        site.active_outing.leader_id = observer_id;
        site.active_outing.target_id = "typed-target";
        site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
        site.active_outing.target_lead_revision = 7;
        site.active_outing.started_minutes = 90;
        site.active_outing.last_progress_minutes = 100;
        site.active_outing.last_advanced_minutes = 100;
        const bandit_live_world::sortie_observation observation =
            make_typed_visual_observation(
                observer_id, 7, 121, cannibal ? "cannibal-visual" : "bandit-visual",
                bandit_live_world::sortie_observation_share_state::shared );

        const bandit_live_world::sortie_observation_effect effect =
            bandit_live_world::record_active_typed_observations(
                site, require_current_simulation_cursor( site ), observer_id, 7,
                { observation }, 125 );
        REQUIRE( effect.valid );
        CHECK( effect.progress );
        REQUIRE( site.active_outing.observations.size() == 1 );

        const bandit_live_world::world_state loaded = round_trip_world( world );
        REQUIRE( loaded.sites.size() == 1 );
        REQUIRE( loaded.sites.front().active_outing.observations.size() == 1 );
        const bandit_live_world::sortie_observation &loaded_observation =
            loaded.sites.front().active_outing.observations.front();
        CHECK( serialize_sortie_observation( loaded_observation ) ==
               serialize_sortie_observation( observation ) );
        CHECK( loaded_observation.record_schema_version == 1 );
        CHECK( loaded_observation.observer_id == observer_id );
        CHECK( loaded_observation.defender_ids ==
               std::vector<std::string> { "defender:1", "defender:2" } );
        CHECK( loaded_observation.share_state ==
               bandit_live_world::sortie_observation_share_state::shared );
    }
}

TEST_CASE( "bandit_live_world_rejects_malformed_typed_observations_atomically",
           "[bandit][live_world][typed_observation][transaction]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45230 );
    add_bandit_camp_member( world, 1, 45230 );
    bandit_live_world::site_record &site = world.sites.front();
    const character_id observer_id( 45230 );
    REQUIRE( bandit_live_world::update_member_state(
                 site, observer_id, bandit_live_world::member_state::outbound,
                 "typed observation fixture" ) );
    set_test_active_outing( site, site.site_id + "#scout:typed-invalid" );
    site.active_outing.member_ids = { observer_id };
    site.active_outing.leader_id = observer_id;
    site.active_outing.target_lead_revision = 7;
    site.active_outing.last_advanced_minutes = 100;
    const bandit_live_world::sortie_observation valid =
        make_typed_visual_observation(
            observer_id, 7, 121, "invalid-candidate",
            bandit_live_world::sortie_observation_share_state::observer_private );

    const auto rejects = [&world, observer_id](
    const bandit_live_world::sortie_observation & observation,
    const character_id supplied_observer = character_id( 45230 ),
    const int expected_revision = 7 ) {
        bandit_live_world::world_state attempt = world;
        bandit_live_world::site_record &attempt_site = attempt.sites.front();
        const std::string before = serialize_world( attempt );
        const bandit_live_world::sortie_observation_effect effect =
            bandit_live_world::record_active_typed_observations(
                attempt_site, require_current_simulation_cursor( attempt_site ),
                supplied_observer, expected_revision, { observation }, 130 );
        CHECK_FALSE( effect.valid );
        CHECK_FALSE( effect.changed );
        CHECK( serialize_world( attempt ) == before );
        CHECK( attempt_site.active_outing.observations.empty() );
        ( void )observer_id;
    };

    bandit_live_world::sortie_observation malformed = valid;
    malformed.bucket_start_minutes++;
    rejects( malformed );
    malformed = valid;
    malformed.simultaneity_start_minutes = malformed.observed_minutes + 1;
    rejects( malformed );
    malformed = valid;
    malformed.expiry_minutes = malformed.observed_minutes - 1;
    rejects( malformed );
    malformed = valid;
    malformed.strength = 7;
    rejects( malformed );
    malformed = valid;
    malformed.sense = bandit_live_world::sortie_observation_sense::sound;
    rejects( malformed );
    malformed = valid;
    malformed.observed_power_high = 201;
    rejects( malformed );
    malformed = valid;
    malformed.defender_ids = { "defender:2", "defender:1" };
    rejects( malformed );
    malformed = valid;
    malformed.defender_ids.assign( 17, "defender" );
    rejects( malformed );
    malformed = valid;
    malformed.defender_ids = { "defender:1", "defender:1" };
    rejects( malformed );
    malformed = valid;
    malformed.source_omt = tripoint_abs_omt::invalid;
    rejects( malformed );
    malformed = valid;
    malformed.share_state = bandit_live_world::sortie_observation_share_state::reported;
    rejects( malformed );
    rejects( valid, character_id( 49999 ) );
    rejects( valid, observer_id, 6 );

    bandit_live_world::simulation_advance_cursor stale_cursor =
        require_current_simulation_cursor( site );
    stale_cursor.generation++;
    const std::string before_stale = serialize_world( world );
    CHECK_FALSE( bandit_live_world::record_active_typed_observations(
                     site, stale_cursor, observer_id, 7, { valid }, 130 ).valid );
    CHECK( serialize_world( world ) == before_stale );

    bandit_live_world::world_state structural_world = world;
    bandit_live_world::site_record &structural_site = structural_world.sites.front();
    structural_site.active_outing.kind = bandit_live_world::outing_kind::structural_sortie;
    structural_site.active_outing.job_type = "scavenge";
    CHECK( bandit_live_world::record_active_typed_observations(
               structural_site, require_current_simulation_cursor( structural_site ),
               observer_id, 7, { valid }, 130 ).valid );

    bandit_live_world::world_state hostile_world = world;
    bandit_live_world::site_record &hostile_site = hostile_world.sites.front();
    hostile_site.active_outing.kind = bandit_live_world::outing_kind::hostile_operation;
    const std::string before_hostile = serialize_world( hostile_world );
    CHECK_FALSE( bandit_live_world::record_active_typed_observations(
                     hostile_site, require_current_simulation_cursor( hostile_site ),
                     observer_id, 7, { valid }, 130 ).valid );
    CHECK( serialize_world( hostile_world ) == before_hostile );

    bandit_live_world::world_state extreme_world = world;
    bandit_live_world::site_record &extreme_site = extreme_world.sites.front();
    bandit_live_world::sortie_observation extreme = valid;
    extreme.observed_minutes = std::numeric_limits<int>::max();
    extreme.bucket_start_minutes = extreme.observed_minutes - extreme.observed_minutes % 30;
    extreme.simultaneity_start_minutes = extreme.observed_minutes;
    extreme.simultaneity_end_minutes = extreme.observed_minutes;
    extreme.expiry_minutes = extreme.observed_minutes;
    CHECK( bandit_live_world::record_active_typed_observations(
               extreme_site, require_current_simulation_cursor( extreme_site ),
               observer_id, 7, { extreme }, std::numeric_limits<int>::max() ).valid );
}

TEST_CASE( "bandit_live_world_deduplicates_and_caps_typed_observation_buckets",
           "[bandit][live_world][typed_observation][retention]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45240 );
    bandit_live_world::site_record &site = world.sites.front();
    const character_id observer_id( 45240 );
    REQUIRE( bandit_live_world::update_member_state(
                 site, observer_id, bandit_live_world::member_state::outbound,
                 "typed observation fixture" ) );
    set_test_active_outing( site, site.site_id + "#scout:typed-retention" );
    site.active_outing.member_ids = { observer_id };
    site.active_outing.leader_id = observer_id;
    site.active_outing.target_lead_revision = 3;
    site.active_outing.last_advanced_minutes = 100;

    bandit_live_world::sortie_observation private_sample =
        make_typed_visual_observation(
            observer_id, 3, 121, "recurring-defenders",
            bandit_live_world::sortie_observation_share_state::observer_private );
    bandit_live_world::sortie_observation shared_sample = private_sample;
    shared_sample.observed_minutes = 122;
    shared_sample.simultaneity_start_minutes = 121;
    shared_sample.simultaneity_end_minutes = 123;
    shared_sample.share_state = bandit_live_world::sortie_observation_share_state::shared;
    const bandit_live_world::sortie_observation later_bucket =
        make_typed_visual_observation(
            observer_id, 3, 151, "recurring-defenders",
            bandit_live_world::sortie_observation_share_state::shared );
    REQUIRE( bandit_live_world::record_active_typed_observations(
                 site, require_current_simulation_cursor( site ), observer_id, 3,
                 { private_sample, shared_sample, later_bucket }, 160 ).valid );
    REQUIRE( site.active_outing.observations.size() == 2 );
    CHECK( site.active_outing.observations.front().bucket_start_minutes == 120 );
    CHECK( site.active_outing.observations.front().share_state ==
           bandit_live_world::sortie_observation_share_state::shared );
    CHECK( site.active_outing.observations.back().bucket_start_minutes == 150 );

    const std::size_t before_replay = site.active_outing.observations.size();
    const bandit_live_world::sortie_observation_effect replay =
        bandit_live_world::record_active_typed_observations(
            site, require_current_simulation_cursor( site ), observer_id, 3,
            { shared_sample }, 170 );
    REQUIRE( replay.valid );
    CHECK_FALSE( replay.progress );
    CHECK( site.active_outing.observations.size() == before_replay );

    std::vector<bandit_live_world::sortie_observation> additional;
    for( int index = 0; index < 16; ++index ) {
        additional.push_back( make_typed_visual_observation(
                                  observer_id, 3, 181 + index,
                                  "bounded-typed-" + std::to_string( index ),
                                  bandit_live_world::sortie_observation_share_state::shared ) );
    }
    REQUIRE( bandit_live_world::record_active_typed_observations(
                 site, require_current_simulation_cursor( site ), observer_id, 3,
                 additional, 210 ).valid );
    CHECK( site.active_outing.observations.size() == 16 );
}

TEST_CASE( "bandit_live_world_migrates_unversioned_observations_and_rejects_partial_typed_records",
           "[bandit][live_world][typed_observation][migration]" )
{
    JsonValue legacy_json = json_loader::from_string(
                                R"({"fact_key":"legacy","summary":"old fact","confidence":40,"observed_minutes":10,"critical":false})" );
    bandit_live_world::sortie_observation legacy;
    legacy.deserialize( legacy_json.get_object() );
    CHECK( legacy.record_schema_version == 0 );
    CHECK( legacy.fact_key == "legacy" );
    CHECK( serialize_sortie_observation( legacy ).find( "\"schema_version\": 0" ) !=
           std::string::npos );

    const bandit_live_world::sortie_observation typed = make_typed_visual_observation(
                character_id( 45250 ), 5, 121, "typed-persistence",
                bandit_live_world::sortie_observation_share_state::shared );
    std::string partial = serialize_sortie_observation( typed );
    erase_pretty_json_member_line( partial, "source_id" );
    CHECK_THROWS( [&partial]() {
        JsonValue json = json_loader::from_string( partial );
        bandit_live_world::sortie_observation observation;
        observation.deserialize( json.get_object() );
    }() );

    std::string unknown = serialize_sortie_observation( typed );
    const std::string supported = "\"schema_version\": 1";
    REQUIRE( unknown.find( supported ) != std::string::npos );
    unknown.replace( unknown.find( supported ), supported.size(), "\"schema_version\": 2" );
    CHECK_THROWS( [&unknown]() {
        JsonValue json = json_loader::from_string( unknown );
        bandit_live_world::sortie_observation observation;
        observation.deserialize( json.get_object() );
    }() );
}

TEST_CASE( "bandit_live_world_private_observer_evidence_dies_without_reaching_the_report",
           "[bandit][live_world][typed_observation][physical_report]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45260 );
    add_bandit_camp_member( world, 1, 45260 );
    bandit_live_world::site_record &site = world.sites.front();
    const character_id doomed_observer( 45260 );
    const character_id surviving_partner( 45261 );
    REQUIRE( bandit_live_world::update_member_state(
                 site, doomed_observer, bandit_live_world::member_state::local_contact,
                 "paired observers active" ) );
    REQUIRE( bandit_live_world::update_member_state(
                 site, surviving_partner, bandit_live_world::member_state::local_contact,
                 "paired observers active" ) );
    set_test_active_outing( site, site.site_id + "#scout:typed-report" );
    site.active_outing.member_ids = { doomed_observer, surviving_partner };
    site.active_outing.leader_id = doomed_observer;
    site.active_outing.target_id = "typed-report-target";
    site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
    site.active_outing.target_lead_revision = 9;
    site.active_outing.started_minutes = 90;
    site.active_outing.last_progress_minutes = 100;
    site.active_outing.owner = bandit_live_world::simulation_owner::local;
    site.active_outing.handoff_epoch = 1;
    site.active_outing.last_advanced_minutes = 100;
    const bandit_live_world::sortie_observation private_observation =
        make_typed_visual_observation(
            doomed_observer, 9, 105, "private-before-death",
            bandit_live_world::sortie_observation_share_state::observer_private );
    REQUIRE( bandit_live_world::record_active_typed_observations(
                 site, require_current_simulation_cursor( site ), doomed_observer, 9,
                 { private_observation }, 110 ).valid );
    const bandit_live_world::sortie_observation shared_observation =
        make_typed_visual_observation(
            surviving_partner, 9, 115, "shared-by-survivor",
            bandit_live_world::sortie_observation_share_state::shared );
    REQUIRE( bandit_live_world::record_active_typed_observations(
                 site, require_current_simulation_cursor( site ), surviving_partner, 9,
                 { shared_observation }, 120 ).valid );

    const std::vector<bandit_live_world::active_member_observation> aftermath = {
        { doomed_observer, bandit_live_world::active_member_observation_state::dead,
          "observer died before sharing" },
        { surviving_partner, bandit_live_world::active_member_observation_state::home,
          "partner returned with shared evidence" }
    };
    const bandit_live_world::scout_resolution_effect resolved =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), aftermath, 200 );
    REQUIRE( resolved.valid );
    REQUIRE( resolved.completed );
    REQUIRE( site.current_scout_report.is_present() );
    REQUIRE( site.current_scout_report.observations.size() == 1 );
    CHECK( site.current_scout_report.observations.front().fact_key ==
           "shared-by-survivor" );
    CHECK( site.current_scout_report.observations.front().share_state ==
           bandit_live_world::sortie_observation_share_state::reported );
    CHECK( std::none_of( site.current_scout_report.observations.begin(),
                        site.current_scout_report.observations.end(),
    []( const bandit_live_world::sortie_observation & observation ) {
        return observation.fact_key == "private-before-death";
    } ) );
}

TEST_CASE( "bandit_live_world_persists_partial_scout_casualties_without_closing_the_partner",
           "[bandit][live_world][scout_state][save]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45300 );
    add_bandit_camp_member( world, 1, 45300 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45300 ),
             bandit_live_world::member_state::local_contact, "scout pair active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45301 ),
             bandit_live_world::member_state::local_contact, "scout pair active" ) );
    set_test_active_outing( site, site.site_id + "#scout:partial-casualty" );
    site.active_outing.member_ids = { character_id( 45300 ), character_id( 45301 ) };
    site.active_outing.leader_id = character_id( 45300 );
    site.active_outing.job_type = "scout";
    REQUIRE( bandit_live_world::note_active_sortie_started(
                 site, require_current_simulation_cursor( site ), 100 ) );
    REQUIRE( bandit_live_world::note_active_sortie_local_contact(
                 site, require_current_simulation_cursor( site ), character_id( 45300 ), 100 ) );
    CHECK( site.active_outing.expected_return_minutes == 940 );
    CHECK( site.active_outing.missing_deadline_minutes == 2380 );

    REQUIRE( bandit_live_world::record_active_outing_casualty(
                 site, require_current_simulation_cursor( site ), character_id( 45301 ),
                 bandit_live_world::member_state::dead, 400, "observer killed" ) );
    CHECK( site.active_outing.is_active() );
    CHECK( site.active_outing.casualty_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    CHECK( site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    bandit_pursuit_handoff::return_packet stale_pre_casualty_packet;
    stale_pre_casualty_packet.valid = true;
    stale_pre_casualty_packet.group_id = site.active_outing.activity_id;
    stale_pre_casualty_packet.source_camp_id = site.site_id;
    stale_pre_casualty_packet.activity_generation = site.active_outing.generation;
    stale_pre_casualty_packet.handoff_epoch = site.active_outing.handoff_epoch;
    stale_pre_casualty_packet.return_application_key = site.active_outing.return_application_key;
    stale_pre_casualty_packet.job_type = bandit_dry_run::job_template::scout;
    stale_pre_casualty_packet.survivors_remaining = 2;
    const std::string before_stale_return = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_return_packet( site, stale_pre_casualty_packet ) );
    CHECK( serialize_world( world ) == before_stale_return );
    bandit_pursuit_handoff::return_packet contradictory_casualty_packet =
        stale_pre_casualty_packet;
    contradictory_casualty_packet.anchored_identity_updates = { { "45301", "missing" } };
    contradictory_casualty_packet.survivors_remaining = 1;
    CHECK_FALSE( bandit_live_world::apply_return_packet( site, contradictory_casualty_packet ) );
    CHECK( serialize_world( world ) == before_stale_return );
    bandit_live_world::local_gate_input gate_input;
    gate_input.local_threat = 1;
    gate_input.local_opportunity = 2;
    const bandit_live_world::local_gate_decision gate =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    CHECK( gate.dispatch_strength == 1 );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.is_active() );
    CHECK( loaded_site.active_outing.member_ids.size() == 2 );
    CHECK( loaded_site.active_outing.casualty_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    CHECK( loaded_site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    CHECK( loaded_site.active_outing.expected_return_minutes == 940 );
    CHECK( loaded_site.active_outing.missing_deadline_minutes == 2380 );
    REQUIRE( loaded_site.find_member( character_id( 45300 ) ) != nullptr );
    REQUIRE( loaded_site.find_member( character_id( 45301 ) ) != nullptr );
    CHECK( loaded_site.find_member( character_id( 45300 ) )->state ==
           bandit_live_world::member_state::local_contact );
    CHECK( loaded_site.find_member( character_id( 45301 ) )->state ==
           bandit_live_world::member_state::dead );

    bandit_live_world::world_state inconsistent = world;
    REQUIRE( inconsistent.sites.front().find_member( character_id( 45301 ) ) != nullptr );
    inconsistent.sites.front().find_member( character_id( 45301 ) )->state =
        bandit_live_world::member_state::local_contact;
    inconsistent.sites.front().living_total = 2;
    CHECK_THROWS( round_trip_world( inconsistent ) );
    CHECK( inconsistent.sites.front().active_outing.is_active() );
    CHECK( inconsistent.sites.front().find_member( character_id( 45301 ) )->state ==
           bandit_live_world::member_state::local_contact );
}

TEST_CASE( "bandit_live_world_first_scout_survivor_applies_provisional_receipts_once",
           "[bandit][live_world][scout_state][split_return][replay]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45500 );
    add_bandit_camp_member( world, 1, 45500 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45500 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45501 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    set_test_active_outing( site, site.site_id + "#scout:split" );
    site.active_outing.member_ids = { character_id( 45500 ), character_id( 45501 ) };
    site.active_outing.leader_id = character_id( 45500 );
    site.active_outing.job_type = "scout";
    site.active_outing.target_id = "split-target";
    site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
    site.active_outing.target_lead_revision = 4;
    site.active_outing.observations = {
        { "shared-visual", "one visible defender", 75, 490, false,
          bandit_live_world::sortie_observation_kind::routine, "" }
    };
    site.active_outing.cargo = { 3, 60 };
    site.active_outing.started_minutes = 100;
    site.active_outing.owner = bandit_live_world::simulation_owner::local;
    site.active_outing.handoff_epoch = 1;
    site.active_outing.last_advanced_minutes = 100;

    const std::vector<bandit_live_world::active_member_observation> first_arrival = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first survivor returned" },
        { character_id( 45501 ), bandit_live_world::active_member_observation_state::returning_home,
          "partner still returning" }
    };
    const bandit_live_world::scout_resolution_effect first_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), first_arrival, 500 );
    CHECK( first_effect.valid );
    CHECK( first_effect.changed );
    CHECK_FALSE( first_effect.completed );
    CHECK( first_effect.newly_resolved == 1 );
    CHECK( first_effect.newly_returned == 1 );
    CHECK( first_effect.provisional_report_applied );
    CHECK( first_effect.cargo_credited );
    CHECK( site.active_outing.is_active() );
    CHECK( site.active_outing.member_ids.size() == 2 );
    CHECK( site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45500 ) } );
    CHECK( site.find_member( character_id( 45500 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( site.find_member( character_id( 45501 ) )->state ==
           bandit_live_world::member_state::local_contact );
    REQUIRE( site.current_scout_report.is_present() );
    CHECK( site.current_scout_report.provisional );
    CHECK( site.current_scout_report.revision == 1 );
    CHECK( site.current_scout_report.action_policy ==
           bandit_live_world::camp_report_policy::bandit_shakedown );
    CHECK( site.current_scout_report.source_generation == site.active_outing.generation );
    CHECK( site.current_scout_report.application_key.find( ":members:45500" ) !=
           std::string::npos );
    CHECK( site.returned_cargo_stock.supply_units == 3 );
    CHECK( site.returned_cargo_stock.trade_value == 60 );
    CHECK( site.active_outing.cargo.supply_units == 0 );
    CHECK( site.active_outing.cargo.trade_value == 0 );
    CHECK( site.applied_return_generation == 0 );
    CHECK( site.applied_report_generation == 0 );
    CHECK( site.applied_cargo_generation == 0 );
    CHECK( site.camp_decision.state == bandit_live_world::camp_decision_state::idle );
    CHECK_FALSE( site.camp_decision.has_pinned_report() );

    const bandit_live_world::dispatch_plan blocked_dispatch =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 19, 20, 0 ),
                "new-target-before-partner-resolves" );
    CHECK_FALSE( blocked_dispatch.valid );

    const std::string before_replay = serialize_world( world );
    const bandit_live_world::scout_resolution_effect replay_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), first_arrival, 510 );
    CHECK( replay_effect.valid );
    CHECK_FALSE( replay_effect.changed );
    CHECK( serialize_world( world ) == before_replay );

    const std::vector<bandit_live_world::active_member_observation> contradictory_replay = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::dead,
          "contradictory stale state" },
        { character_id( 45501 ), bandit_live_world::active_member_observation_state::returning_home,
          "partner still returning" }
    };
    CHECK_FALSE( bandit_live_world::apply_active_scout_observations(
                     site, require_current_simulation_cursor( site ),
                     contradictory_replay, 520 ).valid );
    CHECK( serialize_world( world ) == before_replay );

    const std::vector<bandit_live_world::active_member_observation> duplicate_member = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first duplicate observation" },
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "second duplicate observation" }
    };
    const bandit_live_world::scout_resolution_effect duplicate_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), duplicate_member, 520 );
    CHECK_FALSE( duplicate_effect.valid );
    CHECK( duplicate_effect.newly_resolved == 0 );
    CHECK( serialize_world( world ) == before_replay );

    const std::vector<bandit_live_world::active_member_observation> unknown_member = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first survivor already home" },
        { character_id( 49999 ), bandit_live_world::active_member_observation_state::home,
          "unknown member" }
    };
    const bandit_live_world::scout_resolution_effect unknown_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), unknown_member, 520 );
    CHECK_FALSE( unknown_effect.valid );
    CHECK( unknown_effect.newly_resolved == 0 );
    CHECK( serialize_world( world ) == before_replay );

    bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.is_active() );
    CHECK( loaded_site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45500 ) } );
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK( loaded_site.current_scout_report.schema_version == 4 );
    CHECK( loaded_site.current_scout_report.provisional );
    CHECK( loaded_site.applied_report_generation == 0 );
    CHECK( loaded_site.returned_cargo_stock.supply_units == 3 );
    CHECK( loaded_site.camp_decision.state == bandit_live_world::camp_decision_state::idle );
    const std::string loaded_before_replay = serialize_world( loaded );
    CHECK_FALSE( bandit_live_world::apply_active_scout_observations(
                     loaded_site, require_current_simulation_cursor( loaded_site ),
                     first_arrival, 530 ).changed );
    CHECK( serialize_world( loaded ) == loaded_before_replay );

    loaded_site.active_outing.cargo = { 1, 20 };
    const std::vector<bandit_live_world::active_member_observation> final_arrival = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first survivor already home" },
        { character_id( 45501 ), bandit_live_world::active_member_observation_state::home,
          "second survivor returned" }
    };
    const bandit_live_world::scout_resolution_effect final_effect =
        bandit_live_world::apply_active_scout_observations(
            loaded_site, require_current_simulation_cursor( loaded_site ), final_arrival, 600 );
    CHECK( final_effect.valid );
    CHECK( final_effect.changed );
    CHECK( final_effect.completed );
    CHECK( final_effect.newly_resolved == 1 );
    CHECK_FALSE( loaded_site.active_outing.is_active() );
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK_FALSE( loaded_site.current_scout_report.provisional );
    CHECK( loaded_site.current_scout_report.revision == 2 );
    CHECK( loaded_site.current_scout_report.application_key.find( ":members" ) ==
           std::string::npos );
    CHECK( loaded_site.returned_cargo_stock.supply_units == 4 );
    CHECK( loaded_site.returned_cargo_stock.trade_value == 80 );
    CHECK( loaded_site.applied_return_generation == 1 );
    CHECK( loaded_site.applied_report_generation == 1 );
    CHECK( loaded_site.applied_cargo_generation == 1 );
    CHECK( loaded_site.camp_decision.state ==
           bandit_live_world::camp_decision_state::report_awaiting_assessment );
    CHECK( loaded_site.camp_decision.source_report_revision == 2 );
    CHECK( loaded_site.camp_decision.report_policy ==
           bandit_live_world::camp_report_policy::bandit_shakedown );
    REQUIRE( loaded_site.acted_reports.size() == 1 );
    CHECK( loaded_site.acted_reports.front().report_revision == 2 );
    CHECK( loaded_site.camp_decision.source_report_application_key ==
           loaded_site.current_scout_report.application_key );
    CHECK( loaded_site.camp_decision.target_id == "split-target" );
    CHECK( loaded_site.camp_decision.target_lead_revision == 4 );
    CHECK( loaded_site.camp_decision.last_transition_minutes == 600 );
    CHECK( loaded_site.find_member( character_id( 45500 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_site.find_member( character_id( 45501 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_report_revision_overflow_is_atomic",
           "[bandit][live_world][scout_state][report_policy][boundary]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45520 );
    add_bandit_camp_member( world, 1, 45520 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45520 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45521 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    set_test_active_outing( site, site.site_id + "#scout:overflow" );
    site.active_outing.member_ids = { character_id( 45520 ), character_id( 45521 ) };
    site.active_outing.leader_id = character_id( 45520 );
    site.active_outing.job_type = "scout";
    site.active_outing.target_id = "overflow-target";
    site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
    site.active_outing.owner = bandit_live_world::simulation_owner::local;
    site.active_outing.handoff_epoch = 1;
    site.active_outing.last_advanced_minutes = 100;
    site.current_scout_report.revision = std::numeric_limits<int>::max();
    site.current_scout_report.action_policy =
        bandit_live_world::camp_report_policy::bandit_shakedown;
    site.current_scout_report.source_activity_id = site.active_outing.activity_id;
    site.current_scout_report.source_generation = site.active_outing.generation;
    site.current_scout_report.source_job_type = "scout";
    site.current_scout_report.target_id = site.active_outing.target_id;
    site.current_scout_report.target_omt = site.active_outing.target_omt;
    site.current_scout_report.application_key = "overflow-provisional";
    site.current_scout_report.delivered_minutes = 100;
    site.current_scout_report.provisional = true;
    const std::vector<bandit_live_world::active_member_observation> observations = {
        { character_id( 45520 ), bandit_live_world::active_member_observation_state::home,
          "first survivor returned" },
        { character_id( 45521 ), bandit_live_world::active_member_observation_state::returning_home,
          "partner still returning" }
    };

    const std::string before = serialize_world( world );
    const bandit_live_world::scout_resolution_effect effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), observations, 500 );
    CHECK_FALSE( effect.valid );
    CHECK_FALSE( effect.changed );
    CHECK( serialize_world( world ) == before );
}

TEST_CASE( "bandit_live_world_split_scout_repairs_and_terminal_loss_paths_are_bounded",
           "[bandit][live_world][scout_state][split_return][save]" )
{
    bandit_live_world::world_state partial_world;
    add_bandit_camp_member( partial_world, 0, 45600 );
    add_bandit_camp_member( partial_world, 1, 45600 );
    bandit_live_world::site_record &partial_site = partial_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( partial_site, character_id( 45600 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( partial_site, character_id( 45601 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    set_test_active_outing( partial_site, partial_site.site_id + "#scout:deadline" );
    partial_site.active_outing.member_ids = { character_id( 45600 ), character_id( 45601 ) };
    partial_site.active_outing.leader_id = character_id( 45600 );
    partial_site.active_outing.job_type = "scout";
    partial_site.active_outing.target_id = "deadline-target";
    partial_site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
    partial_site.active_outing.cargo = { 2, 40 };
    partial_site.active_outing.expected_return_minutes = 960;
    partial_site.active_outing.missing_deadline_minutes = 2400;
    partial_site.active_outing.owner = bandit_live_world::simulation_owner::local;
    partial_site.active_outing.handoff_epoch = 1;

    const std::vector<bandit_live_world::active_member_observation> first_arrival = {
        { character_id( 45600 ), bandit_live_world::active_member_observation_state::home,
          "first survivor returned" },
        { character_id( 45601 ), bandit_live_world::active_member_observation_state::unresolved,
          "partner not loaded before deadline" }
    };
    REQUIRE( bandit_live_world::apply_active_scout_observations(
                 partial_site, require_current_simulation_cursor( partial_site ),
                 first_arrival, 500 ).provisional_report_applied );

    const std::vector<bandit_live_world::active_member_observation> premature_missing = {
        { character_id( 45600 ), bandit_live_world::active_member_observation_state::home,
          "first survivor already home" },
        { character_id( 45601 ), bandit_live_world::active_member_observation_state::missing,
          "premature missing declaration" }
    };
    const std::string before_premature_missing = serialize_world( partial_world );
    const bandit_live_world::scout_resolution_effect premature_effect =
        bandit_live_world::apply_active_scout_observations(
            partial_site, require_current_simulation_cursor( partial_site ),
            premature_missing, 2399 );
    CHECK_FALSE( premature_effect.valid );
    CHECK_FALSE( premature_effect.changed );
    CHECK( serialize_world( partial_world ) == before_premature_missing );

    SECTION( "malformed provisional report is cleared without losing the active reservation" ) {
        bandit_live_world::world_state malformed = partial_world;
        malformed.sites.front().current_scout_report.target_id = "forged-target";
        const bandit_live_world::world_state repaired = round_trip_world( malformed );
        REQUIRE( repaired.sites.size() == 1 );
        CHECK( repaired.sites.front().active_outing.is_active() );
        CHECK( repaired.sites.front().active_outing.resolved_member_ids ==
               std::vector<character_id> { character_id( 45600 ) } );
        CHECK_FALSE( repaired.sites.front().current_scout_report.is_present() );
        CHECK( repaired.sites.front().find_member( character_id( 45600 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( repaired.sites.front().find_member( character_id( 45601 ) )->state ==
               bandit_live_world::member_state::local_contact );
    }

    SECTION( "partner declared missing at the fixed deadline finalizes the scout" ) {
        const std::vector<bandit_live_world::active_member_observation> deadline = {
            { character_id( 45600 ), bandit_live_world::active_member_observation_state::home,
              "first survivor already home" },
            { character_id( 45601 ), bandit_live_world::active_member_observation_state::missing,
              "partner unresolved beyond fixed grace" }
        };
        const bandit_live_world::scout_resolution_effect effect =
            bandit_live_world::apply_active_scout_observations(
                partial_site, require_current_simulation_cursor( partial_site ), deadline, 2400 );
        CHECK( effect.valid );
        CHECK( effect.completed );
        CHECK_FALSE( partial_site.active_outing.is_active() );
        CHECK( partial_site.find_member( character_id( 45600 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( partial_site.find_member( character_id( 45601 ) )->state ==
               bandit_live_world::member_state::missing );
        REQUIRE( partial_site.current_scout_report.is_present() );
        CHECK_FALSE( partial_site.current_scout_report.provisional );
        CHECK( partial_site.current_scout_report.casualty_ids ==
               std::vector<character_id> { character_id( 45601 ) } );
        CHECK( partial_site.returned_cargo_stock.supply_units == 2 );
    }

    SECTION( "all casualties close lost without creating a physical report or cargo credit" ) {
        bandit_live_world::world_state lost_world;
        add_bandit_camp_member( lost_world, 0, 45610 );
        add_bandit_camp_member( lost_world, 1, 45610 );
        bandit_live_world::site_record &lost_site = lost_world.sites.front();
        REQUIRE( bandit_live_world::update_member_state( lost_site, character_id( 45610 ),
                 bandit_live_world::member_state::local_contact, "paired scout active" ) );
        REQUIRE( bandit_live_world::update_member_state( lost_site, character_id( 45611 ),
                 bandit_live_world::member_state::local_contact, "paired scout active" ) );
        set_test_active_outing( lost_site, lost_site.site_id + "#scout:lost" );
        lost_site.active_outing.member_ids = { character_id( 45610 ), character_id( 45611 ) };
        lost_site.active_outing.leader_id = character_id( 45610 );
        lost_site.active_outing.job_type = "scout";
        lost_site.active_outing.target_id = "lost-target";
        lost_site.active_outing.cargo = { 5, 100 };
        lost_site.active_outing.expected_return_minutes = 960;
        lost_site.active_outing.missing_deadline_minutes = 2400;
        lost_site.active_outing.owner = bandit_live_world::simulation_owner::local;
        lost_site.active_outing.handoff_epoch = 1;
        const std::vector<bandit_live_world::active_member_observation> lost = {
            { character_id( 45610 ), bandit_live_world::active_member_observation_state::dead,
              "lead scout killed" },
            { character_id( 45611 ), bandit_live_world::active_member_observation_state::missing,
              "escort never returned" }
        };
        const bandit_live_world::scout_resolution_effect effect =
            bandit_live_world::apply_active_scout_observations(
                lost_site, require_current_simulation_cursor( lost_site ), lost, 2400 );
        CHECK( effect.valid );
        CHECK( effect.completed );
        CHECK_FALSE( lost_site.active_outing.is_active() );
        CHECK_FALSE( lost_site.current_scout_report.is_present() );
        CHECK( lost_site.returned_cargo_stock.supply_units == 0 );
        CHECK( lost_site.returned_cargo_stock.trade_value == 0 );
        CHECK( lost_site.applied_return_generation == 1 );
        CHECK( lost_site.applied_report_generation == 1 );
        CHECK( lost_site.applied_cargo_generation == 1 );
        CHECK( lost_site.camp_decision.state ==
               bandit_live_world::camp_decision_state::idle );
        CHECK_FALSE( lost_site.camp_decision.has_pinned_report() );
    }
}

TEST_CASE( "bandit_live_world_atomically_rehomes_scout_report_and_cargo_before_closing",
           "[bandit][live_world][scout_state][handoff][replay]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45400 );
    add_bandit_camp_member( world, 1, 45400 );
    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan = bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 18, 20, 0 ), "target-for-return-receipt" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    REQUIRE( site.active_outing.member_ids.size() == 2 );
    const character_id scout_id = site.active_outing.member_ids.front();
    const character_id escort_id = site.active_outing.member_ids.back();
    site.active_outing.observations.push_back( { "visual-window-1", "one visible defender",
                                                 70, 500, false,
                                                 bandit_live_world::sortie_observation_kind::routine, "" } );
    site.active_outing.cargo.supply_units = 4;
    site.active_outing.cargo.trade_value = 120;
    site.active_outing.started_minutes = 450;
    site.active_outing.last_progress_minutes = 500;
    site.active_outing.last_advanced_minutes = 510;

    const std::vector<bandit_live_world::active_member_observation> home = {
        { scout_id, bandit_live_world::active_member_observation_state::home, "returned home" },
        { escort_id, bandit_live_world::active_member_observation_state::home,
          "escort returned home" }
    };
    const std::optional<bandit_pursuit_handoff::return_packet> packet =
        bandit_live_world::resolve_active_group_aftermath( site, home );
    REQUIRE( packet.has_value() );
    REQUIRE( bandit_live_world::apply_return_packet( site, *packet ) );
    CHECK_FALSE( site.active_outing.is_active() );
    CHECK( site.applied_return_generation == 1 );
    CHECK( site.applied_report_generation == 1 );
    CHECK( site.applied_cargo_generation == 1 );
    REQUIRE( site.current_scout_report.is_present() );
    CHECK( site.current_scout_report.revision == 1 );
    CHECK( site.current_scout_report.source_generation == 1 );
    CHECK( site.current_scout_report.target_id == "target-for-return-receipt" );
    CHECK( site.current_scout_report.target_omt == tripoint_abs_omt( 18, 20, 0 ) );
    CHECK_FALSE( site.current_scout_report.target_lead_id.empty() );
    CHECK( site.current_scout_report.target_lead_revision == 1 );
    REQUIRE( site.current_scout_report.observations.size() == 1 );
    CHECK( site.current_scout_report.observations.front().fact_key == "visual-window-1" );
    CHECK( site.current_scout_report.delivered_minutes == 510 );
    CHECK( site.returned_cargo_stock.supply_units == 4 );
    CHECK( site.returned_cargo_stock.trade_value == 120 );
    CHECK_FALSE( site.last_cargo_application_key.empty() );

    bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK( loaded_site.current_scout_report.observations.front().fact_key == "visual-window-1" );
    CHECK( loaded_site.current_scout_report.target_id == "target-for-return-receipt" );
    CHECK( loaded_site.returned_cargo_stock.supply_units == 4 );
    CHECK( loaded_site.returned_cargo_stock.trade_value == 120 );
    const std::string before_replay = serialize_world( loaded );
    CHECK_FALSE( bandit_live_world::apply_return_packet( loaded_site, *packet ) );
    CHECK( serialize_world( loaded ) == before_replay );
}

TEST_CASE( "bandit_live_world_keeps_several_hostile_sites_independent_across_save_and_writeback",
           "[bandit][live_world][multi_site]" )
{
    bandit_live_world::world_state original;
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 1001 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "thug", character_id( 1002 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit_mechanic", character_id( 2001 ),
             tripoint_abs_ms( 960, 1200, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 2002 ),
             tripoint_abs_ms( 984, 1224, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 3001 ),
             tripoint_abs_ms( 168, 120, 0 ), std::nullopt, std::string( "mx_bandits_block" ),
             special_lookup ) );

    REQUIRE( original.sites.size() == 3 );
    bandit_live_world::site_record &camp =
        *original.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &work_camp =
        *original.find_site( "overmap_special:bandit_work_camp@40,50,0" );
    bandit_live_world::site_record &roadblock =
        *original.find_site( "map_extra:mx_bandits_block@7,5,0" );

    const bandit_live_world::dispatch_plan camp_plan =
        bandit_live_world::plan_site_dispatch( camp, tripoint_abs_omt( 18, 20, 0 ),
                                               "player@18,20,0" );
    REQUIRE( camp_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( camp, camp_plan ) );
    camp.remembered_threat_estimate = 7;
    camp.remembered_bounty_estimate = 11;
    camp.remembered_retreat_bias = 2;
    camp.remembered_return_clock = 30;
    camp.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    camp.known_recent_marks = { "west-basecamp-pressure" };

    const bandit_live_world::dispatch_plan work_camp_plan =
        bandit_live_world::plan_site_dispatch( work_camp, tripoint_abs_omt( 48, 50, 0 ),
                                               "player@48,50,0" );
    REQUIRE( work_camp_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( work_camp, work_camp_plan ) );
    work_camp.remembered_threat_estimate = 3;
    work_camp.remembered_bounty_estimate = 5;
    work_camp.remembered_retreat_bias = 1;
    work_camp.remembered_return_clock = 90;
    work_camp.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::tight;
    work_camp.known_recent_marks = { "east-workcamp-pressure" };

    const bandit_live_world::dispatch_plan roadblock_plan =
        bandit_live_world::plan_site_dispatch( roadblock, tripoint_abs_omt( 8, 5, 0 ),
                                               "player@8,5,0" );
    REQUIRE( roadblock_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( roadblock, roadblock_plan ) );
    roadblock.remembered_threat_estimate = 1;
    roadblock.known_recent_marks = { "roadblock-probe" };

    std::ostringstream out;
    JsonOut jsout( out, true );
    original.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    REQUIRE( loaded.sites.size() == 3 );
    bandit_live_world::site_record &loaded_camp =
        *loaded.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &loaded_work_camp =
        *loaded.find_site( "overmap_special:bandit_work_camp@40,50,0" );
    bandit_live_world::site_record &loaded_roadblock =
        *loaded.find_site( "map_extra:mx_bandits_block@7,5,0" );

    CHECK( loaded_camp.anchor == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( loaded_camp.living_total == 2 );
    CHECK( loaded_camp.active_outing.activity_id == "overmap_special:bandit_camp@10,20,0#dispatch" );
    CHECK( loaded_camp.active_outing.target_id == "player@18,20,0" );
    REQUIRE( loaded_camp.active_outing.member_ids ==
             std::vector<character_id>( { character_id( 1001 ), character_id( 1002 ) } ) );
    CHECK( loaded_camp.find_member( character_id( 1001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_camp.find_member( character_id( 1002 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_camp.dispatchable_member_capacity() == 0 );
    CHECK( loaded_camp.remembered_threat_estimate == 7 );
    CHECK( loaded_camp.remembered_bounty_estimate == 11 );
    REQUIRE( loaded_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_camp.known_recent_marks.front() == "west-basecamp-pressure" );

    CHECK( loaded_work_camp.anchor == tripoint_abs_omt( 40, 50, 0 ) );
    CHECK( loaded_work_camp.living_total == 2 );
    CHECK( loaded_work_camp.active_outing.activity_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    CHECK( loaded_work_camp.active_outing.target_id == "player@48,50,0" );
    REQUIRE( loaded_work_camp.active_outing.member_ids ==
             std::vector<character_id>( { character_id( 2002 ), character_id( 2001 ) } ) );
    CHECK( loaded_work_camp.active_outing.leader_id == character_id( 2002 ) );
    CHECK( loaded_work_camp.find_member( character_id( 2001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_work_camp.find_member( character_id( 2002 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_work_camp.dispatchable_member_capacity() == 0 );
    CHECK( loaded_work_camp.remembered_threat_estimate == 3 );
    CHECK( loaded_work_camp.remembered_bounty_estimate == 5 );
    CHECK( loaded_work_camp.remembered_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( loaded_work_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_work_camp.known_recent_marks.front() == "east-workcamp-pressure" );

    CHECK( loaded_roadblock.anchor == tripoint_abs_omt( 7, 5, 0 ) );
    CHECK( loaded_roadblock.living_total == 1 );
    CHECK( loaded_roadblock.active_outing.activity_id == "map_extra:mx_bandits_block@7,5,0#dispatch" );
    CHECK( loaded_roadblock.active_outing.target_id == "player@8,5,0" );
    REQUIRE( loaded_roadblock.active_outing.member_ids == std::vector<character_id>( { character_id( 3001 ) } ) );
    CHECK( loaded_roadblock.remembered_threat_estimate == 1 );
    REQUIRE( loaded_roadblock.known_recent_marks.size() == 1 );
    CHECK( loaded_roadblock.known_recent_marks.front() == "roadblock-probe" );

    bandit_pursuit_handoff::local_outcome camp_loss;
    camp_loss.survivors_remaining = 1;
    camp_loss.anchored_identity_updates = { { "1001", "missing" } };
    camp_loss.result = bandit_pursuit_handoff::mission_result::broken;
    camp_loss.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    camp_loss.posture = bandit_pursuit_handoff::return_posture::broken_flee;
    camp_loss.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    const bandit_pursuit_handoff::return_packet camp_packet =
        bandit_pursuit_handoff::build_return_packet( camp_plan.entry, camp_loss );

    REQUIRE( bandit_live_world::apply_return_packet( loaded_camp, camp_packet ) );
    CHECK( loaded_camp.living_total == 1 );
    CHECK( loaded_camp.active_outing.activity_id.empty() );
    CHECK( loaded_camp.active_outing.target_id.empty() );
    CHECK( loaded_camp.active_outing.member_ids.empty() );
    CHECK( loaded_camp.find_member( character_id( 1001 ) )->state ==
           bandit_live_world::member_state::missing );
    CHECK( loaded_camp.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->assigned_living_total == 0 );
    CHECK( loaded_camp.dispatchable_member_capacity() == 0 );
    CHECK_FALSE( bandit_live_world::plan_site_dispatch( loaded_camp, tripoint_abs_omt( 18, 20, 0 ),
                 "player@18,20,0" ).valid );

    CHECK( loaded_work_camp.living_total == 2 );
    CHECK( loaded_work_camp.active_outing.activity_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    REQUIRE( loaded_work_camp.active_outing.member_ids ==
             std::vector<character_id>( { character_id( 2002 ), character_id( 2001 ) } ) );
    CHECK( loaded_work_camp.active_outing.leader_id == character_id( 2002 ) );
    CHECK( loaded_work_camp.find_member( character_id( 2001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_work_camp.find_member( character_id( 2002 ) )->state ==
           bandit_live_world::member_state::outbound );
    REQUIRE( loaded_work_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_work_camp.known_recent_marks.front() == "east-workcamp-pressure" );

    CHECK( loaded_roadblock.living_total == 1 );
    CHECK( loaded_roadblock.active_outing.activity_id == "map_extra:mx_bandits_block@7,5,0#dispatch" );
    REQUIRE( loaded_roadblock.active_outing.member_ids == std::vector<character_id>( { character_id( 3001 ) } ) );
}

TEST_CASE( "bandit_live_world_builds_a_bounded_scout_dispatch_plan_from_owned_members", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 401 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 402 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 403 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    const bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );

    REQUIRE( plan.valid );
    CHECK( plan.site_id == site.site_id );
    CHECK( plan.target_id == "player_basecamp_nearby" );
    REQUIRE( plan.evaluation.candidates.size() > 1 );
    CHECK( plan.evaluation.candidates[plan.evaluation.winner_index].job ==
           bandit_dry_run::job_template::scout );
    CHECK( plan.entry.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
    CHECK( plan.entry.group_id == site.site_id + "#dispatch" );
    CHECK( plan.entry.current_target_or_mark == "player_basecamp_nearby" );
    REQUIRE( plan.member_ids.size() == 2 );
    CHECK( plan.group.group_strength == 2 );
    REQUIRE( plan.group.anchored_identities.size() == 2 );
    CHECK( plan.group.anchored_identities.front().id == "401" );
    CHECK( plan.group.anchored_identities.back().id == "402" );
}

TEST_CASE( "bandit_live_world_applies_a_dispatch_plan_by_marking_the_selected_member_outbound", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 501 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 502 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );

    bandit_live_world::dispatch_plan forged_singleton = plan;
    forged_singleton.member_ids.resize( 1 );
    const std::string before_forged_singleton = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( site, forged_singleton ) );
    CHECK( serialize_world( world ) == before_forged_singleton );

    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    REQUIRE( site.find_member( character_id( 501 ) ) != nullptr );
    CHECK( site.find_member( character_id( 501 ) )->state == bandit_live_world::member_state::outbound );
    CHECK( site.find_member( character_id( 501 ) )->last_writeback_summary ==
           "dispatch scout toward player_basecamp_nearby" );
    CHECK( site.find_member( character_id( 502 ) )->state == bandit_live_world::member_state::outbound );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::outbound ) == 2 );

    const bandit_live_world::dispatch_plan second_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    CHECK_FALSE( second_plan.valid );
    REQUIRE_FALSE( second_plan.notes.empty() );
    CHECK( second_plan.notes.back().find( "active outside group/contact" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_persists_independent_camp_intelligence_maps",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state original;
    REQUIRE( bandit_live_world::register_abstract_site( original,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_camp ) ) );
    REQUIRE( bandit_live_world::register_abstract_site( original,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_work_camp",
             tripoint_abs_omt( 41, 51, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_work_camp ) ) );

    bandit_live_world::site_record &camp =
        *original.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::camp_map_lead camp_lead;
    camp_lead.lead_id = camp.site_id + "#lead:basecamp_activity:west";
    camp_lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    camp_lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    camp_lead.target_id = "player@18,20,0";
    camp_lead.omt = tripoint_abs_omt( 18, 20, 0 );
    camp_lead.source_key = "scout-west";
    camp_lead.source_summary = "scout confirmed west target";
    camp_lead.bounty = 9;
    camp_lead.threat = 2;
    camp_lead.confidence = 4;
    camp_lead.target_alert = true;
    camp.intelligence_map.leads.push_back( camp_lead );

    bandit_live_world::site_record &work_camp =
        *original.find_site( "overmap_special:bandit_work_camp@40,50,0" );
    bandit_live_world::camp_map_lead work_lead;
    work_lead.lead_id = work_camp.site_id + "#lead:human_activity:east";
    work_lead.kind = bandit_live_world::camp_lead_kind::human_activity;
    work_lead.status = bandit_live_world::camp_lead_status::suspected;
    work_lead.target_id = "traveler@48,50,0";
    work_lead.omt = tripoint_abs_omt( 48, 50, 0 );
    work_lead.source_key = "smoke-east";
    work_lead.source_summary = "smoke mark suspected east target";
    work_lead.bounty = 3;
    work_lead.threat = 6;
    work_lead.confidence = 1;
    work_lead.scout_seen = false;
    work_camp.intelligence_map.leads.push_back( work_lead );

    std::ostringstream out;
    JsonOut jsout( out, true );
    original.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    const bandit_live_world::site_record &loaded_camp =
        *loaded.find_site( "overmap_special:bandit_camp@10,20,0" );
    const bandit_live_world::site_record &loaded_work_camp =
        *loaded.find_site( "overmap_special:bandit_work_camp@40,50,0" );

    REQUIRE( loaded_camp.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &loaded_camp_lead =
        loaded_camp.intelligence_map.leads.front();
    CHECK( loaded_camp_lead.lead_id == camp_lead.lead_id );
    CHECK( loaded_camp_lead.kind == bandit_live_world::camp_lead_kind::basecamp_activity );
    CHECK( loaded_camp_lead.status == bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( loaded_camp_lead.target_id == "player@18,20,0" );
    CHECK( loaded_camp_lead.omt == tripoint_abs_omt( 18, 20, 0 ) );
    CHECK( loaded_camp_lead.bounty == 9 );
    CHECK( loaded_camp_lead.threat == 2 );
    CHECK( loaded_camp_lead.confidence == 4 );
    CHECK( loaded_camp_lead.target_alert );

    REQUIRE( loaded_work_camp.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &loaded_work_lead =
        loaded_work_camp.intelligence_map.leads.front();
    CHECK( loaded_work_lead.lead_id == work_lead.lead_id );
    CHECK( loaded_work_lead.kind == bandit_live_world::camp_lead_kind::human_activity );
    CHECK( loaded_work_lead.status == bandit_live_world::camp_lead_status::suspected );
    CHECK( loaded_work_lead.target_id == "traveler@48,50,0" );
    CHECK( loaded_work_lead.omt == tripoint_abs_omt( 48, 50, 0 ) );
    CHECK( loaded_work_lead.bounty == 3 );
    CHECK( loaded_work_lead.threat == 6 );
    CHECK( loaded_work_lead.confidence == 1 );
    CHECK_FALSE( loaded_work_lead.scout_seen );
}

TEST_CASE( "bandit_camp_lead_origins_round_trip_and_legacy_leads_infer_origin",
           "[bandit][live_world][camp_map][origin]" )
{
    struct origin_case {
        bandit_live_world::camp_lead_origin origin;
        bandit_live_world::camp_lead_kind kind;
    };
    const std::vector<origin_case> explicit_cases = {
        { bandit_live_world::camp_lead_origin::legacy_radar,
          bandit_live_world::camp_lead_kind::human_activity },
        { bandit_live_world::camp_lead_origin::observer,
          bandit_live_world::camp_lead_kind::moving_actor },
        { bandit_live_world::camp_lead_origin::signal,
          bandit_live_world::camp_lead_kind::smoke_signal },
        { bandit_live_world::camp_lead_origin::returned_report,
          bandit_live_world::camp_lead_kind::basecamp_activity },
        { bandit_live_world::camp_lead_origin::structural_routine,
          bandit_live_world::camp_lead_kind::structural_bounty },
    };

    for( std::size_t index = 0; index < explicit_cases.size(); ++index ) {
        const origin_case &origin = explicit_cases[index];
        INFO( "origin=" << bandit_live_world::to_string( origin.origin ) );
        bandit_live_world::camp_map_lead lead;
        lead.lead_id = "origin-round-trip-" + std::to_string( index );
        lead.kind = origin.kind;
        lead.origin = origin.origin;
        lead.target_id = "origin-target-" + std::to_string( index );
        lead.omt = tripoint_abs_omt( 20 + static_cast<int>( index ), 30, 0 );

        const std::string bytes = serialize_camp_map_lead( lead );
        CHECK( bytes.find( "\"origin\": \"" +
                           bandit_live_world::to_string( origin.origin ) + "\"" ) !=
               std::string::npos );
        JsonValue value = json_loader::from_string( bytes );
        bandit_live_world::camp_map_lead loaded;
        loaded.deserialize( value.get_object() );
        CHECK( loaded.origin == origin.origin );
        CHECK( serialize_camp_map_lead( loaded ) == bytes );
    }

    const std::vector<origin_case> legacy_cases = {
        { bandit_live_world::camp_lead_origin::legacy_radar,
          bandit_live_world::camp_lead_kind::human_activity },
        { bandit_live_world::camp_lead_origin::signal,
          bandit_live_world::camp_lead_kind::light_signal },
        { bandit_live_world::camp_lead_origin::returned_report,
          bandit_live_world::camp_lead_kind::basecamp_activity },
        { bandit_live_world::camp_lead_origin::structural_routine,
          bandit_live_world::camp_lead_kind::terrain_opportunity },
    };
    for( std::size_t index = 0; index < legacy_cases.size(); ++index ) {
        const origin_case &expected = legacy_cases[index];
        INFO( "legacy kind index=" << index );
        bandit_live_world::camp_map_lead lead;
        lead.lead_id = "legacy-origin-" + std::to_string( index );
        lead.kind = expected.kind;
        lead.origin = bandit_live_world::camp_lead_origin::observer;
        lead.target_id = "legacy-target-" + std::to_string( index );
        lead.omt = tripoint_abs_omt( 40 + static_cast<int>( index ), 50, 0 );
        std::string bytes = serialize_camp_map_lead( lead );
        erase_pretty_json_member_line( bytes, "origin" );

        JsonValue value = json_loader::from_string( bytes );
        bandit_live_world::camp_map_lead loaded;
        loaded.deserialize( value.get_object() );
        CHECK( loaded.origin == expected.origin );
        CHECK( serialize_camp_map_lead( loaded ).find(
                   "\"origin\": \"" + bandit_live_world::to_string( expected.origin ) + "\"" ) !=
               std::string::npos );
    }
}

TEST_CASE( "bandit_camp_lead_origin_is_a_single_writer_boundary",
           "[bandit][live_world][camp_map][origin][single_writer]" )
{
    bandit_live_world::world_state world;
    bandit_live_world::site_record site;
    site.site_id = "origin-owner";
    world.sites.push_back( site );
    bandit_live_world::site_record &owned_site = world.sites.front();

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "shared-lead-id";
    lead.kind = bandit_live_world::camp_lead_kind::smoke_signal;
    lead.origin = bandit_live_world::camp_lead_origin::signal;
    lead.target_id = "smoke@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.bounty = 1;
    REQUIRE( bandit_live_world::upsert_camp_map_lead( owned_site, lead ) );
    REQUIRE( owned_site.intelligence_map.leads.size() == 1 );
    const int original_revision = owned_site.intelligence_map.leads.front().revision;
    const std::string before_cross_origin = serialize_world( world );

    bandit_live_world::camp_map_lead cross_origin = lead;
    cross_origin.origin = bandit_live_world::camp_lead_origin::observer;
    cross_origin.bounty = 9;
    CHECK_FALSE( bandit_live_world::upsert_camp_map_lead( owned_site, cross_origin ) );
    CHECK( serialize_world( world ) == before_cross_origin );
    REQUIRE( owned_site.intelligence_map.leads.size() == 1 );
    CHECK( owned_site.intelligence_map.leads.front().revision == original_revision );
    CHECK( owned_site.intelligence_map.leads.front().origin ==
           bandit_live_world::camp_lead_origin::signal );
    CHECK( owned_site.intelligence_map.leads.front().bounty == 1 );

    lead.bounty = 2;
    REQUIRE( bandit_live_world::upsert_camp_map_lead( owned_site, lead ) );
    REQUIRE( owned_site.intelligence_map.leads.size() == 1 );
    CHECK( owned_site.intelligence_map.leads.front().revision == original_revision + 1 );
    CHECK( owned_site.intelligence_map.leads.front().bounty == 2 );
}

TEST_CASE( "legacy_player_pressure_lookup_ignores_structural_leads_without_moving_memory",
           "[bandit][live_world][camp_map][origin][dispatch]" )
{
    const std::vector<bandit_live_world::camp_lead_kind> structural_kinds = {
        bandit_live_world::camp_lead_kind::structural_bounty,
        bandit_live_world::camp_lead_kind::terrain_opportunity,
        bandit_live_world::camp_lead_kind::frontier_probe,
    };
    for( std::size_t index = 0; index < structural_kinds.size(); ++index ) {
        bandit_live_world::site_record site;
        site.anchor = tripoint_abs_omt( 10, 20, 0 );
        bandit_live_world::camp_map_lead lead;
        lead.lead_id = "structural-near-avatar-" + std::to_string( index );
        lead.kind = structural_kinds[index];
        lead.origin = bandit_live_world::camp_lead_origin::structural_routine;
        lead.target_id = "structural-target-" + std::to_string( index );
        lead.omt = tripoint_abs_omt( 18, 20, 0 );
        site.intelligence_map.leads.push_back( lead );

        CHECK( bandit_live_world::find_camp_map_dispatch_lead_for_target(
                   site, lead.omt, lead.target_id ) == nullptr );
        CHECK( site.intelligence_map.leads.front().omt == tripoint_abs_omt( 18, 20, 0 ) );
    }

    bandit_live_world::site_record basecamp_site;
    basecamp_site.anchor = tripoint_abs_omt( 10, 20, 0 );
    bandit_live_world::camp_map_lead basecamp;
    basecamp.lead_id = "returned-basecamp";
    basecamp.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    basecamp.origin = bandit_live_world::camp_lead_origin::returned_report;
    basecamp.target_id = "player-basecamp";
    basecamp.omt = tripoint_abs_omt( 18, 20, 0 );
    basecamp_site.intelligence_map.leads.push_back( basecamp );
    REQUIRE( bandit_live_world::find_camp_map_dispatch_lead_for_target(
                 basecamp_site, tripoint_abs_omt( 19, 20, 0 ), "" ) != nullptr );
    const tripoint_abs_omt remembered_basecamp = basecamp_site.intelligence_map.leads.front().omt;
    CHECK( bandit_live_world::find_camp_map_dispatch_lead_for_target(
               basecamp_site, tripoint_abs_omt( 90, 90, 0 ), "new-avatar-position" ) == nullptr );
    CHECK( basecamp_site.intelligence_map.leads.front().omt == remembered_basecamp );

    bandit_live_world::site_record activity_site;
    activity_site.anchor = tripoint_abs_omt( 10, 20, 0 );
    bandit_live_world::camp_map_lead activity;
    activity.lead_id = "legacy-human-activity";
    activity.kind = bandit_live_world::camp_lead_kind::human_activity;
    activity.origin = bandit_live_world::camp_lead_origin::legacy_radar;
    activity.target_id = "player-pressure";
    activity.omt = tripoint_abs_omt( 21, 20, 0 );
    activity_site.intelligence_map.leads.push_back( activity );
    CHECK( bandit_live_world::find_camp_map_dispatch_lead_for_target(
               activity_site, activity.omt, activity.target_id ) != nullptr );
}

TEST_CASE( "bandit_live_world_scout_return_writes_a_persistent_camp_map_lead",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 551 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 552 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt target_omt( 18, 20, 0 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, target_omt, "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    site.active_outing.started_minutes = 120;
    site.active_outing.local_contact_minutes = 180;

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 2;
    outcome.result = bandit_pursuit_handoff::mission_result::scouted;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::still_valid;
    outcome.posture = bandit_pursuit_handoff::return_posture::escape_home;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );

    REQUIRE( site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &lead = site.intelligence_map.leads.front();
    CHECK( lead.kind == bandit_live_world::camp_lead_kind::basecamp_activity );
    CHECK( lead.origin == bandit_live_world::camp_lead_origin::returned_report );
    CHECK( lead.status == bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( lead.target_id == "player_basecamp_nearby" );
    CHECK( lead.omt == target_omt );
    CHECK( lead.source_key == plan.entry.group_id );
    CHECK( lead.bounty >= 2 );
    CHECK( lead.threat >= 1 );
    CHECK( lead.confidence >= 2 );
    CHECK( lead.threat_confirmed );
    CHECK_FALSE( lead.target_alert );
    CHECK_FALSE( lead.scout_seen );
    CHECK( lead.last_scouted_minutes == 120 );
    CHECK( lead.last_checked_minutes == 180 );
    CHECK( lead.last_outcome == "scouted" );
    CHECK( site.active_outing.activity_id.empty() );
}

TEST_CASE( "bandit_live_world_migrates_legacy_scalar_memory_only_as_fallback",
           "[bandit][live_world][camp_map]" )
{
    {
        const std::string legacy_json = R"({
            "site_id": "legacy_camp",
            "active_target_id": "legacy-active",
            "remembered_target_or_mark": "legacy-mark",
            "remembered_threat_estimate": 5,
            "remembered_bounty_estimate": 7
        })";
        JsonValue legacy_value = json_loader::from_string( legacy_json );
        bandit_live_world::site_record legacy_site;
        legacy_site.deserialize( legacy_value.get_object() );

        REQUIRE( legacy_site.intelligence_map.leads.size() == 1 );
        const bandit_live_world::camp_map_lead &lead = legacy_site.intelligence_map.leads.front();
        CHECK( lead.kind == bandit_live_world::camp_lead_kind::human_activity );
        CHECK( lead.origin == bandit_live_world::camp_lead_origin::legacy_radar );
        CHECK( lead.status == bandit_live_world::camp_lead_status::suspected );
        CHECK( lead.target_id == "legacy-mark" );
        CHECK( lead.bounty == 7 );
        CHECK( lead.threat == 5 );
        CHECK( lead.confidence == 2 );
        CHECK( lead.threat_confirmed );
        CHECK_FALSE( lead.scout_seen );
        CHECK_FALSE( lead.target_alert );
        CHECK( lead.last_outcome == "legacy_memory" );
        CHECK( lead.source_summary == "migrated from legacy remembered_* site memory" );
    }

    {
        const std::string explicit_empty_map_json = R"({
            "site_id": "empty_modern_camp",
            "remembered_target_or_mark": "legacy-mark",
            "remembered_threat_estimate": 9,
            "remembered_bounty_estimate": 9,
            "intelligence_map": {
                "schema_version": 1,
                "leads": []
            }
        })";
        JsonValue explicit_empty_map_value = json_loader::from_string( explicit_empty_map_json );
        bandit_live_world::site_record explicit_empty_map_site;
        explicit_empty_map_site.deserialize( explicit_empty_map_value.get_object() );

        CHECK( explicit_empty_map_site.intelligence_map.leads.empty() );
    }

    {
        const std::string modern_json = R"({
            "site_id": "modern_camp",
            "remembered_target_or_mark": "legacy-mark",
            "remembered_threat_estimate": 9,
            "remembered_bounty_estimate": 9,
            "intelligence_map": {
                "schema_version": 1,
                "leads": [ {
                    "lead_id": "modern_camp#lead:basecamp_activity:confirmed",
                    "kind": "basecamp_activity",
                    "status": "scout_confirmed",
                    "target_id": "confirmed-mark",
                    "bounty": 4,
                    "threat": 1,
                    "confidence": 3,
                    "source_summary": "existing camp map proof"
                } ]
            }
        })";
        JsonValue modern_value = json_loader::from_string( modern_json );
        bandit_live_world::site_record modern_site;
        modern_site.deserialize( modern_value.get_object() );

        REQUIRE( modern_site.intelligence_map.leads.size() == 1 );
        const bandit_live_world::camp_map_lead &lead = modern_site.intelligence_map.leads.front();
        CHECK( lead.lead_id == "modern_camp#lead:basecamp_activity:confirmed" );
        CHECK( lead.kind == bandit_live_world::camp_lead_kind::basecamp_activity );
        CHECK( lead.origin == bandit_live_world::camp_lead_origin::returned_report );
        CHECK( lead.status == bandit_live_world::camp_lead_status::scout_confirmed );
        CHECK( lead.target_id == "confirmed-mark" );
        CHECK( lead.bounty == 4 );
        CHECK( lead.threat == 1 );
        CHECK( lead.confidence == 3 );
        CHECK( lead.source_summary == "existing camp map proof" );
    }
}

TEST_CASE( "bandit_structural_bounty_classifies_coarse_terrain",
           "[bandit][live_world][structural_bounty]" )
{
    CHECK( bandit_live_world::normalize_ground_bounty_opportunity( 0 ) == 0 );
    CHECK( bandit_live_world::normalize_ground_bounty_opportunity( 1 ) == 333 );
    CHECK( bandit_live_world::normalize_ground_bounty_opportunity( 2 ) == 667 );
    CHECK( bandit_live_world::normalize_ground_bounty_opportunity( 3 ) == 1000 );

    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest_thick" );
    CHECK( forest.eligible );
    CHECK( forest.terrain_class == "forest" );
    CHECK( forest.terrain_fit_class == "deep_forest" );
    CHECK( forest.bounty == 1 );
    CHECK( forest.confidence == 1 );

    const bandit_live_world::structural_bounty_read town =
        bandit_live_world::classify_structural_bounty_terrain( "house_base_north" );
    CHECK( town.eligible );
    CHECK( town.terrain_class == "town" );
    CHECK( town.terrain_fit_class == "building" );
    CHECK( town.bounty == 2 );
    CHECK( town.confidence == 1 );
    CHECK( town.latent_threat == 1 );

    const bandit_live_world::structural_bounty_read city =
        bandit_live_world::classify_structural_bounty_terrain( "city_downtown" );
    CHECK( city.eligible );
    CHECK( city.terrain_class == "town" );
    CHECK( city.terrain_fit_class == "dense_urban" );
    CHECK( city.bounty == 3 );
    CHECK( city.latent_threat == 2 );

    const bandit_live_world::structural_bounty_read open =
        bandit_live_world::classify_structural_bounty_terrain( "road_nesw" );
    CHECK_FALSE( open.eligible );
    CHECK( open.bounty == 0 );
    CHECK( open.terrain_class == "open" );
    CHECK( open.terrain_fit_class == "road" );

    for( const std::string &unsafe_compound : {
             "pond_field", "mil_base_minefield_nw", "open_air"
         } ) {
        const bandit_live_world::structural_bounty_read unsafe =
            bandit_live_world::classify_structural_bounty_terrain( unsafe_compound );
        INFO( "unsafe_compound=" << unsafe_compound );
        CHECK_FALSE( unsafe.eligible );
        CHECK( unsafe.terrain_fit_class == "unknown" );
        CHECK( unsafe.bounty == 0 );
    }
}

TEST_CASE( "hostile_camp_terrain_fit_uses_exact_faction_table_and_static_priors",
           "[bandit][live_world][structural_bounty][terrain_fit]" )
{
    using bandit_live_world::hostile_site_profile;
    const std::vector<std::pair<std::string, std::pair<int, int>>> expected_fit = {
        { "road", { 1000, 250 } },
        { "shelter", { 1000, 1000 } },
        { "building", { 1000, 250 } },
        { "town_edge", { 1000, 500 } },
        { "field", { 500, 500 } },
        { "forest_edge", { 500, 1000 } },
        { "rural", { 500, 1000 } },
        { "deep_forest", { 250, 500 } },
        { "swamp", { 250, 500 } },
        { "impassable", { 0, 0 } },
        { "unknown", { 333, 333 } },
    };
    for( const auto &entry : expected_fit ) {
        INFO( "terrain=" << entry.first );
        CHECK( bandit_live_world::hostile_camp_terrain_fit(
                   hostile_site_profile::camp_style, entry.first ) == entry.second.first );
        CHECK( bandit_live_world::hostile_camp_terrain_fit(
                   hostile_site_profile::cannibal_camp, entry.first ) == entry.second.second );
    }

    CHECK( bandit_live_world::structural_terrain_static_risk( "road" ) == 200 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "field" ) == 200 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "building" ) == 400 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "forest_edge" ) == 400 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "deep_forest" ) == 400 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "swamp" ) == 600 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "dense_urban" ) == 600 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "impassable" ) == 1000 );
    CHECK( bandit_live_world::structural_terrain_static_risk( "unknown" ) == 300 );
}

TEST_CASE( "hostile_camp_structural_cheap_score_prefers_faction_terrain_without_hidden_facts",
           "[bandit][live_world][structural_bounty][terrain_fit][score]" )
{
    for( const bool cannibal : { false, true } ) {
        bandit_live_world::world_state world;
        add_scheduler_test_site( world, 0, cannibal, cannibal ? 14600 : 14500 );
        bandit_live_world::site_record &site = world.sites.front();
        const tripoint_abs_omt building_omt( site.anchor.x() - 4, site.anchor.y(), 0 );
        const tripoint_abs_omt forest_edge_omt( site.anchor.x() + 4, site.anchor.y(), 0 );
        const bandit_live_world::structural_bounty_read building =
            bandit_live_world::classify_structural_bounty_terrain( "house_base" );
        const bandit_live_world::structural_bounty_read forest_edge =
            bandit_live_world::classify_structural_bounty_terrain( "forest_trail" );
        REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                     site, building_omt, building, 0 ) );
        REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                     site, forest_edge_omt, forest_edge, 0 ) );

        const bandit_live_world::camp_map_lead *building_lead =
            site.intelligence_map.find_lead( bandit_live_world::make_structural_bounty_lead_id(
                    site.site_id, building_omt, "town" ) );
        const bandit_live_world::camp_map_lead *forest_lead =
            site.intelligence_map.find_lead( bandit_live_world::make_structural_bounty_lead_id(
                    site.site_id, forest_edge_omt, "forest" ) );
        REQUIRE( building_lead != nullptr );
        REQUIRE( forest_lead != nullptr );
        CHECK( building_lead->origin ==
               bandit_live_world::camp_lead_origin::structural_routine );
        CHECK( forest_lead->origin ==
               bandit_live_world::camp_lead_origin::structural_routine );
        CHECK_FALSE( building_lead->threat_confirmed );
        CHECK_FALSE( forest_lead->threat_confirmed );
        CHECK( building_lead->last_checked_minutes == -1 );
        CHECK( forest_lead->last_checked_minutes == -1 );

        const bandit_live_world::structural_outing_plan building_plan =
            bandit_live_world::plan_structural_bounty_outing( site, *building_lead, 100 );
        const bandit_live_world::structural_outing_plan forest_plan =
            bandit_live_world::plan_structural_bounty_outing( site, *forest_lead, 100 );
        REQUIRE( building_plan.valid );
        REQUIRE( forest_plan.valid );
        CHECK( building_plan.cheap_route_quality == 778 );
        CHECK( forest_plan.cheap_route_quality == 778 );
        CHECK( building_plan.final_route_quality == 556 );
        CHECK( forest_plan.final_route_quality == 556 );

        const bandit_live_world::structural_outing_plan selected =
            bandit_live_world::plan_structural_bounty_outing( site, 100 );
        REQUIRE( selected.valid );
        if( cannibal ) {
            CHECK( building_plan.terrain_fit == 250 );
            CHECK( forest_plan.terrain_fit == 1000 );
            CHECK( building_plan.cheap_score == 372 );
            CHECK( forest_plan.cheap_score == 447 );
            CHECK( selected.target_omt == forest_edge_omt );
        } else {
            CHECK( building_plan.terrain_fit == 1000 );
            CHECK( forest_plan.terrain_fit == 500 );
            CHECK( building_plan.cheap_score == 447 );
            CHECK( forest_plan.cheap_score == 397 );
            CHECK( selected.target_omt == building_omt );
        }
    }
}

TEST_CASE( "hostile_camp_checked_estimates_decay_and_routine_history_penalizes_repetition",
           "[bandit][live_world][structural_bounty][terrain_fit][score][save]" )
{
    bandit_live_world::world_state world;
    add_scheduler_test_site( world, 0, false, 14700 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt target( site.anchor.x() - 4, site.anchor.y(), site.anchor.z() );
    const bandit_live_world::structural_bounty_read read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, target, read, 0 ) );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead(
            bandit_live_world::make_structural_bounty_lead_id( site.site_id, target, "town" ) );
    REQUIRE( lead != nullptr );
    lead->bounty = 3;
    lead->confidence = 3;
    lead->last_checked_minutes = 0;

    const bandit_live_world::structural_outing_plan fresh =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 0 );
    const bandit_live_world::structural_outing_plan half_fresh =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 7 * 24 * 60 );
    const bandit_live_world::structural_outing_plan expired =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 14 * 24 * 60 );
    CHECK( fresh.estimate_freshness == 1000 );
    CHECK( half_fresh.estimate_freshness == 500 );
    CHECK( expired.estimate_freshness == 0 );
    CHECK( half_fresh.valid );
    CHECK( expired.valid );

    bandit_live_world::camp_map_lead unobserved = *lead;
    unobserved.lead_id = "aaa-unobserved-equal-score";
    unobserved.bounty = 2;
    unobserved.confidence = 1;
    unobserved.last_checked_minutes = -1;
    unobserved.last_scouted_minutes = -1;
    site.intelligence_map.leads.push_back( unobserved );
    lead = site.intelligence_map.find_lead(
               bandit_live_world::make_structural_bounty_lead_id( site.site_id, target, "town" ) );
    REQUIRE( lead != nullptr );
    const bandit_live_world::structural_outing_plan unobserved_plan =
        bandit_live_world::plan_structural_bounty_outing(
            site, site.intelligence_map.leads.back(), 14 * 24 * 60 );
    CHECK( expired.cheap_score == unobserved_plan.cheap_score );
    CHECK( expired.effective_interest > unobserved_plan.effective_interest );
    const bandit_live_world::structural_outing_plan tie_selected =
        bandit_live_world::plan_structural_bounty_outing( site, 14 * 24 * 60 );
    CHECK( tie_selected.lead_id == "aaa-unobserved-equal-score" );

    site.intelligence_map.last_routine_target_lead_id = lead->lead_id;
    const bandit_live_world::structural_outing_plan immediate_repeat =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 14 * 24 * 60 );
    site.intelligence_map.last_routine_target_lead_id = "another-lead";
    site.intelligence_map.previous_routine_target_lead_id = lead->lead_id;
    const bandit_live_world::structural_outing_plan previous_repeat =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 14 * 24 * 60 );
    site.intelligence_map.previous_routine_target_lead_id.clear();
    const bandit_live_world::structural_outing_plan no_repeat =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 14 * 24 * 60 );
    CHECK( immediate_repeat.repetition_penalty == 1000 );
    CHECK( previous_repeat.repetition_penalty == 500 );
    CHECK( no_repeat.repetition_penalty == 0 );
    CHECK( no_repeat.cheap_score - previous_repeat.cheap_score == 75 );
    CHECK( previous_repeat.cheap_score - immediate_repeat.cheap_score == 75 );

    site.intelligence_map.last_routine_target_lead_id = "last-routine-lead";
    site.intelligence_map.previous_routine_target_lead_id = "previous-routine-lead";
    const bandit_live_world::world_state loaded = round_trip_world( world );
    CHECK( loaded.sites.front().intelligence_map.schema_version == 5 );
    CHECK( loaded.sites.front().intelligence_map.last_routine_target_lead_id ==
           "last-routine-lead" );
    CHECK( loaded.sites.front().intelligence_map.previous_routine_target_lead_id ==
           "previous-routine-lead" );
}

TEST_CASE( "bandit_road_opportunity_is_physically_scouted_without_fabricating_bounty",
           "[bandit][live_world][structural_bounty][terrain_fit][outing]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14750 );
    add_bandit_camp_member( world, 1, 14750 );
    bandit_live_world::site_record *site = &world.sites.front();
    site->supply_units = 0;
    site->supply_last_update_minutes = 0;

    const bandit_live_world::structural_bounty_maintenance_result maintenance =
        bandit_live_world::advance_structural_bounty_maintenance( world, 100, 1, 1,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "road" );
    }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    REQUIRE( maintenance.scan.leads_seeded == 1 );
    CHECK( maintenance.dispatches_planned == 1 );
    CHECK( maintenance.dispatches_applied == 1 );
    REQUIRE( site->intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &road = site->intelligence_map.leads.front();
    REQUIRE( road.kind == bandit_live_world::camp_lead_kind::terrain_opportunity );
    CHECK( road.origin == bandit_live_world::camp_lead_origin::structural_routine );
    CHECK( road.bounty == 0 );
    CHECK( road.confidence == 0 );
    CHECK_FALSE( road.threat_confirmed );
    const std::string lead_id = road.lead_id;
    CHECK( site->active_outing.job_type == "scout" );
    CHECK( site->active_outing.started_minutes == 100 );
    CHECK( site->active_outing.expected_return_minutes == 240 );
    CHECK( site->intelligence_map.last_routine_target_lead_id == lead_id );
    CHECK( site->intelligence_map.previous_routine_target_lead_id.empty() );

    world = round_trip_world( world );
    site = &world.sites.front();
    CHECK( site->active_outing.phase == bandit_live_world::scout_phase::outbound );

    const bandit_live_world::structural_outing_result stalk =
        bandit_live_world::advance_structural_bounty_outings( world, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "road is presently quiet" };
    } );
    CHECK( stalk.stalking_checks_processed == 1 );

    world = round_trip_world( world );
    site = &world.sites.front();
    CHECK( site->active_outing.phase == bandit_live_world::scout_phase::observing );
    const bandit_live_world::structural_outing_result arrived =
        bandit_live_world::advance_structural_bounty_outings( world, 200,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( arrived.arrivals_processed == 1 );
    const bandit_live_world::camp_map_lead *checked =
        site->intelligence_map.find_lead( lead_id );
    REQUIRE( checked != nullptr );
    CHECK( checked->status == bandit_live_world::camp_lead_status::stale );
    CHECK( checked->bounty == 0 );
    CHECK( checked->times_harvested == 0 );
    CHECK( checked->last_outcome == "terrain_opportunity_physically_checked" );
    CHECK( site->returned_cargo_stock.supply_units == 0 );
    CHECK( site->returned_cargo_stock.trade_value == 0 );

    world = round_trip_world( world );
    site = &world.sites.front();
    CHECK( site->active_outing.phase == bandit_live_world::scout_phase::returning_home );

    const bandit_live_world::structural_outing_result returned =
        bandit_live_world::advance_structural_bounty_outings( world, 240,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( returned.members_returned == 2 );
    CHECK( site->active_outing.activity_id.empty() );
    CHECK( site->returned_cargo_stock.supply_units == 0 );
    CHECK( site->returned_cargo_stock.trade_value == 0 );
}

TEST_CASE( "bandit_structural_bounty_claim_becomes_bounded_supply_only_after_return",
           "[bandit][live_world][structural_bounty][resource][supply][save]" )
{
    bandit_live_world::world_state world;
    add_scheduler_test_site( world, 0, false, 14800 );
    bandit_live_world::site_record *site = &world.sites.front();
    site->supply_units = bandit_live_world::camp_supply_cap( *site ) - 2;
    const tripoint_abs_omt target( site->anchor.x() + 4, site->anchor.y(), 0 );
    const auto prepared = start_test_structural_bounty_outing( world, 0, target, 100 );
    const std::string &lead_id = prepared.first;
    const bandit_live_world::structural_outing_plan &plan = prepared.second;
    const int generation = site->active_outing.generation;
    const std::string cargo_key = site->active_outing.cargo_application_key;
    CHECK( site->active_outing.job_type == "scout" );

    const auto quiet = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet structural target" };
    };
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, plan.expected_stalking_minutes, quiet ).stalking_checks_processed == 1 );
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, plan.expected_arrival_minutes, quiet ).arrivals_processed == 1 );

    site = &world.sites.front();
    const bandit_live_world::finite_resource_record *resource =
        world.find_finite_resource( target );
    REQUIRE( resource != nullptr );
    CHECK( resource->remaining_units == 1 );
    CHECK( resource->revision == 1 );
    const bandit_live_world::camp_map_lead *lead =
        site->intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    CHECK( lead->bounty == 1 );
    CHECK( lead->confidence == 3 );
    CHECK( lead->last_checked_minutes == plan.expected_arrival_minutes );
    CHECK( lead->last_outcome == "physical_resource_estimate_updated" );
    CHECK( site->applied_resource_generation == generation );
    CHECK( site->last_resource_claimed_units == 2 );
    CHECK( site->last_resource_application_key ==
           bandit_live_world::finite_resource_claim_application_key(
               plan.activity_id, generation, target ) );
    CHECK( site->active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( site->active_outing.cargo.supply_units == 4 );
    CHECK( site->supply_units == bandit_live_world::camp_supply_cap( *site ) - 2 );
    CHECK( site->returned_cargo_stock.supply_units == 0 );

    world = round_trip_world( world );
    site = &world.sites.front();
    CHECK( site->active_outing.cargo.supply_units == 4 );
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, plan.expected_return_minutes, quiet ).members_returned == 2 );
    site = &world.sites.front();
    CHECK( site->active_outing.kind == bandit_live_world::outing_kind::none );
    CHECK( site->supply_units == bandit_live_world::camp_supply_cap( *site ) );
    CHECK( site->returned_cargo_stock.supply_units == 0 );
    CHECK( site->applied_cargo_generation == generation );
    CHECK( site->last_cargo_application_key == cargo_key );
    CHECK( site->applied_return_generation == generation );

    const std::string completed = serialize_world( world );
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, plan.expected_return_minutes, quiet ).members_returned == 0 );
    CHECK( serialize_world( world ) == completed );
    CHECK( serialize_world( round_trip_world( world ) ) == completed );
}

TEST_CASE( "bandit_structural_bounty_contest_is_global_but_estimates_stay_private",
           "[bandit][live_world][structural_bounty][resource][knowledge]" )
{
    const auto quiet = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet contested target" };
    };

    SECTION( "full pairs divide a three-unit target two then one" ) {
        bandit_live_world::world_state world;
        add_scheduler_test_site( world, 0, false, 14900 );
        add_scheduler_test_site( world, 1, false, 14900 );
        world.sites[1].anchor = tripoint_abs_omt( 0, 5, 0 );
        world.sites[1].footprint = { world.sites[1].anchor };
        const tripoint_abs_omt target( 4, 0, 0 );
        const auto first = start_test_structural_bounty_outing( world, 0, target, 100 );
        const auto second = start_test_structural_bounty_outing( world, 1, target, 100 );
        REQUIRE( first.second.expected_arrival_minutes < second.second.expected_arrival_minutes );

        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, std::max( first.second.expected_stalking_minutes,
                                    second.second.expected_stalking_minutes ), quiet
               ).stalking_checks_processed == 2 );
        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, first.second.expected_arrival_minutes, quiet
               ).arrivals_processed == 1 );
        const bandit_live_world::finite_resource_record *resource =
            world.find_finite_resource( target );
        REQUIRE( resource != nullptr );
        CHECK( resource->remaining_units == 1 );
        CHECK( resource->revision == 1 );
        CHECK( world.sites[0].intelligence_map.find_lead( first.first )->bounty == 1 );
        CHECK( world.sites[1].intelligence_map.find_lead( second.first )->bounty == 3 );
        CHECK( world.sites[0].active_outing.cargo.supply_units == 4 );
        CHECK( world.sites[1].active_outing.cargo.supply_units == 0 );

        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, second.second.expected_arrival_minutes, quiet
               ).arrivals_processed == 1 );
        resource = world.find_finite_resource( target );
        REQUIRE( resource != nullptr );
        CHECK( resource->remaining_units == 0 );
        CHECK( resource->revision == 2 );
        CHECK( world.sites[0].intelligence_map.find_lead( first.first )->bounty == 1 );
        CHECK( world.sites[1].intelligence_map.find_lead( second.first )->bounty == 0 );
        CHECK( world.sites[0].last_resource_claimed_units == 2 );
        CHECK( world.sites[1].last_resource_claimed_units == 1 );
        CHECK( world.sites[0].active_outing.cargo.supply_units == 4 );
        CHECK( world.sites[1].active_outing.cargo.supply_units == 2 );
        CHECK( serialize_world( round_trip_world( world ) ) == serialize_world( world ) );
    }

    SECTION( "one physically recorded survivor takes one before the full pair takes two" ) {
        bandit_live_world::world_state world;
        add_scheduler_test_site( world, 0, false, 15000 );
        add_scheduler_test_site( world, 1, false, 15000 );
        world.sites[1].anchor = tripoint_abs_omt( 0, 5, 0 );
        world.sites[1].footprint = { world.sites[1].anchor };
        const tripoint_abs_omt target( 4, 0, 0 );
        const auto first = start_test_structural_bounty_outing( world, 0, target, 100 );
        const auto second = start_test_structural_bounty_outing( world, 1, target, 100 );
        REQUIRE( first.second.expected_arrival_minutes < second.second.expected_arrival_minutes );
        const character_id casualty_id = world.sites[0].active_outing.member_ids.back();
        REQUIRE( bandit_live_world::record_matching_external_outing_casualty(
                     world.sites[0], first.second.activity_id, first.second.generation,
                     casualty_id, bandit_live_world::member_state::dead, 101,
                     "physical casualty leaves one structural survivor" ) );
        CHECK( world.sites[0].active_outing_survivor_count() == 1 );

        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, std::max( first.second.expected_stalking_minutes,
                                    second.second.expected_stalking_minutes ), quiet
               ).stalking_checks_processed == 2 );
        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, first.second.expected_arrival_minutes, quiet
               ).arrivals_processed == 1 );
        const bandit_live_world::finite_resource_record *resource =
            world.find_finite_resource( target );
        REQUIRE( resource != nullptr );
        CHECK( resource->remaining_units == 2 );
        CHECK( resource->revision == 1 );
        CHECK( world.sites[0].intelligence_map.find_lead( first.first )->bounty == 2 );
        CHECK( world.sites[1].intelligence_map.find_lead( second.first )->bounty == 3 );
        CHECK( world.sites[0].last_resource_claimed_units == 1 );
        CHECK( world.sites[0].active_outing.cargo.supply_units == 2 );

        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, second.second.expected_arrival_minutes, quiet
               ).arrivals_processed == 1 );
        resource = world.find_finite_resource( target );
        REQUIRE( resource != nullptr );
        CHECK( resource->remaining_units == 0 );
        CHECK( resource->revision == 2 );
        CHECK( world.sites[0].intelligence_map.find_lead( first.first )->bounty == 2 );
        CHECK( world.sites[1].intelligence_map.find_lead( second.first )->bounty == 0 );
        CHECK( world.sites[1].last_resource_claimed_units == 2 );
        CHECK( world.sites[1].active_outing.cargo.supply_units == 4 );
    }
}

TEST_CASE( "bandit_structural_bounty_depleted_arrival_is_empty_and_idempotent",
           "[bandit][live_world][structural_bounty][resource]" )
{
    bandit_live_world::world_state world;
    add_scheduler_test_site( world, 0, false, 15100 );
    const tripoint_abs_omt target( 4, 0, 0 );
    world.finite_resources.emplace( target,
                                    bandit_live_world::finite_resource_record { 0, 1 } );
    const auto prepared = start_test_structural_bounty_outing( world, 0, target, 100 );
    CHECK( world.sites.front().active_outing.job_type == "scout" );
    const auto quiet = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet depleted target" };
    };
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, prepared.second.expected_stalking_minutes,
               quiet ).stalking_checks_processed == 1 );
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, prepared.second.expected_arrival_minutes,
               quiet ).arrivals_processed == 1 );
    const bandit_live_world::finite_resource_record *resource =
        world.find_finite_resource( target );
    REQUIRE( resource != nullptr );
    CHECK( resource->remaining_units == 0 );
    CHECK( resource->revision == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::camp_map_lead *lead =
        site.intelligence_map.find_lead( prepared.first );
    REQUIRE( lead != nullptr );
    CHECK( lead->bounty == 0 );
    CHECK( lead->confidence == 3 );
    CHECK( lead->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( lead->last_outcome == "physical_resource_depletion_observed" );
    CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( site.active_outing.cargo.supply_units == 0 );
    CHECK( site.applied_resource_generation == 0 );
    CHECK( site.last_resource_application_key.empty() );

    const std::string after_empty_arrival = serialize_world( world );
    CHECK( bandit_live_world::advance_structural_bounty_outings(
               world, prepared.second.expected_arrival_minutes,
               quiet ).arrivals_processed == 0 );
    CHECK( serialize_world( world ) == after_empty_arrival );
}

TEST_CASE( "bandit_structural_bounty_lead_upsert_respects_debounce",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13000 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 13, 20, 0 );
    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest_water" );
    REQUIRE( forest.eligible );

    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest, 100 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, forest.terrain_class );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    CHECK( lead->kind == bandit_live_world::camp_lead_kind::structural_bounty );
    CHECK( lead->status == bandit_live_world::camp_lead_status::suspected );
    CHECK( lead->bounty == 1 );
    CHECK( lead->threat == 0 );
    CHECK_FALSE( lead->threat_confirmed );

    lead->status = bandit_live_world::camp_lead_status::harvested;
    lead->bounty = 0;
    lead->times_harvested = 1;
    CHECK( bandit_live_world::structural_bounty_memory_suppresses_refresh( site.intelligence_map,
            forest_omt, forest.terrain_class ) );
    CHECK_FALSE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest, 200 ) );
    CHECK( site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::harvested );
    CHECK( site.intelligence_map.find_lead( lead_id )->bounty == 0 );
    CHECK( site.intelligence_map.find_lead( lead_id )->times_harvested == 1 );

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town, 300 ) );
    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                              town_omt, town.terrain_class );
    site.intelligence_map.find_lead( town_id )->status = bandit_live_world::camp_lead_status::dangerous;
    CHECK( bandit_live_world::structural_bounty_memory_suppresses_refresh( site.intelligence_map,
            town_omt, town.terrain_class ) );
    CHECK_FALSE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town, 400 ) );
}

TEST_CASE( "bandit_structural_scan_seeds_sparse_leads",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13200 );
    bandit_live_world::site_record &site = world.sites.front();

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { tripoint_abs_omt( 6, 20, 0 ), "forest_thick" },
        { tripoint_abs_omt( 14, 20, 0 ), "house_base_north" },
        { tripoint_abs_omt( 10, 16, 0 ), "road_nesw" },
        { tripoint_abs_omt( 10, 24, 0 ), "forest_water" },
    };

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 4,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );

    CHECK( result.sites_considered == 1 );
    CHECK( result.candidates_sampled == 4 );
    CHECK( result.budget_used == 4 );
    CHECK( result.leads_seeded == 4 );
    CHECK( site.intelligence_map.known_radius_omt == 8 );
    CHECK( site.intelligence_map.next_near_tick_minutes == 60 );
    CHECK( site.intelligence_map.leads.size() == 4 );

    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  tripoint_abs_omt( 6, 20, 0 ), "forest" );
    const bandit_live_world::camp_map_lead *forest = site.intelligence_map.find_lead( forest_id );
    REQUIRE( forest != nullptr );
    CHECK( forest->kind == bandit_live_world::camp_lead_kind::structural_bounty );
    CHECK( forest->status == bandit_live_world::camp_lead_status::suspected );
    CHECK( forest->target_id == "forest" );
    CHECK( forest->bounty == 1 );
    CHECK( forest->threat == 0 );
    CHECK( forest->confidence == 1 );
    CHECK_FALSE( forest->generated_by_this_camp_routine );

    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                tripoint_abs_omt( 14, 20, 0 ), "town" );
    const bandit_live_world::camp_map_lead *town = site.intelligence_map.find_lead( town_id );
    REQUIRE( town != nullptr );
    CHECK( town->bounty == 2 );
    CHECK( town->threat == 0 );
    CHECK( town->source_key == "structural_bounty:town:terrain_fit:building" );

    const auto road = std::find_if( site.intelligence_map.leads.begin(),
    site.intelligence_map.leads.end(), []( const bandit_live_world::camp_map_lead & lead ) {
        return lead.kind == bandit_live_world::camp_lead_kind::terrain_opportunity;
    } );
    REQUIRE( road != site.intelligence_map.leads.end() );
    CHECK( road->omt == tripoint_abs_omt( 10, 16, 0 ) );
    CHECK( road->target_id == "road" );
    CHECK( road->bounty == 0 );
    CHECK( road->confidence == 0 );
    CHECK_FALSE( road->threat_confirmed );
    CHECK( road->source_key == "terrain_opportunity:terrain_fit:road" );
}

TEST_CASE( "bandit_structural_scan_respects_global_budget_cap",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13300 );
    add_bandit_work_camp_member( world, 0, 13400 );

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 5,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    REQUIRE( world.sites.size() == 2 );
    CHECK( result.sites_considered == 2 );
    CHECK( result.candidates_sampled == 5 );
    CHECK( result.budget_used == 5 );
    CHECK( result.budget_exhausted );
    CHECK( result.leads_seeded == 5 );
    CHECK( world.sites.front().intelligence_map.leads.size() == 4 );
    CHECK( world.sites.back().intelligence_map.leads.size() == 1 );
}

TEST_CASE( "bandit_structural_scan_respects_per_camp_near_cadence",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13500 );
    bandit_live_world::site_record &site = world.sites.front();

    const bandit_live_world::structural_bounty_scan_result first =
        bandit_live_world::advance_structural_bounty_scan( world, 10, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );
    CHECK( first.candidates_sampled == 4 );
    CHECK( site.intelligence_map.next_near_tick_minutes == 70 );

    const bandit_live_world::structural_bounty_scan_result second =
        bandit_live_world::advance_structural_bounty_scan( world, 20, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );
    CHECK( second.sites_considered == 1 );
    CHECK( second.sites_deferred_by_cadence == 1 );
    CHECK( second.candidates_sampled == 0 );
    CHECK( second.leads_seeded == 0 );
    CHECK( site.intelligence_map.next_near_tick_minutes == 70 );
}

TEST_CASE( "bandit_structural_scan_skips_non_camp_profiles",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_work_camp_member( world, 0, 13600 );
    bandit_live_world::site_record &site = world.sites.front();
    site.profile = bandit_live_world::hostile_site_profile::small_hostile_site;

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.sites_considered == 1 );
    CHECK( result.sites_skipped_not_camp == 1 );
    CHECK( result.candidates_sampled == 0 );
    CHECK( site.intelligence_map.leads.empty() );
}

TEST_CASE( "bandit_structural_scan_skips_active_outside_or_no_ready_home_sites",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13700 );
    add_bandit_work_camp_member( world, 0, 13800 );
    bandit_live_world::site_record &active_outside_site = world.sites.front();
    bandit_live_world::site_record &empty_home_site = world.sites.back();
    active_outside_site.active_outing.member_ids.push_back( character_id( 13700 ) );
    empty_home_site.members.clear();
    empty_home_site.living_total = 0;
    empty_home_site.spawn_tiles.clear();

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 8,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.sites_considered == 2 );
    CHECK( result.sites_skipped_active_outside == 1 );
    CHECK( result.sites_skipped_no_ready_home == 1 );
    CHECK( result.candidates_sampled == 0 );
    CHECK( active_outside_site.intelligence_map.leads.empty() );
    CHECK( empty_home_site.intelligence_map.leads.empty() );
}

TEST_CASE( "bandit_structural_scan_skips_retired_empty_sites",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13900 );
    bandit_live_world::site_record &site = world.sites.front();
    site.retired_empty_site = true;

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.sites_considered == 1 );
    CHECK( result.sites_skipped_retired == 1 );
    CHECK( result.candidates_sampled == 0 );
    CHECK( result.leads_seeded == 0 );
    CHECK( site.intelligence_map.leads.empty() );
    CHECK( site.intelligence_map.next_near_tick_minutes == -1 );
}

TEST_CASE( "bandit_structural_scan_does_not_refresh_harvested_or_dangerous_bounty",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14000 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 10 ) );
    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  forest_omt, "forest" );
    site.intelligence_map.find_lead( forest_id )->status =
        bandit_live_world::camp_lead_status::harvested;
    site.intelligence_map.find_lead( forest_id )->bounty = 0;
    site.intelligence_map.find_lead( forest_id )->times_harvested = 1;

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 10 ) );
    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    site.intelligence_map.find_lead( town_id )->status =
        bandit_live_world::camp_lead_status::dangerous;
    site.intelligence_map.find_lead( town_id )->threat = 5;

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
        { town_omt, "house_base" },
    };
    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 2,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );

    CHECK( result.candidates_sampled == 2 );
    CHECK( result.leads_seeded == 0 );
    CHECK( result.leads_suppressed_by_memory == 2 );
    REQUIRE( site.intelligence_map.find_lead( forest_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( forest_id )->status ==
           bandit_live_world::camp_lead_status::harvested );
    CHECK( site.intelligence_map.find_lead( forest_id )->bounty == 0 );
    CHECK( site.intelligence_map.find_lead( forest_id )->times_harvested == 1 );
    REQUIRE( site.intelligence_map.find_lead( town_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( town_id )->status ==
           bandit_live_world::camp_lead_status::dangerous );
    CHECK( site.intelligence_map.find_lead( town_id )->threat == 5 );
}

TEST_CASE( "bandit_structural_scan_suppresses_recently_checked_structural_lead",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14100 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    site.intelligence_map.find_lead( lead_id )->last_checked_minutes = 20;
    site.intelligence_map.find_lead( lead_id )->last_outcome = "recently_checked_low_interest";

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 30, 1,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.candidates_sampled == 1 );
    CHECK( result.leads_seeded == 0 );
    CHECK( result.leads_suppressed_by_memory == 1 );
    REQUIRE( site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( lead_id )->last_checked_minutes == 20 );
    CHECK( site.intelligence_map.find_lead( lead_id )->last_outcome == "recently_checked_low_interest" );
}

TEST_CASE( "bandit_structural_scan_does_not_convert_mobile_actor_to_ground_bounty",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14200 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt player_omt( 6, 20, 0 );

    bandit_live_world::camp_map_lead player;
    player.lead_id = "player_seen_in_open_ground";
    player.kind = bandit_live_world::camp_lead_kind::moving_actor;
    player.status = bandit_live_world::camp_lead_status::scout_confirmed;
    player.target_id = "player@6,20,0";
    player.omt = player_omt;
    player.bounty = 9;
    player.confidence = 3;
    site.intelligence_map.leads.push_back( player );

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { player_omt, "field_road" },
    };
    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );

    CHECK( result.candidates_sampled == 1 );
    CHECK( result.leads_seeded == 0 );
    REQUIRE( site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead *mobile =
        site.intelligence_map.find_lead( "player_seen_in_open_ground" );
    REQUIRE( mobile != nullptr );
    CHECK( mobile->kind == bandit_live_world::camp_lead_kind::moving_actor );
    CHECK( mobile->target_id == "player@6,20,0" );
    CHECK( mobile->bounty == 9 );
}

TEST_CASE( "bandit_structural_bounty_keeps_mobile_actor_separate_from_ground_bounty",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13100 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 13, 21, 0 );

    bandit_live_world::camp_map_lead player;
    player.lead_id = "player_seen_in_forest";
    player.kind = bandit_live_world::camp_lead_kind::moving_actor;
    player.status = bandit_live_world::camp_lead_status::scout_confirmed;
    player.target_id = "player@13,21,0";
    player.omt = forest_omt;
    player.bounty = 9;
    player.confidence = 3;
    site.intelligence_map.leads.push_back( player );

    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest, 500 ) );

    const std::string structural_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                       forest_omt, forest.terrain_class );
    const bandit_live_world::camp_map_lead *structural =
        site.intelligence_map.find_lead( structural_id );
    REQUIRE( structural != nullptr );
    CHECK( structural->kind == bandit_live_world::camp_lead_kind::structural_bounty );
    CHECK( structural->target_id == "forest" );
    CHECK( structural->bounty == 1 );
    CHECK( structural->confidence == 1 );

    const bandit_live_world::camp_map_lead *mobile =
        site.intelligence_map.find_lead( "player_seen_in_forest" );
    REQUIRE( mobile != nullptr );
    CHECK( mobile->kind == bandit_live_world::camp_lead_kind::moving_actor );
    CHECK( mobile->target_id == "player@13,21,0" );
    CHECK( mobile->bounty == 9 );
    CHECK( site.intelligence_map.leads.size() == 2 );
}

TEST_CASE( "bandit_structural_outing_planner_selects_forest_and_town_jobs",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14300 );
    add_bandit_camp_member( world, 1, 14300 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *forest = site.intelligence_map.find_lead( forest_id );
    REQUIRE( forest != nullptr );

    const bandit_live_world::structural_outing_plan forest_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *forest, 100 );
    REQUIRE( forest_plan.valid );
    CHECK( forest_plan.job == bandit_dry_run::job_template::scavenge );
    CHECK( forest_plan.member_ids.size() == 2 );
    CHECK( forest_plan.effective_interest == 2 );
    CHECK( forest_plan.shared_route == std::vector<tripoint_abs_omt> {
        site.anchor, tripoint_abs_omt( 7, 20, 0 ), forest_omt, site.anchor
    } );
    CHECK( forest_plan.expected_stalking_minutes == 160 );
    CHECK( forest_plan.expected_arrival_minutes == 200 );
    CHECK( forest_plan.expected_return_minutes == 240 );

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 0 ) );

    const bandit_live_world::structural_outing_plan best_plan =
        bandit_live_world::plan_structural_bounty_outing( site, 100 );
    REQUIRE( best_plan.valid );
    CHECK( best_plan.target_omt == town_omt );
    CHECK( best_plan.job == bandit_dry_run::job_template::scout );
    CHECK( best_plan.effective_interest == 3 );
    CHECK( best_plan.shared_route == std::vector<tripoint_abs_omt> {
        site.anchor, tripoint_abs_omt( 13, 20, 0 ), town_omt, site.anchor
    } );

    bandit_live_world::camp_map_lead route_limit = *forest;
    route_limit.lead_id = "route-limit";
    route_limit.omt = tripoint_abs_omt( 19, 20, 0 );
    const bandit_live_world::structural_outing_plan route_limit_plan =
        bandit_live_world::plan_structural_bounty_outing( site, route_limit, 100 );
    REQUIRE( route_limit_plan.valid );
    CHECK( route_limit_plan.shared_route.size() == 4 );
    CHECK( route_limit_plan.expected_return_minutes == 415 );

    bandit_live_world::camp_map_lead beyond_route_limit = route_limit;
    beyond_route_limit.lead_id = "beyond-route-limit";
    beyond_route_limit.omt = tripoint_abs_omt( 20, 20, 0 );
    CHECK_FALSE( bandit_live_world::plan_structural_bounty_outing(
                     site, beyond_route_limit, 100 ).valid );
}

TEST_CASE( "hostile_camp_frontier_memory_migrates_and_rejects_malformed_current_state",
           "[bandit][live_world][structural_bounty][frontier]" )
{
    JsonValue legacy_json = json_loader::from_string(
                                R"({"schema_version":2,"known_radius_omt":4,"leads":[]})" );
    bandit_live_world::camp_intelligence_map migrated;
    migrated.deserialize( legacy_json.get_object() );
    CHECK( migrated.schema_version == 5 );
    CHECK( migrated.terrain_scan_cursor == 0 );
    CHECK( migrated.frontier_sector_cursor == 0 );
    CHECK( migrated.frontier_last_resolved_minutes == std::vector<int>( 8, -1 ) );
    CHECK( migrated.known_radius_omt == 4 );
    CHECK( migrated.terrain_scan_cursor == 0 );

    JsonValue v3_json = json_loader::from_string(
                            R"({"schema_version":3,"frontier_sector_cursor":2,"frontier_last_resolved_minutes":[-1,-1,100,-1,-1,-1,-1,-1],"leads":[]})" );
    bandit_live_world::camp_intelligence_map migrated_v3;
    migrated_v3.deserialize( v3_json.get_object() );
    CHECK( migrated_v3.schema_version == 5 );
    CHECK( migrated_v3.terrain_scan_cursor == 0 );
    CHECK( migrated_v3.frontier_sector_cursor == 2 );

    bandit_live_world::camp_intelligence_map retained = migrated;
    retained.frontier_sector_cursor = 5;
    retained.frontier_last_resolved_minutes[5] = 900;
    JsonValue malformed_json = json_loader::from_string(
                                   R"({"schema_version":3,"frontier_sector_cursor":8,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( malformed_json.get_object() ) );
    CHECK( retained.frontier_sector_cursor == 5 );
    CHECK( retained.frontier_last_resolved_minutes[5] == 900 );

    JsonValue short_history_json = json_loader::from_string(
                                       R"({"schema_version":3,"frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( short_history_json.get_object() ) );
    CHECK( retained.frontier_sector_cursor == 5 );
    CHECK( retained.frontier_last_resolved_minutes[5] == 900 );

    JsonValue spoofed_legacy_json = json_loader::from_string(
                                       R"({"schema_version":2,"frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( spoofed_legacy_json.get_object() ) );

    JsonValue missing_terrain_cursor = json_loader::from_string(
                                           R"({"schema_version":4,"frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( missing_terrain_cursor.get_object() ) );
    JsonValue invalid_terrain_cursor = json_loader::from_string(
                                           R"({"schema_version":4,"terrain_scan_cursor":12,"frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( invalid_terrain_cursor.get_object() ) );
    JsonValue spoofed_v3_terrain_cursor = json_loader::from_string(
            R"({"schema_version":3,"terrain_scan_cursor":1,"frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( spoofed_v3_terrain_cursor.get_object() ) );

    JsonValue v4_json = json_loader::from_string(
                            R"({"schema_version":4,"terrain_scan_cursor":7,"frontier_sector_cursor":2,"frontier_last_resolved_minutes":[-1,-1,100,-1,-1,-1,-1,-1],"leads":[]})" );
    bandit_live_world::camp_intelligence_map migrated_v4;
    migrated_v4.deserialize( v4_json.get_object() );
    CHECK( migrated_v4.schema_version == 5 );
    CHECK( migrated_v4.terrain_scan_cursor == 7 );
    CHECK( migrated_v4.last_routine_target_lead_id.empty() );
    CHECK( migrated_v4.previous_routine_target_lead_id.empty() );

    JsonValue missing_target_history = json_loader::from_string(
            R"({"schema_version":5,"terrain_scan_cursor":0,"frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( missing_target_history.get_object() ) );
    JsonValue spoofed_v4_target_history = json_loader::from_string(
            R"({"schema_version":4,"terrain_scan_cursor":0,"last_routine_target_lead_id":"lead-a","previous_routine_target_lead_id":"lead-b","frontier_sector_cursor":0,"frontier_last_resolved_minutes":[-1,-1,-1,-1,-1,-1,-1,-1],"leads":[]})" );
    CHECK_THROWS( retained.deserialize( spoofed_v4_target_history.get_object() ) );
}

TEST_CASE( "hostile_camp_frontier_route_resolves_only_after_the_pair_reports_home",
           "[bandit][live_world][structural_bounty][frontier]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14350 );
    add_bandit_camp_member( world, 1, 14350 );
    bandit_live_world::site_record &site = world.sites.front();
    for( int index = 0; index < 64; ++index ) {
        site.intelligence_map.leads.push_back( make_retention_test_lead( index ) );
    }

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_frontier_outing( site, 100 );
    REQUIRE( plan.valid );
    CHECK( plan.frontier_sector == 0 );
    CHECK( plan.frontier_cursor == 0 );
    CHECK( plan.frontier_prior_resolved_minutes == -1 );
    CHECK( plan.job == bandit_dry_run::job_template::scout );
    CHECK( plan.shared_route == std::vector<tripoint_abs_omt> {
        site.anchor, tripoint_abs_omt( 10, 16, 0 ), tripoint_abs_omt( 10, 11, 0 ), site.anchor
    } );
    CHECK( plan.expected_stalking_minutes == 235 );
    CHECK( plan.expected_arrival_minutes == 325 );
    CHECK( plan.expected_return_minutes == 415 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes == std::vector<int>( 8, -1 ) );

    bandit_live_world::structural_outing_plan stale = plan;
    site.intelligence_map.frontier_sector_cursor = 1;
    const std::string before_stale_apply = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan( site, stale, 100 ) );
    CHECK( serialize_world( world ) == before_stale_apply );
    site.intelligence_map.frontier_sector_cursor = 0;

    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );
    CHECK( site.intelligence_map.frontier_sector_cursor == 0 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    const bandit_live_world::camp_map_lead *frontier =
        site.intelligence_map.find_lead( "frontier_probe:0" );
    REQUIRE( frontier != nullptr );
    CHECK( site.intelligence_map.leads.size() == 64 );
    CHECK( frontier->kind == bandit_live_world::camp_lead_kind::frontier_probe );
    CHECK( frontier->origin == bandit_live_world::camp_lead_origin::structural_routine );
    CHECK( frontier->radius_omt == 0 );
    CHECK( frontier->bounty == 0 );
    bandit_live_world::site_record target_match_site;
    target_match_site.intelligence_map.leads.push_back( *frontier );
    CHECK( bandit_live_world::find_camp_map_dispatch_lead_for_target(
               target_match_site, frontier->omt, frontier->target_id ) == nullptr );

    const auto quiet = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet frontier" };
    };
    CHECK( bandit_live_world::advance_structural_bounty_outings( world, 235,
            quiet ).stalking_checks_processed == 1 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    CHECK( bandit_live_world::advance_structural_bounty_outings( world, 325,
            quiet ).arrivals_processed == 1 );
    CHECK( site.active_outing.waypoint_index == 2 );
    CHECK( site.intelligence_map.frontier_sector_cursor == 0 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    frontier = site.intelligence_map.find_lead( "frontier_probe:0" );
    REQUIRE( frontier != nullptr );
    CHECK( frontier->status == bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( frontier->bounty == 0 );
    CHECK( frontier->times_harvested == 0 );
    CHECK( frontier->last_outcome == "frontier_outer_sample_complete" );

    site.routine_no_candidate_streak = 3;
    world = round_trip_world( world );
    bandit_live_world::site_record &loaded_site = world.sites.front();
    CHECK( loaded_site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    CHECK( bandit_live_world::advance_structural_bounty_outings( world, 415,
            quiet ).members_returned == 2 );
    CHECK( loaded_site.intelligence_map.frontier_last_resolved_minutes[0] == 415 );
    CHECK( loaded_site.intelligence_map.frontier_sector_cursor == 1 );
    CHECK( loaded_site.intelligence_map.frontier_radius_omt == 9 );
    CHECK( loaded_site.routine_no_candidate_streak == 0 );
    CHECK( loaded_site.active_outing.kind == bandit_live_world::outing_kind::none );
    const std::string completed = serialize_world( world );
    CHECK( bandit_live_world::advance_structural_bounty_outings( world, 415,
            quiet ).members_returned == 0 );
    CHECK( serialize_world( world ) == completed );

    REQUIRE( loaded_site.next_routine_dispatch_eligible_minutes > 415 );
    CHECK_FALSE( bandit_live_world::plan_frontier_outing(
                     loaded_site, loaded_site.next_routine_dispatch_eligible_minutes - 1 ).valid );
    const bandit_live_world::structural_outing_plan next =
        bandit_live_world::plan_frontier_outing(
            loaded_site, loaded_site.next_routine_dispatch_eligible_minutes );
    REQUIRE( next.valid );
    CHECK( next.frontier_sector == 1 );
    CHECK( next.shared_route == std::vector<tripoint_abs_omt> {
        loaded_site.anchor, tripoint_abs_omt( 14, 16, 0 ),
        tripoint_abs_omt( 19, 11, 0 ), loaded_site.anchor
    } );
    const std::string before_cooldown_apply = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan(
                     loaded_site, next,
                     loaded_site.next_routine_dispatch_eligible_minutes - 1 ) );
    CHECK( serialize_world( world ) == before_cooldown_apply );
}

TEST_CASE( "hostile_camp_frontier_danger_return_does_not_resolve_the_sector",
           "[bandit][live_world][structural_bounty][frontier]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14375 );
    add_bandit_camp_member( world, 1, 14375 );
    bandit_live_world::site_record &site = world.sites.front();
    site.routine_no_candidate_streak = 3;
    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_frontier_outing( site, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    const bandit_live_world::structural_outing_result withdrew =
        bandit_live_world::advance_structural_bounty_outings( world, 235,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 1, true, "danger on inner frontier" };
    } );
    CHECK( withdrew.lost_interest_returns == 1 );
    CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( site.active_outing.waypoint_index == 1 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    CHECK( bandit_live_world::advance_structural_bounty_outings( world, 415,
            {} ).members_returned == 2 );
    CHECK( site.routine_no_candidate_streak == 3 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    CHECK( site.intelligence_map.frontier_sector_cursor == 0 );
    REQUIRE( site.next_routine_dispatch_eligible_minutes > 415 );
    CHECK_FALSE( bandit_live_world::plan_frontier_outing(
                     site, site.next_routine_dispatch_eligible_minutes - 1 ).valid );
    const bandit_live_world::structural_outing_plan next =
        bandit_live_world::plan_frontier_outing(
            site, site.next_routine_dispatch_eligible_minutes );
    REQUIRE( next.valid );
    CHECK( next.frontier_sector == 1 );
    CHECK( site.intelligence_map.frontier_last_resolved_minutes[0] == -1 );
    CHECK( site.intelligence_map.frontier_sector_cursor == 0 );
}

TEST_CASE( "hostile_camp_frontier_cursor_covers_all_eight_sectors_in_order",
           "[bandit][live_world][structural_bounty][frontier]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14390 );
    add_bandit_camp_member( world, 1, 14390 );
    bandit_live_world::site_record &site = world.sites.front();
    const auto quiet = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    };
    const auto chebyshev_distance = []( const tripoint_abs_omt & lhs,
    const tripoint_abs_omt & rhs ) {
        return std::max( std::abs( lhs.x() - rhs.x() ), std::abs( lhs.y() - rhs.y() ) );
    };

    int started_minutes = 0;
    for( int sector = 0; sector < 8; ++sector ) {
        const bandit_live_world::structural_outing_plan plan =
            bandit_live_world::plan_frontier_outing( site, started_minutes );
        REQUIRE( plan.valid );
        CHECK( plan.frontier_sector == sector );
        CHECK( plan.shared_route.size() == 4 );
        CHECK( chebyshev_distance( plan.shared_route[0], plan.shared_route[1] ) == 4 );
        CHECK( chebyshev_distance( plan.shared_route[0], plan.shared_route[2] ) == 9 );
        CHECK( chebyshev_distance( plan.shared_route[0], plan.shared_route[1] ) +
               chebyshev_distance( plan.shared_route[1], plan.shared_route[2] ) +
               chebyshev_distance( plan.shared_route[2], plan.shared_route[3] ) == 18 );
        REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan(
                     site, plan, started_minutes ) );
        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, plan.expected_stalking_minutes, quiet ).stalking_checks_processed == 1 );
        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, plan.expected_arrival_minutes, quiet ).arrivals_processed == 1 );
        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, plan.expected_return_minutes, quiet ).members_returned == 2 );
        CHECK( site.intelligence_map.frontier_last_resolved_minutes[
                   static_cast<std::size_t>( sector )] == plan.expected_return_minutes );
        CHECK( site.intelligence_map.frontier_sector_cursor == ( sector + 1 ) % 8 );
        started_minutes = site.next_routine_dispatch_eligible_minutes;
    }
    CHECK( std::count_if( site.intelligence_map.frontier_last_resolved_minutes.begin(),
    site.intelligence_map.frontier_last_resolved_minutes.end(), []( const int resolved_minutes ) {
        return resolved_minutes >= 0;
    } ) == 8 );
    CHECK( site.intelligence_map.leads.size() == 8 );
    CHECK( serialize_world( round_trip_world( world ) ) == serialize_world( world ) );
}

TEST_CASE( "bandit_structural_outing_planner_blocks_active_outside_pressure",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14400 );
    add_bandit_camp_member( world, 1, 14400 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan open_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( open_plan.valid );

    set_test_active_outing( site, site.site_id + "#dispatch" );
    site.active_outing.member_ids.push_back( character_id( 14400 ) );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    CHECK_FALSE( plan.valid );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan( site, open_plan, 100 ) );
}

TEST_CASE( "bandit_live_world_reserves_members_and_mission_slot_under_one_generation",
           "[bandit][live_world][reservation][generation][structural_bounty]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        add_bandit_camp_member( world, index, 14450 );
    }
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt structural_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read structural_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                 site, structural_omt, structural_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                    site.site_id, structural_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan stale_structural_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( stale_structural_plan.valid );
    CHECK( stale_structural_plan.activity_id == site.site_id + "#structural" );
    CHECK( stale_structural_plan.generation == 1 );

    const bandit_live_world::dispatch_plan first_dispatch =
        bandit_live_world::plan_site_dispatch(
            site, tripoint_abs_omt( 18, 20, 0 ), "reservation-first-target" );
    REQUIRE( first_dispatch.valid );
    CHECK( first_dispatch.entry.activity_generation == stale_structural_plan.generation );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, first_dispatch ) );
    const std::string first_reservation = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan(
                     site, stale_structural_plan, 100 ) );
    CHECK( serialize_world( world ) == first_reservation );
    REQUIRE( site.roster().valid );
    CHECK( site.roster().reserved_unresolved_ids == first_dispatch.member_ids );
    CHECK( site.active_outing.activity_id == first_dispatch.entry.group_id );
    CHECK( site.active_outing.generation == first_dispatch.entry.activity_generation );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 2;
    outcome.result = bandit_pursuit_handoff::mission_result::scouted;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::still_valid;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( first_dispatch.entry, outcome );
    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );
    REQUIRE( site.camp_decision.state ==
             bandit_live_world::camp_decision_state::report_awaiting_assessment );
    lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    CHECK_FALSE( bandit_live_world::plan_structural_bounty_outing(
                     site, *lead, 100 ).valid );

    const int report_revision = site.camp_decision.source_report_revision;
    const int report_generation = site.camp_decision.source_report_generation;
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 site, bandit_live_world::camp_decision_state::report_awaiting_assessment,
                 bandit_live_world::camp_decision_state::cooldown,
                 report_revision, report_generation, 0, 0,
                 "reservation test report assessed" ) ==
             bandit_live_world::camp_decision_transition_result::applied );
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 site, bandit_live_world::camp_decision_state::cooldown,
                 bandit_live_world::camp_decision_state::idle,
                 report_revision, report_generation, 0, -1,
                 "reservation test cooldown elapsed" ) ==
             bandit_live_world::camp_decision_transition_result::applied );
    REQUIRE( site.next_outing_generation == 2 );
    const std::string after_first_resolution = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan(
                     site, stale_structural_plan, 100 ) );
    CHECK( serialize_world( world ) == after_first_resolution );

    lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    const bandit_live_world::structural_outing_plan fresh_structural_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( fresh_structural_plan.valid );
    CHECK( fresh_structural_plan.generation == 2 );
    const bandit_live_world::dispatch_plan competing_dispatch =
        bandit_live_world::plan_site_dispatch(
            site, tripoint_abs_omt( 19, 20, 0 ), "reservation-competing-target" );
    REQUIRE( competing_dispatch.valid );
    CHECK( competing_dispatch.entry.activity_generation == fresh_structural_plan.generation );

    bandit_live_world::structural_outing_plan forged_route_plan = fresh_structural_plan;
    REQUIRE( forged_route_plan.shared_route.size() == 4 );
    forged_route_plan.shared_route[1] = tripoint_abs_omt( 8, 21, 0 );
    const std::string before_forged_route_plan = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan(
                     site, forged_route_plan, 100 ) );
    CHECK( serialize_world( world ) == before_forged_route_plan );

    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan(
                 site, fresh_structural_plan, 100 ) );
    REQUIRE( site.roster().valid );
    CHECK( site.active_outing.schema_version == 8 );
    CHECK( site.active_outing.activity_id == fresh_structural_plan.activity_id );
    CHECK( site.active_outing.generation == fresh_structural_plan.generation );
    CHECK( site.active_outing.member_ids == fresh_structural_plan.member_ids );
    CHECK( site.active_outing.shared_route == fresh_structural_plan.shared_route );
    CHECK( site.active_outing.waypoint_index == 0 );
    CHECK( site.active_outing.expected_return_minutes ==
           fresh_structural_plan.expected_return_minutes );
    CHECK( site.active_outing.missing_deadline_minutes ==
           fresh_structural_plan.expected_return_minutes + 24 * 60 );
    CHECK( site.roster().reserved_unresolved_ids == fresh_structural_plan.member_ids );
    CHECK( site.next_outing_generation == 3 );
    const std::string structural_reservation = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( site, competing_dispatch ) );
    CHECK( serialize_world( world ) == structural_reservation );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    REQUIRE( loaded.sites.front().roster().valid );
    CHECK( loaded.sites.front().active_outing.activity_id ==
           fresh_structural_plan.activity_id );
    CHECK( loaded.sites.front().active_outing.generation ==
           fresh_structural_plan.generation );
    CHECK( loaded.sites.front().active_outing.member_ids ==
           fresh_structural_plan.member_ids );
    CHECK( loaded.sites.front().active_outing.shared_route ==
           fresh_structural_plan.shared_route );
    CHECK( loaded.sites.front().active_outing.waypoint_index == 0 );
    CHECK( loaded.sites.front().roster().reserved_unresolved_ids ==
           fresh_structural_plan.member_ids );

    bandit_live_world::world_state forged = loaded;
    bandit_live_world::site_record &forged_site = forged.sites.front();
    const character_id forged_home_member = forged_site.active_outing.member_ids.back();
    forged_site.active_outing.member_ids.pop_back();
    REQUIRE( forged_site.find_member( forged_home_member ) != nullptr );
    forged_site.find_member( forged_home_member )->state = bandit_live_world::member_state::at_home;
    REQUIRE( forged_site.roster().valid );
    const std::string forged_bytes = serialize_world( forged );
    JsonValue forged_json = json_loader::from_string( forged_bytes );
    bandit_live_world::world_state protected_world = loaded;
    const std::string protected_bytes = serialize_world( protected_world );
    CHECK_THROWS( protected_world.deserialize( forged_json.get_object() ) );
    CHECK( serialize_world( protected_world ) == protected_bytes );

    bandit_live_world::world_state malformed_route = loaded;
    malformed_route.sites.front().active_outing.shared_route[1] =
        tripoint_abs_omt( 8, 21, 0 );
    JsonValue malformed_route_json = json_loader::from_string(
                                         serialize_world( malformed_route ) );
    CHECK_THROWS( protected_world.deserialize( malformed_route_json.get_object() ) );
    CHECK( serialize_world( protected_world ) == protected_bytes );

    bandit_live_world::world_state malformed_route_clock = loaded;
    malformed_route_clock.sites.front().active_outing.expected_return_minutes++;
    JsonValue malformed_route_clock_json = json_loader::from_string(
            serialize_world( malformed_route_clock ) );
    CHECK_THROWS( protected_world.deserialize( malformed_route_clock_json.get_object() ) );
    CHECK( serialize_world( protected_world ) == protected_bytes );

    bandit_live_world::world_state legacy_route_less = loaded;
    bandit_live_world::active_outing_state &legacy_outing =
        legacy_route_less.sites.front().active_outing;
    legacy_outing.schema_version = 5;
    legacy_outing.shared_route.clear();
    legacy_outing.waypoint_index = 0;
    legacy_outing.expected_return_minutes = -1;
    legacy_outing.missing_deadline_minutes = -1;
    const bandit_live_world::world_state migrated_route = round_trip_world( legacy_route_less );
    CHECK( migrated_route.sites.front().active_outing.schema_version == 6 );
    CHECK( migrated_route.sites.front().active_outing.shared_route ==
           fresh_structural_plan.shared_route );
    CHECK( migrated_route.sites.front().active_outing.waypoint_index == 0 );
    CHECK( migrated_route.sites.front().active_outing.expected_return_minutes ==
           fresh_structural_plan.expected_return_minutes );

    const std::string before_stale_identity_release = serialize_world( world );
    CHECK_FALSE( bandit_live_world::release_structural_outing_reservation(
                     site, fresh_structural_plan.activity_id + ":stale",
                     fresh_structural_plan.generation, "stale identity cleanup" ) );
    CHECK( serialize_world( world ) == before_stale_identity_release );
    CHECK_FALSE( bandit_live_world::release_structural_outing_reservation(
                     site, fresh_structural_plan.activity_id,
                     fresh_structural_plan.generation + 1, "stale generation cleanup" ) );
    CHECK( serialize_world( world ) == before_stale_identity_release );

    const character_id resolved_casualty = fresh_structural_plan.member_ids.front();
    REQUIRE( bandit_live_world::record_matching_external_outing_casualty(
                 site, fresh_structural_plan.activity_id, fresh_structural_plan.generation,
                 resolved_casualty, bandit_live_world::member_state::dead, 101,
                 "resolved structural casualty" ) );
    REQUIRE( site.roster().valid );
    const std::optional<int> returned =
        bandit_live_world::release_structural_outing_reservation(
            site, fresh_structural_plan.activity_id,
            fresh_structural_plan.generation, "matching structural cleanup" );
    REQUIRE( returned );
    CHECK( *returned == 1 );
    CHECK_FALSE( site.active_outing.is_active() );
    CHECK( site.active_outing.member_ids.empty() );
    REQUIRE( site.roster().valid );
    CHECK( site.roster().reserved_unresolved_ids.empty() );
    CHECK( site.roster().ready_concrete_total == 2 );
    CHECK( site.find_member( resolved_casualty )->state ==
           bandit_live_world::member_state::dead );

    const tripoint_abs_omt newer_structural_omt( 7, 20, 0 );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                 site, newer_structural_omt, structural_read, 0 ) );
    const std::string newer_lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                          site.site_id, newer_structural_omt, "forest" );
    const bandit_live_world::camp_map_lead *newer_lead =
        site.intelligence_map.find_lead( newer_lead_id );
    REQUIRE( newer_lead != nullptr );
    const bandit_live_world::structural_outing_plan newer_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *newer_lead, 100 );
    REQUIRE( newer_plan.valid );
    REQUIRE( newer_plan.activity_id == fresh_structural_plan.activity_id );
    REQUIRE( newer_plan.generation == 3 );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, newer_plan, 100 ) );
    const std::string newer_reservation = serialize_world( world );
    CHECK_FALSE( bandit_live_world::release_structural_outing_reservation(
                     site, fresh_structural_plan.activity_id,
                     fresh_structural_plan.generation, "stale cleanup after redispatch" ) );
    CHECK( serialize_world( world ) == newer_reservation );
    CHECK( site.active_outing.activity_id == newer_plan.activity_id );
    CHECK( site.active_outing.generation == newer_plan.generation );
    CHECK( site.roster().reserved_unresolved_ids == newer_plan.member_ids );
}

TEST_CASE( "hostile_camp_local_handoff_binds_the_complete_pair_transactionally",
           "[bandit][live_world][structural_bounty][local_handoff][save]" )
{
    const auto make_world = []( const bool cannibal ) {
        bandit_live_world::world_state world;
        const int id_base = cannibal ? 14580 : 14560;
        for( int index = 0; index < 3; ++index ) {
            if( cannibal ) {
                add_cannibal_camp_member( world, index, id_base );
            } else {
                add_bandit_camp_member( world, index, id_base );
            }
        }
        bandit_live_world::site_record &site = world.sites.front();
        const tripoint_abs_omt target( site.anchor.x() + 4, site.anchor.y(), site.anchor.z() );
        const bandit_live_world::structural_bounty_read read =
            bandit_live_world::classify_structural_bounty_terrain( "forest" );
        REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, target, read, 0 ) );
        const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id(
                                        site.site_id, target, "forest" );
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        const bandit_live_world::structural_outing_plan plan =
            bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );
        REQUIRE( site.active_outing.schema_version == 8 );
        return world;
    };
    const auto make_reads = []( const bandit_live_world::site_record &site ) {
        std::vector<bandit_live_world::local_handoff_member_read> reads;
        const tripoint_abs_omt route_position = site.active_outing.shared_route[
                    static_cast<std::size_t>( site.active_outing.waypoint_index )];
        const tripoint_abs_ms origin = project_to<coords::ms>( route_position );
        for( std::size_t index = 0; index < site.active_outing.member_ids.size(); ++index ) {
            const character_id member_id = site.active_outing.member_ids[index];
            const bandit_live_world::member_record *member = site.find_member( member_id );
            REQUIRE( member != nullptr );
            bandit_live_world::local_handoff_member_read read;
            read.npc_id = member_id;
            read.bindable = true;
            read.hp_percent = index == 0 ? 92 : 73;
            read.current_position = member->home_spawn_tile;
            read.entry_position = tripoint_abs_ms( origin.x() + static_cast<int>( index ),
                                                   origin.y(), origin.z() );
            read.staging_position = tripoint_abs_ms( origin.x() + static_cast<int>( index ),
                                                     origin.y() + 4, origin.z() );
            reads.push_back( read );
        }
        return reads;
    };
    const auto assemble_pair = []( bandit_live_world::site_record &site, const int minute ) {
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        std::vector<bandit_live_world::local_cohesion_member_read> reads;
        for( const bandit_live_world::local_handoff_member_snapshot &member :
             site.active_outing.local_handoff.members ) {
            bandit_live_world::local_cohesion_member_read read;
            read.npc_id = member.npc_id;
            read.dead = member.dead;
            read.present = !member.dead;
            read.current_position = member.dead ? member.exit_position : member.staging_position;
            reads.push_back( read );
        }
        const bandit_live_world::local_cohesion_plan plan =
            bandit_live_world::plan_local_pair_cohesion( site, *cursor, minute, reads );
        REQUIRE( plan.valid );
        REQUIRE( plan.snapshot.cohesion_assembled );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, plan, false, false ) );
    };

    SECTION( "bandit and cannibal pairs commit once and round trip" ) {
        for( const bool cannibal : { false, true } ) {
            bandit_live_world::world_state world = make_world( cannibal );
            bandit_live_world::site_record &site = world.sites.front();
            const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
                bandit_live_world::current_external_simulation_cursor( site );
            REQUIRE( cursor );
            const std::vector<bandit_live_world::local_handoff_member_read> reads =
                make_reads( site );
            const bandit_live_world::local_handoff_plan plan =
                bandit_live_world::plan_local_pair_handoff( site, *cursor, 100, reads );
            REQUIRE( plan.valid );
            REQUIRE( plan.snapshot.members.size() == 2 );
            CHECK( plan.snapshot.activity_id == site.active_outing.activity_id );
            CHECK( plan.snapshot.activity_generation == site.active_outing.generation );
            CHECK( plan.snapshot.handoff_epoch == 1 );
            CHECK( plan.snapshot.phase == site.active_outing.phase );
            CHECK( plan.snapshot.route_position == site.active_outing.shared_route.front() );
            CHECK( plan.snapshot.approach_from == plan.snapshot.route_position );
            CHECK( plan.snapshot.egress_omt == site.active_outing.shared_route[1] );
            CHECK( plan.snapshot.members[0].hp_percent == 92 );
            CHECK( plan.snapshot.members[1].hp_percent == 73 );
            CHECK_FALSE( plan.snapshot.cohesion_assembled );
            CHECK( plan.snapshot.cohesion_deadline_minutes == -1 );

            int bind_calls = 0;
            int rollback_calls = 0;
            const bandit_live_world::local_handoff_commit_result committed =
                bandit_live_world::commit_local_pair_handoff(
                    site, plan,
            [&bind_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
                bind_calls++;
                return true;
            }, [&rollback_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
                rollback_calls++;
            } );
            REQUIRE( committed == bandit_live_world::local_handoff_commit_result::applied );
            CHECK( bind_calls == 2 );
            CHECK( rollback_calls == 0 );
            CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
            CHECK( site.active_outing.handoff_epoch == 1 );
            CHECK( site.active_outing.local_handoff.is_active() );
            CHECK( bandit_live_world::advance_external_simulation(
                       site, cursor->activity_id, cursor->generation,
                       bandit_live_world::simulation_owner::abstract,
                       cursor->handoff_epoch, cursor->last_advanced_minutes, 101 ) ==
                   bandit_live_world::simulation_owner_transition_result::rejected );

            const std::string committed_bytes = serialize_world( world );
            bandit_live_world::world_state loaded = round_trip_world( world );
            CHECK( serialize_world( loaded ) == committed_bytes );
            bandit_live_world::site_record &loaded_site = loaded.sites.front();
            CHECK( bandit_live_world::commit_local_pair_handoff(
                       loaded_site, plan,
            [&bind_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
                bind_calls++;
                return true;
            }, [&rollback_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
                rollback_calls++;
            } ) == bandit_live_world::local_handoff_commit_result::unchanged );
            CHECK( bind_calls == 2 );
            CHECK( rollback_calls == 0 );
        }
    }

    SECTION( "a second-member bind failure rolls back every touched member" ) {
        bandit_live_world::world_state world = make_world( false );
        bandit_live_world::site_record &site = world.sites.front();
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        const bandit_live_world::local_handoff_plan plan =
            bandit_live_world::plan_local_pair_handoff( site, *cursor, 100,
                    make_reads( site ) );
        REQUIRE( plan.valid );
        const std::string before = serialize_world( world );
        int bind_calls = 0;
        std::vector<character_id> rollback_ids;
        CHECK( bandit_live_world::commit_local_pair_handoff(
                   site, plan,
        [&bind_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
            bind_calls++;
            return bind_calls == 1;
        }, [&rollback_ids]( const bandit_live_world::local_handoff_member_snapshot & member ) {
            rollback_ids.push_back( member.npc_id );
        } ) == bandit_live_world::local_handoff_commit_result::rolled_back );
        CHECK( bind_calls == 2 );
        REQUIRE( rollback_ids.size() == 2 );
        CHECK( rollback_ids[0] == plan.snapshot.members[1].npc_id );
        CHECK( rollback_ids[1] == plan.snapshot.members[0].npc_id );
        CHECK( serialize_world( world ) == before );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
        CHECK( site.active_outing.handoff_epoch == 0 );
    }

    SECTION( "duplicate entry tiles and stale cursors reject without mutation" ) {
        bandit_live_world::world_state world = make_world( false );
        bandit_live_world::site_record &site = world.sites.front();
        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        std::vector<bandit_live_world::local_handoff_member_read> duplicate = make_reads( site );
        REQUIRE( duplicate.size() == 2 );
        duplicate[1].entry_position = duplicate[0].entry_position;
        CHECK_FALSE( bandit_live_world::plan_local_pair_handoff(
                         site, *cursor, 100, duplicate ).valid );

        bandit_live_world::simulation_advance_cursor stale = *cursor;
        stale.last_advanced_minutes--;
        CHECK_FALSE( bandit_live_world::plan_local_pair_handoff(
                         site, stale, 100, make_reads( site ) ).valid );
    }

    SECTION( "arrival waits for every survivor at staging and replay is inert" ) {
        bandit_live_world::world_state world = make_world( false );
        bandit_live_world::site_record &site = world.sites.front();
        const std::optional<bandit_live_world::simulation_advance_cursor> abstract_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( abstract_cursor );
        const bandit_live_world::local_handoff_plan handoff =
            bandit_live_world::plan_local_pair_handoff(
                site, *abstract_cursor, 100, make_reads( site ) );
        REQUIRE( handoff.valid );
        REQUIRE( bandit_live_world::commit_local_pair_handoff(
                     site, handoff,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );

        std::vector<bandit_live_world::local_cohesion_member_read> reads;
        for( std::size_t index = 0; index < site.active_outing.local_handoff.members.size(); ++index ) {
            const bandit_live_world::local_handoff_member_snapshot &member =
                site.active_outing.local_handoff.members[index];
            bandit_live_world::local_cohesion_member_read read;
            read.npc_id = member.npc_id;
            read.present = index != 0;
            read.current_position = index == 0 ? member.entry_position : member.staging_position;
            reads.push_back( read );
        }
        std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        bandit_live_world::local_cohesion_plan cohesion =
            bandit_live_world::plan_local_pair_cohesion( site, *cursor, 101, reads );
        REQUIRE( cohesion.valid );
        CHECK_FALSE( cohesion.snapshot.cohesion_assembled );
        CHECK( cohesion.leader_id == site.active_outing.leader_id );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, false, false ) );
        const std::string first_member_only = serialize_world( world );
        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   world, 102, {} ).active_outings_considered == 1 );
        CHECK( serialize_world( world ) == first_member_only );

        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 101, reads );
        REQUIRE( cohesion.valid );
        CHECK_FALSE( bandit_live_world::commit_local_pair_cohesion(
                         site, cohesion, false, false ) );
        CHECK( serialize_world( world ) == first_member_only );

        reads[0].present = true;
        reads[0].current_position = site.active_outing.local_handoff.members[0].staging_position;
        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 102, reads );
        REQUIRE( cohesion.valid );
        REQUIRE( cohesion.snapshot.cohesion_assembled );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, false, false ) );
        const std::string assembled = serialize_world( world );
        CHECK( serialize_world( round_trip_world( world ) ) == assembled );
    }

    SECTION( "separation has a fresh timeout and two failed routes abort coherently" ) {
        bandit_live_world::world_state world = make_world( true );
        bandit_live_world::site_record &site = world.sites.front();
        const std::optional<bandit_live_world::simulation_advance_cursor> abstract_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( abstract_cursor );
        const bandit_live_world::local_handoff_plan handoff =
            bandit_live_world::plan_local_pair_handoff(
                site, *abstract_cursor, 100, make_reads( site ) );
        REQUIRE( handoff.valid );
        REQUIRE( bandit_live_world::commit_local_pair_handoff(
                     site, handoff,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );

        std::vector<bandit_live_world::local_cohesion_member_read> reads;
        for( const bandit_live_world::local_handoff_member_snapshot &member :
             site.active_outing.local_handoff.members ) {
            bandit_live_world::local_cohesion_member_read read;
            read.npc_id = member.npc_id;
            read.present = true;
            read.current_position = member.entry_position;
            reads.push_back( read );
        }
        std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        bandit_live_world::local_cohesion_plan cohesion =
            bandit_live_world::plan_local_pair_cohesion( site, *cursor, 100, reads );
        REQUIRE( cohesion.valid );
        CHECK_FALSE( cohesion.snapshot.cohesion_assembled );
        CHECK( cohesion.snapshot.cohesion_deadline_minutes == 110 );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, false, false ) );

        for( std::size_t index = 0; index < reads.size(); ++index ) {
            reads[index].current_position =
                site.active_outing.local_handoff.members[index].staging_position;
        }
        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 101, reads );
        REQUIRE( cohesion.valid );
        REQUIRE( cohesion.snapshot.cohesion_assembled );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, false, false ) );

        reads[1].current_position = reads[0].current_position + point( 10, 0 );
        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 200, reads );
        REQUIRE( cohesion.valid );
        CHECK( cohesion.snapshot.cohesion_deadline_minutes == 210 );
        REQUIRE( cohesion.reroute_needed );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, true, true ) );
        CHECK( site.active_outing.local_handoff.cohesion_reroutes_used == 1 );
        CHECK_FALSE( site.active_outing.local_handoff.cohesion_abort_return );

        reads[1].current_position = site.active_outing.local_handoff.members[1].staging_position;
        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 201, reads );
        REQUIRE( cohesion.valid );
        REQUIRE( cohesion.snapshot.cohesion_assembled );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, false, false ) );
        CHECK( site.active_outing.local_handoff.cohesion_deadline_minutes == -1 );
        CHECK( site.active_outing.local_handoff.cohesion_reroutes_used == 0 );

        reads[1].current_position = reads[0].current_position + point( 10, 0 );
        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 300, reads );
        REQUIRE( cohesion.valid );
        CHECK( cohesion.snapshot.cohesion_deadline_minutes == 310 );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, true, true ) );
        cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        cohesion = bandit_live_world::plan_local_pair_cohesion( site, *cursor, 301, reads );
        REQUIRE( cohesion.valid );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, true, true ) );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
        CHECK( site.active_outing.local_handoff.cohesion_abort_return );
        CHECK_FALSE( site.active_outing.local_handoff.cohesion_assembled );
    }

    SECTION( "a complete local pair snapshots out and abstract work resumes once" ) {
        bandit_live_world::world_state world = make_world( false );
        bandit_live_world::site_record &site = world.sites.front();
        const std::optional<bandit_live_world::simulation_advance_cursor> abstract_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( abstract_cursor );
        const bandit_live_world::local_handoff_plan handoff =
            bandit_live_world::plan_local_pair_handoff( site, *abstract_cursor, 100,
                    make_reads( site ) );
        REQUIRE( handoff.valid );
        REQUIRE( bandit_live_world::commit_local_pair_handoff(
                     site, handoff,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );
        assemble_pair( site, 100 );
        const std::optional<bandit_live_world::simulation_advance_cursor> local_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( local_cursor );
        CHECK( bandit_live_world::transition_external_simulation_owner(
                   site, local_cursor->activity_id, local_cursor->generation,
                   bandit_live_world::simulation_owner::local,
                   bandit_live_world::simulation_owner::abstract,
                   local_cursor->handoff_epoch, local_cursor->last_advanced_minutes, 101 ) ==
               bandit_live_world::simulation_owner_transition_result::rejected );

        std::vector<bandit_live_world::local_dematerialization_member_read> reads;
        for( std::size_t index = 0; index < site.active_outing.local_handoff.members.size(); ++index ) {
            const bandit_live_world::local_handoff_member_snapshot &member =
                site.active_outing.local_handoff.members[index];
            bandit_live_world::local_dematerialization_member_read read;
            read.npc_id = member.npc_id;
            read.readable = true;
            read.hp_percent = index == 0 ? 61 : 42;
            read.current_position = member.staging_position;
            reads.push_back( read );
        }
        bandit_live_world::sortie_cargo cargo;
        cargo.supply_units = 3;
        cargo.trade_value = 400;
        const bandit_live_world::local_dematerialization_plan dematerialization =
            bandit_live_world::plan_local_pair_dematerialization(
                site, *local_cursor, 101, reads, cargo );
        REQUIRE( dematerialization.valid );
        REQUIRE( dematerialization.resume_snapshot.is_abstract_resume() );
        int quiesce_calls = 0;
        int rollback_calls = 0;
        REQUIRE( bandit_live_world::commit_local_pair_dematerialization(
                     site, dematerialization,
        [&quiesce_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
            quiesce_calls++;
            return true;
        }, [&rollback_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
            rollback_calls++;
        } ) == bandit_live_world::local_handoff_commit_result::applied );
        CHECK( quiesce_calls == 2 );
        CHECK( rollback_calls == 0 );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
        CHECK( site.active_outing.handoff_epoch == 2 );
        CHECK( site.active_outing.local_handoff.is_abstract_resume() );
        CHECK( site.active_outing.cargo.supply_units == 3 );
        CHECK( site.active_outing.cargo.trade_value == 400 );
        CHECK_FALSE( site.find_member( reads[0].npc_id )->wounded_or_unready );
        CHECK( site.find_member( reads[1].npc_id )->wounded_or_unready );

        const std::string resumed_bytes = serialize_world( world );
        bandit_live_world::world_state loaded = round_trip_world( world );
        CHECK( serialize_world( loaded ) == resumed_bytes );
        bandit_live_world::site_record &loaded_site = loaded.sites.front();
        CHECK( bandit_live_world::commit_local_pair_dematerialization(
                   loaded_site, dematerialization,
        [&quiesce_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
            quiesce_calls++;
            return true;
        }, [&rollback_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
            rollback_calls++;
        } ) == bandit_live_world::local_handoff_commit_result::unchanged );
        CHECK( quiesce_calls == 2 );
        CHECK( rollback_calls == 0 );

        CHECK( bandit_live_world::advance_structural_bounty_outings(
                   loaded, 101, {} ).active_outings_considered == 1 );
        CHECK( serialize_world( loaded ) == resumed_bytes );
        const bandit_live_world::structural_outing_result resumed =
            bandit_live_world::advance_structural_bounty_outings( loaded, 102, {} );
        CHECK( resumed.active_outings_considered == 1 );
        CHECK( loaded_site.active_outing.last_advanced_minutes == 102 );
        CHECK_FALSE( loaded_site.active_outing.local_handoff.is_abstract_resume() );
        CHECK( loaded_site.active_outing.local_handoff.activity_id.empty() );
        const std::string advanced_once = serialize_world( loaded );
        bandit_live_world::advance_structural_bounty_outings( loaded, 102, {} );
        CHECK( serialize_world( loaded ) == advanced_once );
    }

    SECTION( "partial reads and partial quiesce leave the local owner byte identical" ) {
        bandit_live_world::world_state world = make_world( false );
        bandit_live_world::site_record &site = world.sites.front();
        const std::optional<bandit_live_world::simulation_advance_cursor> abstract_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( abstract_cursor );
        const bandit_live_world::local_handoff_plan handoff =
            bandit_live_world::plan_local_pair_handoff( site, *abstract_cursor, 100,
                    make_reads( site ) );
        REQUIRE( handoff.valid );
        REQUIRE( bandit_live_world::commit_local_pair_handoff(
                     site, handoff,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );
        assemble_pair( site, 100 );
        const std::optional<bandit_live_world::simulation_advance_cursor> local_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( local_cursor );
        std::vector<bandit_live_world::local_dematerialization_member_read> reads;
        for( const bandit_live_world::local_handoff_member_snapshot &member :
             site.active_outing.local_handoff.members ) {
            bandit_live_world::local_dematerialization_member_read read;
            read.npc_id = member.npc_id;
            read.readable = true;
            read.hp_percent = 80;
            read.current_position = member.staging_position;
            reads.push_back( read );
        }
        std::vector<bandit_live_world::local_dematerialization_member_read> partial = reads;
        partial.pop_back();
        const std::string local_bytes = serialize_world( world );
        CHECK_FALSE( bandit_live_world::plan_local_pair_dematerialization(
                         site, *local_cursor, 101, partial, site.active_outing.cargo ).valid );
        CHECK( serialize_world( world ) == local_bytes );

        const bandit_live_world::local_dematerialization_plan plan =
            bandit_live_world::plan_local_pair_dematerialization(
                site, *local_cursor, 101, reads, site.active_outing.cargo );
        REQUIRE( plan.valid );
        int quiesce_calls = 0;
        std::vector<character_id> rollback_ids;
        CHECK( bandit_live_world::commit_local_pair_dematerialization(
                   site, plan,
        [&quiesce_calls]( const bandit_live_world::local_handoff_member_snapshot & ) {
            quiesce_calls++;
            return quiesce_calls == 1;
        }, [&rollback_ids]( const bandit_live_world::local_handoff_member_snapshot & member ) {
            rollback_ids.push_back( member.npc_id );
        } ) == bandit_live_world::local_handoff_commit_result::rolled_back );
        CHECK( quiesce_calls == 2 );
        REQUIRE( rollback_ids.size() == 2 );
        CHECK( rollback_ids[0] == plan.resume_snapshot.members[1].npc_id );
        CHECK( rollback_ids[1] == plan.resume_snapshot.members[0].npc_id );
        CHECK( serialize_world( world ) == local_bytes );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
    }

    SECTION( "physical death and cargo are written once before abstract ownership" ) {
        bandit_live_world::world_state world = make_world( true );
        bandit_live_world::site_record &site = world.sites.front();
        const int living_before = site.living_total;
        const std::optional<bandit_live_world::simulation_advance_cursor> abstract_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( abstract_cursor );
        const bandit_live_world::local_handoff_plan handoff =
            bandit_live_world::plan_local_pair_handoff( site, *abstract_cursor, 100,
                    make_reads( site ) );
        REQUIRE( handoff.valid );
        REQUIRE( bandit_live_world::commit_local_pair_handoff(
                     site, handoff,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );
        const std::optional<bandit_live_world::simulation_advance_cursor> local_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( local_cursor );
        const character_id dead_member_id = site.active_outing.local_handoff.members[0].npc_id;
        const character_id surviving_member_id = site.active_outing.local_handoff.members[1].npc_id;
        const tripoint_abs_ms death_position =
            site.active_outing.local_handoff.members[0].entry_position;
        REQUIRE( bandit_live_world::record_local_pair_member_death(
                     site, *local_cursor, dead_member_id, death_position, 101 ) );
        CHECK_FALSE( bandit_live_world::record_local_pair_member_death(
                         site, *local_cursor, dead_member_id, death_position, 101 ) );
        CHECK( site.living_total == living_before - 1 );
        CHECK( site.active_outing.leader_id == surviving_member_id );
        CHECK( site.active_outing.local_handoff.cohesion_leader_id == surviving_member_id );
        assemble_pair( site, 101 );
        const std::optional<bandit_live_world::simulation_advance_cursor> post_death_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( post_death_cursor );
        std::vector<bandit_live_world::local_dematerialization_member_read> reads;
        for( std::size_t index = 0; index < site.active_outing.local_handoff.members.size(); ++index ) {
            const bandit_live_world::local_handoff_member_snapshot &member =
                site.active_outing.local_handoff.members[index];
            bandit_live_world::local_dematerialization_member_read read;
            read.npc_id = member.npc_id;
            read.readable = true;
            read.dead = member.dead;
            read.hp_percent = read.dead ? 0 : 75;
            read.current_position = member.dead ? member.exit_position : member.staging_position;
            reads.push_back( read );
        }
        bandit_live_world::sortie_cargo cargo;
        cargo.supply_units = 2;
        const bandit_live_world::local_dematerialization_plan plan =
            bandit_live_world::plan_local_pair_dematerialization(
                site, *post_death_cursor, 102, reads, cargo );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::commit_local_pair_dematerialization(
                     site, plan,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );
        CHECK( site.active_outing.casualty_ids == std::vector<character_id>{ dead_member_id } );
        CHECK( site.active_outing.member_is_resolved( dead_member_id ) );
        CHECK( site.find_member( dead_member_id )->state ==
               bandit_live_world::member_state::dead );
        CHECK( site.active_outing.cargo.supply_units == 2 );
        const std::string committed = serialize_world( world );
        CHECK( serialize_world( round_trip_world( world ) ) == committed );
    }
}

TEST_CASE( "bandit_live_world_releases_every_matching_external_owner_without_resurrecting_losses",
           "[bandit][live_world][reservation][release_paths]" )
{
    SECTION( "ordinary abort and death release only the captured generation" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 3; ++index ) {
            add_bandit_camp_member( world, index, 14500 );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const bandit_live_world::dispatch_plan plan =
            bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 18, 20, 0 ), "release-path-target" );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
        const std::string activity_id = site.active_outing.activity_id;
        const int generation = site.active_outing.generation;
        const std::string before_stale_release = serialize_world( world );
        CHECK_FALSE( bandit_live_world::release_matching_external_reservation(
                         site, activity_id + ":stale", generation, "stale ordinary abort" ) );
        CHECK_FALSE( bandit_live_world::release_matching_external_reservation(
                         site, activity_id, generation + 1, "future ordinary abort" ) );
        CHECK( serialize_world( world ) == before_stale_release );

        const character_id casualty_id = plan.member_ids.front();
        CHECK_FALSE( bandit_live_world::record_matching_external_outing_casualty(
                         site, activity_id, generation + 1, casualty_id,
                         bandit_live_world::member_state::dead, 1,
                         "stale ordinary casualty" ) );
        CHECK_FALSE( bandit_live_world::record_matching_external_outing_casualty(
                         site, activity_id, generation, casualty_id,
                         bandit_live_world::member_state::missing, 1,
                         "premature ordinary missing resolution" ) );
        CHECK( serialize_world( world ) == before_stale_release );
        REQUIRE( bandit_live_world::record_matching_external_outing_casualty(
                     site, activity_id, generation, casualty_id,
                     bandit_live_world::member_state::dead, 1,
                     "matching ordinary casualty" ) );
        const std::optional<int> released =
            bandit_live_world::release_matching_external_reservation(
                site, activity_id, generation, "matching ordinary abort" );
        REQUIRE( released );
        CHECK( *released == 1 );
        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.find_member( casualty_id )->state ==
               bandit_live_world::member_state::dead );
        CHECK( site.applied_return_generation == generation );
        REQUIRE( site.roster().valid );
        CHECK( site.roster().reserved_unresolved_ids.empty() );

        const bandit_live_world::dispatch_plan newer_plan =
            bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 19, 20, 0 ), "newer-release-path-target" );
        REQUIRE( newer_plan.valid );
        REQUIRE( bandit_live_world::apply_dispatch_plan( site, newer_plan ) );
        const std::string newer_bytes = serialize_world( world );
        CHECK_FALSE( bandit_live_world::release_matching_external_reservation(
                         site, activity_id, generation, "stale cleanup after ordinary redispatch" ) );
        CHECK( serialize_world( world ) == newer_bytes );
    }

    SECTION( "hostile return closes its operation and camp mission slot" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 7; ++index ) {
            add_bandit_camp_member( world, index, 14550 );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const tripoint_abs_omt target( 18, 20, 0 );
        prepare_hostile_follow_on( site, 7, 4, "hostile-release-target", target, 600 );
        const bandit_live_world::hostile_operation_plan plan =
            bandit_live_world::plan_hostile_operation(
                site, bandit_live_world::hostile_operation_kind::shakedown,
                { site.anchor, tripoint_abs_omt( 14, 20, 0 ), target },
                tripoint_abs_omt( 14, 20, 0 ), 602 );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
        REQUIRE( transition_test_hostile_operation(
                     site, bandit_live_world::hostile_operation_phase::assembling,
                     bandit_live_world::hostile_operation_phase::outbound, 603,
                     "release-path hostile departure" ) ==
                 bandit_live_world::hostile_operation_transition_result::applied );
        REQUIRE( transition_test_hostile_operation(
                     site, bandit_live_world::hostile_operation_phase::outbound,
                     bandit_live_world::hostile_operation_phase::returning_home, 604,
                     "release-path hostile return" ) ==
                 bandit_live_world::hostile_operation_transition_result::applied );
        const std::string activity_id =
            site.active_hostile_operation.reservation.activity_id;
        const int generation = site.active_hostile_operation.reservation.generation;
        const int party_size = static_cast<int>(
                                   site.active_hostile_operation.reservation.member_ids.size() );
        const std::string before_stale_release = serialize_world( world );
        CHECK_FALSE( bandit_live_world::release_matching_external_reservation(
                         site, activity_id, generation - 1, "stale hostile return" ) );
        CHECK( serialize_world( world ) == before_stale_release );

        const std::optional<int> released =
            bandit_live_world::release_matching_external_reservation(
                site, activity_id, generation, "matching hostile return" );
        REQUIRE( released );
        CHECK( *released == party_size );
        CHECK_FALSE( site.active_hostile_operation.is_active() );
        CHECK( site.active_external_outing() == nullptr );
        CHECK( site.camp_decision.state ==
               bandit_live_world::camp_decision_state::abandoned );
        CHECK( site.applied_return_generation == generation );
        REQUIRE( site.roster().valid );
        CHECK( site.roster().reserved_unresolved_ids.empty() );
    }

    SECTION( "one returned scout and one later casualty is not an all-lost party" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 3; ++index ) {
            add_bandit_camp_member( world, index, 14525 );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const bandit_live_world::dispatch_plan plan =
            bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 18, 20, 0 ), "split-release-target" );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
        REQUIRE( bandit_live_world::transition_external_simulation_owner(
                     site, site.active_outing.activity_id, site.active_outing.generation,
                     bandit_live_world::simulation_owner::abstract,
                     bandit_live_world::simulation_owner::local,
                     site.active_outing.handoff_epoch,
                     site.active_outing.last_advanced_minutes, 0 ) ==
                 bandit_live_world::simulation_owner_transition_result::applied );
        const std::vector<bandit_live_world::active_member_observation> split = {
            { plan.member_ids.front(),
              bandit_live_world::active_member_observation_state::home,
              "first scout returned" },
            { plan.member_ids.back(),
              bandit_live_world::active_member_observation_state::unresolved,
              "second scout still away" }
        };
        const bandit_live_world::scout_resolution_effect partial =
            bandit_live_world::apply_active_scout_observations(
                site, require_current_simulation_cursor( site ), split, 1 );
        REQUIRE( partial.valid );
        REQUIRE( partial.changed );
        REQUIRE( partial.provisional_report_applied );
        REQUIRE( site.current_scout_report.provisional );
        const bandit_live_world::scout_phase phase_before_casualty =
            site.active_outing.phase;
        REQUIRE( bandit_live_world::record_active_outing_casualty(
                     site, require_current_simulation_cursor( site ), plan.member_ids.back(),
                     bandit_live_world::member_state::dead, 2,
                     "second scout died after split return" ) );
        CHECK( site.active_outing.phase == phase_before_casualty );
        CHECK( site.active_outing.casualty_ids.size() == 1 );
        CHECK( site.active_outing.resolved_member_ids.size() == 2 );

        const std::vector<bandit_live_world::active_member_observation> resolved = {
            { plan.member_ids.front(),
              bandit_live_world::active_member_observation_state::home,
              "first scout remains home" },
            { plan.member_ids.back(),
              bandit_live_world::active_member_observation_state::dead,
              "second scout casualty confirmed" }
        };
        const bandit_live_world::scout_resolution_effect completed =
            bandit_live_world::apply_active_scout_observations(
                site, require_current_simulation_cursor( site ), resolved, 3 );
        REQUIRE( completed.valid );
        CHECK( completed.completed );
        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.find_member( plan.member_ids.front() )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( site.find_member( plan.member_ids.back() )->state ==
               bandit_live_world::member_state::dead );
    }

    SECTION( "hostile all-dead resolution clears without returning casualties" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 7; ++index ) {
            add_bandit_camp_member( world, index, 14600 );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const tripoint_abs_omt target( 18, 20, 0 );
        prepare_hostile_follow_on( site, 8, 4, "hostile-loss-target", target, 700 );
        const bandit_live_world::hostile_operation_plan plan =
            bandit_live_world::plan_hostile_operation(
                site, bandit_live_world::hostile_operation_kind::shakedown,
                { site.anchor, tripoint_abs_omt( 14, 20, 0 ), target },
                tripoint_abs_omt( 14, 20, 0 ), 702 );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
        REQUIRE( transition_test_hostile_operation(
                     site, bandit_live_world::hostile_operation_phase::assembling,
                     bandit_live_world::hostile_operation_phase::outbound, 703,
                     "release-path hostile loss departure" ) ==
                 bandit_live_world::hostile_operation_transition_result::applied );
        const std::string activity_id =
            site.active_hostile_operation.reservation.activity_id;
        const int generation = site.active_hostile_operation.reservation.generation;
        const std::vector<character_id> member_ids =
            site.active_hostile_operation.reservation.member_ids;
        int casualty_minutes = 704;
        for( const character_id &member_id : member_ids ) {
            REQUIRE( bandit_live_world::record_matching_external_outing_casualty(
                         site, activity_id, generation, member_id,
                         bandit_live_world::member_state::dead, casualty_minutes++,
                         "matching hostile casualty" ) );
        }
        CHECK( site.active_hostile_operation.phase ==
               bandit_live_world::hostile_operation_phase::lost );
        const std::optional<int> released =
            bandit_live_world::release_matching_external_reservation(
                site, activity_id, generation, "matching hostile all-dead cleanup" );
        REQUIRE( released );
        CHECK( *released == 0 );
        CHECK_FALSE( site.active_hostile_operation.is_active() );
        for( const character_id &member_id : member_ids ) {
            CHECK( site.find_member( member_id )->state ==
                   bandit_live_world::member_state::dead );
        }
        REQUIRE( site.roster().valid );
        CHECK( site.roster().reserved_unresolved_ids.empty() );
    }
}

TEST_CASE( "bandit_live_world_rejects_cross_camp_member_identity_aliasing",
           "[bandit][live_world][reservation][identity]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn(
                 world, "bandit", character_id( 14650 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ),
                 std::nullopt, special_lookup ) );
    const std::string one_site_bytes = serialize_world( world );

    REQUIRE( bandit_live_world::claim_tracked_spawn(
                 world, "bandit", character_id( 14650 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ),
                 std::nullopt, special_lookup ) );
    CHECK( serialize_world( world ) == one_site_bytes );

    CHECK_FALSE( bandit_live_world::claim_tracked_spawn(
                     world, "cannibal_hunter", character_id( 14650 ),
                     tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ),
                     std::nullopt, special_lookup ) );
    CHECK( serialize_world( world ) == one_site_bytes );
    REQUIRE( world.sites.size() == 1 );
    CHECK( world.sites.front().has_member( character_id( 14650 ) ) );

    bandit_live_world::world_state duplicate_source;
    REQUIRE( bandit_live_world::claim_tracked_spawn(
                 duplicate_source, "bandit", character_id( 14650 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ),
                 std::nullopt, special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn(
                 duplicate_source, "cannibal_hunter", character_id( 14651 ),
                 tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ),
                 std::nullopt, special_lookup ) );
    std::string duplicated_identity = serialize_world( duplicate_source );
    const std::string distinct_id = "\"npc_id\": 14651";
    REQUIRE( duplicated_identity.find( distinct_id ) != std::string::npos );
    duplicated_identity.replace( duplicated_identity.find( distinct_id ), distinct_id.size(),
                                  "\"npc_id\": 14650" );
    JsonValue duplicated_json = json_loader::from_string( duplicated_identity );
    CHECK_THROWS( world.deserialize( duplicated_json.get_object() ) );
    CHECK( serialize_world( world ) == one_site_bytes );
}

TEST_CASE( "bandit_live_world_origin_loss_recalls_and_releases_the_exact_external_party",
           "[bandit][live_world][reservation][origin_loss]" )
{
    SECTION( "physical recall survives save load and terminal return orphans survivors" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 3; ++index ) {
            add_bandit_camp_member( world, index, 14700 );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const bandit_live_world::dispatch_plan plan =
            bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 18, 20, 0 ), "lost-origin-target" );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
        const std::string activity_id = site.active_outing.activity_id;
        const int generation = site.active_outing.generation;
        const bandit_live_world::simulation_advance_cursor cursor =
            require_current_simulation_cursor( site );

        const std::string before_signal = serialize_world( world );
        CHECK_FALSE( bandit_live_world::request_origin_recall(
                         site, cursor, false, 1, "no physical messenger" ) );
        bandit_live_world::simulation_advance_cursor stale_cursor = cursor;
        stale_cursor.generation++;
        CHECK_FALSE( bandit_live_world::request_origin_recall(
                         site, stale_cursor, true, 1, "stale physical messenger" ) );
        CHECK( serialize_world( world ) == before_signal );

        REQUIRE( bandit_live_world::request_origin_recall(
                     site, cursor, true, 1, "survivor carried a physical recall" ) );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
        REQUIRE( bandit_live_world::invalidate_site_origin(
                     site, bandit_live_world::origin_disposition::captured_non_hostile,
                     2, "camp captured before the party returned" ) );
        CHECK( site.retired_empty_site );
        CHECK( site.active_outing.is_active() );
        CHECK( site.roster().reserved_unresolved_ids == plan.member_ids );
        CHECK( bandit_live_world::camp_supply_living_total( site ) == 0 );
        CHECK( bandit_live_world::camp_supply_cap( site ) == 0 );

        world = round_trip_world( world );
        bandit_live_world::site_record &loaded_site = world.sites.front();
        CHECK( loaded_site.schema_version == 12 );
        CHECK( loaded_site.origin ==
               bandit_live_world::origin_disposition::captured_non_hostile );
        CHECK( loaded_site.origin_changed_minutes == 2 );
        CHECK( loaded_site.active_outing.activity_id == activity_id );
        CHECK( loaded_site.active_outing.generation == generation );
        REQUIRE( loaded_site.roster().valid );

        const std::string terminal_bytes = serialize_world( world );
        CHECK_FALSE( bandit_live_world::claim_tracked_spawn(
                         world, "bandit", character_id( 14799 ),
                         tripoint_abs_ms( 243, 480, 0 ), std::string( "bandit_camp" ),
                         std::nullopt, special_lookup ) );
        CHECK_FALSE( bandit_live_world::register_abstract_site(
                         world, bandit_live_world::anchor_source_kind::overmap_special,
                         "bandit_camp", tripoint_abs_omt( 10, 20, 0 ), special_lookup, 5 ) );
        CHECK_FALSE( bandit_live_world::invalidate_site_origin(
                         loaded_site, bandit_live_world::origin_disposition::deleted,
                         3, "duplicate terminal origin event" ) );
        CHECK( serialize_world( world ) == terminal_bytes );

        const std::vector<bandit_live_world::active_member_observation> observations = {
            { plan.member_ids.front(), bandit_live_world::active_member_observation_state::home,
              "returned to find a captured camp" },
            { plan.member_ids.back(), bandit_live_world::active_member_observation_state::dead,
              "died during the return" }
        };
        CHECK_FALSE( bandit_live_world::resolve_origin_loss_return(
                         loaded_site, activity_id + ":stale", generation,
                         observations, 3, "stale origin return" ).valid );
        CHECK( serialize_world( world ) == terminal_bytes );

        const bandit_live_world::origin_loss_resolution_effect effect =
            bandit_live_world::resolve_origin_loss_return(
                loaded_site, activity_id, generation, observations, 3,
                "party resolved against terminal origin" );
        REQUIRE( effect.valid );
        CHECK( effect.changed );
        CHECK( effect.reservation_released );
        CHECK( effect.orphaned_survivors == 1 );
        CHECK( effect.dead_members == 1 );
        CHECK( effect.missing_members == 0 );
        CHECK( loaded_site.active_external_outing() == nullptr );
        CHECK( loaded_site.find_member( plan.member_ids.front() )->state ==
               bandit_live_world::member_state::orphaned );
        CHECK( loaded_site.find_member( plan.member_ids.back() )->state ==
               bandit_live_world::member_state::dead );
        REQUIRE( loaded_site.roster().valid );
        CHECK( loaded_site.roster().orphaned_ids ==
               std::vector<character_id> { plan.member_ids.front() } );
        CHECK( loaded_site.roster().ready_concrete_total == 1 );
        CHECK( loaded_site.applied_return_generation == generation );
        CHECK( serialize_world( round_trip_world( world ) ) == serialize_world( world ) );
    }

    SECTION( "missing resolution waits for the persisted deadline" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 3; ++index ) {
            add_bandit_camp_member( world, index, 14750 );
        }
        bandit_live_world::site_record &site = world.sites.front();
        const bandit_live_world::dispatch_plan plan =
            bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 18, 20, 0 ), "missing-origin-target" );
        REQUIRE( plan.valid );
        REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
        site.active_outing.missing_deadline_minutes = 10;
        const std::string activity_id = site.active_outing.activity_id;
        const int generation = site.active_outing.generation;
        REQUIRE( bandit_live_world::invalidate_site_origin(
                     site, bandit_live_world::origin_disposition::deleted,
                     1, "origin deleted while party was away" ) );
        const std::vector<bandit_live_world::active_member_observation> observations = {
            { plan.member_ids.front(), bandit_live_world::active_member_observation_state::home,
              "survivor found no camp" },
            { plan.member_ids.back(), bandit_live_world::active_member_observation_state::missing,
              "member did not return" }
        };
        const std::string before_deadline = serialize_world( world );
        CHECK_FALSE( bandit_live_world::resolve_origin_loss_return(
                         site, activity_id, generation, observations, 9,
                         "premature missing resolution" ).valid );
        CHECK( serialize_world( world ) == before_deadline );

        const bandit_live_world::origin_loss_resolution_effect effect =
            bandit_live_world::resolve_origin_loss_return(
                site, activity_id, generation, observations, 10,
                "missing deadline reached at terminal origin" );
        REQUIRE( effect.valid );
        CHECK( effect.orphaned_survivors == 1 );
        CHECK( effect.missing_members == 1 );
        CHECK( site.find_member( plan.member_ids.back() )->state ==
               bandit_live_world::member_state::missing );
        CHECK( site.active_external_outing() == nullptr );
        REQUIRE( site.roster().valid );
    }

    SECTION( "v10 packets migrate to active origin and v12 requires current fields" ) {
        bandit_live_world::world_state world;
        add_bandit_camp_member( world, 0, 14790 );
        std::string legacy_bytes = serialize_world( world );
        const std::string current_schema = "\"schema_version\": 12";
        REQUIRE( legacy_bytes.find( current_schema ) != std::string::npos );
        legacy_bytes.replace( legacy_bytes.find( current_schema ), current_schema.size(),
                              "\"schema_version\": 10" );
        erase_pretty_json_member_line( legacy_bytes, "origin_disposition" );
        erase_pretty_json_member_line( legacy_bytes, "origin_changed_minutes" );
        erase_pretty_json_member_line( legacy_bytes, "origin_summary" );
        erase_pretty_json_member_line( legacy_bytes, "routine_activated_minutes" );
        erase_pretty_json_member_line( legacy_bytes, "last_routine_resolved_minutes" );
        erase_pretty_json_member_line( legacy_bytes, "next_routine_dispatch_eligible_minutes" );
        erase_pretty_json_member_line( legacy_bytes, "routine_no_candidate_streak" );
        bandit_live_world::world_state migrated;
        JsonValue legacy_json = json_loader::from_string( legacy_bytes );
        migrated.deserialize( legacy_json.get_object() );
        REQUIRE( migrated.sites.size() == 1 );
        CHECK( migrated.sites.front().schema_version == 12 );
        CHECK( migrated.sites.front().origin ==
               bandit_live_world::origin_disposition::active_hostile );
        CHECK( migrated.sites.front().origin_changed_minutes == -1 );
        CHECK( migrated.sites.front().origin_summary.empty() );

        std::string malformed_v12 = serialize_world( world );
        erase_pretty_json_member_line( malformed_v12, "origin_summary" );
        JsonValue malformed_json = json_loader::from_string( malformed_v12 );
        const std::string before = serialize_world( migrated );
        CHECK_THROWS( migrated.deserialize( malformed_json.get_object() ) );
        CHECK( serialize_world( migrated ) == before );
    }
}

TEST_CASE( "bandit_structural_dispatch_holds_high_known_threat_low_reward",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14800 );
    add_bandit_camp_member( world, 1, 14800 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    lead->threat = 3;
    lead->threat_confirmed = true;

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    CHECK_FALSE( plan.valid );
    CHECK( plan.known_threat == 3 );
    CHECK( plan.effective_interest <= 0 );
}

TEST_CASE( "bandit_structural_outing_reveals_threat_and_turns_back_before_arrival",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14500 );
    add_bandit_camp_member( world, 1, 14500 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    CHECK( lead->threat == 0 );
    CHECK_FALSE( lead->threat_confirmed );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );
    CHECK( site.active_outing.target_id == lead_id );
    REQUIRE( site.find_member( character_id( 14500 ) ) != nullptr );
    CHECK( site.find_member( character_id( 14500 ) )->state == bandit_live_world::member_state::outbound );

    const bandit_live_world::structural_outing_result result =
        bandit_live_world::advance_structural_bounty_outings( world, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 4, true, "test threat at stalking distance" };
    } );

    CHECK( result.stalking_checks_processed == 1 );
    CHECK( result.lost_interest_returns == 1 );
    CHECK( result.arrivals_processed == 0 );
    CHECK( result.members_returned == 0 );
    const bandit_live_world::camp_map_lead *updated = site.intelligence_map.find_lead( lead_id );
    REQUIRE( updated != nullptr );
    CHECK( updated->status == bandit_live_world::camp_lead_status::dangerous );
    CHECK( updated->threat == 4 );
    CHECK( updated->threat_confirmed );
    CHECK( updated->bounty == 2 );
    CHECK( updated->last_checked_minutes == 160 );
    CHECK( updated->last_outcome == "threat_revealed_lost_interest" );
    CHECK( site.active_outing.activity_id == plan.activity_id );
    CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( site.active_outing.waypoint_index == 1 );
    CHECK( site.find_member( character_id( 14500 ) )->state ==
           bandit_live_world::member_state::outbound );

    const bandit_live_world::structural_outing_result returned =
        bandit_live_world::advance_structural_bounty_outings( world, 240,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( returned.members_returned == 2 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
    CHECK( site.find_member( character_id( 14500 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_structural_outing_consumes_bounty_on_arrival_after_interest_survives",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14600 );
    add_bandit_camp_member( world, 1, 14600 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    const bandit_live_world::structural_outing_result stalk =
        bandit_live_world::advance_structural_bounty_outings( world, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet forest edge" };
    } );
    CHECK( stalk.stalking_checks_processed == 1 );
    CHECK( stalk.arrivals_processed == 0 );
    CHECK( site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( site.intelligence_map.find_lead( lead_id )->bounty == 1 );

    const bandit_live_world::structural_outing_result arrived =
        bandit_live_world::advance_structural_bounty_outings( world, 200,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( arrived.arrivals_processed == 1 );
    CHECK( arrived.members_returned == 0 );
    const bandit_live_world::camp_map_lead *updated = site.intelligence_map.find_lead( lead_id );
    REQUIRE( updated != nullptr );
    CHECK( updated->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( updated->bounty == 0 );
    CHECK( updated->times_harvested == 1 );
    CHECK( updated->last_checked_minutes == 200 );
    CHECK( updated->last_outcome == "harvested_structural_bounty" );
    CHECK( site.active_outing.activity_id == plan.activity_id );
    CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( site.active_outing.waypoint_index == 2 );
    CHECK( site.find_member( character_id( 14600 ) )->state ==
           bandit_live_world::member_state::outbound );

    const bandit_live_world::structural_outing_result returned =
        bandit_live_world::advance_structural_bounty_outings( world, 240,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( returned.members_returned == 2 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
    CHECK( site.find_member( character_id( 14600 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_structural_outing_arrival_is_once_only_after_reload",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14900 );
    add_bandit_camp_member( world, 1, 14900 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    bandit_live_world::world_state before_stalk = round_trip_world( world );
    bandit_live_world::site_record &before_site = before_stalk.sites.front();
    CHECK( before_site.active_outing.activity_id == before_site.site_id + "#structural" );
    CHECK( before_site.active_outing.target_id == lead_id );
    CHECK( before_site.active_outing.target_omt == forest_omt );
    CHECK( before_site.active_outing.job_type == "scavenge" );
    CHECK( before_site.active_outing.started_minutes == 100 );
    CHECK( before_site.active_outing.local_contact_minutes == -1 );
    CHECK( before_site.active_outing.shared_route == plan.shared_route );
    CHECK( before_site.active_outing.waypoint_index == 0 );
    CHECK( before_site.active_outing.expected_return_minutes == 240 );
    REQUIRE( before_site.find_member( character_id( 14900 ) ) != nullptr );
    CHECK( before_site.find_member( character_id( 14900 ) )->state ==
           bandit_live_world::member_state::outbound );
    REQUIRE( before_site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::active );
    CHECK_FALSE( before_site.intelligence_map.find_lead( lead_id )->threat_confirmed );

    const bandit_live_world::structural_outing_result too_early =
        bandit_live_world::advance_structural_bounty_outings( before_stalk, 159,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "should not run before stalking" };
    } );
    CHECK( too_early.stalking_checks_processed == 0 );
    CHECK( too_early.arrivals_processed == 0 );
    CHECK( before_site.active_outing.local_contact_minutes == -1 );

    const bandit_live_world::structural_outing_result stalk =
        bandit_live_world::advance_structural_bounty_outings( before_stalk, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet forest edge after reload" };
    } );
    CHECK( stalk.stalking_checks_processed == 1 );
    CHECK( stalk.arrivals_processed == 0 );
    REQUIRE( before_site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->bounty == 1 );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->times_harvested == 0 );
    CHECK( before_site.active_outing.local_contact_minutes == 160 );
    CHECK( before_site.active_outing.shared_route == plan.shared_route );
    CHECK( before_site.active_outing.waypoint_index == 1 );

    const std::string before_same_minute_replay = serialize_world( before_stalk );
    int replay_threat_reads = 0;
    const auto replay_threat_lookup = [&replay_threat_reads](
                                          const bandit_live_world::site_record &,
                                          const bandit_live_world::camp_map_lead & ) {
        replay_threat_reads++;
        return bandit_live_world::structural_threat_read{ 9, true, "replayed same-minute threat" };
    };
    const bandit_live_world::structural_outing_result same_minute_replay =
        bandit_live_world::advance_structural_bounty_outings( before_stalk, 160,
                replay_threat_lookup );
    CHECK( same_minute_replay.active_outings_considered == 1 );
    CHECK( same_minute_replay.stalking_checks_processed == 0 );
    CHECK( same_minute_replay.arrivals_processed == 0 );
    CHECK( replay_threat_reads == 0 );
    CHECK( serialize_world( before_stalk ) == before_same_minute_replay );

    bandit_live_world::world_state after_stalk = round_trip_world( before_stalk );
    bandit_live_world::site_record &after_site = after_stalk.sites.front();
    REQUIRE( after_site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->threat_confirmed );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->bounty == 1 );
    CHECK( after_site.active_outing.activity_id == after_site.site_id + "#structural" );
    CHECK( after_site.active_outing.local_contact_minutes == 160 );
    CHECK( after_site.active_outing.shared_route == plan.shared_route );
    CHECK( after_site.active_outing.waypoint_index == 1 );

    const bandit_live_world::structural_outing_result arrived =
        bandit_live_world::advance_structural_bounty_outings( after_stalk, 200,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( arrived.arrivals_processed == 1 );
    CHECK( arrived.members_returned == 0 );
    const bandit_live_world::camp_map_lead *harvested = after_site.intelligence_map.find_lead( lead_id );
    REQUIRE( harvested != nullptr );
    CHECK( harvested->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( harvested->bounty == 0 );
    CHECK( harvested->times_harvested == 1 );
    CHECK( harvested->last_outcome == "harvested_structural_bounty" );
    CHECK( after_site.active_outing.activity_id == plan.activity_id );
    CHECK( after_site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( after_site.active_outing.shared_route == plan.shared_route );
    CHECK( after_site.active_outing.waypoint_index == 2 );
    CHECK( after_site.find_member( character_id( 14900 ) )->state ==
           bandit_live_world::member_state::outbound );

    bandit_live_world::world_state returning_home = round_trip_world( after_stalk );
    CHECK( returning_home.sites.front().active_outing.shared_route == plan.shared_route );
    CHECK( returning_home.sites.front().active_outing.waypoint_index == 2 );
    const bandit_live_world::structural_outing_result returned =
        bandit_live_world::advance_structural_bounty_outings( returning_home, 240,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( returned.members_returned == 2 );
    CHECK( returning_home.sites.front().active_outing.activity_id.empty() );
    CHECK( returning_home.sites.front().active_outing.member_ids.empty() );
    CHECK( returning_home.sites.front().find_member( character_id( 14900 ) )->state ==
           bandit_live_world::member_state::at_home );
    bandit_live_world::site_record &completed_site = returning_home.sites.front();

    const bandit_live_world::structural_outing_result repeat =
        bandit_live_world::advance_structural_bounty_outings( returning_home, 260,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( repeat.active_outings_considered == 0 );
    CHECK( repeat.arrivals_processed == 0 );
    CHECK( completed_site.intelligence_map.find_lead( lead_id )->times_harvested == 1 );

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
    };
    const bandit_live_world::structural_bounty_scan_result scan =
        bandit_live_world::advance_structural_bounty_scan( returning_home, 0, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );
    CHECK( scan.candidates_sampled == 1 );
    CHECK( scan.leads_seeded == 0 );
    CHECK( scan.leads_suppressed_by_memory == 1 );
    CHECK( completed_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::harvested );
    CHECK( completed_site.intelligence_map.find_lead( lead_id )->bounty == 0 );
    CHECK( completed_site.intelligence_map.find_lead( lead_id )->times_harvested == 1 );
}

TEST_CASE( "bandit_structural_outing_dangerous_turnback_survives_reload_and_blocks_reselection",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15000 );
    add_bandit_camp_member( world, 1, 15000 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    bandit_live_world::world_state loaded = round_trip_world( world );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.local_contact_minutes == -1 );
    CHECK( loaded_site.find_member( character_id( 15000 ) )->state ==
           bandit_live_world::member_state::outbound );

    const bandit_live_world::structural_outing_result turned_back =
        bandit_live_world::advance_structural_bounty_outings( loaded, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 4, true, "dangerous town edge after reload" };
    } );
    CHECK( turned_back.stalking_checks_processed == 1 );
    CHECK( turned_back.lost_interest_returns == 1 );
    CHECK( turned_back.arrivals_processed == 0 );
    CHECK( turned_back.members_returned == 0 );
    CHECK( loaded_site.active_outing.phase ==
           bandit_live_world::scout_phase::returning_home );
    CHECK( loaded_site.active_outing.shared_route == plan.shared_route );
    CHECK( loaded_site.active_outing.waypoint_index == 1 );
    CHECK( loaded_site.find_member( character_id( 15000 ) )->state ==
           bandit_live_world::member_state::outbound );

    bandit_live_world::world_state reloaded_danger = round_trip_world( loaded );
    bandit_live_world::site_record &danger_site = reloaded_danger.sites.front();
    const bandit_live_world::camp_map_lead *danger = danger_site.intelligence_map.find_lead( lead_id );
    REQUIRE( danger != nullptr );
    CHECK( danger->status == bandit_live_world::camp_lead_status::dangerous );
    CHECK( danger->bounty == 2 );
    CHECK( danger->threat == 4 );
    CHECK( danger->threat_confirmed );
    CHECK( danger->last_outcome == "threat_revealed_lost_interest" );
    CHECK( danger_site.active_outing.activity_id == plan.activity_id );
    CHECK( danger_site.active_outing.shared_route == plan.shared_route );
    CHECK( danger_site.active_outing.waypoint_index == 1 );

    const bandit_live_world::structural_outing_plan blocked =
        bandit_live_world::plan_structural_bounty_outing( danger_site, *danger, 220 );
    CHECK_FALSE( blocked.valid );

    const bandit_live_world::structural_outing_result returned =
        bandit_live_world::advance_structural_bounty_outings( reloaded_danger, 240,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( returned.members_returned == 2 );
    CHECK( danger_site.active_outing.activity_id.empty() );
    CHECK( danger_site.active_outing.member_ids.empty() );
    CHECK( danger_site.find_member( character_id( 15000 ) )->state ==
           bandit_live_world::member_state::at_home );

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { town_omt, "house_base" },
    };
    const bandit_live_world::structural_bounty_scan_result scan =
        bandit_live_world::advance_structural_bounty_scan( reloaded_danger, 0, 2,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );
    CHECK( scan.candidates_sampled == 2 );
    CHECK( scan.leads_seeded == 0 );
    CHECK( scan.leads_suppressed_by_memory == 1 );
    CHECK( danger_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::dangerous );
    CHECK( danger_site.intelligence_map.find_lead( lead_id )->threat == 4 );
}

TEST_CASE( "bandit_playback_structural_forest_town_progression_across_cooldown",
           "[bandit][live_world][structural_bounty][playback]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15100 );
    add_bandit_camp_member( world, 1, 15100 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
        { town_omt, "house_base" },
    };

    int scan_candidates = 0;
    int leads_seeded = 0;
    int dispatches_planned = 0;
    int stalking_checks = 0;
    int lost_interest_returns = 0;
    int arrivals = 0;
    int members_returned = 0;
    std::vector<std::string> dispatched_leads;

    for( int minute = 0; minute <= 2400; ++minute ) {
        const bandit_live_world::structural_outing_result outing =
            bandit_live_world::advance_structural_bounty_outings( world, minute,
        [&town_omt]( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & lead ) {
            if( lead.omt == town_omt ) {
                return bandit_live_world::structural_threat_read{ 4, true, "deterministic town danger" };
            }
            return bandit_live_world::structural_threat_read{ 0, true, "deterministic quiet forest" };
        } );
        stalking_checks += outing.stalking_checks_processed;
        lost_interest_returns += outing.lost_interest_returns;
        arrivals += outing.arrivals_processed;
        members_returned += outing.members_returned;

        if( !site.has_active_outside_pressure() ) {
            const bandit_live_world::structural_outing_plan plan =
                bandit_live_world::plan_structural_bounty_outing( site, minute );
            if( plan.valid ) {
                REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, minute ) );
                dispatches_planned++;
                dispatched_leads.push_back( plan.lead_id );
            }
        }

        if( leads_seeded < 2 ) {
            const bandit_live_world::structural_bounty_scan_result scan =
                bandit_live_world::advance_structural_bounty_scan( world, minute, 4,
            [&terrain]( const tripoint_abs_omt & omt ) {
                return lookup_test_terrain( terrain, omt );
            } );
            scan_candidates += scan.candidates_sampled;
            leads_seeded += scan.leads_seeded;
        }
    }

    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  forest_omt, "forest" );
    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    const bandit_live_world::camp_map_lead *forest = site.intelligence_map.find_lead( forest_id );
    const bandit_live_world::camp_map_lead *town = site.intelligence_map.find_lead( town_id );
    REQUIRE( forest != nullptr );
    REQUIRE( town != nullptr );

    CHECK( leads_seeded == 2 );
    CHECK( dispatches_planned == 2 );
    CHECK( stalking_checks == 2 );
    CHECK( lost_interest_returns == 1 );
    CHECK( arrivals == 1 );
    CHECK( members_returned == 4 );
    CHECK( forest->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( forest->bounty == 0 );
    CHECK( forest->times_harvested == 1 );
    CHECK( town->status == bandit_live_world::camp_lead_status::dangerous );
    CHECK( town->bounty == 2 );
    CHECK( town->threat == 4 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
    CHECK( site.find_member( character_id( 15100 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( scan_candidates <= 40 );
    REQUIRE( dispatched_leads.size() == 2 );
    CHECK( std::count( dispatched_leads.begin(), dispatched_leads.end(), forest_id ) == 1 );
    CHECK( std::count( dispatched_leads.begin(), dispatched_leads.end(), town_id ) == 1 );
}

TEST_CASE( "bandit_playback_structural_multi_camp_budget_stays_bounded",
           "[bandit][live_world][structural_bounty][playback]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15200 );
    add_bandit_camp_member( world, 1, 15200 );
    add_bandit_work_camp_member( world, 0, 15300 );
    add_bandit_work_camp_member( world, 1, 15300 );

    int scan_candidates = 0;
    int scan_budget_hits = 0;
    int leads_seeded = 0;
    int leads_suppressed = 0;
    int dispatches_planned = 0;
    int stalking_checks = 0;
    int lost_interest_returns = 0;
    int arrivals = 0;
    int members_returned = 0;
    int max_active_outings = 0;
    std::vector<std::string> dispatched_leads;

    for( int minute = 0; minute <= 7200; ++minute ) {
        const bandit_live_world::structural_outing_result outing =
            bandit_live_world::advance_structural_bounty_outings( world, minute,
        []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{ 0, true, "deterministic quiet structural target" };
        } );
        stalking_checks += outing.stalking_checks_processed;
        lost_interest_returns += outing.lost_interest_returns;
        arrivals += outing.arrivals_processed;
        members_returned += outing.members_returned;

        int active_now = 0;
        for( bandit_live_world::site_record &site : world.sites ) {
            if( site.has_active_outside_pressure() ) {
                active_now++;
                continue;
            }
            const bandit_live_world::structural_outing_plan plan =
                bandit_live_world::plan_structural_bounty_outing( site, minute );
            if( plan.valid && minute <= 7000 ) {
                REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, minute ) );
                dispatches_planned++;
                active_now++;
                dispatched_leads.push_back( plan.lead_id );
            }
        }
        max_active_outings = std::max( max_active_outings, active_now );

        if( leads_seeded < 8 ) {
            const bandit_live_world::structural_bounty_scan_result scan =
                bandit_live_world::advance_structural_bounty_scan( world, minute, 8,
            []( const tripoint_abs_omt & omt ) -> std::optional<std::string> {
                if( omt.z() != 0 ) {
                    return std::nullopt;
                }
                return std::string( "forest" );
            } );
            scan_candidates += scan.candidates_sampled;
            scan_budget_hits += scan.budget_exhausted ? 1 : 0;
            leads_seeded += scan.leads_seeded;
            leads_suppressed += scan.leads_suppressed_by_memory;
        }
    }

    const bandit_live_world::structural_outing_result final_returns =
        bandit_live_world::advance_structural_bounty_outings( world, 7201,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true,
                "deterministic final shared-route return" };
    } );
    stalking_checks += final_returns.stalking_checks_processed;
    lost_interest_returns += final_returns.lost_interest_returns;
    arrivals += final_returns.arrivals_processed;
    members_returned += final_returns.members_returned;

    REQUIRE( world.sites.size() == 2 );
    CHECK( leads_seeded == 8 );
    CHECK( dispatches_planned == 8 );
    CHECK( stalking_checks == 8 );
    CHECK( lost_interest_returns == 0 );
    CHECK( arrivals == 8 );
    CHECK( members_returned == 16 );
    CHECK( max_active_outings == 2 );
    CHECK( scan_budget_hits == 1 );
    CHECK( scan_candidates <= 32 );
    CHECK( leads_suppressed == 0 );
    REQUIRE( dispatched_leads.size() == 8 );
    for( const std::string &lead_id : dispatched_leads ) {
        CHECK( std::count( dispatched_leads.begin(), dispatched_leads.end(), lead_id ) == 1 );
    }

    for( const bandit_live_world::site_record &site : world.sites ) {
        CHECK( site.active_outing.activity_id.empty() );
        CHECK( site.active_outing.member_ids.empty() );
        int harvested = 0;
        for( const bandit_live_world::camp_map_lead &lead : site.intelligence_map.leads ) {
            if( lead.status == bandit_live_world::camp_lead_status::harvested ) {
                harvested++;
                CHECK( lead.bounty == 0 );
                CHECK( lead.times_harvested == 1 );
                CHECK( lead.last_outcome == "harvested_structural_bounty" );
            }
        }
        CHECK( harvested == 4 );
    }
}

TEST_CASE( "hostile_camp_structural_live_maintenance_seeds_dispatches_and_advances",
           "[bandit][live_world][structural_bounty]" )
{
    for( const bool cannibal : { false, true } ) {
        INFO( "profile=" << ( cannibal ? "cannibal" : "bandit" ) );
        bandit_live_world::world_state world;
        const int id_base = cannibal ? 15500 : 15400;
        if( cannibal ) {
            add_cannibal_camp_member( world, 0, id_base );
            add_cannibal_camp_member( world, 1, id_base );
        } else {
            add_bandit_camp_member( world, 0, id_base );
            add_bandit_camp_member( world, 1, id_base );
        }
        bandit_live_world::site_record &site = world.sites.front();
        site.supply_units = 0;
        site.supply_last_update_minutes = 0;
        const tripoint_abs_omt forest_omt( site.anchor.x() - 4, site.anchor.y(), site.anchor.z() );
        const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
            { forest_omt, "forest" },
        };

        const bandit_live_world::structural_bounty_maintenance_result seeded =
            bandit_live_world::advance_structural_bounty_maintenance( world, 0, 4, 1,
        [&terrain]( const tripoint_abs_omt & omt ) {
            return lookup_test_terrain( terrain, omt );
        }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{ 0, true, "quiet live-maintenance structural target" };
        } );
        CHECK( seeded.scan.candidates_sampled == 1 );
        CHECK( seeded.scan.leads_seeded == 1 );
        CHECK( seeded.dispatch_cap == 1 );
        CHECK( seeded.dispatches_planned == 1 );
        CHECK( seeded.dispatches_applied == 1 );
        CHECK( seeded.outing.active_outings_considered == 0 );
        CHECK( site.active_outing.activity_id == site.site_id + "#structural" );
        CHECK( site.active_outing.target_omt == forest_omt );
        CHECK( site.active_outing.shared_route == std::vector<tripoint_abs_omt> {
            site.anchor,
            tripoint_abs_omt( forest_omt.x() + 1, forest_omt.y(), forest_omt.z() ),
            forest_omt, site.anchor
        } );
        CHECK( site.active_outing.waypoint_index == 0 );
        CHECK( site.find_member( character_id( id_base ) )->state ==
               bandit_live_world::member_state::outbound );
        CHECK( site.find_member( character_id( id_base + 1 ) )->state ==
               bandit_live_world::member_state::outbound );
        const std::string seeded_report = bandit_live_world::render_structural_bounty_maintenance_report(
                                              seeded );
        CHECK( seeded_report.find( "leads_seeded=1" ) != std::string::npos );
        CHECK( seeded_report.find( "dispatches_applied=1" ) != std::string::npos );

        const bandit_live_world::structural_bounty_maintenance_result stalked =
            bandit_live_world::advance_structural_bounty_maintenance( world, 60, 4, 1,
        [&terrain]( const tripoint_abs_omt & omt ) {
            return lookup_test_terrain( terrain, omt );
        }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{ 0, true, "quiet live-maintenance structural target" };
        } );
        CHECK( stalked.outing.active_outings_considered == 1 );
        CHECK( stalked.outing.stalking_checks_processed == 1 );
        CHECK( stalked.scan.sites_skipped_active_outside == 0 );
        CHECK( stalked.scan.sites_skipped_no_ready_home == 1 );
        CHECK( stalked.scan.candidates_sampled == 0 );
        CHECK( stalked.dispatches_applied == 0 );
        const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead(
                    site.active_outing.target_id );
        REQUIRE( lead != nullptr );
        CHECK( lead->status == bandit_live_world::camp_lead_status::scout_confirmed );
        CHECK( site.active_outing.local_contact_minutes == 60 );
        CHECK( site.active_outing.waypoint_index == 1 );

        const std::string lead_id = site.active_outing.target_id;
        const bandit_live_world::structural_bounty_maintenance_result arrived =
            bandit_live_world::advance_structural_bounty_maintenance( world, 100, 4, 1,
        [&terrain]( const tripoint_abs_omt & omt ) {
            return lookup_test_terrain( terrain, omt );
        }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{ 0, true, "quiet live-maintenance structural target" };
        } );
        CHECK( arrived.outing.arrivals_processed == 1 );
        CHECK( arrived.outing.members_returned == 0 );
        CHECK( arrived.dispatches_applied == 0 );
        const bandit_live_world::camp_map_lead *harvested = site.intelligence_map.find_lead( lead_id );
        REQUIRE( harvested != nullptr );
        CHECK( harvested->status == bandit_live_world::camp_lead_status::harvested );
        CHECK( harvested->bounty == 0 );
        CHECK( harvested->times_harvested == 1 );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
        CHECK( site.active_outing.waypoint_index == 2 );
        CHECK( site.find_member( character_id( id_base ) )->state ==
               bandit_live_world::member_state::outbound );
        CHECK( site.find_member( character_id( id_base + 1 ) )->state ==
               bandit_live_world::member_state::outbound );

        const bandit_live_world::structural_bounty_maintenance_result returned =
            bandit_live_world::advance_structural_bounty_maintenance( world, 140, 4, 1,
        [&terrain]( const tripoint_abs_omt & omt ) {
            return lookup_test_terrain( terrain, omt );
        }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{};
        } );
        CHECK( returned.outing.members_returned == 2 );
        CHECK( site.active_outing.activity_id.empty() );
        CHECK( site.find_member( character_id( id_base ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( site.find_member( character_id( id_base + 1 ) )->state ==
               bandit_live_world::member_state::at_home );
    }
}

TEST_CASE( "bandit_structural_outing_recent_check_debounce_blocks_reselection",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14700 );
    add_bandit_camp_member( world, 1, 14700 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    lead->last_checked_minutes = 140;
    lead->last_outcome = "recently_checked_low_interest";

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 160 );
    CHECK_FALSE( plan.valid );
}

TEST_CASE( "hostile_camp_abstract_threat_observer_is_corridor_bounded_and_detours_only_to_withdraw",
           "[bandit][live_world][structural_bounty][abstract_threat]" )
{
    SECTION( "observer receives only current and committed forward route OMTs" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15600 );
        bandit_live_world::site_record &site = world.sites.front();
        const std::vector<tripoint_abs_omt> canonical_route = site.active_outing.shared_route;
        int observer_calls = 0;
        bandit_live_world::structural_threat_observer_request captured;
        const bandit_live_world::structural_outing_result early =
            bandit_live_world::advance_structural_bounty_outings( world, 101, {},
        [&observer_calls, &captured]( const bandit_live_world::site_record &,
                                    const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            observer_calls++;
            captured = request;
            return make_abstract_threat_read(
                       tripoint_abs_omt( request.current_omt.x() + 20,
                                         request.current_omt.y() + 20,
                                         request.current_omt.z() ), 200, false );
        } );
        REQUIRE( observer_calls == 1 );
        REQUIRE( canonical_route.size() == 4 );
        CHECK( captured.current_omt == canonical_route[0] );
        CHECK( captured.visible_forward_omts ==
               std::vector<tripoint_abs_omt> { canonical_route[1], canonical_route[2] } );
        CHECK( early.active_outings_considered == 1 );
        CHECK( captured.visible_forward_omts.size() <= 3 );
        CHECK( captured.party_power == bandit_live_world::structural_outing_party_power( site ) );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::outbound );
        CHECK_FALSE( site.active_outing.abstract_encounter.active );
        CHECK( site.active_outing.abstract_detour_attempts == 0 );
        CHECK( site.active_outing.shared_route == canonical_route );
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( site.active_outing.target_id );
        REQUIRE( lead != nullptr );
        CHECK( lead->status == bandit_live_world::camp_lead_status::active );
        CHECK_FALSE( lead->threat_confirmed );
    }

    SECTION( "at most two adjacent reads select a passable withdrawal detour" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( true, 15620 );
        bandit_live_world::site_record &site = world.sites.front();
        const std::vector<tripoint_abs_omt> canonical_route = site.active_outing.shared_route;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        []( const bandit_live_world::site_record &,
            const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            REQUIRE_FALSE( request.visible_forward_omts.empty() );
            bandit_live_world::abstract_threat_read read = make_abstract_threat_read(
                        request.visible_forward_omts.front(), 150, false );
            read.detours = {
                { tripoint_abs_omt( request.current_omt.x(), request.current_omt.y() - 1,
                                    request.current_omt.z() ), false },
                { tripoint_abs_omt( request.current_omt.x(), request.current_omt.y() + 1,
                                    request.current_omt.z() ), true },
            };
            return read;
        } );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
        CHECK( site.active_outing.abstract_detour_attempts == 2 );
        CHECK( site.active_outing.has_withdrawal_detour );
        CHECK( site.active_outing.withdrawal_detour_omt ==
               tripoint_abs_omt( canonical_route[1].x(), canonical_route[1].y() + 1,
                                 canonical_route[1].z() ) );
        CHECK( site.active_outing.shared_route == canonical_route );
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( site.active_outing.target_id );
        REQUIRE( lead != nullptr );
        CHECK( lead->status == bandit_live_world::camp_lead_status::active );
        CHECK_FALSE( lead->threat_confirmed );
    }

    SECTION( "forward danger defers while the party is inside local reality" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15630 );
        bandit_live_world::site_record &site = world.sites.front();
        const std::vector<tripoint_abs_omt> canonical_route = site.active_outing.shared_route;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        []( const bandit_live_world::site_record &,
            const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            REQUIRE_FALSE( request.visible_forward_omts.empty() );
            bandit_live_world::abstract_threat_read read = make_abstract_threat_read(
                        request.visible_forward_omts.front(), 200, false );
            read.local_reality = true;
            return read;
        } );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::outbound );
        CHECK( site.active_outing.abstract_detour_attempts == 0 );
        CHECK_FALSE( site.active_outing.has_withdrawal_detour );
        CHECK_FALSE( site.active_outing.abstract_encounter.active );
        CHECK( site.active_outing.shared_route == canonical_route );
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( site.active_outing.target_id );
        REQUIRE( lead != nullptr );
        CHECK( lead->status == bandit_live_world::camp_lead_status::active );
        CHECK_FALSE( lead->threat_confirmed );
    }

    SECTION( "arrival promotes the target to current overlap before harvest" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15640 );
        bandit_live_world::site_record &site = world.sites.front();
        const tripoint_abs_omt target = site.active_outing.target_omt;
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes,
        []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read { 0, true, "quiet approach" };
        }, []( const bandit_live_world::site_record &,
               const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & ) {
            return bandit_live_world::abstract_threat_read();
        } );
        REQUIRE( site.active_outing.phase == bandit_live_world::scout_phase::observing );
        REQUIRE( site.active_outing.waypoint_index == 1 );

        int target_reads = 0;
        const bandit_live_world::structural_outing_result arrival =
            bandit_live_world::advance_structural_bounty_outings(
                world, 200, {},
        [&target_reads, target, party_power]( const bandit_live_world::site_record &,
                const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            target_reads++;
            CHECK( request.current_omt == target );
            CHECK( request.visible_forward_omts.empty() );
            return make_abstract_threat_read( request.current_omt, party_power );
        } );
        CHECK( target_reads == 1 );
        CHECK( arrival.arrivals_processed == 0 );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
        CHECK( site.active_outing.casualty_ids.size() == 1 );
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( site.active_outing.target_id );
        REQUIRE( lead != nullptr );
        CHECK( lead->times_harvested == 0 );
        CHECK( lead->bounty == 1 );
    }
}

TEST_CASE( "hostile_camp_structural_observer_visibility_uses_real_sight_inputs",
           "[bandit][live_world][structural_bounty][phase4_visibility]" )
{
    struct visibility_case {
        const char *label;
        int ordinary_sight_range_ms;
        float weather_sight_penalty;
        int elevation_omt;
        bool has_optic;
        int expected_omt_range;
    };
    const std::vector<visibility_case> cases = {
        { "unlit night", SEEX - 1, 1.0f, 0, false, 1 },
        { "intermediate light", SEEX * 2, 1.0f, 0, false, 2 },
        { "clear day", SEEX * 4 + 1, 1.0f, 0, false, 3 },
        { "ordinary bad weather", SEEX * 4 + 1, 1.2f, 0, false, 2 },
        { "severe bad weather", SEEX * 4 + 1, 1.7f, 0, false, 1 },
        { "foggy unlit night", SEEX - 1, 1.7f, 0, false, 0 },
        { "night optics", SEEX - 1, 1.0f, 0, true, 2 },
        { "elevated night observer", SEEX - 1, 1.0f, 1, false, 3 },
        { "clear elevated optical terrain budget", SEEX * 4 + 1, 1.0f, 2, true, 14 },
    };
    for( const visibility_case &fixture : cases ) {
        CAPTURE( fixture.label );
        bandit_live_world::structural_observer_visibility_read read;
        read.ordinary_sight_range_ms = fixture.ordinary_sight_range_ms;
        read.weather_sight_penalty = fixture.weather_sight_penalty;
        read.elevation_omt = fixture.elevation_omt;
        read.has_optic = fixture.has_optic;
        CHECK( bandit_live_world::structural_observer_omt_sight_range( read ) ==
               fixture.expected_omt_range );
    }

    standard_npc observer( "Phase 4 visibility observer" );
    observer.recalc_sight_limits();
    const weather_type_id sunny( "sunny" );
    const weather_type_id rainstorm( "rainstorm" );
    const weather_type_id fog( "fog" );
    REQUIRE( sunny.is_valid() );
    REQUIRE( rainstorm.is_valid() );
    REQUIRE( fog.is_valid() );
    const auto real_visibility = [&observer]( const float light,
    const weather_type_id & weather, const int elevation_omt = 0,
    const bool has_optic = false ) {
        bandit_live_world::structural_observer_visibility_read read;
        read.ordinary_sight_range_ms = observer.sight_range( light, light );
        read.weather_sight_penalty = weather->sight_penalty;
        read.elevation_omt = elevation_omt;
        read.has_optic = has_optic;
        return bandit_live_world::structural_observer_omt_sight_range( read );
    };
    const int actual_night = real_visibility( LIGHT_AMBIENT_MINIMAL, sunny );
    const int actual_intermediate = real_visibility( LIGHT_AMBIENT_LIT, sunny );
    const int actual_clear_day = real_visibility( 100.0f, sunny );
    CAPTURE( observer.sight_range( LIGHT_AMBIENT_MINIMAL, LIGHT_AMBIENT_MINIMAL ) );
    CAPTURE( observer.sight_range( LIGHT_AMBIENT_LIT, LIGHT_AMBIENT_LIT ) );
    CAPTURE( observer.sight_range( 100.0f, 100.0f ) );
    CHECK( actual_night == 1 );
    CHECK( actual_intermediate == 2 );
    CHECK( actual_clear_day == 3 );
    CHECK( real_visibility( 100.0f, rainstorm ) == 2 );
    CHECK( real_visibility( 100.0f, fog ) == 1 );

    const oter_str_id field( "field" );
    const oter_str_id forest( "forest" );
    REQUIRE( field.is_valid() );
    REQUIRE( forest.is_valid() );
    const int field_see_cost = static_cast<int>( field->get_see_cost() );
    const int forest_see_cost = static_cast<int>( forest->get_see_cost() );
    CAPTURE( field_see_cost, forest_see_cost );
    CHECK( field_see_cost == 0 );
    CHECK( forest_see_cost == 4 );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_visible(
                     actual_clear_day, { forest_see_cost } ) );
    CHECK( bandit_live_world::structural_observer_route_is_visible(
               real_visibility( 100.0f, sunny, 0, true ), { forest_see_cost } ) );
    CHECK( bandit_live_world::structural_observer_route_is_visible(
               real_visibility( 100.0f, sunny, 1 ), { forest_see_cost } ) );

    bandit_live_world::structural_observer_visibility_read invalid;
    invalid.ordinary_sight_range_ms = -1;
    CHECK( bandit_live_world::structural_observer_omt_sight_range( invalid ) == 0 );
    invalid.ordinary_sight_range_ms = SEEX * 4 + 1;
    invalid.weather_sight_penalty = std::numeric_limits<float>::quiet_NaN();
    CHECK( bandit_live_world::structural_observer_omt_sight_range( invalid ) == 0 );

    CHECK( bandit_live_world::structural_observer_route_is_visible( 3, { 1, 1, 1 } ) );
    CHECK( bandit_live_world::structural_observer_route_is_visible( 2, { 1, 1 } ) );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_visible( 2, { 1, 1, 1 } ) );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_visible( 3, { 1, 3 } ) );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_visible( 3, { -1 } ) );
}

TEST_CASE( "hostile_camp_structural_observer_retains_only_an_exact_recent_visual_track",
           "[bandit][live_world][structural_bounty][phase4_hysteresis]" )
{
    CHECK( bandit_live_world::structural_observer_last_known_max_age_minutes() == 60 );

    CHECK_FALSE( bandit_live_world::structural_observer_route_is_visible( 2, { 1, 1, 1 } ) );
    CHECK( bandit_live_world::structural_observer_route_is_retained( 2, { 1, 1, 1 }, 0 ) );
    CHECK( bandit_live_world::structural_observer_route_is_retained( 2, { 1, 1, 1 }, 60 ) );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_retained( 2, { 1, 1, 1 }, -1 ) );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_retained( 2, { 1, 1, 1 }, 61 ) );
    CHECK_FALSE( bandit_live_world::structural_observer_route_is_retained( 2, { 1, 1, 1, 1 },
                 60 ) );

    const tripoint_abs_omt known_omt( 18, 20, 0 );
    const std::vector<std::string> known_ids = { "horde:a", "horde:b" };
    bandit_live_world::structural_threat_observer_request request;
    request.retained_threat_omt = known_omt;
    request.retained_threat_ids = known_ids;
    request.retained_threat_age_minutes = 60;
    CHECK( bandit_live_world::structural_observer_retained_threat_matches(
               request, known_omt, known_ids ) );
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, tripoint_abs_omt( 19, 20, 0 ), known_ids ) );
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, { "horde:a" } ) );
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, {} ) );
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, { "horde:b", "horde:a" } ) );
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, { "horde:a", "horde:a" } ) );

    request.retained_threat_age_minutes = 61;
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, known_ids ) );
    request.retained_threat_age_minutes = -1;
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, known_ids ) );
    request.retained_threat_age_minutes = 60;
    request.retained_threat_ids = { "horde:b", "horde:a" };
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, known_ids ) );
    request.retained_threat_ids = { "horde:a", "horde:a" };
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, request.retained_threat_ids ) );
    request.retained_threat_ids = known_ids;
    request.retained_threat_omt.reset();
    CHECK_FALSE( bandit_live_world::structural_observer_retained_threat_matches(
                     request, known_omt, known_ids ) );
}

TEST_CASE( "hostile_camp_visual_track_retention_survives_save_until_its_exact_age_boundary",
           "[bandit][live_world][structural_bounty][phase4_hysteresis][save]" )
{
    constexpr int target_distance_omt = 8;
    constexpr int stalking_minutes = 220;
    constexpr int first_observed_minutes = stalking_minutes + 1;
    for( const bool cannibal : { false, true } ) {
        CAPTURE( cannibal );
        bandit_live_world::world_state world = make_abstract_threat_test_world(
                    cannibal, cannibal ? 15860 : 15850, target_distance_omt );
        bandit_live_world::site_record &site = world.sites.front();
        REQUIRE( site.active_outing.started_minutes == 100 );
        bandit_live_world::advance_structural_bounty_outings( world, stalking_minutes, {} );
        REQUIRE( site.active_outing.phase == bandit_live_world::scout_phase::observing );
        REQUIRE( site.active_outing.observations.empty() );

        int initial_observer_calls = 0;
        tripoint_abs_omt observed_omt;
        bandit_live_world::advance_structural_bounty_outings(
            world, first_observed_minutes, {},
        [&initial_observer_calls, &observed_omt]( const bandit_live_world::site_record &,
                const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            initial_observer_calls++;
            REQUIRE_FALSE( request.visible_forward_omts.empty() );
            CHECK_FALSE( request.retained_threat_omt.has_value() );
            CHECK( request.retained_threat_ids.empty() );
            CHECK( request.retained_threat_age_minutes == -1 );
            observed_omt = request.visible_forward_omts.front();
            return make_abstract_threat_read( observed_omt, 1, false );
        } );
        REQUIRE( initial_observer_calls == 1 );
        REQUIRE( site.active_outing.observations.size() == 1 );
        CHECK( site.active_outing.observations.front().record_schema_version == 1 );
        CHECK( site.active_outing.observations.front().sense ==
               bandit_live_world::sortie_observation_sense::visual );
        CHECK( site.active_outing.observations.front().source_omt == observed_omt );
        CHECK( site.active_outing.observations.front().defender_ids ==
               std::vector<std::string> { "horde:test" } );

        const bandit_live_world::world_state saved_snapshot = round_trip_world( world );
        bandit_live_world::world_state at_age_60 = saved_snapshot;
        bandit_live_world::world_state at_age_61 = saved_snapshot;

        int age_60_calls = 0;
        bandit_live_world::structural_threat_observer_request retained_at_60;
        bandit_live_world::advance_structural_bounty_outings(
            at_age_60, first_observed_minutes + 60, {},
        [&age_60_calls, &retained_at_60]( const bandit_live_world::site_record &,
                                         const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            age_60_calls++;
            retained_at_60 = request;
            return bandit_live_world::abstract_threat_read();
        } );
        REQUIRE( age_60_calls == 1 );
        REQUIRE( retained_at_60.retained_threat_omt.has_value() );
        CHECK( *retained_at_60.retained_threat_omt == observed_omt );
        CHECK( retained_at_60.retained_threat_ids ==
               std::vector<std::string> { "horde:test" } );
        CHECK( retained_at_60.retained_threat_age_minutes == 60 );

        int age_61_calls = 0;
        bandit_live_world::structural_threat_observer_request expired_at_61;
        bandit_live_world::advance_structural_bounty_outings(
            at_age_61, first_observed_minutes + 61, {},
        [&age_61_calls, &expired_at_61]( const bandit_live_world::site_record &,
                                        const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            age_61_calls++;
            expired_at_61 = request;
            return bandit_live_world::abstract_threat_read();
        } );
        REQUIRE( age_61_calls == 1 );
        CHECK_FALSE( expired_at_61.retained_threat_omt.has_value() );
        CHECK( expired_at_61.retained_threat_ids.empty() );
        CHECK( expired_at_61.retained_threat_age_minutes == -1 );
    }
}

TEST_CASE( "hostile_camp_structural_scout_records_route_bounded_smoke_and_light",
           "[bandit][live_world][phase4_signal_observation]" )
{
    constexpr int observed_minutes = 221;
    for( const bool cannibal : { false, true } ) {
        CAPTURE( cannibal );
        bandit_live_world::world_state world = make_structural_signal_test_world(
                    cannibal, cannibal ? 15880 : 15870 );
        bandit_live_world::site_record &site = world.sites.front();
        const character_id observer_id = site.active_outing.leader_id;
        const int target_revision = site.active_outing.target_lead_revision;
        const std::string lead_id = site.active_outing.target_lead_id;
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        REQUIRE( target_revision == lead->revision );
        const std::string lead_before_observation = serialize_camp_map_lead( *lead );

        int signal_calls = 0;
        bandit_live_world::advance_structural_bounty_outings(
            world, observed_minutes, {}, {},
        [&signal_calls]( const bandit_live_world::site_record &,
                        const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            signal_calls++;
            REQUIRE_FALSE( request.visible_forward_omts.empty() );
            const tripoint_abs_omt forward = request.visible_forward_omts.front();
            return std::vector<bandit_live_world::structural_signal_read> {
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::smoke,
                    forward, 6, 90, 1 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::light,
                    forward, 3, 65, 2 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::smoke,
                    request.current_omt, 2, 55, 3 ),
            };
        } );

        REQUIRE( signal_calls == 1 );
        REQUIRE( site.active_outing.observations.size() == 2 );
        const auto smoke_iter = std::find_if( site.active_outing.observations.begin(),
        site.active_outing.observations.end(), []( const bandit_live_world::sortie_observation & observation ) {
            return observation.sense == bandit_live_world::sortie_observation_sense::smoke;
        } );
        const auto light_iter = std::find_if( site.active_outing.observations.begin(),
        site.active_outing.observations.end(), []( const bandit_live_world::sortie_observation & observation ) {
            return observation.sense == bandit_live_world::sortie_observation_sense::light;
        } );
        REQUIRE( smoke_iter != site.active_outing.observations.end() );
        REQUIRE( light_iter != site.active_outing.observations.end() );
        const bandit_live_world::sortie_observation &smoke = *smoke_iter;
        const bandit_live_world::sortie_observation &light = *light_iter;
        CHECK( smoke.sense == bandit_live_world::sortie_observation_sense::smoke );
        CHECK( light.sense == bandit_live_world::sortie_observation_sense::light );
        CHECK( smoke.source_omt == smoke.receiver_omt );
        CHECK( light.source_omt != light.receiver_omt );
        CHECK( smoke.strength == 2 );
        CHECK( light.strength == 3 );
        CHECK( smoke.confidence == 55 );
        CHECK( light.confidence == 65 );
        CHECK( smoke.uncertainty_radius_omt == 3 );
        CHECK( light.uncertainty_radius_omt == 2 );
        CHECK( smoke.share_state ==
               bandit_live_world::sortie_observation_share_state::observer_private );
        CHECK( light.share_state ==
               bandit_live_world::sortie_observation_share_state::shared );
        for( const bandit_live_world::sortie_observation *observation : { &smoke, &light } ) {
            CHECK( observation->record_schema_version == 1 );
            CHECK( observation->observer_id == observer_id );
            CHECK( observation->target_revision == target_revision );
            CHECK( observation->observed_minutes == observed_minutes );
            CHECK( observation->expiry_minutes == observed_minutes + 6 * 60 );
            CHECK( observation->source_id.find( "structural-" ) == 0 );
            CHECK( observation->source_id.find( "player@" ) == std::string::npos );
            CHECK( observation->fact_key.find( "player@" ) == std::string::npos );
            CHECK( observation->visual_quality == 0 );
            CHECK( observation->defender_ids.empty() );
            CHECK( observation->observed_power_low == 0 );
            CHECK( observation->observed_power_high == 0 );
            CHECK( observation->equipment_detail == 0 );
        }
        lead = site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        CHECK( serialize_camp_map_lead( *lead ) == lead_before_observation );

        const std::string before_round_trip = serialize_world( world );
        world = round_trip_world( world );
        REQUIRE( world.sites.front().active_outing.observations.size() == 2 );
        CHECK( serialize_world( world ) == before_round_trip );
    }
}

TEST_CASE( "hostile_camp_structural_signal_batch_rejects_malformed_input_atomically",
           "[bandit][live_world][phase4_signal_observation]" )
{
    SECTION( "more than four reads exceeds the bounded input cap" ) {
        bandit_live_world::world_state world = make_structural_signal_test_world( false, 15890 );
        bandit_live_world::site_record &site = world.sites.front();
        bandit_live_world::advance_structural_bounty_outings(
            world, 221, {}, {},
        []( const bandit_live_world::site_record &,
            const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            REQUIRE_FALSE( request.visible_forward_omts.empty() );
            return std::vector<bandit_live_world::structural_signal_read> {
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::smoke,
                    request.current_omt, 2, 55, 3 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::light,
                    request.current_omt, 3, 65, 2 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::smoke,
                    request.visible_forward_omts[0], 2, 55, 3 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::light,
                    request.visible_forward_omts[0], 3, 65, 2 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::smoke,
                    request.visible_forward_omts[0], 4, 75, 4 ),
            };
        } );
        CHECK( site.active_outing.observations.empty() );
    }

    SECTION( "one local-reality read rejects its valid batch mate" ) {
        bandit_live_world::world_state world = make_structural_signal_test_world( true, 15900 );
        bandit_live_world::site_record &site = world.sites.front();
        bandit_live_world::advance_structural_bounty_outings(
            world, 221, {}, {},
        []( const bandit_live_world::site_record &,
            const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return std::vector<bandit_live_world::structural_signal_read> {
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::smoke,
                    request.current_omt, 2, 55, 3 ),
                make_structural_signal_read(
                    bandit_live_world::sortie_observation_sense::light,
                    request.current_omt, 3, 65, 2, true ),
            };
        } );
        CHECK( site.active_outing.observations.empty() );
    }
}

TEST_CASE( "hostile_camp_structural_signal_and_visual_fact_share_one_advance",
           "[bandit][live_world][phase4_signal_observation]" )
{
    bandit_live_world::world_state world = make_structural_signal_test_world( false, 15910 );
    bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::advance_structural_bounty_outings(
        world, 221, {},
    []( const bandit_live_world::site_record &,
        const bandit_live_world::active_outing_state &,
    const bandit_live_world::structural_threat_observer_request & request ) {
        REQUIRE_FALSE( request.visible_forward_omts.empty() );
        return make_abstract_threat_read( request.visible_forward_omts.front(), 1, false );
    }, []( const bandit_live_world::site_record &,
           const bandit_live_world::active_outing_state &,
    const bandit_live_world::structural_threat_observer_request & request ) {
        return std::vector<bandit_live_world::structural_signal_read> {
            make_structural_signal_read(
                bandit_live_world::sortie_observation_sense::smoke,
                request.current_omt, 2, 55, 3 ),
        };
    } );

    REQUIRE( site.active_outing.observations.size() == 2 );
    CHECK( site.active_outing.observations[0].sense ==
           bandit_live_world::sortie_observation_sense::smoke );
    CHECK( site.active_outing.observations[1].sense ==
           bandit_live_world::sortie_observation_sense::visual );
    CHECK( site.active_outing.observations[0].observed_minutes == 221 );
    CHECK( site.active_outing.observations[1].observed_minutes == 221 );
    CHECK( site.active_outing.last_advanced_minutes == 221 );

    const std::string after_one_advance = serialize_world( world );
    bandit_live_world::advance_structural_bounty_outings( world, 221, {}, {},
    []( const bandit_live_world::site_record &,
       const bandit_live_world::active_outing_state &,
    const bandit_live_world::structural_threat_observer_request & request ) {
        return std::vector<bandit_live_world::structural_signal_read> {
            make_structural_signal_read(
                bandit_live_world::sortie_observation_sense::light,
                request.current_omt, 3, 65, 2 ),
        };
    } );
    CHECK( serialize_world( world ) == after_one_advance );
}

TEST_CASE( "hostile_camp_structural_observer_records_owned_evidence_before_returning_report",
           "[bandit][live_world][structural_bounty][abstract_threat][phase4_observation]" )
{
    for( const bool cannibal : { false, true } ) {
        CAPTURE( cannibal );
        bandit_live_world::world_state world = make_abstract_threat_test_world(
                    cannibal, cannibal ? 15655 : 15650 );
        bandit_live_world::site_record &site = world.sites.front();
        site.routine_no_candidate_streak = 2;
        const character_id observer_id = site.active_outing.leader_id;
        const int target_revision = site.active_outing.target_lead_revision;
        const std::string lead_id = site.active_outing.target_lead_id;
        const std::vector<tripoint_abs_omt> route = site.active_outing.shared_route;
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        const std::string lead_before_observation = serialize_camp_map_lead( *lead );

        int observer_calls = 0;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [&observer_calls]( const bandit_live_world::site_record &,
                          const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            observer_calls++;
            REQUIRE_FALSE( request.visible_forward_omts.empty() );
            bandit_live_world::abstract_threat_read read = make_abstract_threat_read(
                        request.visible_forward_omts.front(), 150, false );
            read.visual_quality = 2;
            read.uncertainty_radius_omt = 1;
            read.equipment_detail = 1;
            return read;
        } );
        REQUIRE( observer_calls == 1 );
        REQUIRE( site.active_outing.observations.size() == 1 );
        const bandit_live_world::sortie_observation &observation =
            site.active_outing.observations.front();
        CHECK( observation.record_schema_version == 1 );
        CHECK( observation.observer_id == observer_id );
        CHECK( observation.source_omt == route[2] );
        CHECK( observation.receiver_omt == route[1] );
        CHECK( observation.target_revision == target_revision );
        CHECK( observation.sense == bandit_live_world::sortie_observation_sense::visual );
        CHECK( observation.defender_ids == std::vector<std::string> { "horde:test" } );
        CHECK( observation.observed_power_low == 150 );
        CHECK( observation.observed_power_high == 150 );
        CHECK( observation.bucket_start_minutes == 150 );
        CHECK( observation.share_state ==
               bandit_live_world::sortie_observation_share_state::shared );
        lead = site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        CHECK( serialize_camp_map_lead( *lead ) == lead_before_observation );

        const std::string before_same_minute_replay = serialize_world( world );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        []( const bandit_live_world::site_record &,
           const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return make_abstract_threat_read( request.current_omt, 200 );
        } );
        CHECK( serialize_world( world ) == before_same_minute_replay );

        world = round_trip_world( world );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes + 1, {} );
        CHECK( world.sites.front().active_outing.observations.size() == 1 );
        lead = world.sites.front().intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        CHECK( serialize_camp_map_lead( *lead ) == lead_before_observation );

        const int return_minutes = world.sites.front().active_outing.expected_return_minutes;
        bandit_live_world::advance_structural_bounty_outings( world, return_minutes, {} );
        lead = world.sites.front().intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        CHECK( lead->status == bandit_live_world::camp_lead_status::dangerous );
        CHECK( lead->threat_confirmed );
        CHECK( lead->threat == 150 );
        CHECK( lead->last_outcome == "returned_shared_structural_threat_report" );
        CHECK( world.sites.front().routine_no_candidate_streak == 2 );
    }

    SECTION( "an overlap that loses the observer cannot teach the camp" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15670 );
        bandit_live_world::site_record &site = world.sites.front();
        const character_id observer_id = site.active_outing.leader_id;
        REQUIRE( site.find_member( observer_id ) != nullptr );
        site.find_member( observer_id )->npc_template_id = "bandit_trader";
        for( const character_id member_id : site.active_outing.member_ids ) {
            if( member_id != observer_id ) {
                REQUIRE( site.find_member( member_id ) != nullptr );
                site.find_member( member_id )->npc_template_id = "hells_raiders_boss";
            }
        }
        const std::string lead_id = site.active_outing.target_lead_id;
        const bandit_live_world::camp_map_lead *lead =
            site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        const std::string lead_before_observation = serialize_camp_map_lead( *lead );
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [party_power]( const bandit_live_world::site_record &,
                       const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return make_abstract_threat_read( request.current_omt, party_power );
        } );
        REQUIRE( site.active_outing.observations.size() == 1 );
        CHECK( site.active_outing.observations.front().share_state ==
               bandit_live_world::sortie_observation_share_state::observer_private );
        REQUIRE( site.find_member( observer_id ) != nullptr );
        CHECK( site.find_member( observer_id )->state == bandit_live_world::member_state::missing );
        lead = site.intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        CHECK( serialize_camp_map_lead( *lead ) == lead_before_observation );

        const int return_minutes = site.active_outing.expected_return_minutes;
        bandit_live_world::advance_structural_bounty_outings( world, return_minutes, {} );
        lead = world.sites.front().intelligence_map.find_lead( lead_id );
        REQUIRE( lead != nullptr );
        CHECK( serialize_camp_map_lead( *lead ) == lead_before_observation );
    }

    SECTION( "local simulation ownership excludes the abstract writer" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( true, 15690 );
        bandit_live_world::site_record &site = world.sites.front();
        site.active_outing.owner = bandit_live_world::simulation_owner::local;
        int observer_calls = 0;
        const std::string before = serialize_world( world );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [&observer_calls]( const bandit_live_world::site_record &,
                          const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            observer_calls++;
            return make_abstract_threat_read( request.current_omt, 200 );
        } );
        CHECK( observer_calls == 0 );
        CHECK( serialize_world( world ) == before );
    }
}

TEST_CASE( "hostile_camp_below_gate_observation_does_not_stall_route_progress",
           "[bandit][live_world][structural_bounty][abstract_threat][phase4_observation]" )
{
    bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15700 );
    bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::advance_structural_bounty_outings(
        world, abstract_threat_test_stalking_minutes, {},
    []( const bandit_live_world::site_record &,
        const bandit_live_world::active_outing_state &,
    const bandit_live_world::structural_threat_observer_request & request ) {
        REQUIRE_FALSE( request.visible_forward_omts.empty() );
        return make_abstract_threat_read( request.visible_forward_omts.front(), 1, false );
    } );

    REQUIRE( site.active_outing.observations.size() == 1 );
    CHECK_FALSE( site.active_outing.observations.front().critical );
    CHECK( site.active_outing.phase == bandit_live_world::scout_phase::observing );
    CHECK( site.active_outing.local_contact_minutes == abstract_threat_test_stalking_minutes );
    CHECK( site.active_outing.last_advanced_minutes == abstract_threat_test_stalking_minutes );
}

TEST_CASE( "hostile_camp_abstract_overlap_uses_exact_party_power_boundaries_and_stable_casualties",
           "[bandit][live_world][structural_bounty][abstract_threat]" )
{
    enum class expected_outcome {
        wounded_pair,
        one_missing,
        all_missing,
    };
    const std::vector<std::pair<int, expected_outcome>> boundaries = {
        { -1, expected_outcome::wounded_pair },
        { 0, expected_outcome::one_missing },
        { 1, expected_outcome::one_missing },
        { 2, expected_outcome::all_missing },
    };
    int fixture = 0;
    for( const std::pair<int, expected_outcome> &boundary : boundaries ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world(
                    fixture % 2 != 0, 15650 + fixture * 10 );
        bandit_live_world::site_record &site = world.sites.front();
        const std::vector<character_id> members = site.active_outing.member_ids;
        REQUIRE( members.size() == 2 );
        const character_id stable_lower_id = std::min( members[0], members[1] );
        const character_id stable_other_id = std::max( members[0], members[1] );
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        REQUIRE( party_power > 0 );
        const int danger = boundary.first < 0 ? party_power - 1 :
                           boundary.first < 2 ? party_power +
                           boundary.first * ( party_power - 1 ) : 2 * party_power;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [danger]( const bandit_live_world::site_record &,
                 const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return make_abstract_threat_read( request.current_omt, danger );
        } );
        const bandit_live_world::member_record *lower = site.find_member( stable_lower_id );
        const bandit_live_world::member_record *other = site.find_member( stable_other_id );
        REQUIRE( lower != nullptr );
        REQUIRE( other != nullptr );
        REQUIRE( site.active_outing.abstract_encounter.outcome_applied );
        CHECK( site.active_outing.abstract_encounter.last_applied_episode == 1 );
        if( boundary.second == expected_outcome::wounded_pair ) {
            CHECK( danger == party_power - 1 );
            CHECK( lower->state == bandit_live_world::member_state::outbound );
            CHECK( lower->wounded_or_unready );
            CHECK( bandit_live_world::member_has_abstract_wound_recovery(
                       *lower, abstract_threat_test_stalking_minutes ) );
            CHECK_FALSE( bandit_live_world::member_has_abstract_wound_recovery(
                             *lower, lower->abstract_wound_until_minutes ) );
            CHECK( other->state == bandit_live_world::member_state::outbound );
            CHECK_FALSE( other->wounded_or_unready );
            CHECK( site.active_outing.abstract_encounter.outcome ==
                   "lower_power_member_wounded" );
        } else if( boundary.second == expected_outcome::one_missing ) {
            CHECK( ( danger == party_power || danger == 2 * party_power - 1 ) );
            CHECK( lower->state == bandit_live_world::member_state::missing );
            CHECK( other->state == bandit_live_world::member_state::outbound );
            CHECK( other->wounded_or_unready );
            CHECK( bandit_live_world::member_has_abstract_wound_recovery(
                       *other, abstract_threat_test_stalking_minutes ) );
            CHECK( site.active_outing.abstract_encounter.outcome ==
                   "one_missing_survivor_wounded" );
        } else {
            CHECK( danger == 2 * party_power );
            CHECK( lower->state == bandit_live_world::member_state::missing );
            CHECK( other->state == bandit_live_world::member_state::missing );
            CHECK( site.active_outing.phase == bandit_live_world::scout_phase::lost );
            CHECK( site.active_outing.abstract_encounter.outcome == "all_members_missing" );
        }
        fixture++;
    }

    SECTION( "lower capability precedes stable ID when selecting the wounded member" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15710 );
        bandit_live_world::site_record &site = world.sites.front();
        REQUIRE( site.active_outing.member_ids.size() == 2 );
        const character_id stronger_id = site.active_outing.member_ids[0];
        const character_id weaker_id = site.active_outing.member_ids[1];
        REQUIRE( site.find_member( stronger_id ) != nullptr );
        REQUIRE( site.find_member( weaker_id ) != nullptr );
        site.find_member( stronger_id )->npc_template_id = "hells_raiders_boss";
        site.find_member( weaker_id )->npc_template_id = "bandit_trader";
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [party_power]( const bandit_live_world::site_record &,
                      const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return make_abstract_threat_read( request.current_omt, party_power - 1 );
        } );
        CHECK_FALSE( site.find_member( stronger_id )->wounded_or_unready );
        CHECK( site.find_member( weaker_id )->wounded_or_unready );
    }
}

TEST_CASE( "hostile_camp_abstract_encounter_episode_is_once_only_local_exclusive_and_reentrant",
           "[bandit][live_world][structural_bounty][abstract_threat][save]" )
{
    SECTION( "continuous overlap survives save-load without applying a second outcome" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15730 );
        bandit_live_world::site_record &site = world.sites.front();
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        bandit_live_world::abstract_threat_read applied_read;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [&applied_read, party_power]( const bandit_live_world::site_record &,
                                     const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            applied_read = make_abstract_threat_read( request.current_omt, party_power );
            return applied_read;
        } );
        REQUIRE( site.active_outing.casualty_ids.size() == 1 );
        REQUIRE( site.active_outing.abstract_encounter.outcome_applied );
        const character_id casualty = site.active_outing.casualty_ids.front();
        bandit_live_world::world_state loaded = round_trip_world( world );
        bandit_live_world::site_record &loaded_site = loaded.sites.front();
        const std::string before_replay = serialize_world( loaded );
        const bandit_live_world::abstract_threat_resolution replay =
            bandit_live_world::resolve_structural_abstract_threat(
                loaded_site, loaded_site.active_outing.shared_route[
                    static_cast<std::size_t>( loaded_site.active_outing.waypoint_index )],
                applied_read, abstract_threat_test_stalking_minutes );
        CHECK( replay.valid );
        CHECK_FALSE( replay.changed );
        CHECK_FALSE( replay.outcome_applied );
        CHECK( loaded_site.active_outing.casualty_ids ==
               std::vector<character_id> { casualty } );
        CHECK( serialize_world( loaded ) == before_replay );
    }

    SECTION( "local reality owns the episode and abstract resolution cannot add casualties" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( true, 15750 );
        bandit_live_world::site_record &site = world.sites.front();
        bandit_live_world::abstract_threat_read overlap;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [&overlap]( const bandit_live_world::site_record &,
                    const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            overlap = make_abstract_threat_read( request.current_omt, 200, true, true );
            return overlap;
        } );
        REQUIRE( site.active_outing.abstract_encounter.active );
        CHECK_FALSE( site.active_outing.abstract_encounter.local_claimed );
        CHECK_FALSE( site.active_outing.abstract_encounter.outcome_applied );
        CHECK( site.active_outing.casualty_ids.empty() );

        const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( cursor );
        const tripoint_abs_ms origin = project_to<coords::ms>(
                                           site.active_outing.shared_route[
                                               static_cast<std::size_t>(
                                                   site.active_outing.waypoint_index )] );
        std::vector<bandit_live_world::local_handoff_member_read> member_reads;
        for( std::size_t index = 0; index < site.active_outing.member_ids.size(); ++index ) {
            const character_id member_id = site.active_outing.member_ids[index];
            const bandit_live_world::member_record *member = site.find_member( member_id );
            REQUIRE( member != nullptr );
            bandit_live_world::local_handoff_member_read read;
            read.npc_id = member_id;
            read.bindable = true;
            read.hp_percent = 100;
            read.current_position = member->home_spawn_tile;
            read.entry_position = tripoint_abs_ms( origin.x() + static_cast<int>( index ),
                                                   origin.y(), origin.z() );
            read.staging_position = tripoint_abs_ms( origin.x() + static_cast<int>( index ),
                                                     origin.y() + 4, origin.z() );
            member_reads.push_back( read );
        }
        const bandit_live_world::local_handoff_plan handoff =
            bandit_live_world::plan_local_pair_handoff(
                site, *cursor, abstract_threat_test_stalking_minutes, member_reads );
        REQUIRE( handoff.valid );
        REQUIRE( bandit_live_world::commit_local_pair_handoff(
                     site, handoff,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );
        REQUIRE( site.active_outing.abstract_encounter.local_claimed );

        overlap.local_reality = false;
        const bandit_live_world::abstract_threat_resolution while_local =
            bandit_live_world::resolve_structural_abstract_threat(
                site, site.active_outing.shared_route[
                    static_cast<std::size_t>( site.active_outing.waypoint_index )], overlap,
                abstract_threat_test_stalking_minutes );
        CHECK_FALSE( while_local.valid );
        CHECK( site.active_outing.casualty_ids.empty() );
        for( const character_id &member_id : site.active_outing.member_ids ) {
            REQUIRE( site.find_member( member_id ) != nullptr );
            CHECK_FALSE( site.find_member( member_id )->wounded_or_unready );
        }

        std::optional<bandit_live_world::simulation_advance_cursor> local_cursor =
            bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( local_cursor );
        std::vector<bandit_live_world::local_cohesion_member_read> cohesion_reads;
        for( const bandit_live_world::local_handoff_member_snapshot &member :
             site.active_outing.local_handoff.members ) {
            bandit_live_world::local_cohesion_member_read read;
            read.npc_id = member.npc_id;
            read.present = true;
            read.current_position = member.staging_position;
            cohesion_reads.push_back( read );
        }
        const bandit_live_world::local_cohesion_plan cohesion =
            bandit_live_world::plan_local_pair_cohesion(
                site, *local_cursor, abstract_threat_test_stalking_minutes, cohesion_reads );
        REQUIRE( cohesion.valid );
        REQUIRE( cohesion.snapshot.cohesion_assembled );
        REQUIRE( bandit_live_world::commit_local_pair_cohesion(
                     site, cohesion, false, false ) );

        local_cursor = bandit_live_world::current_external_simulation_cursor( site );
        REQUIRE( local_cursor );
        std::vector<bandit_live_world::local_dematerialization_member_read> exit_reads;
        for( const bandit_live_world::local_handoff_member_snapshot &member :
             site.active_outing.local_handoff.members ) {
            bandit_live_world::local_dematerialization_member_read read;
            read.npc_id = member.npc_id;
            read.readable = true;
            read.hp_percent = 100;
            read.current_position = member.staging_position;
            exit_reads.push_back( read );
        }
        const bandit_live_world::local_dematerialization_plan dematerialization =
            bandit_live_world::plan_local_pair_dematerialization(
                site, *local_cursor, abstract_threat_test_stalking_minutes + 1,
                exit_reads, site.active_outing.cargo );
        REQUIRE( dematerialization.valid );
        REQUIRE( bandit_live_world::commit_local_pair_dematerialization(
                     site, dematerialization,
        []( const bandit_live_world::local_handoff_member_snapshot & ) {
            return true;
        }, []( const bandit_live_world::local_handoff_member_snapshot & ) {} ) ==
                 bandit_live_world::local_handoff_commit_result::applied );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
        CHECK_FALSE( site.active_outing.abstract_encounter.local_claimed );
        CHECK( site.active_outing.abstract_encounter.outcome_applied );
        CHECK( site.active_outing.abstract_encounter.outcome == "resolved_by_local_reality" );

        const bandit_live_world::abstract_threat_resolution after_local =
            bandit_live_world::resolve_structural_abstract_threat(
                site, site.active_outing.shared_route[
                    static_cast<std::size_t>( site.active_outing.waypoint_index )], overlap,
                abstract_threat_test_stalking_minutes + 1 );
        CHECK( after_local.valid );
        CHECK_FALSE( after_local.changed );
        CHECK( site.active_outing.casualty_ids.empty() );
    }

    SECTION( "one absent segment latches before clearing and a later overlap opens a new episode" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15770 );
        bandit_live_world::site_record &site = world.sites.front();
        bandit_live_world::abstract_threat_read overlap;
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [&overlap]( const bandit_live_world::site_record &,
                    const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            overlap = make_abstract_threat_read( request.current_omt, 10, true, true );
            return overlap;
        } );
        REQUIRE( site.active_outing.abstract_encounter.episode == 1 );
        site.active_outing.phase = bandit_live_world::scout_phase::returning_home;
        site.active_outing.waypoint_index = 2;
        site.active_outing.last_advanced_minutes = abstract_threat_test_stalking_minutes + 1;
        bandit_live_world::abstract_threat_read absent;
        bandit_live_world::abstract_threat_resolution first_absence =
            bandit_live_world::resolve_structural_abstract_threat(
                site, site.active_outing.shared_route[2], absent,
                abstract_threat_test_stalking_minutes + 1 );
        REQUIRE( first_absence.valid );
        CHECK( first_absence.changed );
        CHECK_FALSE( first_absence.encounter_cleared );
        CHECK( site.active_outing.abstract_encounter.absent_segment_advances == 1 );

        world = round_trip_world( world );
        bandit_live_world::site_record &loaded_site = world.sites.front();
        loaded_site.active_outing.waypoint_index = 0;
        loaded_site.active_outing.last_advanced_minutes = abstract_threat_test_stalking_minutes + 2;
        const bandit_live_world::abstract_threat_resolution cleared =
            bandit_live_world::resolve_structural_abstract_threat(
                loaded_site, loaded_site.active_outing.shared_route[0], absent,
                abstract_threat_test_stalking_minutes + 2 );
        REQUIRE( cleared.valid );
        CHECK( cleared.encounter_cleared );
        CHECK_FALSE( loaded_site.active_outing.abstract_encounter.active );
        CHECK( loaded_site.active_outing.abstract_encounter.episode == 1 );

        loaded_site.active_outing.waypoint_index = 1;
        loaded_site.active_outing.last_advanced_minutes = abstract_threat_test_stalking_minutes + 3;
        overlap.threat_omt = loaded_site.active_outing.shared_route[1];
        const bandit_live_world::abstract_threat_resolution reencounter =
            bandit_live_world::resolve_structural_abstract_threat(
                loaded_site, loaded_site.active_outing.shared_route[1], overlap,
                abstract_threat_test_stalking_minutes + 3 );
        REQUIRE( reencounter.valid );
        CHECK( reencounter.encounter_started );
        CHECK( loaded_site.active_outing.abstract_encounter.episode == 2 );
        CHECK_FALSE( loaded_site.active_outing.abstract_encounter.local_claimed );
    }
}

TEST_CASE( "hostile_camp_abstract_missing_outcomes_round_trip_and_close_at_their_deadlines",
           "[bandit][live_world][structural_bounty][abstract_threat][save]" )
{
    SECTION( "one missing leaves a wounded survivor who returns home" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( true, 15800 );
        bandit_live_world::site_record &site = world.sites.front();
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [party_power]( const bandit_live_world::site_record &,
                      const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return make_abstract_threat_read( request.current_omt, party_power );
        } );
        REQUIRE( site.active_outing.casualty_ids.size() == 1 );
        const character_id missing_id = site.active_outing.casualty_ids.front();
        const character_id survivor_id = site.active_outing.member_ids[0] == missing_id ?
                                         site.active_outing.member_ids[1] :
                                         site.active_outing.member_ids[0];
        const int return_minutes = site.active_outing.expected_return_minutes;
        world = round_trip_world( world );
        CHECK( world.sites.front().active_outing.abstract_encounter.outcome ==
               "one_missing_survivor_wounded" );
        const bandit_live_world::structural_outing_result returned =
            bandit_live_world::advance_structural_bounty_outings( world, return_minutes, {} );
        CHECK( returned.members_returned == 1 );
        const bandit_live_world::site_record &returned_site = world.sites.front();
        CHECK( returned_site.active_outing.kind == bandit_live_world::outing_kind::none );
        REQUIRE( returned_site.find_member( missing_id ) != nullptr );
        REQUIRE( returned_site.find_member( survivor_id ) != nullptr );
        CHECK( returned_site.find_member( missing_id )->state ==
               bandit_live_world::member_state::missing );
        CHECK( returned_site.find_member( survivor_id )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( returned_site.find_member( survivor_id )->wounded_or_unready );
        CHECK( bandit_live_world::member_has_abstract_wound_recovery(
                   *returned_site.find_member( survivor_id ), return_minutes ) );
        CHECK( returned_site.next_routine_dispatch_eligible_minutes >=
               return_minutes + 72 * 60 );
        CHECK( returned_site.next_routine_dispatch_eligible_minutes <=
               return_minutes + 78 * 60 );
    }

    SECTION( "all missing remains lost through reload and releases at the missing deadline" ) {
        bandit_live_world::world_state world = make_abstract_threat_test_world( false, 15820 );
        bandit_live_world::site_record &site = world.sites.front();
        const int party_power = bandit_live_world::structural_outing_party_power( site );
        bandit_live_world::advance_structural_bounty_outings(
            world, abstract_threat_test_stalking_minutes, {},
        [party_power]( const bandit_live_world::site_record &,
                      const bandit_live_world::active_outing_state &,
        const bandit_live_world::structural_threat_observer_request & request ) {
            return make_abstract_threat_read( request.current_omt, 2 * party_power );
        } );
        REQUIRE( site.active_outing.phase == bandit_live_world::scout_phase::lost );
        REQUIRE( site.active_outing.casualty_ids.size() == 2 );
        const std::vector<character_id> missing_ids = site.active_outing.casualty_ids;
        const int deadline = site.active_outing.missing_deadline_minutes;
        world = round_trip_world( world );
        const bandit_live_world::structural_outing_result waiting =
            bandit_live_world::advance_structural_bounty_outings( world, deadline - 1, {} );
        CHECK( waiting.active_outings_considered == 1 );
        CHECK( world.sites.front().active_outing.phase == bandit_live_world::scout_phase::lost );
        const bandit_live_world::structural_outing_result closed =
            bandit_live_world::advance_structural_bounty_outings( world, deadline, {} );
        CHECK( closed.active_outings_considered == 1 );
        CHECK( world.sites.front().active_outing.kind == bandit_live_world::outing_kind::none );
        for( const character_id &missing_id : missing_ids ) {
            REQUIRE( world.sites.front().find_member( missing_id ) != nullptr );
            CHECK( world.sites.front().find_member( missing_id )->state ==
                   bandit_live_world::member_state::missing );
        }
    }
}

TEST_CASE( "abstract_encounter_v1_rejects_malformed_v8_threat_state",
           "[bandit][live_world][structural_bounty][abstract_threat][save]" )
{
    bandit_live_world::abstract_encounter_state valid;
    valid.episode = 1;
    valid.active = true;
    valid.overlap_omt = tripoint_abs_omt( 10, 20, 0 );
    valid.stable_threat_ids = { "horde:a", "horde:b" };
    valid.danger_low = 10;
    valid.danger_high = 20;

    const auto rejects = []( const bandit_live_world::abstract_encounter_state & state ) {
        JsonValue input = json_loader::from_string( serialize_abstract_encounter( state ) );
        bandit_live_world::abstract_encounter_state loaded;
        CHECK_THROWS( loaded.deserialize( input.get_object() ) );
    };

    bandit_live_world::abstract_encounter_state bad_ids = valid;
    bad_ids.stable_threat_ids = { "horde:b", "horde:a" };
    rejects( bad_ids );

    bandit_live_world::abstract_encounter_state bad_danger = valid;
    bad_danger.danger_high = 201;
    rejects( bad_danger );

    bandit_live_world::abstract_encounter_state bad_detours = valid;
    bad_detours.detour_attempts = 3;
    rejects( bad_detours );
}

TEST_CASE( "bandit_live_world_keeps_a_home_reserve_for_site_backed_camps", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 601 );

    bandit_live_world::site_record &camp = world.sites.front();
    CHECK( camp.count_live_members() == 1 );
    CHECK( camp.dispatchable_member_capacity() == 0 );

    const bandit_live_world::dispatch_plan blocked_plan =
        bandit_live_world::plan_site_dispatch( camp, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    CHECK_FALSE( blocked_plan.valid );
    REQUIRE_FALSE( blocked_plan.notes.empty() );
    CHECK( blocked_plan.notes.back().find( "home reserve" ) != std::string::npos );

    bandit_live_world::world_state roadside_world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( roadside_world, "bandit", character_id( 602 ),
             tripoint_abs_ms( 120, 96, 0 ), std::nullopt, std::string( "mx_looters" ),
             special_lookup ) );
    const bandit_live_world::site_record &roadside_site = roadside_world.sites.front();
    CHECK( roadside_site.dispatchable_member_capacity() == 1 );
}

TEST_CASE( "bandit_live_world_bandit_camp_reserve_scales_with_living_roster",
           "[bandit][live_world][camp_map]" )
{
    const std::vector<std::pair<int, int>> expected_dispatchable = {
        { 2, 1 },
        { 4, 3 },
        { 5, 3 },
        { 7, 5 },
        { 10, 6 },
    };

    for( const std::pair<int, int> &row : expected_dispatchable ) {
        bandit_live_world::world_state world;
        for( int i = 0; i < row.first; ++i ) {
            add_bandit_camp_member( world, i, 11000 + row.first * 100 );
        }
        REQUIRE( world.sites.size() == 1 );
        const bandit_live_world::site_record &site = world.sites.front();
        CHECK( site.count_live_members() == row.first );
        CHECK( site.dispatchable_member_capacity() == row.second );
    }
}

TEST_CASE( "bandit_live_world_two_bandit_camp_uses_zero_reserve_only_for_routine_pair",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 2; ++i ) {
        add_bandit_camp_member( world, i, 11950 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.dispatchable_member_capacity() == 1 );

    bandit_live_world::camp_map_lead uncertain;
    uncertain.status = bandit_live_world::camp_lead_status::stale;
    uncertain.bounty = 8;
    uncertain.threat = 1;
    uncertain.confidence = 1;
    const bandit_live_world::camp_map_dispatch_decision uncertain_decision =
        bandit_live_world::choose_camp_map_dispatch( site, uncertain );
    CHECK( uncertain_decision.intent == bandit_dry_run::job_template::scout );
    CHECK( uncertain_decision.hard_home_reserve == 0 );
    CHECK( uncertain_decision.dispatchable == 2 );
    CHECK( uncertain_decision.selected_member_count == 2 );

    bandit_live_world::camp_map_lead confirmed;
    confirmed.lead_id = "confirmed_basecamp@18,20,0";
    confirmed.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    confirmed.status = bandit_live_world::camp_lead_status::scout_confirmed;
    confirmed.target_id = "player@18,20,0";
    confirmed.omt = tripoint_abs_omt( 18, 20, 0 );
    confirmed.bounty = 8;
    confirmed.threat = 1;
    confirmed.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision confirmed_decision =
        bandit_live_world::choose_camp_map_dispatch( site, confirmed );
    CHECK( confirmed_decision.intent == bandit_dry_run::job_template::scout );
    CHECK( confirmed_decision.hard_home_reserve == 0 );
    CHECK( confirmed_decision.dispatchable == 2 );
    CHECK( confirmed_decision.selected_member_count == 2 );

    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, confirmed );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
    CHECK( plan.member_ids.size() == 2 );

    const bandit_live_world::response_party_policy_result response =
        bandit_live_world::response_party_policy(
            site, bandit_dry_run::job_template::stalk, 2 );
    CHECK_FALSE( response.eligible );
    CHECK( response.required_local_reserve == 1 );

    const bandit_live_world::hostile_operation_plan operation =
        bandit_live_world::plan_hostile_operation(
            site, bandit_live_world::hostile_operation_kind::shakedown,
            { site.anchor, confirmed.omt }, site.anchor, 102 );
    CHECK_FALSE( operation.valid );
    CHECK_FALSE( site.active_hostile_operation.is_active() );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 2 );
}

TEST_CASE( "bandit_live_world_scout_confirmed_basecamp_keeps_small_roster_to_stalk",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 11970 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 3 );
    site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "confirmed_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 3 );
    CHECK( decision.selected_member_count == 2 );
}

TEST_CASE( "bandit_live_world_scout_confirmed_basecamp_promotes_to_toll_party",
           "[bandit][live_world][camp_map][shakedown]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 7; ++i ) {
        add_bandit_camp_member( world, i, 11980 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 5 );
    site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "confirmed_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.revision = 3;
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;
    site.intelligence_map.leads.push_back( lead );

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::toll );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 5 );
    CHECK( decision.selected_member_count == 3 );

    prepare_hostile_follow_on( site, 2, 1, lead.target_id, lead.omt, 100,
                               lead.lead_id );
    const bandit_live_world::response_party_selection_result response =
        bandit_live_world::select_fresh_response_party(
            site, bandit_live_world::hostile_operation_kind::shakedown );
    REQUIRE( response.eligible );
    CHECK( response.threat_derived );
    CHECK( response.party_size == 3 );

    bandit_live_world::world_state stale_lead_world = world;
    bandit_live_world::site_record &stale_lead_site = stale_lead_world.sites.front();
    stale_lead_site.intelligence_map.leads.front().omt = tripoint_abs_omt( 19, 20, 0 );
    const std::string before_stale_selection = serialize_world( stale_lead_world );
    CHECK_FALSE( bandit_live_world::select_fresh_response_party(
                     stale_lead_site,
                     bandit_live_world::hostile_operation_kind::shakedown ).eligible );
    CHECK( serialize_world( stale_lead_world ) == before_stale_selection );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::toll );
    CHECK( plan.member_ids.size() == 3 );

    apply_test_hostile_dispatch( site, plan, 102 );
    CHECK( site.active_hostile_operation.reservation.job_type == "toll" );
    CHECK( site.active_hostile_operation.reservation.member_ids.size() == 3 );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 4 );

    bandit_live_world::local_gate_input input;
    input.local_threat = 1;
    input.local_opportunity = 2;
    input.local_contact_established = true;
    const bandit_live_world::local_gate_decision gate =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( gate.valid );
    CHECK( gate.posture == bandit_live_world::local_gate_posture::open_shakedown );
    CHECK( gate.shakedown_capable );
    CHECK( gate.opens_shakedown_surface );
    CHECK_FALSE( gate.combat_forward );
}

TEST_CASE( "bandit_live_world_cannibal_scout_confirmation_promotes_to_attack_pack",
           "[bandit][live_world][camp_map][cannibal]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 6; ++i ) {
        add_cannibal_camp_member( world, i, 11990 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    REQUIRE( site.dispatchable_member_capacity() == 4 );

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "confirmed_basecamp@72,80,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@72,80,0";
    lead.omt = tripoint_abs_omt( 72, 80, 0 );
    lead.revision = 3;
    lead.bounty = 9;
    lead.threat = 1;
    lead.confidence = 3;
    site.intelligence_map.leads.push_back( lead );

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::raid );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 4 );
    CHECK( decision.selected_member_count == 4 );

    prepare_hostile_follow_on( site, 2, 1, lead.target_id, lead.omt, 100,
                               lead.lead_id );
    const bandit_live_world::response_party_selection_result response =
        bandit_live_world::select_fresh_response_party(
            site, bandit_live_world::hostile_operation_kind::raid );
    REQUIRE( response.eligible );
    CHECK( response.threat_derived );
    CHECK( response.party_size == 4 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::raid );
    CHECK( plan.member_ids.size() == 4 );

    apply_test_hostile_dispatch( site, plan, 102 );
    CHECK( site.active_hostile_operation.operation_kind ==
           bandit_live_world::hostile_operation_kind::raid );
    CHECK( site.active_hostile_operation.reservation.job_type == "raid" );
    CHECK( site.active_hostile_operation.reservation.member_ids.size() == 4 );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 2 );

    bandit_live_world::local_gate_input input;
    input.local_threat = 2;
    input.local_opportunity = 1;
    input.local_contact_established = true;
    input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision gate =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( gate.valid );
    CHECK( gate.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK( gate.combat_forward );
    CHECK_FALSE( gate.opens_shakedown_surface );
}

TEST_CASE( "bandit_live_world_active_outside_group_blocks_parallel_dispatch",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 12000 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 3 );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 12000 ),
             bandit_live_world::member_state::outbound, "already outside" ) );
    site.active_outing.member_ids = { character_id( 12000 ) };
    set_test_active_outing( site, site.site_id + "#already_outside" );

    bandit_live_world::camp_map_lead lead;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;
    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( decision.active_outside == 1 );

    const bandit_live_world::dispatch_plan blocked_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                                               "player_basecamp_nearby" );
    CHECK_FALSE( blocked_plan.valid );
    REQUIRE_FALSE( blocked_plan.notes.empty() );
    CHECK( blocked_plan.notes.front().find( "active outside" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_wounded_and_killed_members_shrink_camp_map_dispatch",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 7; ++i ) {
        add_bandit_camp_member( world, i, 12100 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 5 );
    REQUIRE( site.find_member( character_id( 12101 ) ) != nullptr );
    site.find_member( character_id( 12101 ) )->wounded_or_unready = true;
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 12102 ),
             bandit_live_world::member_state::dead, "killed before the next pressure beat" ) );

    CHECK( site.count_live_members() == 6 );
    CHECK( site.dispatchable_member_capacity() == 3 );

    bandit_live_world::camp_map_lead lead;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.bounty = 6;
    lead.threat = 2;
    lead.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.ready_at_home == 5 );
    CHECK( decision.wounded_or_unready == 1 );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 3 );
    CHECK( decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( decision.selected_member_count == 2 );
}

TEST_CASE( "bandit_live_world_camp_map_risk_reward_handles_pressure_and_cooling",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 12200 );
    }
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::camp_map_lead dangerous;
    dangerous.status = bandit_live_world::camp_lead_status::scout_confirmed;
    dangerous.bounty = 1;
    dangerous.threat = 6;
    dangerous.confidence = 2;
    const bandit_live_world::camp_map_dispatch_decision dangerous_decision =
        bandit_live_world::choose_camp_map_dispatch( site, dangerous );
    CHECK( dangerous_decision.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( dangerous_decision.selected_member_count == 0 );

    bandit_live_world::camp_map_lead hungry;
    hungry.status = bandit_live_world::camp_lead_status::scout_confirmed;
    hungry.bounty = 2;
    hungry.threat = 2;
    hungry.confidence = 2;
    bandit_live_world::camp_map_dispatch_pressure hungry_pressure;
    hungry_pressure.stockpile_pressure = 3;
    const bandit_live_world::camp_map_dispatch_decision hungry_decision =
        bandit_live_world::choose_camp_map_dispatch( site, hungry, hungry_pressure );
    CHECK( hungry_decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( hungry_decision.hard_home_reserve == 2 );
    CHECK( hungry_decision.dispatchable == 3 );
    CHECK( hungry_decision.selected_member_count == 2 );

    bandit_live_world::camp_map_lead defender_loss;
    defender_loss.status = bandit_live_world::camp_lead_status::scout_confirmed;
    defender_loss.bounty = 1;
    defender_loss.threat = 4;
    defender_loss.confidence = 2;
    const bandit_live_world::camp_map_dispatch_decision before_defender_loss =
        bandit_live_world::choose_camp_map_dispatch( site, defender_loss );
    defender_loss.prior_defender_losses = 2;
    const bandit_live_world::camp_map_dispatch_decision after_defender_loss =
        bandit_live_world::choose_camp_map_dispatch( site, defender_loss );
    CHECK( before_defender_loss.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( after_defender_loss.intent == bandit_dry_run::job_template::stalk );
    CHECK( after_defender_loss.selected_member_count == 2 );

    bandit_live_world::camp_map_lead no_opening;
    no_opening.status = bandit_live_world::camp_lead_status::active;
    no_opening.bounty = 7;
    no_opening.threat = 1;
    no_opening.confidence = 3;
    bandit_live_world::camp_map_dispatch_pressure no_opening_pressure;
    no_opening_pressure.opening_available = false;
    const bandit_live_world::camp_map_dispatch_decision no_opening_decision =
        bandit_live_world::choose_camp_map_dispatch( site, no_opening, no_opening_pressure );
    CHECK( no_opening_decision.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( no_opening_decision.selected_member_count == 0 );
}

TEST_CASE( "bandit_live_world_prior_bandit_losses_cool_large_camp_pressure",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 10; ++i ) {
        add_bandit_camp_member( world, i, 12300 );
    }
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::camp_map_lead lead;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.bounty = 8;
    lead.threat = 2;
    lead.confidence = 3;
    const bandit_live_world::camp_map_dispatch_decision clean_pressure =
        bandit_live_world::choose_camp_map_dispatch( site, lead );

    lead.prior_bandit_losses = 2;
    const bandit_live_world::camp_map_dispatch_decision cooled_pressure =
        bandit_live_world::choose_camp_map_dispatch( site, lead );

    CHECK( clean_pressure.intent == bandit_dry_run::job_template::stalk );
    CHECK( clean_pressure.hard_home_reserve == 4 );
    CHECK( clean_pressure.dispatchable == 6 );
    CHECK( clean_pressure.selected_member_count == 3 );
    CHECK( cooled_pressure.intent == bandit_dry_run::job_template::stalk );
    CHECK( cooled_pressure.hard_home_reserve == 5 );
    CHECK( cooled_pressure.dispatchable == 5 );
    CHECK( cooled_pressure.selected_member_count == 2 );
    CHECK( cooled_pressure.risk_score > clean_pressure.risk_score );
}

TEST_CASE( "bandit_live_world_dispatch_rules_are_driven_by_hostile_site_profile", "[bandit][live_world][profile]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 650 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 651 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 652 ),
             tripoint_abs_ms( 168, 120, 0 ), std::nullopt, std::string( "mx_bandits_block" ),
             special_lookup ) );

    bandit_live_world::site_record &camp =
        *world.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &roadblock =
        *world.find_site( "map_extra:mx_bandits_block@7,5,0" );

    CHECK( camp.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( roadblock.profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( camp.dispatchable_member_capacity() == 1 );
    CHECK( roadblock.dispatchable_member_capacity() == 1 );

    const bandit_live_world::dispatch_plan camp_plan =
        bandit_live_world::plan_site_dispatch( camp, tripoint_abs_omt( 18, 20, 0 ),
                                               "player@18,20,0" );
    REQUIRE( camp_plan.valid );
    CHECK( camp_plan.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( camp_plan.group.retreat_bias == 1 );
    CHECK( camp_plan.group.return_clock == 2 );
    CHECK( camp_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::ample );
    REQUIRE( camp_plan.notes.size() >= 2 );
    CHECK( camp_plan.notes[camp_plan.notes.size() - 2].find( "persistent camp pressure" ) !=
           std::string::npos );

    const bandit_live_world::dispatch_plan roadblock_plan =
        bandit_live_world::plan_site_dispatch( roadblock, tripoint_abs_omt( 8, 5, 0 ),
                                               "player@8,5,0" );
    REQUIRE( roadblock_plan.valid );
    CHECK( roadblock_plan.profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( roadblock_plan.group.retreat_bias == 2 );
    CHECK( roadblock_plan.group.return_clock == 1 );
    CHECK( roadblock_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( roadblock_plan.notes.size() >= 2 );
    CHECK( roadblock_plan.notes[roadblock_plan.notes.size() - 2].find( "brittle local pressure" ) !=
           std::string::npos );
}

TEST_CASE( "bandit_live_world_keeps_cannibal_camp_separate_from_bandit_camp_ownership", "[bandit][live_world][profile][cannibal]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 660 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 661 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 760 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 761 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 762 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 763 ),
             tripoint_abs_ms( 1705, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    REQUIRE( world.sites.size() == 2 );
    bandit_live_world::site_record &bandit_camp =
        *world.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &cannibal_camp =
        *world.find_site( "overmap_special:cannibal_camp@70,80,0" );

    CHECK( bandit_camp.site_kind == bandit_live_world::owned_site_kind::bandit_camp );
    CHECK( bandit_camp.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( cannibal_camp.site_kind == bandit_live_world::owned_site_kind::cannibal_camp );
    CHECK( cannibal_camp.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( bandit_camp.dispatchable_member_capacity() == 1 );
    CHECK( cannibal_camp.dispatchable_member_capacity() == 2 );
    REQUIRE( cannibal_camp.footprint.size() == 4 );
    CHECK( cannibal_camp.footprint.front() == tripoint_abs_omt( 70, 80, 0 ) );
    CHECK( cannibal_camp.footprint.back() == tripoint_abs_omt( 71, 81, 0 ) );

    const bandit_live_world::dispatch_plan bandit_plan =
        bandit_live_world::plan_site_dispatch( bandit_camp, tripoint_abs_omt( 18, 20, 0 ),
                                               "player@18,20,0" );
    REQUIRE( bandit_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( bandit_camp, bandit_plan ) );

    prepare_hostile_follow_on( cannibal_camp, 2, 1, "player@72,80,0",
                               tripoint_abs_omt( 72, 80, 0 ), 100 );
    const bandit_live_world::dispatch_plan cannibal_plan =
        bandit_live_world::plan_site_dispatch( cannibal_camp, tripoint_abs_omt( 72, 80, 0 ),
                                               "player@72,80,0" );
    REQUIRE( cannibal_plan.valid );
    CHECK( cannibal_plan.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( cannibal_plan.entry.job_type == bandit_dry_run::job_template::stalk );
    CHECK( cannibal_plan.group.retreat_bias == 3 );
    CHECK( cannibal_plan.group.return_clock == 3 );
    CHECK( cannibal_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( cannibal_plan.notes.size() >= 3 );
    std::string cannibal_plan_notes;
    for( const std::string &note : cannibal_plan.notes ) {
        cannibal_plan_notes += note + "\n";
    }
    CHECK( cannibal_plan_notes.find( "hungry camp pressure" ) != std::string::npos );
    CHECK( cannibal_plan_notes.find( "pack_size 2" ) != std::string::npos );
    apply_test_hostile_dispatch( cannibal_camp, cannibal_plan, 102 );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    REQUIRE( loaded.sites.size() == 2 );
    const bandit_live_world::site_record *loaded_bandit =
        loaded.find_site( "overmap_special:bandit_camp@10,20,0" );
    const bandit_live_world::site_record *loaded_cannibal =
        loaded.find_site( "overmap_special:cannibal_camp@70,80,0" );
    REQUIRE( loaded_bandit != nullptr );
    REQUIRE( loaded_cannibal != nullptr );

    CHECK( loaded_bandit->profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( loaded_bandit->active_outing.activity_id == "overmap_special:bandit_camp@10,20,0#dispatch" );
    CHECK( loaded_bandit->active_outing.target_id == "player@18,20,0" );
    REQUIRE( loaded_bandit->active_outing.member_ids ==
             std::vector<character_id>( { character_id( 660 ), character_id( 661 ) } ) );
    CHECK( loaded_bandit->find_member( character_id( 661 ) )->state ==
           bandit_live_world::member_state::outbound );

    CHECK( loaded_cannibal->profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( loaded_cannibal->active_hostile_operation.reservation.activity_id ==
           "overmap_special:cannibal_camp@70,80,0#hostile:2" );
    CHECK( loaded_cannibal->active_hostile_operation.reservation.target_id == "player@72,80,0" );
    REQUIRE( loaded_cannibal->active_hostile_operation.reservation.member_ids ==
             std::vector<character_id>( { character_id( 760 ), character_id( 761 ) } ) );
    CHECK( loaded_cannibal->find_member( character_id( 762 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_cannibal->find_member( character_id( 763 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_blocks_lone_cannibal_pack_pressure", "[bandit][live_world][profile][cannibal]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 770 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 771 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 772 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( site.dispatchable_member_capacity() == 1 );

    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
    REQUIRE( plan.member_ids ==
             std::vector<character_id>( { character_id( 770 ), character_id( 771 ) } ) );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    CHECK( site.active_outing.job_type == "scout" );

    bandit_live_world::local_gate_input input;
    input.local_threat = 1;
    input.local_opportunity = 3;
    input.local_contact_established = true;
    input.darkness_or_concealment = true;
    const bandit_live_world::local_gate_decision decision =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( decision.combat_forward );

    site.active_outing.job_type = "raid";
    const bandit_live_world::local_gate_decision disguised_attack_decision =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( disguised_attack_decision.valid );
    CHECK( disguised_attack_decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( disguised_attack_decision.combat_forward );
}

TEST_CASE( "bandit_live_world_darkness_can_turn_cannibal_pack_contact_into_attack", "[bandit][live_world][profile][cannibal]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 780 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 781 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 782 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 783 ),
             tripoint_abs_ms( 1705, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    prepare_hostile_follow_on( site, 2, 1, "player@72,80,0",
                               tripoint_abs_omt( 72, 80, 0 ), 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    apply_test_hostile_dispatch( site, plan, 102 );
    REQUIRE( site.active_hostile_operation.reservation.member_ids.size() == 2 );

    bandit_live_world::local_gate_input daylight;
    daylight.local_threat = 3;
    daylight.local_opportunity = 1;
    daylight.local_contact_established = true;
    daylight.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision daylight_decision =
        bandit_live_world::choose_local_gate_posture( site, daylight );
    CHECK( daylight_decision.valid );
    CHECK( daylight_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( daylight_decision.combat_forward );

    bandit_live_world::local_gate_input dark = daylight;
    dark.darkness_or_concealment = true;
    const bandit_live_world::local_gate_decision dark_decision =
        bandit_live_world::choose_local_gate_posture( site, dark );
    CHECK( dark_decision.valid );
    CHECK( dark_decision.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK( dark_decision.combat_forward );
    const std::string dark_report = bandit_live_world::render_local_gate_report( site, dark,
                                    dark_decision );
    CHECK( dark_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( dark_report.find( "pack_size=2" ) != std::string::npos );
    CHECK( dark_report.find( "darkness_or_concealment=yes" ) != std::string::npos );
    CHECK( dark_report.find( "posture=attack_now" ) != std::string::npos );

    bandit_live_world::local_gate_input high_threat = dark;
    high_threat.local_threat = 8;
    high_threat.local_opportunity = 0;
    const bandit_live_world::local_gate_decision high_threat_decision =
        bandit_live_world::choose_local_gate_posture( site, high_threat );
    CHECK_FALSE( high_threat_decision.valid );
    CHECK( high_threat_decision.posture == bandit_live_world::local_gate_posture::abort );
}

TEST_CASE( "bandit_live_world_writeback_shrinks_living_total_and_future_dispatch_capacity",
           "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 701 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 702 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 703 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.living_total == 3 );
    CHECK( site.count_live_members() == 3 );
    CHECK( site.dispatchable_member_capacity() == 2 );
    REQUIRE( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) ) != nullptr );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) )->assigned_living_total == 1 );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 702 ),
             bandit_live_world::member_state::dead, "killed during local contact" ) );
    CHECK( site.living_total == 2 );
    CHECK( site.count_live_members() == 2 );
    CHECK( site.dispatchable_member_capacity() == 1 );
    CHECK( site.find_member( character_id( 702 ) )->last_writeback_summary ==
           "killed during local contact" );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) )->assigned_living_total == 0 );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 703 ),
             bandit_live_world::member_state::missing, "never returned from scout" ) );
    CHECK( site.living_total == 1 );
    CHECK( site.count_live_members() == 1 );
    CHECK( site.dispatchable_member_capacity() == 0 );

    const bandit_live_world::dispatch_plan blocked_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    CHECK_FALSE( blocked_plan.valid );
    REQUIRE_FALSE( blocked_plan.notes.empty() );
    CHECK( blocked_plan.notes.back().find( "home reserve" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_chooses_reviewer_readable_local_approach_gate_posture", "[bandit][live_world][approach_gate]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 901 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 902 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 903 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_live_world::local_gate_input stalk_input;
    stalk_input.local_threat = 2;
    stalk_input.local_opportunity = 0;
    bandit_live_world::local_gate_decision decision =
        bandit_live_world::choose_local_gate_posture( site, stalk_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::stalk );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );

    bandit_live_world::local_gate_input camp_input;
    camp_input.local_threat = 3;
    camp_input.local_opportunity = 2;
    camp_input.standoff_distance = 10;
    camp_input.basecamp_or_camp_scene = true;
    camp_input.recent_exposure = true;
    decision = bandit_live_world::choose_local_gate_posture( site, camp_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );
    const std::string camp_report = bandit_live_world::render_local_gate_report( site, camp_input,
                                    decision );
    CHECK( camp_report.find( "posture=hold_off" ) != std::string::npos );
    CHECK( camp_report.find( "strength=2" ) != std::string::npos );
    CHECK( camp_report.find( "threat=3" ) != std::string::npos );
    CHECK( bandit_live_world::hot_defended_doorstep_blocks_pickup( site, camp_input, decision,
            character_id( 901 ) ) );

    bandit_live_world::local_gate_input distant_watch_input = camp_input;
    distant_watch_input.current_exposure = false;
    distant_watch_input.recent_exposure = false;
    distant_watch_input.standoff_distance = bandit_live_world::minimum_hold_off_standoff_omt();
    const bandit_live_world::local_gate_decision distant_watch_decision =
        bandit_live_world::choose_local_gate_posture( site, distant_watch_input );
    CHECK_FALSE( bandit_live_world::hot_defended_doorstep_blocks_pickup( site, distant_watch_input,
                 distant_watch_decision, character_id( 901 ) ) );

    bandit_live_world::local_gate_input smoked_watcher_input = camp_input;
    smoked_watcher_input.current_exposure = false;
    smoked_watcher_input.recent_exposure = false;
    smoked_watcher_input.smoke_obscured_lead = true;
    smoked_watcher_input.smoke_on_watcher_tile = true;
    const bandit_live_world::local_gate_decision smoked_watcher_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_watcher_input );
    CHECK( smoked_watcher_decision.valid );
    CHECK( smoked_watcher_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_watcher_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_watcher_decision.combat_forward );
    const std::string smoke_report = bandit_live_world::render_local_gate_report( site,
                                      smoked_watcher_input, smoked_watcher_decision );
    CHECK( smoke_report.find( "smoke_obscured=yes" ) != std::string::npos );
    CHECK( smoke_report.find( "smoke_on_watcher=yes" ) != std::string::npos );
    CHECK( smoke_report.find( "camping the smoked tile" ) != std::string::npos );
    CHECK( bandit_live_world::hot_defended_doorstep_blocks_pickup( site, smoked_watcher_input,
            smoked_watcher_decision, character_id( 901 ) ) );

    bandit_live_world::local_gate_input smoked_sightline_contact_input = camp_input;
    smoked_sightline_contact_input.current_exposure = false;
    smoked_sightline_contact_input.recent_exposure = false;
    smoked_sightline_contact_input.local_contact_established = true;
    smoked_sightline_contact_input.smoke_obscured_lead = true;
    smoked_sightline_contact_input.smoke_between_watcher_and_camp = true;
    const bandit_live_world::local_gate_decision smoked_sightline_contact_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_sightline_contact_input );
    CHECK( smoked_sightline_contact_decision.valid );
    CHECK( smoked_sightline_contact_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_sightline_contact_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_sightline_contact_decision.combat_forward );
    const std::string smoked_sightline_contact_report = bandit_live_world::render_local_gate_report(
                site, smoked_sightline_contact_input, smoked_sightline_contact_decision );
    CHECK( smoked_sightline_contact_report.find( "smoke_obscured=yes" ) != std::string::npos );
    CHECK( smoked_sightline_contact_report.find( "smoke_sightline=yes" ) != std::string::npos );
    CHECK( smoked_sightline_contact_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( smoked_sightline_contact_report.find( "backs off/waits" ) != std::string::npos );

    bandit_live_world::local_gate_input probe_input;
    probe_input.local_threat = 1;
    probe_input.local_opportunity = 1;
    decision = bandit_live_world::choose_local_gate_posture( site, probe_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 902 ),
             bandit_live_world::member_state::outbound, "joins local gate proof group" ) );
    site.active_outing.member_ids.push_back( character_id( 902 ) );

    bandit_live_world::local_gate_input shakedown_input;
    shakedown_input.local_threat = 1;
    shakedown_input.local_opportunity = 3;
    shakedown_input.local_contact_established = true;
    decision = bandit_live_world::choose_local_gate_posture( site, shakedown_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::open_shakedown );
    CHECK( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );

    bandit_live_world::local_gate_input rolling_input;
    rolling_input.local_threat = 2;
    rolling_input.local_opportunity = 1;
    rolling_input.rolling_travel_scene = true;
    decision = bandit_live_world::choose_local_gate_posture( site, rolling_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK( decision.combat_forward );
    const std::string rolling_report = bandit_live_world::render_local_gate_report( site, rolling_input,
                                       decision );
    CHECK( rolling_report.find( "posture=attack_now" ) != std::string::npos );
    CHECK( rolling_report.find( "rolling_travel=yes" ) != std::string::npos );
    CHECK( rolling_report.find( "combat_forward=yes" ) != std::string::npos );

    bandit_live_world::local_gate_input hopeless_input;
    hopeless_input.local_threat = 8;
    hopeless_input.local_opportunity = 0;
    decision = bandit_live_world::choose_local_gate_posture( site, hopeless_input );
    CHECK_FALSE( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::abort );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );
}

TEST_CASE( "bandit_live_world_vertical_targets_route_via_reachable_ground_dispatch_target",
           "[bandit][live_world][multi_z][dispatch]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 154 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 155 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 156 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt roof_target( 18, 20, 5 );
    const tripoint_abs_omt ground_target( 18, 20, 0 );
    CHECK( bandit_live_world::reachable_ground_dispatch_target( site, roof_target ) == ground_target );

    const bandit_live_world::dispatch_plan direct_plan =
        bandit_live_world::plan_site_dispatch( site, roof_target, "player@18,20,5" );
    REQUIRE( direct_plan.valid );
    CHECK( direct_plan.target_id == "player@18,20,5" );
    CHECK( direct_plan.target_omt == ground_target );
    CHECK( std::any_of( direct_plan.notes.begin(), direct_plan.notes.end(), []( const std::string &note ) {
        return note.find( "vertical dispatch fallback" ) != std::string::npos &&
               note.find( "18,20,5" ) != std::string::npos &&
               note.find( "18,20,0" ) != std::string::npos;
    } ) );

    bandit_live_world::camp_map_lead elevated_lead;
    elevated_lead.lead_id = "elevated_basecamp@18,20,5";
    elevated_lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    elevated_lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    elevated_lead.target_id = "player@18,20,5";
    elevated_lead.omt = roof_target;
    elevated_lead.radius_omt = 2;
    elevated_lead.bounty = 8;
    elevated_lead.threat = 1;
    elevated_lead.confidence = 3;
    site.intelligence_map.leads.push_back( elevated_lead );

    const bandit_live_world::camp_map_lead *matched_lead =
        bandit_live_world::find_camp_map_dispatch_lead_for_target( site, roof_target, "" );
    REQUIRE( matched_lead != nullptr );
    CHECK( matched_lead->lead_id == elevated_lead.lead_id );

    const bandit_live_world::camp_map_lead *ground_matched_lead =
        bandit_live_world::find_camp_map_dispatch_lead_for_target( site, ground_target,
                "player@18,20,0" );
    REQUIRE( ground_matched_lead != nullptr );
    CHECK( ground_matched_lead->lead_id == elevated_lead.lead_id );

    prepare_hostile_follow_on( site, 2, 1, elevated_lead.target_id, ground_target, 100 );
    matched_lead = bandit_live_world::find_camp_map_dispatch_lead_for_target(
                       site, roof_target, "" );
    REQUIRE( matched_lead != nullptr );
    const bandit_live_world::dispatch_plan camp_map_plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, *matched_lead );
    REQUIRE( camp_map_plan.valid );
    CHECK( camp_map_plan.target_id == "player@18,20,5" );
    CHECK( camp_map_plan.target_omt == ground_target );
    CHECK( std::any_of( camp_map_plan.notes.begin(), camp_map_plan.notes.end(), []( const std::string &note ) {
        return note.find( "vertical dispatch fallback" ) != std::string::npos &&
               note.find( "18,20,5" ) != std::string::npos &&
               note.find( "18,20,0" ) != std::string::npos;
    } ) );

    apply_test_hostile_dispatch( site, camp_map_plan, 102 );
    CHECK( site.active_hostile_operation.reservation.target_id == "player@18,20,5" );
    CHECK( site.active_hostile_operation.reservation.target_omt == ground_target );
}

TEST_CASE( "bandit_live_world_hold_off_goal_keeps_visible_standoff",
           "[bandit][live_world][local_gate]" )
{
    const tripoint_abs_omt player( 140, 41, 0 );
    const tripoint_abs_omt camp_anchor( 140, 51, 0 );
    const tripoint_abs_omt goal = bandit_live_world::choose_hold_off_standoff_goal(
                                      camp_anchor, player, 2 );

    CHECK( bandit_live_world::ordinary_scout_watch_standoff_omt() == 5 );
    CHECK( bandit_live_world::minimum_hold_off_standoff_omt() == 5 );
    CHECK( rl_dist( goal, player ) >= bandit_live_world::minimum_hold_off_standoff_omt() );
    CHECK( goal == tripoint_abs_omt( 140, 46, 0 ) );

    const tripoint_abs_omt diagonal_goal = bandit_live_world::choose_hold_off_standoff_goal(
            tripoint_abs_omt( 150, 51, 0 ), player, 2 );
    CHECK( diagonal_goal == tripoint_abs_omt( 145, 46, 0 ) );

    CHECK( bandit_live_world::choose_hold_off_standoff_goal( player, player, 2 ) == player );
}

TEST_CASE( "bandit_live_world_sight_avoid_uses_only_bounded_local_reposition_candidates",
           "[bandit][live_world][sight_avoid]" )
{
    const tripoint_abs_ms current( 100, 100, 0 );
    const std::vector<bandit_live_world::sight_avoid_candidate> candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0 },
        { tripoint_abs_ms( 99, 100, 0 ), true, false, true, 1 },
        { tripoint_abs_ms( 110, 100, 0 ), true, false, false, 99 },
        { tripoint_abs_ms( 100, 99, 0 ), false, false, false, 20 },
    };

    const bandit_live_world::sight_avoid_decision quiet =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false, candidates );
    CHECK( quiet.valid );
    CHECK_FALSE( quiet.repositions );
    CHECK( quiet.reason == "still stalking" );

    const bandit_live_world::sight_avoid_decision exposed =
        bandit_live_world::choose_sight_avoid_reposition( current, true, false, candidates );
    CHECK( exposed.valid );
    REQUIRE( exposed.repositions );
    CHECK( exposed.destination == tripoint_abs_ms( 99, 100, 0 ) );
    CHECK( exposed.reason == "repositioning because exposed" );
    CHECK( rl_dist( exposed.destination, current ) == 1 );

    const std::vector<bandit_live_world::sight_avoid_candidate> hollow_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0 },
        { tripoint_abs_ms( 100, 101, 0 ), false, false, false, 10 },
    };
    const bandit_live_world::sight_avoid_decision no_good_step =
        bandit_live_world::choose_sight_avoid_reposition( current, true, true, hollow_candidates );
    CHECK( no_good_step.valid );
    CHECK_FALSE( no_good_step.repositions );

    const std::vector<bandit_live_world::sight_avoid_candidate> smoke_blocked_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), false, true, true, 0, true },
        { tripoint_abs_ms( 99, 100, 0 ), false, true, true, 0, true },
    };
    const bandit_live_world::sight_avoid_decision smoke_blocked =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false,
                smoke_blocked_candidates, true );
    CHECK( smoke_blocked.valid );
    CHECK_FALSE( smoke_blocked.repositions );
    CHECK( smoke_blocked.reason == "blocked: smoke-obscured no adjacent passable reposition candidate" );

    const std::vector<bandit_live_world::sight_avoid_candidate> smoked_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0, false },
        { tripoint_abs_ms( 99, 100, 0 ), true, true, true, 0, true },
    };
    const bandit_live_world::sight_avoid_decision smoked_tile =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false, smoked_candidates,
                true );
    CHECK( smoked_tile.valid );
    REQUIRE( smoked_tile.repositions );
    CHECK( smoked_tile.destination == tripoint_abs_ms( 101, 100, 0 ) );
    CHECK( smoked_tile.reason == "repositioning because smoke obscures lead" );

    const std::vector<bandit_live_world::sight_avoid_candidate> smoky_no_clear_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0, true },
        { tripoint_abs_ms( 99, 100, 0 ), true, true, true, 0, true },
    };
    const bandit_live_world::sight_avoid_decision smoky_no_clear =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false,
                smoky_no_clear_candidates, true );
    CHECK( smoky_no_clear.valid );
    REQUIRE( smoky_no_clear.repositions );
    CHECK( rl_dist( smoky_no_clear.destination, current ) == 1 );
    CHECK( smoky_no_clear.reason == "repositioning because smoke obscures lead" );
}

TEST_CASE( "bandit_live_world_scout_sortie_has_finite_return_home_clock",
           "[bandit][live_world][scout_return]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 921 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 922 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "player@18,20,0" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    CHECK( site.active_outing.job_type == "scout" );
    REQUIRE( site.active_outing.member_ids.size() == 2 );

    REQUIRE( bandit_live_world::note_active_sortie_started(
                 site, require_current_simulation_cursor( site ), 100 ) );
    CHECK_FALSE( bandit_live_world::note_active_sortie_started(
                     site, require_current_simulation_cursor( site ), 105 ) );
    CHECK( bandit_live_world::ordinary_scout_sortie_limit_minutes() == 720 );
    CHECK_FALSE( bandit_live_world::scout_sortie_should_return_home( site, 819,
                 bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    CHECK( bandit_live_world::scout_sortie_should_return_home( site, 820,
            bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    REQUIRE( bandit_live_world::note_active_sortie_local_contact(
                 site, require_current_simulation_cursor( site ),
                 site.active_outing.member_ids.front(), 130 ) );
    CHECK( site.active_outing.expected_return_minutes == 970 );
    CHECK( site.active_outing.missing_deadline_minutes == 2410 );
    CHECK_FALSE( bandit_live_world::scout_sortie_should_return_home( site, 849,
                 bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    CHECK( bandit_live_world::scout_sortie_should_return_home( site, 850,
            bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.job_type == "scout" );
    CHECK( loaded_site.active_outing.started_minutes == 100 );
    CHECK( loaded_site.active_outing.local_contact_minutes == 130 );
    CHECK( loaded_site.active_outing.expected_return_minutes == 970 );
    CHECK( loaded_site.active_outing.missing_deadline_minutes == 2410 );

    REQUIRE( bandit_live_world::update_member_state( site, site.active_outing.member_ids.front(),
             bandit_live_world::member_state::local_contact, "watched long enough near player@18,20,0" ) );
    const std::vector<bandit_live_world::active_member_observation> still_watching = {
        { site.active_outing.member_ids.front(),
          bandit_live_world::active_member_observation_state::returning_home,
          "scout sortie limit reached; returning home" },
        { site.active_outing.member_ids.back(),
          bandit_live_world::active_member_observation_state::returning_home,
          "escort returns with scout" }
    };
    CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, still_watching ).has_value() );

    const std::vector<bandit_live_world::active_member_observation> home = {
        { site.active_outing.member_ids.front(), bandit_live_world::active_member_observation_state::home,
          "npc back on home footprint" },
        { site.active_outing.member_ids.back(), bandit_live_world::active_member_observation_state::home,
          "escort back on home footprint" }
    };
    const std::optional<bandit_pursuit_handoff::return_packet> packet =
        bandit_live_world::resolve_active_group_aftermath( site, home );
    REQUIRE( packet.has_value() );
    CHECK( packet->result == bandit_pursuit_handoff::mission_result::withdrawn );
    REQUIRE( bandit_live_world::apply_return_packet( site, *packet ) );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.job_type.empty() );
    CHECK( site.active_outing.started_minutes == -1 );
    CHECK( site.find_member( character_id( 921 ) )->state == bandit_live_world::member_state::at_home );
    CHECK( site.find_member( character_id( 922 ) )->state == bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_makes_cannibal_camp_attack_instead_of_extort", "[bandit][live_world][cannibal][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 911 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 912 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 913 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 914 ),
             tripoint_abs_ms( 1705, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    prepare_hostile_follow_on( site, 2, 1, "player@72,80,0",
                               tripoint_abs_omt( 72, 80, 0 ), 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    apply_test_hostile_dispatch( site, plan, 102 );

    bandit_live_world::local_gate_input favorable_input;
    favorable_input.local_threat = 1;
    favorable_input.local_opportunity = 3;
    favorable_input.local_contact_established = true;
    favorable_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision favorable_decision =
        bandit_live_world::choose_local_gate_posture( site, favorable_input );
    CHECK( favorable_decision.valid );
    CHECK( favorable_decision.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK_FALSE( favorable_decision.opens_shakedown_surface );
    CHECK( favorable_decision.combat_forward );
    const std::string favorable_report =
        bandit_live_world::render_local_gate_report( site, favorable_input, favorable_decision );
    CHECK( favorable_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( favorable_report.find( "posture=attack_now" ) != std::string::npos );
    CHECK( favorable_report.find( "pack_size=2" ) != std::string::npos );
    CHECK( favorable_report.find( "darkness_or_concealment=no" ) != std::string::npos );
    CHECK( favorable_report.find( "current_exposure=no" ) != std::string::npos );
    CHECK( favorable_report.find( "sight_exposure=none" ) != std::string::npos );
    CHECK( favorable_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( favorable_report.find( "attack-to-kill" ) != std::string::npos );

    bandit_live_world::shakedown_goods_pool pool;
    pool.player_carried_value = 100;
    pool.companion_carried_value = 50;
    pool.reachable_basecamp_value = 850;
    pool.basecamp_or_camp_scene = true;
    const bandit_live_world::shakedown_surface blocked_surface =
        bandit_live_world::build_shakedown_surface( site, favorable_input, favorable_decision, pool );
    CHECK_FALSE( blocked_surface.valid );
    REQUIRE_FALSE( blocked_surface.notes.empty() );
    CHECK( blocked_surface.notes.front().find( "local gate did not open" ) != std::string::npos );

    bandit_live_world::local_gate_input cautious_input = favorable_input;
    cautious_input.local_contact_established = false;
    cautious_input.local_threat = 2;
    cautious_input.local_opportunity = 1;
    cautious_input.basecamp_or_camp_scene = false;
    const bandit_live_world::local_gate_decision cautious_decision =
        bandit_live_world::choose_local_gate_posture( site, cautious_input );
    CHECK( cautious_decision.valid );
    CHECK( cautious_decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( cautious_decision.opens_shakedown_surface );
    CHECK_FALSE( cautious_decision.combat_forward );

    bandit_live_world::local_gate_input exposed_input = favorable_input;
    exposed_input.local_threat = 3;
    exposed_input.local_opportunity = 2;
    exposed_input.darkness_or_concealment = true;
    exposed_input.current_exposure = true;
    const bandit_live_world::local_gate_decision exposed_decision =
        bandit_live_world::choose_local_gate_posture( site, exposed_input );
    CHECK( exposed_decision.valid );
    CHECK( exposed_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( exposed_decision.opens_shakedown_surface );
    CHECK_FALSE( exposed_decision.combat_forward );
    const std::string exposed_report =
        bandit_live_world::render_local_gate_report( site, exposed_input, exposed_decision );
    CHECK( exposed_report.find( "current_exposure=yes" ) != std::string::npos );
    CHECK( exposed_report.find( "sight_exposure=current" ) != std::string::npos );
    CHECK( exposed_report.find( "visible beeline" ) != std::string::npos );

    bandit_live_world::local_gate_input smoked_cannibal_input = favorable_input;
    smoked_cannibal_input.local_contact_established = false;
    smoked_cannibal_input.smoke_obscured_lead = true;
    smoked_cannibal_input.smoke_on_watcher_tile = true;
    const bandit_live_world::local_gate_decision smoked_cannibal_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_cannibal_input );
    CHECK( smoked_cannibal_decision.valid );
    CHECK( smoked_cannibal_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_cannibal_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_cannibal_decision.combat_forward );
    const std::string smoked_cannibal_report =
        bandit_live_world::render_local_gate_report( site, smoked_cannibal_input,
                smoked_cannibal_decision );
    CHECK( smoked_cannibal_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "smoke_obscured=yes" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "smoke_on_watcher=yes" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "camping the smoked tile" ) != std::string::npos );

    bandit_live_world::local_gate_input smoked_contact_cannibal_input = favorable_input;
    smoked_contact_cannibal_input.smoke_obscured_lead = true;
    smoked_contact_cannibal_input.smoke_between_watcher_and_camp = true;
    const bandit_live_world::local_gate_decision smoked_contact_cannibal_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_contact_cannibal_input );
    CHECK( smoked_contact_cannibal_decision.valid );
    CHECK( smoked_contact_cannibal_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_contact_cannibal_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_contact_cannibal_decision.combat_forward );
    const std::string smoked_contact_cannibal_report =
        bandit_live_world::render_local_gate_report( site, smoked_contact_cannibal_input,
                smoked_contact_cannibal_decision );
    CHECK( smoked_contact_cannibal_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( smoked_contact_cannibal_report.find( "smoke_sightline=yes" ) != std::string::npos );
    CHECK( smoked_contact_cannibal_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( smoked_contact_cannibal_report.find( "camping the smoked tile" ) != std::string::npos );

    const tripoint_abs_ms current_tile( 100, 100, 0 );
    const std::vector<bandit_live_world::sight_avoid_candidate> exposed_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0 },
        { tripoint_abs_ms( 99, 100, 0 ), true, false, true, 2 },
    };
    const bandit_live_world::sight_avoid_decision reposition =
        bandit_live_world::choose_sight_avoid_reposition( current_tile, true, false,
                exposed_candidates );
    REQUIRE( reposition.repositions );
    CHECK( rl_dist( reposition.destination, current_tile ) == 1 );

    bandit_live_world::local_gate_input hopeless_input;
    hopeless_input.local_threat = 8;
    hopeless_input.local_opportunity = 0;
    const bandit_live_world::local_gate_decision hopeless_decision =
        bandit_live_world::choose_local_gate_posture( site, hopeless_input );
    CHECK_FALSE( hopeless_decision.valid );
    CHECK( hopeless_decision.posture == bandit_live_world::local_gate_posture::abort );
    CHECK_FALSE( hopeless_decision.opens_shakedown_surface );
}

TEST_CASE( "bandit_live_world_builds_a_bounded_pay_or_fight_shakedown_surface", "[bandit][live_world][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 951 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 952 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 953 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player@18,20,0" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_live_world::local_gate_input single_bandit_gate_input;
    single_bandit_gate_input.local_threat = 1;
    single_bandit_gate_input.local_opportunity = 3;
    single_bandit_gate_input.local_contact_established = true;
    single_bandit_gate_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision single_bandit_gate_decision =
        bandit_live_world::choose_local_gate_posture( site, single_bandit_gate_input );
    CHECK( single_bandit_gate_decision.posture ==
           bandit_live_world::local_gate_posture::open_shakedown );
    CHECK( single_bandit_gate_decision.opens_shakedown_surface );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 952 ),
             bandit_live_world::member_state::outbound, "joins shakedown proof group" ) );
    site.active_outing.member_ids.push_back( character_id( 952 ) );

    bandit_live_world::local_gate_input gate_input;
    gate_input.local_threat = 1;
    gate_input.local_opportunity = 3;
    gate_input.local_contact_established = true;
    gate_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision gate_decision =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    REQUIRE( gate_decision.valid );
    REQUIRE( gate_decision.posture == bandit_live_world::local_gate_posture::open_shakedown );
    REQUIRE( gate_decision.opens_shakedown_surface );

    bandit_live_world::shakedown_goods_pool basecamp_pool;
    basecamp_pool.player_carried_value = 100;
    basecamp_pool.companion_carried_value = 50;
    basecamp_pool.vehicle_carried_value = 10000;
    basecamp_pool.reachable_basecamp_value = 850;
    basecamp_pool.basecamp_or_camp_scene = true;
    const bandit_live_world::shakedown_surface basecamp_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision,
                basecamp_pool );
    CHECK( basecamp_surface.valid );
    CHECK( basecamp_surface.pay_available );
    CHECK( basecamp_surface.fight_available );
    CHECK( basecamp_surface.includes_basecamp_inventory );
    CHECK_FALSE( basecamp_surface.includes_vehicle_inventory );
    CHECK( basecamp_surface.reachable_goods_value == 1000 );
    CHECK( basecamp_surface.demanded_value == 350 );
    CHECK( basecamp_surface.opening_id == "basecamp_pressure" );
    const std::string basecamp_report =
        bandit_live_world::render_shakedown_surface_report( site, basecamp_surface );
    CHECK( basecamp_report.find( "pay_option=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "fight_option=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "visible_responses=pay/fight" ) != std::string::npos );
    CHECK( basecamp_report.find( "payment_surface=npc_trade_ui" ) != std::string::npos );
    CHECK( basecamp_report.find( "pay/fight/refuse" ) == std::string::npos );
    CHECK( basecamp_report.find( "basecamp_inventory=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "vehicle_inventory=no" ) != std::string::npos );
    CHECK( basecamp_report.find( "opening=basecamp_pressure" ) != std::string::npos );
    CHECK( basecamp_report.find( "demanded_toll=350" ) != std::string::npos );

    bandit_live_world::local_gate_input offbase_gate_input = gate_input;
    offbase_gate_input.basecamp_or_camp_scene = false;
    bandit_live_world::shakedown_goods_pool offbase_pool;
    offbase_pool.player_carried_value = 200;
    offbase_pool.companion_carried_value = 100;
    offbase_pool.vehicle_carried_value = 600;
    offbase_pool.reachable_basecamp_value = 5000;
    const bandit_live_world::shakedown_surface offbase_surface =
        bandit_live_world::build_shakedown_surface( site, offbase_gate_input, gate_decision,
                offbase_pool );
    CHECK( offbase_surface.valid );
    CHECK_FALSE( offbase_surface.includes_basecamp_inventory );
    CHECK( offbase_surface.includes_vehicle_inventory );
    CHECK( offbase_surface.reachable_goods_value == 900 );
    CHECK( offbase_surface.demanded_value == 315 );
    CHECK( offbase_surface.opening_id == "weakness_read" );

    bandit_live_world::local_gate_input cover_gate_input = offbase_gate_input;
    cover_gate_input.standoff_distance = 2;
    const bandit_live_world::shakedown_surface cover_surface =
        bandit_live_world::build_shakedown_surface( site, cover_gate_input, gate_decision,
                offbase_pool );
    CHECK( cover_surface.valid );
    CHECK( cover_surface.opening_id == "warning_from_cover" );

    bandit_live_world::local_gate_input roadblock_gate_input = offbase_gate_input;
    roadblock_gate_input.local_threat = 3;
    const bandit_live_world::local_gate_decision roadblock_gate_decision =
        bandit_live_world::choose_local_gate_posture( site, roadblock_gate_input );
    REQUIRE( roadblock_gate_decision.opens_shakedown_surface );
    const bandit_live_world::shakedown_surface roadblock_surface =
        bandit_live_world::build_shakedown_surface( site, roadblock_gate_input,
                roadblock_gate_decision, offbase_pool );
    CHECK( roadblock_surface.valid );
    CHECK( roadblock_surface.opening_id == "roadblock_toll" );

    bandit_live_world::local_gate_input rolling_gate_input = gate_input;
    rolling_gate_input.rolling_travel_scene = true;
    const bandit_live_world::shakedown_surface rolling_surface =
        bandit_live_world::build_shakedown_surface( site, rolling_gate_input, gate_decision,
                offbase_pool );
    CHECK_FALSE( rolling_surface.valid );
    REQUIRE_FALSE( rolling_surface.notes.empty() );
    CHECK( rolling_surface.notes.front().find( "direct-ambush" ) != std::string::npos );
}


TEST_CASE( "bandit_live_world_records_shakedown_aftermath_for_renegotiation_pressure", "[bandit][live_world][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 971 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 972 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 973 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player@18,20,0" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    site.remembered_threat_estimate = 4;

    bandit_live_world::local_gate_input gate_input;
    gate_input.local_threat = 1;
    gate_input.local_opportunity = 3;
    gate_input.local_contact_established = true;
    gate_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision gate_decision =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    REQUIRE( gate_decision.opens_shakedown_surface );

    bandit_live_world::shakedown_goods_pool pool;
    pool.player_carried_value = 100;
    pool.companion_carried_value = 50;
    pool.reachable_basecamp_value = 850;
    pool.basecamp_or_camp_scene = true;

    const bandit_live_world::shakedown_surface first_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    REQUIRE( first_surface.valid );
    REQUIRE( first_surface.demanded_value == 350 );

    bandit_live_world::shakedown_outcome fight_opened;
    fight_opened.fought = true;
    fight_opened.basecamp_or_camp_scene = true;
    fight_opened.demanded_value = first_surface.demanded_value;
    fight_opened.reachable_goods_value = first_surface.reachable_goods_value;
    REQUIRE( bandit_live_world::apply_shakedown_outcome( site, fight_opened ).valid );
    bandit_live_world::begin_shakedown_basecamp_defender_observation( site, 2 );
    CHECK( site.shakedown_basecamp_defender_observation_pending );
    CHECK_FALSE( bandit_live_world::apply_shakedown_basecamp_defender_observation( site, 2 ).valid );
    CHECK( site.shakedown_basecamp_defender_observation_pending );
    const bandit_live_world::shakedown_aftermath_effect harsh_effect =
        bandit_live_world::apply_shakedown_basecamp_defender_observation( site, 1 );
    CHECK( harsh_effect.valid );
    CHECK( harsh_effect.stronger_reopen );
    CHECK_FALSE( harsh_effect.cools_later_pressure );
    CHECK( harsh_effect.demand_modifier_percent == 140 );
    CHECK( site.last_shakedown_outcome == "fight_defender_loss" );
    CHECK( site.shakedown_defender_losses == 1 );
    CHECK_FALSE( site.shakedown_basecamp_defender_observation_pending );
    CHECK( site.shakedown_reopen_available );
    CHECK_FALSE( site.shakedown_reopen_used );
    CHECK( site.remembered_threat_estimate == 3 );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().shakedown_reopen_available );
    CHECK( loaded.sites.front().shakedown_defender_losses == 1 );
    CHECK_FALSE( loaded.sites.front().shakedown_basecamp_defender_observation_pending );
    CHECK( loaded.sites.front().last_shakedown_outcome == "fight_defender_loss" );

    const bandit_live_world::shakedown_surface reopened_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    CHECK( reopened_surface.valid );
    CHECK( reopened_surface.pay_available );
    CHECK( reopened_surface.fight_available );
    CHECK( reopened_surface.opening_id == "reopened_demand" );
    CHECK( reopened_surface.demanded_value == 490 );
    const std::string reopened_report =
        bandit_live_world::render_shakedown_surface_report( site, reopened_surface );
    CHECK( reopened_report.find( "renegotiation reopen" ) != std::string::npos );
    CHECK( reopened_report.find( "demanded_toll=490" ) != std::string::npos );

    REQUIRE( bandit_live_world::mark_shakedown_reopen_used( site ) );
    const bandit_live_world::shakedown_surface spent_reopen_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    CHECK( spent_reopen_surface.demanded_value == 350 );

    bandit_live_world::shakedown_outcome bandit_loss;
    bandit_loss.fought = true;
    bandit_loss.demanded_value = first_surface.demanded_value;
    bandit_loss.reachable_goods_value = first_surface.reachable_goods_value;
    bandit_loss.bandit_losses = 1;
    bandit_loss.extraction_failed = true;
    const bandit_live_world::shakedown_aftermath_effect cool_effect =
        bandit_live_world::apply_shakedown_outcome( site, bandit_loss );
    CHECK( cool_effect.valid );
    CHECK_FALSE( cool_effect.stronger_reopen );
    CHECK( cool_effect.cools_later_pressure );
    CHECK( cool_effect.demand_modifier_percent == 75 );
    CHECK( site.shakedown_bandit_losses == 1 );
    CHECK( site.shakedown_caution == 1 );
    CHECK( site.remembered_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::collapsed );

    const bandit_live_world::shakedown_surface cooled_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    CHECK( cooled_surface.demanded_value == 263 );
    const std::string cooled_report =
        bandit_live_world::render_shakedown_surface_report( site, cooled_surface );
    CHECK( cooled_report.find( "aftermath caution" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_cools_shakedown_pressure_when_the_active_bandit_is_lost", "[bandit][live_world][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 981 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 982 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_live_world::shakedown_outcome fight_opened;
    fight_opened.fought = true;
    fight_opened.basecamp_or_camp_scene = true;
    fight_opened.demanded_value = 350;
    fight_opened.reachable_goods_value = 1000;
    REQUIRE( bandit_live_world::apply_shakedown_outcome( site, fight_opened ).valid );
    CHECK( site.last_shakedown_outcome == "fight_unresolved" );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 1;
    outcome.anchored_identity_updates = { { "981", "missing" } };
    outcome.result = bandit_pursuit_handoff::mission_result::broken;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    outcome.posture = bandit_pursuit_handoff::return_posture::escape_safe;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );
    CHECK( site.find_member( character_id( 981 ) )->state == bandit_live_world::member_state::missing );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.last_shakedown_outcome == "fight_bandit_loss" );
    CHECK( site.shakedown_bandit_losses == 1 );
    CHECK( site.shakedown_caution == 1 );
    CHECK( site.remembered_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::collapsed );
}

TEST_CASE( "bandit_live_world_applies_a_return_packet_onto_the_active_owned_outing", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 801 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 802 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    REQUIRE( site.active_outing.member_ids ==
             std::vector<character_id>( { character_id( 801 ), character_id( 802 ) } ) );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 1;
    outcome.anchored_identity_updates = { { "801", "dead" } };
    outcome.result = bandit_pursuit_handoff::mission_result::repelled;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    outcome.posture = bandit_pursuit_handoff::return_posture::broken_flee;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );
    CHECK( site.find_member( character_id( 801 ) )->state == bandit_live_world::member_state::dead );
    CHECK( site.find_member( character_id( 801 ) )->last_writeback_summary ==
           "return repelled from player_basecamp_nearby (dead)" );
    CHECK( site.find_member( character_id( 802 ) )->state == bandit_live_world::member_state::at_home );
    CHECK( site.living_total == 1 );
    CHECK( site.dispatchable_member_capacity() == 0 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.target_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
}

TEST_CASE( "bandit_live_world_rejects_malformed_return_packets_atomically",
           "[bandit][live_world][handoff]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 811 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 812 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 1;
    outcome.anchored_identity_updates = { { "811", "dead" } };
    outcome.result = bandit_pursuit_handoff::mission_result::repelled;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    const bandit_pursuit_handoff::return_packet valid_packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    SECTION( "survivor count mismatch" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.survivors_remaining = 0;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "duplicate member resolution" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.anchored_identity_updates.push_back( { "811", "missing" } );
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "unknown member resolution" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.anchored_identity_updates = { { "999999", "dead" } };
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "unknown resolution state" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.anchored_identity_updates = { { "811", "teleported" } };
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "stale generation" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        ++malformed.activity_generation;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "stale handoff epoch" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        ++malformed.handoff_epoch;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "wrong application key" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.return_application_key += ":wrong";
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "wrong report application key" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.report_application_key += ":wrong";
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "wrong cargo application key" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.cargo_application_key += ":wrong";
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "missing member return receipt" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.member_return_receipts.clear();
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "swapped member return receipt" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        REQUIRE( malformed.member_return_receipts.size() == 2 );
        malformed.member_return_receipts.front().member_id = "812";
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "wrong job type" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.job_type = bandit_dry_run::job_template::toll;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }
}

TEST_CASE( "bandit_live_world_report_watermark_does_not_close_a_legacy_hostile_operation",
           "[bandit][live_world][handoff][replay]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45700 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45700 ),
             bandit_live_world::member_state::outbound, "test stale hostile outing" ) );
    set_test_active_outing( site, site.site_id + "#toll:stale",
                            bandit_live_world::outing_kind::hostile_operation );
    site.active_outing.member_ids = { character_id( 45700 ) };
    site.active_outing.leader_id = character_id( 45700 );
    site.active_outing.job_type = "toll";
    site.applied_report_generation = site.active_outing.generation;

    const bandit_live_world::world_state loaded = round_trip_legacy_site_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK_FALSE( loaded.sites.front().active_outing.is_active() );
    CHECK( loaded.sites.front().active_outing.member_ids.empty() );
    REQUIRE( loaded.sites.front().active_hostile_operation.is_active() );
    CHECK( loaded.sites.front().active_hostile_operation.legacy_unpinned );
    CHECK( loaded.sites.front().active_hostile_operation.phase ==
           bandit_live_world::hostile_operation_phase::returning_home );
    CHECK( loaded.sites.front().active_hostile_operation.reservation.generation ==
           loaded.sites.front().applied_report_generation );
    REQUIRE( loaded.sites.front().find_member( character_id( 45700 ) ) != nullptr );
    CHECK( loaded.sites.front().find_member( character_id( 45700 ) )->state ==
           bandit_live_world::member_state::outbound );
}

TEST_CASE( "bandit_live_world_rejects_a_completed_return_after_reload_and_redispatch",
           "[bandit][live_world][handoff][replay]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit",
                 character_id( 820 + index ), tripoint_abs_ms( 240 + index, 480, 0 ),
                 std::string( "bandit_camp" ), std::nullopt, special_lookup ) );
    }

    bandit_live_world::site_record &first_site = world.sites.front();
    const bandit_live_world::dispatch_plan first_plan =
        bandit_live_world::plan_site_dispatch( first_site, tripoint_abs_omt( 18, 20, 0 ),
                "player_basecamp_nearby" );
    REQUIRE( first_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( first_site, first_plan ) );

    bandit_pursuit_handoff::local_outcome first_outcome;
    first_outcome.survivors_remaining = 2;
    first_outcome.result = bandit_pursuit_handoff::mission_result::scouted;
    first_outcome.resolution = bandit_pursuit_handoff::lead_resolution::still_valid;
    const bandit_pursuit_handoff::return_packet first_packet =
        bandit_pursuit_handoff::build_return_packet( first_plan.entry, first_outcome );
    REQUIRE( bandit_live_world::apply_return_packet( first_site, first_packet ) );
    REQUIRE( first_site.applied_return_generation == 1 );
    REQUIRE( first_site.camp_decision.state ==
             bandit_live_world::camp_decision_state::report_awaiting_assessment );
    const int first_report_revision = first_site.camp_decision.source_report_revision;
    const int first_report_generation = first_site.camp_decision.source_report_generation;
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 first_site,
                 bandit_live_world::camp_decision_state::report_awaiting_assessment,
                 bandit_live_world::camp_decision_state::cooldown,
                 first_report_revision, first_report_generation, 0, 0,
                 "first report assessed without follow-on" ) ==
             bandit_live_world::camp_decision_transition_result::applied );
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 first_site, bandit_live_world::camp_decision_state::cooldown,
                 bandit_live_world::camp_decision_state::idle,
                 first_report_revision, first_report_generation, 0, -1,
                 "zero-duration test cooldown elapsed" ) ==
             bandit_live_world::camp_decision_transition_result::applied );

    const bandit_live_world::dispatch_plan second_plan =
        bandit_live_world::plan_site_dispatch( first_site, tripoint_abs_omt( 19, 20, 0 ),
                "player_basecamp_moved" );
    REQUIRE( second_plan.valid );
    REQUIRE( second_plan.entry.activity_generation == 2 );
    REQUIRE( bandit_live_world::apply_dispatch_plan( first_site, second_plan ) );

    bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    REQUIRE( loaded_site.active_outing.generation == 2 );
    REQUIRE( loaded_site.applied_return_generation == 1 );
    const std::string before_replay = serialize_world( loaded );
    CHECK_FALSE( bandit_live_world::apply_return_packet( loaded_site, first_packet ) );
    CHECK( serialize_world( loaded ) == before_replay );
}

TEST_CASE( "bandit_live_world_scout_persistence_stays_within_a_coarse_save_budget",
           "[bandit][live_world][save_size]" )
{
    bandit_live_world::world_state empty_world;
    const std::size_t empty_bytes = serialize_world( empty_world ).size();

    bandit_live_world::world_state normal_world;
    for( int index = 0; index < 4; ++index ) {
        add_bandit_camp_member( normal_world, index, 11800 );
    }
    bandit_live_world::site_record &normal_site = normal_world.sites.front();
    const bandit_live_world::dispatch_plan normal_plan =
        bandit_live_world::plan_site_dispatch( normal_site, tripoint_abs_omt( 18, 20, 0 ),
                "player@18,20,0" );
    REQUIRE( normal_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( normal_site, normal_plan ) );
    normal_site.active_outing.shared_route = {
        tripoint_abs_omt( 11, 20, 0 ), tripoint_abs_omt( 13, 20, 0 ),
        tripoint_abs_omt( 15, 20, 0 ), tripoint_abs_omt( 18, 20, 0 )
    };
    normal_site.active_outing.observations = {
        { "smoke@18,20,0", "thin smoke over the target", 45, 120, false,
          bandit_live_world::sortie_observation_kind::routine, "" },
        { "defenders@18,20,0", "two visible defenders", 70, 125, true,
          bandit_live_world::sortie_observation_kind::routine, "" }
    };
    normal_site.active_outing.cargo = { 2, 60 };
    normal_site.active_outing.started_minutes = 100;
    normal_site.active_outing.last_advanced_minutes = 100;
    normal_site.active_outing.expected_return_minutes = 940;
    normal_site.active_outing.missing_deadline_minutes = 2380;
    const std::size_t normal_bytes = serialize_world( normal_world ).size();

    bandit_live_world::world_state saturated_world = normal_world;
    bandit_live_world::site_record &saturated_site = saturated_world.sites.front();
    saturated_site.active_outing.generation = 2;
    saturated_site.next_outing_generation = 3;
    saturated_site.active_outing.return_application_key =
        saturated_site.active_outing.activity_id + ":return:2";
    saturated_site.active_outing.report_application_key =
        saturated_site.active_outing.activity_id + ":report:2";
    saturated_site.active_outing.cargo_application_key =
        saturated_site.active_outing.activity_id + ":cargo:2";
    saturated_site.active_outing.shared_route.clear();
    for( int index = 0; index < 256; ++index ) {
        saturated_site.active_outing.shared_route.emplace_back( 10 + index, 20 + index, 0 );
    }
    saturated_site.active_outing.observations.clear();
    saturated_site.current_scout_report.revision = 1;
    saturated_site.current_scout_report.action_policy =
        bandit_live_world::camp_report_policy::bandit_shakedown;
    saturated_site.current_scout_report.source_activity_id = saturated_site.site_id + "#scout:1";
    saturated_site.current_scout_report.source_generation = 1;
    saturated_site.current_scout_report.source_job_type = "scout";
    saturated_site.applied_report_generation = 1;
    saturated_site.current_scout_report.target_id = saturated_site.active_outing.target_id;
    saturated_site.current_scout_report.target_omt = saturated_site.active_outing.target_omt;
    saturated_site.current_scout_report.application_key =
        saturated_site.current_scout_report.source_activity_id + ":report:1";
    for( int index = 0; index < 16; ++index ) {
        const bandit_live_world::sortie_observation observation = {
            "bounded-fact-" + std::to_string( index ), std::string( 512, 'x' ),
            100, 1000 + index, index % 2 == 0,
            bandit_live_world::sortie_observation_kind::routine, ""
        };
        saturated_site.active_outing.observations.push_back( observation );
        saturated_site.current_scout_report.observations.push_back( observation );
    }
    saturated_site.active_outing.cargo = { 999999, 999999 };
    saturated_site.returned_cargo_stock = { 999999, 999999 };
    for( int index = 0; index < 64; ++index ) {
        saturated_site.intelligence_map.leads.push_back( make_retention_test_lead( index ) );
    }
    const std::size_t saturated_bytes = serialize_world( saturated_world ).size();

    CAPTURE( empty_bytes, normal_bytes, saturated_bytes );
    CHECK( empty_bytes < normal_bytes );
    CHECK( normal_bytes < saturated_bytes );
    CHECK( saturated_bytes < 64 * 1024 );
    CHECK( serialize_world( round_trip_world( saturated_world ) ).size() == saturated_bytes );
}

TEST_CASE( "bandit_live_world_empty_site_retirement_requires_both_home_and_active_sides_empty",
           "[bandit][live_world][retirement]" )
{
    SECTION( "home members remain without dispatch keeps the site active" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10001 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 0 );
        REQUIRE( world.sites.size() == 1 );
        CHECK_FALSE( world.sites.front().retired_empty_site );
        CHECK( reports.empty() );
        CHECK( world.sites.front().count_members_in_state( bandit_live_world::member_state::at_home ) == 1 );
        CHECK( world.sites.front().count_home_side_signals() > 0 );
    }

    SECTION( "active dispatch without at-home members keeps the site active" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10002 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        bandit_live_world::site_record &site = world.sites.front();
        set_test_active_outing( site, site.site_id + "#dispatch" );
        site.active_outing.target_id = "player_basecamp_nearby";
        site.active_outing.member_ids = { character_id( 10002 ) };
        REQUIRE( bandit_live_world::update_member_state( site, character_id( 10002 ),
                 bandit_live_world::member_state::outbound, "test outbound dispatch" ) );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 0 );
        CHECK_FALSE( site.retired_empty_site );
        CHECK( reports.empty() );
        CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 0 );
        CHECK( site.has_active_outside_pressure() );
    }

    SECTION( "unresolved returning-home active aftermath keeps the site active until resolved" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10003 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        bandit_live_world::site_record &site = world.sites.front();
        set_test_active_outing( site, site.site_id + "#dispatch" );
        site.active_outing.target_id = "player_basecamp_nearby";
        site.active_outing.member_ids = { character_id( 10003 ) };
        REQUIRE( bandit_live_world::update_member_state( site, character_id( 10003 ),
                 bandit_live_world::member_state::local_contact, "test unresolved contact" ) );

        const std::vector<bandit_live_world::active_member_observation> returning_home = {
            { character_id( 10003 ), bandit_live_world::active_member_observation_state::returning_home,
              "still returning home" }
        };
        CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, returning_home ).has_value() );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 0 );
        CHECK_FALSE( site.retired_empty_site );
        CHECK( reports.empty() );
        CHECK( site.has_active_outside_pressure() );
    }

    SECTION( "no live home side and no active outside pressure retires the site from AI surfaces" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10004 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        bandit_live_world::site_record &site = world.sites.front();
        REQUIRE( bandit_live_world::update_member_state( site, character_id( 10004 ),
                 bandit_live_world::member_state::dead, "test camp cleared" ) );
        REQUIRE( site.count_home_side_signals() == 0 );
        REQUIRE_FALSE( site.has_active_outside_pressure() );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 1 );
        CHECK( site.retired_empty_site );
        REQUIRE( reports.size() == 1 );
        CHECK( reports.front().find( "retired_empty_site" ) != std::string::npos );
        CHECK( reports.front().find( "home_side_signals=0" ) != std::string::npos );
        CHECK( reports.front().find( "active_outside=no" ) != std::string::npos );

        const bandit_live_world::dispatch_plan retired_plan =
            bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                    "player_basecamp_nearby" );
        CHECK_FALSE( retired_plan.valid );
        REQUIRE_FALSE( retired_plan.notes.empty() );
        CHECK( retired_plan.notes.front().find( "retired_empty_site" ) != std::string::npos );

        bandit_live_world::live_signal_mark smoke_mark;
        smoke_mark.mark_id = "live_smoke@18,20,0";
        smoke_mark.kind = "smoke";
        smoke_mark.source_omt = tripoint_abs_omt( 18, 20, 0 );
        smoke_mark.range_cap_omt = 15;
        CHECK_FALSE( bandit_live_world::record_live_signal_mark( site, smoke_mark ) );
    }
}

TEST_CASE( "bandit_live_world_resolves_bounded_live_aftermath_observations", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 901 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 902 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    const std::vector<bandit_live_world::active_member_observation> in_contact = {
        { character_id( 901 ), bandit_live_world::active_member_observation_state::local_contact,
          "still in local contact" }
    };
    CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, in_contact ).has_value() );

    SECTION( "dead member resolves to a broken return packet" ) {
        const std::vector<bandit_live_world::active_member_observation> wiped = {
            { character_id( 901 ), bandit_live_world::active_member_observation_state::dead,
              "killed near player target" },
            { character_id( 902 ), bandit_live_world::active_member_observation_state::dead,
              "escort killed near player target" }
        };
        const std::optional<bandit_pursuit_handoff::return_packet> wiped_packet =
            bandit_live_world::resolve_active_group_aftermath( site, wiped );
        REQUIRE( wiped_packet.has_value() );
        CHECK( wiped_packet->valid );
        CHECK( wiped_packet->group_id == site.active_outing.activity_id );
        CHECK( wiped_packet->source_camp_id == site.site_id );
        CHECK( wiped_packet->survivors_remaining == 0 );
        CHECK( wiped_packet->result == bandit_pursuit_handoff::mission_result::broken );
        REQUIRE( wiped_packet->anchored_identity_updates.size() == 2 );
        CHECK( wiped_packet->anchored_identity_updates.front().id == "901" );
        CHECK( wiped_packet->anchored_identity_updates.front().status == "dead" );
        REQUIRE( bandit_live_world::apply_return_packet( site, *wiped_packet ) );
        CHECK( site.find_member( character_id( 901 ) )->state == bandit_live_world::member_state::dead );
        CHECK( site.find_member( character_id( 902 ) )->state == bandit_live_world::member_state::dead );
        CHECK( site.living_total == 0 );
    }

    SECTION( "missing member resolves to a broken return packet" ) {
        const std::vector<bandit_live_world::active_member_observation> lost = {
            { character_id( 901 ), bandit_live_world::active_member_observation_state::missing,
              "vanished during live contact" },
            { character_id( 902 ), bandit_live_world::active_member_observation_state::missing,
              "escort vanished during live contact" }
        };
        const std::optional<bandit_pursuit_handoff::return_packet> lost_packet =
            bandit_live_world::resolve_active_group_aftermath( site, lost );
        REQUIRE( lost_packet.has_value() );
        CHECK( lost_packet->valid );
        CHECK( lost_packet->group_id == site.active_outing.activity_id );
        CHECK( lost_packet->source_camp_id == site.site_id );
        CHECK( lost_packet->survivors_remaining == 0 );
        CHECK( lost_packet->result == bandit_pursuit_handoff::mission_result::broken );
        REQUIRE( lost_packet->anchored_identity_updates.size() == 2 );
        CHECK( lost_packet->anchored_identity_updates.front().id == "901" );
        CHECK( lost_packet->anchored_identity_updates.front().status == "missing" );
        REQUIRE( bandit_live_world::apply_return_packet( site, *lost_packet ) );
        CHECK( site.find_member( character_id( 901 ) )->state == bandit_live_world::member_state::missing );
        CHECK( site.find_member( character_id( 902 ) )->state == bandit_live_world::member_state::missing );
        CHECK( site.living_total == 0 );
    }
}

TEST_CASE( "bandit_live_world_plans_live_dispatch_from_remembered_camp_map_lead",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 7; ++i ) {
        add_bandit_camp_member( world, i, 12400 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "remembered_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.revision = 3;
    lead.radius_omt = 2;
    lead.bounty = 7;
    lead.threat = 1;
    lead.confidence = 3;
    site.intelligence_map.leads.push_back( lead );

    const bandit_live_world::camp_map_lead *matched_lead =
        bandit_live_world::find_camp_map_dispatch_lead_for_target( site,
                tripoint_abs_omt( 19, 20, 0 ), "" );
    REQUIRE( matched_lead != nullptr );
    CHECK( matched_lead->lead_id == lead.lead_id );

    prepare_hostile_follow_on( site, 2, 1, lead.target_id, lead.omt, 100,
                               lead.lead_id );
    matched_lead = bandit_live_world::find_camp_map_dispatch_lead_for_target(
                       site, tripoint_abs_omt( 19, 20, 0 ), "" );
    REQUIRE( matched_lead != nullptr );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, *matched_lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::toll );
    CHECK( plan.member_ids.size() == 3 );
    CHECK( plan.target_omt == lead.omt );
    REQUIRE_FALSE( plan.notes.empty() );
    CHECK( plan.notes.back().find( "profile camp_style" ) != std::string::npos );

    apply_test_hostile_dispatch( site, plan, 102 );
    CHECK( site.active_hostile_operation.reservation.job_type == "toll" );
    CHECK( site.active_hostile_operation.reservation.target_id == lead.target_id );
    CHECK( site.active_hostile_operation.reservation.target_omt == lead.omt );
    CHECK( site.remembered_bounty_estimate == lead.bounty );
    CHECK( site.active_hostile_operation.reservation.member_ids.size() == 3 );
}

TEST_CASE( "bandit_live_world_remembered_camp_map_lead_can_hold_when_no_opening",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 12500 );
    }

    const bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "active_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::active;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;

    bandit_live_world::camp_map_dispatch_pressure pressure;
    pressure.opening_available = false;
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, lead, pressure );
    CHECK_FALSE( plan.valid );
    REQUIRE_FALSE( plan.notes.empty() );
    CHECK( plan.notes.back().find( "held pressure" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_live_signal_marks_write_camp_map_signal_leads",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 12600 );
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::live_signal_mark smoke;
    smoke.mark_id = "live_smoke@18,20,0";
    smoke.kind = "smoke";
    smoke.source_omt = tripoint_abs_omt( 18, 20, 0 );
    smoke.range_cap_omt = 15;
    smoke.bounty_add = 2;
    smoke.threat_add = 1;
    smoke.confidence = 2;

    CHECK( bandit_live_world::record_live_signal_mark( site, smoke ) );
    REQUIRE( site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &lead = site.intelligence_map.leads.front();
    CHECK( lead.kind == bandit_live_world::camp_lead_kind::smoke_signal );
    CHECK( lead.status == bandit_live_world::camp_lead_status::suspected );
    CHECK( lead.target_id == smoke.mark_id );
    CHECK( lead.omt == smoke.source_omt );
    CHECK( lead.radius_omt == smoke.range_cap_omt );
    CHECK( lead.bounty == smoke.bounty_add );
    CHECK( lead.threat == smoke.threat_add );
    CHECK( lead.confidence == smoke.confidence );
}

TEST_CASE( "bandit_live_world_scheduler_state_migrates_and_fails_closed",
           "[bandit][live_world][scheduler][save]" )
{
    bandit_live_world::world_state current;
    for( int index = 0; index < 3; ++index ) {
        add_scheduler_test_site( current, index, index % 2 != 0 );
    }
    current.sites.front().intelligence_map.terrain_scan_cursor = 7;
    current.routine_scheduler_cursor = 2;
    current.routine_terrain_scan_cursor = 1;
    current.routine_scheduler_last_hour = 17;
    const std::string current_bytes = serialize_world( current );
    const bandit_live_world::world_state loaded = round_trip_world( current );
    CHECK( loaded.schema_version == 6 );
    CHECK( loaded.routine_scheduler_cursor == 2 );
    CHECK( loaded.routine_terrain_scan_cursor == 1 );
    CHECK( loaded.routine_scheduler_last_hour == 17 );
    CHECK( loaded.sites.front().intelligence_map.terrain_scan_cursor == 7 );
    CHECK( serialize_world( loaded ) == current_bytes );

    const auto replace_once = []( std::string &bytes, const std::string &before,
    const std::string &after ) {
        const std::size_t at = bytes.find( before );
        REQUIRE( at != std::string::npos );
        bytes.replace( at, before.size(), after );
    };
    std::string legacy_bytes = current_bytes;
    replace_once( legacy_bytes, "\"schema_version\": 6", "\"schema_version\": 4" );
    erase_pretty_json_member_line( legacy_bytes, "routine_scheduler_cursor" );
    erase_pretty_json_member_line( legacy_bytes, "routine_terrain_scan_cursor" );
    erase_pretty_json_member_line( legacy_bytes, "routine_scheduler_last_hour" );
    bandit_live_world::world_state migrated;
    JsonValue legacy_json = json_loader::from_string( legacy_bytes );
    migrated.deserialize( legacy_json.get_object() );
    CHECK( migrated.schema_version == 6 );
    CHECK( migrated.routine_scheduler_cursor == 0 );
    CHECK( migrated.routine_terrain_scan_cursor == 0 );
    CHECK( migrated.routine_scheduler_last_hour == -1 );

    std::string v5_bytes = current_bytes;
    replace_once( v5_bytes, "\"schema_version\": 6", "\"schema_version\": 5" );
    erase_pretty_json_member_line( v5_bytes, "routine_terrain_scan_cursor" );
    bandit_live_world::world_state migrated_v5;
    JsonValue v5_json = json_loader::from_string( v5_bytes );
    migrated_v5.deserialize( v5_json.get_object() );
    CHECK( migrated_v5.schema_version == 6 );
    CHECK( migrated_v5.routine_scheduler_cursor == 2 );
    CHECK( migrated_v5.routine_terrain_scan_cursor == 0 );
    CHECK( migrated_v5.routine_scheduler_last_hour == 17 );

    std::vector<std::string> malformed_packets;
    std::string missing_both = current_bytes;
    erase_pretty_json_member_line( missing_both, "routine_scheduler_cursor" );
    erase_pretty_json_member_line( missing_both, "routine_scheduler_last_hour" );
    malformed_packets.push_back( std::move( missing_both ) );
    std::string missing_cursor = current_bytes;
    erase_pretty_json_member_line( missing_cursor, "routine_scheduler_cursor" );
    malformed_packets.push_back( std::move( missing_cursor ) );
    std::string missing_hour = current_bytes;
    erase_pretty_json_member_line( missing_hour, "routine_scheduler_last_hour" );
    malformed_packets.push_back( std::move( missing_hour ) );
    std::string missing_terrain_cursor = current_bytes;
    erase_pretty_json_member_line( missing_terrain_cursor, "routine_terrain_scan_cursor" );
    malformed_packets.push_back( std::move( missing_terrain_cursor ) );
    std::string negative_cursor = current_bytes;
    replace_once( negative_cursor, "\"routine_scheduler_cursor\": 2",
                  "\"routine_scheduler_cursor\": -1" );
    malformed_packets.push_back( std::move( negative_cursor ) );
    std::string out_of_range_cursor = current_bytes;
    replace_once( out_of_range_cursor, "\"routine_scheduler_cursor\": 2",
                  "\"routine_scheduler_cursor\": 3" );
    malformed_packets.push_back( std::move( out_of_range_cursor ) );
    std::string invalid_terrain_cursor = current_bytes;
    replace_once( invalid_terrain_cursor, "\"routine_terrain_scan_cursor\": 1",
                  "\"routine_terrain_scan_cursor\": 3" );
    malformed_packets.push_back( std::move( invalid_terrain_cursor ) );
    std::string invalid_hour = current_bytes;
    replace_once( invalid_hour, "\"routine_scheduler_last_hour\": 17",
                  "\"routine_scheduler_last_hour\": -2" );
    malformed_packets.push_back( std::move( invalid_hour ) );
    std::string future_schema = current_bytes;
    replace_once( future_schema, "\"schema_version\": 6", "\"schema_version\": 7" );
    malformed_packets.push_back( std::move( future_schema ) );
    std::string spoofed_legacy = current_bytes;
    replace_once( spoofed_legacy, "\"schema_version\": 6", "\"schema_version\": 5" );
    malformed_packets.push_back( std::move( spoofed_legacy ) );

    bandit_live_world::world_state protected_world = current;
    const std::string protected_bytes = serialize_world( protected_world );
    for( const std::string &packet : malformed_packets ) {
        JsonValue malformed = json_loader::from_string( packet );
        CHECK_THROWS( protected_world.deserialize( malformed.get_object() ) );
        CHECK( serialize_world( protected_world ) == protected_bytes );
    }

    bandit_live_world::world_state site_state;
    add_scheduler_test_site( site_state, 0, false, 320000 );
    site_state.sites.front().routine_activated_minutes = 100;
    site_state.sites.front().last_routine_resolved_minutes = 200;
    site_state.sites.front().next_routine_dispatch_eligible_minutes = 1640;
    site_state.sites.front().routine_no_candidate_streak = 2;
    const std::string current_site_bytes = serialize_world( site_state );
    const bandit_live_world::world_state loaded_site_state = round_trip_world( site_state );
    REQUIRE( loaded_site_state.sites.size() == 1 );
    CHECK( loaded_site_state.sites.front().schema_version == 12 );
    CHECK( loaded_site_state.sites.front().routine_activated_minutes == 100 );
    CHECK( loaded_site_state.sites.front().last_routine_resolved_minutes == 200 );
    CHECK( loaded_site_state.sites.front().next_routine_dispatch_eligible_minutes == 1640 );
    CHECK( loaded_site_state.sites.front().routine_no_candidate_streak == 2 );
    CHECK( serialize_world( loaded_site_state ) == current_site_bytes );

    std::string legacy_site_bytes = current_site_bytes;
    replace_once( legacy_site_bytes, "\"schema_version\": 12", "\"schema_version\": 11" );
    erase_pretty_json_member_line( legacy_site_bytes, "routine_activated_minutes" );
    erase_pretty_json_member_line( legacy_site_bytes, "last_routine_resolved_minutes" );
    erase_pretty_json_member_line( legacy_site_bytes, "next_routine_dispatch_eligible_minutes" );
    erase_pretty_json_member_line( legacy_site_bytes, "routine_no_candidate_streak" );
    bandit_live_world::world_state migrated_site_state;
    JsonValue legacy_site_json = json_loader::from_string( legacy_site_bytes );
    migrated_site_state.deserialize( legacy_site_json.get_object() );
    REQUIRE( migrated_site_state.sites.size() == 1 );
    CHECK( migrated_site_state.sites.front().schema_version == 12 );
    CHECK( migrated_site_state.sites.front().routine_activated_minutes == -1 );
    CHECK( migrated_site_state.sites.front().last_routine_resolved_minutes == -1 );
    CHECK( migrated_site_state.sites.front().next_routine_dispatch_eligible_minutes == -1 );
    CHECK( migrated_site_state.sites.front().routine_no_candidate_streak == 0 );

    std::vector<std::string> malformed_site_packets;
    std::string missing_site_field = current_site_bytes;
    erase_pretty_json_member_line( missing_site_field, "routine_no_candidate_streak" );
    malformed_site_packets.push_back( std::move( missing_site_field ) );
    std::string invalid_activation = current_site_bytes;
    replace_once( invalid_activation, "\"routine_activated_minutes\": 100",
                  "\"routine_activated_minutes\": -2" );
    malformed_site_packets.push_back( std::move( invalid_activation ) );
    std::string invalid_resolution = current_site_bytes;
    replace_once( invalid_resolution, "\"last_routine_resolved_minutes\": 200",
                  "\"last_routine_resolved_minutes\": 99" );
    malformed_site_packets.push_back( std::move( invalid_resolution ) );
    std::string invalid_cooldown = current_site_bytes;
    replace_once( invalid_cooldown, "\"next_routine_dispatch_eligible_minutes\": 1640",
                  "\"next_routine_dispatch_eligible_minutes\": 99" );
    malformed_site_packets.push_back( std::move( invalid_cooldown ) );
    std::string invalid_streak = current_site_bytes;
    replace_once( invalid_streak, "\"routine_no_candidate_streak\": 2",
                  "\"routine_no_candidate_streak\": 4" );
    malformed_site_packets.push_back( std::move( invalid_streak ) );
    std::string future_site_schema = current_site_bytes;
    replace_once( future_site_schema, "\"schema_version\": 12", "\"schema_version\": 13" );
    malformed_site_packets.push_back( std::move( future_site_schema ) );
    std::string spoofed_v11_site = current_site_bytes;
    replace_once( spoofed_v11_site, "\"schema_version\": 12", "\"schema_version\": 11" );
    malformed_site_packets.push_back( std::move( spoofed_v11_site ) );

    bandit_live_world::world_state protected_site_state = site_state;
    const std::string protected_site_bytes = serialize_world( protected_site_state );
    for( const std::string &packet : malformed_site_packets ) {
        JsonValue malformed = json_loader::from_string( packet );
        CHECK_THROWS( protected_site_state.deserialize( malformed.get_object() ) );
        CHECK( serialize_world( protected_site_state ) == protected_site_bytes );
    }
}

TEST_CASE( "bandit_live_world_scheduler_rotates_bounded_fair_service",
           "[bandit][live_world][scheduler][fairness]" )
{
    const auto verify_scale = []( const int site_count, const int hourly_passes,
    const int expected_max_first_wait ) {
        bandit_live_world::world_state world;
        world.sites.reserve( static_cast<std::size_t>( site_count ) );
        for( int index = 0; index < site_count; ++index ) {
            add_scheduler_test_site( world, index, index % 2 != 0 );
            world.sites.back().routine_activated_minutes = 0;
        }
        std::vector<int> first_considered_pass( static_cast<std::size_t>( site_count ), -1 );
        bandit_live_world_probe::session probe(
            bandit_live_world_probe::collection_mode::site_services, 0,
            static_cast<std::size_t>( site_count ) );
        int expected_cursor = 0;
        for( int pass = 0; pass < hourly_passes; ++pass ) {
            const bandit_live_world::structural_bounty_maintenance_result result =
                bandit_live_world::advance_structural_bounty_maintenance(
                    world, ( 18 + pass ) * 60, 0, 99,
            []( const tripoint_abs_omt & ) -> std::optional<std::string> {
                return std::nullopt;
            }, []( const bandit_live_world::site_record &,
                   const bandit_live_world::camp_map_lead & ) {
                return bandit_live_world::structural_threat_read{};
            } );
            CHECK( result.scheduler_hour == 18 + pass );
            CHECK( result.scheduler_cursor_before == expected_cursor );
            CHECK( result.scheduler_consider_cap == 16 );
            CHECK( result.sites_considered_for_dispatch == 16 );
            CHECK( result.dispatch_cap == 2 );
            CHECK( result.dispatches_applied == 2 );
            CHECK( result.full_route_solve_cap == 8 );
            CHECK( result.full_route_solves == 8 );
            expected_cursor = ( expected_cursor + 16 ) % site_count;
            CHECK( result.scheduler_cursor_after == expected_cursor );

            const bandit_live_world_probe::snapshot &snapshot = probe.result();
            for( const bandit_live_world_probe::site_service_record &service :
                 snapshot.site_services ) {
                const std::string prefix = "scheduler-site-";
                REQUIRE( service.site_id.find( prefix ) == 0 );
                const int index = std::stoi( service.site_id.substr( prefix.size() ) );
                const std::uint64_t considered = service.counts[static_cast<std::size_t>(
                        bandit_live_world_probe::site_service::dispatch_considered )];
                if( considered > 0 && first_considered_pass[static_cast<std::size_t>( index )] < 0 ) {
                    first_considered_pass[static_cast<std::size_t>( index )] = pass;
                }
            }
        }

        CHECK( world.sites.front().profile ==
               bandit_live_world::hostile_site_profile::camp_style );
        CHECK( world.sites[1].profile ==
               bandit_live_world::hostile_site_profile::cannibal_camp );
        CHECK( *std::max_element( first_considered_pass.begin(),
                                 first_considered_pass.end() ) == expected_max_first_wait );
        CHECK( std::count( first_considered_pass.begin(), first_considered_pass.end(), -1 ) == 0 );
    };

    SECTION( "one hundred sites complete a rotation within seven hours" ) {
        verify_scale( 100, 7, 6 );
    }
    SECTION( "five hundred sites complete a rotation within thirty two hours" ) {
        verify_scale( 500, 32, 31 );
    }
    SECTION( "retired and micro sites do not consume routine camp service slots" ) {
        bandit_live_world::world_state world;
        world.sites.reserve( 500 );
        for( int index = 0; index < 500; ++index ) {
            add_scheduler_test_site( world, index, index % 2 != 0 );
            bandit_live_world::site_record &site = world.sites.back();
            site.routine_activated_minutes = 0;
            if( index < 200 ) {
                site.profile = bandit_live_world::hostile_site_profile::small_hostile_site;
            } else if( index < 400 ) {
                site.retired_empty_site = true;
            }
        }

        std::vector<int> first_considered_pass( 100, -1 );
        bandit_live_world_probe::session probe(
            bandit_live_world_probe::collection_mode::site_services, 0, 500 );
        for( int pass = 0; pass < 7; ++pass ) {
            const bandit_live_world::structural_bounty_maintenance_result result =
                bandit_live_world::advance_structural_bounty_maintenance(
                    world, ( 18 + pass ) * 60, 0, 99,
            []( const tripoint_abs_omt & ) -> std::optional<std::string> {
                return std::nullopt;
            }, []( const bandit_live_world::site_record &,
                   const bandit_live_world::camp_map_lead & ) {
                return bandit_live_world::structural_threat_read{};
            } );
            CHECK( result.sites_considered_for_dispatch == 16 );
            const bandit_live_world_probe::snapshot &snapshot = probe.result();
            for( const bandit_live_world_probe::site_service_record &service :
                 snapshot.site_services ) {
                const std::string prefix = "scheduler-site-";
                REQUIRE( service.site_id.find( prefix ) == 0 );
                const int index = std::stoi( service.site_id.substr( prefix.size() ) );
                const std::uint64_t considered = service.counts[static_cast<std::size_t>(
                        bandit_live_world_probe::site_service::dispatch_considered )];
                if( considered > 0 ) {
                    REQUIRE( index >= 400 );
                    if( first_considered_pass[static_cast<std::size_t>( index - 400 )] < 0 ) {
                        first_considered_pass[static_cast<std::size_t>( index - 400 )] = pass;
                    }
                }
            }
        }
        CHECK( std::count( first_considered_pass.begin(), first_considered_pass.end(), -1 ) == 0 );
        CHECK( *std::max_element( first_considered_pass.begin(),
                                 first_considered_pass.end() ) == 6 );
        CHECK( world.routine_scheduler_cursor == 12 );
    }
    SECTION( "repeatedly eligible window leaders cannot monopolize the two start slots" ) {
        bandit_live_world::world_state world;
        std::vector<std::string> lead_ids;
        std::vector<bool> started( 16, false );
        for( int index = 0; index < 16; ++index ) {
            add_scheduler_test_site( world, index, index % 2 != 0, 350000 );
            bandit_live_world::site_record &site = world.sites.back();
            site.routine_activated_minutes = 100000;
            site.intelligence_map.frontier_last_resolved_minutes.assign( 8, 100000 );
            const tripoint_abs_omt target( site.anchor.x() + 4, site.anchor.y(), site.anchor.z() );
            const bandit_live_world::structural_bounty_read read =
                bandit_live_world::classify_structural_bounty_terrain( "forest" );
            REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, target, read, 0 ) );
            lead_ids.push_back( bandit_live_world::make_structural_bounty_lead_id(
                                    site.site_id, target, "forest" ) );
        }

        for( int pass = 0; pass < 8; ++pass ) {
            const int now_minutes = 100000 + pass * 125 * 60;
            for( bandit_live_world::site_record &site : world.sites ) {
                site.intelligence_map.frontier_last_resolved_minutes.assign( 8, now_minutes );
                site.last_routine_resolved_minutes = now_minutes - 72 * 60;
            }
            const bandit_live_world::structural_bounty_maintenance_result result =
                bandit_live_world::advance_structural_bounty_maintenance(
                    world, now_minutes, 0, 99,
            []( const tripoint_abs_omt & ) -> std::optional<std::string> {
                return std::nullopt;
            }, []( const bandit_live_world::site_record &,
                   const bandit_live_world::camp_map_lead & ) {
                return bandit_live_world::structural_threat_read{};
            } );
            CHECK( result.dispatches_applied == 2 );
            for( std::size_t index = 0; index < world.sites.size(); ++index ) {
                bandit_live_world::site_record &site = world.sites[index];
                if( site.active_outing.kind != bandit_live_world::outing_kind::structural_sortie ) {
                    continue;
                }
                started[index] = true;
                const std::string activity_id = site.active_outing.activity_id;
                const int generation = site.active_outing.generation;
                REQUIRE( bandit_live_world::release_matching_external_reservation(
                             site, activity_id, generation, "scheduler fairness test release" ) == 2 );
                bandit_live_world::camp_map_lead *lead =
                    site.intelligence_map.find_lead( lead_ids[index] );
                REQUIRE( lead != nullptr );
                lead->status = bandit_live_world::camp_lead_status::suspected;
                lead->last_checked_minutes = -1;
                lead->last_outcome.clear();
                site.last_routine_resolved_minutes = now_minutes;
                site.next_routine_dispatch_eligible_minutes = now_minutes + 1;
            }
        }
        CHECK( std::count( started.begin(), started.end(), false ) == 0 );
    }
}

TEST_CASE( "hostile_camp_routed_dispatch_uses_exact_drive_score_and_risk_boundaries",
           "[bandit][live_world][scheduler][structural_bounty][routed_dispatch]" )
{
    CHECK( bandit_live_world::hostile_camp_dispatch_drive( 1000, 599, 0, 0 ) == 499 );
    CHECK( bandit_live_world::hostile_camp_dispatch_drive( 1000, 600, 0, 0 ) == 500 );
    CHECK_FALSE( bandit_live_world::hostile_camp_routine_score_eligible( 299, false ) );
    CHECK( bandit_live_world::hostile_camp_routine_score_eligible( 300, false ) );
    CHECK_FALSE( bandit_live_world::hostile_camp_routine_score_eligible( 149, true ) );
    CHECK( bandit_live_world::hostile_camp_routine_score_eligible( 150, true ) );
    CHECK_FALSE( bandit_live_world::hostile_camp_routine_risk_blocked( 749 ) );
    CHECK( bandit_live_world::hostile_camp_routine_risk_blocked( 750 ) );
    CHECK( bandit_live_world::hostile_camp_routine_route_risk_eligible( 749, 499 ) );
    CHECK_FALSE( bandit_live_world::hostile_camp_routine_route_risk_eligible( 749, 500 ) );
    CHECK_FALSE( bandit_live_world::hostile_camp_routine_route_risk_eligible( 750, 0 ) );

    bandit_live_world::world_state world;
    add_scheduler_test_site( world, 0, false, 520000 );
    bandit_live_world::site_record &site = world.sites.front();
    site.routine_activated_minutes = 0;
    site.supply_last_update_minutes = 0;
    site.intelligence_map.frontier_last_resolved_minutes.assign( 8, 1000 );

    site.supply_units = 21;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 1000, 0 ).need == 0 );
    site.supply_units = 20;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 1000, 0 ).need == 333 );
    site.supply_units = 9;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 1000, 0 ).need == 333 );
    site.supply_units = 8;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 1000, 0 ).need == 667 );
    site.supply_units = 3;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 1000, 0 ).need == 667 );
    site.supply_units = 2;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 1000, 0 ).need == 1000 );

    site.intelligence_map.frontier_last_resolved_minutes.assign( 8, 0 );
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 7 * 24 * 60, 0 ).knowledge_gap == 0 );
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 7 * 24 * 60 + 1, 0 ).knowledge_gap == 1000 );
    site.last_routine_resolved_minutes = 100;
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 100 + 24 * 60, 0 ).cadence == 0 );
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 100 + 72 * 60, 0 ).cadence == 1000 );
    CHECK_FALSE( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
                     site, 100 + 72 * 60 - 1, 0 ).force_due );
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, 100 + 72 * 60, 0 ).force_due );

    site.last_routine_resolved_minutes = -1;
    int first_force_due = -1;
    for( int hour = 6; hour <= 18; ++hour ) {
        if( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
                site, hour * 60, 0 ).force_due ) {
            first_force_due = hour * 60;
            break;
        }
    }
    REQUIRE( first_force_due >= 6 * 60 );
    CHECK_FALSE( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
                     site, first_force_due - 1, 0 ).force_due );
    CHECK( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
               site, first_force_due, 0 ).force_due );
}

TEST_CASE( "hostile_camp_routed_dispatch_ranks_cheap_then_solves_only_top_two",
           "[bandit][live_world][scheduler][structural_bounty][routed_dispatch]" )
{
    bandit_live_world::world_state world;
    add_scheduler_test_site( world, 0, false, 521000 );
    bandit_live_world::site_record &site = world.sites.front();
    site.routine_activated_minutes = 0;
    site.supply_units = 0;
    site.supply_last_update_minutes = 0;
    site.intelligence_map.frontier_last_resolved_minutes.assign( 8, -1 );
    const bandit_live_world::structural_bounty_read read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    std::vector<std::string> lead_ids;
    for( const int distance : { 2, 3, 4 } ) {
        const tripoint_abs_omt target( site.anchor.x() + distance,
                                       site.anchor.y(), site.anchor.z() );
        REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, target, read, 0 ) );
        lead_ids.push_back( bandit_live_world::make_structural_bounty_lead_id(
                                site.site_id, target, "forest" ) );
    }

    std::vector<std::string> routed_leads;
    const bandit_live_world::structural_bounty_maintenance_result result =
        bandit_live_world::advance_structural_bounty_maintenance(
            world, 60, 0, 1,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::nullopt;
    }, []( const bandit_live_world::site_record &,
           const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    }, [&routed_leads, &lead_ids]( const bandit_live_world::site_record &,
    const bandit_live_world::structural_outing_plan & plan ) {
        routed_leads.push_back( plan.lead_id );
        bandit_live_world::structural_route_read route;
        route.reachable = true;
        route.complete_route_cost = plan.lead_id == lead_ids.front() ? 18 : 2;
        route.max_segment_risk = 400;
        return route;
    } );
    REQUIRE( routed_leads.size() == 2 );
    CHECK( routed_leads[0] == lead_ids[0] );
    CHECK( routed_leads[1] == lead_ids[1] );
    CHECK( std::find( routed_leads.begin(), routed_leads.end(), lead_ids[2] ) ==
           routed_leads.end() );
    CHECK( result.full_route_solves == 2 );
    CHECK( result.dispatches_applied == 1 );
    CHECK( site.active_outing.target_id == lead_ids[1] );
}

TEST_CASE( "hostile_camp_routed_dispatch_enforces_global_eight_solve_two_start_budget",
           "[bandit][live_world][scheduler][structural_bounty][routed_dispatch][fairness][save]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 16; ++index ) {
        add_scheduler_test_site( world, index, index % 2 != 0, 522000 );
        bandit_live_world::site_record &site = world.sites.back();
        site.routine_activated_minutes = 0;
        site.supply_units = 0;
        site.supply_last_update_minutes = 0;
        site.intelligence_map.frontier_last_resolved_minutes.assign( 8, -1 );
        const bandit_live_world::structural_bounty_read read =
            bandit_live_world::classify_structural_bounty_terrain( "building" );
        for( const int distance : { 2, 3, 4 } ) {
            const tripoint_abs_omt target( site.anchor.x() + distance,
                                           site.anchor.y(), site.anchor.z() );
            REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, target, read, 0 ) );
        }
    }

    int route_calls = 0;
    const auto route_lookup = [&route_calls]( const bandit_live_world::site_record &,
    const bandit_live_world::structural_outing_plan & ) {
        route_calls++;
        return bandit_live_world::structural_route_read{ true, 4, 400, "test route" };
    };
    const auto terrain_lookup = []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::nullopt;
    };
    const auto threat_lookup = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    };
    const bandit_live_world::structural_bounty_maintenance_result first =
        bandit_live_world::advance_structural_bounty_maintenance(
            world, 60, 0, 99, terrain_lookup, threat_lookup, route_lookup );
    CHECK( route_calls == 8 );
    CHECK( first.full_route_solves == 8 );
    CHECK( first.dispatches_applied == 2 );
    CHECK( first.dispatches_planned <= 4 );
    for( const bandit_live_world::site_record &site : world.sites ) {
        if( site.active_outing.kind == bandit_live_world::outing_kind::none ) {
            CHECK( site.routine_no_candidate_streak == 0 );
        }
    }

    const std::string before_replay = serialize_world( world );
    const bandit_live_world::structural_bounty_maintenance_result replay =
        bandit_live_world::advance_structural_bounty_maintenance(
            world, 60, 0, 99, terrain_lookup, threat_lookup, route_lookup );
    CHECK( replay.scheduler_replay_suppressed );
    CHECK( replay.full_route_solves == 0 );
    CHECK( route_calls == 8 );
    CHECK( serialize_world( world ) == before_replay );
}

TEST_CASE( "hostile_camp_routed_dispatch_drive_and_force_due_do_not_create_false_bypass",
           "[bandit][live_world][scheduler][structural_bounty][routed_dispatch]" )
{
    const auto terrain_lookup = []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::nullopt;
    };
    const auto threat_lookup = []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    };

    bandit_live_world::world_state low_drive;
    add_scheduler_test_site( low_drive, 0, false, 523000 );
    bandit_live_world::site_record &low_site = low_drive.sites.front();
    low_site.routine_activated_minutes = 0;
    low_site.supply_units = 21;
    low_site.supply_last_update_minutes = 0;
    low_site.intelligence_map.frontier_last_resolved_minutes.assign( 8, 60 );
    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                 low_site, tripoint_abs_omt( 2, 0, 0 ), forest, 0 ) );
    int low_route_calls = 0;
    const bandit_live_world::structural_bounty_maintenance_result gated =
        bandit_live_world::advance_structural_bounty_maintenance(
            low_drive, 60, 0, 1, terrain_lookup, threat_lookup,
    [&low_route_calls]( const bandit_live_world::site_record &,
    const bandit_live_world::structural_outing_plan & ) {
        low_route_calls++;
        return bandit_live_world::structural_route_read{ true, 4, 400, "" };
    } );
    CHECK( gated.dispatches_applied == 0 );
    CHECK( low_route_calls == 0 );
    CHECK( low_site.routine_no_candidate_streak == 0 );

    bandit_live_world::world_state no_candidate;
    add_scheduler_test_site( no_candidate, 0, false, 523500 );
    bandit_live_world::site_record &empty_site = no_candidate.sites.front();
    empty_site.routine_activated_minutes = 0;
    empty_site.supply_units = 21;
    empty_site.supply_last_update_minutes = 0;
    empty_site.intelligence_map.frontier_last_resolved_minutes.assign( 8, 60 );
    const bandit_live_world::structural_bounty_maintenance_result empty_gated =
        bandit_live_world::advance_structural_bounty_maintenance(
            no_candidate, 60, 0, 1, terrain_lookup, threat_lookup );
    CHECK( empty_gated.dispatches_applied == 0 );
    CHECK( empty_site.routine_no_candidate_streak == 0 );
    CHECK( empty_site.next_routine_dispatch_eligible_minutes == -1 );

    bandit_live_world::world_state hard_risk;
    add_scheduler_test_site( hard_risk, 0, false, 524000 );
    bandit_live_world::site_record &risk_site = hard_risk.sites.front();
    risk_site.routine_activated_minutes = 0;
    risk_site.supply_units = 0;
    risk_site.supply_last_update_minutes = 0;
    risk_site.intelligence_map.frontier_last_resolved_minutes.clear();
    bandit_live_world::camp_map_lead risk_lead;
    risk_lead.lead_id = "force-due-impassable";
    risk_lead.revision = 1;
    risk_lead.kind = bandit_live_world::camp_lead_kind::terrain_opportunity;
    risk_lead.status = bandit_live_world::camp_lead_status::suspected;
    risk_lead.target_id = "impassable";
    risk_lead.omt = tripoint_abs_omt( 2, 0, 0 );
    risk_lead.source_key = "structural_bounty:open:terrain_fit:impassable";
    risk_site.intelligence_map.leads.push_back( risk_lead );
    const int force_due_minutes = 18 * 60;
    REQUIRE( bandit_live_world::evaluate_hostile_camp_routine_dispatch(
                 risk_site, force_due_minutes, 0 ).force_due );
    int risk_route_calls = 0;
    const bandit_live_world::structural_bounty_maintenance_result blocked =
        bandit_live_world::advance_structural_bounty_maintenance(
            hard_risk, force_due_minutes, 0, 1, terrain_lookup, threat_lookup,
    [&risk_route_calls]( const bandit_live_world::site_record &,
    const bandit_live_world::structural_outing_plan & ) {
        risk_route_calls++;
        return bandit_live_world::structural_route_read{ true, 4, 0, "" };
    } );
    CHECK( blocked.dispatches_applied == 0 );
    CHECK( risk_route_calls == 0 );
}

TEST_CASE( "hostile_camp_terrain_scan_rotates_fairly_and_resumes_exact_offsets",
           "[bandit][live_world][scheduler][structural_bounty][terrain_fit][fairness][save]" )
{
    const auto verify_scale = []( const int site_count, const int scan_budget,
    const int hourly_passes ) {
        bandit_live_world::world_state world;
        world.sites.reserve( static_cast<std::size_t>( site_count ) );
        for( int index = 0; index < site_count; ++index ) {
            add_scheduler_test_site( world, index, index % 2 != 0, 500000 );
        }
        bandit_live_world_probe::session probe(
            bandit_live_world_probe::collection_mode::site_services, 0,
            static_cast<std::size_t>( site_count ) );
        for( int pass = 0; pass < hourly_passes; ++pass ) {
            const bandit_live_world::structural_bounty_maintenance_result result =
                bandit_live_world::advance_structural_bounty_maintenance(
                    world, ( 18 + pass ) * 60, scan_budget, 0,
            []( const tripoint_abs_omt & ) -> std::optional<std::string> {
                return std::string( "field" );
            }, []( const bandit_live_world::site_record &,
                   const bandit_live_world::camp_map_lead & ) {
                return bandit_live_world::structural_threat_read{};
            } );
            CHECK( result.terrain_scan_sites_selected == scan_budget );
            CHECK( result.scan.candidates_sampled == scan_budget );
            CHECK( result.scan.budget_used == scan_budget );
            CHECK( result.dispatches_applied == 0 );
            CHECK( result.full_route_solves == 0 );
        }

        const bandit_live_world_probe::snapshot &snapshot = probe.result();
        REQUIRE( snapshot.site_services.size() == static_cast<std::size_t>( site_count ) );
        for( const bandit_live_world_probe::site_service_record &service : snapshot.site_services ) {
            CHECK( service.counts[static_cast<std::size_t>(
                       bandit_live_world_probe::site_service::scan_samples )] == 1 );
        }
        for( const bandit_live_world::site_record &site : world.sites ) {
            CHECK( site.intelligence_map.terrain_scan_cursor == 1 );
            REQUIRE( site.intelligence_map.leads.size() == 1 );
            CHECK( site.intelligence_map.leads.front().kind ==
                   bandit_live_world::camp_lead_kind::terrain_opportunity );
            CHECK( site.intelligence_map.leads.front().target_id == "field" );
            CHECK( site.intelligence_map.leads.front().bounty == 0 );
        }
        CHECK( world.routine_terrain_scan_cursor == 0 );
    };

    SECTION( "four lookups per hour cover one hundred and five hundred camps without prefix bias" ) {
        verify_scale( 100, 4, 25 );
        verify_scale( 500, 4, 125 );
    }

    SECTION( "save load resumes the next near-ring offset and replay performs no lookup" ) {
        bandit_live_world::world_state world;
        add_scheduler_test_site( world, 0, false, 510000 );
        const tripoint_abs_omt anchor = world.sites.front().anchor;
        std::vector<tripoint_abs_omt> sampled;
        const auto terrain_lookup = [&sampled]( const tripoint_abs_omt &omt ) ->
        std::optional<std::string> {
            sampled.push_back( omt );
            return std::string( "field" );
        };
        const auto threat_lookup = []( const bandit_live_world::site_record &,
        const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{};
        };
        const bandit_live_world::structural_bounty_maintenance_result first =
            bandit_live_world::advance_structural_bounty_maintenance(
                world, 18 * 60, 1, 0, terrain_lookup, threat_lookup );
        REQUIRE( sampled.size() == 1 );
        CHECK( sampled.front() == tripoint_abs_omt( anchor.x() - 4, anchor.y(), anchor.z() ) );
        CHECK( first.terrain_scan_cursor_before == 0 );
        CHECK( first.terrain_scan_cursor_after == 0 );
        CHECK( world.sites.front().intelligence_map.terrain_scan_cursor == 1 );

        bandit_live_world::world_state resumed = round_trip_world( world );
        const std::string before_replay = serialize_world( resumed );
        const bandit_live_world::structural_bounty_maintenance_result replay =
            bandit_live_world::advance_structural_bounty_maintenance(
                resumed, 18 * 60, 1, 0, terrain_lookup, threat_lookup );
        CHECK( replay.scheduler_replay_suppressed );
        CHECK( sampled.size() == 1 );
        CHECK( serialize_world( resumed ) == before_replay );

        bandit_live_world::advance_structural_bounty_maintenance(
            resumed, 19 * 60, 1, 0, terrain_lookup, threat_lookup );
        REQUIRE( sampled.size() == 2 );
        CHECK( sampled.back() == tripoint_abs_omt( anchor.x() + 4, anchor.y(), anchor.z() ) );
        CHECK( resumed.sites.front().intelligence_map.terrain_scan_cursor == 2 );
    }
}

TEST_CASE( "bandit_live_world_scheduler_replay_and_frontier_due_state_are_deterministic",
           "[bandit][live_world][scheduler][frontier][save]" )
{
    SECTION( "save load resumes the exact cursor and same-hour replay is byte stable" ) {
        bandit_live_world::world_state world;
        for( int index = 0; index < 20; ++index ) {
            add_scheduler_test_site( world, index, index % 2 != 0 );
            world.sites.back().routine_activated_minutes = 0;
        }
        const auto terrain_lookup = []( const tripoint_abs_omt & ) -> std::optional<std::string> {
            return std::nullopt;
        };
        const auto threat_lookup = []( const bandit_live_world::site_record &,
        const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{};
        };
        const bandit_live_world::structural_bounty_maintenance_result first =
            bandit_live_world::advance_structural_bounty_maintenance(
                world, 18 * 60, 0, 2, terrain_lookup, threat_lookup );
        CHECK( first.scheduler_cursor_before == 0 );
        CHECK( first.scheduler_cursor_after == 16 );
        CHECK( first.dispatches_applied == 2 );

        bandit_live_world::world_state resumed = round_trip_world( world );
        CHECK( resumed.routine_scheduler_cursor == 16 );
        CHECK( resumed.routine_scheduler_last_hour == 18 );
        const bandit_live_world::structural_bounty_maintenance_result continued =
            bandit_live_world::advance_structural_bounty_maintenance(
                resumed, 19 * 60, 0, 2, terrain_lookup, threat_lookup );
        CHECK( continued.scheduler_cursor_before == 16 );
        CHECK( continued.scheduler_cursor_after == 12 );
        CHECK( continued.sites_considered_for_dispatch == 16 );
        const std::string before_replay = serialize_world( resumed );
        const bandit_live_world::structural_bounty_maintenance_result replay =
            bandit_live_world::advance_structural_bounty_maintenance(
                resumed, 19 * 60, 0, 2, terrain_lookup, threat_lookup );
        CHECK( replay.scheduler_replay_suppressed );
        CHECK( replay.sites_considered_for_dispatch == 0 );
        CHECK( replay.scheduler_cursor_before == 12 );
        CHECK( replay.scheduler_cursor_after == 12 );
        CHECK( serialize_world( resumed ) == before_replay );
    }

    SECTION( "no bounded candidate backs off twelve twenty four then forty eight hours" ) {
        bandit_live_world::world_state world;
        add_scheduler_test_site( world, 0, false, 380000 );
        bandit_live_world::site_record &site = world.sites.front();
        site.routine_activated_minutes = 0;
        site.intelligence_map.frontier_last_resolved_minutes.assign( 8, 0 );
        const tripoint_abs_omt unreachable( site.anchor.x() + 100,
                                            site.anchor.y(), site.anchor.z() );
        const bandit_live_world::structural_bounty_read read =
            bandit_live_world::classify_structural_bounty_terrain( "forest" );
        REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                     site, unreachable, read, 0 ) );
        const auto terrain_lookup = []( const tripoint_abs_omt & ) -> std::optional<std::string> {
            return std::nullopt;
        };
        const auto threat_lookup = []( const bandit_live_world::site_record &,
        const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{};
        };

        int now_minutes = 18 * 60;
        const bandit_live_world::structural_bounty_maintenance_result first =
            bandit_live_world::advance_structural_bounty_maintenance(
                world, now_minutes, 0, 2, terrain_lookup, threat_lookup );
        CHECK( first.dispatches_applied == 0 );
        CHECK( site.routine_no_candidate_streak == 1 );
        CHECK( site.next_routine_dispatch_eligible_minutes - now_minutes >= 12 * 60 );
        CHECK( site.next_routine_dispatch_eligible_minutes - now_minutes <= 18 * 60 );

        bandit_live_world::world_state resumed = round_trip_world( world );
        const std::string before_replay = serialize_world( resumed );
        const bandit_live_world::structural_bounty_maintenance_result replay =
            bandit_live_world::advance_structural_bounty_maintenance(
                resumed, now_minutes, 0, 2, terrain_lookup, threat_lookup );
        CHECK( replay.scheduler_replay_suppressed );
        CHECK( serialize_world( resumed ) == before_replay );

        now_minutes = resumed.sites.front().next_routine_dispatch_eligible_minutes;
        bandit_live_world::advance_structural_bounty_maintenance(
            resumed, now_minutes, 0, 2, terrain_lookup, threat_lookup );
        CHECK( resumed.sites.front().routine_no_candidate_streak == 2 );
        CHECK( resumed.sites.front().next_routine_dispatch_eligible_minutes - now_minutes >=
               24 * 60 );
        CHECK( resumed.sites.front().next_routine_dispatch_eligible_minutes - now_minutes <=
               30 * 60 );

        now_minutes = resumed.sites.front().next_routine_dispatch_eligible_minutes;
        bandit_live_world::advance_structural_bounty_maintenance(
            resumed, now_minutes, 0, 2, terrain_lookup, threat_lookup );
        CHECK( resumed.sites.front().routine_no_candidate_streak == 3 );
        CHECK( resumed.sites.front().next_routine_dispatch_eligible_minutes - now_minutes >=
               48 * 60 );
        CHECK( resumed.sites.front().next_routine_dispatch_eligible_minutes - now_minutes <=
               54 * 60 );

        bandit_live_world::world_state hard_gated;
        add_scheduler_test_site( hard_gated, 0, false, 390000 );
        bandit_live_world::site_record &hard_gated_site = hard_gated.sites.front();
        REQUIRE( bandit_live_world::upsert_structural_bounty_lead(
                     hard_gated_site, unreachable, read, 0 ) );
        hard_gated_site.members[0].wounded_or_unready = true;
        hard_gated_site.members[1].wounded_or_unready = true;
        bandit_live_world::advance_structural_bounty_maintenance(
            hard_gated, 0, 0, 2, terrain_lookup, threat_lookup );
        CHECK( hard_gated_site.routine_no_candidate_streak == 0 );
        CHECK( hard_gated_site.next_routine_dispatch_eligible_minutes == -1 );
    }

    SECTION( "brand new camps take structural work first and due camps send exact pairs" ) {
        for( const bool cannibal : { false, true } ) {
            INFO( "profile=" << ( cannibal ? "cannibal" : "bandit" ) );
            bandit_live_world::world_state structural_world;
            add_scheduler_test_site( structural_world, 0, cannibal, cannibal ? 410000 : 400000 );
            bandit_live_world::site_record &structural_site = structural_world.sites.front();
            const tripoint_abs_omt forest( structural_site.anchor.x() - 4,
                                           structural_site.anchor.y(), structural_site.anchor.z() );
            const bandit_live_world::structural_bounty_maintenance_result structural =
                bandit_live_world::advance_structural_bounty_maintenance(
                    structural_world, 0, 4, 2,
            [&forest]( const tripoint_abs_omt & omt ) -> std::optional<std::string> {
                return omt == forest ? std::optional<std::string>( "forest" ) : std::nullopt;
            }, []( const bandit_live_world::site_record &,
                   const bandit_live_world::camp_map_lead & ) {
                return bandit_live_world::structural_threat_read{};
            } );
            REQUIRE( structural.dispatches_applied == 1 );
            CHECK( structural_site.routine_activated_minutes == 0 );
            CHECK( structural_site.active_outing.target_omt == forest );
            const bandit_live_world::camp_map_lead *structural_lead =
                structural_site.intelligence_map.find_lead( structural_site.active_outing.target_id );
            REQUIRE( structural_lead != nullptr );
            CHECK( structural_lead->kind == bandit_live_world::camp_lead_kind::structural_bounty );

            bandit_live_world::world_state due_world;
            add_scheduler_test_site( due_world, 0, cannibal, cannibal ? 430000 : 420000 );
            due_world.sites.front().routine_activated_minutes = 0;
            const bandit_live_world::structural_bounty_maintenance_result due =
                bandit_live_world::advance_structural_bounty_maintenance(
                    due_world, 18 * 60, 0, 2,
            []( const tripoint_abs_omt & ) -> std::optional<std::string> {
                return std::nullopt;
            }, []( const bandit_live_world::site_record &,
                   const bandit_live_world::camp_map_lead & ) {
                return bandit_live_world::structural_threat_read{};
            } );
            REQUIRE( due.dispatches_applied == 1 );
            const bandit_live_world::site_record &due_site = due_world.sites.front();
            CHECK( due_site.active_outing.member_ids.size() == 2 );
            CHECK( due_site.active_outing.target_id == "frontier_probe:0" );
            const bandit_live_world::camp_map_lead *frontier =
                due_site.intelligence_map.find_lead( due_site.active_outing.target_id );
            REQUIRE( frontier != nullptr );
            CHECK( frontier->kind == bandit_live_world::camp_lead_kind::frontier_probe );
            CHECK( frontier->status == bandit_live_world::camp_lead_status::active );
        }
    }
}

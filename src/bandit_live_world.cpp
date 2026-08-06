#include "bandit_live_world.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "bandit_live_world_probe.h"
#include "game_constants.h"
#include "json.h"

namespace
{
using bandit_live_world::anchor_source_kind;
using bandit_live_world::camp_decision_state;
using bandit_live_world::camp_lead_kind;
using bandit_live_world::camp_lead_origin;
using bandit_live_world::camp_lead_status;
using bandit_live_world::camp_intelligence_map;
using bandit_live_world::camp_map_lead;
using bandit_live_world::camp_report_policy;
using bandit_live_world::hostile_site_profile;
using bandit_live_world::hostile_operation_kind;
using bandit_live_world::hostile_operation_phase;
using bandit_live_world::member_state;
using bandit_live_world::owned_site_kind;
using bandit_live_world::origin_disposition;
using bandit_live_world::outing_kind;
using bandit_live_world::scout_phase;
using bandit_live_world::simulation_owner;
using bandit_live_world::site_record;
using bandit_live_world::sortie_observation_kind;
using bandit_live_world::sortie_observation_sense;
using bandit_live_world::sortie_observation_share_state;
using bandit_live_world::structural_signal_read;
using bandit_live_world::structural_watch_kind;
using bandit_live_world::target_footprint_watch_distance;

constexpr std::size_t max_active_outing_members = 16;
constexpr std::size_t max_active_outing_route_steps = 256;
constexpr std::size_t max_hostile_operation_members = 6;
constexpr std::size_t max_active_outing_observations = 16;
constexpr std::size_t max_sortie_observation_batch = 64;
constexpr std::size_t max_active_outing_casualties = 16;
constexpr std::size_t max_structural_target_footprint_omts = 64;
constexpr std::size_t max_structural_watch_candidates = 256;
constexpr std::size_t max_covert_burn_egress_candidates = 8;
constexpr int max_covert_egress_attempts = 3;
constexpr int current_covert_egress_chain_version = 1;
constexpr std::size_t max_covert_egress_route_omts = 64;
constexpr std::size_t max_failed_covert_egress_route_omts =
    max_covert_egress_attempts * max_covert_egress_route_omts;
constexpr std::size_t max_structural_watch_exact_terrain_reads = 128;
constexpr std::size_t max_structural_watch_distance_four_terrain_reads = 64;
constexpr std::size_t max_structural_watch_distance_five_terrain_reads = 64;
constexpr int max_structural_watch_exact_route_reads = 4;
constexpr int max_structural_watch_distance_four_route_reads = 2;
constexpr int max_structural_watch_distance_five_route_reads = 2;
constexpr std::size_t max_abstract_threat_ids = 16;
constexpr std::size_t max_abstract_threat_id_length = 128;
constexpr std::size_t max_structural_signal_reads = 4;
constexpr std::size_t max_abstract_encounter_outcome_length = 128;
constexpr int abstract_wound_recovery_minutes = 72 * 60;
constexpr std::size_t max_camp_intelligence_leads = 64;
constexpr std::size_t max_acted_report_summaries = 64;
constexpr std::size_t max_live_signal_marks = 8;
constexpr std::size_t max_camp_lead_id_length = 192;
constexpr std::size_t max_camp_lead_target_id_length = 192;
constexpr std::size_t max_camp_lead_source_key_length = 192;
constexpr std::size_t max_camp_lead_summary_length = 256;
constexpr std::size_t max_camp_lead_outcome_length = 128;
constexpr std::size_t max_live_signal_mark_length = 192;
constexpr std::size_t max_sortie_fact_key_length = 128;
constexpr std::size_t max_sortie_source_id_length = 128;
constexpr std::size_t max_sortie_defender_id_length = 128;
constexpr std::size_t max_sortie_state_key_length = 128;
constexpr std::size_t max_sortie_summary_length = 512;
constexpr std::size_t max_camp_decision_reason_length = 256;
constexpr std::size_t max_operation_application_key_length = 256;
constexpr int max_finite_resource_units = 3;
constexpr int max_finite_resource_claim_units = 2;
constexpr int camp_supply_days_at_capacity = 14;
constexpr int legacy_camp_supply_seed_days = 7;
constexpr int max_camp_supply_units = 256;
constexpr int minutes_per_member_day = 24 * 60;
constexpr int scout_return_cohesion_minutes = 2 * 60;
constexpr int scout_missing_grace_minutes = 24 * 60;
constexpr int max_structural_route_cost_omt = 18;
constexpr int local_pair_cohesion_radius_ms = 6;
constexpr int local_pair_rendezvous_minutes = 10;
constexpr int local_pair_reroute_cap = 2;
constexpr int frontier_sector_count = 8;
constexpr int frontier_inner_radius_omt = 4;
constexpr int frontier_outer_radius_omt = 9;
constexpr int routine_scheduler_consider_cap = 16;
constexpr int routine_scheduler_urgent_signal_cap = 8;
constexpr int routine_scheduler_full_route_solve_cap = 8;
constexpr int routine_scheduler_start_cap = 2;
constexpr int routine_frontier_recurrence_minutes = 72 * 60;
constexpr int routine_candidate_full_route_solve_cap = 2;
constexpr int routine_remembered_ground_candidate_cap = 6;
constexpr int routine_acquire_score = 300;
constexpr int routine_retain_score = 150;
constexpr int routine_hard_risk = 750;
constexpr std::array<std::pair<int, int>, frontier_sector_count> frontier_directions = { {
        { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
        { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 },
    } };

void bound_camp_map_lead_strings( camp_map_lead &lead )
{
    lead.lead_id.resize( std::min( lead.lead_id.size(), max_camp_lead_id_length ) );
    lead.target_id.resize( std::min( lead.target_id.size(), max_camp_lead_target_id_length ) );
    lead.source_key.resize( std::min( lead.source_key.size(), max_camp_lead_source_key_length ) );
    lead.source_summary.resize( std::min( lead.source_summary.size(),
                                         max_camp_lead_summary_length ) );
    lead.last_outcome.resize( std::min( lead.last_outcome.size(), max_camp_lead_outcome_length ) );
}

int minutes_after_saturated( const int base_minutes, const int delta_minutes )
{
    if( base_minutes < 0 || delta_minutes < 0 ) {
        return -1;
    }
    const long long result = static_cast<long long>( base_minutes ) + delta_minutes;
    return static_cast<int>( std::min<long long>( result, std::numeric_limits<int>::max() ) );
}

int omt_chebyshev_distance( const tripoint_abs_omt &from, const tripoint_abs_omt &to )
{
    if( from.z() != to.z() ) {
        return std::numeric_limits<int>::max();
    }
    const long long x_distance = std::abs( static_cast<long long>( to.x() ) - from.x() );
    const long long y_distance = std::abs( static_cast<long long>( to.y() ) - from.y() );
    return static_cast<int>( std::min<long long>( std::max( x_distance, y_distance ),
                             std::numeric_limits<int>::max() ) );
}

std::vector<tripoint_abs_omt> make_structural_radial_route(
    const tripoint_abs_omt &anchor, const tripoint_abs_omt &target )
{
    const int distance = omt_chebyshev_distance( anchor, target );
    if( distance <= 0 || distance > max_structural_route_cost_omt / 2 ) {
        return {};
    }

    std::vector<tripoint_abs_omt> route;
    route.push_back( anchor );
    if( distance > 1 ) {
        const int x_step = target.x() == anchor.x() ? 0 : target.x() > anchor.x() ? 1 : -1;
        const int y_step = target.y() == anchor.y() ? 0 : target.y() > anchor.y() ? 1 : -1;
        route.emplace_back( target.x() - x_step, target.y() - y_step, target.z() );
    }
    route.push_back( target );
    route.push_back( anchor );
    return route;
}

int structural_route_cost( const std::vector<tripoint_abs_omt> &route )
{
    int cost = 0;
    for( std::size_t index = 1; index < route.size(); ++index ) {
        const int segment = omt_chebyshev_distance( route[index - 1], route[index] );
        if( segment <= 0 || segment == std::numeric_limits<int>::max() ||
            cost > max_structural_route_cost_omt - segment ) {
            return std::numeric_limits<int>::max();
        }
        cost += segment;
    }
    return cost;
}

std::string frontier_probe_target_id( const int sector )
{
    return "frontier_sector_" + std::to_string( sector );
}

std::string frontier_probe_lead_id( const int sector )
{
    return "frontier_probe:" + std::to_string( sector );
}

std::optional<int> frontier_sector_from_lead( const camp_map_lead &lead )
{
    if( lead.kind != camp_lead_kind::frontier_probe ) {
        return std::nullopt;
    }
    for( int sector = 0; sector < frontier_sector_count; ++sector ) {
        const std::string expected = frontier_probe_target_id( sector );
        if( lead.lead_id == frontier_probe_lead_id( sector ) &&
            lead.target_id == expected && lead.source_key == expected ) {
            return sector;
        }
    }
    return std::nullopt;
}

std::optional<tripoint_abs_omt> frontier_outer_target( const tripoint_abs_omt &anchor,
        const int sector )
{
    if( sector < 0 || sector >= frontier_sector_count ) {
        return std::nullopt;
    }
    const std::pair<int, int> direction = frontier_directions[static_cast<std::size_t>( sector )];
    const long long outer_x = static_cast<long long>( anchor.x() ) +
                              direction.first * frontier_outer_radius_omt;
    const long long outer_y = static_cast<long long>( anchor.y() ) +
                              direction.second * frontier_outer_radius_omt;
    if( outer_x < std::numeric_limits<int>::min() ||
        outer_x > std::numeric_limits<int>::max() ||
        outer_y < std::numeric_limits<int>::min() ||
        outer_y > std::numeric_limits<int>::max() ) {
        return std::nullopt;
    }
    return tripoint_abs_omt( static_cast<int>( outer_x ), static_cast<int>( outer_y ), anchor.z() );
}

std::vector<tripoint_abs_omt> make_frontier_radial_route(
    const tripoint_abs_omt &anchor, const int sector )
{
    if( sector < 0 || sector >= frontier_sector_count ) {
        return {};
    }
    const std::pair<int, int> direction = frontier_directions[static_cast<std::size_t>( sector )];
    const long long inner_x = static_cast<long long>( anchor.x() ) +
                              direction.first * frontier_inner_radius_omt;
    const long long inner_y = static_cast<long long>( anchor.y() ) +
                              direction.second * frontier_inner_radius_omt;
    const long long outer_x = static_cast<long long>( anchor.x() ) +
                              direction.first * frontier_outer_radius_omt;
    const long long outer_y = static_cast<long long>( anchor.y() ) +
                              direction.second * frontier_outer_radius_omt;
    if( inner_x < std::numeric_limits<int>::min() ||
        inner_x > std::numeric_limits<int>::max() ||
        inner_y < std::numeric_limits<int>::min() ||
        inner_y > std::numeric_limits<int>::max() ||
        outer_x < std::numeric_limits<int>::min() ||
        outer_x > std::numeric_limits<int>::max() ||
        outer_y < std::numeric_limits<int>::min() ||
        outer_y > std::numeric_limits<int>::max() ) {
        return {};
    }
    return {
        anchor,
        tripoint_abs_omt( static_cast<int>( inner_x ), static_cast<int>( inner_y ), anchor.z() ),
        tripoint_abs_omt( static_cast<int>( outer_x ), static_cast<int>( outer_y ), anchor.z() ),
        anchor,
    };
}

bool frontier_route_is_canonical( const std::vector<tripoint_abs_omt> &route,
                                  const tripoint_abs_omt &anchor, const int sector )
{
    return route == make_frontier_radial_route( anchor, sector ) && route.size() == 4 &&
           omt_chebyshev_distance( anchor, route[1] ) >= 4 &&
           omt_chebyshev_distance( anchor, route[1] ) <= 6 &&
           omt_chebyshev_distance( anchor, route[2] ) >= 7 &&
           omt_chebyshev_distance( anchor, route[2] ) <= 9 &&
           structural_route_cost( route ) <= max_structural_route_cost_omt;
}

bool frontier_memory_is_valid( const camp_intelligence_map &intelligence )
{
    return intelligence.frontier_sector_cursor >= 0 &&
           intelligence.frontier_sector_cursor < frontier_sector_count &&
           intelligence.frontier_last_resolved_minutes.size() == frontier_sector_count &&
           std::all_of( intelligence.frontier_last_resolved_minutes.begin(),
                        intelligence.frontier_last_resolved_minutes.end(),
    []( const int resolved_minutes ) {
        return resolved_minutes >= -1;
    } );
}

unsigned int stable_string_hash( const std::string &value )
{
    unsigned int hash = 2166136261U;
    for( const unsigned char character : value ) {
        hash ^= character;
        hash *= 16777619U;
    }
    return hash;
}

int first_frontier_due_delay_minutes( const std::string &site_id )
{
    return ( 6 + static_cast<int>( stable_string_hash( site_id ) % 13U ) ) * 60;
}

int routine_cooldown_delay_minutes( const std::string &site_id, const int base_minutes )
{
    const unsigned int jitter_hash = stable_string_hash( site_id + ":routine_cooldown" );
    return base_minutes + static_cast<int>( jitter_hash % 7U ) * 60;
}

int routine_no_candidate_base_delay_minutes( const int streak )
{
    if( streak <= 1 ) {
        return 12 * 60;
    }
    if( streak == 2 ) {
        return 24 * 60;
    }
    return 48 * 60;
}

bool frontier_dispatch_is_due( const site_record &site, const int now_minutes )
{
    if( now_minutes < 0 || site.routine_activated_minutes < 0 ||
        !frontier_memory_is_valid( site.intelligence_map ) ) {
        return false;
    }
    const int latest_frontier_resolution = *std::max_element(
            site.intelligence_map.frontier_last_resolved_minutes.begin(),
            site.intelligence_map.frontier_last_resolved_minutes.end() );
    if( latest_frontier_resolution < 0 ) {
        return now_minutes >= minutes_after_saturated(
                   site.routine_activated_minutes,
                   first_frontier_due_delay_minutes( site.site_id ) );
    }
    return now_minutes >= minutes_after_saturated(
               latest_frontier_resolution, routine_frontier_recurrence_minutes );
}

int routine_dispatch_wait_minutes( const site_record &site, const int now_minutes,
                                   const bool frontier_due )
{
    int eligible_since = site.routine_activated_minutes;
    if( frontier_due ) {
        const int latest_frontier_resolution = *std::max_element(
                site.intelligence_map.frontier_last_resolved_minutes.begin(),
                site.intelligence_map.frontier_last_resolved_minutes.end() );
        eligible_since = latest_frontier_resolution < 0 ? minutes_after_saturated(
                             site.routine_activated_minutes,
                             first_frontier_due_delay_minutes( site.site_id ) ) :
                         minutes_after_saturated( latest_frontier_resolution,
                                 routine_frontier_recurrence_minutes );
    } else if( site.next_routine_dispatch_eligible_minutes >= 0 ) {
        eligible_since = site.next_routine_dispatch_eligible_minutes;
    } else if( site.last_routine_resolved_minutes >= 0 ) {
        eligible_since = site.last_routine_resolved_minutes;
    }
    if( eligible_since < 0 || now_minutes <= eligible_since ) {
        return 0;
    }
    return now_minutes - eligible_since;
}

std::vector<int> least_recent_frontier_sectors( const camp_intelligence_map &intelligence )
{
    if( !frontier_memory_is_valid( intelligence ) ) {
        return {};
    }
    std::vector<int> sectors;
    sectors.reserve( frontier_sector_count );
    for( int offset = 0; offset < frontier_sector_count; ++offset ) {
        sectors.push_back( ( intelligence.frontier_sector_cursor + offset ) % frontier_sector_count );
    }
    std::stable_sort( sectors.begin(), sectors.end(), [&intelligence]( const int lhs, const int rhs ) {
        return intelligence.frontier_last_resolved_minutes[static_cast<std::size_t>( lhs )] <
               intelligence.frontier_last_resolved_minutes[static_cast<std::size_t>( rhs )];
    } );
    return sectors;
}

bool structural_route_is_canonical( const std::vector<tripoint_abs_omt> &route,
                                    const tripoint_abs_omt &anchor,
                                    const tripoint_abs_omt &target )
{
    return route == make_structural_radial_route( anchor, target ) &&
           route.size() >= 3 && route.size() <= 5 &&
           structural_route_cost( route ) <= max_structural_route_cost_omt;
}

bool returned_structural_signal_lead( const camp_map_lead &lead )
{
    const bool signal_kind = lead.kind == camp_lead_kind::smoke_signal ||
                             lead.kind == camp_lead_kind::light_signal ||
                             lead.kind == camp_lead_kind::sound_signal;
    return signal_kind && lead.origin == camp_lead_origin::returned_report &&
           lead.generated_by_this_camp_routine;
}

bool structural_route_is_canonical_for_lead( const std::vector<tripoint_abs_omt> &route,
        const tripoint_abs_omt &anchor, const camp_map_lead &lead )
{
    const std::optional<int> frontier_sector = frontier_sector_from_lead( lead );
    if( frontier_sector ) {
        return route.size() >= 2 && lead.omt == route[route.size() - 2] &&
               frontier_route_is_canonical( route, anchor, *frontier_sector );
    }
    return ( lead.kind == camp_lead_kind::structural_bounty ||
             lead.kind == camp_lead_kind::terrain_opportunity ||
             returned_structural_signal_lead( lead ) ) &&
           structural_route_is_canonical( route, anchor, lead.omt );
}

bool structural_outing_uses_watch_route(
    const bandit_live_world::active_outing_state &outing )
{
    return outing.kind == outing_kind::structural_sortie && outing.schema_version >= 10 &&
           outing.selected_watch_kind != structural_watch_kind::none;
}

int structural_outing_destination_waypoint(
    const bandit_live_world::active_outing_state &outing )
{
    if( outing.shared_route.size() < 3 ) {
        return -1;
    }
    return structural_outing_uses_watch_route( outing ) ? 2 :
           static_cast<int>( outing.shared_route.size() ) - 2;
}

tripoint_abs_omt structural_outing_travel_destination(
    const bandit_live_world::active_outing_state &outing )
{
    return structural_outing_uses_watch_route( outing ) ?
           outing.selected_watch_omt : outing.target_omt;
}

bool structural_route_is_canonical_for_outing(
        const bandit_live_world::active_outing_state &outing,
        const tripoint_abs_omt &anchor, const camp_map_lead &lead )
{
    if( !structural_outing_uses_watch_route( outing ) ) {
        return structural_route_is_canonical_for_lead( outing.shared_route, anchor, lead );
    }
    return !frontier_sector_from_lead( lead ) &&
           bandit_live_world::structural_watch_shared_route_is_canonical(
               outing.shared_route, anchor, outing.selected_watch_omt,
               outing.target_footprint );
}

int structural_stalking_delay_minutes( const tripoint_abs_omt &anchor,
                                       const tripoint_abs_omt &target )
{
    const int distance = std::max( 1, omt_chebyshev_distance( anchor, target ) );
    return std::clamp( distance * 15, 30, 240 );
}

int structural_arrival_delay_minutes( const tripoint_abs_omt &anchor,
                                      const tripoint_abs_omt &target )
{
    const int distance = std::max( 1, omt_chebyshev_distance( anchor, target ) );
    return structural_stalking_delay_minutes( anchor, target ) +
           std::clamp( distance * 10, 30, 180 );
}

int structural_return_delay_minutes( const tripoint_abs_omt &anchor,
                                     const tripoint_abs_omt &target )
{
    const int distance = std::max( 1, omt_chebyshev_distance( anchor, target ) );
    return structural_arrival_delay_minutes( anchor, target ) +
           std::clamp( distance * 10, 30, 180 );
}

int structural_expected_return_minutes( const int started_minutes,
                                        const tripoint_abs_omt &anchor,
                                        const tripoint_abs_omt &target )
{
    return minutes_after_saturated( started_minutes,
                                    structural_return_delay_minutes( anchor, target ) );
}

bool finite_resource_record_is_valid( const bandit_live_world::finite_resource_record &record )
{
    return record.remaining_units >= 0 && record.remaining_units < max_finite_resource_units &&
           record.revision > 0 && record.revision <= max_finite_resource_units &&
           record.remaining_units + record.revision <= max_finite_resource_units;
}

bool resource_receipt_key_matches( const std::string &site_id, const int generation,
                                   const std::string &application_key )
{
    const std::string site_prefix = site_id + "#";
    const std::string component_marker = ":resource:";
    const std::string generation_suffix = ":" + std::to_string( generation );
    const std::size_t component_at = application_key.find( component_marker );
    return !site_id.empty() && application_key.rfind( site_prefix, 0 ) == 0 &&
           application_key.size() >= generation_suffix.size() &&
           component_at != std::string::npos && component_at >= site_prefix.size() &&
           component_at + component_marker.size() <
           application_key.size() - generation_suffix.size() &&
           application_key.compare( application_key.size() - generation_suffix.size(),
                                    generation_suffix.size(), generation_suffix ) == 0;
}

void seed_camp_supply( bandit_live_world::site_record &site )
{
    const int living_total = bandit_live_world::camp_supply_living_total( site );
    const long long seeded_units = static_cast<long long>( legacy_camp_supply_seed_days ) *
                                   living_total;
    site.supply_units = static_cast<int>( std::min<long long>(
                            bandit_live_world::camp_supply_cap( site ), seeded_units ) );
    site.supply_last_update_minutes = -1;
    site.supply_accounted_living_total = living_total;
    site.supply_member_minute_remainder = 0;
}

void seed_uninitialized_camp_supply( bandit_live_world::site_record &site )
{
    if( site.supply_last_update_minutes != -1 ||
        site.supply_member_minute_remainder != 0 ) {
        return;
    }
    const int prior_living_total = std::max( 0, site.supply_accounted_living_total );
    const int prior_cap = static_cast<int>( std::min<long long>( max_camp_supply_units,
                          static_cast<long long>( camp_supply_days_at_capacity ) *
                          std::max( 1, prior_living_total ) ) );
    const int prior_seed = static_cast<int>( std::min<long long>( prior_cap,
                           static_cast<long long>( legacy_camp_supply_seed_days ) *
                           prior_living_total ) );
    if( site.supply_units == prior_seed ) {
        seed_camp_supply( site );
    }
}

struct bounded_route_state {
    std::vector<tripoint_abs_omt> route;
    int waypoint_index = 0;
};

bounded_route_state make_bounded_route_state( const std::vector<tripoint_abs_omt> &route,
        const int waypoint_index )
{
    bounded_route_state result;
    const std::size_t clamped_waypoint = static_cast<std::size_t>( std::clamp( waypoint_index, 0,
                                           static_cast<int>( route.size() ) ) );
    if( route.size() <= max_active_outing_route_steps ) {
        result.route = route;
        result.waypoint_index = static_cast<int>( clamped_waypoint );
        return result;
    }

    if( clamped_waypoint >= route.size() ) {
        const auto first = route.end() - max_active_outing_route_steps;
        result.route.assign( first, route.end() );
        result.waypoint_index = static_cast<int>( result.route.size() );
        return result;
    }

    const auto first = route.begin() + clamped_waypoint;
    const auto last = first + std::min( max_active_outing_route_steps,
                                       route.size() - clamped_waypoint );
    result.route.assign( first, last );
    if( result.route.size() == max_active_outing_route_steps &&
        result.route.back() != route.back() ) {
        result.route.back() = route.back();
    }
    result.waypoint_index = 0;
    return result;
}

int sortie_observation_retention_rank( const bandit_live_world::sortie_observation &observation )
{
    switch( observation.kind ) {
        case sortie_observation_kind::routine:
            return observation.critical ? 2 : 0;
        case sortie_observation_kind::certainty:
        case sortie_observation_kind::bounds:
        case sortie_observation_kind::route_state:
        case sortie_observation_kind::alert:
            return observation.critical ? 2 : 1;
        case sortie_observation_kind::target_revision:
        case sortie_observation_kind::hard_danger:
        case sortie_observation_kind::contradiction:
        case sortie_observation_kind::casualty:
        case sortie_observation_kind::burn:
            return 2;
    }
    return 0;
}

bool sortie_observation_counts_as_progress( const sortie_observation_kind kind )
{
    return kind != sortie_observation_kind::routine;
}

int sortie_observation_share_rank(
    const bandit_live_world::sortie_observation &observation )
{
    if( observation.record_schema_version != 1 ) {
        return 0;
    }
    switch( observation.share_state ) {
        case sortie_observation_share_state::observer_private:
            return 1;
        case sortie_observation_share_state::shared:
            return 2;
        case sortie_observation_share_state::reported:
            return 3;
    }
    return 0;
}

bool sortie_observation_identity_matches(
    const bandit_live_world::sortie_observation &lhs,
    const bandit_live_world::sortie_observation &rhs )
{
    if( lhs.record_schema_version == 1 && rhs.record_schema_version == 1 ) {
        return lhs.fact_key == rhs.fact_key &&
               lhs.bucket_start_minutes == rhs.bucket_start_minutes;
    }
    return lhs.record_schema_version == 0 && rhs.record_schema_version == 0 &&
           lhs.fact_key == rhs.fact_key;
}

bool typed_sortie_observation_is_valid(
    const bandit_live_world::sortie_observation &observation )
{
    if( observation.record_schema_version != 1 || observation.fact_key.empty() ||
        observation.fact_key.size() > max_sortie_fact_key_length ||
        observation.summary.size() > max_sortie_summary_length ||
        observation.confidence < 0 || observation.confidence > 100 ||
        observation.observed_minutes < 0 ||
        observation.state_key.size() > max_sortie_state_key_length ||
        observation.source_id.empty() ||
        observation.source_id.size() > max_sortie_source_id_length ||
        !observation.observer_id.is_valid() || observation.source_omt.is_invalid() ||
        observation.receiver_omt.is_invalid() ||
        observation.bucket_start_minutes !=
        observation.observed_minutes - observation.observed_minutes % 30 ||
        observation.strength < 0 || observation.strength > 6 ||
        observation.visual_quality < 0 || observation.visual_quality > 3 ||
        observation.defender_ids.size() > max_abstract_threat_ids ||
        !std::is_sorted( observation.defender_ids.begin(), observation.defender_ids.end() ) ||
        std::adjacent_find( observation.defender_ids.begin(), observation.defender_ids.end() ) !=
        observation.defender_ids.end() ||
        std::any_of( observation.defender_ids.begin(), observation.defender_ids.end(),
    []( const std::string & defender_id ) {
        return defender_id.empty() || defender_id.size() > max_sortie_defender_id_length;
    } ) || observation.simultaneity_start_minutes < observation.bucket_start_minutes ||
        observation.simultaneity_start_minutes > observation.observed_minutes ||
        observation.simultaneity_end_minutes < observation.observed_minutes ||
        observation.simultaneity_end_minutes - observation.bucket_start_minutes >= 30 ||
        observation.observed_power_low < 0 ||
        observation.observed_power_high < observation.observed_power_low ||
        observation.observed_power_high > 200 || observation.equipment_detail < 0 ||
        observation.equipment_detail > 3 || observation.target_revision <= 0 ||
        observation.uncertainty_radius_omt < 0 || observation.uncertainty_radius_omt > 40 ||
        observation.expiry_minutes < observation.observed_minutes ) {
        return false;
    }
    switch( observation.sense ) {
        case sortie_observation_sense::visual:
            break;
        case sortie_observation_sense::smoke:
        case sortie_observation_sense::light:
        case sortie_observation_sense::sound:
            if( observation.visual_quality != 0 ) {
                return false;
            }
            break;
        default:
            return false;
    }
    switch( observation.share_state ) {
        case sortie_observation_share_state::observer_private:
        case sortie_observation_share_state::shared:
        case sortie_observation_share_state::reported:
            return true;
    }
    return false;
}

bool sortie_observation_is_better_retention(
    const bandit_live_world::sortie_observation &lhs,
    const bandit_live_world::sortie_observation &rhs )
{
    const int lhs_rank = sortie_observation_retention_rank( lhs );
    const int rhs_rank = sortie_observation_retention_rank( rhs );
    const int lhs_share_rank = sortie_observation_share_rank( lhs );
    const int rhs_share_rank = sortie_observation_share_rank( rhs );
    return std::tie( lhs.record_schema_version, lhs_share_rank, lhs_rank,
                     lhs.observed_minutes, lhs.confidence, lhs.strength,
                     lhs.visual_quality, lhs.observed_power_high, lhs.equipment_detail,
                     lhs.state_key, lhs.kind, lhs.critical, lhs.summary ) >
           std::tie( rhs.record_schema_version, rhs_share_rank, rhs_rank,
                     rhs.observed_minutes, rhs.confidence, rhs.strength,
                     rhs.visual_quality, rhs.observed_power_high, rhs.equipment_detail,
                     rhs.state_key, rhs.kind, rhs.critical, rhs.summary );
}

bool sortie_observation_is_better_for_cap(
    const bandit_live_world::sortie_observation &lhs,
    const bandit_live_world::sortie_observation &rhs )
{
    const int lhs_rank = sortie_observation_retention_rank( lhs );
    const int rhs_rank = sortie_observation_retention_rank( rhs );
    const int lhs_share_rank = sortie_observation_share_rank( lhs );
    const int rhs_share_rank = sortie_observation_share_rank( rhs );
    return std::tie( lhs.record_schema_version, lhs_share_rank, lhs_rank,
                     lhs.observed_minutes, lhs.confidence, lhs.fact_key,
                     lhs.bucket_start_minutes, lhs.state_key, lhs.kind, lhs.critical,
                     lhs.summary ) >
           std::tie( rhs.record_schema_version, rhs_share_rank, rhs_rank,
                     rhs.observed_minutes, rhs.confidence, rhs.fact_key,
                     rhs.bucket_start_minutes, rhs.state_key, rhs.kind, rhs.critical,
                     rhs.summary );
}

void normalize_sortie_observation( bandit_live_world::sortie_observation &observation )
{
    if( observation.record_schema_version != 0 ) {
        return;
    }
    observation.fact_key.resize( std::min( observation.fact_key.size(),
                                           max_sortie_fact_key_length ) );
    observation.state_key.resize( std::min( observation.state_key.size(),
                                            max_sortie_state_key_length ) );
    observation.summary.resize( std::min( observation.summary.size(), max_sortie_summary_length ) );
    observation.confidence = std::clamp( observation.confidence, 0, 100 );
    observation.observed_minutes = std::max( -1, observation.observed_minutes );
}

bool sortie_observations_equal( const bandit_live_world::sortie_observation &lhs,
                                const bandit_live_world::sortie_observation &rhs )
{
    return lhs.fact_key == rhs.fact_key && lhs.summary == rhs.summary &&
           lhs.confidence == rhs.confidence && lhs.observed_minutes == rhs.observed_minutes &&
           lhs.critical == rhs.critical && lhs.kind == rhs.kind &&
           lhs.state_key == rhs.state_key &&
           lhs.record_schema_version == rhs.record_schema_version &&
           lhs.source_id == rhs.source_id && lhs.sense == rhs.sense &&
           lhs.observer_id == rhs.observer_id && lhs.source_omt == rhs.source_omt &&
           lhs.receiver_omt == rhs.receiver_omt &&
           lhs.bucket_start_minutes == rhs.bucket_start_minutes &&
           lhs.strength == rhs.strength && lhs.visual_quality == rhs.visual_quality &&
           lhs.defender_ids == rhs.defender_ids &&
           lhs.simultaneity_start_minutes == rhs.simultaneity_start_minutes &&
           lhs.simultaneity_end_minutes == rhs.simultaneity_end_minutes &&
           lhs.observed_power_low == rhs.observed_power_low &&
           lhs.observed_power_high == rhs.observed_power_high &&
           lhs.equipment_detail == rhs.equipment_detail &&
           lhs.target_revision == rhs.target_revision &&
           lhs.uncertainty_radius_omt == rhs.uncertainty_radius_omt &&
           lhs.expiry_minutes == rhs.expiry_minutes && lhs.share_state == rhs.share_state;
}

std::vector<bandit_live_world::sortie_observation> make_bounded_sortie_observations(
            const std::vector<bandit_live_world::sortie_observation> &observations )
{
    std::vector<bandit_live_world::sortie_observation> deduplicated;
    deduplicated.reserve( std::min( observations.size(), max_active_outing_observations ) );
    for( bandit_live_world::sortie_observation observation : observations ) {
        normalize_sortie_observation( observation );
        if( observation.fact_key.empty() ||
            ( observation.record_schema_version == 1 &&
              !typed_sortie_observation_is_valid( observation ) ) ||
            ( observation.record_schema_version != 0 &&
              observation.record_schema_version != 1 ) ) {
            continue;
        }
        const auto existing = std::find_if( deduplicated.begin(), deduplicated.end(),
        [&observation]( const bandit_live_world::sortie_observation & retained ) {
            return sortie_observation_identity_matches( retained, observation );
        } );
        if( existing == deduplicated.end() ) {
            deduplicated.push_back( std::move( observation ) );
        } else if( observation.record_schema_version == 1 ) {
            if( sortie_observation_is_better_retention( observation, *existing ) ) {
                *existing = std::move( observation );
            }
        } else if( existing->kind == observation.kind &&
                   existing->state_key == observation.state_key ) {
            const int existing_minutes = existing->observed_minutes;
            const int observation_minutes = observation.observed_minutes;
            if( observation.confidence > existing->confidence ||
                ( observation.confidence == existing->confidence &&
                  observation.summary > existing->summary ) ) {
                existing->confidence = observation.confidence;
                existing->summary = observation.summary;
            }
            existing->critical = existing->critical || observation.critical;
            if( existing_minutes < 0 ) {
                existing->observed_minutes = observation_minutes;
            } else if( observation_minutes >= 0 ) {
                existing->observed_minutes = std::min( existing_minutes, observation_minutes );
            }
        } else if( sortie_observation_is_better_retention( observation, *existing ) ) {
            *existing = std::move( observation );
        }
    }

    std::sort( deduplicated.begin(), deduplicated.end(), sortie_observation_is_better_for_cap );
    if( deduplicated.size() > max_active_outing_observations ) {
        deduplicated.resize( max_active_outing_observations );
    }
    std::sort( deduplicated.begin(), deduplicated.end(),
    []( const bandit_live_world::sortie_observation & lhs,
    const bandit_live_world::sortie_observation & rhs ) {
        return std::tie( lhs.observed_minutes, lhs.fact_key, lhs.state_key, lhs.kind, lhs.summary,
                         lhs.confidence, lhs.critical, lhs.record_schema_version,
                         lhs.bucket_start_minutes ) <
               std::tie( rhs.observed_minutes, rhs.fact_key, rhs.state_key, rhs.kind, rhs.summary,
                         rhs.confidence, rhs.critical, rhs.record_schema_version,
                         rhs.bucket_start_minutes );
    } );
    return deduplicated;
}

std::vector<bandit_live_world::sortie_observation> make_reportable_sortie_observations(
            const std::vector<bandit_live_world::sortie_observation> &observations,
            const std::vector<character_id> &carrier_ids )
{
    std::vector<bandit_live_world::sortie_observation> reportable;
    for( bandit_live_world::sortie_observation observation :
         make_bounded_sortie_observations( observations ) ) {
        if( observation.record_schema_version == 1 ) {
            const bool observer_is_carrier =
                std::find( carrier_ids.begin(), carrier_ids.end(), observation.observer_id ) !=
                carrier_ids.end();
            if( carrier_ids.empty() ||
                ( observation.share_state == sortie_observation_share_state::observer_private &&
                  !observer_is_carrier ) ) {
                continue;
            }
            observation.share_state = sortie_observation_share_state::reported;
        }
        reportable.push_back( std::move( observation ) );
    }
    return reportable;
}

std::optional<anchor_source_kind> anchor_source_kind_from_string( const std::string &value )
{
    if( value == "overmap_special" ) {
        return anchor_source_kind::overmap_special;
    }
    if( value == "map_extra" ) {
        return anchor_source_kind::map_extra;
    }
    if( value == "none" ) {
        return anchor_source_kind::none;
    }
    return std::nullopt;
}

std::optional<owned_site_kind> owned_site_kind_from_string( const std::string &value )
{
    if( value == "bandit_camp" ) {
        return owned_site_kind::bandit_camp;
    }
    if( value == "bandit_work_camp" ) {
        return owned_site_kind::bandit_work_camp;
    }
    if( value == "bandit_cabin" ) {
        return owned_site_kind::bandit_cabin;
    }
    if( value == "cannibal_camp" ) {
        return owned_site_kind::cannibal_camp;
    }
    if( value == "looters" ) {
        return owned_site_kind::looters;
    }
    if( value == "bandits_block" ) {
        return owned_site_kind::bandits_block;
    }
    if( value == "none" ) {
        return owned_site_kind::none;
    }
    return std::nullopt;
}

std::optional<hostile_site_profile> hostile_site_profile_from_string( const std::string &value )
{
    if( value == "camp_style" ) {
        return hostile_site_profile::camp_style;
    }
    if( value == "cannibal_camp" ) {
        return hostile_site_profile::cannibal_camp;
    }
    if( value == "small_hostile_site" ) {
        return hostile_site_profile::small_hostile_site;
    }
    if( value == "none" ) {
        return hostile_site_profile::none;
    }
    return std::nullopt;
}

std::optional<member_state> member_state_from_string( const std::string &value )
{
    if( value == "at_home" ) {
        return member_state::at_home;
    }
    if( value == "outbound" ) {
        return member_state::outbound;
    }
    if( value == "local_contact" ) {
        return member_state::local_contact;
    }
    if( value == "orphaned" ) {
        return member_state::orphaned;
    }
    if( value == "dead" ) {
        return member_state::dead;
    }
    if( value == "missing" ) {
        return member_state::missing;
    }
    return std::nullopt;
}

std::optional<origin_disposition> origin_disposition_from_string( const std::string &value )
{
    if( value == "active_hostile" ) {
        return origin_disposition::active_hostile;
    }
    if( value == "captured_non_hostile" ) {
        return origin_disposition::captured_non_hostile;
    }
    if( value == "deleted" ) {
        return origin_disposition::deleted;
    }
    if( value == "invalidated" ) {
        return origin_disposition::invalidated;
    }
    return std::nullopt;
}

bool terminal_origin_disposition( const origin_disposition disposition )
{
    return disposition != origin_disposition::active_hostile;
}

std::optional<camp_lead_kind> camp_lead_kind_from_string( const std::string &value )
{
    if( value == "structural_bounty" ) {
        return camp_lead_kind::structural_bounty;
    }
    if( value == "terrain_opportunity" ) {
        return camp_lead_kind::terrain_opportunity;
    }
    if( value == "harvested_site" ) {
        return camp_lead_kind::harvested_site;
    }
    if( value == "human_activity" ) {
        return camp_lead_kind::human_activity;
    }
    if( value == "basecamp_activity" ) {
        return camp_lead_kind::basecamp_activity;
    }
    if( value == "moving_actor" ) {
        return camp_lead_kind::moving_actor;
    }
    if( value == "route_activity" ) {
        return camp_lead_kind::route_activity;
    }
    if( value == "smoke_signal" ) {
        return camp_lead_kind::smoke_signal;
    }
    if( value == "light_signal" ) {
        return camp_lead_kind::light_signal;
    }
    if( value == "sound_signal" ) {
        return camp_lead_kind::sound_signal;
    }
    if( value == "threat_memory" ) {
        return camp_lead_kind::threat_memory;
    }
    if( value == "loss_site" ) {
        return camp_lead_kind::loss_site;
    }
    if( value == "false_lead" ) {
        return camp_lead_kind::false_lead;
    }
    if( value == "frontier_probe" ) {
        return camp_lead_kind::frontier_probe;
    }
    return std::nullopt;
}

std::optional<camp_lead_origin> camp_lead_origin_from_string( const std::string &value )
{
    if( value == "legacy_radar" ) {
        return camp_lead_origin::legacy_radar;
    }
    if( value == "observer" ) {
        return camp_lead_origin::observer;
    }
    if( value == "signal" ) {
        return camp_lead_origin::signal;
    }
    if( value == "returned_report" ) {
        return camp_lead_origin::returned_report;
    }
    if( value == "structural_routine" ) {
        return camp_lead_origin::structural_routine;
    }
    return std::nullopt;
}

camp_lead_origin infer_legacy_camp_lead_origin( const camp_map_lead &lead )
{
    switch( lead.kind ) {
        case camp_lead_kind::structural_bounty:
        case camp_lead_kind::terrain_opportunity:
        case camp_lead_kind::frontier_probe:
            return camp_lead_origin::structural_routine;
        case camp_lead_kind::smoke_signal:
        case camp_lead_kind::light_signal:
        case camp_lead_kind::sound_signal:
            return camp_lead_origin::signal;
        case camp_lead_kind::basecamp_activity:
            return camp_lead_origin::returned_report;
        case camp_lead_kind::harvested_site:
        case camp_lead_kind::human_activity:
        case camp_lead_kind::moving_actor:
        case camp_lead_kind::route_activity:
        case camp_lead_kind::threat_memory:
        case camp_lead_kind::loss_site:
        case camp_lead_kind::false_lead:
            return camp_lead_origin::legacy_radar;
    }
    return camp_lead_origin::legacy_radar;
}

std::optional<camp_lead_status> camp_lead_status_from_string( const std::string &value )
{
    if( value == "suspected" ) {
        return camp_lead_status::suspected;
    }
    if( value == "scout_confirmed" ) {
        return camp_lead_status::scout_confirmed;
    }
    if( value == "active" ) {
        return camp_lead_status::active;
    }
    if( value == "harvested" ) {
        return camp_lead_status::harvested;
    }
    if( value == "stale" ) {
        return camp_lead_status::stale;
    }
    if( value == "invalidated" ) {
        return camp_lead_status::invalidated;
    }
    if( value == "dangerous" ) {
        return camp_lead_status::dangerous;
    }
    return std::nullopt;
}

std::optional<outing_kind> outing_kind_from_string( const std::string &value )
{
    if( value == "scout_sortie" ) {
        return outing_kind::scout_sortie;
    }
    if( value == "hostile_operation" ) {
        return outing_kind::hostile_operation;
    }
    if( value == "structural_sortie" ) {
        return outing_kind::structural_sortie;
    }
    if( value == "none" ) {
        return outing_kind::none;
    }
    return std::nullopt;
}

std::optional<structural_watch_kind> structural_watch_kind_from_string(
    const std::string &value )
{
    if( value == "none" ) {
        return structural_watch_kind::none;
    }
    if( value == "exact" ) {
        return structural_watch_kind::exact;
    }
    if( value == "fallback" ) {
        return structural_watch_kind::fallback;
    }
    return std::nullopt;
}

std::optional<bandit_live_world::scout_assessment_threshold_class>
scout_assessment_threshold_from_string(
    const std::string &value )
{
    if( value == "none" ) {
        return bandit_live_world::scout_assessment_threshold_class::none;
    }
    if( value == "normal" ) {
        return bandit_live_world::scout_assessment_threshold_class::normal;
    }
    if( value == "burned" ) {
        return bandit_live_world::scout_assessment_threshold_class::burned;
    }
    return std::nullopt;
}

std::string scout_assessment_threshold_to_string(
    const bandit_live_world::scout_assessment_threshold_class threshold )
{
    switch( threshold ) {
        case bandit_live_world::scout_assessment_threshold_class::none:
            return "none";
        case bandit_live_world::scout_assessment_threshold_class::normal:
            return "normal";
        case bandit_live_world::scout_assessment_threshold_class::burned:
            return "burned";
    }
    return "none";
}

std::optional<sortie_observation_kind> sortie_observation_kind_from_string(
    const std::string &value )
{
    if( value == "routine" ) {
        return sortie_observation_kind::routine;
    }
    if( value == "certainty" ) {
        return sortie_observation_kind::certainty;
    }
    if( value == "bounds" ) {
        return sortie_observation_kind::bounds;
    }
    if( value == "route_state" ) {
        return sortie_observation_kind::route_state;
    }
    if( value == "alert" ) {
        return sortie_observation_kind::alert;
    }
    if( value == "target_revision" ) {
        return sortie_observation_kind::target_revision;
    }
    if( value == "hard_danger" ) {
        return sortie_observation_kind::hard_danger;
    }
    if( value == "contradiction" ) {
        return sortie_observation_kind::contradiction;
    }
    if( value == "casualty" ) {
        return sortie_observation_kind::casualty;
    }
    if( value == "burn" ) {
        return sortie_observation_kind::burn;
    }
    return std::nullopt;
}

std::optional<sortie_observation_sense> sortie_observation_sense_from_string(
    const std::string &value )
{
    if( value == "visual" ) {
        return sortie_observation_sense::visual;
    }
    if( value == "smoke" ) {
        return sortie_observation_sense::smoke;
    }
    if( value == "light" ) {
        return sortie_observation_sense::light;
    }
    if( value == "sound" ) {
        return sortie_observation_sense::sound;
    }
    return std::nullopt;
}

std::optional<sortie_observation_share_state> sortie_observation_share_state_from_string(
    const std::string &value )
{
    if( value == "private" ) {
        return sortie_observation_share_state::observer_private;
    }
    if( value == "shared" ) {
        return sortie_observation_share_state::shared;
    }
    if( value == "reported" ) {
        return sortie_observation_share_state::reported;
    }
    return std::nullopt;
}

std::string sortie_observation_sense_to_string( const sortie_observation_sense sense )
{
    switch( sense ) {
        case sortie_observation_sense::visual:
            return "visual";
        case sortie_observation_sense::smoke:
            return "smoke";
        case sortie_observation_sense::light:
            return "light";
        case sortie_observation_sense::sound:
            return "sound";
    }
    return "visual";
}

std::string sortie_observation_share_state_to_string(
    const sortie_observation_share_state share_state )
{
    switch( share_state ) {
        case sortie_observation_share_state::observer_private:
            return "private";
        case sortie_observation_share_state::shared:
            return "shared";
        case sortie_observation_share_state::reported:
            return "reported";
    }
    return "private";
}

std::optional<simulation_owner> simulation_owner_from_string( const std::string &value )
{
    if( value == "abstract" ) {
        return simulation_owner::abstract;
    }
    if( value == "local" ) {
        return simulation_owner::local;
    }
    return std::nullopt;
}

std::optional<scout_phase> scout_phase_from_string( const std::string &value )
{
    if( value == "assembling" ) {
        return scout_phase::assembling;
    }
    if( value == "outbound" ) {
        return scout_phase::outbound;
    }
    if( value == "searching" ) {
        return scout_phase::searching;
    }
    if( value == "observing" ) {
        return scout_phase::observing;
    }
    if( value == "harvesting" ) {
        return scout_phase::harvesting;
    }
    if( value == "burned_withdrawal" ) {
        return scout_phase::burned_withdrawal;
    }
    if( value == "returning_exposed" ) {
        return scout_phase::returning_exposed;
    }
    if( value == "returning_report" ) {
        return scout_phase::returning_report;
    }
    if( value == "returning_home" ) {
        return scout_phase::returning_home;
    }
    if( value == "lost" ) {
        return scout_phase::lost;
    }
    return std::nullopt;
}

std::optional<camp_decision_state> camp_decision_state_from_string( const std::string &value )
{
    if( value == "idle" ) {
        return camp_decision_state::idle;
    }
    if( value == "report_awaiting_assessment" ) {
        return camp_decision_state::report_awaiting_assessment;
    }
    if( value == "preparing_follow_on" ) {
        return camp_decision_state::preparing_follow_on;
    }
    if( value == "cooldown" ) {
        return camp_decision_state::cooldown;
    }
    if( value == "abandoned" ) {
        return camp_decision_state::abandoned;
    }
    return std::nullopt;
}

std::optional<camp_report_policy> camp_report_policy_from_string( const std::string &value )
{
    if( value == "bandit_shakedown" ) {
        return camp_report_policy::bandit_shakedown;
    }
    if( value == "cannibal_night_raid" ) {
        return camp_report_policy::cannibal_night_raid;
    }
    if( value == "none" ) {
        return camp_report_policy::none;
    }
    return std::nullopt;
}

std::optional<hostile_operation_kind> hostile_operation_kind_from_string(
    const std::string &value )
{
    if( value == "shakedown" ) {
        return hostile_operation_kind::shakedown;
    }
    if( value == "raid" ) {
        return hostile_operation_kind::raid;
    }
    if( value == "none" ) {
        return hostile_operation_kind::none;
    }
    return std::nullopt;
}

std::optional<hostile_operation_phase> hostile_operation_phase_from_string(
    const std::string &value )
{
    if( value == "assembling" ) {
        return hostile_operation_phase::assembling;
    }
    if( value == "outbound" ) {
        return hostile_operation_phase::outbound;
    }
    if( value == "rallying" ) {
        return hostile_operation_phase::rallying;
    }
    if( value == "waiting_night" ) {
        return hostile_operation_phase::waiting_night;
    }
    if( value == "approaching" ) {
        return hostile_operation_phase::approaching;
    }
    if( value == "committed_contact" ) {
        return hostile_operation_phase::committed_contact;
    }
    if( value == "returning_home" ) {
        return hostile_operation_phase::returning_home;
    }
    if( value == "lost" ) {
        return hostile_operation_phase::lost;
    }
    return std::nullopt;
}

outing_kind classify_legacy_outing_kind( const std::string &activity_id,
        const std::string &job_type )
{
    if( activity_id.find( "#structural" ) != std::string::npos ) {
        return outing_kind::structural_sortie;
    }
    if( job_type == "scout" || job_type == "scavenge" ) {
        return outing_kind::scout_sortie;
    }
    return outing_kind::hostile_operation;
}

std::optional<bandit_pursuit_handoff::remaining_return_pressure_state>
remaining_return_pressure_state_from_string( const std::string &value )
{
    using bandit_pursuit_handoff::remaining_return_pressure_state;
    if( value == "ample" ) {
        return remaining_return_pressure_state::ample;
    }
    if( value == "tight" ) {
        return remaining_return_pressure_state::tight;
    }
    if( value == "plain_return_only" ) {
        return remaining_return_pressure_state::plain_return_only;
    }
    if( value == "collapsed" ) {
        return remaining_return_pressure_state::collapsed;
    }
    return std::nullopt;
}

std::optional<bandit_dry_run::job_template> job_template_from_string( const std::string &value )
{
    using bandit_dry_run::job_template;
    if( value == "scout" ) {
        return job_template::scout;
    }
    if( value == "scavenge" ) {
        return job_template::scavenge;
    }
    if( value == "toll" ) {
        return job_template::toll;
    }
    if( value == "stalk" ) {
        return job_template::stalk;
    }
    if( value == "steal" ) {
        return job_template::steal;
    }
    if( value == "raid" ) {
        return job_template::raid;
    }
    if( value == "reinforce" ) {
        return job_template::reinforce;
    }
    if( value == "hold / chill" || value == "hold_chill" || value.empty() ) {
        return job_template::hold_chill;
    }
    return std::nullopt;
}

void push_unique_mark( std::vector<std::string> &marks, const std::string &mark )
{
    if( mark.empty() ) {
        return;
    }
    if( std::find( marks.begin(), marks.end(), mark ) == marks.end() ) {
        marks.push_back( mark );
    }
}

std::string camp_lead_id_for( const std::string &site_id, const camp_lead_kind kind,
                              const std::string &target_id, const tripoint_abs_omt &omt )
{
    std::ostringstream out;
    out << site_id << "#lead:" << bandit_live_world::to_string( kind ) << ':'
        << target_id << '@' << omt.x() << ',' << omt.y() << ',' << omt.z();
    return out.str();
}

std::string lowercase_copy( const std::string &value )
{
    std::string lowered = value;
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( const unsigned char ch ) {
        return static_cast<char>( std::tolower( ch ) );
    } );
    return lowered;
}

bool contains_any_token( const std::string &haystack, const std::vector<std::string> &needles )
{
    for( const std::string &needle : needles ) {
        if( haystack.find( needle ) != std::string::npos ) {
            return true;
        }
    }
    return false;
}

bool matches_terrain_id_family( const std::string &terrain_id,
                                const std::vector<std::string> &families )
{
    for( const std::string &family : families ) {
        if( terrain_id == family || terrain_id.rfind( family + "_", 0 ) == 0 ) {
            return true;
        }
    }
    return false;
}

camp_lead_kind signal_kind_to_camp_lead_kind( const std::string &kind )
{
    if( kind == "smoke" ) {
        return camp_lead_kind::smoke_signal;
    }
    if( kind == "light" ) {
        return camp_lead_kind::light_signal;
    }
    if( kind == "sound" ) {
        return camp_lead_kind::sound_signal;
    }
    return camp_lead_kind::human_activity;
}

bandit_dry_run::lead_family family_for_camp_map_lead( const camp_map_lead &lead )
{
    switch( lead.kind ) {
        case camp_lead_kind::moving_actor:
            return bandit_dry_run::lead_family::moving_carrier;
        case camp_lead_kind::route_activity:
        case camp_lead_kind::smoke_signal:
        case camp_lead_kind::light_signal:
        case camp_lead_kind::sound_signal:
            return bandit_dry_run::lead_family::corridor;
        case camp_lead_kind::structural_bounty:
        case camp_lead_kind::terrain_opportunity:
        case camp_lead_kind::harvested_site:
        case camp_lead_kind::human_activity:
        case camp_lead_kind::basecamp_activity:
        case camp_lead_kind::threat_memory:
        case camp_lead_kind::loss_site:
        case camp_lead_kind::false_lead:
        case camp_lead_kind::frontier_probe:
            return bandit_dry_run::lead_family::site;
    }

    return bandit_dry_run::lead_family::site;
}

bandit_dry_run::candidate_debug make_camp_map_dispatch_candidate( const camp_map_lead &lead,
        const bandit_live_world::camp_map_dispatch_decision &decision )
{
    bandit_dry_run::candidate_debug candidate;
    candidate.job = decision.intent;
    candidate.lead_id = lead.lead_id.empty() ? lead.target_id : lead.lead_id;
    candidate.envelope_id = lead.target_id.empty() ? candidate.lead_id : lead.target_id;
    candidate.family = family_for_camp_map_lead( lead );
    candidate.generated = true;
    candidate.valid = decision.intent == bandit_dry_run::job_template::scout ||
                      decision.intent == bandit_dry_run::job_template::stalk ||
                      decision.intent == bandit_dry_run::job_template::toll ||
                      decision.intent == bandit_dry_run::job_template::raid;
    candidate.winner = candidate.valid;
    candidate.score.lead_bounty_value = lead.bounty;
    candidate.score.lead_confidence_bonus = lead.confidence;
    candidate.score.threat_penalty = lead.threat;
    candidate.score.threat_gate_result = lead.threat_confirmed ?
                                         bandit_dry_run::threat_gate::soft_veto :
                                         bandit_dry_run::threat_gate::discount_only;
    candidate.score.reward_profile_match = decision.reward_score;
    candidate.score.effective_threat_penalty = decision.risk_score;
    candidate.score.final_job_score = decision.margin;
    candidate.notes = decision.notes;
    candidate.notes.push_back( "camp-map remembered lead " + candidate.lead_id +
                               " selected " + bandit_dry_run::to_string( decision.intent ) +
                               " reward=" + std::to_string( decision.reward_score ) +
                               " risk=" + std::to_string( decision.risk_score ) +
                               " margin=" + std::to_string( decision.margin ) );
    return candidate;
}

std::optional<int> next_camp_map_lead_revision( const int revision )
{
    if( revision >= std::numeric_limits<int>::max() ) {
        return std::nullopt;
    }
    return std::max( 1, revision + 1 );
}

bool camp_map_lead_payload_matches( const camp_map_lead &lhs, const camp_map_lead &rhs )
{
    return lhs.lead_id == rhs.lead_id && lhs.kind == rhs.kind && lhs.origin == rhs.origin &&
           lhs.status == rhs.status &&
           lhs.target_id == rhs.target_id && lhs.omt == rhs.omt &&
           lhs.radius_omt == rhs.radius_omt && lhs.source_key == rhs.source_key &&
           lhs.source_summary == rhs.source_summary &&
           lhs.first_seen_minutes == rhs.first_seen_minutes &&
           lhs.last_seen_minutes == rhs.last_seen_minutes &&
           lhs.last_checked_minutes == rhs.last_checked_minutes &&
           lhs.last_scouted_minutes == rhs.last_scouted_minutes && lhs.bounty == rhs.bounty &&
           lhs.threat == rhs.threat && lhs.confidence == rhs.confidence &&
           lhs.threat_confirmed == rhs.threat_confirmed && lhs.target_alert == rhs.target_alert &&
           lhs.scout_seen == rhs.scout_seen &&
           lhs.generated_by_this_camp_routine == rhs.generated_by_this_camp_routine &&
           lhs.prior_bandit_losses == rhs.prior_bandit_losses &&
           lhs.prior_defender_losses == rhs.prior_defender_losses &&
           lhs.times_checked_empty == rhs.times_checked_empty &&
           lhs.times_harvested == rhs.times_harvested && lhs.last_outcome == rhs.last_outcome;
}

void update_target_lead_reference( bandit_live_world::active_outing_state &outing,
                                   const std::string &lead_id, const int old_revision,
                                   const int new_revision )
{
    if( outing.target_lead_id == lead_id &&
        ( outing.target_lead_revision == old_revision || outing.target_lead_revision <= 0 ) ) {
        if( outing.kind == outing_kind::structural_sortie &&
            outing.phase == scout_phase::observing &&
            outing.assessment.observation_started_minutes >= 0 ) {
            return;
        }
        outing.target_lead_revision = new_revision;
        if( outing.kind == outing_kind::structural_sortie ) {
            outing.assessment.pinned_target_revision = new_revision;
        }
    }
}

void update_target_lead_reference( bandit_live_world::scout_report_record &report,
                                   const std::string &lead_id, const int old_revision,
                                   const int new_revision )
{
    if( report.target_lead_id == lead_id &&
        ( report.target_lead_revision == old_revision || report.target_lead_revision <= 0 ) ) {
        report.target_lead_revision = new_revision;
    }
}

void update_target_lead_reference( bandit_live_world::camp_decision_record &decision,
                                   const std::string &lead_id, const int old_revision,
                                   const int new_revision )
{
    if( decision.target_lead_id == lead_id &&
        ( decision.target_lead_revision == old_revision || decision.target_lead_revision <= 0 ) ) {
        decision.target_lead_revision = new_revision;
    }
}

void update_target_lead_references( bandit_live_world::site_record &site,
                                    const std::string &lead_id, const int old_revision,
                                    const int new_revision )
{
    update_target_lead_reference( site.active_outing, lead_id, old_revision, new_revision );
    update_target_lead_reference( site.active_hostile_operation.reservation, lead_id,
                                  old_revision, new_revision );
    update_target_lead_reference( site.current_scout_report, lead_id, old_revision, new_revision );
    update_target_lead_reference( site.camp_decision, lead_id, old_revision, new_revision );
}

bool advance_camp_map_lead_revision( bandit_live_world::site_record &site,
                                     camp_map_lead &lead )
{
    const int old_revision = std::max( 1, lead.revision );
    const std::optional<int> new_revision = next_camp_map_lead_revision( old_revision );
    if( !new_revision ) {
        return false;
    }
    lead.revision = *new_revision;
    update_target_lead_references( site, lead.lead_id, old_revision, *new_revision );
    return true;
}

bool upsert_camp_map_lead_transaction( bandit_live_world::site_record &site,
                                      camp_map_lead lead )
{
    bound_camp_map_lead_strings( lead );
    if( lead.lead_id.empty() ) {
        return false;
    }
    const std::string lead_id = lead.lead_id;

    bandit_live_world::site_record candidate = site;
    if( camp_map_lead *existing = candidate.intelligence_map.find_lead( lead.lead_id ) ) {
        if( existing->origin != lead.origin ) {
            return false;
        }
        const int old_revision = std::max( 1, existing->revision );
        lead.revision = old_revision;
        if( camp_map_lead_payload_matches( *existing, lead ) ) {
            bandit_live_world::normalize_camp_intelligence( candidate );
            site = std::move( candidate );
            return true;
        }
        const std::optional<int> new_revision = next_camp_map_lead_revision( old_revision );
        if( !new_revision ) {
            return false;
        }
        lead.revision = *new_revision;
        *existing = lead;
        update_target_lead_references( candidate, lead.lead_id, old_revision, lead.revision );
    } else {
        lead.revision = std::max( 1, lead.revision );
        candidate.intelligence_map.leads.push_back( std::move( lead ) );
    }
    bandit_live_world::normalize_camp_intelligence( candidate );
    if( candidate.intelligence_map.find_lead( lead_id ) == nullptr ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

void migrate_scalar_memory_to_intelligence_map( bandit_live_world::site_record &site,
        const bool intelligence_map_was_present )
{
    if( intelligence_map_was_present || !site.intelligence_map.leads.empty() ||
        ( site.remembered_target_or_mark.empty() && site.remembered_bounty_estimate <= 0 &&
          site.remembered_threat_estimate <= 0 ) ) {
        return;
    }

    camp_map_lead lead;
    lead.kind = camp_lead_kind::human_activity;
    lead.origin = camp_lead_origin::legacy_radar;
    lead.status = camp_lead_status::suspected;
    lead.target_id = site.remembered_target_or_mark.empty() ? site.active_outing.target_id :
                     site.remembered_target_or_mark;
    lead.omt = site.active_outing.target_omt;
    lead.source_key = lead.target_id;
    lead.source_summary = "migrated from legacy remembered_* site memory";
    lead.bounty = std::max( 0, site.remembered_bounty_estimate );
    lead.threat = std::max( 0, site.remembered_threat_estimate );
    lead.confidence = lead.target_id.empty() ? 1 : 2;
    lead.threat_confirmed = lead.threat > 0;
    lead.last_outcome = "legacy_memory";
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    bandit_live_world::upsert_camp_map_lead( site, lead );
}

void record_scout_return_lead( bandit_live_world::site_record &site,
                               const bandit_pursuit_handoff::return_packet &packet,
                               const int bandit_losses )
{
    if( packet.job_type != bandit_dry_run::job_template::scout ||
        packet.current_target_or_mark.empty() || packet.survivors_remaining <= 0 ) {
        return;
    }

    camp_map_lead lead;
    lead.kind = camp_lead_kind::basecamp_activity;
    lead.origin = camp_lead_origin::returned_report;
    lead.status = camp_lead_status::scout_confirmed;
    lead.target_id = packet.current_target_or_mark;
    lead.omt = site.active_outing.target_omt;
    lead.source_key = packet.group_id;
    lead.source_summary = "scout-return writeback from active owned outing";
    lead.last_scouted_minutes = site.active_outing.started_minutes;
    lead.last_checked_minutes = site.active_outing.local_contact_minutes;
    lead.bounty = std::max( 1, site.remembered_bounty_estimate );
    lead.threat = std::max( 0, site.remembered_threat_estimate );
    lead.confidence = std::max( 2, packet.survivors_remaining + 1 );
    lead.threat_confirmed = lead.threat > 0 || packet.resolution == bandit_pursuit_handoff::lead_resolution::still_valid;
    lead.target_alert = packet.resolution == bandit_pursuit_handoff::lead_resolution::target_lost ||
                        packet.posture == bandit_pursuit_handoff::return_posture::broken_flee;
    lead.scout_seen = lead.target_alert;
    lead.prior_bandit_losses = bandit_losses;
    lead.last_outcome = bandit_pursuit_handoff::to_string( packet.result );
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    lead.revision = std::max( 1, site.active_outing.target_lead_revision );
    bandit_live_world::upsert_camp_map_lead( site, lead );
}

bandit_pursuit_handoff::abstract_group_state make_site_memory_group(
    const bandit_live_world::site_record &site )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    bandit_pursuit_handoff::abstract_group_state group;
    group.group_id = outing == nullptr ? site.site_id + "#dispatch" : outing->activity_id;
    group.source_camp_id = site.site_id;
    group.activity_generation = outing == nullptr ? site.next_outing_generation :
                                outing->generation;
    group.handoff_epoch = outing == nullptr ? 0 : outing->handoff_epoch;
    group.return_application_key = outing == nullptr ?
                                   bandit_pursuit_handoff::make_operation_component_key(
                                       group.group_id, group.activity_generation, "return" ) :
                                   outing->return_application_key;
    group.report_application_key = outing == nullptr ?
                                   bandit_pursuit_handoff::make_operation_component_key(
                                       group.group_id, group.activity_generation, "report" ) :
                                   outing->report_application_key;
    group.cargo_application_key = outing == nullptr ?
                                  bandit_pursuit_handoff::make_operation_component_key(
                                      group.group_id, group.activity_generation, "cargo" ) :
                                  outing->cargo_application_key;
    group.group_strength = site.count_live_members();
    group.current_target_or_mark = site.remembered_target_or_mark.empty() && outing != nullptr ?
                                   outing->target_id : site.remembered_target_or_mark;
    group.current_threat_estimate = site.remembered_threat_estimate;
    group.current_bounty_estimate = site.remembered_bounty_estimate;
    group.retreat_bias = site.remembered_retreat_bias;
    group.return_clock = site.remembered_return_clock;
    group.remaining_pressure = site.remembered_pressure;
    group.known_recent_marks = site.known_recent_marks;
    if( outing != nullptr ) {
        for( const character_id &member_id : outing->member_ids ) {
            group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
        }
    }
    return group;
}

std::string provisional_report_application_key( const bandit_live_world::site_record &site )
{
    std::string key = site.active_outing.report_application_key + ":members";
    for( const character_id &member_id : site.active_outing.resolved_member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member != nullptr && member->state == bandit_live_world::member_state::at_home ) {
            key += ":" + std::to_string( member_id.get_value() );
        }
    }
    return key;
}

void apply_group_memory( bandit_live_world::site_record &site,
                         const bandit_pursuit_handoff::abstract_group_state &group )
{
    site.remembered_target_or_mark = group.current_target_or_mark;
    site.remembered_threat_estimate = group.current_threat_estimate;
    site.remembered_bounty_estimate = group.current_bounty_estimate;
    site.remembered_retreat_bias = group.retreat_bias;
    site.remembered_return_clock = group.return_clock;
    site.remembered_pressure = group.remaining_pressure;
    site.known_recent_marks = group.known_recent_marks;
}

int shakedown_demand_modifier_percent( const bandit_live_world::site_record &site )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return 140;
    }
    if( site.shakedown_caution > 0 || site.shakedown_bandit_losses > 0 ) {
        return std::max( 50, 100 - 25 * std::max( site.shakedown_caution, site.shakedown_bandit_losses ) );
    }
    return 100;
}

std::string shakedown_outcome_label( const bandit_live_world::shakedown_outcome &outcome )
{
    if( outcome.paid ) {
        return "paid";
    }
    if( outcome.fought && outcome.bandit_losses > 0 ) {
        return "fight_bandit_loss";
    }
    if( outcome.fought && outcome.defender_losses > 0 ) {
        return "fight_defender_loss";
    }
    if( outcome.fought ) {
        return "fight_unresolved";
    }
    return "unknown";
}

struct shakedown_opening_beat {
    std::string id;
    std::string summary;
    std::string bark;
};

shakedown_opening_beat choose_shakedown_opening_beat( const bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::local_gate_decision &decision )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return { "reopened_demand",
                 "seen-you-before reopened demand after prior bloodshed",
                 "Last time you made this expensive.  Now you pay the higher cut, or we finish it." };
    }
    if( input.basecamp_or_camp_scene ) {
        return { "basecamp_pressure",
                 "basecamp leverage against supplies and workers",
                 "Nice camp.  Lots of hands, lots of supplies.  Pay our share and nobody has to count bodies." };
    }
    if( input.darkness_or_concealment || input.standoff_distance >= 2 ) {
        return { "warning_from_cover",
                 "bandits call from cover before closing the fork",
                 "You hear us before you see all of us.  Put the goods down and walk away breathing." };
    }
    if( input.local_threat <= 1 && decision.pressure_margin >= 3 ) {
        return { "weakness_read",
                 "bandits read the player's weak odds before demanding payment",
                 "You look light on friends and heavy on things worth taking.  Make this easy." };
    }
    return { "roadblock_toll",
             "roadblock toll demand",
             "Road's taxed now.  Pay the toll or fight for the privilege." };
}

int special_footprint_radius( const std::string &special_id )
{
    if( special_id == "bandit_camp" || special_id == "bandit_work_camp" ||
        special_id == "cannibal_camp" ) {
        return 1;
    }
    return 0;
}

std::vector<int> special_footprint_z_levels( const tripoint_abs_omt &origin )
{
    std::vector<int> z_levels;
    z_levels.reserve( 9 );
    for( int z = -2; z <= 5; ++z ) {
        z_levels.push_back( z );
    }
    if( std::find( z_levels.begin(), z_levels.end(), origin.z() ) == z_levels.end() ) {
        z_levels.push_back( origin.z() );
        std::sort( z_levels.begin(), z_levels.end() );
    }
    return z_levels;
}

bool omt_less( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs )
{
    if( lhs.z() != rhs.z() ) {
        return lhs.z() < rhs.z();
    }
    if( lhs.y() != rhs.y() ) {
        return lhs.y() < rhs.y();
    }
    return lhs.x() < rhs.x();
}

int required_dispatch_members( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::hold_chill:
            return 0;
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::scavenge:
        case bandit_dry_run::job_template::stalk:
        case bandit_dry_run::job_template::steal:
            return 1;
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return 2;
    }

    return 0;
}

bool is_routine_scout_job( const bandit_dry_run::job_template job )
{
    return job == bandit_dry_run::job_template::scout ||
           job == bandit_dry_run::job_template::scavenge;
}

bool counts_toward_live_headcount( member_state state )
{
    return state == member_state::at_home || state == member_state::outbound ||
           state == member_state::local_contact || state == member_state::orphaned;
}

struct hostile_site_profile_rules {
    hostile_site_profile profile = hostile_site_profile::none;
    std::string id;
    int home_reserve = 0;
    int scout_job_bonus = 0;
    int threat_penalty = 1;
    int retreat_bias_floor = 1;
    int return_clock_floor = 1;
    bandit_pursuit_handoff::remaining_return_pressure_state default_remaining_pressure =
        bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    std::string writeback_expectation;
};

hostile_site_profile effective_profile( const bandit_live_world::site_record &site )
{
    return site.profile == hostile_site_profile::none ?
           bandit_live_world::profile_for_site_kind( site.site_kind ) : site.profile;
}

bool supports_routine_camp_ecology( const hostile_site_profile profile )
{
    return profile == hostile_site_profile::camp_style ||
           profile == hostile_site_profile::cannibal_camp;
}

camp_report_policy report_policy_for_profile( const hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::cannibal_camp:
            return camp_report_policy::cannibal_night_raid;
        case hostile_site_profile::camp_style:
        case hostile_site_profile::small_hostile_site:
            return camp_report_policy::bandit_shakedown;
        case hostile_site_profile::none:
            return camp_report_policy::none;
    }
    return camp_report_policy::none;
}

hostile_operation_kind operation_kind_for_report_policy( const camp_report_policy policy )
{
    switch( policy ) {
        case camp_report_policy::bandit_shakedown:
            return hostile_operation_kind::shakedown;
        case camp_report_policy::cannibal_night_raid:
            return hostile_operation_kind::raid;
        case camp_report_policy::none:
            return hostile_operation_kind::none;
    }
    return hostile_operation_kind::none;
}

hostile_site_profile_rules rules_for_profile( hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::camp_style:
            return { profile, "camp_style", 1, 1, 1, 1, 2,
                bandit_pursuit_handoff::remaining_return_pressure_state::ample,
                "checks the shared 30-minute cadence, keeps a home reserve, and writes back as persistent camp pressure" };
        case hostile_site_profile::cannibal_camp:
            return { profile, "cannibal_camp", 2, 2, 0, 3, 3,
                bandit_pursuit_handoff::remaining_return_pressure_state::tight,
                "checks the shared 30-minute cadence, keeps a larger home larder guard, and writes back as hungry camp pressure" };
        case hostile_site_profile::small_hostile_site:
            return { profile, "small_hostile_site", 0, 1, 0, 2, 1,
                bandit_pursuit_handoff::remaining_return_pressure_state::tight,
                "can commit its whole small roster and writes back as brittle local pressure" };
        case hostile_site_profile::none:
            return { profile, "none", 0, 0, 1, 1, 1,
                bandit_pursuit_handoff::remaining_return_pressure_state::ample,
                "falls back to minimal hostile-site defaults" };
    }

    return { hostile_site_profile::none, "none", 0, 0, 1, 1, 1,
        bandit_pursuit_handoff::remaining_return_pressure_state::ample,
        "falls back to minimal hostile-site defaults" };
}

int required_home_reserve( const bandit_live_world::site_record &site )
{
    const hostile_site_profile profile = effective_profile( site );
    if( profile != hostile_site_profile::camp_style ) {
        return rules_for_profile( profile ).home_reserve;
    }

    const bandit_live_world::roster_view roster = site.roster();
    const int living_roster = roster.valid ? roster.living_total : 0;
    if( living_roster <= 1 ) {
        return living_roster;
    }
    if( living_roster == 2 ) {
        return 1;
    }
    if( living_roster <= 4 ) {
        return 1;
    }
    if( living_roster <= 7 ) {
        return 2;
    }
    return std::max( 3, ( living_roster * 35 + 99 ) / 100 );
}

bool hostile_operation_party_preserves_home( const bandit_live_world::site_record &site,
        const std::size_t party_size )
{
    const int ready_at_home = static_cast<int>( std::count_if( site.members.begin(),
                              site.members.end(), []( const bandit_live_world::member_record & member ) {
        return member.state == member_state::at_home && !member.wounded_or_unready;
    } ) );
    const int remaining_ready = ready_at_home - static_cast<int>( party_size );
    return remaining_ready >= required_home_reserve( site );
}

bool cannibal_job_requires_attack_pack( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::stalk:
        case bandit_dry_run::job_template::steal:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return true;
        case bandit_dry_run::job_template::hold_chill:
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::scavenge:
            return false;
    }

    return false;
}

int required_dispatch_members_for_profile( const bandit_live_world::site_record &site,
        bandit_dry_run::job_template job )
{
    const int generic_required = required_dispatch_members( job );
    if( generic_required <= 0 ) {
        return generic_required;
    }

    if( effective_profile( site ) != hostile_site_profile::cannibal_camp ||
        !cannibal_job_requires_attack_pack( job ) ) {
        return generic_required;
    }

    // Cannibal attack pressure is a pack choice.  Explicit scouts may remain small, but a stalk/raid
    // handoff must not turn one disposable hunter into the whole fight.
    const int available = site.dispatchable_member_capacity();
    if( available < 2 ) {
        return 2;
    }
    return std::clamp( available, 2, 3 );
}

bandit_dry_run::camp_input make_dispatch_camp_input( const bandit_live_world::site_record &site )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_dry_run::camp_input camp;
    camp.available_manpower = site.dispatchable_member_capacity();
    if( camp.available_manpower >= 3 ) {
        camp.shortage = bandit_dry_run::shortage_band::stable;
    } else if( camp.available_manpower == 2 ) {
        camp.shortage = bandit_dry_run::shortage_band::low;
    } else {
        camp.shortage = bandit_dry_run::shortage_band::critical;
    }
    camp.job_type_bonus[bandit_dry_run::job_template::scout] = rules.scout_job_bonus;
    return camp;
}

bandit_dry_run::lead_input make_nearby_target_lead( const bandit_live_world::site_record &site,
        const tripoint_abs_omt &target_omt, const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_dry_run::lead_input lead;
    lead.id = target_id;
    lead.envelope_id = target_id;
    lead.family = bandit_dry_run::lead_family::site;
    const int distance = rl_dist( site.anchor, target_omt );
    lead.distance_multiplier = std::clamp( 1.0 - static_cast<double>( distance ) / 20.0, 0.35, 1.0 );
    lead.lead_bounty_value = distance <= 10 ? 2 : 1;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = rules.threat_penalty;
    lead.threat_gate_result = bandit_dry_run::threat_gate::soft_veto;
    lead.hard_blocked_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
    };
    if( rules.profile == hostile_site_profile::cannibal_camp ) {
        if( site.dispatchable_member_capacity() >= 2 ) {
            lead.family = bandit_dry_run::lead_family::corridor;
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::scout );
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::toll );
            lead.validity_notes.push_back(
                "cannibal_camp pack pressure: nearby target promotes stalk pressure only after reserve leaves a pack" );
        } else {
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::raid );
            lead.validity_notes.push_back(
                "cannibal_camp scout/probe pressure: lone available member may scout but cannot become the whole attack pack" );
        }
    } else {
        lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::raid );
    }
    lead.validity_notes.push_back( "live-world nearby target envelope from owned site " + site.site_id );
    lead.validity_notes.push_back( "hostile profile " + rules.id + ": " + rules.writeback_expectation );
    lead.validity_notes.push_back( rules.profile == hostile_site_profile::cannibal_camp ?
                                   "bounded v0 dispatch separates cannibal scout/probe pressure from pack attack pressure" :
                                   "bounded v0 dispatch only promotes scout pursuit from real owned members" );
    return lead;
}

std::vector<character_id> select_dispatch_members( const bandit_live_world::site_record &site, int count )
{
    std::vector<character_id> member_ids;
    member_ids.reserve( std::max( count, 0 ) );
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::at_home || member.wounded_or_unready ) {
            continue;
        }
        member_ids.push_back( member.npc_id );
        if( static_cast<int>( member_ids.size() ) >= count ) {
            break;
        }
    }
    return member_ids;
}

struct routine_member_capability {
    int observer = 0;
    int escort = 0;
    int defender = 0;
};

routine_member_capability routine_capability_for_template( const std::string &template_id )
{
    if( template_id == "bandit" || template_id == "cannibal_hunter" ) {
        return { 3, 2, 2 };
    }
    if( template_id == "bandit_mechanic" ) {
        return { 2, 2, 2 };
    }
    if( template_id == "hells_raiders_boss" || template_id == "cannibal_camp_leader" ) {
        return { 2, 3, 4 };
    }
    if( template_id == "thug" || template_id == "cannibal_butcher" ) {
        return { 1, 3, 3 };
    }
    if( template_id == "bandit_trader" || template_id == "bandit_quartermaster" ) {
        return { 1, 1, 1 };
    }
    return {};
}

bandit_pursuit_handoff::abstract_group_state make_dispatch_group( const bandit_live_world::site_record &site,
        const std::vector<character_id> &member_ids, const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_pursuit_handoff::abstract_group_state group = make_site_memory_group( site );
    group.group_id = site.site_id + "#dispatch";
    group.source_camp_id = site.site_id;
    group.group_strength = member_ids.size();
    group.confidence = std::clamp( site.count_live_members(), 1, 3 );
    group.panic_threshold = std::max( 1, static_cast<int>( member_ids.size() ) );
    group.cargo_capacity = std::max( 1, static_cast<int>( member_ids.size() ) * 2 );
    group.current_target_or_mark = target_id;
    group.current_threat_estimate = std::max( 1, group.current_threat_estimate );
    group.current_bounty_estimate = std::max( 2, group.current_bounty_estimate );
    group.mission_urgency = 1;
    group.retreat_bias = std::max( group.retreat_bias, rules.retreat_bias_floor );
    group.goal_stickiness = 1;
    group.goal_preemption_posture = 1;
    group.return_clock = std::max( group.return_clock, rules.return_clock_floor );
    group.remaining_pressure = rules.default_remaining_pressure;
    group.anchored_identities.clear();
    for( const character_id &member_id : member_ids ) {
        group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
    }
    push_unique_mark( group.known_recent_marks, target_id );
    return group;
}

bool report_matches_camp_decision( const bandit_live_world::scout_report_record &report,
                                   const bandit_live_world::camp_decision_record &decision )
{
    const bool lead_id_matches = report.target_lead_id.empty() ||
                                 decision.target_lead_id.empty() ||
                                 decision.target_lead_id == report.target_lead_id;
    return report.is_present() && !report.provisional && report.source_job_type == "scout" &&
           report.action_policy != camp_report_policy::none &&
           decision.report_policy == report.action_policy &&
           decision.has_pinned_report() &&
           decision.source_report_revision == report.revision &&
           decision.source_report_generation == report.source_generation &&
           decision.source_report_activity_id == report.source_activity_id &&
           decision.source_report_application_key == report.application_key &&
           decision.target_id == report.target_id && decision.target_omt == report.target_omt &&
           lead_id_matches &&
           decision.target_lead_revision == report.target_lead_revision;
}

bool acted_report_key_matches( const bandit_live_world::acted_report_summary &summary,
                               const std::string &target_id,
                               const tripoint_abs_omt &target_omt,
                               const camp_report_policy policy )
{
    return summary.target_id == target_id && summary.target_omt == target_omt &&
           summary.policy == policy;
}

bool acted_report_key_less( const bandit_live_world::acted_report_summary &lhs,
                            const bandit_live_world::acted_report_summary &rhs )
{
    if( lhs.target_id != rhs.target_id ) {
        return lhs.target_id < rhs.target_id;
    }
    if( lhs.target_omt != rhs.target_omt ) {
        return lhs.target_omt < rhs.target_omt;
    }
    return static_cast<int>( lhs.policy ) < static_cast<int>( rhs.policy );
}

bool acted_report_is_newer( const bandit_live_world::acted_report_summary &lhs,
                            const bandit_live_world::acted_report_summary &rhs )
{
    return std::tie( lhs.source_generation, lhs.report_revision, lhs.acted_minutes ) >
           std::tie( rhs.source_generation, rhs.report_revision, rhs.acted_minutes );
}

int acted_report_retention_priority( const bandit_live_world::site_record &site,
                                     const bandit_live_world::acted_report_summary &summary )
{
    if( site.camp_decision.has_pinned_report() &&
        acted_report_key_matches( summary, site.camp_decision.target_id,
                                  site.camp_decision.target_omt,
                                  site.camp_decision.report_policy ) ) {
        return 2;
    }
    if( site.current_scout_report.is_present() &&
        acted_report_key_matches( summary, site.current_scout_report.target_id,
                                  site.current_scout_report.target_omt,
                                  site.current_scout_report.action_policy ) ) {
        return 1;
    }
    return 0;
}

void normalize_acted_reports( bandit_live_world::site_record &site )
{
    std::vector<bandit_live_world::acted_report_summary> candidates =
        std::move( site.acted_reports );
    for( bandit_live_world::acted_report_summary &summary : candidates ) {
        summary.target_id.resize( std::min( summary.target_id.size(),
                                           max_camp_lead_target_id_length ) );
        summary.source_generation = std::max( 0, summary.source_generation );
        summary.report_revision = std::max( 0, summary.report_revision );
        summary.acted_minutes = std::max( -1, summary.acted_minutes );
    }
    candidates.erase( std::remove_if( candidates.begin(), candidates.end(),
    []( const bandit_live_world::acted_report_summary & summary ) {
        return summary.target_id.empty() || summary.policy == camp_report_policy::none ||
               summary.source_generation <= 0 || summary.report_revision <= 0;
    } ), candidates.end() );
    std::sort( candidates.begin(), candidates.end(),
    []( const bandit_live_world::acted_report_summary & lhs,
    const bandit_live_world::acted_report_summary & rhs ) {
        if( acted_report_key_less( lhs, rhs ) ) {
            return true;
        }
        if( acted_report_key_less( rhs, lhs ) ) {
            return false;
        }
        return acted_report_is_newer( lhs, rhs );
    } );

    std::vector<bandit_live_world::acted_report_summary> normalized;
    normalized.reserve( std::min( candidates.size(), max_acted_report_summaries ) );
    for( bandit_live_world::acted_report_summary &summary : candidates ) {
        if( !normalized.empty() && !acted_report_key_less( normalized.back(), summary ) &&
            !acted_report_key_less( summary, normalized.back() ) ) {
            continue;
        }
        normalized.push_back( std::move( summary ) );
    }
    std::sort( normalized.begin(), normalized.end(), [&site](
    const bandit_live_world::acted_report_summary & lhs,
    const bandit_live_world::acted_report_summary & rhs ) {
        const int lhs_priority = acted_report_retention_priority( site, lhs );
        const int rhs_priority = acted_report_retention_priority( site, rhs );
        if( lhs_priority != rhs_priority ) {
            return lhs_priority > rhs_priority;
        }
        if( acted_report_is_newer( lhs, rhs ) ) {
            return true;
        }
        if( acted_report_is_newer( rhs, lhs ) ) {
            return false;
        }
        return acted_report_key_less( lhs, rhs );
    } );
    if( normalized.size() > max_acted_report_summaries ) {
        normalized.resize( max_acted_report_summaries );
    }
    std::sort( normalized.begin(), normalized.end(), acted_report_key_less );
    site.acted_reports = std::move( normalized );
}

const bandit_live_world::acted_report_summary *find_acted_report(
    const bandit_live_world::site_record &site,
    const bandit_live_world::scout_report_record &report )
{
    const auto match = std::find_if( site.acted_reports.begin(), site.acted_reports.end(),
    [&report]( const bandit_live_world::acted_report_summary & summary ) {
        return acted_report_key_matches( summary, report.target_id, report.target_omt,
                                         report.action_policy );
    } );
    return match == site.acted_reports.end() ? nullptr : &*match;
}

void remember_acted_report( bandit_live_world::site_record &site,
                            const bandit_live_world::scout_report_record &report )
{
    bandit_live_world::acted_report_summary summary;
    summary.target_id = report.target_id;
    summary.target_omt = report.target_omt;
    summary.policy = report.action_policy;
    summary.source_generation = report.source_generation;
    summary.report_revision = report.revision;
    summary.acted_minutes = report.delivered_minutes;
    site.acted_reports.push_back( std::move( summary ) );
    normalize_acted_reports( site );
}

std::optional<int> next_scout_report_revision( const bandit_live_world::site_record &site )
{
    const bandit_live_world::active_outing_state &outing = site.active_outing;
    const bandit_live_world::scout_report_record &current = site.current_scout_report;
    if( current.is_present() && current.source_activity_id == outing.activity_id &&
        current.source_generation == outing.generation ) {
        if( current.revision >= std::numeric_limits<int>::max() ) {
            return std::nullopt;
        }
        return current.revision + 1;
    }
    return 1;
}

bool hostile_operation_job_matches( const hostile_operation_kind operation_kind,
                                    const std::string &job_type )
{
    switch( operation_kind ) {
        case hostile_operation_kind::shakedown:
            return job_type == "stalk" || job_type == "toll";
        case hostile_operation_kind::raid:
            return job_type == "raid";
        case hostile_operation_kind::none:
            return false;
    }

    return false;
}

hostile_operation_kind hostile_operation_kind_for_job( const std::string &job_type )
{
    return job_type == "raid" ? hostile_operation_kind::raid :
           hostile_operation_kind::shakedown;
}

bool report_matches_hostile_operation( const bandit_live_world::scout_report_record &report,
                                       const bandit_live_world::hostile_operation_state &operation )
{
    const bool lead_id_matches = report.target_lead_id.empty() ||
                                 operation.reservation.target_lead_id.empty() ||
                                 operation.reservation.target_lead_id == report.target_lead_id;
    return report.is_present() && !report.provisional && report.source_job_type == "scout" &&
           operation.source_report_revision == report.revision &&
           operation.source_report_generation == report.source_generation &&
           operation.source_report_activity_id == report.source_activity_id &&
           operation.source_report_application_key == report.application_key &&
           operation.reservation.target_id == report.target_id &&
           operation.reservation.target_omt == report.target_omt &&
           lead_id_matches &&
           operation.reservation.target_lead_revision == report.target_lead_revision;
}

bool hostile_operation_phase_matches_reservation(
    const bandit_live_world::hostile_operation_state &operation )
{
    switch( operation.phase ) {
        case hostile_operation_phase::assembling:
            return operation.reservation.phase == scout_phase::assembling;
        case hostile_operation_phase::committed_contact:
            return operation.reservation.phase == scout_phase::observing;
        case hostile_operation_phase::returning_home:
            return operation.reservation.phase == scout_phase::returning_home;
        case hostile_operation_phase::lost:
            return operation.reservation.phase == scout_phase::lost;
        default:
            return operation.reservation.phase == scout_phase::outbound;
    }
}

bool local_handoff_snapshot_is_empty(
    const bandit_live_world::local_handoff_snapshot &snapshot )
{
    return snapshot.activity_id.empty() && snapshot.activity_generation == 0 &&
           snapshot.handoff_epoch == -1 && snapshot.members.empty() &&
           snapshot.casualty_ids.empty() && snapshot.committed_minutes == -1 &&
           snapshot.cohesion_leader_id == character_id() &&
           snapshot.cohesion_deadline_minutes == -1 &&
           snapshot.cohesion_reroutes_used == 0 && !snapshot.cohesion_assembled &&
           !snapshot.cohesion_abort_return &&
           snapshot.cargo.supply_units == 0 && snapshot.cargo.trade_value == 0;
}

bool active_outing_has_current_covert_burn_receipt(
    const bandit_live_world::active_outing_state &outing )
{
    return std::any_of( outing.observations.begin(), outing.observations.end(),
    [&outing]( const bandit_live_world::sortie_observation & observation ) {
        return observation.record_schema_version == 1 && observation.critical &&
               observation.kind == sortie_observation_kind::burn &&
               observation.sense == sortie_observation_sense::visual &&
               observation.share_state == sortie_observation_share_state::shared &&
               observation.receiver_omt == outing.selected_watch_omt &&
               observation.target_revision == outing.target_lead_revision &&
               std::find( outing.member_ids.begin(), outing.member_ids.end(),
                          observation.observer_id ) != outing.member_ids.end();
    } );
}

bool local_handoff_snapshot_matches_outing(
    const bandit_live_world::active_outing_state &outing )
{
    const bandit_live_world::local_handoff_snapshot &snapshot = outing.local_handoff;
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version < 7 ) {
        return local_handoff_snapshot_is_empty( snapshot );
    }
    if( outing.owner == simulation_owner::abstract ) {
        if( local_handoff_snapshot_is_empty( snapshot ) ) {
            return true;
        }
        if( !snapshot.is_abstract_resume() || snapshot.phase != outing.phase ||
            snapshot.cargo.supply_units != outing.cargo.supply_units ||
            snapshot.cargo.trade_value != outing.cargo.trade_value ||
            snapshot.casualty_ids != outing.casualty_ids ) {
            return false;
        }
    } else if( snapshot.cargo.supply_units != outing.cargo.supply_units ||
               snapshot.cargo.trade_value != outing.cargo.trade_value ||
               snapshot.casualty_ids != outing.casualty_ids ) {
        return false;
    }
    const tripoint_abs_omt canonical_route_position =
        snapshot.waypoint_index >= 0 &&
        snapshot.waypoint_index < static_cast<int>( outing.shared_route.size() ) ?
        outing.shared_route[static_cast<std::size_t>( snapshot.waypoint_index )] :
        tripoint_abs_omt();
    const bool physical_homeward_cursor =
        scout_phase_requires_homeward_only( snapshot.phase ) &&
        snapshot.route_position != canonical_route_position;
    if( ( outing.owner == simulation_owner::local && !snapshot.is_active() ) ||
        snapshot.activity_id != outing.activity_id ||
        snapshot.activity_generation != outing.generation ||
        snapshot.handoff_epoch != outing.handoff_epoch ||
        snapshot.committed_minutes > outing.last_advanced_minutes ||
        snapshot.waypoint_index < 0 ||
        snapshot.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ||
        ( snapshot.route_position != canonical_route_position && !physical_homeward_cursor ) ) {
        return false;
    }
    const tripoint_abs_omt expected_approach = physical_homeward_cursor ?
            snapshot.approach_from : snapshot.waypoint_index == 0 ?
            snapshot.route_position :
            outing.shared_route[static_cast<std::size_t>( snapshot.waypoint_index - 1 )];
    const tripoint_abs_omt expected_egress = physical_homeward_cursor ?
            outing.shared_route.back() :
            snapshot.waypoint_index + 1 < static_cast<int>( outing.shared_route.size() ) ?
            outing.shared_route[static_cast<std::size_t>( snapshot.waypoint_index + 1 )] :
            snapshot.route_position;
    const std::optional<int> burn_origin_distance = target_footprint_watch_distance(
                snapshot.route_position, outing.target_footprint );
    const std::optional<int> burn_egress_distance = target_footprint_watch_distance(
                snapshot.egress_omt, outing.target_footprint );
    const bool scored_burn_egress = outing.schema_version >= 10 &&
                                    active_outing_has_current_covert_burn_receipt( outing ) &&
                                    scout_phase_requires_homeward_only( snapshot.phase ) &&
                                    snapshot.route_position == outing.selected_watch_omt &&
                                    ( outing.covert_egress_attempts > 0 ||
                                      ( outing.covert_egress_attempts == 0 &&
                                        outing.covert_egress_revision == 1 &&
                                        omt_chebyshev_distance( snapshot.egress_omt,
                                                snapshot.route_position ) == 1 ) ) &&
                                    snapshot.egress_omt.z() == snapshot.route_position.z() &&
                                    burn_origin_distance && burn_egress_distance &&
                                    *burn_egress_distance >= *burn_origin_distance;
    if( snapshot.approach_from != expected_approach ||
        ( snapshot.egress_omt != expected_egress && !scored_burn_egress ) ||
        ( physical_homeward_cursor &&
          ( snapshot.route_position.z() != canonical_route_position.z() ||
            snapshot.approach_from.z() != snapshot.route_position.z() ) ) ) {
        return false;
    }
    if( snapshot.schema_version >= 3 &&
        ( std::find_if( snapshot.members.begin(), snapshot.members.end(),
    [&snapshot]( const bandit_live_world::local_handoff_member_snapshot & member ) {
        return member.npc_id == snapshot.cohesion_leader_id;
    } ) == snapshot.members.end() || snapshot.cohesion_leader_id != outing.leader_id ||
          snapshot.cohesion_deadline_minutes < -1 ||
          snapshot.cohesion_reroutes_used < 0 ||
          snapshot.cohesion_reroutes_used > local_pair_reroute_cap ||
          ( snapshot.cohesion_assembled &&
            ( snapshot.cohesion_deadline_minutes != -1 ||
              snapshot.cohesion_reroutes_used != 0 || snapshot.cohesion_abort_return ) ) ||
          ( snapshot.cohesion_abort_return &&
            snapshot.phase != scout_phase::returning_home ) ) ) {
        return false;
    }

    std::vector<character_id> snapshot_member_ids;
    std::vector<tripoint_abs_ms> entry_positions;
    std::vector<tripoint_abs_ms> staging_positions;
    std::vector<tripoint_abs_ms> living_entry_positions;
    std::vector<tripoint_abs_ms> living_staging_positions;
    std::vector<tripoint_abs_ms> living_exit_positions;
    snapshot_member_ids.reserve( snapshot.members.size() );
    entry_positions.reserve( snapshot.members.size() );
    staging_positions.reserve( snapshot.members.size() );
    living_entry_positions.reserve( snapshot.members.size() );
    living_staging_positions.reserve( snapshot.members.size() );
    living_exit_positions.reserve( snapshot.members.size() );
    for( const bandit_live_world::local_handoff_member_snapshot &member : snapshot.members ) {
        const bool health_is_valid = member.dead ? member.hp_percent == 0 :
                                     member.hp_percent > 0 && member.hp_percent <= 100;
        const bool casualty_is_recorded =
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member.npc_id ) !=
            outing.casualty_ids.end();
        if( !health_is_valid ||
            std::find( outing.member_ids.begin(), outing.member_ids.end(), member.npc_id ) ==
            outing.member_ids.end() ||
            std::find( snapshot_member_ids.begin(), snapshot_member_ids.end(), member.npc_id ) !=
            snapshot_member_ids.end() ||
            ( !member.dead &&
              project_to<coords::omt>( member.entry_position ) != snapshot.route_position &&
              !( outing.owner == simulation_owner::abstract && snapshot.is_abstract_resume() &&
                 physical_homeward_cursor ) ) ||
            std::find( entry_positions.begin(), entry_positions.end(), member.entry_position ) !=
            entry_positions.end() ||
            ( !member.dead &&
              project_to<coords::omt>( member.staging_position ) != snapshot.route_position &&
              !( outing.owner == simulation_owner::abstract && snapshot.is_abstract_resume() &&
                 physical_homeward_cursor ) ) ||
            std::find( staging_positions.begin(), staging_positions.end(),
                       member.staging_position ) != staging_positions.end() ||
            ( !member.dead && member.staging_position == member.entry_position ) ||
            ( !member.dead &&
              project_to<coords::omt>( member.exit_position ) != snapshot.route_position ) ||
            ( !member.dead &&
              std::find( living_exit_positions.begin(), living_exit_positions.end(),
                         member.exit_position ) != living_exit_positions.end() ) ||
            ( outing.owner == simulation_owner::local &&
              ( member.dead != casualty_is_recorded ||
                ( !member.dead && member.exit_position != member.entry_position ) ) ) ||
            ( outing.owner == simulation_owner::abstract &&
              member.dead != casualty_is_recorded ) ) {
            return false;
        }
        snapshot_member_ids.push_back( member.npc_id );
        entry_positions.push_back( member.entry_position );
        staging_positions.push_back( member.staging_position );
        if( !member.dead ) {
            living_entry_positions.push_back( member.entry_position );
            living_staging_positions.push_back( member.staging_position );
            living_exit_positions.push_back( member.exit_position );
        }
    }
    if( living_entry_positions.size() == 2 &&
        ( rl_dist( living_entry_positions[0], living_entry_positions[1] ) >
          local_pair_cohesion_radius_ms ||
          rl_dist( living_staging_positions[0], living_staging_positions[1] ) >
          local_pair_cohesion_radius_ms ) ) {
        return false;
    }
    if( snapshot_member_ids.size() != 2 || outing.member_ids.size() != 2 ) {
        return false;
    }
    return std::all_of( outing.member_ids.begin(), outing.member_ids.end(),
    [&snapshot_member_ids]( const character_id member_id ) {
        return std::find( snapshot_member_ids.begin(), snapshot_member_ids.end(), member_id ) !=
               snapshot_member_ids.end();
    } );
}

bool local_handoff_snapshots_equal(
    const bandit_live_world::local_handoff_snapshot &lhs,
    const bandit_live_world::local_handoff_snapshot &rhs )
{
    if( lhs.schema_version != rhs.schema_version || lhs.activity_id != rhs.activity_id ||
        lhs.activity_generation != rhs.activity_generation ||
        lhs.handoff_epoch != rhs.handoff_epoch || lhs.waypoint_index != rhs.waypoint_index ||
        lhs.phase != rhs.phase || lhs.route_position != rhs.route_position ||
        lhs.approach_from != rhs.approach_from || lhs.egress_omt != rhs.egress_omt ||
        lhs.cargo.supply_units != rhs.cargo.supply_units ||
        lhs.cargo.trade_value != rhs.cargo.trade_value ||
        lhs.casualty_ids != rhs.casualty_ids || lhs.committed_minutes != rhs.committed_minutes ||
        lhs.cohesion_leader_id != rhs.cohesion_leader_id ||
        lhs.cohesion_deadline_minutes != rhs.cohesion_deadline_minutes ||
        lhs.cohesion_reroutes_used != rhs.cohesion_reroutes_used ||
        lhs.cohesion_assembled != rhs.cohesion_assembled ||
        lhs.cohesion_abort_return != rhs.cohesion_abort_return ||
        lhs.members.size() != rhs.members.size() ) {
        return false;
    }
    for( std::size_t index = 0; index < lhs.members.size(); ++index ) {
        const bandit_live_world::local_handoff_member_snapshot &lhs_member = lhs.members[index];
        const bandit_live_world::local_handoff_member_snapshot &rhs_member = rhs.members[index];
        if( lhs_member.npc_id != rhs_member.npc_id ||
            lhs_member.prior_position != rhs_member.prior_position ||
            lhs_member.entry_position != rhs_member.entry_position ||
            lhs_member.staging_position != rhs_member.staging_position ||
            lhs_member.exit_position != rhs_member.exit_position ||
            lhs_member.hp_percent != rhs_member.hp_percent ||
            lhs_member.dead != rhs_member.dead ) {
            return false;
        }
    }
    return true;
}

bool structural_watch_omt_precedes( const tripoint_abs_omt &lhs,
                                    const tripoint_abs_omt &rhs )
{
    return std::make_tuple( lhs.z(), lhs.y(), lhs.x() ) <
           std::make_tuple( rhs.z(), rhs.y(), rhs.x() );
}

std::vector<tripoint_abs_omt> canonical_structural_target_footprint(
    std::vector<tripoint_abs_omt> footprint )
{
    std::sort( footprint.begin(), footprint.end(), structural_watch_omt_precedes );
    footprint.erase( std::unique( footprint.begin(), footprint.end() ), footprint.end() );
    return footprint;
}

bool structural_watch_route_state_is_consistent(
    const bandit_live_world::active_outing_state &outing )
{
    const bool empty_watch_state = outing.target_footprint.empty() &&
                                   outing.selected_watch_kind == structural_watch_kind::none &&
                                   outing.selected_watch_omt == tripoint_abs_omt() &&
                                   outing.selected_watch_route_cost == -1 &&
                                   outing.alternate_watch_kind == structural_watch_kind::none &&
                                   outing.alternate_watch_omt == tripoint_abs_omt() &&
                                   outing.alternate_watch_route_cost == -1 &&
                                   outing.alternate_watch_shared_route.empty() &&
                                   !outing.alternate_watch_attempted &&
                                   !outing.alternate_watch_reposition_pending;
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version < 9 ) {
        return empty_watch_state;
    }
    if( outing.schema_version > 10 || outing.target_footprint.empty() ||
        outing.target_footprint.size() > max_structural_target_footprint_omts ||
        canonical_structural_target_footprint( outing.target_footprint ) !=
        outing.target_footprint ||
        std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                   outing.target_omt ) == outing.target_footprint.end() ||
        std::any_of( outing.target_footprint.begin(), outing.target_footprint.end(),
    [&outing]( const tripoint_abs_omt & omt ) {
        return omt.z() != outing.target_omt.z();
    } ) ) {
        return false;
    }
    if( outing.selected_watch_kind == structural_watch_kind::none ) {
        return outing.selected_watch_omt == tripoint_abs_omt() &&
               outing.selected_watch_route_cost == -1 &&
               outing.alternate_watch_kind == structural_watch_kind::none &&
               outing.alternate_watch_omt == tripoint_abs_omt() &&
               outing.alternate_watch_route_cost == -1 &&
               outing.alternate_watch_shared_route.empty() &&
               !outing.alternate_watch_attempted &&
               !outing.alternate_watch_reposition_pending;
    }
    if( outing.selected_watch_route_cost < 0 ) {
        return false;
    }
    const std::optional<int> distance = target_footprint_watch_distance(
                                            outing.selected_watch_omt,
                                            outing.target_footprint );
    if( !distance ) {
        return false;
    }
    const bool selected_distance_is_valid =
        outing.selected_watch_kind == structural_watch_kind::exact ? *distance == 3 :
        outing.selected_watch_kind == structural_watch_kind::fallback &&
        *distance >= 4 && *distance <= 5;
    if( !selected_distance_is_valid ) {
        return false;
    }
    if( outing.alternate_watch_kind == structural_watch_kind::none ) {
        return outing.alternate_watch_omt == tripoint_abs_omt() &&
               outing.alternate_watch_route_cost == -1 &&
               outing.alternate_watch_shared_route.empty() &&
               !outing.alternate_watch_attempted &&
               !outing.alternate_watch_reposition_pending;
    }
    const std::optional<int> alternate_distance = target_footprint_watch_distance(
                outing.alternate_watch_omt, outing.target_footprint );
    const bool alternate_distance_is_valid = alternate_distance &&
            ( outing.alternate_watch_kind == structural_watch_kind::exact ?
              *alternate_distance == 3 :
              outing.alternate_watch_kind == structural_watch_kind::fallback &&
              *alternate_distance >= 4 && *alternate_distance <= 5 );
    const bool route_is_valid = outing.alternate_watch_omt != outing.selected_watch_omt &&
                                outing.alternate_watch_route_cost >= 0 &&
                                alternate_distance_is_valid && !outing.shared_route.empty() &&
                                bandit_live_world::structural_watch_shared_route_is_canonical(
                                    outing.alternate_watch_shared_route,
                                    outing.shared_route.front(),
                                    outing.alternate_watch_omt, outing.target_footprint );
    if( !route_is_valid || !outing.alternate_watch_reposition_pending ) {
        return route_is_valid;
    }
    return outing.schema_version == 10 && !outing.alternate_watch_attempted &&
           outing.owner == simulation_owner::local &&
           outing.phase == scout_phase::observing &&
           outing.waypoint_index == structural_outing_destination_waypoint( outing ) &&
           outing.local_handoff.is_active() &&
           outing.local_handoff.phase == scout_phase::observing &&
           outing.local_handoff.route_position == outing.selected_watch_omt &&
           outing.local_handoff.cohesion_assembled &&
           !outing.local_handoff.cohesion_abort_return && outing.casualty_ids.empty() &&
           outing.resolved_member_ids.empty() &&
           std::none_of( outing.local_handoff.members.begin(),
                         outing.local_handoff.members.end(),
    []( const bandit_live_world::local_handoff_member_snapshot & member ) {
        return member.dead;
    } );
}

bool covert_scout_egress_retry_state_is_consistent(
    const bandit_live_world::active_outing_state &outing )
{
    if( outing.schema_version < 10 ) {
        return outing.covert_egress_chain_version == 0 &&
               outing.covert_egress_attempts == 0 &&
               outing.covert_egress_revision == 0 &&
               outing.failed_covert_egress_omts.empty() &&
               outing.current_covert_egress_route_omts.empty() &&
               outing.failed_covert_egress_route_omts.empty();
    }
    if( outing.covert_egress_chain_version < 0 ||
        outing.covert_egress_chain_version > current_covert_egress_chain_version ||
        outing.covert_egress_attempts < 0 ||
        outing.covert_egress_attempts > max_covert_egress_attempts ||
        outing.covert_egress_revision < 0 ||
        outing.failed_covert_egress_omts.size() >
        static_cast<std::size_t>( outing.covert_egress_attempts ) ||
        outing.failed_covert_egress_omts.size() >
        static_cast<std::size_t>( max_covert_egress_attempts ) ||
        outing.current_covert_egress_route_omts.size() > max_covert_egress_route_omts ||
        outing.failed_covert_egress_route_omts.size() >
        max_failed_covert_egress_route_omts ) {
        return false;
    }
    const bool has_burn = active_outing_has_current_covert_burn_receipt( outing );
    if( outing.phase == scout_phase::burned_withdrawal && !has_burn ) {
        return false;
    }
    if( !has_burn ) {
        return outing.covert_egress_chain_version == 0 &&
               outing.covert_egress_attempts == 0 &&
               outing.covert_egress_revision == 0 &&
               outing.failed_covert_egress_omts.empty() &&
               outing.current_covert_egress_route_omts.empty() &&
               outing.failed_covert_egress_route_omts.empty();
    }
    if( outing.phase == scout_phase::burned_withdrawal &&
        ( outing.covert_egress_attempts < 1 || outing.covert_egress_revision < 1 ||
          outing.failed_covert_egress_omts.size() + 1 !=
          static_cast<std::size_t>( outing.covert_egress_attempts ) ) ) {
        return false;
    }
    if( outing.covert_egress_revision !=
        static_cast<int>( outing.failed_covert_egress_omts.size() ) + 1 ) {
        return false;
    }
    const std::optional<int> origin_distance = target_footprint_watch_distance(
                outing.selected_watch_omt, outing.target_footprint );
    if( !origin_distance ) {
        return false;
    }
    const auto route_footprint_is_valid = [&](
        const std::vector<tripoint_abs_omt> &footprint ) {
        std::vector<tripoint_abs_omt> seen;
        for( const tripoint_abs_omt &omt : footprint ) {
            const std::optional<int> distance = target_footprint_watch_distance(
                                                    omt, outing.target_footprint );
            if( omt.is_invalid() || omt.z() != outing.selected_watch_omt.z() ||
                !origin_distance || !distance || *distance < *origin_distance ||
                std::find( seen.begin(), seen.end(), omt ) != seen.end() ) {
                return false;
            }
            seen.push_back( omt );
        }
        return true;
    };
    if( !route_footprint_is_valid( outing.current_covert_egress_route_omts ) ||
        !route_footprint_is_valid( outing.failed_covert_egress_route_omts ) ||
        ( outing.phase == scout_phase::burned_withdrawal &&
          std::any_of( outing.current_covert_egress_route_omts.begin(),
                       outing.current_covert_egress_route_omts.end(),
        [&outing]( const tripoint_abs_omt &omt ) {
            return std::find( outing.failed_covert_egress_route_omts.begin(),
                              outing.failed_covert_egress_route_omts.end(), omt ) !=
                   outing.failed_covert_egress_route_omts.end();
        } ) ) ) {
        return false;
    }
    std::vector<tripoint_abs_omt> seen;
    if( outing.covert_egress_chain_version == 0 ) {
        for( const tripoint_abs_omt &failed : outing.failed_covert_egress_omts ) {
            const std::optional<int> failed_distance = target_footprint_watch_distance(
                        failed, outing.target_footprint );
            if( failed.is_invalid() || failed.z() != outing.selected_watch_omt.z() ||
                omt_chebyshev_distance( failed, outing.selected_watch_omt ) != 1 ||
                !failed_distance || *failed_distance < *origin_distance ||
                std::find( seen.begin(), seen.end(), failed ) != seen.end() ) {
                return false;
            }
            seen.push_back( failed );
        }
        if( outing.covert_egress_attempts == 0 ) {
            return outing.failed_covert_egress_omts.empty();
        }
        const bool current_attempt_pending = outing.failed_covert_egress_omts.size() + 1 ==
                                             static_cast<std::size_t>(
                                                 outing.covert_egress_attempts );
        const bool every_attempt_failed = outing.failed_covert_egress_omts.size() ==
                                          static_cast<std::size_t>(
                                              outing.covert_egress_attempts );
        if( !current_attempt_pending && !every_attempt_failed ) {
            return false;
        }
        if( outing.phase != scout_phase::burned_withdrawal ) {
            return true;
        }
        if( every_attempt_failed ) {
            return !seen.empty() && outing.current_covert_egress_route_omts.empty() &&
                   outing.local_handoff.egress_omt == seen.back();
        }
        const std::optional<int> current_distance = target_footprint_watch_distance(
                    outing.local_handoff.egress_omt, outing.target_footprint );
        return current_distance && *current_distance >= *origin_distance &&
               omt_chebyshev_distance( outing.local_handoff.egress_omt,
                                       outing.selected_watch_omt ) == 1 &&
               std::find( seen.begin(), seen.end(), outing.local_handoff.egress_omt ) ==
               seen.end();
    }
    tripoint_abs_omt previous_endpoint = outing.selected_watch_omt;
    int previous_distance = *origin_distance;
    for( const tripoint_abs_omt &failed : outing.failed_covert_egress_omts ) {
        const std::optional<int> failed_distance = target_footprint_watch_distance(
                    failed, outing.target_footprint );
        if( failed.is_invalid() || failed.z() != outing.selected_watch_omt.z() ||
            omt_chebyshev_distance( failed, previous_endpoint ) != 1 ||
            !failed_distance || *failed_distance < previous_distance ||
            ( !seen.empty() && *failed_distance <= previous_distance ) ||
            std::find( seen.begin(), seen.end(), failed ) != seen.end() ) {
            return false;
        }
        seen.push_back( failed );
        previous_endpoint = failed;
        previous_distance = *failed_distance;
    }
    if( outing.covert_egress_attempts == 0 ) {
        return outing.failed_covert_egress_omts.empty();
    }
    const bool current_attempt_pending = outing.failed_covert_egress_omts.size() + 1 ==
                                         static_cast<std::size_t>(
                                             outing.covert_egress_attempts );
    const bool every_attempt_failed = outing.failed_covert_egress_omts.size() ==
                                      static_cast<std::size_t>(
                                          outing.covert_egress_attempts );
    if( !current_attempt_pending && !every_attempt_failed ) {
        return false;
    }
    if( outing.phase != scout_phase::burned_withdrawal ) {
        return true;
    }
    if( every_attempt_failed ) {
        return !seen.empty() && outing.current_covert_egress_route_omts.empty() &&
               outing.local_handoff.egress_omt == seen.back();
    }
    const std::optional<int> current_distance = target_footprint_watch_distance(
                outing.local_handoff.egress_omt, outing.target_footprint );
    if( !current_distance ||
        omt_chebyshev_distance( outing.local_handoff.egress_omt, previous_endpoint ) != 1 ||
        *current_distance < previous_distance ||
        ( !seen.empty() && *current_distance <= previous_distance ) ||
        std::find( seen.begin(), seen.end(), outing.local_handoff.egress_omt ) != seen.end() ) {
        return false;
    }
    // This footprint is an unordered union of member paths.  A failure can occur before a member
    // reaches the persisted endpoint, so the union may legitimately begin inside that endpoint's
    // ring.  The persisted owner enforces the original watch floor above; the ordered live path is
    // separately required to move monotonically outward from each member's physical start.
    return true;
}

bool simulation_owner_state_is_consistent(
    const bandit_live_world::active_outing_state &outing )
{
    const bool owner_matches_epoch =
        ( outing.owner == simulation_owner::abstract && outing.handoff_epoch % 2 == 0 ) ||
        ( outing.owner == simulation_owner::local && outing.handoff_epoch % 2 == 1 );
    const int minimum_advanced_minutes = std::max( { outing.started_minutes,
            outing.local_contact_minutes, outing.last_progress_minutes } );
    return outing.handoff_epoch >= 0 && owner_matches_epoch &&
           outing.last_advanced_minutes >= minimum_advanced_minutes &&
           local_handoff_snapshot_matches_outing( outing ) &&
           structural_watch_route_state_is_consistent( outing ) &&
           covert_scout_egress_retry_state_is_consistent( outing );
}

bool current_serialized_owner_fields_are_consistent( JsonObject owner_json )
{
    owner_json.allow_omitted_members();
    if( !owner_json.has_member( "schema_version" ) ||
        !owner_json.has_member( "simulation_owner" ) ||
        !owner_json.has_member( "handoff_epoch" ) ||
        !owner_json.has_member( "last_advanced_minutes" ) ||
        !owner_json.has_member( "started_minutes" ) ||
        !owner_json.has_member( "local_contact_minutes" ) ||
        !owner_json.has_member( "last_progress_minutes" ) ) {
        return false;
    }

    int schema_version = 0;
    std::string kind;
    std::string owner;
    int handoff_epoch = -1;
    int last_advanced_minutes = -1;
    int started_minutes = -1;
    int local_contact_minutes = -1;
    int last_progress_minutes = -1;
    owner_json.read( "schema_version", schema_version );
    owner_json.read( "kind", kind );
    owner_json.read( "simulation_owner", owner );
    owner_json.read( "handoff_epoch", handoff_epoch );
    owner_json.read( "last_advanced_minutes", last_advanced_minutes );
    owner_json.read( "started_minutes", started_minutes );
    owner_json.read( "local_contact_minutes", local_contact_minutes );
    owner_json.read( "last_progress_minutes", last_progress_minutes );
    const bool owner_matches_epoch =
        ( owner == "abstract" && handoff_epoch % 2 == 0 ) ||
        ( owner == "local" && handoff_epoch % 2 == 1 );
    const bool supported_schema = schema_version == 5 ||
                                  ( ( schema_version == 6 || schema_version == 7 ||
                                      schema_version == 8 || schema_version == 9 ||
                                      schema_version == 10 ) &&
                                    kind == "structural_sortie" );
    const bool complete_structural_route = schema_version < 6 ||
            ( owner_json.has_member( "shared_route" ) &&
              owner_json.has_member( "waypoint_index" ) &&
              owner_json.has_member( "expected_return_minutes" ) &&
              owner_json.has_member( "missing_deadline_minutes" ) );
    const bool complete_local_handoff = schema_version < 7 ||
                                        owner_json.has_member( "local_handoff" );
    const bool complete_abstract_encounter = schema_version < 8 ||
            ( owner_json.has_member( "abstract_encounter" ) &&
              owner_json.has_member( "abstract_detour_attempts" ) &&
              owner_json.has_member( "has_withdrawal_detour" ) &&
              owner_json.has_member( "withdrawal_detour_omt" ) );
    const bool complete_watch_route = schema_version < 9 ||
            ( owner_json.has_member( "target_footprint" ) &&
              owner_json.has_member( "selected_watch_kind" ) &&
              owner_json.has_member( "selected_watch_omt" ) &&
              owner_json.has_member( "selected_watch_route_cost" ) );
    return supported_schema && complete_structural_route && complete_local_handoff &&
           complete_abstract_encounter && complete_watch_route &&
           handoff_epoch >= 0 && owner_matches_epoch &&
           last_advanced_minutes >= std::max( { started_minutes, local_contact_minutes,
                   last_progress_minutes } );
}

bool simulation_cursor_matches(
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::simulation_advance_cursor &cursor )
{
    return outing.is_active() && simulation_owner_state_is_consistent( outing ) &&
           outing.activity_id == cursor.activity_id &&
           outing.generation == cursor.generation && outing.owner == cursor.owner &&
           outing.handoff_epoch == cursor.handoff_epoch &&
           outing.last_advanced_minutes == cursor.last_advanced_minutes &&
           outing.covert_egress_revision == cursor.covert_egress_revision;
}

void normalize_legacy_simulation_owner_state(
    bandit_live_world::active_outing_state &outing )
{
    if( !outing.is_active() || outing.schema_version >= 4 ) {
        return;
    }
    outing.handoff_epoch = std::max( 0, outing.handoff_epoch );
    const bool parity_matches =
        ( outing.owner == simulation_owner::abstract && outing.handoff_epoch % 2 == 0 ) ||
        ( outing.owner == simulation_owner::local && outing.handoff_epoch % 2 == 1 );
    if( !parity_matches ) {
        if( outing.handoff_epoch < std::numeric_limits<int>::max() ) {
            outing.handoff_epoch++;
        } else {
            outing.handoff_epoch--;
        }
    }
    outing.last_advanced_minutes = std::max( { outing.last_advanced_minutes,
            outing.started_minutes, outing.local_contact_minutes,
            outing.last_progress_minutes } );
    outing.schema_version = 4;
}

bool camp_decision_allows_dispatch( const bandit_live_world::camp_decision_record &decision,
                                    const bandit_dry_run::job_template job )
{
    if( job == bandit_dry_run::job_template::scout ) {
        return decision.state == camp_decision_state::idle ||
               decision.state == camp_decision_state::abandoned;
    }
    return decision.state == camp_decision_state::preparing_follow_on;
}
} // namespace

namespace bandit_live_world
{
static void record_scout_phase_transition_event( const active_outing_state &outing,
        scout_phase previous_phase, scout_phase next_phase,
        std::string_view reason, int current_minutes );

bool upsert_camp_map_lead( site_record &site, camp_map_lead lead )
{
    return upsert_camp_map_lead_transaction( site, std::move( lead ) );
}

std::string to_string( anchor_source_kind source_kind )
{
    switch( source_kind ) {
        case anchor_source_kind::none:
            return "none";
        case anchor_source_kind::overmap_special:
            return "overmap_special";
        case anchor_source_kind::map_extra:
            return "map_extra";
    }

    return "none";
}

std::string to_string( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::none:
            return "none";
        case owned_site_kind::bandit_camp:
            return "bandit_camp";
        case owned_site_kind::bandit_work_camp:
            return "bandit_work_camp";
        case owned_site_kind::bandit_cabin:
            return "bandit_cabin";
        case owned_site_kind::cannibal_camp:
            return "cannibal_camp";
        case owned_site_kind::looters:
            return "looters";
        case owned_site_kind::bandits_block:
            return "bandits_block";
    }

    return "none";
}

std::string to_string( hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::none:
            return "none";
        case hostile_site_profile::camp_style:
            return "camp_style";
        case hostile_site_profile::cannibal_camp:
            return "cannibal_camp";
        case hostile_site_profile::small_hostile_site:
            return "small_hostile_site";
    }

    return "none";
}

std::string to_string( member_state state )
{
    switch( state ) {
        case member_state::at_home:
            return "at_home";
        case member_state::outbound:
            return "outbound";
        case member_state::local_contact:
            return "local_contact";
        case member_state::orphaned:
            return "orphaned";
        case member_state::dead:
            return "dead";
        case member_state::missing:
            return "missing";
    }

    return "at_home";
}

std::string to_string( origin_disposition disposition )
{
    switch( disposition ) {
        case origin_disposition::active_hostile:
            return "active_hostile";
        case origin_disposition::captured_non_hostile:
            return "captured_non_hostile";
        case origin_disposition::deleted:
            return "deleted";
        case origin_disposition::invalidated:
            return "invalidated";
    }

    return "active_hostile";
}

std::string to_string( active_member_observation_state state )
{
    switch( state ) {
        case active_member_observation_state::unresolved:
            return "unresolved";
        case active_member_observation_state::local_contact:
            return "local_contact";
        case active_member_observation_state::returning_home:
            return "returning_home";
        case active_member_observation_state::home:
            return "home";
        case active_member_observation_state::dead:
            return "dead";
        case active_member_observation_state::missing:
            return "missing";
    }

    return "unresolved";
}

std::string to_string( local_gate_posture posture )
{
    switch( posture ) {
        case local_gate_posture::stalk:
            return "stalk";
        case local_gate_posture::hold_off:
            return "hold_off";
        case local_gate_posture::probe:
            return "probe";
        case local_gate_posture::open_shakedown:
            return "open_shakedown";
        case local_gate_posture::attack_now:
            return "attack_now";
        case local_gate_posture::abort:
            return "abort";
    }

    return "abort";
}

std::string to_string( camp_lead_kind kind )
{
    switch( kind ) {
        case camp_lead_kind::structural_bounty:
            return "structural_bounty";
        case camp_lead_kind::terrain_opportunity:
            return "terrain_opportunity";
        case camp_lead_kind::harvested_site:
            return "harvested_site";
        case camp_lead_kind::human_activity:
            return "human_activity";
        case camp_lead_kind::basecamp_activity:
            return "basecamp_activity";
        case camp_lead_kind::moving_actor:
            return "moving_actor";
        case camp_lead_kind::route_activity:
            return "route_activity";
        case camp_lead_kind::smoke_signal:
            return "smoke_signal";
        case camp_lead_kind::light_signal:
            return "light_signal";
        case camp_lead_kind::sound_signal:
            return "sound_signal";
        case camp_lead_kind::threat_memory:
            return "threat_memory";
        case camp_lead_kind::loss_site:
            return "loss_site";
        case camp_lead_kind::false_lead:
            return "false_lead";
        case camp_lead_kind::frontier_probe:
            return "frontier_probe";
    }

    return "human_activity";
}

std::string to_string( camp_lead_origin origin )
{
    switch( origin ) {
        case camp_lead_origin::legacy_radar:
            return "legacy_radar";
        case camp_lead_origin::observer:
            return "observer";
        case camp_lead_origin::signal:
            return "signal";
        case camp_lead_origin::returned_report:
            return "returned_report";
        case camp_lead_origin::structural_routine:
            return "structural_routine";
    }
    return "legacy_radar";
}

std::string to_string( camp_lead_status status )
{
    switch( status ) {
        case camp_lead_status::suspected:
            return "suspected";
        case camp_lead_status::scout_confirmed:
            return "scout_confirmed";
        case camp_lead_status::active:
            return "active";
        case camp_lead_status::harvested:
            return "harvested";
        case camp_lead_status::stale:
            return "stale";
        case camp_lead_status::invalidated:
            return "invalidated";
        case camp_lead_status::dangerous:
            return "dangerous";
    }

    return "suspected";
}

std::string to_string( outing_kind kind )
{
    switch( kind ) {
        case outing_kind::none:
            return "none";
        case outing_kind::scout_sortie:
            return "scout_sortie";
        case outing_kind::hostile_operation:
            return "hostile_operation";
        case outing_kind::structural_sortie:
            return "structural_sortie";
    }

    return "none";
}

std::string to_string( structural_watch_kind kind )
{
    switch( kind ) {
        case structural_watch_kind::none:
            return "none";
        case structural_watch_kind::exact:
            return "exact";
        case structural_watch_kind::fallback:
            return "fallback";
    }

    return "none";
}

std::string to_string( simulation_owner owner )
{
    switch( owner ) {
        case simulation_owner::abstract:
            return "abstract";
        case simulation_owner::local:
            return "local";
    }

    return "abstract";
}

std::string to_string( scout_phase phase )
{
    switch( phase ) {
        case scout_phase::assembling:
            return "assembling";
        case scout_phase::outbound:
            return "outbound";
        case scout_phase::searching:
            return "searching";
        case scout_phase::observing:
            return "observing";
        case scout_phase::harvesting:
            return "harvesting";
        case scout_phase::burned_withdrawal:
            return "burned_withdrawal";
        case scout_phase::returning_exposed:
            return "returning_exposed";
        case scout_phase::returning_report:
            return "returning_report";
        case scout_phase::returning_home:
            return "returning_home";
        case scout_phase::lost:
            return "lost";
    }

    return "assembling";
}

std::string to_string( sortie_observation_kind kind )
{
    switch( kind ) {
        case sortie_observation_kind::routine:
            return "routine";
        case sortie_observation_kind::certainty:
            return "certainty";
        case sortie_observation_kind::bounds:
            return "bounds";
        case sortie_observation_kind::route_state:
            return "route_state";
        case sortie_observation_kind::alert:
            return "alert";
        case sortie_observation_kind::target_revision:
            return "target_revision";
        case sortie_observation_kind::hard_danger:
            return "hard_danger";
        case sortie_observation_kind::contradiction:
            return "contradiction";
        case sortie_observation_kind::casualty:
            return "casualty";
        case sortie_observation_kind::burn:
            return "burn";
    }
    return "routine";
}

std::string to_string( camp_decision_state state )
{
    switch( state ) {
        case camp_decision_state::idle:
            return "idle";
        case camp_decision_state::report_awaiting_assessment:
            return "report_awaiting_assessment";
        case camp_decision_state::preparing_follow_on:
            return "preparing_follow_on";
        case camp_decision_state::cooldown:
            return "cooldown";
        case camp_decision_state::abandoned:
            return "abandoned";
    }

    return "abandoned";
}

std::string to_string( camp_report_policy policy )
{
    switch( policy ) {
        case camp_report_policy::none:
            return "none";
        case camp_report_policy::bandit_shakedown:
            return "bandit_shakedown";
        case camp_report_policy::cannibal_night_raid:
            return "cannibal_night_raid";
    }
    return "none";
}

std::string to_string( hostile_operation_kind kind )
{
    switch( kind ) {
        case hostile_operation_kind::none:
            return "none";
        case hostile_operation_kind::shakedown:
            return "shakedown";
        case hostile_operation_kind::raid:
            return "raid";
    }

    return "none";
}

std::string to_string( hostile_operation_phase phase )
{
    switch( phase ) {
        case hostile_operation_phase::assembling:
            return "assembling";
        case hostile_operation_phase::outbound:
            return "outbound";
        case hostile_operation_phase::rallying:
            return "rallying";
        case hostile_operation_phase::waiting_night:
            return "waiting_night";
        case hostile_operation_phase::approaching:
            return "approaching";
        case hostile_operation_phase::committed_contact:
            return "committed_contact";
        case hostile_operation_phase::returning_home:
            return "returning_home";
        case hostile_operation_phase::lost:
            return "lost";
    }

    return "lost";
}

bool is_valid_scout_phase_transition( const scout_phase previous_phase,
                                      const scout_phase next_phase )
{
    if( previous_phase == next_phase ) {
        return true;
    }
    if( next_phase == scout_phase::lost ) {
        return previous_phase != scout_phase::lost;
    }

    switch( previous_phase ) {
        case scout_phase::assembling:
            return next_phase == scout_phase::outbound;
        case scout_phase::outbound:
            return next_phase == scout_phase::searching ||
                   next_phase == scout_phase::observing ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::searching:
            return next_phase == scout_phase::observing ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::observing:
            return next_phase == scout_phase::harvesting ||
                   next_phase == scout_phase::burned_withdrawal ||
                   next_phase == scout_phase::returning_exposed ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::harvesting:
            return next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::burned_withdrawal:
            return next_phase == scout_phase::returning_exposed ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::returning_exposed:
            return next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::returning_report:
            return next_phase == scout_phase::returning_home;
        case scout_phase::returning_home:
        case scout_phase::lost:
            return false;
    }

    return false;
}

simulation_owner_transition_result transition_external_simulation_owner(
    site_record &site, const std::string &expected_activity_id,
    const int expected_generation, const simulation_owner expected_owner,
    const simulation_owner next_owner, const int expected_handoff_epoch,
    const int expected_last_advanced_minutes, const int current_minutes )
{
    const active_outing_state *current = site.active_external_outing();
    if( current == nullptr || !current->is_active() ||
        !simulation_owner_state_is_consistent( *current ) ||
        current->activity_id != expected_activity_id ||
        current->generation != expected_generation || current->owner != expected_owner ||
        current->handoff_epoch != expected_handoff_epoch || expected_handoff_epoch < 0 ||
        current->last_advanced_minutes != expected_last_advanced_minutes ||
        current_minutes < 0 || current_minutes < current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }
    if( expected_owner == next_owner ) {
        return simulation_owner_transition_result::unchanged;
    }
    if( current->kind == outing_kind::structural_sortie && current->schema_version >= 7 ) {
        return simulation_owner_transition_result::rejected;
    }
    if( current->handoff_epoch == std::numeric_limits<int>::max() ) {
        return simulation_owner_transition_result::rejected;
    }

    site_record candidate = site;
    active_outing_state *next = candidate.active_external_outing();
    if( next == nullptr ) {
        return simulation_owner_transition_result::rejected;
    }
    next->owner = next_owner;
    next->handoff_epoch++;
    next->last_advanced_minutes = current_minutes;
    if( next_owner == simulation_owner::abstract ) {
        next->local_handoff.clear();
    }
    site = std::move( candidate );
    return simulation_owner_transition_result::applied;
}

static void consume_local_pair_resume_receipt( active_outing_state &outing )
{
    const bool retains_physical_covert_egress =
        outing.phase == scout_phase::burned_withdrawal ||
        outing.phase == scout_phase::returning_exposed ||
        outing.phase == scout_phase::returning_report;
    if( outing.kind == outing_kind::structural_sortie && outing.schema_version >= 7 &&
        outing.owner == simulation_owner::abstract && outing.local_handoff.is_abstract_resume() &&
        !retains_physical_covert_egress ) {
        const tripoint_abs_omt canonical_route_position =
            outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
        if( outing.local_handoff.route_position == canonical_route_position ) {
            outing.local_handoff.clear();
        }
    }
}

static bool hostile_site_contains_omt( const site_record &site,
                                       const tripoint_abs_omt &omt )
{
    return omt == site.anchor ||
           std::find( site.footprint.begin(), site.footprint.end(), omt ) != site.footprint.end();
}

simulation_owner_transition_result advance_external_simulation(
    site_record &site, const std::string &expected_activity_id,
    const int expected_generation, const simulation_owner expected_owner,
    const int expected_handoff_epoch, const int expected_last_advanced_minutes,
    const int expected_covert_egress_revision, const int current_minutes )
{
    const active_outing_state *current = site.active_external_outing();
    if( current == nullptr || !current->is_active() ||
        !simulation_owner_state_is_consistent( *current ) ||
        current->activity_id != expected_activity_id ||
        current->generation != expected_generation || current->owner != expected_owner ||
        current->handoff_epoch != expected_handoff_epoch || expected_handoff_epoch < 0 ||
        current->last_advanced_minutes != expected_last_advanced_minutes ||
        current->covert_egress_revision != expected_covert_egress_revision ||
        current_minutes < 0 || current_minutes < current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }
    if( current_minutes == current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }

    site_record candidate = site;
    active_outing_state *next = candidate.active_external_outing();
    if( next == nullptr ) {
        return simulation_owner_transition_result::rejected;
    }
    next->last_advanced_minutes = current_minutes;
    consume_local_pair_resume_receipt( *next );
    site = std::move( candidate );
    return simulation_owner_transition_result::applied;
}

local_handoff_plan plan_local_pair_handoff( const site_record &site,
        const simulation_advance_cursor &expected_cursor, const int current_minutes,
        const std::vector<local_handoff_member_read> &member_reads )
{
    local_handoff_plan plan;
    plan.expected_cursor = expected_cursor;
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 6 || outing.owner != simulation_owner::abstract ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        current_minutes < 0 || current_minutes < outing.last_advanced_minutes ||
        outing.handoff_epoch == std::numeric_limits<int>::max() ||
        outing.phase == scout_phase::assembling || outing.phase == scout_phase::lost ||
        outing.shared_route.empty() || outing.waypoint_index < 0 ||
        outing.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ) {
        plan.notes.push_back( "local handoff blocked: stale or unsupported abstract owner" );
        return plan;
    }
    const bool resumes_physical_homeward_cursor = outing.local_handoff.is_abstract_resume();
    if( resumes_physical_homeward_cursor &&
        ( !scout_phase_requires_homeward_only( outing.phase ) ||
          current_minutes <= outing.local_handoff.committed_minutes ) ) {
        plan.notes.push_back( "local handoff blocked: physical resume cursor has not advanced" );
        return plan;
    }

    std::vector<character_id> surviving_member_ids;
    for( const character_id &member_id : outing.member_ids ) {
        if( outing.member_is_resolved( member_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
            outing.casualty_ids.end() ) {
            continue;
        }
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::outbound ) {
            plan.notes.push_back( "local handoff blocked: surviving reservation is not outbound" );
            return plan;
        }
        surviving_member_ids.push_back( member_id );
    }
    if( surviving_member_ids.empty() || surviving_member_ids.size() > 2 ||
        member_reads.size() != surviving_member_ids.size() ) {
        plan.notes.push_back( "local handoff blocked: complete surviving pair is unavailable" );
        return plan;
    }

    local_handoff_snapshot snapshot;
    snapshot.activity_id = outing.activity_id;
    snapshot.activity_generation = outing.generation;
    snapshot.handoff_epoch = outing.handoff_epoch + 1;
    snapshot.waypoint_index = outing.waypoint_index;
    snapshot.phase = outing.phase;
    snapshot.route_position = resumes_physical_homeward_cursor ?
                              outing.local_handoff.route_position :
                              outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
    snapshot.approach_from = resumes_physical_homeward_cursor ?
                             outing.local_handoff.approach_from :
                             outing.waypoint_index == 0 ? snapshot.route_position :
                             outing.shared_route[static_cast<std::size_t>( outing.waypoint_index - 1 )];
    snapshot.egress_omt = resumes_physical_homeward_cursor ? outing.local_handoff.egress_omt :
                          outing.waypoint_index + 1 < static_cast<int>( outing.shared_route.size() ) ?
                          outing.shared_route[static_cast<std::size_t>( outing.waypoint_index + 1 )] :
                          snapshot.route_position;
    snapshot.cargo = outing.cargo;
    snapshot.casualty_ids = outing.casualty_ids;
    snapshot.cohesion_leader_id = std::find( surviving_member_ids.begin(),
                                  surviving_member_ids.end(), outing.leader_id ) !=
                                  surviving_member_ids.end() ? outing.leader_id :
                                  surviving_member_ids.front();
    snapshot.committed_minutes = current_minutes;

    std::vector<tripoint_abs_ms> entry_positions;
    std::vector<tripoint_abs_ms> staging_positions;
    for( const character_id &member_id : outing.member_ids ) {
        if( std::find( surviving_member_ids.begin(), surviving_member_ids.end(), member_id ) ==
            surviving_member_ids.end() ) {
            if( !resumes_physical_homeward_cursor ) {
                plan.notes.push_back(
                    "local handoff blocked: casualty has no physical resume snapshot" );
                return plan;
            }
            const auto casualty_snapshot = std::find_if(
                    outing.local_handoff.members.begin(), outing.local_handoff.members.end(),
            [&member_id]( const local_handoff_member_snapshot & candidate ) {
                return candidate.npc_id == member_id && candidate.dead;
            } );
            if( casualty_snapshot == outing.local_handoff.members.end() ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) ==
                outing.casualty_ids.end() ) {
                plan.notes.push_back(
                    "local handoff blocked: casualty resume state is contradictory" );
                return plan;
            }
            snapshot.members.push_back( *casualty_snapshot );
            continue;
        }
        const auto read_iter = std::find_if( member_reads.begin(), member_reads.end(),
        [&member_id]( const local_handoff_member_read & read ) {
            return read.npc_id == member_id;
        } );
        if( read_iter == member_reads.end() || !read_iter->bindable || read_iter->dead ||
            read_iter->hp_percent <= 0 || read_iter->hp_percent > 100 ||
            project_to<coords::omt>( read_iter->entry_position ) != snapshot.route_position ||
            std::find( entry_positions.begin(), entry_positions.end(),
                       read_iter->entry_position ) != entry_positions.end() ||
            project_to<coords::omt>( read_iter->staging_position ) != snapshot.route_position ||
            std::find( staging_positions.begin(), staging_positions.end(),
                       read_iter->staging_position ) != staging_positions.end() ||
            read_iter->staging_position == read_iter->entry_position ) {
            plan.notes.push_back( "local handoff blocked: member preflight or entry edge is invalid" );
            return plan;
        }
        local_handoff_member_snapshot member_snapshot;
        member_snapshot.npc_id = member_id;
        member_snapshot.prior_position = read_iter->current_position;
        member_snapshot.entry_position = read_iter->entry_position;
        member_snapshot.staging_position = read_iter->staging_position;
        member_snapshot.exit_position = read_iter->entry_position;
        member_snapshot.hp_percent = read_iter->hp_percent;
        snapshot.members.push_back( member_snapshot );
        entry_positions.push_back( read_iter->entry_position );
        staging_positions.push_back( read_iter->staging_position );
    }
    if( entry_positions.size() == 2 &&
        ( rl_dist( entry_positions[0], entry_positions[1] ) > local_pair_cohesion_radius_ms ||
          rl_dist( staging_positions[0], staging_positions[1] ) >
          local_pair_cohesion_radius_ms ) ) {
        plan.notes.push_back( "local handoff blocked: pair slots are outside cohesion radius" );
        return plan;
    }
    plan.snapshot = std::move( snapshot );
    plan.valid = true;
    plan.notes.push_back( "local handoff preflight captured the complete surviving pair" );
    return plan;
}

local_handoff_commit_result commit_local_pair_handoff( site_record &site,
        const local_handoff_plan &plan,
        const std::function<bool( const local_handoff_member_snapshot & )> &bind_member,
        const std::function<void( const local_handoff_member_snapshot & )> &rollback_member )
{
    if( !plan.valid || !plan.snapshot.is_active() || !bind_member || !rollback_member ) {
        return local_handoff_commit_result::rejected;
    }
    const active_outing_state *current = site.active_external_outing();
    if( current == nullptr || !current->is_active() ) {
        return local_handoff_commit_result::rejected;
    }
    if( current->owner == simulation_owner::local ) {
        return local_handoff_snapshots_equal( current->local_handoff, plan.snapshot ) ?
               local_handoff_commit_result::unchanged :
               local_handoff_commit_result::rejected;
    }
    if( current->kind != outing_kind::structural_sortie ||
        !simulation_cursor_matches( *current, plan.expected_cursor ) ||
        current->handoff_epoch == std::numeric_limits<int>::max() ||
        plan.snapshot.handoff_epoch != current->handoff_epoch + 1 ||
        plan.snapshot.committed_minutes < current->last_advanced_minutes ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    next.schema_version = std::max( next.schema_version, 8 );
    next.owner = simulation_owner::local;
    next.handoff_epoch = plan.snapshot.handoff_epoch;
    next.last_advanced_minutes = plan.snapshot.committed_minutes;
    next.last_progress_minutes = std::max( next.last_progress_minutes,
                                          plan.snapshot.committed_minutes );
    next.local_handoff = plan.snapshot;
    next.leader_id = next.local_handoff.cohesion_leader_id;
    if( next.abstract_encounter.active &&
        next.abstract_encounter.overlap_omt == plan.snapshot.route_position ) {
        if( next.abstract_encounter.outcome_applied ) {
            return local_handoff_commit_result::rejected;
        }
        next.abstract_encounter.local_claimed = true;
    }
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return local_handoff_commit_result::rejected;
    }

    std::vector<const local_handoff_member_snapshot *> bound_members;
    const auto rollback_bound_members = [&bound_members, &rollback_member]() {
        for( auto iter = bound_members.rbegin(); iter != bound_members.rend(); ++iter ) {
            try {
                rollback_member( **iter );
            } catch( ... ) {
                // Keep rolling back the rest of the complete pair.
            }
        }
    };
    try {
        for( const local_handoff_member_snapshot &member : plan.snapshot.members ) {
            bound_members.push_back( &member );
            if( !bind_member( member ) ) {
                rollback_bound_members();
                return local_handoff_commit_result::rolled_back;
            }
        }
    } catch( ... ) {
        rollback_bound_members();
        return local_handoff_commit_result::rolled_back;
    }

    site = std::move( candidate );
    return local_handoff_commit_result::applied;
}

local_dematerialization_plan plan_local_pair_dematerialization( const site_record &site,
        const simulation_advance_cursor &expected_cursor, const int current_minutes,
        const std::vector<local_dematerialization_member_read> &member_reads,
        const sortie_cargo &cargo )
{
    local_dematerialization_plan plan;
    plan.expected_cursor = expected_cursor;
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 7 || outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        !outing.local_handoff.is_active() || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ||
        outing.handoff_epoch == std::numeric_limits<int>::max() ||
        outing.member_ids.size() != 2 || outing.local_handoff.members.size() != 2 ||
        member_reads.size() != 2 || cargo.supply_units < 0 || cargo.trade_value < 0 ||
        outing.local_handoff.schema_version < 3 ||
        ( !outing.local_handoff.cohesion_assembled &&
          !outing.local_handoff.cohesion_abort_return && outing.phase != scout_phase::lost ) ||
        outing.shared_route.empty() || outing.waypoint_index < 0 ||
        outing.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ) {
        plan.notes.push_back( "local dematerialization blocked: stale or incomplete local pair" );
        return plan;
    }

    local_handoff_snapshot snapshot = outing.local_handoff;
    const tripoint_abs_omt prior_local_route_position = snapshot.route_position;
    const tripoint_abs_omt prior_local_approach = snapshot.approach_from;
    const tripoint_abs_omt prior_local_egress = snapshot.egress_omt;
    snapshot.schema_version = 3;
    snapshot.handoff_epoch = outing.handoff_epoch + 1;
    snapshot.waypoint_index = outing.waypoint_index;
    snapshot.phase = outing.phase;
    snapshot.route_position = outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
    snapshot.approach_from = outing.waypoint_index == 0 ? snapshot.route_position :
                             outing.shared_route[static_cast<std::size_t>( outing.waypoint_index - 1 )];
    snapshot.egress_omt = outing.phase == scout_phase::burned_withdrawal ? prior_local_egress :
                          outing.waypoint_index + 1 < static_cast<int>( outing.shared_route.size() ) ?
                          outing.shared_route[static_cast<std::size_t>( outing.waypoint_index + 1 )] :
                          snapshot.route_position;
    snapshot.cargo = cargo;
    snapshot.casualty_ids = outing.casualty_ids;
    snapshot.committed_minutes = current_minutes;

    std::vector<character_id> read_member_ids;
    std::vector<tripoint_abs_ms> surviving_exit_positions;
    bool all_survivors_confirmed_homeward_exit = true;
    for( local_handoff_member_snapshot &member_snapshot : snapshot.members ) {
        const auto read_iter = std::find_if( member_reads.begin(), member_reads.end(),
        [&member_snapshot]( const local_dematerialization_member_read & read ) {
            return read.npc_id == member_snapshot.npc_id;
        } );
        if( read_iter == member_reads.end() || !read_iter->readable ||
            std::find( read_member_ids.begin(), read_member_ids.end(), read_iter->npc_id ) !=
            read_member_ids.end() ||
            ( read_iter->dead ? read_iter->hp_percent != 0 :
              read_iter->hp_percent <= 0 || read_iter->hp_percent > 100 ) ) {
            plan.notes.push_back( "local dematerialization blocked: member read is partial or contradictory" );
            return plan;
        }
        const bool casualty_was_recorded = std::find( snapshot.casualty_ids.begin(),
                                           snapshot.casualty_ids.end(), read_iter->npc_id ) !=
                                           snapshot.casualty_ids.end();
        const bool survivor_left_route_omt = !read_iter->dead &&
                project_to<coords::omt>( read_iter->current_position ) != snapshot.route_position;
        const bool confirmed_homeward_exit = survivor_left_route_omt &&
                                             scout_phase_requires_homeward_only( snapshot.phase ) &&
                                             read_iter->homeward_route_confirmed;
        if( ( !read_iter->dead &&
              ( ( survivor_left_route_omt && !confirmed_homeward_exit ) ||
                std::find( surviving_exit_positions.begin(), surviving_exit_positions.end(),
                           read_iter->current_position ) != surviving_exit_positions.end() ) ) ||
            ( read_iter->dead && casualty_was_recorded &&
              read_iter->current_position != member_snapshot.exit_position ) ) {
            plan.notes.push_back( "local dematerialization blocked: member position is contradictory" );
            return plan;
        }
        if( casualty_was_recorded != read_iter->dead ) {
            if( !read_iter->dead ) {
                plan.notes.push_back( "local dematerialization blocked: casualty returned alive" );
                return plan;
            }
            snapshot.casualty_ids.push_back( read_iter->npc_id );
        }
        member_snapshot.exit_position = read_iter->current_position;
        member_snapshot.hp_percent = read_iter->hp_percent;
        member_snapshot.dead = read_iter->dead;
        if( !read_iter->dead ) {
            all_survivors_confirmed_homeward_exit =
                all_survivors_confirmed_homeward_exit && confirmed_homeward_exit;
            if( snapshot.cohesion_assembled && !snapshot.cohesion_abort_return &&
                !confirmed_homeward_exit &&
                rl_dist( read_iter->current_position, member_snapshot.staging_position ) > 1 ) {
                plan.notes.push_back(
                    "local dematerialization blocked: survivor left assembled staging" );
                return plan;
            }
            surviving_exit_positions.push_back( read_iter->current_position );
        }
        read_member_ids.push_back( read_iter->npc_id );
    }
    if( !snapshot.cohesion_abort_return && !all_survivors_confirmed_homeward_exit &&
        surviving_exit_positions.size() == 2 &&
        rl_dist( surviving_exit_positions[0], surviving_exit_positions[1] ) >
        local_pair_cohesion_radius_ms ) {
        plan.notes.push_back( "local dematerialization blocked: surviving pair separated" );
        return plan;
    }
    std::optional<tripoint_abs_omt> physical_homeward_cursor;
    for( const tripoint_abs_ms &exit_position : surviving_exit_positions ) {
        const tripoint_abs_omt exit_omt = project_to<coords::omt>( exit_position );
        if( exit_omt == snapshot.route_position ) {
            continue;
        }
        if( physical_homeward_cursor && *physical_homeward_cursor != exit_omt ) {
            plan.notes.push_back(
                "local dematerialization blocked: survivors exited into different overmaps" );
            return plan;
        }
        physical_homeward_cursor = exit_omt;
    }
    if( physical_homeward_cursor ) {
        if( std::any_of( surviving_exit_positions.begin(), surviving_exit_positions.end(),
        [&physical_homeward_cursor]( const tripoint_abs_ms & exit_position ) {
            return project_to<coords::omt>( exit_position ) != *physical_homeward_cursor;
        } ) ) {
            plan.notes.push_back(
                "local dematerialization blocked: incomplete overmap-boundary crossing" );
            return plan;
        }
        if( !hostile_site_contains_omt( site, *physical_homeward_cursor ) ) {
            plan.notes.push_back(
                "local dematerialization blocked: physical return has not reached camp" );
            return plan;
        }
        snapshot.route_position = *physical_homeward_cursor;
        snapshot.approach_from = snapshot.route_position == prior_local_route_position ?
                                 prior_local_approach : prior_local_route_position;
        snapshot.egress_omt = outing.shared_route.back();
    }
    if( std::find( snapshot.casualty_ids.begin(), snapshot.casualty_ids.end(),
                   snapshot.cohesion_leader_id ) != snapshot.casualty_ids.end() ) {
        const auto replacement = std::find_if( outing.member_ids.begin(), outing.member_ids.end(),
        [&snapshot]( const character_id candidate_id ) {
            return std::find( snapshot.casualty_ids.begin(), snapshot.casualty_ids.end(),
                              candidate_id ) == snapshot.casualty_ids.end();
        } );
        if( replacement != outing.member_ids.end() ) {
            snapshot.cohesion_leader_id = *replacement;
        }
    }
    if( read_member_ids.size() != 2 || snapshot.casualty_ids.size() > 2 ||
        !snapshot.is_abstract_resume() ) {
        plan.notes.push_back( "local dematerialization blocked: complete resume snapshot is invalid" );
        return plan;
    }

    plan.resume_snapshot = std::move( snapshot );
    plan.valid = true;
    plan.notes.push_back( "local dematerialization captured the complete stable pair" );
    return plan;
}

local_handoff_commit_result commit_local_pair_dematerialization( site_record &site,
        const local_dematerialization_plan &plan,
        const std::function<bool( const local_handoff_member_snapshot & )> &quiesce_member,
        const std::function<void( const local_handoff_member_snapshot & )> &rollback_member )
{
    if( !plan.valid || !plan.resume_snapshot.is_abstract_resume() ||
        !quiesce_member || !rollback_member ) {
        return local_handoff_commit_result::rejected;
    }
    const active_outing_state *current = site.active_external_outing();
    if( current == nullptr || !current->is_active() ) {
        return local_handoff_commit_result::rejected;
    }
    if( current->owner == simulation_owner::abstract ) {
        return local_handoff_snapshots_equal( current->local_handoff,
                                              plan.resume_snapshot ) ?
               local_handoff_commit_result::unchanged :
               local_handoff_commit_result::rejected;
    }
    if( current->kind != outing_kind::structural_sortie || current->schema_version < 7 ||
        !simulation_cursor_matches( *current, plan.expected_cursor ) ||
        current->handoff_epoch == std::numeric_limits<int>::max() ||
        plan.resume_snapshot.handoff_epoch != current->handoff_epoch + 1 ||
        plan.resume_snapshot.committed_minutes < current->last_advanced_minutes ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    advance_camp_supply( candidate, plan.resume_snapshot.committed_minutes );
    active_outing_state &next = candidate.active_outing;
    next.owner = simulation_owner::abstract;
    next.handoff_epoch = plan.resume_snapshot.handoff_epoch;
    next.last_advanced_minutes = plan.resume_snapshot.committed_minutes;
    next.cargo = plan.resume_snapshot.cargo;
    next.casualty_ids = plan.resume_snapshot.casualty_ids;
    next.local_handoff = plan.resume_snapshot;
    next.leader_id = next.local_handoff.cohesion_leader_id;
    if( next.abstract_encounter.active && next.abstract_encounter.local_claimed ) {
        next.abstract_encounter.local_claimed = false;
        next.abstract_encounter.outcome_applied = true;
        next.abstract_encounter.last_applied_episode = next.abstract_encounter.episode;
        next.abstract_encounter.outcome = "resolved_by_local_reality";
    }
    for( const local_handoff_member_snapshot &member_snapshot : plan.resume_snapshot.members ) {
        member_record *member = candidate.find_member( member_snapshot.npc_id );
        if( member == nullptr ) {
            return local_handoff_commit_result::rejected;
        }
        if( member_snapshot.dead ) {
            if( !update_member_state( candidate, member_snapshot.npc_id, member_state::dead,
                    "local dematerialization recorded physical death" ) ) {
                return local_handoff_commit_result::rejected;
            }
            if( !next.member_is_resolved( member_snapshot.npc_id ) ) {
                next.resolved_member_ids.push_back( member_snapshot.npc_id );
            }
            next.last_progress_minutes = std::max( next.last_progress_minutes,
                                                  plan.resume_snapshot.committed_minutes );
        } else {
            if( member->state != member_state::outbound &&
                member->state != member_state::local_contact ) {
                return local_handoff_commit_result::rejected;
            }
            member->state = member_state::outbound;
            member->wounded_or_unready = member_snapshot.hp_percent <= 50;
            member->last_writeback_summary = "local dematerialization hp=" +
                                             std::to_string( member_snapshot.hp_percent );
        }
    }
    if( next.casualty_ids.size() == next.member_ids.size() ) {
        next.phase = scout_phase::lost;
        next.local_handoff.phase = scout_phase::lost;
    }
    advance_camp_supply( candidate, plan.resume_snapshot.committed_minutes );
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return local_handoff_commit_result::rejected;
    }

    std::vector<const local_handoff_member_snapshot *> quiesced_members;
    const auto rollback_quiesced_members = [&quiesced_members, &rollback_member]() {
        for( auto iter = quiesced_members.rbegin(); iter != quiesced_members.rend(); ++iter ) {
            try {
                rollback_member( **iter );
            } catch( ... ) {
                // Keep rolling back the rest of the complete pair.
            }
        }
    };
    try {
        for( const local_handoff_member_snapshot &member : plan.resume_snapshot.members ) {
            quiesced_members.push_back( &member );
            if( !quiesce_member( member ) ) {
                rollback_quiesced_members();
                return local_handoff_commit_result::rolled_back;
            }
        }
    } catch( ... ) {
        rollback_quiesced_members();
        return local_handoff_commit_result::rolled_back;
    }

    site = std::move( candidate );
    return local_handoff_commit_result::applied;
}

local_handoff_commit_result start_local_pair_alternate_watch_reposition(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const int current_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( outing.alternate_watch_reposition_pending ) {
        return simulation_cursor_matches( outing, expected_cursor ) &&
               current_minutes == outing.last_advanced_minutes ?
               local_handoff_commit_result::unchanged :
               local_handoff_commit_result::rejected;
    }
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version != 10 ||
        outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        current_minutes != outing.last_advanced_minutes || current_minutes < 0 ||
        outing.phase != scout_phase::observing ||
        outing.waypoint_index != structural_outing_destination_waypoint( outing ) ||
        !outing.local_handoff.is_active() ||
        !outing.local_handoff.cohesion_assembled ||
        outing.local_handoff.cohesion_abort_return ||
        outing.local_handoff.route_position != outing.selected_watch_omt ||
        outing.alternate_watch_attempted ||
        outing.alternate_watch_kind == structural_watch_kind::none ||
        outing.alternate_watch_shared_route.empty() ||
        outing.assessment.observation_started_minutes < 0 ||
        outing.assessment.last_progress_minutes <
        outing.assessment.observation_started_minutes ||
        current_minutes - outing.assessment.last_progress_minutes < 2 * 60 ||
        outing.assessment.pinned_target_revision != outing.target_lead_revision ||
        outing.casualty_ids.size() > 0 || outing.resolved_member_ids.size() > 0 ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    candidate.active_outing.alternate_watch_reposition_pending = true;
    if( !simulation_owner_state_is_consistent( candidate.active_outing ) ||
        !candidate.roster().valid ) {
        return local_handoff_commit_result::rejected;
    }
    site = std::move( candidate );
    record_scout_phase_transition_event(
        site.active_outing, scout_phase::observing, scout_phase::observing,
        "local pair started persisted alternate watch reposition", current_minutes );
    return local_handoff_commit_result::applied;
}

local_handoff_commit_result abort_local_pair_alternate_watch_reposition(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const int current_minutes, const std::string_view reason )
{
    const std::string bounded_reason = reason.empty() ?
                                       "alternate watch route aborted" :
                                       std::string( reason ).substr( 0, max_sortie_summary_length );
    const active_outing_state &outing = site.active_outing;
    if( !outing.alternate_watch_reposition_pending ) {
        const bool replay_matches = outing.kind == outing_kind::structural_sortie &&
                                    outing.schema_version == 10 &&
                                    outing.activity_id == expected_cursor.activity_id &&
                                    outing.generation == expected_cursor.generation &&
                                    outing.owner == expected_cursor.owner &&
                                    outing.handoff_epoch == expected_cursor.handoff_epoch &&
                                    outing.phase == scout_phase::returning_report &&
                                    outing.last_advanced_minutes == current_minutes &&
                                    outing.assessment.exit_reason == bounded_reason;
        if( replay_matches ) {
            return local_handoff_commit_result::unchanged;
        }
        const bool eligible_start_failed =
            outing.kind == outing_kind::structural_sortie &&
            outing.schema_version == 10 && outing.owner == simulation_owner::local &&
            simulation_cursor_matches( outing, expected_cursor ) &&
            outing.phase == scout_phase::observing &&
            current_minutes >= 0 && current_minutes == outing.last_advanced_minutes &&
            outing.waypoint_index == structural_outing_destination_waypoint( outing ) &&
            outing.local_handoff.is_active() &&
            outing.local_handoff.cohesion_assembled &&
            !outing.local_handoff.cohesion_abort_return &&
            outing.local_handoff.route_position == outing.selected_watch_omt &&
            !outing.alternate_watch_attempted &&
            outing.alternate_watch_kind != structural_watch_kind::none &&
            !outing.alternate_watch_shared_route.empty() &&
            outing.assessment.observation_started_minutes >= 0 &&
            outing.assessment.last_progress_minutes >=
            outing.assessment.observation_started_minutes &&
            outing.assessment.pinned_target_revision == outing.target_lead_revision &&
            outing.casualty_ids.empty() && outing.resolved_member_ids.empty() &&
            current_minutes - outing.assessment.last_progress_minutes >= 2 * 60;
        if( !eligible_start_failed ) {
            return local_handoff_commit_result::rejected;
        }
    }
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version != 10 ||
        outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        outing.phase != scout_phase::observing ||
        current_minutes < outing.last_advanced_minutes || current_minutes < 0 ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    next.alternate_watch_reposition_pending = false;
    next.assessment.readiness_latched = false;
    next.assessment.threshold_class = scout_assessment_threshold_class::none;
    next.assessment.last_progress_minutes = current_minutes;
    next.assessment.next_eligible_minutes = minutes_after_saturated(
            current_minutes, 12 * 60 );
    next.assessment.exit_reason = bounded_reason;
    next.phase = scout_phase::returning_report;
    next.last_progress_minutes = current_minutes;
    next.last_advanced_minutes = current_minutes;
    next.local_handoff.phase = next.phase;
    next.local_handoff.committed_minutes = current_minutes;
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return local_handoff_commit_result::rejected;
    }
    site = std::move( candidate );
    record_scout_phase_transition_event(
        site.active_outing, scout_phase::observing, scout_phase::returning_report,
        bounded_reason, current_minutes );
    return local_handoff_commit_result::applied;
}

local_alternate_watch_reposition_plan plan_local_pair_alternate_watch_reposition(
    const site_record &site, const simulation_advance_cursor &expected_cursor,
    const int current_minutes,
    const std::vector<local_alternate_watch_member_read> &member_reads )
{
    local_alternate_watch_reposition_plan plan;
    plan.expected_cursor = expected_cursor;
    const active_outing_state &outing = site.active_outing;
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version != 10 ||
        outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        !outing.alternate_watch_reposition_pending || outing.alternate_watch_attempted ||
        outing.phase != scout_phase::observing ||
        !outing.local_handoff.is_active() ||
        !outing.local_handoff.cohesion_assembled ||
        outing.local_handoff.cohesion_abort_return ||
        outing.handoff_epoch == std::numeric_limits<int>::max() ||
        current_minutes < outing.last_advanced_minutes ||
        outing.member_ids.size() != 2 || outing.local_handoff.members.size() != 2 ||
        member_reads.size() != 2 || !outing.casualty_ids.empty() ||
        !outing.resolved_member_ids.empty() ||
        outing.alternate_watch_kind == structural_watch_kind::none ||
        outing.alternate_watch_shared_route.empty() ) {
        plan.notes.push_back(
            "alternate watch reposition blocked: stale or incomplete local pair" );
        return plan;
    }

    local_handoff_snapshot snapshot = outing.local_handoff;
    snapshot.handoff_epoch = outing.handoff_epoch + 1;
    snapshot.waypoint_index = structural_outing_destination_waypoint( outing );
    if( snapshot.waypoint_index < 0 || snapshot.waypoint_index >=
        static_cast<int>( outing.alternate_watch_shared_route.size() ) ) {
        plan.notes.push_back(
            "alternate watch reposition blocked: alternate route has no destination waypoint" );
        return plan;
    }
    snapshot.phase = scout_phase::observing;
    snapshot.route_position = outing.alternate_watch_omt;
    snapshot.approach_from = snapshot.waypoint_index == 0 ? snapshot.route_position :
                             outing.alternate_watch_shared_route[
                                 static_cast<std::size_t>( snapshot.waypoint_index - 1 )];
    snapshot.egress_omt = snapshot.waypoint_index + 1 <
                          static_cast<int>( outing.alternate_watch_shared_route.size() ) ?
                          outing.alternate_watch_shared_route[
                              static_cast<std::size_t>( snapshot.waypoint_index + 1 )] :
                          snapshot.route_position;
    snapshot.committed_minutes = current_minutes;

    std::vector<character_id> read_member_ids;
    std::vector<tripoint_abs_ms> arrival_positions;
    for( local_handoff_member_snapshot &member_snapshot : snapshot.members ) {
        const auto read_iter = std::find_if( member_reads.begin(), member_reads.end(),
        [&member_snapshot]( const local_alternate_watch_member_read & read ) {
            return read.npc_id == member_snapshot.npc_id;
        } );
        if( read_iter == member_reads.end() || !read_iter->readable || read_iter->dead ||
            !read_iter->alternate_route_confirmed || read_iter->hp_percent <= 0 ||
            read_iter->hp_percent > 100 ||
            project_to<coords::omt>( read_iter->current_position ) !=
            outing.alternate_watch_omt ||
            std::find( read_member_ids.begin(), read_member_ids.end(),
                       read_iter->npc_id ) != read_member_ids.end() ||
            std::find( arrival_positions.begin(), arrival_positions.end(),
                       read_iter->current_position ) != arrival_positions.end() ) {
            plan.notes.push_back(
                "alternate watch reposition blocked: both routed members have not physically arrived" );
            return plan;
        }
        member_snapshot.exit_position = read_iter->current_position;
        member_snapshot.hp_percent = read_iter->hp_percent;
        member_snapshot.dead = false;
        read_member_ids.push_back( read_iter->npc_id );
        arrival_positions.push_back( read_iter->current_position );
    }
    if( read_member_ids.size() != 2 || arrival_positions.size() != 2 ||
        rl_dist( arrival_positions[0], arrival_positions[1] ) >
        local_pair_cohesion_radius_ms ) {
        plan.notes.push_back(
            "alternate watch reposition blocked: arrived pair is incomplete or separated" );
        return plan;
    }
    for( std::size_t index = 0; index < snapshot.members.size(); ++index ) {
        snapshot.members[index].entry_position = arrival_positions[index];
        snapshot.members[index].staging_position =
            arrival_positions[( index + 1 ) % arrival_positions.size()];
        snapshot.members[index].exit_position = arrival_positions[index];
    }

    plan.expected_target_revision = outing.target_lead_revision;
    plan.expected_member_ids = outing.member_ids;
    plan.expected_selected_watch_kind = outing.selected_watch_kind;
    plan.expected_selected_watch_omt = outing.selected_watch_omt;
    plan.expected_selected_watch_route_cost = outing.selected_watch_route_cost;
    plan.expected_alternate_watch_kind = outing.alternate_watch_kind;
    plan.expected_alternate_watch_omt = outing.alternate_watch_omt;
    plan.expected_alternate_watch_route_cost = outing.alternate_watch_route_cost;
    plan.expected_shared_route = outing.shared_route;
    plan.expected_alternate_watch_shared_route = outing.alternate_watch_shared_route;
    plan.resume_snapshot = std::move( snapshot );
    plan.valid = true;
    plan.notes.push_back(
        "alternate watch reposition captured both physical arrivals" );
    return plan;
}

local_handoff_commit_result commit_local_pair_alternate_watch_reposition(
    site_record &site, const local_alternate_watch_reposition_plan &plan,
    const std::function<bool( const local_handoff_member_snapshot & )> &quiesce_member,
    const std::function<void( const local_handoff_member_snapshot & )> &rollback_member )
{
    if( !plan.valid || !plan.resume_snapshot.is_abstract_resume() ||
        !quiesce_member || !rollback_member ) {
        return local_handoff_commit_result::rejected;
    }
    const active_outing_state &current = site.active_outing;
    if( current.owner == simulation_owner::abstract ) {
        const bool replay_matches = current.alternate_watch_attempted &&
                                    !current.alternate_watch_reposition_pending &&
                                    current.selected_watch_kind ==
                                    plan.expected_alternate_watch_kind &&
                                    current.selected_watch_omt ==
                                    plan.expected_alternate_watch_omt &&
                                    current.selected_watch_route_cost ==
                                    plan.expected_alternate_watch_route_cost &&
                                    current.shared_route ==
                                    plan.expected_alternate_watch_shared_route &&
                                    current.alternate_watch_kind ==
                                    plan.expected_selected_watch_kind &&
                                    current.alternate_watch_omt ==
                                    plan.expected_selected_watch_omt &&
                                    current.alternate_watch_route_cost ==
                                    plan.expected_selected_watch_route_cost &&
                                    current.alternate_watch_shared_route ==
                                    plan.expected_shared_route &&
                                    local_handoff_snapshots_equal(
                                        current.local_handoff, plan.resume_snapshot );
        return replay_matches ? local_handoff_commit_result::unchanged :
               local_handoff_commit_result::rejected;
    }
    if( current.kind != outing_kind::structural_sortie || current.schema_version != 10 ||
        !simulation_cursor_matches( current, plan.expected_cursor ) ||
        !current.alternate_watch_reposition_pending || current.alternate_watch_attempted ||
        current.target_lead_revision != plan.expected_target_revision ||
        current.member_ids != plan.expected_member_ids ||
        current.selected_watch_kind != plan.expected_selected_watch_kind ||
        current.selected_watch_omt != plan.expected_selected_watch_omt ||
        current.selected_watch_route_cost != plan.expected_selected_watch_route_cost ||
        current.alternate_watch_kind != plan.expected_alternate_watch_kind ||
        current.alternate_watch_omt != plan.expected_alternate_watch_omt ||
        current.alternate_watch_route_cost != plan.expected_alternate_watch_route_cost ||
        current.shared_route != plan.expected_shared_route ||
        current.alternate_watch_shared_route !=
        plan.expected_alternate_watch_shared_route ||
        plan.resume_snapshot.handoff_epoch != current.handoff_epoch + 1 ||
        plan.resume_snapshot.committed_minutes < current.last_advanced_minutes ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    std::swap( next.shared_route, next.alternate_watch_shared_route );
    std::swap( next.selected_watch_kind, next.alternate_watch_kind );
    std::swap( next.selected_watch_omt, next.alternate_watch_omt );
    std::swap( next.selected_watch_route_cost, next.alternate_watch_route_cost );
    next.alternate_watch_reposition_pending = false;
    next.alternate_watch_attempted = true;
    next.waypoint_index = structural_outing_destination_waypoint( next );
    next.owner = simulation_owner::abstract;
    next.handoff_epoch = plan.resume_snapshot.handoff_epoch;
    next.last_advanced_minutes = plan.resume_snapshot.committed_minutes;
    next.last_progress_minutes = plan.resume_snapshot.committed_minutes;
    next.assessment.last_progress_minutes = plan.resume_snapshot.committed_minutes;
    next.expected_return_minutes = structural_expected_return_minutes(
                                       next.started_minutes, candidate.anchor,
                                       next.selected_watch_omt );
    next.missing_deadline_minutes = minutes_after_saturated(
                                        next.expected_return_minutes,
                                        scout_missing_grace_minutes );
    next.local_handoff = plan.resume_snapshot;
    for( const local_handoff_member_snapshot &member_snapshot :
         plan.resume_snapshot.members ) {
        member_record *member = candidate.find_member( member_snapshot.npc_id );
        if( member == nullptr || member_snapshot.dead ||
            ( member->state != member_state::outbound &&
              member->state != member_state::local_contact ) ) {
            return local_handoff_commit_result::rejected;
        }
        member->state = member_state::outbound;
        member->wounded_or_unready = member_snapshot.hp_percent <= 50;
        member->last_writeback_summary = "alternate watch reposition hp=" +
                                         std::to_string( member_snapshot.hp_percent );
    }
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return local_handoff_commit_result::rejected;
    }

    std::vector<const local_handoff_member_snapshot *> quiesced_members;
    const auto rollback_quiesced_members = [&quiesced_members, &rollback_member]() {
        for( auto iter = quiesced_members.rbegin(); iter != quiesced_members.rend(); ++iter ) {
            try {
                rollback_member( **iter );
            } catch( ... ) {
                // Keep rolling back the rest of the complete pair.
            }
        }
    };
    try {
        for( const local_handoff_member_snapshot &member : plan.resume_snapshot.members ) {
            quiesced_members.push_back( &member );
            if( !quiesce_member( member ) ) {
                rollback_quiesced_members();
                return local_handoff_commit_result::rolled_back;
            }
        }
    } catch( ... ) {
        rollback_quiesced_members();
        return local_handoff_commit_result::rolled_back;
    }

    site = std::move( candidate );
    record_scout_phase_transition_event(
        site.active_outing, scout_phase::observing, scout_phase::observing,
        "local pair reached persisted alternate watch",
        plan.resume_snapshot.committed_minutes );
    return local_handoff_commit_result::applied;
}

local_handoff_commit_result commit_loaded_local_pair_alternate_watch_reposition(
    site_record &site, const local_alternate_watch_reposition_plan &plan )
{
    if( !plan.valid || !plan.resume_snapshot.is_abstract_resume() ) {
        return local_handoff_commit_result::rejected;
    }
    const active_outing_state &current = site.active_outing;
    if( current.kind != outing_kind::structural_sortie || current.schema_version != 10 ||
        current.owner != simulation_owner::local ||
        !simulation_cursor_matches( current, plan.expected_cursor ) ||
        !current.alternate_watch_reposition_pending || current.alternate_watch_attempted ||
        current.handoff_epoch > std::numeric_limits<int>::max() - 2 ||
        current.target_lead_revision != plan.expected_target_revision ||
        current.member_ids != plan.expected_member_ids ||
        current.selected_watch_kind != plan.expected_selected_watch_kind ||
        current.selected_watch_omt != plan.expected_selected_watch_omt ||
        current.selected_watch_route_cost != plan.expected_selected_watch_route_cost ||
        current.alternate_watch_kind != plan.expected_alternate_watch_kind ||
        current.alternate_watch_omt != plan.expected_alternate_watch_omt ||
        current.alternate_watch_route_cost != plan.expected_alternate_watch_route_cost ||
        current.shared_route != plan.expected_shared_route ||
        current.alternate_watch_shared_route !=
        plan.expected_alternate_watch_shared_route ||
        plan.resume_snapshot.committed_minutes < current.last_advanced_minutes ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    std::swap( next.shared_route, next.alternate_watch_shared_route );
    std::swap( next.selected_watch_kind, next.alternate_watch_kind );
    std::swap( next.selected_watch_omt, next.alternate_watch_omt );
    std::swap( next.selected_watch_route_cost, next.alternate_watch_route_cost );
    next.alternate_watch_reposition_pending = false;
    next.alternate_watch_attempted = true;
    next.waypoint_index = structural_outing_destination_waypoint( next );
    next.handoff_epoch += 2;
    next.last_advanced_minutes = plan.resume_snapshot.committed_minutes;
    next.last_progress_minutes = plan.resume_snapshot.committed_minutes;
    next.assessment.last_progress_minutes = plan.resume_snapshot.committed_minutes;
    next.expected_return_minutes = structural_expected_return_minutes(
                                       next.started_minutes, candidate.anchor,
                                       next.selected_watch_omt );
    next.missing_deadline_minutes = minutes_after_saturated(
                                        next.expected_return_minutes,
                                        scout_missing_grace_minutes );
    next.local_handoff = plan.resume_snapshot;
    next.local_handoff.handoff_epoch = next.handoff_epoch;
    for( const local_handoff_member_snapshot &member_snapshot :
         next.local_handoff.members ) {
        member_record *member = candidate.find_member( member_snapshot.npc_id );
        if( member == nullptr || member_snapshot.dead ||
            ( member->state != member_state::outbound &&
              member->state != member_state::local_contact ) ) {
            return local_handoff_commit_result::rejected;
        }
        member->wounded_or_unready = member_snapshot.hp_percent <= 50;
        member->last_writeback_summary = "loaded alternate watch reposition hp=" +
                                         std::to_string( member_snapshot.hp_percent );
    }
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return local_handoff_commit_result::rejected;
    }
    site = std::move( candidate );
    record_scout_phase_transition_event(
        site.active_outing, scout_phase::observing, scout_phase::observing,
        "loaded local pair reached persisted alternate watch",
        plan.resume_snapshot.committed_minutes );
    return local_handoff_commit_result::applied;
}

bool record_local_pair_member_death( site_record &site,
                                     const simulation_advance_cursor &expected_cursor,
                                     const character_id member_id,
                                     const tripoint_abs_ms &death_position,
                                     const int current_minutes )
{
    return reconcile_local_pair_casualties( site, expected_cursor,
    { { member_id, member_state::dead, death_position } }, current_minutes );
}

bool reconcile_local_pair_casualties( site_record &site,
                                      const simulation_advance_cursor &expected_cursor,
                                      const std::vector<local_pair_casualty_read> &reads,
                                      const int current_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 7 || outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        !outing.local_handoff.is_active() || reads.empty() || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ) {
        return false;
    }

    std::vector<character_id> read_ids;
    read_ids.reserve( reads.size() );
    for( const local_pair_casualty_read &read : reads ) {
        const auto snapshot_iter = std::find_if( outing.local_handoff.members.begin(),
        outing.local_handoff.members.end(), [&read]( const local_handoff_member_snapshot & member ) {
            return member.npc_id == read.npc_id;
        } );
        if( ( read.state != member_state::dead && read.state != member_state::missing ) ||
            std::find( read_ids.begin(), read_ids.end(), read.npc_id ) != read_ids.end() ||
            snapshot_iter == outing.local_handoff.members.end() || snapshot_iter->dead ||
            outing.member_is_resolved( read.npc_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), read.npc_id ) !=
            outing.casualty_ids.end() ||
            ( read.state == member_state::missing &&
              ( outing.missing_deadline_minutes < 0 ||
                current_minutes < outing.missing_deadline_minutes ) ) ) {
            return false;
        }
        read_ids.push_back( read.npc_id );
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    active_outing_state &next = candidate.active_outing;
    const bool interrupted_alternate_reposition =
        next.alternate_watch_reposition_pending;
    if( interrupted_alternate_reposition ) {
        next.alternate_watch_reposition_pending = false;
        next.assessment.readiness_latched = false;
        next.assessment.threshold_class = scout_assessment_threshold_class::none;
        next.assessment.last_progress_minutes = current_minutes;
        next.assessment.next_eligible_minutes = minutes_after_saturated(
                current_minutes, 12 * 60 );
        next.assessment.exit_reason =
            "alternate watch reposition interrupted by casualty";
        next.phase = scout_phase::returning_report;
        next.local_handoff.phase = next.phase;
    }
    for( const local_pair_casualty_read &read : reads ) {
        const std::string summary = read.state == member_state::dead ?
                                    "local handoff member died under physical simulation" :
                                    "local handoff member missing beyond persisted grace";
        if( !update_member_state( candidate, read.npc_id, read.state, summary ) ) {
            return false;
        }
        next.casualty_ids.push_back( read.npc_id );
        next.resolved_member_ids.push_back( read.npc_id );
        auto death_snapshot = std::find_if( next.local_handoff.members.begin(),
        next.local_handoff.members.end(), [&read]( const local_handoff_member_snapshot & member ) {
            return member.npc_id == read.npc_id;
        } );
        if( death_snapshot == next.local_handoff.members.end() ) {
            return false;
        }
        death_snapshot->exit_position = read.last_position;
        death_snapshot->hp_percent = 0;
        death_snapshot->dead = true;
    }
    next.last_progress_minutes = std::max( next.last_progress_minutes, current_minutes );
    next.last_advanced_minutes = current_minutes;
    next.local_handoff.cargo = next.cargo;
    next.local_handoff.casualty_ids = next.casualty_ids;
    next.local_handoff.committed_minutes = current_minutes;
    if( std::find( read_ids.begin(), read_ids.end(), next.leader_id ) != read_ids.end() ) {
        const auto replacement = std::find_if( next.member_ids.begin(), next.member_ids.end(),
        [&next]( const character_id candidate_id ) {
            return std::find( next.casualty_ids.begin(), next.casualty_ids.end(), candidate_id ) ==
                   next.casualty_ids.end();
        } );
        if( replacement != next.member_ids.end() ) {
            next.leader_id = *replacement;
            next.local_handoff.cohesion_leader_id = *replacement;
        }
    }
    if( next.casualty_ids.size() == next.member_ids.size() ) {
        next.phase = scout_phase::lost;
        next.local_handoff.phase = scout_phase::lost;
    }
    advance_camp_supply( candidate, current_minutes );
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    if( interrupted_alternate_reposition ) {
        record_scout_phase_transition_event(
            site.active_outing, scout_phase::observing, site.active_outing.phase,
            "alternate watch reposition interrupted by casualty", current_minutes );
    }
    return true;
}

local_cohesion_plan plan_local_pair_cohesion( const site_record &site,
        const simulation_advance_cursor &expected_cursor, const int current_minutes,
        const std::vector<local_cohesion_member_read> &member_reads )
{
    local_cohesion_plan plan;
    plan.expected_cursor = expected_cursor;
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 7 || outing.owner != simulation_owner::local ||
        !simulation_owner_state_is_consistent( outing ) ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ||
        outing.member_ids.size() != 2 || member_reads.size() != 2 || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ) {
        plan.notes.push_back( "local cohesion blocked: stale or incomplete local pair" );
        return plan;
    }

    local_handoff_snapshot snapshot = outing.local_handoff;
    plan.leader_id = outing.leader_id;
    if( snapshot.schema_version < 3 ) {
        snapshot.schema_version = 3;
        snapshot.cohesion_leader_id = outing.leader_id;
        snapshot.cohesion_deadline_minutes = current_minutes;
        snapshot.cohesion_reroutes_used = local_pair_reroute_cap;
        snapshot.cohesion_assembled = false;
        snapshot.cohesion_abort_return = true;
        snapshot.phase = scout_phase::returning_home;
        snapshot.committed_minutes = current_minutes;
        plan.snapshot = std::move( snapshot );
        plan.abort_return = true;
        plan.valid = true;
        plan.notes.push_back( "legacy local pair failed safe to coherent return" );
        return plan;
    }

    std::vector<character_id> read_ids;
    std::vector<const local_cohesion_member_read *> living_reads;
    for( const local_handoff_member_snapshot &member_snapshot : snapshot.members ) {
        const auto read = std::find_if( member_reads.begin(), member_reads.end(),
        [&member_snapshot]( const local_cohesion_member_read & candidate ) {
            return candidate.npc_id == member_snapshot.npc_id;
        } );
        const bool casualty_recorded = std::find( outing.casualty_ids.begin(),
                                       outing.casualty_ids.end(), member_snapshot.npc_id ) !=
                                       outing.casualty_ids.end();
        if( read == member_reads.end() ||
            std::find( read_ids.begin(), read_ids.end(), read->npc_id ) != read_ids.end() ||
            member_snapshot.dead != casualty_recorded || read->dead != member_snapshot.dead ) {
            plan.notes.push_back( "local cohesion blocked: member read contradicts physical state" );
            return plan;
        }
        read_ids.push_back( read->npc_id );
        if( !member_snapshot.dead ) {
            living_reads.push_back( &*read );
        }
    }
    if( read_ids.size() != 2 ) {
        plan.notes.push_back( "local cohesion blocked: member reads are not exact" );
        return plan;
    }

    const bool current_leader_is_living = std::any_of( living_reads.begin(), living_reads.end(),
    [&outing]( const local_cohesion_member_read * read ) {
        return read->npc_id == outing.leader_id;
    } );
    if( !current_leader_is_living && !living_reads.empty() ) {
        const auto replacement = std::find_if( outing.member_ids.begin(), outing.member_ids.end(),
        [&living_reads]( const character_id candidate_id ) {
            return std::any_of( living_reads.begin(), living_reads.end(),
            [&candidate_id]( const local_cohesion_member_read * read ) {
                return read->npc_id == candidate_id;
            } );
        } );
        if( replacement == outing.member_ids.end() ) {
            plan.notes.push_back( "local cohesion blocked: no confirmed survivor can lead" );
            return plan;
        }
        plan.leader_id = *replacement;
    }
    snapshot.cohesion_leader_id = plan.leader_id;

    const local_cohesion_member_read *leader_read = nullptr;
    bool all_present_on_route = !living_reads.empty();
    for( const local_cohesion_member_read *read : living_reads ) {
        if( read->npc_id == plan.leader_id ) {
            leader_read = read;
        }
        all_present_on_route = all_present_on_route && read->present &&
                               project_to<coords::omt>( read->current_position ) ==
                               snapshot.route_position;
    }
    bool cohesive = all_present_on_route && leader_read != nullptr;
    if( cohesive ) {
        for( const local_cohesion_member_read *read : living_reads ) {
            if( rl_dist( read->current_position, leader_read->current_position ) >
                local_pair_cohesion_radius_ms ) {
                cohesive = false;
                break;
            }
        }
    }
    if( cohesive && living_reads.size() == 2 ) {
        for( const sortie_observation &observation : outing.observations ) {
            bool observer_is_present = false;
            for( const local_cohesion_member_read *read : living_reads ) {
                observer_is_present = observer_is_present ||
                                      read->npc_id == observation.observer_id;
            }
            if( observation.record_schema_version == 1 && observer_is_present &&
                observation.share_state == sortie_observation_share_state::observer_private ) {
                plan.observations_shared++;
            }
        }
        plan.share_private_observations = plan.observations_shared > 0;
    }
    const bool homeward_assembly_released = snapshot.cohesion_assembled &&
            scout_phase_requires_homeward_only( outing.phase );
    bool assembled = cohesive;
    if( assembled ) {
        for( const local_cohesion_member_read *read : living_reads ) {
            const auto member = std::find_if( snapshot.members.begin(), snapshot.members.end(),
            [read]( const local_handoff_member_snapshot & candidate ) {
                return candidate.npc_id == read->npc_id;
            } );
            if( member == snapshot.members.end() ||
                rl_dist( read->current_position, member->staging_position ) > 1 ) {
                assembled = false;
                break;
            }
        }
    }

    assembled = assembled || homeward_assembly_released;
    snapshot.cohesion_assembled = assembled;
    snapshot.cohesion_abort_return = false;
    if( assembled ) {
        snapshot.cohesion_deadline_minutes = -1;
        snapshot.cohesion_reroutes_used = 0;
    } else {
        if( snapshot.cohesion_deadline_minutes < 0 ) {
            snapshot.cohesion_deadline_minutes = minutes_after_saturated(
                    current_minutes, local_pair_rendezvous_minutes );
        }
        const bool timed_out = snapshot.cohesion_deadline_minutes >= 0 &&
                               current_minutes >= snapshot.cohesion_deadline_minutes;
        if( timed_out || snapshot.cohesion_reroutes_used >= local_pair_reroute_cap ||
            living_reads.empty() ) {
            snapshot.cohesion_abort_return = true;
            snapshot.phase = scout_phase::returning_home;
            plan.abort_return = true;
        } else if( current_minutes > outing.local_handoff.committed_minutes ) {
            for( const local_cohesion_member_read *read : living_reads ) {
                if( !read->present || project_to<coords::omt>( read->current_position ) !=
                    snapshot.route_position ) {
                    continue;
                }
                const auto member = std::find_if( snapshot.members.begin(), snapshot.members.end(),
                [read]( const local_handoff_member_snapshot & candidate ) {
                    return candidate.npc_id == read->npc_id;
                } );
                if( member != snapshot.members.end() &&
                    rl_dist( read->current_position, member->staging_position ) > 1 ) {
                    plan.movement_orders.emplace_back( read->npc_id, member->staging_position );
                }
            }
            plan.reroute_needed = !plan.movement_orders.empty();
        }
    }
    if( living_reads.size() == 2 ) {
        plan.follower_id = living_reads[0]->npc_id == plan.leader_id ?
                           living_reads[1]->npc_id : living_reads[0]->npc_id;
    }
    if( !local_handoff_snapshots_equal( snapshot, outing.local_handoff ) ||
        plan.reroute_needed || plan.leader_id != outing.leader_id ||
        plan.share_private_observations ) {
        snapshot.committed_minutes = current_minutes;
    }
    plan.snapshot = std::move( snapshot );
    plan.valid = true;
    plan.notes.push_back( assembled ? "complete surviving pair assembled at staging" :
                          plan.abort_return ? "local pair cohesion forced coherent return" :
                          "local pair is rendezvousing at staging" );
    return plan;
}

bool commit_local_pair_cohesion( site_record &site, const local_cohesion_plan &plan,
                                 const bool route_attempted, const bool route_failed )
{
    if( !plan.valid || !plan.snapshot.is_active() ||
        plan.observations_shared < 0 ||
        plan.share_private_observations != ( plan.observations_shared > 0 ) ||
        ( route_failed && !route_attempted ) ||
        ( route_attempted && !plan.reroute_needed ) ) {
        return false;
    }
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 7 || outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, plan.expected_cursor ) ||
        plan.snapshot.activity_id != outing.activity_id ||
        plan.snapshot.activity_generation != outing.generation ||
        plan.snapshot.handoff_epoch != outing.handoff_epoch ||
        plan.snapshot.committed_minutes < outing.last_advanced_minutes ) {
        return false;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    int observations_shared = 0;
    if( plan.share_private_observations ) {
        const bool complete_living_pair = std::all_of(
        next.local_handoff.members.begin(), next.local_handoff.members.end(), [&next](
        const local_handoff_member_snapshot & member ) {
            return !member.dead && !next.member_is_resolved( member.npc_id ) &&
                   std::find( next.casualty_ids.begin(), next.casualty_ids.end(), member.npc_id ) ==
                   next.casualty_ids.end();
        } );
        if( !complete_living_pair ) {
            return false;
        }
        for( sortie_observation &observation : next.observations ) {
            if( observation.record_schema_version == 1 &&
                observation.share_state == sortie_observation_share_state::observer_private &&
                std::find( next.member_ids.begin(), next.member_ids.end(),
                           observation.observer_id ) != next.member_ids.end() ) {
                observation.share_state = sortie_observation_share_state::shared;
                observations_shared++;
            }
        }
        if( observations_shared != plan.observations_shared ) {
            return false;
        }
    }
    next.leader_id = plan.leader_id;
    next.local_handoff = plan.snapshot;
    if( route_failed ) {
        next.local_handoff.cohesion_reroutes_used = std::min(
                    local_pair_reroute_cap, next.local_handoff.cohesion_reroutes_used + 1 );
        if( next.local_handoff.cohesion_deadline_minutes < 0 ) {
            next.local_handoff.cohesion_deadline_minutes = minutes_after_saturated(
                        next.local_handoff.committed_minutes, local_pair_rendezvous_minutes );
        }
        if( next.local_handoff.cohesion_reroutes_used >= local_pair_reroute_cap ) {
            next.local_handoff.cohesion_abort_return = true;
            next.local_handoff.phase = scout_phase::returning_home;
        }
    }
    if( plan.abort_return || next.local_handoff.cohesion_abort_return ) {
        next.phase = scout_phase::returning_home;
        next.local_handoff.phase = scout_phase::returning_home;
        next.local_handoff.cohesion_assembled = false;
        next.last_progress_minutes = std::max( next.last_progress_minutes,
                                              next.local_handoff.committed_minutes );
    } else if( next.local_handoff.cohesion_assembled || observations_shared > 0 ) {
        next.last_progress_minutes = std::max( next.last_progress_minutes,
                                              next.local_handoff.committed_minutes );
    }
    next.last_advanced_minutes = next.local_handoff.committed_minutes;
    if( local_handoff_snapshots_equal( outing.local_handoff, next.local_handoff ) &&
        outing.leader_id == next.leader_id && outing.phase == next.phase &&
        observations_shared == 0 ) {
        return false;
    }
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

bool claim_local_pair_site_ownership( const site_record &site,
                                      std::set<character_id> &claimed_members )
{
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.owner != simulation_owner::local ) {
        return true;
    }
    if( site.retired_empty_site || outing.schema_version < 7 ||
        !simulation_owner_state_is_consistent( outing ) ||
        !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ||
        outing.member_ids.size() != 2 ) {
        return false;
    }

    std::set<character_id> candidate_members;
    for( const local_handoff_member_snapshot &snapshot : outing.local_handoff.members ) {
        if( !candidate_members.emplace( snapshot.npc_id ).second ||
            claimed_members.count( snapshot.npc_id ) > 0 ) {
            return false;
        }
    }
    claimed_members.insert( candidate_members.begin(), candidate_members.end() );
    return true;
}

std::map<character_id, tripoint_abs_ms> local_pair_assembly_orders(
    const active_outing_state &outing )
{
    std::map<character_id, tripoint_abs_ms> result;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 7 || outing.owner != simulation_owner::local ||
        !simulation_owner_state_is_consistent( outing ) ||
        !outing.local_handoff.is_active() || outing.local_handoff.cohesion_assembled ||
        outing.local_handoff.cohesion_abort_return ||
        outing.local_handoff.members.size() != 2 || outing.member_ids.size() != 2 ) {
        return result;
    }
    for( const local_handoff_member_snapshot &snapshot : outing.local_handoff.members ) {
        if( snapshot.dead || outing.member_is_resolved( snapshot.npc_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(),
                       snapshot.npc_id ) != outing.casualty_ids.end() ||
            std::find( outing.member_ids.begin(), outing.member_ids.end(),
                       snapshot.npc_id ) == outing.member_ids.end() ) {
            continue;
        }
        if( !result.emplace( snapshot.npc_id, snapshot.staging_position ).second ) {
            return {};
        }
    }
    return result;
}

std::set<character_id> local_pair_homeward_travel_ids( const world_state &state )
{
    std::set<character_id> claimed_members;
    for( const site_record &site : state.sites ) {
        if( !claim_local_pair_site_ownership( site, claimed_members ) ) {
            return {};
        }
    }

    std::set<character_id> result;
    for( const site_record &site : state.sites ) {
        const active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != outing_kind::structural_sortie ||
            outing.owner != simulation_owner::local ||
            !simulation_owner_state_is_consistent( outing ) ||
            !outing.local_handoff.is_active() ||
            !scout_phase_requires_homeward_only( outing.phase ) ) {
            continue;
        }
        for( const local_handoff_member_snapshot &snapshot : outing.local_handoff.members ) {
            if( snapshot.dead || outing.member_is_resolved( snapshot.npc_id ) ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(),
                           snapshot.npc_id ) != outing.casualty_ids.end() ) {
                continue;
            }
            result.insert( snapshot.npc_id );
        }
    }
    return result;
}

std::map<character_id, tripoint_abs_omt> local_pair_alternate_watch_travel_destinations(
    const world_state &state )
{
    std::set<character_id> claimed_members;
    for( const site_record &site : state.sites ) {
        if( !claim_local_pair_site_ownership( site, claimed_members ) ) {
            return {};
        }
    }

    std::map<character_id, tripoint_abs_omt> result;
    for( const site_record &site : state.sites ) {
        const active_outing_state &outing = site.active_outing;
        if( site.retired_empty_site || !outing.is_active() ||
            outing.kind != outing_kind::structural_sortie ||
            outing.owner != simulation_owner::local ||
            !outing.alternate_watch_reposition_pending ||
            !simulation_owner_state_is_consistent( outing ) ) {
            continue;
        }
        for( const local_handoff_member_snapshot &snapshot : outing.local_handoff.members ) {
            if( snapshot.dead || outing.member_is_resolved( snapshot.npc_id ) ||
                std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(),
                           snapshot.npc_id ) != outing.casualty_ids.end() ||
                !result.emplace( snapshot.npc_id,
                                 outing.alternate_watch_omt ).second ) {
                return {};
            }
        }
    }
    return result;
}

scout_phase scout_phase_after_burned_evacuation( const bool concealed_rally_reached )
{
    return concealed_rally_reached ? scout_phase::returning_report :
           scout_phase::returning_exposed;
}

bool scout_phase_requires_homeward_only( const scout_phase phase )
{
    return phase == scout_phase::burned_withdrawal ||
           phase == scout_phase::returning_exposed ||
           phase == scout_phase::returning_report ||
           phase == scout_phase::returning_home || phase == scout_phase::lost;
}

bool active_outing_requires_homeward_routing( const active_outing_state &outing )
{
    return ( outing.kind == outing_kind::scout_sortie ||
             outing.kind == outing_kind::structural_sortie ) &&
           scout_phase_requires_homeward_only( outing.phase );
}

static void record_scout_phase_transition_event( const active_outing_state &outing,
        const scout_phase previous_phase, const scout_phase next_phase,
        const std::string_view reason, const int current_minutes )
{
    if( !bandit_live_world_probe::transition_events_enabled() ) {
        return;
    }
    bandit_live_world_probe::record_transition_event(
        outing.activity_id, outing.generation, to_string( outing.owner ),
        to_string( previous_phase ), to_string( next_phase ), reason, current_minutes );
}

static scout_phase_transition_result transition_active_scout_phase_impl( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const scout_phase expected_phase, const scout_phase next_phase,
        const int current_minutes, const std::string_view reason,
        const bool emit_transition_event )
{
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.kind != outing_kind::scout_sortie ||
        ( site.active_outing.job_type != "scout" &&
          site.active_outing.job_type != "scavenge" ) || current_minutes < 0 ||
        site.active_outing.phase != expected_phase ||
        !is_valid_scout_phase_transition( expected_phase, next_phase ) ||
        ( expected_phase != next_phase && reason.empty() ) ||
        current_minutes < site.active_outing.last_progress_minutes ||
        current_minutes < site.active_outing.last_advanced_minutes ) {
        return scout_phase_transition_result::rejected;
    }
    if( expected_phase == next_phase ) {
        return scout_phase_transition_result::unchanged;
    }
    if( current_minutes <= site.active_outing.last_advanced_minutes ) {
        return scout_phase_transition_result::rejected;
    }

    site.active_outing.phase = next_phase;
    site.active_outing.last_progress_minutes = current_minutes;
    site.active_outing.last_advanced_minutes = current_minutes;
    if( emit_transition_event ) {
        record_scout_phase_transition_event( site.active_outing, expected_phase, next_phase,
                                             reason, current_minutes );
    }
    return scout_phase_transition_result::applied;
}

scout_phase_transition_result transition_active_scout_phase( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const scout_phase expected_phase, const scout_phase next_phase,
        const int current_minutes, const std::string_view reason )
{
    return transition_active_scout_phase_impl( site, expected_cursor, expected_phase,
            next_phase, current_minutes, reason, true );
}

bool is_valid_hostile_operation_phase_transition(
    const hostile_operation_phase previous_phase,
    const hostile_operation_phase next_phase )
{
    if( previous_phase == next_phase ) {
        return true;
    }
    if( next_phase == hostile_operation_phase::lost ) {
        return previous_phase != hostile_operation_phase::lost;
    }

    switch( previous_phase ) {
        case hostile_operation_phase::assembling:
            return next_phase == hostile_operation_phase::outbound;
        case hostile_operation_phase::outbound:
            return next_phase == hostile_operation_phase::rallying ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::rallying:
            return next_phase == hostile_operation_phase::waiting_night ||
                   next_phase == hostile_operation_phase::approaching ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::waiting_night:
            return next_phase == hostile_operation_phase::approaching ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::approaching:
            return next_phase == hostile_operation_phase::committed_contact ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::committed_contact:
            return next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::returning_home:
        case hostile_operation_phase::lost:
            return false;
    }

    return false;
}

hostile_operation_transition_result transition_hostile_operation_phase(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const hostile_operation_phase expected_phase,
    const hostile_operation_phase next_phase, const int current_minutes,
    const std::string &reason )
{
    const hostile_operation_state &operation = site.active_hostile_operation;
    const active_outing_state &reservation = operation.reservation;
    if( !operation.is_active() ||
        !simulation_cursor_matches( reservation, expected_cursor ) ||
        operation.phase != expected_phase ||
        current_minutes < 0 || current_minutes < reservation.last_progress_minutes ||
        current_minutes < reservation.last_advanced_minutes ||
        !is_valid_hostile_operation_phase_transition( expected_phase, next_phase ) ||
        ( expected_phase != next_phase && reason.empty() ) ||
        ( operation.legacy_unpinned && expected_phase != next_phase &&
          next_phase != hostile_operation_phase::lost ) ) {
        return hostile_operation_transition_result::rejected;
    }
    if( expected_phase == next_phase ) {
        return hostile_operation_transition_result::unchanged;
    }
    if( current_minutes <= reservation.last_advanced_minutes ) {
        return hostile_operation_transition_result::rejected;
    }

    site_record candidate = site;
    hostile_operation_state &next_operation = candidate.active_hostile_operation;
    active_outing_state &next_reservation = next_operation.reservation;
    if( expected_phase == hostile_operation_phase::assembling &&
        next_phase == hostile_operation_phase::outbound ) {
        if( !hostile_operation_party_preserves_home( candidate,
                next_reservation.member_ids.size() ) ) {
            return hostile_operation_transition_result::rejected;
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member == nullptr || member->state != member_state::at_home ||
                member->wounded_or_unready ) {
                return hostile_operation_transition_result::rejected;
            }
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            member->state = member_state::outbound;
            member->last_writeback_summary = "hostile operation departed camp";
        }
    } else if( expected_phase == hostile_operation_phase::assembling &&
               next_phase == hostile_operation_phase::lost ) {
        next_reservation.resolved_member_ids = next_reservation.member_ids;
    }
    if( next_phase == hostile_operation_phase::committed_contact ) {
        if( next_reservation.owner == simulation_owner::abstract ) {
            if( next_reservation.handoff_epoch == std::numeric_limits<int>::max() ) {
                return hostile_operation_transition_result::rejected;
            }
            next_reservation.owner = simulation_owner::local;
            next_reservation.handoff_epoch++;
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member != nullptr && member->state == member_state::outbound &&
                !next_reservation.member_is_resolved( member_id ) ) {
                member->state = member_state::local_contact;
                member->last_writeback_summary = "hostile operation committed contact";
            }
        }
        if( next_reservation.local_contact_minutes < 0 ) {
            next_reservation.local_contact_minutes = current_minutes;
        }
    } else if( next_phase == hostile_operation_phase::returning_home ) {
        if( next_reservation.owner == simulation_owner::local ) {
            if( next_reservation.handoff_epoch == std::numeric_limits<int>::max() ) {
                return hostile_operation_transition_result::rejected;
            }
            next_reservation.owner = simulation_owner::abstract;
            next_reservation.handoff_epoch++;
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member != nullptr && member->state == member_state::local_contact &&
                !next_reservation.member_is_resolved( member_id ) ) {
                member->state = member_state::outbound;
                member->last_writeback_summary = "hostile operation withdrawing home";
            }
        }
    }

    next_operation.phase = next_phase;
    switch( next_phase ) {
        case hostile_operation_phase::assembling:
            next_reservation.phase = scout_phase::assembling;
            break;
        case hostile_operation_phase::committed_contact:
            next_reservation.phase = scout_phase::observing;
            break;
        case hostile_operation_phase::returning_home:
            next_reservation.phase = scout_phase::returning_home;
            break;
        case hostile_operation_phase::lost:
            next_reservation.phase = scout_phase::lost;
            break;
        default:
            next_reservation.phase = scout_phase::outbound;
            break;
    }
    next_reservation.last_progress_minutes = current_minutes;
    next_reservation.last_advanced_minutes = current_minutes;
    next_operation.last_transition_reason = reason.substr( 0,
            max_camp_decision_reason_length );
    site = std::move( candidate );
    if( bandit_live_world_probe::transition_events_enabled() ) {
        const active_outing_state &committed_reservation =
            site.active_hostile_operation.reservation;
        bandit_live_world_probe::record_transition_event(
            committed_reservation.activity_id, committed_reservation.generation,
            to_string( committed_reservation.owner ), to_string( expected_phase ),
            to_string( next_phase ), reason, current_minutes );
    }
    return hostile_operation_transition_result::applied;
}

bool is_valid_camp_decision_transition( const camp_decision_state previous_state,
                                        const camp_decision_state next_state )
{
    if( previous_state == next_state ) {
        return true;
    }
    if( next_state == camp_decision_state::abandoned ) {
        return previous_state != camp_decision_state::abandoned;
    }

    switch( previous_state ) {
        case camp_decision_state::idle:
            return next_state == camp_decision_state::report_awaiting_assessment;
        case camp_decision_state::report_awaiting_assessment:
            return next_state == camp_decision_state::preparing_follow_on ||
                   next_state == camp_decision_state::cooldown;
        case camp_decision_state::preparing_follow_on:
            return next_state == camp_decision_state::cooldown;
        case camp_decision_state::cooldown:
            return next_state == camp_decision_state::idle ||
                   next_state == camp_decision_state::report_awaiting_assessment;
        case camp_decision_state::abandoned:
            return next_state == camp_decision_state::report_awaiting_assessment;
    }

    return false;
}

camp_decision_transition_result accept_current_scout_report_for_assessment( site_record &site )
{
    const scout_report_record &report = site.current_scout_report;
    if( site.active_outing.is_active() || !report.is_present() || report.provisional ||
        report.source_job_type != "scout" || report.target_id.empty() ||
        report.target_id.size() > max_camp_lead_target_id_length ||
        report.action_policy == camp_report_policy::none ||
        report.action_policy != report_policy_for_profile( effective_profile( site ) ) ||
        report.delivered_minutes < site.camp_decision.last_transition_minutes ||
        site.camp_decision.state == camp_decision_state::preparing_follow_on ||
        !is_valid_camp_decision_transition( site.camp_decision.state,
                                            camp_decision_state::report_awaiting_assessment ) ) {
        return camp_decision_transition_result::rejected;
    }
    site_record candidate = site;
    normalize_acted_reports( candidate );
    const acted_report_summary *acted = find_acted_report( candidate, report );
    const bool report_precedes_acted = acted != nullptr &&
                                       ( report.source_generation < acted->source_generation ||
                                         ( report.source_generation == acted->source_generation &&
                                           report.revision < acted->report_revision ) ||
                                         report.delivered_minutes < acted->acted_minutes );
    const bool same_report = report_matches_camp_decision( report, site.camp_decision );
    if( same_report ) {
        if( report_precedes_acted ) {
            return camp_decision_transition_result::rejected;
        }
        return site.camp_decision.state == camp_decision_state::report_awaiting_assessment ?
               camp_decision_transition_result::unchanged :
               camp_decision_transition_result::rejected;
    }
    if( acted != nullptr &&
        ( report.source_generation < acted->source_generation ||
          ( report.source_generation == acted->source_generation &&
            report.revision <= acted->report_revision ) ||
          report.delivered_minutes < acted->acted_minutes ) ) {
        return camp_decision_transition_result::rejected;
    }

    camp_decision_record &decision = candidate.camp_decision;
    decision.state = camp_decision_state::report_awaiting_assessment;
    decision.report_policy = report.action_policy;
    decision.source_report_revision = report.revision;
    decision.source_report_generation = report.source_generation;
    decision.source_report_activity_id = report.source_activity_id;
    decision.source_report_application_key = report.application_key;
    decision.target_id = report.target_id;
    decision.target_omt = report.target_omt;
    decision.target_lead_id = report.target_lead_id;
    decision.target_lead_revision = report.target_lead_revision;
    decision.last_transition_minutes = report.delivered_minutes;
    decision.next_eligible_minutes = -1;
    decision.transition_reason = "final scout report delivered for assessment";
    if( report.assessment.next_eligible_minutes >= report.delivered_minutes ) {
        candidate.next_routine_dispatch_eligible_minutes = std::max(
                    candidate.next_routine_dispatch_eligible_minutes,
                    report.assessment.next_eligible_minutes );
    }
    remember_acted_report( candidate, report );
    site = std::move( candidate );
    return camp_decision_transition_result::applied;
}

camp_decision_transition_result transition_camp_decision_state( site_record &site,
        const camp_decision_state expected_state, const camp_decision_state next_state,
        const int expected_report_revision, const int expected_report_generation,
        const int current_minutes, const int next_eligible_minutes, const std::string &reason )
{
    camp_decision_record &decision = site.camp_decision;
    if( decision.state != expected_state || current_minutes < 0 ||
        current_minutes < decision.last_transition_minutes ||
        !is_valid_camp_decision_transition( expected_state, next_state ) ) {
        return camp_decision_transition_result::rejected;
    }
    if( expected_state == next_state ) {
        if( decision.has_pinned_report() &&
            ( decision.source_report_revision != expected_report_revision ||
              decision.source_report_generation != expected_report_generation ) ) {
            return camp_decision_transition_result::rejected;
        }
        return camp_decision_transition_result::unchanged;
    }
    if( reason.empty() || !decision.has_pinned_report() ||
        decision.source_report_revision != expected_report_revision ||
        decision.source_report_generation != expected_report_generation ||
        next_state == camp_decision_state::report_awaiting_assessment ) {
        return camp_decision_transition_result::rejected;
    }
    if( next_state == camp_decision_state::preparing_follow_on ) {
        const scout_report_record &report = site.current_scout_report;
        const scout_report_effective_state effective_report =
            evaluate_scout_report_at( report, current_minutes );
        if( site.active_outing.is_active() ||
            decision.report_policy != report_policy_for_profile( effective_profile( site ) ) ||
            !report_matches_camp_decision( report, decision ) ||
            !effective_report.valid || !effective_report.attack_authorization_usable ) {
            return camp_decision_transition_result::rejected;
        }
    }
    if( next_state == camp_decision_state::cooldown &&
        next_eligible_minutes < current_minutes ) {
        return camp_decision_transition_result::rejected;
    }
    if( next_state == camp_decision_state::idle ) {
        if( expected_state != camp_decision_state::cooldown ||
            decision.next_eligible_minutes < 0 ||
            current_minutes < decision.next_eligible_minutes ) {
            return camp_decision_transition_result::rejected;
        }
        decision.state = camp_decision_state::idle;
        decision.last_transition_minutes = current_minutes;
        decision.next_eligible_minutes = -1;
        decision.transition_reason = reason.substr( 0, max_camp_decision_reason_length );
        return camp_decision_transition_result::applied;
    }

    decision.state = next_state;
    decision.last_transition_minutes = current_minutes;
    decision.next_eligible_minutes = next_state == camp_decision_state::cooldown ?
                                     next_eligible_minutes : -1;
    decision.transition_reason = reason.substr( 0, max_camp_decision_reason_length );
    return camp_decision_transition_result::applied;
}

void member_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "npc_id", npc_id.get_value() );
    json.member( "npc_template_id", npc_template_id );
    json.member( "home_spawn_tile", home_spawn_tile );
    json.member( "state", to_string( state ) );
    json.member( "wounded_or_unready", wounded_or_unready );
    if( abstract_wound_until_minutes >= 0 ) {
        json.member( "abstract_wound_until_minutes", abstract_wound_until_minutes );
    }
    json.member( "last_writeback_summary", last_writeback_summary );
    json.end_object();
}

void member_record::deserialize( const JsonObject &jo )
{
    int raw_npc_id = -1;
    jo.read( "npc_id", raw_npc_id );
    npc_id.deserialize( raw_npc_id );
    jo.read( "npc_template_id", npc_template_id );
    jo.read( "home_spawn_tile", home_spawn_tile );
    std::string state_string = "at_home";
    jo.read( "state", state_string );
    state = member_state_from_string( state_string ).value_or( member_state::at_home );
    jo.read( "wounded_or_unready", wounded_or_unready );
    jo.read( "abstract_wound_until_minutes", abstract_wound_until_minutes );
    if( abstract_wound_until_minutes < -1 ) {
        jo.throw_error( "member has invalid abstract wound recovery time" );
    }
    jo.read( "last_writeback_summary", last_writeback_summary );
}

void spawn_tile_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "tile", tile );
    json.member( "assigned_living_total", assigned_living_total );
    json.end_object();
}

void spawn_tile_record::deserialize( const JsonObject &jo )
{
    jo.read( "tile", tile );
    assigned_living_total = 0;
    if( jo.has_member( "assigned_living_total" ) ) {
        jo.read( "assigned_living_total", assigned_living_total );
    } else {
        jo.read( "headcount", assigned_living_total );
    }
}

void camp_map_lead::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "lead_id", lead_id.substr( 0, max_camp_lead_id_length ) );
    json.member( "revision", std::max( 1, revision ) );
    json.member( "kind", to_string( kind ) );
    json.member( "origin", to_string( origin ) );
    json.member( "status", to_string( status ) );
    if( !target_id.empty() ) {
        json.member( "target_id", target_id.substr( 0, max_camp_lead_target_id_length ) );
    }
    json.member( "omt", omt );
    if( radius_omt != 0 ) {
        json.member( "radius_omt", radius_omt );
    }
    if( !source_key.empty() ) {
        json.member( "source_key", source_key.substr( 0, max_camp_lead_source_key_length ) );
    }
    if( !source_summary.empty() ) {
        json.member( "source_summary", source_summary.substr( 0, max_camp_lead_summary_length ) );
    }
    if( first_seen_minutes != -1 ) {
        json.member( "first_seen_minutes", first_seen_minutes );
    }
    if( last_seen_minutes != -1 ) {
        json.member( "last_seen_minutes", last_seen_minutes );
    }
    if( last_checked_minutes != -1 ) {
        json.member( "last_checked_minutes", last_checked_minutes );
    }
    if( last_scouted_minutes != -1 ) {
        json.member( "last_scouted_minutes", last_scouted_minutes );
    }
    if( bounty != 0 ) {
        json.member( "bounty", bounty );
    }
    if( threat != 0 ) {
        json.member( "threat", threat );
    }
    if( confidence != 0 ) {
        json.member( "confidence", confidence );
    }
    if( threat_confirmed ) {
        json.member( "threat_confirmed", true );
    }
    if( target_alert ) {
        json.member( "target_alert", true );
    }
    if( scout_seen ) {
        json.member( "scout_seen", true );
    }
    if( generated_by_this_camp_routine ) {
        json.member( "generated_by_this_camp_routine", true );
    }
    if( prior_bandit_losses != 0 ) {
        json.member( "prior_bandit_losses", prior_bandit_losses );
    }
    if( prior_defender_losses != 0 ) {
        json.member( "prior_defender_losses", prior_defender_losses );
    }
    if( times_checked_empty != 0 ) {
        json.member( "times_checked_empty", times_checked_empty );
    }
    if( times_harvested != 0 ) {
        json.member( "times_harvested", times_harvested );
    }
    if( !last_outcome.empty() ) {
        json.member( "last_outcome", last_outcome.substr( 0, max_camp_lead_outcome_length ) );
    }
    json.end_object();
}

void camp_map_lead::deserialize( const JsonObject &jo )
{
    camp_map_lead candidate;
    jo.read( "lead_id", candidate.lead_id );
    jo.read( "revision", candidate.revision );
    candidate.revision = std::max( 1, candidate.revision );
    std::string kind_string = "human_activity";
    jo.read( "kind", kind_string );
    candidate.kind = camp_lead_kind_from_string( kind_string ).value_or(
                         camp_lead_kind::human_activity );
    if( jo.has_member( "origin" ) ) {
        std::string origin_string;
        jo.read( "origin", origin_string );
        const std::optional<camp_lead_origin> parsed_origin =
            camp_lead_origin_from_string( origin_string );
        if( !parsed_origin ) {
            jo.throw_error( "camp lead has invalid origin" );
        }
        candidate.origin = *parsed_origin;
    } else {
        candidate.origin = infer_legacy_camp_lead_origin( candidate );
    }
    std::string status_string = "suspected";
    jo.read( "status", status_string );
    candidate.status = camp_lead_status_from_string( status_string ).value_or(
                           camp_lead_status::suspected );
    jo.read( "target_id", candidate.target_id );
    jo.read( "omt", candidate.omt );
    jo.read( "radius_omt", candidate.radius_omt );
    jo.read( "source_key", candidate.source_key );
    jo.read( "source_summary", candidate.source_summary );
    jo.read( "first_seen_minutes", candidate.first_seen_minutes );
    jo.read( "last_seen_minutes", candidate.last_seen_minutes );
    jo.read( "last_checked_minutes", candidate.last_checked_minutes );
    jo.read( "last_scouted_minutes", candidate.last_scouted_minutes );
    jo.read( "bounty", candidate.bounty );
    jo.read( "threat", candidate.threat );
    jo.read( "confidence", candidate.confidence );
    jo.read( "threat_confirmed", candidate.threat_confirmed );
    jo.read( "target_alert", candidate.target_alert );
    jo.read( "scout_seen", candidate.scout_seen );
    jo.read( "generated_by_this_camp_routine", candidate.generated_by_this_camp_routine );
    jo.read( "prior_bandit_losses", candidate.prior_bandit_losses );
    jo.read( "prior_defender_losses", candidate.prior_defender_losses );
    jo.read( "times_checked_empty", candidate.times_checked_empty );
    jo.read( "times_harvested", candidate.times_harvested );
    jo.read( "last_outcome", candidate.last_outcome );
    bound_camp_map_lead_strings( candidate );
    *this = std::move( candidate );
}

void camp_intelligence_map::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", 5 );
    json.member( "last_daily_cleanup_minutes", last_daily_cleanup_minutes );
    json.member( "next_near_tick_minutes", next_near_tick_minutes );
    json.member( "next_mid_tick_minutes", next_mid_tick_minutes );
    json.member( "next_far_tick_minutes", next_far_tick_minutes );
    json.member( "next_frontier_tick_minutes", next_frontier_tick_minutes );
    json.member( "known_radius_omt", known_radius_omt );
    json.member( "terrain_scan_cursor", std::clamp( terrain_scan_cursor, 0, 11 ) );
    json.member( "last_routine_target_lead_id",
                 last_routine_target_lead_id.substr( 0, max_camp_lead_id_length ) );
    json.member( "previous_routine_target_lead_id",
                 previous_routine_target_lead_id.substr( 0, max_camp_lead_id_length ) );
    json.member( "frontier_radius_omt", frontier_radius_omt );
    json.member( "frontier_sector_cursor", frontier_sector_cursor );
    json.member( "frontier_last_resolved_minutes", frontier_last_resolved_minutes );
    json.member( "leads", leads );
    json.end_object();
}

void camp_intelligence_map::deserialize( const JsonObject &jo )
{
    camp_intelligence_map candidate;
    int loaded_schema_version = 1;
    jo.read( "schema_version", loaded_schema_version );
    if( loaded_schema_version < 1 || loaded_schema_version > 5 ) {
        jo.throw_error( "camp intelligence schema version is unsupported" );
    }
    jo.read( "last_daily_cleanup_minutes", candidate.last_daily_cleanup_minutes );
    jo.read( "next_near_tick_minutes", candidate.next_near_tick_minutes );
    jo.read( "next_mid_tick_minutes", candidate.next_mid_tick_minutes );
    jo.read( "next_far_tick_minutes", candidate.next_far_tick_minutes );
    jo.read( "next_frontier_tick_minutes", candidate.next_frontier_tick_minutes );
    jo.read( "known_radius_omt", candidate.known_radius_omt );
    if( loaded_schema_version >= 4 ) {
        if( !jo.has_member( "terrain_scan_cursor" ) ) {
            jo.throw_error( "schema-v4 camp intelligence is missing terrain scan state" );
        }
        jo.read( "terrain_scan_cursor", candidate.terrain_scan_cursor );
        if( candidate.terrain_scan_cursor < 0 || candidate.terrain_scan_cursor >= 12 ) {
            jo.throw_error( "schema-v4 camp intelligence has malformed terrain scan state" );
        }
    } else if( jo.has_member( "terrain_scan_cursor" ) ) {
        jo.throw_error( "legacy camp intelligence cannot contain schema-v4 terrain scan state" );
    }
    const bool any_routine_target_history = jo.has_member( "last_routine_target_lead_id" ) ||
                                            jo.has_member( "previous_routine_target_lead_id" );
    if( loaded_schema_version >= 5 ) {
        if( !jo.has_member( "last_routine_target_lead_id" ) ||
            !jo.has_member( "previous_routine_target_lead_id" ) ) {
            jo.throw_error( "schema-v5 camp intelligence is missing routine target history" );
        }
        jo.read( "last_routine_target_lead_id", candidate.last_routine_target_lead_id );
        jo.read( "previous_routine_target_lead_id", candidate.previous_routine_target_lead_id );
        if( candidate.last_routine_target_lead_id.size() > max_camp_lead_id_length ||
            candidate.previous_routine_target_lead_id.size() > max_camp_lead_id_length ) {
            jo.throw_error( "schema-v5 camp intelligence has oversized routine target history" );
        }
    } else if( any_routine_target_history ) {
        jo.throw_error( "legacy camp intelligence cannot contain schema-v5 routine target history" );
    }
    jo.read( "frontier_radius_omt", candidate.frontier_radius_omt );
    if( loaded_schema_version >= 3 ) {
        if( !jo.has_member( "frontier_sector_cursor" ) ||
            !jo.has_member( "frontier_last_resolved_minutes" ) ) {
            jo.throw_error( "schema-v3 camp intelligence is missing frontier memory" );
        }
        jo.read( "frontier_sector_cursor", candidate.frontier_sector_cursor );
        jo.read( "frontier_last_resolved_minutes",
                 candidate.frontier_last_resolved_minutes );
        if( !frontier_memory_is_valid( candidate ) ) {
            jo.throw_error( "schema-v3 camp intelligence has malformed frontier memory" );
        }
    } else if( jo.has_member( "frontier_sector_cursor" ) ||
               jo.has_member( "frontier_last_resolved_minutes" ) ) {
        jo.throw_error( "legacy camp intelligence cannot contain schema-v3 frontier memory" );
    }
    jo.read( "leads", candidate.leads );
    candidate.schema_version = 5;
    *this = std::move( candidate );
}

camp_map_lead *camp_intelligence_map::find_lead( const std::string &lead_id )
{
    auto iter = std::find_if( leads.begin(), leads.end(), [&lead_id]( const camp_map_lead & lead ) {
        return lead.lead_id == lead_id;
    } );
    return iter != leads.end() ? &*iter : nullptr;
}

const camp_map_lead *camp_intelligence_map::find_lead( const std::string &lead_id ) const
{
    auto iter = std::find_if( leads.begin(), leads.end(), [&lead_id]( const camp_map_lead & lead ) {
        return lead.lead_id == lead_id;
    } );
    return iter != leads.end() ? &*iter : nullptr;
}

static int camp_lead_status_retention_rank( const camp_lead_status status )
{
    switch( status ) {
        case camp_lead_status::harvested:
        case camp_lead_status::dangerous:
            return 4;
        case camp_lead_status::active:
        case camp_lead_status::scout_confirmed:
            return 3;
        case camp_lead_status::suspected:
        case camp_lead_status::stale:
            return 2;
        case camp_lead_status::invalidated:
            return 1;
    }
    return 0;
}

static int camp_lead_recency( const camp_map_lead &lead )
{
    return std::max( { lead.first_seen_minutes, lead.last_seen_minutes,
                       lead.last_checked_minutes, lead.last_scouted_minutes } );
}

static auto camp_lead_tie_breaker( const camp_map_lead &lead )
{
    return std::make_tuple( lead.lead_id, lead.revision, static_cast<int>( lead.kind ),
                            static_cast<int>( lead.status ), lead.target_id,
                            lead.omt.x(), lead.omt.y(), lead.omt.z(), lead.radius_omt,
                            lead.source_key, lead.source_summary, lead.first_seen_minutes,
                            lead.last_seen_minutes, lead.last_checked_minutes,
                            lead.last_scouted_minutes, lead.bounty, lead.threat,
                            lead.confidence, lead.threat_confirmed, lead.target_alert,
                            lead.scout_seen, lead.generated_by_this_camp_routine,
                            lead.prior_bandit_losses, lead.prior_defender_losses,
                            lead.times_checked_empty, lead.times_harvested, lead.last_outcome );
}

static int target_lead_reference_strength( const camp_map_lead &lead,
        const std::string &lead_id, const int revision, const std::string &target_id,
        const tripoint_abs_omt &target_omt )
{
    if( !lead_id.empty() ) {
        if( lead.lead_id != lead_id ) {
            return 0;
        }
        return revision > 0 && lead.revision == revision ? 3 : 2;
    }
    if( target_id.empty() ) {
        return 0;
    }
    if( lead.lead_id == target_id ) {
        return 1;
    }
    return lead.target_id == target_id && lead.omt == target_omt ? 1 : 0;
}

static int camp_lead_reference_strength( const site_record &site, const camp_map_lead &lead )
{
    int strength = 0;
    if( site.active_outing.is_active() ) {
        strength = std::max( strength, target_lead_reference_strength( lead,
                             site.active_outing.target_lead_id,
                             site.active_outing.target_lead_revision,
                             site.active_outing.target_id, site.active_outing.target_omt ) );
    }
    if( site.active_hostile_operation.is_active() ) {
        const active_outing_state &reservation = site.active_hostile_operation.reservation;
        strength = std::max( strength, target_lead_reference_strength( lead,
                             reservation.target_lead_id, reservation.target_lead_revision,
                             reservation.target_id, reservation.target_omt ) );
    }
    if( site.current_scout_report.is_present() ) {
        strength = std::max( strength, target_lead_reference_strength( lead,
                             site.current_scout_report.target_lead_id,
                             site.current_scout_report.target_lead_revision,
                             site.current_scout_report.target_id,
                             site.current_scout_report.target_omt ) );
    }
    if( site.camp_decision.has_pinned_report() &&
        ( site.camp_decision.state == camp_decision_state::report_awaiting_assessment ||
          site.camp_decision.state == camp_decision_state::preparing_follow_on ) ) {
        strength = std::max( strength, target_lead_reference_strength( lead,
                             site.camp_decision.target_lead_id,
                             site.camp_decision.target_lead_revision,
                             site.camp_decision.target_id, site.camp_decision.target_omt ) );
    }
    return strength;
}

static bool camp_lead_is_better_retention( const site_record &site, const camp_map_lead &lhs,
        const camp_map_lead &rhs )
{
    const int lhs_reference = camp_lead_reference_strength( site, lhs );
    const int rhs_reference = camp_lead_reference_strength( site, rhs );
    if( lhs_reference != rhs_reference ) {
        return lhs_reference > rhs_reference;
    }
    const int lhs_status = camp_lead_status_retention_rank( lhs.status );
    const int rhs_status = camp_lead_status_retention_rank( rhs.status );
    if( lhs_status != rhs_status ) {
        return lhs_status > rhs_status;
    }
    const int lhs_recency = camp_lead_recency( lhs );
    const int rhs_recency = camp_lead_recency( rhs );
    if( lhs_recency != rhs_recency ) {
        return lhs_recency > rhs_recency;
    }
    if( lhs.revision != rhs.revision ) {
        return lhs.revision > rhs.revision;
    }
    if( lhs.confidence != rhs.confidence ) {
        return lhs.confidence > rhs.confidence;
    }
    if( lhs.bounty != rhs.bounty ) {
        return lhs.bounty > rhs.bounty;
    }
    return camp_lead_tie_breaker( lhs ) > camp_lead_tie_breaker( rhs );
}

template<typename Reference>
static void bound_target_lead_reference( Reference &reference )
{
    reference.target_lead_id.resize( std::min( reference.target_lead_id.size(),
                                     max_camp_lead_id_length ) );
    reference.target_id.resize( std::min( reference.target_id.size(),
                                         max_camp_lead_target_id_length ) );
}

template<typename Reference>
static int compatible_reference_revision( const camp_map_lead &lead,
        const Reference &reference )
{
    if( reference.target_lead_revision <= 0 ) {
        return 1;
    }
    if( !reference.target_lead_id.empty() ) {
        return reference.target_lead_id == lead.lead_id ? reference.target_lead_revision : 1;
    }
    if( lead.lead_id == reference.target_id ||
        ( lead.target_id == reference.target_id && lead.omt == reference.target_omt ) ) {
        return reference.target_lead_revision;
    }
    return 1;
}

static int camp_lead_reference_revision_floor( const site_record &site,
        const camp_map_lead &lead )
{
    int revision = 1;
    if( site.active_outing.is_active() ) {
        revision = std::max( revision, compatible_reference_revision( lead,
                             site.active_outing ) );
    }
    if( site.active_hostile_operation.is_active() ) {
        revision = std::max( revision, compatible_reference_revision( lead,
                             site.active_hostile_operation.reservation ) );
    }
    if( site.current_scout_report.is_present() ) {
        revision = std::max( revision, compatible_reference_revision( lead,
                             site.current_scout_report ) );
    }
    if( site.camp_decision.has_pinned_report() ) {
        revision = std::max( revision, compatible_reference_revision( lead,
                             site.camp_decision ) );
    }
    return revision;
}

template<typename Reference>
static void resolve_camp_lead_reference( Reference &reference,
        const std::vector<camp_map_lead> &leads )
{
    const camp_map_lead *resolved = nullptr;
    if( !reference.target_lead_id.empty() ) {
        const auto exact = std::find_if( leads.begin(), leads.end(), [&reference](
        const camp_map_lead & lead ) {
            return lead.lead_id == reference.target_lead_id;
        } );
        if( exact != leads.end() ) {
            resolved = &*exact;
        }
    }
    if( resolved == nullptr && !reference.target_id.empty() ) {
        const auto exact_legacy = std::find_if( leads.begin(), leads.end(), [&reference](
        const camp_map_lead & lead ) {
            return lead.lead_id == reference.target_id;
        } );
        if( exact_legacy != leads.end() ) {
            resolved = &*exact_legacy;
        } else {
            for( const camp_map_lead &lead : leads ) {
                if( lead.target_id != reference.target_id || lead.omt != reference.target_omt ) {
                    continue;
                }
                if( resolved != nullptr ) {
                    return;
                }
                resolved = &lead;
            }
        }
    }
    if( resolved != nullptr ) {
        reference.target_lead_id = resolved->lead_id;
        reference.target_lead_revision = resolved->revision;
    }
}

void normalize_camp_intelligence( site_record &site )
{
    const bool preserve_in_field_assessment_revision =
        site.active_outing.kind == outing_kind::structural_sortie &&
        site.active_outing.phase == scout_phase::observing &&
        site.active_outing.assessment.observation_started_minutes >= 0 &&
        site.active_outing.assessment.pinned_target_revision ==
        site.active_outing.target_lead_revision &&
        site.active_outing.target_lead_revision > 0;
    bound_target_lead_reference( site.active_outing );
    bound_target_lead_reference( site.active_hostile_operation.reservation );
    bound_target_lead_reference( site.current_scout_report );
    bound_target_lead_reference( site.camp_decision );
    site.remembered_target_or_mark.resize( std::min( site.remembered_target_or_mark.size(),
                                           max_camp_lead_target_id_length ) );

    std::vector<camp_map_lead> candidates = std::move( site.intelligence_map.leads );
    for( camp_map_lead &lead : candidates ) {
        bound_camp_map_lead_strings( lead );
        if( lead.lead_id.empty() ) {
            lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
            bound_camp_map_lead_strings( lead );
        }
        lead.revision = std::max( { 1, lead.revision,
                                   camp_lead_reference_revision_floor( site, lead ) } );
    }
    std::sort( candidates.begin(), candidates.end(), [&site]( const camp_map_lead &lhs,
    const camp_map_lead &rhs ) {
        if( lhs.lead_id != rhs.lead_id ) {
            return lhs.lead_id < rhs.lead_id;
        }
        return camp_lead_is_better_retention( site, lhs, rhs );
    } );

    std::vector<camp_map_lead> normalized;
    normalized.reserve( std::min( candidates.size(), max_camp_intelligence_leads ) );
    for( camp_map_lead &lead : candidates ) {
        if( !normalized.empty() && normalized.back().lead_id == lead.lead_id ) {
            continue;
        }
        normalized.push_back( std::move( lead ) );
    }
    site.intelligence_map.leads = std::move( normalized );

    if( !preserve_in_field_assessment_revision ) {
        resolve_camp_lead_reference( site.active_outing, site.intelligence_map.leads );
    }
    resolve_camp_lead_reference( site.active_hostile_operation.reservation,
                                 site.intelligence_map.leads );
    resolve_camp_lead_reference( site.current_scout_report, site.intelligence_map.leads );
    resolve_camp_lead_reference( site.camp_decision, site.intelligence_map.leads );

    std::sort( site.intelligence_map.leads.begin(), site.intelligence_map.leads.end(),
    [&site]( const camp_map_lead &lhs, const camp_map_lead &rhs ) {
        return camp_lead_is_better_retention( site, lhs, rhs );
    } );
    if( site.intelligence_map.leads.size() > max_camp_intelligence_leads ) {
        site.intelligence_map.leads.resize( max_camp_intelligence_leads );
    }
    std::sort( site.intelligence_map.leads.begin(), site.intelligence_map.leads.end(),
    []( const camp_map_lead &lhs, const camp_map_lead &rhs ) {
        return lhs.lead_id < rhs.lead_id;
    } );
    if( !frontier_memory_is_valid( site.intelligence_map ) ) {
        site.intelligence_map.frontier_sector_cursor = 0;
        site.intelligence_map.frontier_last_resolved_minutes.assign( frontier_sector_count, -1 );
    }
    site.intelligence_map.terrain_scan_cursor = std::clamp(
            site.intelligence_map.terrain_scan_cursor, 0, 11 );
    site.intelligence_map.last_routine_target_lead_id.resize( std::min(
                site.intelligence_map.last_routine_target_lead_id.size(), max_camp_lead_id_length ) );
    site.intelligence_map.previous_routine_target_lead_id.resize( std::min(
                site.intelligence_map.previous_routine_target_lead_id.size(), max_camp_lead_id_length ) );
    site.intelligence_map.schema_version = 5;

    for( std::string &mark : site.known_recent_marks ) {
        mark.resize( std::min( mark.size(), max_live_signal_mark_length ) );
    }
    if( site.known_recent_marks.size() > max_live_signal_marks ) {
        site.known_recent_marks.erase( site.known_recent_marks.begin(),
                                      site.known_recent_marks.end() - max_live_signal_marks );
    }
}

camp_intelligence_aging_result advance_camp_intelligence_aging( site_record &site,
        const int now_minutes )
{
    camp_intelligence_aging_result result;
    result.sites_considered = 1;
    if( now_minutes < 0 ) {
        return result;
    }

    constexpr int day_minutes = 24 * 60;
    constexpr int stale_retention_minutes = 30 * day_minutes;
    const int current_day_boundary = now_minutes - now_minutes % day_minutes;
    if( site.intelligence_map.last_daily_cleanup_minutes > current_day_boundary ) {
        return result;
    }
    const bool advanced_daily_cursor =
        site.intelligence_map.last_daily_cleanup_minutes < current_day_boundary;
    if( site.intelligence_map.last_daily_cleanup_minutes < 0 || advanced_daily_cursor ) {
        site.intelligence_map.last_daily_cleanup_minutes = current_day_boundary;
    }

    for( camp_map_lead &lead : site.intelligence_map.leads ) {
        result.leads_considered++;
        if( camp_lead_reference_strength( site, lead ) > 0 ||
            !returned_structural_signal_lead( lead ) || lead.last_seen_minutes < 0 ||
            now_minutes < lead.last_seen_minutes ) {
            continue;
        }
        const int expiry_minutes = lead.kind == camp_lead_kind::sound_signal ?
                                   3 * 60 : 6 * 60;
        const long long age_minutes = static_cast<long long>( now_minutes ) -
                                      lead.last_seen_minutes;
        const bool ageable_status = lead.status == camp_lead_status::active ||
                                    lead.status == camp_lead_status::suspected ||
                                    lead.status == camp_lead_status::scout_confirmed;
        if( age_minutes < expiry_minutes || !ageable_status ||
            !advance_camp_map_lead_revision( site, lead ) ) {
            continue;
        }
        lead.status = camp_lead_status::stale;
        lead.confidence = 0;
        lead.last_outcome = "returned signal evidence expired without new support";
        result.leads_aged++;
    }

    const auto prune = [&site, now_minutes]( const camp_map_lead & lead ) {
        if( camp_lead_reference_strength( site, lead ) > 0 ||
            ( lead.status != camp_lead_status::stale &&
              lead.status != camp_lead_status::invalidated ) ) {
            return false;
        }
        const int recency = camp_lead_recency( lead );
        return recency >= 0 && now_minutes >= recency &&
               static_cast<long long>( now_minutes ) - recency >= stale_retention_minutes;
    };
    const std::size_t before_prune = site.intelligence_map.leads.size();
    site.intelligence_map.leads.erase( std::remove_if( site.intelligence_map.leads.begin(),
                                       site.intelligence_map.leads.end(), prune ),
                                       site.intelligence_map.leads.end() );
    result.leads_pruned = static_cast<int>( before_prune -
                                           site.intelligence_map.leads.size() );
    if( result.leads_aged > 0 || result.leads_pruned > 0 ) {
        normalize_camp_intelligence( site );
    }
    result.sites_cleaned = advanced_daily_cursor || result.leads_aged > 0 ||
                           result.leads_pruned > 0 ? 1 : 0;
    return result;
}

camp_intelligence_aging_result advance_camp_intelligence_aging( world_state &state,
        const int now_minutes )
{
    camp_intelligence_aging_result result;
    for( site_record &site : state.sites ) {
        const camp_intelligence_aging_result site_result =
            advance_camp_intelligence_aging( site, now_minutes );
        result.sites_considered += site_result.sites_considered;
        result.sites_cleaned += site_result.sites_cleaned;
        result.leads_considered += site_result.leads_considered;
        result.leads_aged += site_result.leads_aged;
        result.leads_pruned += site_result.leads_pruned;
    }
    return result;
}

void sortie_observation::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", record_schema_version );
    json.member( "fact_key", record_schema_version == 1 ? fact_key :
                 fact_key.substr( 0, max_sortie_fact_key_length ) );
    json.member( "summary", record_schema_version == 1 ? summary :
                 summary.substr( 0, max_sortie_summary_length ) );
    json.member( "confidence", record_schema_version == 1 ? confidence :
                 std::clamp( confidence, 0, 100 ) );
    json.member( "observed_minutes", record_schema_version == 1 ? observed_minutes :
                 std::max( -1, observed_minutes ) );
    json.member( "critical", critical );
    if( record_schema_version == 1 || kind != sortie_observation_kind::routine ) {
        json.member( "kind", to_string( kind ) );
    }
    if( record_schema_version == 1 || !state_key.empty() ) {
        json.member( "state_key", state_key.substr( 0, max_sortie_state_key_length ) );
    }
    if( record_schema_version == 1 ) {
        json.member( "source_id", source_id );
        json.member( "sense", sortie_observation_sense_to_string( sense ) );
        json.member( "observer_id", observer_id.get_value() );
        json.member( "source_omt", source_omt );
        json.member( "receiver_omt", receiver_omt );
        json.member( "bucket_start_minutes", bucket_start_minutes );
        json.member( "strength", strength );
        json.member( "visual_quality", visual_quality );
        json.member( "defender_ids", defender_ids );
        json.member( "simultaneity_start_minutes", simultaneity_start_minutes );
        json.member( "simultaneity_end_minutes", simultaneity_end_minutes );
        json.member( "observed_power_low", observed_power_low );
        json.member( "observed_power_high", observed_power_high );
        json.member( "equipment_detail", equipment_detail );
        json.member( "target_revision", target_revision );
        json.member( "uncertainty_radius_omt", uncertainty_radius_omt );
        json.member( "expiry_minutes", expiry_minutes );
        json.member( "share_state", sortie_observation_share_state_to_string( share_state ) );
    }
    json.end_object();
}

void sortie_observation::deserialize( const JsonObject &jo )
{
    sortie_observation candidate;
    jo.read( "schema_version", candidate.record_schema_version );
    if( candidate.record_schema_version < 0 || candidate.record_schema_version > 1 ) {
        jo.throw_error( "sortie observation schema version is unsupported" );
    }

    if( candidate.record_schema_version == 1 ) {
        const std::vector<std::string> required_members = {
            "fact_key", "summary", "confidence", "observed_minutes", "critical", "kind",
            "state_key", "source_id", "sense", "observer_id", "source_omt", "receiver_omt",
            "bucket_start_minutes", "strength", "visual_quality", "defender_ids",
            "simultaneity_start_minutes", "simultaneity_end_minutes", "observed_power_low",
            "observed_power_high", "equipment_detail", "target_revision",
            "uncertainty_radius_omt", "expiry_minutes", "share_state"
        };
        if( std::any_of( required_members.begin(), required_members.end(),
        [&jo]( const std::string & member ) {
            return !jo.has_member( member );
        } ) ) {
            jo.throw_error( "typed sortie observation is incomplete" );
        }
    }

    jo.read( "fact_key", candidate.fact_key );
    jo.read( "summary", candidate.summary );
    jo.read( "confidence", candidate.confidence );
    jo.read( "observed_minutes", candidate.observed_minutes );
    jo.read( "critical", candidate.critical );
    const bool kind_was_present = jo.has_member( "kind" );
    std::string kind_string = "routine";
    jo.read( "kind", kind_string );
    const std::optional<sortie_observation_kind> parsed_kind =
        sortie_observation_kind_from_string( kind_string );
    candidate.kind = parsed_kind.value_or( sortie_observation_kind::routine );
    if( candidate.record_schema_version == 1 && !parsed_kind ) {
        jo.throw_error( "typed sortie observation has invalid kind" );
    } else if( kind_was_present && !parsed_kind ) {
        candidate.critical = true;
    }
    jo.read( "state_key", candidate.state_key );

    if( candidate.record_schema_version == 1 ) {
        jo.read( "source_id", candidate.source_id );
        std::string sense_string;
        jo.read( "sense", sense_string );
        const std::optional<sortie_observation_sense> parsed_sense =
            sortie_observation_sense_from_string( sense_string );
        if( !parsed_sense ) {
            jo.throw_error( "typed sortie observation has invalid sense" );
        }
        candidate.sense = *parsed_sense;
        int raw_observer_id = -1;
        jo.read( "observer_id", raw_observer_id );
        candidate.observer_id.deserialize( raw_observer_id );
        jo.read( "source_omt", candidate.source_omt );
        jo.read( "receiver_omt", candidate.receiver_omt );
        jo.read( "bucket_start_minutes", candidate.bucket_start_minutes );
        jo.read( "strength", candidate.strength );
        jo.read( "visual_quality", candidate.visual_quality );
        jo.read( "defender_ids", candidate.defender_ids );
        jo.read( "simultaneity_start_minutes", candidate.simultaneity_start_minutes );
        jo.read( "simultaneity_end_minutes", candidate.simultaneity_end_minutes );
        jo.read( "observed_power_low", candidate.observed_power_low );
        jo.read( "observed_power_high", candidate.observed_power_high );
        jo.read( "equipment_detail", candidate.equipment_detail );
        jo.read( "target_revision", candidate.target_revision );
        jo.read( "uncertainty_radius_omt", candidate.uncertainty_radius_omt );
        jo.read( "expiry_minutes", candidate.expiry_minutes );
        std::string share_state_string;
        jo.read( "share_state", share_state_string );
        const std::optional<sortie_observation_share_state> parsed_share_state =
            sortie_observation_share_state_from_string( share_state_string );
        if( !parsed_share_state ) {
            jo.throw_error( "typed sortie observation has invalid share state" );
        }
        candidate.share_state = *parsed_share_state;
        if( !typed_sortie_observation_is_valid( candidate ) ) {
            jo.throw_error( "typed sortie observation is malformed" );
        }
    } else {
        normalize_sortie_observation( candidate );
    }
    *this = std::move( candidate );
}

void sortie_cargo::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "supply_units", std::max( 0, supply_units ) );
    json.member( "trade_value", std::max( 0, trade_value ) );
    json.end_object();
}

void sortie_cargo::deserialize( const JsonObject &jo )
{
    sortie_cargo candidate;
    jo.read( "supply_units", candidate.supply_units );
    jo.read( "trade_value", candidate.trade_value );
    candidate.supply_units = std::max( 0, candidate.supply_units );
    candidate.trade_value = std::max( 0, candidate.trade_value );
    *this = candidate;
}

void local_handoff_member_snapshot::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "npc_id", npc_id.get_value() );
    json.member( "prior_position", prior_position );
    json.member( "entry_position", entry_position );
    json.member( "staging_position", staging_position );
    json.member( "exit_position", exit_position );
    json.member( "hp_percent", std::clamp( hp_percent, 0, 100 ) );
    json.member( "dead", dead );
    json.end_object();
}

void local_handoff_member_snapshot::deserialize( const JsonObject &jo )
{
    local_handoff_member_snapshot candidate;
    int raw_npc_id = -1;
    jo.read( "npc_id", raw_npc_id );
    candidate.npc_id.deserialize( raw_npc_id );
    jo.read( "prior_position", candidate.prior_position );
    jo.read( "entry_position", candidate.entry_position );
    if( jo.has_member( "staging_position" ) ) {
        jo.read( "staging_position", candidate.staging_position );
    } else {
        candidate.staging_position = candidate.entry_position;
    }
    if( jo.has_member( "exit_position" ) ) {
        jo.read( "exit_position", candidate.exit_position );
    } else {
        candidate.exit_position = candidate.entry_position;
    }
    jo.read( "hp_percent", candidate.hp_percent );
    jo.read( "dead", candidate.dead );
    if( candidate.hp_percent < 0 || candidate.hp_percent > 100 ||
        ( candidate.dead && candidate.hp_percent != 0 ) ) {
        jo.throw_error( "local handoff member has invalid health state" );
    }
    *this = candidate;
}

void local_handoff_snapshot::clear()
{
    *this = local_handoff_snapshot();
}

bool local_handoff_snapshot::is_active() const
{
    return schema_version >= 1 && schema_version <= 3 && !activity_id.empty() &&
           activity_generation > 0 &&
           handoff_epoch > 0 && handoff_epoch % 2 == 1 && committed_minutes >= 0 &&
           !members.empty() && members.size() <= 2;
}

bool local_handoff_snapshot::is_abstract_resume() const
{
    return schema_version >= 2 && schema_version <= 3 && !activity_id.empty() &&
           handoff_epoch > 0 && handoff_epoch % 2 == 0 && committed_minutes >= 0 &&
           members.size() == 2;
}

void local_handoff_snapshot::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "activity_id", activity_id );
    json.member( "activity_generation", std::max( 0, activity_generation ) );
    json.member( "handoff_epoch", handoff_epoch );
    json.member( "waypoint_index", std::max( 0, waypoint_index ) );
    json.member( "phase", to_string( phase ) );
    json.member( "route_position", route_position );
    json.member( "approach_from", approach_from );
    json.member( "egress_omt", egress_omt );
    json.member( "cargo", cargo );
    std::vector<int> raw_casualty_ids;
    raw_casualty_ids.reserve( std::min<std::size_t>( casualty_ids.size(), 2 ) );
    for( std::size_t index = 0; index < casualty_ids.size() && index < 2; ++index ) {
        raw_casualty_ids.push_back( casualty_ids[index].get_value() );
    }
    json.member( "casualty_ids", raw_casualty_ids );
    json.member( "members", members );
    if( schema_version >= 3 ) {
        json.member( "cohesion_leader_id", cohesion_leader_id.get_value() );
        json.member( "cohesion_deadline_minutes", cohesion_deadline_minutes );
        json.member( "cohesion_reroutes_used", cohesion_reroutes_used );
        json.member( "cohesion_assembled", cohesion_assembled );
        json.member( "cohesion_abort_return", cohesion_abort_return );
    }
    json.member( "committed_minutes", std::max( -1, committed_minutes ) );
    json.end_object();
}

void local_handoff_snapshot::deserialize( const JsonObject &jo )
{
    local_handoff_snapshot candidate;
    jo.read( "schema_version", candidate.schema_version );
    jo.read( "activity_id", candidate.activity_id );
    jo.read( "activity_generation", candidate.activity_generation );
    jo.read( "handoff_epoch", candidate.handoff_epoch );
    jo.read( "waypoint_index", candidate.waypoint_index );
    std::string phase_string = "assembling";
    jo.read( "phase", phase_string );
    candidate.phase = scout_phase_from_string( phase_string ).value_or( scout_phase::lost );
    jo.read( "route_position", candidate.route_position );
    jo.read( "approach_from", candidate.approach_from );
    jo.read( "egress_omt", candidate.egress_omt );
    jo.read( "cargo", candidate.cargo );
    std::vector<int> raw_casualty_ids;
    jo.read( "casualty_ids", raw_casualty_ids );
    for( const int raw_casualty_id : raw_casualty_ids ) {
        character_id casualty_id;
        casualty_id.deserialize( raw_casualty_id );
        if( std::find( candidate.casualty_ids.begin(), candidate.casualty_ids.end(), casualty_id ) !=
            candidate.casualty_ids.end() ) {
            jo.throw_error( "local handoff snapshot has duplicate casualty identity" );
        }
        candidate.casualty_ids.push_back( casualty_id );
        if( candidate.casualty_ids.size() > 2 ) {
            jo.throw_error( "local handoff snapshot exceeds its casualty bound" );
        }
    }
    jo.read( "members", candidate.members );
    if( candidate.members.size() > 2 ) {
        jo.throw_error( "local handoff snapshot exceeds its member bound" );
    }
    if( candidate.schema_version >= 3 ) {
        int raw_cohesion_leader_id = -1;
        jo.read( "cohesion_leader_id", raw_cohesion_leader_id );
        candidate.cohesion_leader_id.deserialize( raw_cohesion_leader_id );
        jo.read( "cohesion_deadline_minutes", candidate.cohesion_deadline_minutes );
        jo.read( "cohesion_reroutes_used", candidate.cohesion_reroutes_used );
        jo.read( "cohesion_assembled", candidate.cohesion_assembled );
        jo.read( "cohesion_abort_return", candidate.cohesion_abort_return );
        if( candidate.cohesion_deadline_minutes < -1 ||
            candidate.cohesion_reroutes_used < 0 ||
            candidate.cohesion_reroutes_used > local_pair_reroute_cap ||
            ( candidate.cohesion_assembled &&
              ( candidate.cohesion_deadline_minutes != -1 ||
                candidate.cohesion_reroutes_used != 0 || candidate.cohesion_abort_return ) ) ||
            ( candidate.cohesion_abort_return &&
              candidate.phase != scout_phase::returning_home ) ) {
            jo.throw_error( "local handoff snapshot has malformed cohesion state" );
        }
    }
    jo.read( "committed_minutes", candidate.committed_minutes );
    candidate.activity_id.resize( std::min( candidate.activity_id.size(),
                                           max_operation_application_key_length ) );
    candidate.activity_generation = std::max( 0, candidate.activity_generation );
    candidate.committed_minutes = std::max( -1, candidate.committed_minutes );
    if( candidate.activity_id.empty() ) {
        candidate.clear();
    } else if( !candidate.is_active() && !candidate.is_abstract_resume() ) {
        jo.throw_error( "local handoff snapshot is malformed" );
    }
    *this = std::move( candidate );
}

void abstract_encounter_state::clear_active()
{
    const int retained_episode = std::max( episode, last_applied_episode );
    const int retained_last_applied = last_applied_episode;
    *this = abstract_encounter_state();
    episode = retained_episode;
    last_applied_episode = retained_last_applied;
}

void abstract_encounter_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "episode", episode );
    json.member( "last_applied_episode", last_applied_episode );
    json.member( "active", active );
    json.member( "overlap_omt", overlap_omt );
    json.member( "stable_threat_ids", stable_threat_ids );
    json.member( "danger_low", danger_low );
    json.member( "danger_high", danger_high );
    json.member( "absent_segment_advances", absent_segment_advances );
    json.member( "detour_attempts", detour_attempts );
    json.member( "has_selected_detour", has_selected_detour );
    json.member( "selected_detour_omt", selected_detour_omt );
    json.member( "local_claimed", local_claimed );
    json.member( "outcome_applied", outcome_applied );
    json.member( "outcome", outcome );
    json.end_object();
}

void abstract_encounter_state::deserialize( const JsonObject &jo )
{
    abstract_encounter_state candidate;
    jo.read( "schema_version", candidate.schema_version );
    jo.read( "episode", candidate.episode );
    jo.read( "last_applied_episode", candidate.last_applied_episode );
    jo.read( "active", candidate.active );
    jo.read( "overlap_omt", candidate.overlap_omt );
    jo.read( "stable_threat_ids", candidate.stable_threat_ids );
    jo.read( "danger_low", candidate.danger_low );
    jo.read( "danger_high", candidate.danger_high );
    jo.read( "absent_segment_advances", candidate.absent_segment_advances );
    jo.read( "detour_attempts", candidate.detour_attempts );
    jo.read( "has_selected_detour", candidate.has_selected_detour );
    jo.read( "selected_detour_omt", candidate.selected_detour_omt );
    jo.read( "local_claimed", candidate.local_claimed );
    jo.read( "outcome_applied", candidate.outcome_applied );
    jo.read( "outcome", candidate.outcome );
    if( candidate.schema_version != 1 || candidate.episode < 0 ||
        candidate.last_applied_episode < 0 ||
        candidate.last_applied_episode > candidate.episode ||
        candidate.danger_low < 0 || candidate.danger_high < candidate.danger_low ||
        candidate.danger_high > 200 || candidate.absent_segment_advances < 0 ||
        candidate.absent_segment_advances > 1 || candidate.detour_attempts < 0 ||
        candidate.detour_attempts > 2 || candidate.stable_threat_ids.size() >
        max_abstract_threat_ids ||
        ( candidate.active && ( candidate.episode <= 0 ||
                                candidate.stable_threat_ids.empty() ) ) ||
        ( candidate.local_claimed && ( !candidate.active || candidate.outcome_applied ) ) ||
        ( candidate.outcome_applied &&
          ( !candidate.active || candidate.last_applied_episode != candidate.episode ) ) ||
        ( candidate.outcome_applied != !candidate.outcome.empty() ) ||
        ( candidate.has_selected_detour &&
          ( candidate.detour_attempts <= 0 || !candidate.outcome_applied ||
            candidate.selected_detour_omt.z() != candidate.overlap_omt.z() ||
            omt_chebyshev_distance( candidate.selected_detour_omt,
                                    candidate.overlap_omt ) != 1 ) ) ||
        candidate.outcome.size() > max_abstract_encounter_outcome_length ||
        ( !candidate.active &&
          ( !candidate.stable_threat_ids.empty() || candidate.danger_low != 0 ||
            candidate.danger_high != 0 || candidate.absent_segment_advances != 0 ||
            candidate.detour_attempts != 0 || candidate.has_selected_detour ||
            candidate.local_claimed || candidate.outcome_applied ) ) ) {
        jo.throw_error( "abstract encounter state is malformed" );
    }
    if( !std::is_sorted( candidate.stable_threat_ids.begin(),
                        candidate.stable_threat_ids.end() ) ||
        std::adjacent_find( candidate.stable_threat_ids.begin(),
                           candidate.stable_threat_ids.end() ) !=
        candidate.stable_threat_ids.end() ||
        std::any_of( candidate.stable_threat_ids.begin(),
    candidate.stable_threat_ids.end(), []( const std::string & id ) {
        return id.empty() || id.size() > max_abstract_threat_id_length;
    } ) ) {
        jo.throw_error( "abstract encounter state has invalid threat identities" );
    }
    if( !candidate.active ) {
        candidate.clear_active();
    }
    *this = std::move( candidate );
}

void scout_report_record::clear()
{
    *this = scout_report_record();
}

bool scout_report_record::is_present() const
{
    return revision > 0 && source_generation > 0 && !source_activity_id.empty() &&
           !application_key.empty();
}

void scout_report_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", 5 );
    json.member( "revision", std::max( 0, revision ) );
    json.member( "action_policy", to_string( action_policy ) );
    json.member( "source_activity_id", source_activity_id );
    json.member( "source_generation", std::max( 0, source_generation ) );
    json.member( "source_job_type", source_job_type );
    json.member( "target_id", target_id );
    json.member( "target_omt", target_omt );
    json.member( "target_lead_id", target_lead_id );
    json.member( "target_lead_revision", std::max( 0, target_lead_revision ) );
    json.member( "application_key", application_key );
    json.member( "observations", make_bounded_sortie_observations( observations ) );
    json.member( "assessment", assessment );
    std::vector<int> raw_casualty_ids;
    raw_casualty_ids.reserve( std::min( casualty_ids.size(), max_active_outing_casualties ) );
    for( std::size_t index = 0; index < casualty_ids.size() &&
         index < max_active_outing_casualties; ++index ) {
        raw_casualty_ids.push_back( casualty_ids[index].get_value() );
    }
    json.member( "casualty_ids", raw_casualty_ids );
    json.member( "delivered_minutes", std::max( -1, delivered_minutes ) );
    json.member( "provisional", provisional );
    json.end_object();
}

void scout_report_record::deserialize( const JsonObject &jo )
{
    scout_report_record candidate;
    candidate.schema_version = 0;
    jo.read( "schema_version", candidate.schema_version );
    const int loaded_schema_version = candidate.schema_version;
    if( loaded_schema_version > 5 ) {
        jo.throw_error( "scout report schema version is newer than supported schema v5" );
    }
    jo.read( "revision", candidate.revision );
    std::string action_policy_string = "none";
    jo.read( "action_policy", action_policy_string );
    candidate.action_policy = camp_report_policy_from_string( action_policy_string ).value_or(
                                  camp_report_policy::none );
    jo.read( "source_activity_id", candidate.source_activity_id );
    jo.read( "source_generation", candidate.source_generation );
    jo.read( "source_job_type", candidate.source_job_type );
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    jo.read( "target_lead_id", candidate.target_lead_id );
    jo.read( "target_lead_revision", candidate.target_lead_revision );
    jo.read( "application_key", candidate.application_key );
    jo.read( "observations", candidate.observations );
    candidate.observations = make_bounded_sortie_observations( candidate.observations );
    if( loaded_schema_version >= 5 ) {
        if( !jo.has_member( "assessment" ) ) {
            jo.throw_error( "schema-v5 scout report is missing assessment state" );
        }
        jo.read( "assessment", candidate.assessment );
    }
    std::vector<int> raw_casualty_ids;
    jo.read( "casualty_ids", raw_casualty_ids );
    for( const int raw_casualty_id : raw_casualty_ids ) {
        character_id casualty_id;
        casualty_id.deserialize( raw_casualty_id );
        if( std::find( candidate.casualty_ids.begin(), candidate.casualty_ids.end(), casualty_id ) ==
            candidate.casualty_ids.end() ) {
            candidate.casualty_ids.push_back( casualty_id );
        }
        if( candidate.casualty_ids.size() >= max_active_outing_casualties ) {
            break;
        }
    }
    jo.read( "delivered_minutes", candidate.delivered_minutes );
    jo.read( "provisional", candidate.provisional );
    candidate.revision = std::max( 0, candidate.revision );
    candidate.source_generation = std::max( 0, candidate.source_generation );
    candidate.target_lead_revision = std::max( 0, candidate.target_lead_revision );
    candidate.delivered_minutes = std::max( -1, candidate.delivered_minutes );
    if( candidate.schema_version < 3 && candidate.source_job_type.empty() &&
        candidate.source_activity_id.find( "#scout" ) != std::string::npos ) {
        candidate.source_job_type = "scout";
    }
    candidate.schema_version = 5;
    if( !candidate.is_present() ) {
        candidate.clear();
    }
    *this = std::move( candidate );
}

void acted_report_summary::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "target_id", target_id.substr( 0, max_camp_lead_target_id_length ) );
    json.member( "target_omt", target_omt );
    json.member( "policy", to_string( policy ) );
    json.member( "source_generation", std::max( 0, source_generation ) );
    json.member( "report_revision", std::max( 0, report_revision ) );
    json.member( "acted_minutes", std::max( -1, acted_minutes ) );
    json.end_object();
}

void acted_report_summary::deserialize( const JsonObject &jo )
{
    acted_report_summary candidate;
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    std::string policy_string = "none";
    jo.read( "policy", policy_string );
    candidate.policy = camp_report_policy_from_string( policy_string ).value_or(
                           camp_report_policy::none );
    jo.read( "source_generation", candidate.source_generation );
    jo.read( "report_revision", candidate.report_revision );
    jo.read( "acted_minutes", candidate.acted_minutes );
    candidate.target_id.resize( std::min( candidate.target_id.size(),
                                         max_camp_lead_target_id_length ) );
    candidate.source_generation = std::max( 0, candidate.source_generation );
    candidate.report_revision = std::max( 0, candidate.report_revision );
    candidate.acted_minutes = std::max( -1, candidate.acted_minutes );
    *this = std::move( candidate );
}

void camp_decision_record::clear()
{
    *this = camp_decision_record();
}

bool camp_decision_record::has_pinned_report() const
{
    return source_report_revision > 0 && source_report_generation > 0 &&
           !source_report_activity_id.empty() && !source_report_application_key.empty();
}

void camp_decision_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", 2 );
    json.member( "state", to_string( state ) );
    json.member( "report_policy", to_string( report_policy ) );
    json.member( "source_report_revision", std::max( 0, source_report_revision ) );
    json.member( "source_report_generation", std::max( 0, source_report_generation ) );
    json.member( "source_report_activity_id", source_report_activity_id );
    json.member( "source_report_application_key", source_report_application_key );
    json.member( "target_id", target_id );
    json.member( "target_omt", target_omt );
    json.member( "target_lead_id", target_lead_id );
    json.member( "target_lead_revision", std::max( 0, target_lead_revision ) );
    json.member( "last_transition_minutes", std::max( -1, last_transition_minutes ) );
    json.member( "next_eligible_minutes", std::max( -1, next_eligible_minutes ) );
    json.member( "transition_reason",
                 transition_reason.substr( 0, max_camp_decision_reason_length ) );
    json.end_object();
}

void camp_decision_record::deserialize( const JsonObject &jo )
{
    camp_decision_record candidate;
    jo.read( "schema_version", candidate.schema_version );
    const bool state_was_present = jo.has_member( "state" );
    std::string state_string = "idle";
    jo.read( "state", state_string );
    candidate.state = camp_decision_state_from_string( state_string ).value_or(
                          state_was_present ? camp_decision_state::abandoned :
                          camp_decision_state::idle );
    const bool policy_was_present = jo.has_member( "report_policy" );
    std::string report_policy_string = "none";
    jo.read( "report_policy", report_policy_string );
    const std::optional<camp_report_policy> parsed_policy =
        camp_report_policy_from_string( report_policy_string );
    candidate.report_policy = parsed_policy.value_or( camp_report_policy::none );
    jo.read( "source_report_revision", candidate.source_report_revision );
    jo.read( "source_report_generation", candidate.source_report_generation );
    jo.read( "source_report_activity_id", candidate.source_report_activity_id );
    jo.read( "source_report_application_key", candidate.source_report_application_key );
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    jo.read( "target_lead_id", candidate.target_lead_id );
    jo.read( "target_lead_revision", candidate.target_lead_revision );
    jo.read( "last_transition_minutes", candidate.last_transition_minutes );
    jo.read( "next_eligible_minutes", candidate.next_eligible_minutes );
    jo.read( "transition_reason", candidate.transition_reason );
    candidate.schema_version = 2;
    candidate.source_report_revision = std::max( 0, candidate.source_report_revision );
    candidate.source_report_generation = std::max( 0, candidate.source_report_generation );
    candidate.target_lead_revision = std::max( 0, candidate.target_lead_revision );
    candidate.last_transition_minutes = std::max( -1, candidate.last_transition_minutes );
    candidate.next_eligible_minutes = std::max( -1, candidate.next_eligible_minutes );
    candidate.transition_reason.resize( std::min( candidate.transition_reason.size(),
                                        max_camp_decision_reason_length ) );
    if( !camp_decision_state_from_string( state_string ) &&
        candidate.transition_reason.empty() ) {
        candidate.transition_reason = "unknown persisted camp decision state";
    }
    if( policy_was_present && !parsed_policy ) {
        candidate.state = camp_decision_state::abandoned;
        candidate.next_eligible_minutes = -1;
        candidate.transition_reason = "unknown persisted camp report policy";
    }
    *this = std::move( candidate );
}

void scout_assessment_state::clear()
{
    *this = scout_assessment_state();
}

void scout_assessment_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "observation_started_minutes", observation_started_minutes );
    json.member( "last_progress_minutes", last_progress_minutes );
    json.member( "burned_minutes", burned_minutes );
    json.member( "burn_origin_omt", burn_origin_omt );
    json.member( "certainty", certainty );
    json.member( "readiness_latched", readiness_latched );
    json.member( "threshold_class",
                 scout_assessment_threshold_to_string( threshold_class ) );
    json.member( "strong_visual_windows", strong_visual_windows );
    json.member( "defenders_low", defenders_low );
    json.member( "defenders_high", defenders_high );
    json.member( "danger_low", danger_low );
    json.member( "danger_high", danger_high );
    json.member( "target_alert", target_alert );
    json.member( "pinned_target_revision", pinned_target_revision );
    json.member( "next_eligible_minutes", next_eligible_minutes );
    json.member( "exit_reason", exit_reason.substr( 0, max_sortie_summary_length ) );
    json.end_object();
}

void scout_assessment_state::deserialize( const JsonObject &jo )
{
    scout_assessment_state candidate;
    jo.read( "schema_version", candidate.schema_version );
    if( candidate.schema_version != 1 ) {
        jo.throw_error( "scout assessment schema version is not supported schema v1" );
    }
    jo.read( "observation_started_minutes", candidate.observation_started_minutes );
    jo.read( "last_progress_minutes", candidate.last_progress_minutes );
    jo.read( "burned_minutes", candidate.burned_minutes );
    jo.read( "burn_origin_omt", candidate.burn_origin_omt );
    jo.read( "certainty", candidate.certainty );
    jo.read( "readiness_latched", candidate.readiness_latched );
    std::string threshold_string;
    jo.read( "threshold_class", threshold_string );
    const std::optional<scout_assessment_threshold_class> threshold =
        scout_assessment_threshold_from_string( threshold_string );
    if( !threshold ) {
        jo.throw_error( "scout assessment has an invalid threshold class" );
    }
    candidate.threshold_class = *threshold;
    jo.read( "strong_visual_windows", candidate.strong_visual_windows );
    jo.read( "defenders_low", candidate.defenders_low );
    jo.read( "defenders_high", candidate.defenders_high );
    jo.read( "danger_low", candidate.danger_low );
    jo.read( "danger_high", candidate.danger_high );
    jo.read( "target_alert", candidate.target_alert );
    jo.read( "pinned_target_revision", candidate.pinned_target_revision );
    jo.read( "next_eligible_minutes", candidate.next_eligible_minutes );
    jo.read( "exit_reason", candidate.exit_reason );
    const bool valid = candidate.observation_started_minutes >= -1 &&
                       candidate.last_progress_minutes >= -1 &&
                       candidate.burned_minutes >= -1 &&
                       candidate.certainty >= 0 && candidate.certainty <= 95 &&
                       candidate.strong_visual_windows >= 0 &&
                       candidate.strong_visual_windows <= 3 &&
                       candidate.defenders_low >= 0 &&
                       candidate.defenders_high >= candidate.defenders_low &&
                       candidate.danger_low >= 0 &&
                       candidate.danger_high >= candidate.danger_low &&
                       candidate.danger_high <= 200 &&
                       candidate.target_alert >= 0 && candidate.target_alert <= 100 &&
                       candidate.pinned_target_revision >= 0 &&
                       candidate.next_eligible_minutes >= -1 &&
                       candidate.exit_reason.size() <= max_sortie_summary_length &&
                       ( candidate.observation_started_minutes < 0 ||
                         candidate.last_progress_minutes >=
                         candidate.observation_started_minutes ) &&
                       ( !candidate.readiness_latched ||
                         candidate.threshold_class !=
                         scout_assessment_threshold_class::none );
    if( !valid ) {
        jo.throw_error( "scout assessment has malformed bounded state" );
    }
    *this = std::move( candidate );
}

void active_outing_state::clear()
{
    *this = active_outing_state();
}

bool active_outing_state::is_active() const
{
    return !activity_id.empty() && kind != outing_kind::none && generation > 0;
}

bool active_outing_state::member_is_resolved( const character_id npc_id ) const
{
    return std::find( resolved_member_ids.begin(), resolved_member_ids.end(), npc_id ) !=
           resolved_member_ids.end();
}

void active_outing_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "kind", to_string( kind ) );
    json.member( "activity_id", activity_id );
    json.member( "camp_id", camp_id );
    json.member( "generation", generation );
    std::vector<int> raw_member_ids;
    raw_member_ids.reserve( member_ids.size() );
    for( const character_id &member_id : member_ids ) {
        raw_member_ids.push_back( member_id.get_value() );
    }
    json.member( "member_ids", raw_member_ids );
    json.member( "leader_id", leader_id.get_value() );
    const bounded_route_state bounded_route = make_bounded_route_state( shared_route, waypoint_index );
    json.member( "shared_route", bounded_route.route );
    json.member( "waypoint_index", bounded_route.waypoint_index );
    json.member( "target_id", target_id );
    json.member( "target_omt", target_omt );
    json.member( "job_type", job_type );
    json.member( "target_lead_id", target_lead_id );
    json.member( "target_lead_revision", std::max( 0, target_lead_revision ) );
    json.member( "phase", to_string( phase ) );
    json.member( "observations", make_bounded_sortie_observations( observations ) );
    json.member( "cargo", cargo );
    std::vector<int> raw_casualty_ids;
    raw_casualty_ids.reserve( std::min( casualty_ids.size(), max_active_outing_casualties ) );
    for( std::size_t index = 0; index < casualty_ids.size() &&
         index < max_active_outing_casualties; ++index ) {
        raw_casualty_ids.push_back( casualty_ids[index].get_value() );
    }
    json.member( "casualty_ids", raw_casualty_ids );
    std::vector<int> raw_resolved_member_ids;
    raw_resolved_member_ids.reserve( std::min( resolved_member_ids.size(),
                                     max_active_outing_members ) );
    for( std::size_t index = 0; index < resolved_member_ids.size() &&
         index < max_active_outing_members; ++index ) {
        raw_resolved_member_ids.push_back( resolved_member_ids[index].get_value() );
    }
    json.member( "resolved_member_ids", raw_resolved_member_ids );
    json.member( "started_minutes", std::max( -1, started_minutes ) );
    json.member( "local_contact_minutes", std::max( -1, local_contact_minutes ) );
    json.member( "last_progress_minutes", std::max( -1, last_progress_minutes ) );
    json.member( "expected_return_minutes", std::max( -1, expected_return_minutes ) );
    json.member( "missing_deadline_minutes", std::max( -1, missing_deadline_minutes ) );
    json.member( "simulation_owner", to_string( owner ) );
    json.member( "handoff_epoch", handoff_epoch );
    json.member( "last_advanced_minutes", last_advanced_minutes );
    json.member( "return_application_key", return_application_key );
    json.member( "report_application_key", report_application_key );
    json.member( "cargo_application_key", cargo_application_key );
    if( schema_version >= 7 ) {
        json.member( "local_handoff", local_handoff );
    }
    if( schema_version >= 8 ) {
        json.member( "abstract_encounter", abstract_encounter );
        json.member( "abstract_detour_attempts", abstract_detour_attempts );
        json.member( "has_withdrawal_detour", has_withdrawal_detour );
        json.member( "withdrawal_detour_omt", withdrawal_detour_omt );
    }
    if( schema_version >= 9 ) {
        json.member( "target_footprint", target_footprint );
        json.member( "selected_watch_kind", to_string( selected_watch_kind ) );
        json.member( "selected_watch_omt", selected_watch_omt );
        json.member( "selected_watch_route_cost", selected_watch_route_cost );
    }
    if( schema_version >= 10 ) {
        json.member( "alternate_watch_kind", to_string( alternate_watch_kind ) );
        json.member( "alternate_watch_omt", alternate_watch_omt );
        json.member( "alternate_watch_route_cost", alternate_watch_route_cost );
        json.member( "alternate_watch_shared_route", alternate_watch_shared_route );
        json.member( "alternate_watch_attempted", alternate_watch_attempted );
        json.member( "alternate_watch_reposition_pending",
                     alternate_watch_reposition_pending );
        json.member( "covert_egress_chain_version", covert_egress_chain_version );
        json.member( "covert_egress_attempts", covert_egress_attempts );
        json.member( "covert_egress_revision", covert_egress_revision );
        json.member( "failed_covert_egress_omts", failed_covert_egress_omts );
        json.member( "current_covert_egress_route_omts", current_covert_egress_route_omts );
        json.member( "failed_covert_egress_route_omts", failed_covert_egress_route_omts );
        json.member( "assessment", assessment );
    }
    json.end_object();
}

void active_outing_state::deserialize( const JsonObject &jo )
{
    active_outing_state candidate;
    candidate.schema_version = 0;
    jo.read( "schema_version", candidate.schema_version );
    const int loaded_schema_version = candidate.schema_version;
    std::string kind_string = "none";
    jo.read( "kind", kind_string );
    candidate.kind = outing_kind_from_string( kind_string ).value_or( outing_kind::none );
    jo.read( "activity_id", candidate.activity_id );
    jo.read( "camp_id", candidate.camp_id );
    jo.read( "generation", candidate.generation );
    std::vector<int> raw_member_ids;
    jo.read( "member_ids", raw_member_ids );
    for( const int raw_member_id : raw_member_ids ) {
        character_id member_id;
        member_id.deserialize( raw_member_id );
        candidate.member_ids.push_back( member_id );
    }
    int raw_leader_id = -1;
    jo.read( "leader_id", raw_leader_id );
    candidate.leader_id.deserialize( raw_leader_id );
    jo.read( "shared_route", candidate.shared_route );
    jo.read( "waypoint_index", candidate.waypoint_index );
    if( loaded_schema_version >= 6 && loaded_schema_version <= 10 &&
        ( candidate.kind != outing_kind::structural_sortie ||
          candidate.shared_route.size() < 3 || candidate.shared_route.size() > 5 ||
          candidate.waypoint_index < 0 ||
          candidate.waypoint_index >= static_cast<int>( candidate.shared_route.size() ) ) ) {
        jo.throw_error( "current structural outing has malformed route state" );
    }
    const bounded_route_state bounded_route = make_bounded_route_state( candidate.shared_route,
                                              candidate.waypoint_index );
    candidate.shared_route = bounded_route.route;
    candidate.waypoint_index = bounded_route.waypoint_index;
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    jo.read( "job_type", candidate.job_type );
    jo.read( "target_lead_id", candidate.target_lead_id );
    jo.read( "target_lead_revision", candidate.target_lead_revision );
    candidate.target_lead_revision = std::max( 0, candidate.target_lead_revision );
    const bool phase_was_present = jo.has_member( "phase" );
    std::string phase_string = "assembling";
    jo.read( "phase", phase_string );
    candidate.phase = scout_phase_from_string( phase_string ).value_or(
                          phase_was_present ? scout_phase::lost : scout_phase::assembling );
    jo.read( "observations", candidate.observations );
    candidate.observations = make_bounded_sortie_observations( candidate.observations );
    jo.read( "cargo", candidate.cargo );
    std::vector<int> raw_casualty_ids;
    jo.read( "casualty_ids", raw_casualty_ids );
    for( const int raw_casualty_id : raw_casualty_ids ) {
        character_id casualty_id;
        casualty_id.deserialize( raw_casualty_id );
        if( std::find( candidate.member_ids.begin(), candidate.member_ids.end(), casualty_id ) !=
            candidate.member_ids.end() &&
            std::find( candidate.casualty_ids.begin(), candidate.casualty_ids.end(), casualty_id ) ==
            candidate.casualty_ids.end() ) {
            candidate.casualty_ids.push_back( casualty_id );
        }
        if( candidate.casualty_ids.size() >= max_active_outing_casualties ) {
            break;
        }
    }
    std::vector<int> raw_resolved_member_ids;
    jo.read( "resolved_member_ids", raw_resolved_member_ids );
    for( const int raw_resolved_member_id : raw_resolved_member_ids ) {
        character_id resolved_member_id;
        resolved_member_id.deserialize( raw_resolved_member_id );
        if( std::find( candidate.member_ids.begin(), candidate.member_ids.end(), resolved_member_id ) !=
            candidate.member_ids.end() &&
            std::find( candidate.resolved_member_ids.begin(), candidate.resolved_member_ids.end(),
                       resolved_member_id ) == candidate.resolved_member_ids.end() ) {
            candidate.resolved_member_ids.push_back( resolved_member_id );
        }
        if( candidate.resolved_member_ids.size() >= max_active_outing_members ) {
            break;
        }
    }
    for( const character_id &casualty_id : candidate.casualty_ids ) {
        if( std::find( candidate.resolved_member_ids.begin(), candidate.resolved_member_ids.end(),
                      casualty_id ) == candidate.resolved_member_ids.end() ) {
            candidate.resolved_member_ids.push_back( casualty_id );
        }
    }
    jo.read( "started_minutes", candidate.started_minutes );
    jo.read( "local_contact_minutes", candidate.local_contact_minutes );
    jo.read( "last_progress_minutes", candidate.last_progress_minutes );
    jo.read( "expected_return_minutes", candidate.expected_return_minutes );
    jo.read( "missing_deadline_minutes", candidate.missing_deadline_minutes );
    candidate.started_minutes = std::max( -1, candidate.started_minutes );
    candidate.local_contact_minutes = std::max( -1, candidate.local_contact_minutes );
    candidate.last_progress_minutes = std::max( -1, candidate.last_progress_minutes );
    candidate.expected_return_minutes = std::max( -1, candidate.expected_return_minutes );
    candidate.missing_deadline_minutes = std::max( -1, candidate.missing_deadline_minutes );
    if( candidate.expected_return_minutes >= 0 &&
        candidate.missing_deadline_minutes < candidate.expected_return_minutes ) {
        candidate.missing_deadline_minutes = minutes_after_saturated( candidate.expected_return_minutes,
                                             scout_missing_grace_minutes );
    }
    std::string owner_string = "abstract";
    jo.read( "simulation_owner", owner_string );
    candidate.owner = simulation_owner_from_string( owner_string ).value_or(
                          simulation_owner::abstract );
    jo.read( "handoff_epoch", candidate.handoff_epoch );
    jo.read( "last_advanced_minutes", candidate.last_advanced_minutes );
    jo.read( "return_application_key", candidate.return_application_key );
    jo.read( "report_application_key", candidate.report_application_key );
    jo.read( "cargo_application_key", candidate.cargo_application_key );
    if( jo.has_member( "local_handoff" ) ) {
        jo.read( "local_handoff", candidate.local_handoff );
    } else if( loaded_schema_version >= 7 ) {
        jo.throw_error( "current structural outing is missing local handoff state" );
    }
    if( jo.has_member( "abstract_encounter" ) ) {
        jo.read( "abstract_encounter", candidate.abstract_encounter );
    } else if( loaded_schema_version >= 8 ) {
        jo.throw_error( "schema-v8 structural outing is missing abstract encounter state" );
    }
    jo.read( "abstract_detour_attempts", candidate.abstract_detour_attempts );
    jo.read( "has_withdrawal_detour", candidate.has_withdrawal_detour );
    jo.read( "withdrawal_detour_omt", candidate.withdrawal_detour_omt );
    if( candidate.abstract_detour_attempts < 0 || candidate.abstract_detour_attempts > 2 ||
        ( candidate.has_withdrawal_detour && candidate.abstract_detour_attempts <= 0 ) ) {
        jo.throw_error( "current structural outing has malformed abstract detour state" );
    }
    if( loaded_schema_version >= 9 ) {
        if( !jo.has_member( "target_footprint" ) ||
            !jo.has_member( "selected_watch_kind" ) ||
            !jo.has_member( "selected_watch_omt" ) ||
            !jo.has_member( "selected_watch_route_cost" ) ) {
            jo.throw_error( "schema-v9 structural outing is missing watch route state" );
        }
        jo.read( "target_footprint", candidate.target_footprint );
        std::string watch_kind_string;
        jo.read( "selected_watch_kind", watch_kind_string );
        const std::optional<structural_watch_kind> watch_kind =
            structural_watch_kind_from_string( watch_kind_string );
        if( !watch_kind ) {
            jo.throw_error( "schema-v9 structural outing has invalid watch route kind" );
        }
        candidate.selected_watch_kind = *watch_kind;
        jo.read( "selected_watch_omt", candidate.selected_watch_omt );
        jo.read( "selected_watch_route_cost", candidate.selected_watch_route_cost );
    } else if( loaded_schema_version == 8 &&
               candidate.kind == outing_kind::structural_sortie ) {
        candidate.target_footprint = { candidate.target_omt };
        candidate.schema_version = 9;
    }
    if( loaded_schema_version >= 10 ) {
        const bool has_alternate_kind = jo.has_member( "alternate_watch_kind" );
        const bool has_alternate_omt = jo.has_member( "alternate_watch_omt" );
        const bool has_alternate_cost = jo.has_member( "alternate_watch_route_cost" );
        const bool has_alternate_route = jo.has_member( "alternate_watch_shared_route" );
        if( has_alternate_kind != has_alternate_omt ||
            has_alternate_kind != has_alternate_cost ||
            has_alternate_kind != has_alternate_route ) {
            jo.throw_error( "schema-v10 structural outing has incomplete alternate watch state" );
        }
        if( has_alternate_kind ) {
            std::string alternate_kind_string;
            jo.read( "alternate_watch_kind", alternate_kind_string );
            const std::optional<structural_watch_kind> alternate_kind =
                structural_watch_kind_from_string( alternate_kind_string );
            if( !alternate_kind ) {
                jo.throw_error( "schema-v10 structural outing has invalid alternate watch kind" );
            }
            candidate.alternate_watch_kind = *alternate_kind;
            jo.read( "alternate_watch_omt", candidate.alternate_watch_omt );
            jo.read( "alternate_watch_route_cost",
                     candidate.alternate_watch_route_cost );
            jo.read( "alternate_watch_shared_route",
                     candidate.alternate_watch_shared_route );
        }
        jo.read( "alternate_watch_attempted", candidate.alternate_watch_attempted );
        jo.read( "alternate_watch_reposition_pending",
                 candidate.alternate_watch_reposition_pending );
        if( jo.has_member( "covert_egress_chain_version" ) ) {
            jo.read( "covert_egress_chain_version", candidate.covert_egress_chain_version );
        }
        const bool has_attempts = jo.has_member( "covert_egress_attempts" );
        const bool has_revision = jo.has_member( "covert_egress_revision" );
        const bool has_failed_egress = jo.has_member( "failed_covert_egress_omts" );
        const bool has_current_route = jo.has_member( "current_covert_egress_route_omts" );
        const bool has_failed_routes = jo.has_member( "failed_covert_egress_route_omts" );
        if( has_attempts != has_revision || has_attempts != has_failed_egress ||
            has_attempts != has_current_route ||
            has_attempts != has_failed_routes ) {
            jo.throw_error( "schema-v10 structural outing has incomplete covert egress retry state" );
        }
        if( has_attempts ) {
            jo.read( "covert_egress_attempts", candidate.covert_egress_attempts );
            jo.read( "covert_egress_revision", candidate.covert_egress_revision );
            jo.read( "failed_covert_egress_omts", candidate.failed_covert_egress_omts );
            jo.read( "current_covert_egress_route_omts",
                     candidate.current_covert_egress_route_omts );
            jo.read( "failed_covert_egress_route_omts",
                     candidate.failed_covert_egress_route_omts );
        } else if( active_outing_has_current_covert_burn_receipt( candidate ) ) {
            // Schema 10 predates explicit retry memory.  Every retained authoritative burn starts
            // revision one; only a pair still withdrawing has a pending first route attempt.
            candidate.covert_egress_attempts =
                candidate.phase == scout_phase::burned_withdrawal ? 1 : 0;
            candidate.covert_egress_revision = 1;
        }
        if( jo.has_member( "assessment" ) ) {
            jo.read( "assessment", candidate.assessment );
        } else if( candidate.kind == outing_kind::structural_sortie ) {
            candidate.assessment.pinned_target_revision = candidate.target_lead_revision;
            if( candidate.phase == scout_phase::observing &&
                candidate.selected_watch_kind != structural_watch_kind::none &&
                candidate.waypoint_index ==
                structural_outing_destination_waypoint( candidate ) ) {
                int earliest_target_evidence = std::numeric_limits<int>::max();
                for( const sortie_observation &observation : candidate.observations ) {
                    const bool current_target = observation.record_schema_version == 1 &&
                            observation.target_revision == candidate.target_lead_revision &&
                            ( observation.kind == sortie_observation_kind::burn ||
                              std::find( candidate.target_footprint.begin(),
                                         candidate.target_footprint.end(),
                                         observation.source_omt ) !=
                              candidate.target_footprint.end() );
                    if( current_target && observation.observed_minutes >= 0 ) {
                        earliest_target_evidence = std::min(
                                                       earliest_target_evidence,
                                                       observation.observed_minutes );
                    }
                }
                const int earliest_possible_watch = std::max(
                        candidate.started_minutes, candidate.local_contact_minutes );
                candidate.assessment.observation_started_minutes =
                    earliest_target_evidence == std::numeric_limits<int>::max() ?
                    std::max( earliest_possible_watch, candidate.last_progress_minutes ) :
                    std::max( earliest_possible_watch, earliest_target_evidence );
                candidate.assessment.last_progress_minutes =
                    candidate.assessment.observation_started_minutes;
            }
        }
    }
    if( !structural_watch_route_state_is_consistent( candidate ) ) {
        jo.throw_error( "current structural outing has malformed watch route state" );
    }
    if( !covert_scout_egress_retry_state_is_consistent( candidate ) ) {
        jo.throw_error( "current structural outing has malformed covert egress retry state" );
    }
    if( candidate.activity_id.empty() || candidate.kind == outing_kind::none ||
        candidate.generation <= 0 ) {
        std::vector<character_id> reserved_member_ids = std::move( candidate.member_ids );
        candidate.clear();
        candidate.member_ids = std::move( reserved_member_ids );
    } else {
        if( candidate.member_ids.empty() ) {
            candidate.leader_id = character_id();
        } else if( std::find( candidate.member_ids.begin(), candidate.member_ids.end(),
                             candidate.leader_id ) == candidate.member_ids.end() ) {
            candidate.leader_id = candidate.member_ids.front();
        }
        normalize_legacy_simulation_owner_state( candidate );
        const std::string expected_return_key =
            bandit_pursuit_handoff::make_operation_component_key(
                candidate.activity_id, candidate.generation, "return" );
        const std::string expected_report_key =
            bandit_pursuit_handoff::make_operation_component_key(
                candidate.activity_id, candidate.generation, "report" );
        const std::string expected_cargo_key =
            bandit_pursuit_handoff::make_operation_component_key(
                candidate.activity_id, candidate.generation, "cargo" );
        if( loaded_schema_version < 5 ) {
            candidate.return_application_key = expected_return_key;
            candidate.report_application_key = expected_report_key;
            candidate.cargo_application_key = expected_cargo_key;
        } else if( ( loaded_schema_version != 5 && loaded_schema_version != 6 &&
                     loaded_schema_version != 7 && loaded_schema_version != 8 &&
                     loaded_schema_version != 9 && loaded_schema_version != 10 ) ||
                   ( loaded_schema_version >= 6 &&
                     candidate.kind != outing_kind::structural_sortie ) ||
                   candidate.return_application_key != expected_return_key ||
                   candidate.report_application_key != expected_report_key ||
                   candidate.cargo_application_key != expected_cargo_key ) {
            jo.throw_error( "active outing has non-canonical component application keys" );
        }
        candidate.schema_version = loaded_schema_version == 8 ? 9 :
                                   loaded_schema_version >= 6 ? loaded_schema_version : 5;
    }
    *this = std::move( candidate );
}

void hostile_operation_state::clear()
{
    *this = hostile_operation_state();
}

bool hostile_operation_state::is_active() const
{
    return operation_kind != hostile_operation_kind::none && reservation.is_active() &&
           reservation.kind == outing_kind::hostile_operation;
}

void hostile_operation_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "operation_kind", to_string( operation_kind ) );
    json.member( "phase", to_string( phase ) );
    json.member( "reservation", reservation );
    json.member( "source_report_revision", std::max( 0, source_report_revision ) );
    json.member( "source_report_generation", std::max( 0, source_report_generation ) );
    json.member( "source_report_activity_id", source_report_activity_id );
    json.member( "source_report_application_key", source_report_application_key );
    json.member( "has_rally", has_rally );
    json.member( "rally_omt", rally_omt );
    json.member( "last_transition_reason",
                 last_transition_reason.substr( 0, max_camp_decision_reason_length ) );
    json.member( "legacy_unpinned", legacy_unpinned );
    json.end_object();
}

void hostile_operation_state::deserialize( const JsonObject &jo )
{
    hostile_operation_state candidate;
    jo.read( "schema_version", candidate.schema_version );
    if( candidate.schema_version > 1 ) {
        jo.throw_error( "hostile operation schema version is newer than supported schema v1" );
    }
    std::string kind_string = "none";
    jo.read( "operation_kind", kind_string );
    candidate.operation_kind = hostile_operation_kind_from_string( kind_string ).value_or(
                                   hostile_operation_kind::none );
    const bool phase_was_present = jo.has_member( "phase" );
    std::string phase_string = "assembling";
    jo.read( "phase", phase_string );
    candidate.phase = hostile_operation_phase_from_string( phase_string ).value_or(
                          phase_was_present ? hostile_operation_phase::lost :
                          hostile_operation_phase::assembling );
    if( jo.has_member( "reservation" ) ) {
        jo.read( "reservation", candidate.reservation );
    }
    jo.read( "source_report_revision", candidate.source_report_revision );
    jo.read( "source_report_generation", candidate.source_report_generation );
    jo.read( "source_report_activity_id", candidate.source_report_activity_id );
    jo.read( "source_report_application_key", candidate.source_report_application_key );
    jo.read( "has_rally", candidate.has_rally );
    jo.read( "rally_omt", candidate.rally_omt );
    jo.read( "last_transition_reason", candidate.last_transition_reason );
    jo.read( "legacy_unpinned", candidate.legacy_unpinned );
    candidate.schema_version = 1;
    candidate.source_report_revision = std::max( 0, candidate.source_report_revision );
    candidate.source_report_generation = std::max( 0, candidate.source_report_generation );
    candidate.last_transition_reason.resize( std::min( candidate.last_transition_reason.size(),
                                             max_camp_decision_reason_length ) );
    if( candidate.reservation.is_active() ) {
        candidate.reservation.kind = outing_kind::hostile_operation;
    }
    *this = std::move( candidate );
}

void site_record::serialize( JsonOut &json ) const
{
    site_record bounded = *this;
    normalize_camp_intelligence( bounded );
    normalize_acted_reports( bounded );
    json.start_object();
    json.member( "schema_version", 12 );
    json.member( "site_id", site_id );
    json.member( "source_kind", to_string( source_kind ) );
    json.member( "site_kind", to_string( site_kind ) );
    json.member( "hostile_profile", to_string( effective_profile( *this ) ) );
    json.member( "source_id", source_id );
    json.member( "anchor", anchor );
    json.member( "living_total", living_total );
    json.member( "supply_units", supply_units );
    json.member( "supply_last_update_minutes", supply_last_update_minutes );
    json.member( "supply_accounted_living_total", supply_accounted_living_total );
    json.member( "supply_member_minute_remainder", supply_member_minute_remainder );
    json.member( "routine_activated_minutes", routine_activated_minutes );
    json.member( "last_routine_resolved_minutes", last_routine_resolved_minutes );
    json.member( "next_routine_dispatch_eligible_minutes",
                 next_routine_dispatch_eligible_minutes );
    json.member( "routine_no_candidate_streak", routine_no_candidate_streak );
    json.member( "footprint", footprint );
    json.member( "members", members );
    json.member( "spawn_tiles", spawn_tiles );
    json.member( "next_outing_generation", next_outing_generation );
    json.member( "applied_return_generation", applied_return_generation );
    json.member( "applied_report_generation", applied_report_generation );
    json.member( "applied_cargo_generation", applied_cargo_generation );
    json.member( "last_cargo_application_key", last_cargo_application_key );
    json.member( "applied_resource_generation", applied_resource_generation );
    json.member( "last_resource_application_key", last_resource_application_key );
    json.member( "last_resource_claimed_units", last_resource_claimed_units );
    json.member( "current_scout_report", bounded.current_scout_report );
    json.member( "camp_decision", bounded.camp_decision );
    json.member( "acted_reports", bounded.acted_reports );
    json.member( "returned_cargo_stock", returned_cargo_stock );
    json.member( "active_outing", bounded.active_outing );
    json.member( "active_hostile_operation", bounded.active_hostile_operation );
    json.member( "remembered_target_or_mark", remembered_target_or_mark );
    json.member( "remembered_threat_estimate", remembered_threat_estimate );
    json.member( "remembered_bounty_estimate", remembered_bounty_estimate );
    json.member( "remembered_retreat_bias", remembered_retreat_bias );
    json.member( "remembered_return_clock", remembered_return_clock );
    json.member( "remembered_pressure", bandit_pursuit_handoff::to_string( remembered_pressure ) );
    json.member( "known_recent_marks", bounded.known_recent_marks );
    json.member( "intelligence_map", bounded.intelligence_map );
    json.member( "last_shakedown_outcome", last_shakedown_outcome );
    json.member( "shakedown_last_demanded_value", shakedown_last_demanded_value );
    json.member( "shakedown_last_surrendered_value", shakedown_last_surrendered_value );
    json.member( "shakedown_last_reachable_value", shakedown_last_reachable_value );
    json.member( "shakedown_loot_value", shakedown_loot_value );
    json.member( "shakedown_defender_losses", shakedown_defender_losses );
    json.member( "shakedown_bandit_losses", shakedown_bandit_losses );
    json.member( "shakedown_anger", shakedown_anger );
    json.member( "shakedown_caution", shakedown_caution );
    json.member( "shakedown_basecamp_defenders_at_fight", shakedown_basecamp_defenders_at_fight );
    json.member( "shakedown_basecamp_defender_observation_pending",
                 shakedown_basecamp_defender_observation_pending );
    json.member( "shakedown_reopen_available", shakedown_reopen_available );
    json.member( "shakedown_reopen_used", shakedown_reopen_used );
    json.member( "origin_disposition", to_string( origin ) );
    json.member( "origin_changed_minutes", origin_changed_minutes );
    json.member( "origin_summary", origin_summary.substr( 0, max_sortie_summary_length ) );
    json.member( "retired_empty_site", retired_empty_site );
    json.member( "retirement_summary", retirement_summary );
    json.end_object();
}

void site_record::deserialize( const JsonObject &jo )
{
    schema_version = 5;
    living_total = 0;
    supply_units = 0;
    supply_last_update_minutes = -1;
    supply_accounted_living_total = 0;
    supply_member_minute_remainder = 0;
    routine_activated_minutes = -1;
    last_routine_resolved_minutes = -1;
    next_routine_dispatch_eligible_minutes = -1;
    routine_no_candidate_streak = 0;
    next_outing_generation = 1;
    applied_return_generation = 0;
    applied_report_generation = 0;
    applied_cargo_generation = 0;
    last_cargo_application_key.clear();
    applied_resource_generation = 0;
    last_resource_application_key.clear();
    last_resource_claimed_units = 0;
    current_scout_report.clear();
    camp_decision.clear();
    acted_reports.clear();
    returned_cargo_stock = sortie_cargo();
    active_outing.clear();
    active_hostile_operation.clear();
    origin = origin_disposition::active_hostile;
    origin_changed_minutes = -1;
    origin_summary.clear();
    jo.read( "schema_version", schema_version );
    const int loaded_schema_version = schema_version;
    if( loaded_schema_version > 12 ) {
        jo.throw_error( "site schema version is newer than supported schema v12" );
    }
    jo.read( "site_id", site_id );
    std::string source_kind_string = "none";
    jo.read( "source_kind", source_kind_string );
    source_kind = anchor_source_kind_from_string( source_kind_string ).value_or( anchor_source_kind::none );
    std::string site_kind_string = "none";
    jo.read( "site_kind", site_kind_string );
    site_kind = owned_site_kind_from_string( site_kind_string ).value_or( owned_site_kind::none );
    std::string profile_string = "none";
    jo.read( "hostile_profile", profile_string );
    profile = hostile_site_profile_from_string( profile_string ).value_or( profile_for_site_kind( site_kind ) );
    if( profile == hostile_site_profile::none ) {
        profile = profile_for_site_kind( site_kind );
    }
    jo.read( "source_id", source_id );
    jo.read( "anchor", anchor );
    int legacy_headcount = 0;
    if( loaded_schema_version < 10 ) {
        jo.read( "headcount", legacy_headcount );
    } else {
        if( !jo.has_member( "living_total" ) || jo.has_member( "headcount" ) ) {
            jo.throw_error( "v10 site must contain living_total and cannot contain legacy headcount" );
        }
        jo.read( "living_total", living_total );
    }
    const bool complete_supply_payload = jo.has_member( "supply_units" ) &&
                                         jo.has_member( "supply_last_update_minutes" ) &&
                                         jo.has_member( "supply_accounted_living_total" ) &&
                                         jo.has_member( "supply_member_minute_remainder" );
    if( loaded_schema_version >= 10 && !complete_supply_payload ) {
        jo.throw_error( "v10+ site must contain a complete supply payload" );
    }
    jo.read( "supply_units", supply_units );
    jo.read( "supply_last_update_minutes", supply_last_update_minutes );
    jo.read( "supply_accounted_living_total", supply_accounted_living_total );
    jo.read( "supply_member_minute_remainder", supply_member_minute_remainder );
    const bool any_routine_scheduler_field = jo.has_member( "routine_activated_minutes" ) ||
            jo.has_member( "last_routine_resolved_minutes" ) ||
            jo.has_member( "next_routine_dispatch_eligible_minutes" ) ||
            jo.has_member( "routine_no_candidate_streak" );
    const bool complete_routine_scheduler_payload =
        jo.has_member( "routine_activated_minutes" ) &&
        jo.has_member( "last_routine_resolved_minutes" ) &&
        jo.has_member( "next_routine_dispatch_eligible_minutes" ) &&
        jo.has_member( "routine_no_candidate_streak" );
    if( loaded_schema_version < 12 && any_routine_scheduler_field ) {
        jo.throw_error( "pre-v12 site cannot contain routine scheduler state" );
    }
    if( loaded_schema_version >= 12 && !complete_routine_scheduler_payload ) {
        jo.throw_error( "v12 site must contain complete routine scheduler state" );
    }
    if( complete_routine_scheduler_payload ) {
        jo.read( "routine_activated_minutes", routine_activated_minutes );
        jo.read( "last_routine_resolved_minutes", last_routine_resolved_minutes );
        jo.read( "next_routine_dispatch_eligible_minutes",
                 next_routine_dispatch_eligible_minutes );
        jo.read( "routine_no_candidate_streak", routine_no_candidate_streak );
        const bool activation_is_consistent = routine_activated_minutes >= -1 &&
                last_routine_resolved_minutes >= -1 &&
                next_routine_dispatch_eligible_minutes >= -1 &&
                routine_no_candidate_streak >= 0 && routine_no_candidate_streak <= 3 &&
                ( routine_activated_minutes >= 0 ||
                  ( last_routine_resolved_minutes == -1 &&
                    next_routine_dispatch_eligible_minutes == -1 &&
                    routine_no_candidate_streak == 0 ) ) &&
                ( next_routine_dispatch_eligible_minutes < 0 ||
                  next_routine_dispatch_eligible_minutes >= routine_activated_minutes ) &&
                ( last_routine_resolved_minutes < 0 ||
                  last_routine_resolved_minutes >= routine_activated_minutes ) &&
                ( last_routine_resolved_minutes < 0 ||
                  next_routine_dispatch_eligible_minutes < 0 ||
                  next_routine_dispatch_eligible_minutes >= last_routine_resolved_minutes );
        if( !activation_is_consistent ) {
            jo.throw_error( "v12 site has malformed routine scheduler state" );
        }
    }
    jo.read( "footprint", footprint );
    jo.read( "members", members );
    const std::string migrated_member_template =
        effective_profile( *this ) == hostile_site_profile::cannibal_camp ?
        "cannibal_hunter" : "bandit";
    for( member_record &member : members ) {
        if( member.npc_template_id.empty() ) {
            member.npc_template_id = migrated_member_template;
        }
    }
    if( loaded_schema_version < 10 ) {
        living_total = std::max( std::max( 0, legacy_headcount ), count_live_members() );
    } else if( living_total < count_live_members() ) {
        jo.throw_error( "v10 site living_total is smaller than its concrete living roster" );
    }
    if( loaded_schema_version >= 10 && jo.has_member( "spawn_tiles" ) ) {
        for( JsonObject spawn_tile_json : jo.get_array( "spawn_tiles" ) ) {
            if( !spawn_tile_json.has_member( "assigned_living_total" ) ||
                spawn_tile_json.has_member( "headcount" ) ) {
                spawn_tile_json.allow_omitted_members();
                spawn_tile_json.throw_error(
                    "v10 spawn tile must contain assigned_living_total and cannot contain legacy headcount" );
            }
            spawn_tile_json.allow_omitted_members();
        }
    }
    jo.read( "spawn_tiles", spawn_tiles );
    std::vector<spawn_tile_record> unique_spawn_tiles;
    unique_spawn_tiles.reserve( spawn_tiles.size() );
    for( const spawn_tile_record &spawn_tile : spawn_tiles ) {
        auto duplicate = std::find_if( unique_spawn_tiles.begin(), unique_spawn_tiles.end(),
        [&spawn_tile]( const spawn_tile_record & existing ) {
            return existing.tile == spawn_tile.tile;
        } );
        if( duplicate == unique_spawn_tiles.end() ) {
            unique_spawn_tiles.push_back( spawn_tile );
        } else if( loaded_schema_version >= 10 ) {
            jo.throw_error( "v10+ site contains duplicate spawn tile authorities" );
        } else {
            duplicate->assigned_living_total = std::max(
                                                   duplicate->assigned_living_total,
                                                   spawn_tile.assigned_living_total );
        }
    }
    spawn_tiles = std::move( unique_spawn_tiles );
    for( const member_record &member : members ) {
        if( !counts_toward_live_headcount( member.state ) ||
            find_spawn_tile( member.home_spawn_tile ) != nullptr ) {
            continue;
        }
        if( loaded_schema_version >= 10 ) {
            jo.throw_error( "v10+ concrete living member has no spawn tile authority" );
        }
        spawn_tile_record migrated_spawn_tile;
        migrated_spawn_tile.tile = member.home_spawn_tile;
        spawn_tiles.push_back( migrated_spawn_tile );
    }
    long long assigned_living_total = 0;
    for( spawn_tile_record &spawn_tile : spawn_tiles ) {
        const int concrete_living_assigned = static_cast<int>( std::count_if(
                members.begin(), members.end(), [&spawn_tile]( const member_record & member ) {
            return counts_toward_live_headcount( member.state ) &&
                   member.home_spawn_tile == spawn_tile.tile;
        } ) );
        if( loaded_schema_version < 10 ) {
            spawn_tile.assigned_living_total = std::max(
                    std::max( 0, spawn_tile.assigned_living_total ), concrete_living_assigned );
        } else if( spawn_tile.assigned_living_total < 0 ||
                   spawn_tile.assigned_living_total < concrete_living_assigned ) {
            jo.throw_error(
                "v10 spawn tile assigned_living_total is smaller than its concrete living roster" );
        }
        assigned_living_total += spawn_tile.assigned_living_total;
    }
    if( loaded_schema_version < 10 ) {
        living_total = std::max<long long>( living_total,
                                            std::min<long long>( assigned_living_total,
                                                    std::numeric_limits<int>::max() ) );
    } else if( assigned_living_total > living_total ) {
        jo.throw_error( "v10 spawn tile assignments exceed the site living_total" );
    }
    jo.read( "next_outing_generation", next_outing_generation );
    jo.read( "applied_return_generation", applied_return_generation );
    jo.read( "applied_report_generation", applied_report_generation );
    jo.read( "applied_cargo_generation", applied_cargo_generation );
    jo.read( "last_cargo_application_key", last_cargo_application_key );
    const bool any_resource_receipt_field =
        jo.has_member( "applied_resource_generation" ) ||
        jo.has_member( "last_resource_application_key" ) ||
        jo.has_member( "last_resource_claimed_units" );
    const bool complete_resource_receipt =
        jo.has_member( "applied_resource_generation" ) &&
        jo.has_member( "last_resource_application_key" ) &&
        jo.has_member( "last_resource_claimed_units" );
    if( loaded_schema_version < 9 && any_resource_receipt_field ) {
        jo.throw_error( "pre-v9 site cannot contain a resource application receipt" );
    }
    if( loaded_schema_version >= 9 && !complete_resource_receipt ) {
        jo.throw_error( "v9 site must contain a complete resource application receipt" );
    }
    if( complete_resource_receipt ) {
        jo.read( "applied_resource_generation", applied_resource_generation );
        jo.read( "last_resource_application_key", last_resource_application_key );
        jo.read( "last_resource_claimed_units", last_resource_claimed_units );
        const bool empty_receipt = applied_resource_generation == 0 &&
                                   last_resource_application_key.empty() &&
                                   last_resource_claimed_units == 0;
        const bool applied_receipt = applied_resource_generation > 0 &&
                                     applied_resource_generation <
                                     std::numeric_limits<int>::max() - 1 &&
                                     !last_resource_application_key.empty() &&
                                     last_resource_application_key.size() <=
                                     max_operation_application_key_length &&
                                     resource_receipt_key_matches( site_id,
                                             applied_resource_generation,
                                             last_resource_application_key ) &&
                                     next_outing_generation > applied_resource_generation &&
                                     last_resource_claimed_units > 0 &&
                                     last_resource_claimed_units <= max_finite_resource_claim_units;
        if( !empty_receipt && !applied_receipt ) {
            jo.throw_error( "site has an invalid resource application receipt" );
        }
    }
    bool scout_report_policy_was_present = false;
    if( jo.has_member( "current_scout_report" ) ) {
        JsonObject report_json = jo.get_object( "current_scout_report" );
        scout_report_policy_was_present = report_json.has_member( "action_policy" );
        current_scout_report.deserialize( report_json );
    }
    const bool camp_decision_was_present = jo.has_member( "camp_decision" );
    bool camp_decision_policy_was_present = false;
    if( camp_decision_was_present ) {
        JsonObject decision_json = jo.get_object( "camp_decision" );
        camp_decision_policy_was_present = decision_json.has_member( "report_policy" );
        camp_decision.deserialize( decision_json );
    }
    jo.read( "acted_reports", acted_reports );
    if( loaded_schema_version < 8 ) {
        hostile_site_profile legacy_profile = effective_profile( *this );
        const bool legacy_bandit_report =
            legacy_profile == hostile_site_profile::none &&
            ( ( !scout_report_policy_was_present && current_scout_report.is_present() ) ||
              ( !camp_decision_policy_was_present && camp_decision.has_pinned_report() ) );
        if( legacy_bandit_report ) {
            profile = hostile_site_profile::camp_style;
            legacy_profile = profile;
        }
        const camp_report_policy legacy_policy =
            report_policy_for_profile( legacy_profile );
        if( !scout_report_policy_was_present && current_scout_report.is_present() &&
            current_scout_report.action_policy == camp_report_policy::none ) {
            current_scout_report.action_policy = legacy_policy;
        }
        if( !camp_decision_policy_was_present && camp_decision.has_pinned_report() &&
            camp_decision.report_policy == camp_report_policy::none ) {
            camp_decision.report_policy = legacy_policy;
        }
    }
    if( camp_decision.has_pinned_report() &&
        camp_decision.report_policy != camp_report_policy::none ) {
        acted_report_summary acted;
        acted.target_id = camp_decision.target_id;
        acted.target_omt = camp_decision.target_omt;
        acted.policy = camp_decision.report_policy;
        acted.source_generation = camp_decision.source_report_generation;
        acted.report_revision = camp_decision.source_report_revision;
        acted.acted_minutes = camp_decision.last_transition_minutes;
        acted_reports.push_back( std::move( acted ) );
    }
    normalize_acted_reports( *this );
    jo.read( "returned_cargo_stock", returned_cargo_stock );
    const bool active_outing_was_present = jo.has_member( "active_outing" );
    bool active_outing_has_embedded_payload = false;
    int active_outing_loaded_schema_version = 0;
    std::string legacy_active_group_id;
    if( active_outing_was_present ) {
        JsonObject active_outing_json = jo.get_object( "active_outing" );
        active_outing_has_embedded_payload = active_outing_json.has_member( "member_ids" );
        active_outing_json.read( "schema_version", active_outing_loaded_schema_version );
        if( loaded_schema_version >= 10 &&
            !current_serialized_owner_fields_are_consistent( active_outing_json ) ) {
            active_outing_json.allow_omitted_members();
            active_outing_json.throw_error( "v10 site contains a non-canonical active outing owner" );
        }
        active_outing.deserialize( active_outing_json );
    } else {
        jo.read( "active_group_id", legacy_active_group_id );
    }
    const bool active_hostile_operation_was_present =
        jo.has_member( "active_hostile_operation" );
    if( active_hostile_operation_was_present ) {
        JsonObject hostile_operation_json = jo.get_object( "active_hostile_operation" );
        if( loaded_schema_version >= 10 ) {
            int hostile_operation_schema_version = 0;
            if( !hostile_operation_json.has_member( "schema_version" ) ) {
                hostile_operation_json.allow_omitted_members();
                hostile_operation_json.throw_error(
                    "v10 site contains a hostile operation without a schema version" );
            }
            hostile_operation_json.read( "schema_version", hostile_operation_schema_version );
            if( hostile_operation_schema_version != 1 ) {
                hostile_operation_json.allow_omitted_members();
                hostile_operation_json.throw_error(
                    "v10 site contains an unsupported hostile operation schema version" );
            }
            if( !hostile_operation_json.has_member( "reservation" ) ) {
                hostile_operation_json.allow_omitted_members();
                hostile_operation_json.throw_error(
                    "v10 site contains a hostile operation without a reservation owner" );
            }
            JsonObject reservation_json = hostile_operation_json.get_object( "reservation" );
            reservation_json.allow_omitted_members();
            std::string reservation_kind;
            std::string reservation_activity_id;
            int reservation_generation = 0;
            reservation_json.read( "kind", reservation_kind );
            reservation_json.read( "activity_id", reservation_activity_id );
            reservation_json.read( "generation", reservation_generation );
            const bool reservation_has_identity = !reservation_activity_id.empty() ||
                                                  reservation_generation > 0;
            const bool reservation_kind_is_canonical = reservation_has_identity ?
                    reservation_kind == "hostile_operation" : reservation_kind == "none";
            if( !current_serialized_owner_fields_are_consistent( reservation_json ) ||
                !reservation_kind_is_canonical ) {
                hostile_operation_json.allow_omitted_members();
                hostile_operation_json.throw_error(
                    "v10 site contains a non-canonical hostile operation owner" );
            }
        }
        active_hostile_operation.deserialize( hostile_operation_json );
    }
    std::string legacy_active_target_id;
    tripoint_abs_omt legacy_active_target_omt;
    std::string legacy_active_job_type;
    int legacy_active_sortie_started_minutes = -1;
    int legacy_active_sortie_local_contact_minutes = -1;
    jo.read( "active_target_id", legacy_active_target_id );
    jo.read( "active_target_omt", legacy_active_target_omt );
    jo.read( "active_job_type", legacy_active_job_type );
    jo.read( "active_sortie_started_minutes", legacy_active_sortie_started_minutes );
    jo.read( "active_sortie_local_contact_minutes", legacy_active_sortie_local_contact_minutes );
    std::vector<int> raw_active_member_ids;
    jo.read( "active_member_ids", raw_active_member_ids );
    std::vector<character_id> legacy_active_member_ids;
    legacy_active_member_ids.reserve( raw_active_member_ids.size() );
    for( const int raw_member_id : raw_active_member_ids ) {
        character_id member_id;
        member_id.deserialize( raw_member_id );
        legacy_active_member_ids.push_back( member_id );
    }
    const bool import_transitional_active_payload = !active_outing_was_present ||
            ( active_outing_loaded_schema_version < 4 &&
              !active_outing_has_embedded_payload );
    const bool malformed_current_active_payload = active_outing_was_present &&
            active_outing_loaded_schema_version >= 4 && !active_outing_has_embedded_payload;
    if( import_transitional_active_payload ) {
        active_outing.member_ids = legacy_active_member_ids;
        active_outing.leader_id = active_outing.member_ids.empty() ? character_id() :
                                  active_outing.member_ids.front();
        active_outing.target_id = legacy_active_target_id;
        active_outing.target_omt = legacy_active_target_omt;
        active_outing.job_type = legacy_active_job_type;
        active_outing.started_minutes = legacy_active_sortie_started_minutes;
        active_outing.local_contact_minutes = legacy_active_sortie_local_contact_minutes;
        active_outing.last_progress_minutes = std::max( legacy_active_sortie_started_minutes,
                                              legacy_active_sortie_local_contact_minutes );
        active_outing.last_advanced_minutes = std::max( active_outing.last_advanced_minutes,
                                               active_outing.last_progress_minutes );
        if( legacy_active_sortie_local_contact_minutes >= 0 ) {
            active_outing.owner = simulation_owner::local;
        }
        active_outing.phase = legacy_active_sortie_local_contact_minutes >= 0 ?
                              scout_phase::observing : scout_phase::outbound;
        const int return_clock_anchor = active_outing.local_contact_minutes >= 0 ?
                                        active_outing.local_contact_minutes : active_outing.started_minutes;
        if( return_clock_anchor >= 0 ) {
            active_outing.expected_return_minutes = minutes_after_saturated(
                    return_clock_anchor,
                    ordinary_scout_sortie_limit_minutes() + scout_return_cohesion_minutes );
            active_outing.missing_deadline_minutes = minutes_after_saturated(
                    active_outing.expected_return_minutes, scout_missing_grace_minutes );
        }
    } else if( malformed_current_active_payload && loaded_schema_version >= 10 ) {
        jo.throw_error( "v10 site contains an incomplete active outing payload" );
    } else if( malformed_current_active_payload ) {
        for( member_record &member : members ) {
            const bool reserved_for_hostile_operation =
                active_hostile_operation.reservation.is_active() &&
                std::find( active_hostile_operation.reservation.member_ids.begin(),
                           active_hostile_operation.reservation.member_ids.end(),
                           member.npc_id ) !=
                active_hostile_operation.reservation.member_ids.end();
            if( !reserved_for_hostile_operation &&
                ( member.state == member_state::outbound ||
                  member.state == member_state::local_contact ) ) {
                member.state = member_state::at_home;
                member.last_writeback_summary =
                    "closed incomplete current-schema active outing";
            }
        }
        active_outing.clear();
    }
    jo.read( "remembered_target_or_mark", remembered_target_or_mark );
    jo.read( "remembered_threat_estimate", remembered_threat_estimate );
    jo.read( "remembered_bounty_estimate", remembered_bounty_estimate );
    jo.read( "remembered_retreat_bias", remembered_retreat_bias );
    jo.read( "remembered_return_clock", remembered_return_clock );
    std::string remembered_pressure_string = "ample";
    jo.read( "remembered_pressure", remembered_pressure_string );
    remembered_pressure = remaining_return_pressure_state_from_string( remembered_pressure_string ).value_or(
                              bandit_pursuit_handoff::remaining_return_pressure_state::ample );
    jo.read( "known_recent_marks", known_recent_marks );
    const bool intelligence_map_was_present = jo.has_member( "intelligence_map" );
    if( intelligence_map_was_present ) {
        jo.read( "intelligence_map", intelligence_map );
    }
    migrate_scalar_memory_to_intelligence_map( *this, intelligence_map_was_present );
    jo.read( "last_shakedown_outcome", last_shakedown_outcome );
    jo.read( "shakedown_last_demanded_value", shakedown_last_demanded_value );
    jo.read( "shakedown_last_surrendered_value", shakedown_last_surrendered_value );
    jo.read( "shakedown_last_reachable_value", shakedown_last_reachable_value );
    jo.read( "shakedown_loot_value", shakedown_loot_value );
    jo.read( "shakedown_defender_losses", shakedown_defender_losses );
    jo.read( "shakedown_bandit_losses", shakedown_bandit_losses );
    jo.read( "shakedown_anger", shakedown_anger );
    jo.read( "shakedown_caution", shakedown_caution );
    jo.read( "shakedown_basecamp_defenders_at_fight", shakedown_basecamp_defenders_at_fight );
    jo.read( "shakedown_basecamp_defender_observation_pending",
             shakedown_basecamp_defender_observation_pending );
    jo.read( "shakedown_reopen_available", shakedown_reopen_available );
    jo.read( "shakedown_reopen_used", shakedown_reopen_used );
    const bool any_origin_field = jo.has_member( "origin_disposition" ) ||
                                  jo.has_member( "origin_changed_minutes" ) ||
                                  jo.has_member( "origin_summary" );
    const bool complete_origin_payload = jo.has_member( "origin_disposition" ) &&
                                         jo.has_member( "origin_changed_minutes" ) &&
                                         jo.has_member( "origin_summary" );
    if( loaded_schema_version < 11 && any_origin_field ) {
        jo.throw_error( "pre-v11 site cannot contain an origin disposition payload" );
    }
    if( loaded_schema_version >= 11 && !complete_origin_payload ) {
        jo.throw_error( "v11 site must contain a complete origin disposition payload" );
    }
    if( complete_origin_payload ) {
        std::string origin_string;
        jo.read( "origin_disposition", origin_string );
        const std::optional<origin_disposition> parsed_origin =
            origin_disposition_from_string( origin_string );
        if( !parsed_origin ) {
            jo.throw_error( "site has an invalid origin disposition" );
        }
        origin = *parsed_origin;
        jo.read( "origin_changed_minutes", origin_changed_minutes );
        jo.read( "origin_summary", origin_summary );
        if( origin_summary.size() > max_sortie_summary_length ) {
            jo.throw_error( "site origin disposition summary exceeds its persisted bound" );
        }
    }
    jo.read( "retired_empty_site", retired_empty_site );
    jo.read( "retirement_summary", retirement_summary );
    if( origin == origin_disposition::active_hostile ) {
        if( origin_changed_minutes != -1 || !origin_summary.empty() ) {
            jo.throw_error( "active hostile origin has terminal disposition metadata" );
        }
    } else if( origin_changed_minutes < 0 || origin_summary.empty() ||
               !retired_empty_site ) {
        jo.throw_error( "terminal origin disposition is incomplete or not retired" );
    }

    if( !active_outing_was_present && !legacy_active_group_id.empty() ) {
        active_outing.schema_version = 1;
        active_outing.activity_id = legacy_active_group_id;
        active_outing.camp_id = site_id;
        active_outing.generation = std::max( 1, applied_return_generation + 1 );
        active_outing.kind = classify_legacy_outing_kind( legacy_active_group_id,
                             active_outing.job_type );
        active_outing.owner = legacy_active_sortie_local_contact_minutes >= 0 ?
                              simulation_owner::local : simulation_owner::abstract;
        active_outing.last_advanced_minutes = std::max( active_outing.started_minutes,
                                               active_outing.local_contact_minutes );
    }
    if( active_outing.is_active() ) {
        if( loaded_schema_version < 10 ) {
            normalize_legacy_simulation_owner_state( active_outing );
            if( active_outing.camp_id.empty() ) {
                active_outing.camp_id = site_id;
            }
            if( active_outing.schema_version < 5 ) {
                active_outing.return_application_key =
                    bandit_pursuit_handoff::make_operation_component_key(
                        active_outing.activity_id, active_outing.generation, "return" );
                active_outing.report_application_key =
                    bandit_pursuit_handoff::make_operation_component_key(
                        active_outing.activity_id, active_outing.generation, "report" );
                active_outing.cargo_application_key =
                    bandit_pursuit_handoff::make_operation_component_key(
                        active_outing.activity_id, active_outing.generation, "cargo" );
                active_outing.schema_version = 5;
            }
        }
        next_outing_generation = std::max( next_outing_generation, active_outing.generation + 1 );
    }
    const bool new_hostile_payload_present = active_hostile_operation.is_active() ||
            !active_hostile_operation.reservation.member_ids.empty();
    if( loaded_schema_version < 10 && active_outing.is_active() &&
        active_outing.kind == outing_kind::hostile_operation &&
        !new_hostile_payload_present ) {
        active_hostile_operation.clear();
        active_hostile_operation.operation_kind = hostile_operation_kind_for_job(
                    active_outing.job_type );
        active_hostile_operation.reservation = std::move( active_outing );
        active_hostile_operation.reservation.kind = outing_kind::hostile_operation;
        switch( active_hostile_operation.reservation.phase ) {
            case scout_phase::observing:
                active_hostile_operation.phase = hostile_operation_phase::committed_contact;
                break;
            case scout_phase::returning_home:
                active_hostile_operation.phase = hostile_operation_phase::returning_home;
                break;
            case scout_phase::lost:
                active_hostile_operation.phase = hostile_operation_phase::lost;
                break;
            default:
                active_hostile_operation.phase = hostile_operation_phase::outbound;
                break;
        }
        const bool exact_report_pin = report_matches_camp_decision( current_scout_report,
                                      camp_decision ) &&
                                      active_hostile_operation.reservation.target_id ==
                                      current_scout_report.target_id &&
                                      active_hostile_operation.reservation.target_omt ==
                                      current_scout_report.target_omt &&
                                      active_hostile_operation.reservation.target_lead_revision ==
                                      current_scout_report.target_lead_revision;
        if( exact_report_pin ) {
            active_hostile_operation.source_report_revision = current_scout_report.revision;
            active_hostile_operation.source_report_generation = current_scout_report.source_generation;
            active_hostile_operation.source_report_activity_id = current_scout_report.source_activity_id;
            active_hostile_operation.source_report_application_key = current_scout_report.application_key;
        }
        const active_outing_state &reservation = active_hostile_operation.reservation;
        if( !reservation.shared_route.empty() ) {
            const int rally_index = std::clamp( reservation.waypoint_index, 0,
                                                static_cast<int>( reservation.shared_route.size() ) - 1 );
            active_hostile_operation.has_rally = true;
            active_hostile_operation.rally_omt = reservation.shared_route[rally_index];
        }
        active_hostile_operation.legacy_unpinned = true;
        if( active_hostile_operation.legacy_unpinned &&
            active_hostile_operation.phase != hostile_operation_phase::lost ) {
            active_hostile_operation.phase = hostile_operation_phase::returning_home;
            active_hostile_operation.reservation.phase = scout_phase::returning_home;
            active_hostile_operation.last_transition_reason =
                "legacy hostile operation retained for safe withdrawal";
        } else {
            active_hostile_operation.last_transition_reason =
                "migrated legacy hostile outing owner";
        }
        active_outing.clear();
    }
    if( active_hostile_operation.reservation.is_active() ) {
        if( loaded_schema_version < 10 ) {
            active_hostile_operation.schema_version = 1;
            normalize_legacy_simulation_owner_state(
                active_hostile_operation.reservation );
            active_hostile_operation.reservation.kind = outing_kind::hostile_operation;
            if( active_hostile_operation.reservation.camp_id.empty() ) {
                active_hostile_operation.reservation.camp_id = site_id;
            }
            if( active_hostile_operation.reservation.schema_version < 5 ) {
                active_hostile_operation.reservation.return_application_key =
                    bandit_pursuit_handoff::make_operation_component_key(
                        active_hostile_operation.reservation.activity_id,
                        active_hostile_operation.reservation.generation, "return" );
                active_hostile_operation.reservation.report_application_key =
                    bandit_pursuit_handoff::make_operation_component_key(
                        active_hostile_operation.reservation.activity_id,
                        active_hostile_operation.reservation.generation, "report" );
                active_hostile_operation.reservation.cargo_application_key =
                    bandit_pursuit_handoff::make_operation_component_key(
                        active_hostile_operation.reservation.activity_id,
                        active_hostile_operation.reservation.generation, "cargo" );
                active_hostile_operation.reservation.schema_version = 5;
            }
        }
        next_outing_generation = std::max( next_outing_generation,
                                           active_hostile_operation.reservation.generation + 1 );
    }
    applied_return_generation = std::max( 0, applied_return_generation );
    if( !current_scout_report.provisional ) {
        applied_report_generation = std::max( applied_report_generation,
                                              current_scout_report.source_generation );
    }
    applied_report_generation = std::max( 0, applied_report_generation );
    applied_cargo_generation = std::max( 0, applied_cargo_generation );
    applied_resource_generation = std::max( 0, applied_resource_generation );
    next_outing_generation = std::max( next_outing_generation, applied_return_generation + 1 );
    next_outing_generation = std::max( next_outing_generation, applied_report_generation + 1 );
    next_outing_generation = std::max( next_outing_generation, applied_cargo_generation + 1 );
    next_outing_generation = std::max( next_outing_generation, applied_resource_generation + 1 );

    if( loaded_schema_version >= 10 && !roster().valid ) {
        jo.throw_error( "v10+ site has malformed roster authority or external reservation ownership" );
    }

    const bool both_external_owners_present =
        ( active_outing.is_active() || !active_outing.member_ids.empty() ) &&
        ( active_hostile_operation.is_active() ||
          !active_hostile_operation.reservation.member_ids.empty() );
    if( both_external_owners_present && loaded_schema_version >= 10 ) {
        jo.throw_error( "v10+ site contains multiple external reservation owners" );
    } else if( both_external_owners_present ) {
        active_outing_state &duplicate = active_outing.kind == outing_kind::hostile_operation ?
                                         active_outing : active_hostile_operation.reservation;
        const active_outing_state &retained = active_outing.kind == outing_kind::hostile_operation ?
                                              active_hostile_operation.reservation : active_outing;
        for( const character_id &member_id : duplicate.member_ids ) {
            if( std::find( retained.member_ids.begin(), retained.member_ids.end(), member_id ) !=
                retained.member_ids.end() ) {
                continue;
            }
            member_record *member = find_member( member_id );
            if( member != nullptr && ( member->state == member_state::outbound ||
                                      member->state == member_state::local_contact ) ) {
                member->state = member_state::at_home;
                member->last_writeback_summary = "closed duplicate persisted external owner";
            }
        }
        if( active_outing.kind == outing_kind::hostile_operation ) {
            active_outing.clear();
        } else {
            active_hostile_operation.clear();
        }
    }

    if( active_outing.kind == outing_kind::structural_sortie &&
        active_outing.schema_version == 5 ) {
        const bool route_less_structural_shape = active_outing.shared_route.empty() &&
                active_outing.waypoint_index == 0 &&
                active_outing.expected_return_minutes == -1 &&
                active_outing.missing_deadline_minutes == -1;
        const std::vector<tripoint_abs_omt> migrated_route =
            make_structural_radial_route( anchor, active_outing.target_omt );
        const bool migratable_phase = active_outing.phase == scout_phase::outbound ||
                                      active_outing.phase == scout_phase::observing ||
                                      active_outing.phase == scout_phase::returning_home;
        if( route_less_structural_shape && !migrated_route.empty() &&
            active_outing.started_minutes >= 0 && migratable_phase ) {
            active_outing.shared_route = migrated_route;
            active_outing.waypoint_index = active_outing.phase == scout_phase::outbound ? 0 : 1;
            active_outing.expected_return_minutes = structural_expected_return_minutes(
                    active_outing.started_minutes, anchor, active_outing.target_omt );
            active_outing.missing_deadline_minutes = minutes_after_saturated(
                    active_outing.expected_return_minutes, scout_missing_grace_minutes );
            active_outing.schema_version = 6;
        }
    }

    const bool scout_job_is_consistent = active_outing.kind != outing_kind::scout_sortie ||
                                         active_outing.job_type == "scout" ||
                                         active_outing.job_type == "scavenge";
    const camp_map_lead *structural_lead = active_outing.kind == outing_kind::structural_sortie ?
                                           intelligence_map.find_lead(
                                               active_outing.target_lead_id ) : nullptr;
    const std::optional<int> structural_frontier_sector = structural_lead != nullptr ?
            frontier_sector_from_lead( *structural_lead ) : std::nullopt;
    const int structural_target_waypoint = structural_outing_destination_waypoint(
            active_outing );
    const bool structural_phase_is_consistent =
        active_outing.kind != outing_kind::structural_sortie ||
        ( ( active_outing.phase == scout_phase::outbound &&
            active_outing.local_contact_minutes == -1 &&
            ( active_outing.waypoint_index == 0 ||
              ( active_outing.schema_version >= 8 && active_outing.waypoint_index == 1 ) ) ) ||
          ( active_outing.phase == scout_phase::observing &&
            active_outing.local_contact_minutes >= active_outing.started_minutes &&
            ( active_outing.waypoint_index == 1 ||
              ( active_outing.schema_version >= 8 &&
                active_outing.waypoint_index == structural_target_waypoint ) ) ) ||
          ( active_outing.schema_version >= 10 &&
            ( active_outing.phase == scout_phase::burned_withdrawal ||
              active_outing.phase == scout_phase::returning_exposed ||
              active_outing.phase == scout_phase::returning_report ) &&
            active_outing.local_contact_minutes >= active_outing.started_minutes &&
            ( active_outing.waypoint_index == structural_target_waypoint ||
              active_outing.waypoint_index == structural_target_waypoint + 1 ) ) ||
          ( active_outing.phase == scout_phase::returning_home &&
            ( active_outing.waypoint_index == 0 || active_outing.waypoint_index == 1 ||
              active_outing.waypoint_index == structural_target_waypoint ||
              ( structural_outing_uses_watch_route( active_outing ) &&
                active_outing.waypoint_index == structural_target_waypoint + 1 ) ) ) ||
          ( active_outing.schema_version >= 8 && active_outing.phase == scout_phase::lost &&
            active_outing.casualty_ids.size() == active_outing.member_ids.size() &&
            active_outing.resolved_member_ids.size() == active_outing.member_ids.size() ) );
    const bool structural_identity_is_consistent =
        active_outing.kind != outing_kind::structural_sortie ||
        ( active_outing.activity_id == site_id + "#structural" &&
          ( active_outing.job_type == "scout" || active_outing.job_type == "scavenge" ) &&
          active_outing.member_ids.size() == 2 &&
          ( active_outing.schema_version == 6 || active_outing.schema_version == 7 ||
            active_outing.schema_version == 8 || active_outing.schema_version == 9 ||
            active_outing.schema_version == 10 ) &&
          active_outing.started_minutes >= 0 &&
          active_outing.target_id == active_outing.target_lead_id &&
          active_outing.target_lead_revision > 0 && structural_lead != nullptr &&
          ( ( structural_lead->kind == camp_lead_kind::structural_bounty &&
              active_outing.job_type == ( structural_lead->target_id == "forest" ?
                                          "scavenge" : "scout" ) ) ||
            ( structural_lead->kind == camp_lead_kind::terrain_opportunity &&
              active_outing.job_type == "scout" ) ||
            ( returned_structural_signal_lead( *structural_lead ) &&
              active_outing.job_type == "scout" ) ||
            ( structural_frontier_sector && active_outing.job_type == "scout" ) ) &&
          structural_lead->omt == active_outing.target_omt &&
          structural_route_is_canonical_for_outing( active_outing, anchor,
                  *structural_lead ) &&
          active_outing.expected_return_minutes == structural_expected_return_minutes(
              active_outing.started_minutes, anchor,
              structural_outing_travel_destination( active_outing ) ) &&
          active_outing.missing_deadline_minutes == minutes_after_saturated(
              active_outing.expected_return_minutes, scout_missing_grace_minutes ) &&
          structural_phase_is_consistent );
    const bool active_outing_schema_is_consistent =
        active_outing.kind == outing_kind::structural_sortie ?
        ( active_outing.schema_version == 6 || active_outing.schema_version == 7 ||
          active_outing.schema_version == 8 || active_outing.schema_version == 9 ||
          active_outing.schema_version == 10 ) :
        active_outing.schema_version == 5;
    bool active_outing_is_consistent = active_outing.is_active() &&
                                       active_outing.kind != outing_kind::hostile_operation &&
                                       scout_job_is_consistent &&
                                       structural_identity_is_consistent &&
                                       active_outing.camp_id == site_id &&
                                       active_outing_schema_is_consistent &&
                                       active_outing.return_application_key ==
                                       bandit_pursuit_handoff::make_operation_component_key(
                                           active_outing.activity_id, active_outing.generation,
                                           "return" ) &&
                                       active_outing.report_application_key ==
                                       bandit_pursuit_handoff::make_operation_component_key(
                                           active_outing.activity_id, active_outing.generation,
                                           "report" ) &&
                                       active_outing.cargo_application_key ==
                                       bandit_pursuit_handoff::make_operation_component_key(
                                           active_outing.activity_id, active_outing.generation,
                                           "cargo" ) &&
                                       active_outing.generation > applied_return_generation &&
                                       active_outing.generation > applied_cargo_generation &&
                                       active_outing.generation > applied_report_generation &&
                                       simulation_owner_state_is_consistent( active_outing ) &&
                                       !active_outing.member_ids.empty() &&
                                       active_outing.member_ids.size() <= max_active_outing_members;
    std::vector<character_id> checked_active_member_ids;
    checked_active_member_ids.reserve( active_outing.member_ids.size() );
    for( const character_id &member_id : active_outing.member_ids ) {
        const member_record *member = find_member( member_id );
        const bool recorded_casualty = std::find( active_outing.casualty_ids.begin(),
                                       active_outing.casualty_ids.end(), member_id ) !=
                                       active_outing.casualty_ids.end();
        const bool resolved_member = active_outing.member_is_resolved( member_id );
        const bool member_state_matches_reservation = member != nullptr &&
                ( recorded_casualty ?
                  ( resolved_member && ( member->state == member_state::dead ||
                                         member->state == member_state::missing ) ) :
                  ( resolved_member ? member->state == member_state::at_home :
                    ( member->state == member_state::outbound ||
                      member->state == member_state::local_contact ) ) );
        if( !member_state_matches_reservation ||
            std::find( checked_active_member_ids.begin(), checked_active_member_ids.end(), member_id ) !=
            checked_active_member_ids.end() ) {
            active_outing_is_consistent = false;
            break;
        }
        checked_active_member_ids.push_back( member_id );
    }
    const bool active_outing_payload_present = active_outing.is_active() ||
            !active_outing.member_ids.empty();
    if( !active_outing_is_consistent && active_outing_payload_present &&
        loaded_schema_version >= 10 ) {
        jo.throw_error( "v10+ site contains an inconsistent active outing owner" );
    }
    if( !active_outing_is_consistent && active_outing_payload_present ) {
        for( const character_id &member_id : active_outing.member_ids ) {
            member_record *member = find_member( member_id );
            if( member != nullptr && ( member->state == member_state::outbound ||
                                      member->state == member_state::local_contact ) ) {
                member->state = member_state::at_home;
                member->last_writeback_summary = "closed inconsistent persisted active outing";
            }
        }
        active_outing.clear();
    }
    active_outing_state &hostile_reservation = active_hostile_operation.reservation;
    const bool legacy_withdrawal_only = active_hostile_operation.legacy_unpinned &&
                                        ( active_hostile_operation.phase ==
                                          hostile_operation_phase::returning_home ||
                                          active_hostile_operation.phase == hostile_operation_phase::lost );
    const bool rally_is_on_route = active_hostile_operation.has_rally &&
                                   std::find( hostile_reservation.shared_route.begin(),
                                           hostile_reservation.shared_route.end(),
                                           active_hostile_operation.rally_omt ) !=
                                   hostile_reservation.shared_route.end();
    const bool hostile_job_is_consistent = hostile_operation_job_matches(
                active_hostile_operation.operation_kind, hostile_reservation.job_type ) ||
            legacy_withdrawal_only;
    const bool hostile_report_is_consistent = legacy_withdrawal_only ||
            ( camp_decision.state == camp_decision_state::preparing_follow_on &&
              camp_decision.report_policy == report_policy_for_profile(
                  effective_profile( *this ) ) &&
              active_hostile_operation.operation_kind ==
              operation_kind_for_report_policy( camp_decision.report_policy ) &&
              report_matches_camp_decision( current_scout_report, camp_decision ) &&
              report_matches_hostile_operation( current_scout_report,
                                                active_hostile_operation ) );
    const bool hostile_route_is_consistent = legacy_withdrawal_only ||
            ( hostile_reservation.shared_route.size() >= 2 &&
              hostile_reservation.shared_route.front() == anchor &&
              hostile_reservation.shared_route.back() == hostile_reservation.target_omt &&
              rally_is_on_route );
    const std::string expected_hostile_activity_id = site_id + "#hostile:" +
            std::to_string( hostile_reservation.generation );
    const bool hostile_identity_is_consistent = legacy_withdrawal_only ||
            ( hostile_reservation.activity_id == expected_hostile_activity_id &&
              hostile_reservation.generation >
              active_hostile_operation.source_report_generation &&
              hostile_reservation.return_application_key ==
              bandit_pursuit_handoff::make_operation_component_key(
                  expected_hostile_activity_id, hostile_reservation.generation, "return" ) &&
              hostile_reservation.report_application_key ==
              bandit_pursuit_handoff::make_operation_component_key(
                  expected_hostile_activity_id, hostile_reservation.generation, "report" ) &&
              hostile_reservation.cargo_application_key ==
              bandit_pursuit_handoff::make_operation_component_key(
                  expected_hostile_activity_id, hostile_reservation.generation, "cargo" ) );
    const bool hostile_reserve_is_consistent = legacy_withdrawal_only ||
            active_hostile_operation.phase != hostile_operation_phase::assembling ||
            hostile_operation_party_preserves_home( *this,
                    hostile_reservation.member_ids.size() );
    bool hostile_operation_is_consistent = active_hostile_operation.is_active() &&
            hostile_reservation.camp_id == site_id &&
            hostile_reservation.generation > applied_return_generation &&
            hostile_reservation.generation > applied_cargo_generation &&
            !hostile_reservation.member_ids.empty() &&
            ( legacy_withdrawal_only ?
              hostile_reservation.member_ids.size() <= max_active_outing_members :
              hostile_reservation.member_ids.size() >= 2 &&
              hostile_reservation.member_ids.size() <= max_hostile_operation_members ) &&
            hostile_reservation.shared_route.size() <= max_active_outing_route_steps &&
            hostile_route_is_consistent && hostile_job_is_consistent &&
            hostile_report_is_consistent && hostile_identity_is_consistent &&
            hostile_reserve_is_consistent &&
            simulation_owner_state_is_consistent( hostile_reservation ) &&
            hostile_operation_phase_matches_reservation( active_hostile_operation );
    std::vector<character_id> checked_hostile_member_ids;
    checked_hostile_member_ids.reserve( hostile_reservation.member_ids.size() );
    for( const character_id &member_id : hostile_reservation.member_ids ) {
        const member_record *member = find_member( member_id );
        const bool recorded_casualty = std::find( hostile_reservation.casualty_ids.begin(),
                                       hostile_reservation.casualty_ids.end(), member_id ) !=
                                       hostile_reservation.casualty_ids.end();
        const bool resolved_member = hostile_reservation.member_is_resolved( member_id );
        bool member_state_matches_reservation = member != nullptr;
        if( member_state_matches_reservation && recorded_casualty ) {
            member_state_matches_reservation = resolved_member &&
                                               ( member->state == member_state::dead ||
                                                 member->state == member_state::missing );
        } else if( member_state_matches_reservation && resolved_member ) {
            member_state_matches_reservation = member->state == member_state::at_home;
        } else if( member_state_matches_reservation &&
                   active_hostile_operation.phase == hostile_operation_phase::assembling ) {
            member_state_matches_reservation = member->state == member_state::at_home;
        } else if( member_state_matches_reservation &&
                   active_hostile_operation.phase == hostile_operation_phase::committed_contact ) {
            member_state_matches_reservation = member->state == member_state::local_contact ||
                                               member->state == member_state::outbound;
        } else if( member_state_matches_reservation ) {
            member_state_matches_reservation = member->state == member_state::outbound ||
                                               member->state == member_state::local_contact;
        }
        if( !member_state_matches_reservation ||
            std::find( checked_hostile_member_ids.begin(), checked_hostile_member_ids.end(),
                       member_id ) != checked_hostile_member_ids.end() ) {
            hostile_operation_is_consistent = false;
            break;
        }
        checked_hostile_member_ids.push_back( member_id );
    }
    const bool hostile_operation_payload_present = active_hostile_operation.is_active() ||
            !hostile_reservation.member_ids.empty();
    if( !hostile_operation_is_consistent && hostile_operation_payload_present &&
        loaded_schema_version >= 10 ) {
        jo.throw_error( "v10+ site contains an inconsistent hostile operation owner" );
    }
    if( !hostile_operation_is_consistent && hostile_operation_payload_present ) {
        for( const character_id &member_id : hostile_reservation.member_ids ) {
            member_record *member = find_member( member_id );
            if( member != nullptr && ( member->state == member_state::outbound ||
                                      member->state == member_state::local_contact ) ) {
                member->state = member_state::at_home;
                member->last_writeback_summary =
                    "closed inconsistent persisted hostile operation";
            }
        }
        active_hostile_operation.clear();
    }
    if( current_scout_report.provisional ) {
        const int returned_member_count = static_cast<int>( std::count_if(
                    active_outing.resolved_member_ids.begin(),
                    active_outing.resolved_member_ids.end(), [this]( const character_id & member_id ) {
            const member_record *member = find_member( member_id );
            return member != nullptr && member->state == member_state::at_home;
        } ) );
        const bool provisional_report_is_consistent =
            active_outing.is_active() && active_outing.kind == outing_kind::scout_sortie &&
            current_scout_report.source_activity_id == active_outing.activity_id &&
            current_scout_report.source_generation == active_outing.generation &&
            current_scout_report.source_job_type == active_outing.job_type &&
            current_scout_report.action_policy == report_policy_for_profile(
                effective_profile( *this ) ) &&
            current_scout_report.target_id == active_outing.target_id &&
            current_scout_report.target_omt == active_outing.target_omt &&
            ( current_scout_report.target_lead_id.empty() ||
              active_outing.target_lead_id.empty() ||
              current_scout_report.target_lead_id == active_outing.target_lead_id ) &&
            current_scout_report.target_lead_revision == active_outing.target_lead_revision &&
            current_scout_report.application_key == provisional_report_application_key( *this ) &&
            returned_member_count > 0;
        if( !provisional_report_is_consistent ) {
            current_scout_report.clear();
        }
    }
    if( !camp_decision_was_present && !active_outing.is_active() &&
        current_scout_report.is_present() && !current_scout_report.provisional ) {
        accept_current_scout_report_for_assessment( *this );
    }
    if( camp_decision.state == camp_decision_state::abandoned &&
        !camp_decision.has_pinned_report() && current_scout_report.is_present() &&
        !current_scout_report.provisional ) {
        camp_decision.source_report_revision = current_scout_report.revision;
        camp_decision.report_policy = current_scout_report.action_policy;
        camp_decision.source_report_generation = current_scout_report.source_generation;
        camp_decision.source_report_activity_id = current_scout_report.source_activity_id;
        camp_decision.source_report_application_key = current_scout_report.application_key;
        camp_decision.target_id = current_scout_report.target_id;
        camp_decision.target_omt = current_scout_report.target_omt;
        camp_decision.target_lead_id = current_scout_report.target_lead_id;
        camp_decision.target_lead_revision = current_scout_report.target_lead_revision;
        camp_decision.last_transition_minutes = std::max(
                    camp_decision.last_transition_minutes,
                    current_scout_report.delivered_minutes );
    }
    const bool decision_requires_current_report =
        camp_decision.state == camp_decision_state::report_awaiting_assessment ||
        camp_decision.state == camp_decision_state::preparing_follow_on;
    const bool decision_matches_current_report = report_matches_camp_decision(
                current_scout_report, camp_decision );
    const bool malformed_cooldown = camp_decision.state == camp_decision_state::cooldown &&
                                    ( !camp_decision.has_pinned_report() ||
                                      camp_decision.next_eligible_minutes < 0 );
    if( ( decision_requires_current_report && !decision_matches_current_report ) ||
        malformed_cooldown ) {
        camp_decision.state = camp_decision_state::abandoned;
        camp_decision.next_eligible_minutes = -1;
        camp_decision.transition_reason = "repaired inconsistent persisted camp decision";
    }
    if( loaded_schema_version < 6 ) {
        seed_camp_supply( *this );
    } else if( !complete_supply_payload ) {
        supply_units = 0;
        supply_last_update_minutes = -1;
        supply_accounted_living_total = camp_supply_living_total( *this );
        supply_member_minute_remainder = 0;
    } else {
        supply_units = std::clamp( supply_units, 0, camp_supply_cap( *this ) );
        supply_last_update_minutes = std::max( -1, supply_last_update_minutes );
        supply_accounted_living_total = std::max( 0, supply_accounted_living_total );
        supply_member_minute_remainder = std::clamp( supply_member_minute_remainder, 0,
                                         minutes_per_member_day - 1 );
        if( supply_last_update_minutes < 0 ) {
            supply_accounted_living_total = camp_supply_living_total( *this );
            supply_member_minute_remainder = 0;
        }
    }
    normalize_camp_intelligence( *this );
    normalize_acted_reports( *this );
    if( loaded_schema_version < 10 ) {
        std::vector<character_id> unresolved_reservations;
        const auto append_unresolved = [&unresolved_reservations]( const active_outing_state & outing ) {
            for( const character_id &member_id : outing.member_ids ) {
                if( !outing.member_is_resolved( member_id ) ) {
                    unresolved_reservations.push_back( member_id );
                }
            }
        };
        if( active_outing.is_active() ) {
            append_unresolved( active_outing );
        }
        if( active_hostile_operation.is_active() ) {
            append_unresolved( active_hostile_operation.reservation );
        }
        for( member_record &member : members ) {
            if( ( member.state == member_state::outbound ||
                  member.state == member_state::local_contact ) &&
                std::find( unresolved_reservations.begin(), unresolved_reservations.end(),
                           member.npc_id ) == unresolved_reservations.end() ) {
                member.state = member_state::at_home;
                member.last_writeback_summary = "returned orphaned legacy away member home";
            }
        }
    }
    if( !roster().valid ) {
        jo.throw_error( "site roster authority remains inconsistent after legacy repair" );
    }
    schema_version = 12;
}

bool site_record::has_member( character_id target_npc_id ) const
{
    return std::any_of( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
}

member_record *site_record::find_member( character_id target_npc_id )
{
    auto iter = std::find_if( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
    return iter != members.end() ? &*iter : nullptr;
}

const member_record *site_record::find_member( character_id target_npc_id ) const
{
    auto iter = std::find_if( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
    return iter != members.end() ? &*iter : nullptr;
}

spawn_tile_record *site_record::find_spawn_tile( const tripoint_abs_ms &tile )
{
    auto iter = std::find_if( spawn_tiles.begin(), spawn_tiles.end(), [&tile]( const spawn_tile_record & record ) {
        return record.tile == tile;
    } );
    return iter != spawn_tiles.end() ? &*iter : nullptr;
}

const spawn_tile_record *site_record::find_spawn_tile( const tripoint_abs_ms &tile ) const
{
    auto iter = std::find_if( spawn_tiles.begin(), spawn_tiles.end(), [&tile]( const spawn_tile_record & record ) {
        return record.tile == tile;
    } );
    return iter != spawn_tiles.end() ? &*iter : nullptr;
}

int site_record::count_members_in_state( member_state target_state ) const
{
    return static_cast<int>( std::count_if( members.begin(), members.end(),
    [target_state]( const member_record & member ) {
        return member.state == target_state;
    } ) );
}

int site_record::count_live_members() const
{
    return static_cast<int>( std::count_if( members.begin(), members.end(), []( const member_record & member ) {
        return counts_toward_live_headcount( member.state );
    } ) );
}

roster_view site_record::roster() const
{
    roster_view result;
    result.valid = living_total >= 0;
    result.living_total = std::max( 0, living_total );

    std::vector<character_id> concrete_ids;
    concrete_ids.reserve( members.size() );
    for( const member_record &member : members ) {
        if( !member.npc_id.is_valid() ||
            std::find( concrete_ids.begin(), concrete_ids.end(), member.npc_id ) !=
            concrete_ids.end() ) {
            result.valid = false;
            continue;
        }
        concrete_ids.push_back( member.npc_id );
        if( !counts_toward_live_headcount( member.state ) ) {
            continue;
        }
        result.materialized_living_total++;
        if( member.state == member_state::at_home ) {
            result.physically_present_ids.push_back( member.npc_id );
        } else if( member.state == member_state::orphaned ) {
            result.orphaned_ids.push_back( member.npc_id );
        } else {
            result.away_ids.push_back( member.npc_id );
        }
    }
    result.valid &= result.living_total >= result.materialized_living_total;
    result.unmaterialized_home_total = std::max( 0,
                                       result.living_total - result.materialized_living_total );

    const bool outing_present = active_outing.is_active() || !active_outing.member_ids.empty();
    const bool hostile_present = active_hostile_operation.is_active() ||
                                 !active_hostile_operation.reservation.member_ids.empty();
    result.valid &= outing_present != hostile_present || ( !outing_present && !hostile_present );

    const active_outing_state *reservation = nullptr;
    bool hostile_assembling = false;
    if( outing_present && !hostile_present ) {
        reservation = &active_outing;
        result.valid &= active_outing.is_active();
    } else if( hostile_present && !outing_present ) {
        reservation = &active_hostile_operation.reservation;
        result.valid &= active_hostile_operation.is_active();
        hostile_assembling = active_hostile_operation.phase ==
                             hostile_operation_phase::assembling;
    }

    if( reservation != nullptr ) {
        std::vector<character_id> reservation_ids;
        reservation_ids.reserve( reservation->member_ids.size() );
        for( const character_id &member_id : reservation->member_ids ) {
            if( !member_id.is_valid() ||
                std::find( reservation_ids.begin(), reservation_ids.end(), member_id ) !=
                reservation_ids.end() ) {
                result.valid = false;
                continue;
            }
            reservation_ids.push_back( member_id );
            if( reservation->member_is_resolved( member_id ) ) {
                continue;
            }
            const member_record *member = find_member( member_id );
            if( member == nullptr || !counts_toward_live_headcount( member->state ) ) {
                result.valid = false;
                continue;
            }
            result.reserved_unresolved_ids.push_back( member_id );
            if( hostile_assembling ) {
                result.valid &= member->state == member_state::at_home;
            } else {
                result.valid &= member->state == member_state::outbound ||
                                member->state == member_state::local_contact;
            }
        }
    }

    const auto sort_ids = []( std::vector<character_id> &ids ) {
        std::sort( ids.begin(), ids.end() );
    };
    sort_ids( result.physically_present_ids );
    sort_ids( result.away_ids );
    sort_ids( result.orphaned_ids );
    sort_ids( result.reserved_unresolved_ids );
    if( hostile_assembling ) {
        result.valid &= result.away_ids.empty();
    } else {
        result.valid &= result.away_ids == result.reserved_unresolved_ids;
    }

    for( const member_record &member : members ) {
        if( !member.npc_id.is_valid() || member.state != member_state::at_home ||
            member.wounded_or_unready ||
            std::binary_search( result.reserved_unresolved_ids.begin(),
                                result.reserved_unresolved_ids.end(), member.npc_id ) ||
            std::find( result.ready_concrete_ids.begin(), result.ready_concrete_ids.end(),
                       member.npc_id ) != result.ready_concrete_ids.end() ) {
            continue;
        }
        result.ready_concrete_ids.push_back( member.npc_id );
    }
    sort_ids( result.ready_concrete_ids );
    result.ready_concrete_total = static_cast<int>( result.ready_concrete_ids.size() );
    result.ready_total = result.unmaterialized_home_total + result.ready_concrete_total;
    result.physically_present_total = result.unmaterialized_home_total +
                                      static_cast<int>( result.physically_present_ids.size() );
    result.valid &= result.physically_present_total +
                    static_cast<int>( result.away_ids.size() ) +
                    static_cast<int>( result.orphaned_ids.size() ) == result.living_total;
    return result;
}

int camp_supply_living_total( const site_record &site )
{
    if( terminal_origin_disposition( site.origin ) ) {
        return 0;
    }
    return std::max( std::max( 0, site.living_total ), site.count_live_members() );
}

int camp_supply_cap( const site_record &site )
{
    if( terminal_origin_disposition( site.origin ) ) {
        return 0;
    }
    const long long capacity = static_cast<long long>( camp_supply_days_at_capacity ) *
                               std::max( 1, camp_supply_living_total( site ) );
    return static_cast<int>( std::min<long long>( max_camp_supply_units, capacity ) );
}

bool advance_camp_supply( site_record &site, const int now_minutes )
{
    if( now_minutes < 0 || ( site.supply_last_update_minutes >= 0 &&
                             now_minutes < site.supply_last_update_minutes ) ) {
        return false;
    }

    const int previous_units = site.supply_units;
    const int previous_update = site.supply_last_update_minutes;
    const int previous_accounted = site.supply_accounted_living_total;
    const int previous_remainder = site.supply_member_minute_remainder;
    const int current_living_total = camp_supply_living_total( site );
    site.supply_units = std::clamp( site.supply_units, 0, camp_supply_cap( site ) );
    site.supply_accounted_living_total = std::max( 0, site.supply_accounted_living_total );
    site.supply_member_minute_remainder = std::clamp( site.supply_member_minute_remainder, 0,
                                         minutes_per_member_day - 1 );

    if( site.supply_last_update_minutes < 0 ) {
        site.supply_last_update_minutes = now_minutes;
        site.supply_accounted_living_total = current_living_total;
        site.supply_member_minute_remainder = 0;
    } else {
        const long long elapsed_minutes = static_cast<long long>( now_minutes ) -
                                          site.supply_last_update_minutes;
        const long long member_minutes = site.supply_member_minute_remainder +
                                         elapsed_minutes * site.supply_accounted_living_total;
        const long long consumed_units = member_minutes / minutes_per_member_day;
        site.supply_units = static_cast<int>( std::max<long long>( 0,
                            static_cast<long long>( site.supply_units ) - consumed_units ) );
        site.supply_member_minute_remainder = static_cast<int>(
                    member_minutes % minutes_per_member_day );
        site.supply_last_update_minutes = now_minutes;
        site.supply_accounted_living_total = current_living_total;
        site.supply_units = std::min( site.supply_units, camp_supply_cap( site ) );
    }

    return site.supply_units != previous_units ||
           site.supply_last_update_minutes != previous_update ||
           site.supply_accounted_living_total != previous_accounted ||
           site.supply_member_minute_remainder != previous_remainder;
}

int advance_world_camp_supplies( world_state &state, const int now_minutes )
{
    int sites_changed = 0;
    for( site_record &site : state.sites ) {
        if( advance_camp_supply( site, now_minutes ) ) {
            sites_changed++;
        }
    }
    return sites_changed;
}

routine_scout_policy_result routine_scout_policy( const site_record &site )
{
    routine_scout_policy_result policy;
    const hostile_site_profile profile = effective_profile( site );
    policy.applies = supports_routine_camp_ecology( profile );
    if( !policy.applies ) {
        policy.rejection_reason = "routine pair policy does not apply to this site profile";
        return policy;
    }

    const roster_view roster = site.roster();
    if( !roster.valid ) {
        policy.rejection_reason = "invalid roster authority";
        return policy;
    }
    if( site.retired_empty_site ) {
        policy.rejection_reason = "retired empty site";
        return policy;
    }
    if( site.has_active_outside_pressure() ) {
        policy.rejection_reason = "active external operation or unresolved reservation";
        return policy;
    }
    if( roster.living_total < 2 ) {
        policy.rejection_reason = "fewer than two living members";
        return policy;
    }

    policy.party_size = 2;
    policy.required_local_reserve = roster.living_total == 2 ? 0 : 1;
    policy.concrete_ready_goal = policy.party_size + policy.required_local_reserve;
    if( roster.ready_total < policy.concrete_ready_goal ) {
        policy.rejection_reason = "fewer than the paired party plus required ready reserve";
        return policy;
    }

    policy.eligible = true;
    return policy;
}

int routine_scout_materialization_count( const site_record &site )
{
    const routine_scout_policy_result policy = routine_scout_policy( site );
    const roster_view roster = site.roster();
    if( !policy.eligible || !roster.valid ) {
        return 0;
    }
    return std::min( roster.unmaterialized_home_total,
                     std::max( 0, policy.concrete_ready_goal - roster.ready_concrete_total ) );
}

bool routine_member_is_unready( const routine_member_readiness_snapshot &snapshot )
{
    return !snapshot.present || snapshot.dead || snapshot.hp_percent <= 50 ||
           snapshot.sleeping || snapshot.incapacitated;
}

bool member_has_abstract_wound_recovery( const member_record &member, const int now_minutes )
{
    return now_minutes >= 0 && member.abstract_wound_until_minutes > now_minutes;
}

routine_scout_pair_selection_result select_routine_scout_pair( const site_record &site )
{
    routine_scout_pair_selection_result selection;
    const routine_scout_policy_result policy = routine_scout_policy( site );
    if( !policy.eligible ) {
        selection.rejection_reason = policy.rejection_reason;
        return selection;
    }

    const member_record *observer = nullptr;
    routine_member_capability observer_capability;
    for( const member_record &member : site.members ) {
        if( member.state != member_state::at_home || member.wounded_or_unready ) {
            continue;
        }
        const routine_member_capability capability = routine_capability_for_template(
                    member.npc_template_id );
        if( capability.observer <= 0 ) {
            continue;
        }
        if( observer == nullptr || capability.observer > observer_capability.observer ||
            ( capability.observer == observer_capability.observer &&
              capability.defender < observer_capability.defender ) ||
            ( capability.observer == observer_capability.observer &&
              capability.defender == observer_capability.defender &&
              member.npc_id < observer->npc_id ) ) {
            observer = &member;
            observer_capability = capability;
        }
    }
    if( observer == nullptr ) {
        selection.rejection_reason = "no ready member has routine observer capability";
        return selection;
    }

    const member_record *escort = nullptr;
    routine_member_capability escort_capability;
    for( const member_record &member : site.members ) {
        if( member.npc_id == observer->npc_id || member.state != member_state::at_home ||
            member.wounded_or_unready ) {
            continue;
        }
        const routine_member_capability capability = routine_capability_for_template(
                    member.npc_template_id );
        if( capability.escort < 2 ) {
            continue;
        }
        if( escort == nullptr || capability.defender < escort_capability.defender ||
            ( capability.defender == escort_capability.defender &&
              capability.escort > escort_capability.escort ) ||
            ( capability.defender == escort_capability.defender &&
              capability.escort == escort_capability.escort && member.npc_id < escort->npc_id ) ) {
            escort = &member;
            escort_capability = capability;
        }
    }
    if( escort == nullptr ) {
        selection.rejection_reason = "no second ready member has return-safe escort capability";
        return selection;
    }

    selection.eligible = true;
    selection.observer_id = observer->npc_id;
    selection.escort_id = escort->npc_id;
    selection.observer_capability = observer_capability.observer;
    selection.escort_capability = escort_capability.escort;
    selection.return_safe_escort = true;
    selection.member_ids = { selection.observer_id, selection.escort_id };
    return selection;
}

response_party_policy_result response_party_policy( const site_record &site,
        const bandit_dry_run::job_template job, const int requested_party_size )
{
    response_party_policy_result policy;
    policy.applies = !is_routine_scout_job( job ) &&
                     job != bandit_dry_run::job_template::hold_chill;
    if( !policy.applies ) {
        policy.rejection_reason = "response policy does not size routine or idle jobs";
        return policy;
    }

    const roster_view roster = site.roster();
    if( !roster.valid ) {
        policy.rejection_reason = "invalid roster authority";
        return policy;
    }
    if( site.retired_empty_site || site.has_active_outside_pressure() ) {
        policy.rejection_reason = "site cannot start a fresh response operation";
        return policy;
    }

    policy.required_local_reserve = required_home_reserve( site );
    const int minimum_party_size = required_dispatch_members_for_profile( site, job );
    policy.party_size = requested_party_size > 0 ? requested_party_size : minimum_party_size;
    if( policy.party_size <= 0 ) {
        policy.rejection_reason = "response job has no valid party size";
        return policy;
    }
    if( policy.party_size < minimum_party_size ) {
        policy.rejection_reason = "requested response party is below the job/profile minimum";
        return policy;
    }
    if( roster.ready_concrete_total - policy.required_local_reserve < policy.party_size ) {
        policy.rejection_reason = "response party would consume its required home reserve";
        return policy;
    }

    policy.eligible = true;
    return policy;
}

int site_record::active_outing_survivor_count() const
{
    const active_outing_state *outing = active_external_outing();
    if( outing == nullptr ) {
        return 0;
    }
    return static_cast<int>( std::count_if( outing->member_ids.begin(),
    outing->member_ids.end(), [this, outing]( const character_id & member_id ) {
        if( outing->member_is_resolved( member_id ) ) {
            return false;
        }
        if( std::find( outing->casualty_ids.begin(), outing->casualty_ids.end(), member_id ) !=
            outing->casualty_ids.end() ) {
            return false;
        }
        const member_record *member = find_member( member_id );
        return member != nullptr && member->state != member_state::dead &&
               member->state != member_state::missing;
    } ) );
}

int site_record::count_home_side_signals() const
{
    const roster_view current_roster = roster();
    return current_roster.valid ? current_roster.physically_present_total : 1;
}

int site_record::dispatchable_member_capacity() const
{
    if( retired_empty_site ) {
        return 0;
    }
    const roster_view current_roster = roster();
    if( !current_roster.valid ) {
        return 0;
    }
    return std::max( 0, current_roster.ready_concrete_total - required_home_reserve( *this ) );
}

bool site_record::has_active_outside_pressure() const
{
    return active_outing.is_active() || !active_outing.member_ids.empty() ||
           active_hostile_operation.is_active() ||
           !active_hostile_operation.reservation.member_ids.empty() ||
           count_members_in_state( member_state::outbound ) > 0 ||
           count_members_in_state( member_state::local_contact ) > 0;
}

active_outing_state *site_record::active_external_outing()
{
    const bool active_outing_present = active_outing.is_active() ||
                                       !active_outing.member_ids.empty();
    const bool hostile_operation_present = active_hostile_operation.is_active() ||
            !active_hostile_operation.reservation.member_ids.empty();
    if( active_outing_present == hostile_operation_present ) {
        return nullptr;
    }
    if( active_outing_present ) {
        return &active_outing;
    }
    if( hostile_operation_present ) {
        return &active_hostile_operation.reservation;
    }
    return nullptr;
}

const active_outing_state *site_record::active_external_outing() const
{
    const bool active_outing_present = active_outing.is_active() ||
                                       !active_outing.member_ids.empty();
    const bool hostile_operation_present = active_hostile_operation.is_active() ||
            !active_hostile_operation.reservation.member_ids.empty();
    if( active_outing_present == hostile_operation_present ) {
        return nullptr;
    }
    if( active_outing_present ) {
        return &active_outing;
    }
    if( hostile_operation_present ) {
        return &active_hostile_operation.reservation;
    }
    return nullptr;
}

std::optional<simulation_advance_cursor> current_external_simulation_cursor(
    const site_record &site )
{
    const active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr || !outing->is_active() ||
        !simulation_owner_state_is_consistent( *outing ) ) {
        return std::nullopt;
    }
    simulation_advance_cursor cursor;
    cursor.activity_id = outing->activity_id;
    cursor.generation = outing->generation;
    cursor.owner = outing->owner;
    cursor.handoff_epoch = outing->handoff_epoch;
    cursor.last_advanced_minutes = outing->last_advanced_minutes;
    cursor.covert_egress_revision = outing->covert_egress_revision;
    return cursor;
}

bool site_record::eligible_for_empty_site_retirement() const
{
    return !retired_empty_site && site_kind != owned_site_kind::none &&
           profile_for_site_kind( site_kind ) != hostile_site_profile::none &&
           count_home_side_signals() == 0 && !has_active_outside_pressure();
}

void world_state::clear()
{
    schema_version = 6;
    owner_id = "hells_raiders_live_owner_v0";
    routine_scheduler_cursor = 0;
    routine_terrain_scan_cursor = 0;
    routine_scheduler_last_hour = -1;
    sites.clear();
    finite_resources.clear();
}

void world_state::serialize( JsonOut &json ) const
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::world_serialize );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::world_serialize_calls );
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "owner_id", owner_id );
    if( schema_version >= 5 ) {
        const int bounded_scheduler_cursor = sites.empty() || routine_scheduler_cursor < 0 ? 0 :
                                             routine_scheduler_cursor % static_cast<int>( sites.size() );
        json.member( "routine_scheduler_cursor", bounded_scheduler_cursor );
        if( schema_version >= 6 ) {
            const int bounded_terrain_scan_cursor = sites.empty() || routine_terrain_scan_cursor < 0 ? 0 :
                                                    routine_terrain_scan_cursor %
                                                    static_cast<int>( sites.size() );
            json.member( "routine_terrain_scan_cursor", bounded_terrain_scan_cursor );
        }
        json.member( "routine_scheduler_last_hour", routine_scheduler_last_hour );
    }
    json.member( "sites", sites );
    if( !finite_resources.empty() ) {
        json.member( "finite_resources" );
        json.start_array();
        for( const auto &entry : finite_resources ) {
            json.start_array();
            json.write( entry.first.x() );
            json.write( entry.first.y() );
            json.write( entry.first.z() );
            json.write( entry.second.remaining_units );
            json.write( entry.second.revision );
            json.end_array();
        }
        json.end_array();
    }
    json.end_object();
}

void world_state::deserialize( const JsonObject &jo )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::world_deserialize );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::world_deserialize_calls );
    world_state candidate;
    int loaded_schema_version = 0;
    jo.read( "schema_version", loaded_schema_version );
    if( loaded_schema_version < 0 || loaded_schema_version > 6 ) {
        jo.throw_error( "hostile live-world schema version is unsupported" );
    }
    jo.read( "owner_id", candidate.owner_id );
    jo.read( "sites", candidate.sites );
    const bool any_scheduler_field = jo.has_member( "routine_scheduler_cursor" ) ||
                                     jo.has_member( "routine_terrain_scan_cursor" ) ||
                                     jo.has_member( "routine_scheduler_last_hour" );
    const bool complete_v5_scheduler_payload = jo.has_member( "routine_scheduler_cursor" ) &&
                                               jo.has_member( "routine_scheduler_last_hour" );
    const bool complete_v6_scheduler_payload = complete_v5_scheduler_payload &&
                                               jo.has_member( "routine_terrain_scan_cursor" );
    if( loaded_schema_version < 5 && any_scheduler_field ) {
        jo.throw_error( "pre-v5 world cannot contain routine scheduler state" );
    }
    if( loaded_schema_version == 5 &&
        ( !complete_v5_scheduler_payload || jo.has_member( "routine_terrain_scan_cursor" ) ) ) {
        jo.throw_error( "v5 world must contain exactly the v5 routine scheduler state" );
    }
    if( loaded_schema_version >= 6 && !complete_v6_scheduler_payload ) {
        jo.throw_error( "v6 world must contain complete routine scheduler state" );
    }
    if( complete_v5_scheduler_payload ) {
        jo.read( "routine_scheduler_cursor", candidate.routine_scheduler_cursor );
        jo.read( "routine_scheduler_last_hour", candidate.routine_scheduler_last_hour );
        if( loaded_schema_version >= 6 ) {
            jo.read( "routine_terrain_scan_cursor", candidate.routine_terrain_scan_cursor );
        }
        const bool cursor_is_valid = candidate.sites.empty() ?
                                     candidate.routine_scheduler_cursor == 0 :
                                     candidate.routine_scheduler_cursor >= 0 &&
                                     candidate.routine_scheduler_cursor <
                                     static_cast<int>( candidate.sites.size() );
        const bool terrain_cursor_is_valid = candidate.sites.empty() ?
                candidate.routine_terrain_scan_cursor == 0 :
                candidate.routine_terrain_scan_cursor >= 0 &&
                candidate.routine_terrain_scan_cursor < static_cast<int>( candidate.sites.size() );
        if( !cursor_is_valid || !terrain_cursor_is_valid ||
            candidate.routine_scheduler_last_hour < -1 ) {
            jo.throw_error( "world has malformed routine scheduler state" );
        }
    }
    std::set<character_id> claimed_member_ids;
    for( const site_record &site : candidate.sites ) {
        for( const member_record &member : site.members ) {
            if( !claimed_member_ids.insert( member.npc_id ).second ) {
                jo.throw_error( "stable hostile-camp member ID is claimed by more than one site" );
            }
        }
    }
    if( jo.has_member( "finite_resources" ) ) {
        if( loaded_schema_version < 4 ) {
            jo.throw_error( "pre-v4 world cannot contain finite resource state" );
        }
        for( JsonArray packed_resource : jo.get_array( "finite_resources" ) ) {
            if( packed_resource.size() != 5 ) {
                packed_resource.throw_error( "finite resource must be [x,y,z,remaining,revision]" );
            }
            const tripoint_abs_omt omt( packed_resource.get_int( 0 ),
                                        packed_resource.get_int( 1 ),
                                        packed_resource.get_int( 2 ) );
            finite_resource_record resource;
            resource.remaining_units = packed_resource.get_int( 3 );
            resource.revision = packed_resource.get_int( 4 );
            if( !finite_resource_record_is_valid( resource ) ) {
                packed_resource.throw_error( "finite resource has invalid remaining units or revision" );
            }
            if( !candidate.finite_resources.emplace( omt, resource ).second ) {
                packed_resource.throw_error( "finite resource OMT is duplicated" );
            }
        }
    }
    if( loaded_schema_version < 4 ) {
        for( const site_record &site : candidate.sites ) {
            for( const camp_map_lead &lead : site.intelligence_map.leads ) {
                if( lead.kind == camp_lead_kind::structural_bounty &&
                    lead.status == camp_lead_status::harvested ) {
                    candidate.finite_resources.emplace( lead.omt, finite_resource_record { 0, 1 } );
                }
            }
        }
    }
    candidate.schema_version = 6;
    *this = std::move( candidate );
}

site_record *world_state::find_site( const std::string &site_id )
{
    auto iter = std::find_if( sites.begin(), sites.end(), [&site_id]( const site_record & site ) {
        return site.site_id == site_id;
    } );
    return iter != sites.end() ? &*iter : nullptr;
}

const site_record *world_state::find_site( const std::string &site_id ) const
{
    auto iter = std::find_if( sites.begin(), sites.end(), [&site_id]( const site_record & site ) {
        return site.site_id == site_id;
    } );
    return iter != sites.end() ? &*iter : nullptr;
}

const finite_resource_record *world_state::find_finite_resource(
    const tripoint_abs_omt &omt ) const
{
    const auto iter = finite_resources.find( omt );
    return iter != finite_resources.end() ? &iter->second : nullptr;
}

finite_resource_record finite_resource_snapshot( const world_state &state,
        const tripoint_abs_omt &omt, const int undiscovered_units )
{
    if( const finite_resource_record *resource = state.find_finite_resource( omt ) ) {
        return *resource;
    }
    return finite_resource_record { std::clamp( undiscovered_units, 0,
                                    max_finite_resource_units ), 0 };
}

std::string finite_resource_claim_application_key( const std::string &operation_id,
        const int operation_generation, const tripoint_abs_omt &omt )
{
    const std::string component = "resource:" + std::to_string( omt.x() ) + "," +
                                  std::to_string( omt.y() ) + "," +
                                  std::to_string( omt.z() );
    return bandit_pursuit_handoff::make_operation_component_key(
               operation_id, operation_generation, component );
}

finite_resource_claim_result claim_finite_resource_units( world_state &state,
        const std::string &claimant_site_id, const tripoint_abs_omt &omt,
        const finite_resource_record &expected,
        const int requested_units, const std::string &operation_id,
        const int operation_generation, const std::string &application_key )
{
    finite_resource_claim_result result;
    site_record *claimant = state.find_site( claimant_site_id );
    const active_outing_state *issued_operation = claimant == nullptr ? nullptr :
            claimant->active_external_outing();
    if( requested_units <= 0 || requested_units > max_finite_resource_claim_units ||
        expected.remaining_units < 0 || expected.remaining_units > max_finite_resource_units ||
        expected.revision < 0 || expected.revision > max_finite_resource_units ||
        claimant == nullptr || claimant_site_id.empty() || operation_generation <= 0 ||
        operation_generation >= std::numeric_limits<int>::max() - 1 ||
        operation_id.rfind( claimant_site_id + "#", 0 ) != 0 ||
        application_key.size() > max_operation_application_key_length ||
        application_key != finite_resource_claim_application_key(
            operation_id, operation_generation, omt ) ) {
        return result;
    }
    result.application_key = application_key;

    const auto current = state.finite_resources.find( omt );
    if( current != state.finite_resources.end() ) {
        result.remaining_units = current->second.remaining_units;
        result.revision = current->second.revision;
        if( !finite_resource_record_is_valid( current->second ) ) {
            return result;
        }
    }

    if( operation_generation < claimant->applied_resource_generation ) {
        result.status = finite_resource_claim_status::stale;
        return result;
    }
    if( operation_generation == claimant->applied_resource_generation ) {
        if( application_key == claimant->last_resource_application_key ) {
            result.status = finite_resource_claim_status::already_applied;
            result.claimed_units = claimant->last_resource_claimed_units;
        }
        return result;
    }
    const bool issued_resource_job = issued_operation != nullptr &&
            ( ( issued_operation->kind == outing_kind::structural_sortie &&
                ( issued_operation->job_type == "scavenge" ||
                  issued_operation->job_type == "scout" ) ) ||
              ( issued_operation->kind == outing_kind::scout_sortie &&
                issued_operation->job_type == "scavenge" ) );
    if( issued_operation == nullptr || !issued_operation->is_active() ||
        issued_operation->activity_id != operation_id ||
        issued_operation->generation != operation_generation ||
        issued_operation->target_omt != omt || !issued_resource_job ) {
        return result;
    }

    if( current != state.finite_resources.end() ) {
        if( current->second.remaining_units != expected.remaining_units ||
            current->second.revision != expected.revision ) {
            result.status = finite_resource_claim_status::stale;
            return result;
        }
    } else if( expected.revision != 0 ) {
        result.status = finite_resource_claim_status::stale;
        return result;
    }

    if( expected.remaining_units == 0 ) {
        result.status = finite_resource_claim_status::depleted;
        return result;
    }

    const int claimed_units = std::min( requested_units, expected.remaining_units );
    finite_resource_record updated;
    updated.remaining_units = expected.remaining_units - claimed_units;
    updated.revision = expected.revision + 1;
    if( !finite_resource_record_is_valid( updated ) ) {
        return result;
    }

    if( current == state.finite_resources.end() ) {
        state.finite_resources.emplace( omt, updated );
    } else {
        current->second = updated;
    }
    claimant->applied_resource_generation = operation_generation;
    claimant->last_resource_application_key = application_key;
    claimant->last_resource_claimed_units = claimed_units;
    state.schema_version = 6;
    result.status = finite_resource_claim_status::applied;
    result.claimed_units = claimed_units;
    result.remaining_units = updated.remaining_units;
    result.revision = updated.revision;
    return result;
}

bool is_tracked_hostile_template( const std::string &npc_template_id )
{
    static const std::array<std::string, 9> tracked_templates = {
        "bandit",
        "thug",
        "bandit_trader",
        "bandit_quartermaster",
        "bandit_mechanic",
        "hells_raiders_boss",
        "cannibal_hunter",
        "cannibal_butcher",
        "cannibal_camp_leader",
    };

    return std::find( tracked_templates.begin(), tracked_templates.end(),
                      npc_template_id ) != tracked_templates.end();
}

std::optional<owned_site_kind> classify_tracked_source( anchor_source_kind source_kind,
        const std::string &source_id )
{
    switch( source_kind ) {
        case anchor_source_kind::overmap_special:
            if( source_id == "bandit_camp" ) {
                return owned_site_kind::bandit_camp;
            }
            if( source_id == "bandit_work_camp" ) {
                return owned_site_kind::bandit_work_camp;
            }
            if( source_id == "bandit_cabin" ) {
                return owned_site_kind::bandit_cabin;
            }
            if( source_id == "cannibal_camp" ) {
                return owned_site_kind::cannibal_camp;
            }
            break;
        case anchor_source_kind::map_extra:
            if( source_id == "mx_looters" ) {
                return owned_site_kind::looters;
            }
            if( source_id == "mx_bandits_block" ) {
                return owned_site_kind::bandits_block;
            }
            break;
        case anchor_source_kind::none:
            break;
    }

    return std::nullopt;
}

hostile_site_profile profile_for_site_kind( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
        case owned_site_kind::bandit_cabin:
            return hostile_site_profile::camp_style;
        case owned_site_kind::cannibal_camp:
            return hostile_site_profile::cannibal_camp;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
            return hostile_site_profile::small_hostile_site;
        case owned_site_kind::none:
            return hostile_site_profile::none;
    }

    return hostile_site_profile::none;
}

footprint_snapshot make_special_footprint( const std::string &special_id,
        const tripoint_abs_omt &origin,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    footprint_snapshot snapshot;
    snapshot.anchor = origin;
    snapshot.footprint.push_back( origin );

    if( !special_lookup ) {
        return snapshot;
    }

    snapshot.footprint.clear();
    const int radius = special_footprint_radius( special_id );
    for( const int z : special_footprint_z_levels( origin ) ) {
        for( int dx = -radius; dx <= radius; dx++ ) {
            for( int dy = -radius; dy <= radius; dy++ ) {
                const tripoint_abs_omt candidate( origin.x() + dx, origin.y() + dy, z );
                if( special_lookup( candidate ) == std::optional<std::string>( special_id ) ) {
                    snapshot.footprint.push_back( candidate );
                }
            }
        }
    }

    if( snapshot.footprint.empty() ) {
        snapshot.footprint.push_back( origin );
    }

    std::sort( snapshot.footprint.begin(), snapshot.footprint.end(), omt_less );
    snapshot.footprint.erase( std::unique( snapshot.footprint.begin(), snapshot.footprint.end() ),
                              snapshot.footprint.end() );
    snapshot.anchor = snapshot.footprint.front();
    return snapshot;
}

std::string make_site_id( anchor_source_kind source_kind, const std::string &source_id,
                          const tripoint_abs_omt &anchor )
{
    std::ostringstream out;
    out << to_string( source_kind ) << ':' << source_id << '@'
        << anchor.x() << ',' << anchor.y() << ',' << anchor.z();
    return out.str();
}

int abstract_roster_seed_for_site_kind( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
            return 6;
        case owned_site_kind::cannibal_camp:
            return 5;
        case owned_site_kind::bandit_cabin:
            return 3;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
            return 2;
        case owned_site_kind::none:
            return 0;
    }

    return 0;
}

bool register_abstract_site( world_state &state, anchor_source_kind source_kind,
                             const std::string &source_id, const tripoint_abs_omt &origin,
                             const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup,
                             int abstract_headcount )
{
    const std::optional<owned_site_kind> site_kind = classify_tracked_source( source_kind, source_id );
    if( !site_kind ) {
        return false;
    }

    footprint_snapshot footprint;
    if( source_kind == anchor_source_kind::overmap_special ) {
        footprint = make_special_footprint( source_id, origin, special_lookup );
    } else {
        footprint.anchor = origin;
        footprint.footprint = { origin };
    }

    const std::string site_id = make_site_id( source_kind, source_id, footprint.anchor );
    site_record *site = state.find_site( site_id );
    if( site == nullptr ) {
        site_record new_site;
        new_site.site_id = site_id;
        new_site.source_kind = source_kind;
        new_site.site_kind = *site_kind;
        new_site.profile = profile_for_site_kind( *site_kind );
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        new_site.living_total = std::max( 0, abstract_headcount );
        seed_uninitialized_camp_supply( new_site );
        state.sites.push_back( new_site );
        return true;
    }

    if( terminal_origin_disposition( site->origin ) || site->retired_empty_site ) {
        return false;
    }

    if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }
    if( site->source_kind == anchor_source_kind::none ) {
        site->source_kind = source_kind;
    }
    if( site->site_kind == owned_site_kind::none ) {
        site->site_kind = *site_kind;
    }
    if( site->profile == hostile_site_profile::none ) {
        site->profile = profile_for_site_kind( site->site_kind );
    }
    if( site->source_id.empty() ) {
        site->source_id = source_id;
    }
    site->living_total = std::max( site->living_total, std::max( 0, abstract_headcount ) );
    seed_uninitialized_camp_supply( *site );
    return true;
}

abstract_bootstrap_result register_abstract_sites_near(
    world_state &state, const tripoint_abs_omt &center, int radius_omt,
    const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    abstract_bootstrap_result result;
    if( !special_lookup || radius_omt < 0 ) {
        return result;
    }

    for( int dx = -radius_omt; dx <= radius_omt; ++dx ) {
        for( int dy = -radius_omt; dy <= radius_omt; ++dy ) {
            const tripoint_abs_omt candidate( center.x() + dx, center.y() + dy, center.z() );
            if( rl_dist( center, candidate ) > radius_omt ) {
                continue;
            }
            const std::optional<std::string> source_id = special_lookup( candidate );
            if( !source_id ) {
                continue;
            }
            const std::optional<owned_site_kind> site_kind = classify_tracked_source(
                        anchor_source_kind::overmap_special, *source_id );
            if( !site_kind ) {
                continue;
            }
            result.recognized_tiles++;
            const size_t old_site_count = state.sites.size();
            register_abstract_site( state, anchor_source_kind::overmap_special, *source_id, candidate,
                                    special_lookup, abstract_roster_seed_for_site_kind( *site_kind ) );
            if( state.sites.size() > old_site_count ) {
                result.created_sites++;
            }
        }
    }

    return result;
}

bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    if( !is_tracked_hostile_template( npc_template_id ) ) {
        return false;
    }

    anchor_source_kind source_kind = anchor_source_kind::none;
    std::string source_id;
    std::optional<owned_site_kind> site_kind;

    if( overmap_special_id ) {
        site_kind = classify_tracked_source( anchor_source_kind::overmap_special, *overmap_special_id );
        if( site_kind ) {
            source_kind = anchor_source_kind::overmap_special;
            source_id = *overmap_special_id;
        }
    }

    if( !site_kind && map_extra_id ) {
        site_kind = classify_tracked_source( anchor_source_kind::map_extra, *map_extra_id );
        if( site_kind ) {
            source_kind = anchor_source_kind::map_extra;
            source_id = *map_extra_id;
        }
    }

    if( !site_kind ) {
        return false;
    }

    const tripoint_abs_omt spawn_omt = project_to<coords::omt>( spawn_tile );
    footprint_snapshot footprint;
    if( source_kind == anchor_source_kind::overmap_special ) {
        footprint = make_special_footprint( source_id, spawn_omt, special_lookup );
    } else {
        footprint.anchor = spawn_omt;
        footprint.footprint = { spawn_omt };
    }

    const std::string site_id = make_site_id( source_kind, source_id, footprint.anchor );
    for( const site_record &existing_site : state.sites ) {
        if( existing_site.site_id != site_id && existing_site.has_member( npc_id ) ) {
            return false;
        }
    }
    site_record *site = state.find_site( site_id );
    if( site == nullptr ) {
        site_record new_site;
        new_site.site_id = site_id;
        new_site.source_kind = source_kind;
        new_site.site_kind = *site_kind;
        new_site.profile = profile_for_site_kind( *site_kind );
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        state.sites.push_back( new_site );
        site = &state.sites.back();
    } else if( terminal_origin_disposition( site->origin ) ) {
        return false;
    } else if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }
    if( site->profile == hostile_site_profile::none ) {
        site->profile = profile_for_site_kind( site->site_kind );
    }
    if( site->retired_empty_site ) {
        site->retired_empty_site = false;
        site->retirement_summary = "reactivated by tracked hostile spawn " + std::to_string( npc_id.get_value() );
    }

    if( site->has_member( npc_id ) ) {
        seed_uninitialized_camp_supply( *site );
        return true;
    }

    member_record member;
    member.npc_id = npc_id;
    member.npc_template_id = npc_template_id;
    member.home_spawn_tile = spawn_tile;
    site->members.push_back( member );
    site->living_total = std::max( site->living_total, site->count_live_members() );

    spawn_tile_record *spawn_tile_record_ptr = site->find_spawn_tile( spawn_tile );
    if( spawn_tile_record_ptr == nullptr ) {
        spawn_tile_record new_spawn_tile;
        new_spawn_tile.tile = spawn_tile;
        site->spawn_tiles.push_back( new_spawn_tile );
        spawn_tile_record_ptr = &site->spawn_tiles.back();
    }
    const int concrete_living_assigned = static_cast<int>( std::count_if(
            site->members.begin(), site->members.end(), [spawn_tile]( const member_record & existing ) {
        return counts_toward_live_headcount( existing.state ) &&
               existing.home_spawn_tile == spawn_tile;
    } ) );
    spawn_tile_record_ptr->assigned_living_total = std::max(
                spawn_tile_record_ptr->assigned_living_total, concrete_living_assigned );
    long long assigned_living_total = 0;
    for( const spawn_tile_record &record : site->spawn_tiles ) {
        assigned_living_total += std::max( 0, record.assigned_living_total );
    }
    long long excess_assignments = std::max<long long>(
                                       0, assigned_living_total - site->living_total );
    for( spawn_tile_record &record : site->spawn_tiles ) {
        if( excess_assignments <= 0 ) {
            break;
        }
        const int concrete_living_on_tile = static_cast<int>( std::count_if(
                site->members.begin(), site->members.end(), [&record]( const member_record & existing ) {
            return counts_toward_live_headcount( existing.state ) &&
                   existing.home_spawn_tile == record.tile;
        } ) );
        const int transferable_assignments = std::max(
                0, record.assigned_living_total - concrete_living_on_tile );
        const int transferred = static_cast<int>( std::min<long long>(
                                    transferable_assignments, excess_assignments ) );
        record.assigned_living_total -= transferred;
        excess_assignments -= transferred;
    }
    if( excess_assignments > 0 ) {
        site->living_total = static_cast<int>( std::min<long long>(
                                 std::numeric_limits<int>::max(),
                                 static_cast<long long>( site->living_total ) + excess_assignments ) );
    }
    seed_uninitialized_camp_supply( *site );
    return true;
}


static int ready_at_home_member_count( const bandit_live_world::site_record &site )
{
    return static_cast<int>( std::count_if( site.members.begin(), site.members.end(),
    []( const bandit_live_world::member_record & member ) {
        return member.state == member_state::at_home && !member.wounded_or_unready;
    } ) );
}

static int wounded_or_unready_member_count( const bandit_live_world::site_record &site )
{
    return static_cast<int>( std::count_if( site.members.begin(), site.members.end(),
    []( const bandit_live_world::member_record & member ) {
        return counts_toward_live_headcount( member.state ) && member.wounded_or_unready;
    } ) );
}

static int active_outside_member_count( const bandit_live_world::site_record &site )
{
    std::vector<character_id> outside_members;
    outside_members.reserve( site.active_outing.member_ids.size() );
    for( const character_id &member_id : site.active_outing.member_ids ) {
        if( site.active_outing.member_is_resolved( member_id ) ) {
            continue;
        }
        if( std::find( outside_members.begin(), outside_members.end(), member_id ) == outside_members.end() ) {
            outside_members.push_back( member_id );
        }
    }
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::outbound && member.state != member_state::local_contact ) {
            continue;
        }
        if( std::find( outside_members.begin(), outside_members.end(), member.npc_id ) == outside_members.end() ) {
            outside_members.push_back( member.npc_id );
        }
    }
    return static_cast<int>( outside_members.size() );
}

static int ceil_percent( const int value, const int percent )
{
    if( value <= 0 || percent <= 0 ) {
        return 0;
    }
    return ( value * percent + 99 ) / 100;
}

static int camp_map_home_reserve_for_lead( const bandit_live_world::site_record &site,
                                    const bandit_live_world::camp_map_lead &lead,
                                    const int stockpile_pressure )
{
    int reserve = required_home_reserve( site );
    const bandit_live_world::roster_view roster = site.roster();
    const int living_roster = roster.valid ? roster.living_total : 0;
    if( effective_profile( site ) == hostile_site_profile::camp_style &&
        ( lead.prior_bandit_losses > 0 || lead.target_alert || lead.scout_seen ) ) {
        reserve += 1;
    }
    if( stockpile_pressure >= 3 ) {
        const int minimum_reserve = living_roster >= 5 ? 2 : 1;
        reserve = std::max( minimum_reserve, reserve - 1 );
    }
    return std::clamp( reserve, 0, living_roster );
}

static int stalk_pressure_member_count( const int living_roster, const int dispatchable )
{
    if( dispatchable < 2 ) {
        return 0;
    }
    if( living_roster == 2 ) {
        return 2;
    }
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, 35 ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( ceil_percent( dispatchable, 40 ), 2, upper_bound );
}

static int toll_pressure_member_count( const int living_roster, const int dispatchable )
{
    if( dispatchable < 4 || living_roster < 7 ) {
        return 0;
    }
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, 40 ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( ceil_percent( dispatchable, 45 ), 2, upper_bound );
}

static int cannibal_attack_member_count( const int living_roster, const int dispatchable,
                                  const int margin )
{
    if( dispatchable < 2 ) {
        return 0;
    }
    if( margin >= 6 ) {
        return dispatchable;
    }
    const int percent = margin >= 4 ? 60 : 45;
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, percent ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( upper_bound, 2, dispatchable );
}

camp_map_dispatch_decision choose_camp_map_dispatch( const site_record &site,
        const camp_map_lead &lead, const camp_map_dispatch_pressure &pressure )
{
    camp_map_dispatch_decision decision;
    const roster_view roster = site.roster();
    const routine_scout_policy_result routine_policy = routine_scout_policy( site );
    decision.valid = true;
    decision.living_roster = roster.valid ? roster.living_total : 0;
    decision.ready_at_home = ready_at_home_member_count( site );
    decision.wounded_or_unready = wounded_or_unready_member_count( site );
    decision.active_outside = active_outside_member_count( site );
    decision.hard_home_reserve = camp_map_home_reserve_for_lead( site, lead,
                                 pressure.stockpile_pressure );
    decision.dispatchable = std::max( 0, decision.ready_at_home - decision.hard_home_reserve );

    decision.reward_score = std::max( 0, lead.bounty ) + std::max( 0, lead.confidence ) +
                            std::clamp( pressure.stockpile_pressure, 0, 3 ) +
                            std::min( 2, std::max( 0, lead.prior_defender_losses ) );
    if( pressure.opening_available ) {
        decision.reward_score += 1;
    }

    decision.risk_score = std::max( 0, lead.threat ) + std::max( 0, 2 - lead.confidence ) +
                          std::min( 4, std::max( 0, lead.prior_bandit_losses ) * 2 );
    if( lead.target_alert ) {
        decision.risk_score += 2;
    }
    if( lead.scout_seen ) {
        decision.risk_score += 1;
    }
    if( !pressure.opening_available ) {
        decision.risk_score += 2;
    }
    decision.margin = decision.reward_score - decision.risk_score;

    decision.notes.push_back( "camp-map decision uses ready roster, wounded/unready, reserve, and lead pressure" );
    decision.notes.push_back( "camp-map roster living=" + std::to_string( decision.living_roster ) +
                              " ready_at_home=" + std::to_string( decision.ready_at_home ) +
                              " wounded_or_unready=" + std::to_string( decision.wounded_or_unready ) +
                              " active_outside=" + std::to_string( decision.active_outside ) +
                              " reserve=" + std::to_string( decision.hard_home_reserve ) +
                              " dispatchable=" + std::to_string( decision.dispatchable ) );
    if( pressure.stockpile_pressure >= 3 ) {
        decision.notes.push_back( "stockpile pressure may loosen reserve by one but cannot cross hard minimum" );
    }
    decision.notes.push_back( "camp-map opening_state=" + pressure.opening_state +
                              " opening_available=" + std::string( pressure.opening_available ? "yes" : "no" ) );
    if( lead.prior_bandit_losses > 0 || lead.target_alert || lead.scout_seen ) {
        decision.notes.push_back( "losses/alert add caution before greed can size the outing" );
    }

    if( site.retired_empty_site ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: retired empty site" );
        return decision;
    }
    if( site.has_active_outside_pressure() ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: unresolved active outside group/contact blocks dogpile" );
        return decision;
    }
    if( decision.dispatchable <= 0 && !routine_policy.eligible ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: no response capacity and routine pair is ineligible: " +
                                  routine_policy.rejection_reason );
        return decision;
    }

    const auto select_routine_party = [&decision, &routine_policy, &site]( const std::string &reason ) {
        const bool micro_site = effective_profile( site ) ==
                                hostile_site_profile::small_hostile_site;
        if( !routine_policy.eligible && !micro_site ) {
            decision.intent = bandit_dry_run::job_template::hold_chill;
            decision.selected_member_count = 0;
            decision.notes.push_back( "hold: routine pair is ineligible: " +
                                      routine_policy.rejection_reason );
            return;
        }
        decision.intent = bandit_dry_run::job_template::scout;
        decision.selected_member_count = micro_site ? 1 : routine_policy.party_size;
        decision.hard_home_reserve = micro_site ? 0 : routine_policy.required_local_reserve;
        decision.dispatchable = std::max( 0, decision.ready_at_home -
                                          decision.hard_home_reserve );
        decision.notes.push_back( reason );
    };

    if( !pressure.opening_available && lead.status == camp_lead_status::active ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: active stalk pressure found no opening and should return/decay" );
        return decision;
    }

    if( decision.margin <= -2 || ( lead.threat >= lead.bounty + 2 && decision.margin <= 1 ) ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: high threat or poor reward does not escalate by itself" );
        return decision;
    }

    if( lead.confidence <= 1 || lead.status == camp_lead_status::stale ) {
        select_routine_party( "scout party: low-confidence or stale memory needs eyes before pressure" );
        return decision;
    }

    if( decision.margin >= 2 ) {
        const hostile_site_profile profile = effective_profile( site );
        const bool scout_confirmed_basecamp = lead.status == camp_lead_status::scout_confirmed &&
                lead.kind == camp_lead_kind::basecamp_activity;
        if( profile == hostile_site_profile::cannibal_camp && scout_confirmed_basecamp ) {
            const int attackers = cannibal_attack_member_count( decision.living_roster,
                                  decision.dispatchable, decision.margin );
            if( attackers >= 2 ) {
                decision.intent = bandit_dry_run::job_template::raid;
                decision.selected_member_count = attackers;
                decision.notes.push_back(
                    "raid: scout-confirmed basecamp lead lets cannibal camp commit an attack pack instead of another scout" );
                return decision;
            }
            select_routine_party(
                "scout pair: cannibal camp lacks an at-home attack pack after reserve, so it does not dogpile" );
            return decision;
        }

        if( profile == hostile_site_profile::camp_style && scout_confirmed_basecamp ) {
            const int toll_members = toll_pressure_member_count( decision.living_roster,
                                     decision.dispatchable );
            if( toll_members >= 2 ) {
                decision.intent = bandit_dry_run::job_template::toll;
                decision.selected_member_count = toll_members;
                decision.notes.push_back(
                    "toll: scout-confirmed basecamp lead promotes to a shakedown-capable party" );
                return decision;
            }
        }

        const int stalkers = stalk_pressure_member_count( decision.living_roster, decision.dispatchable );
        if( stalkers >= 2 ) {
            decision.intent = bandit_dry_run::job_template::stalk;
            decision.selected_member_count = stalkers;
            decision.notes.push_back( "stalk: remembered high-value lead permits larger-than-scout pressure" );
            return decision;
        }
        select_routine_party( "scout party: pressure margin is good but reserve leaves no stalk response" );
        return decision;
    }

    decision.intent = bandit_dry_run::job_template::hold_chill;
    decision.notes.push_back( "hold: marginal remembered lead waits for better evidence" );
    return decision;
}

response_party_selection_result select_fresh_response_party( const site_record &site,
        const hostile_operation_kind operation_kind )
{
    response_party_selection_result selection;
    selection.job = operation_kind == hostile_operation_kind::raid ?
                    bandit_dry_run::job_template::raid : bandit_dry_run::job_template::toll;
    const camp_report_policy expected_policy = report_policy_for_profile(
                effective_profile( site ) );
    if( site.retired_empty_site || site.has_active_outside_pressure() ||
        site.camp_decision.state != camp_decision_state::preparing_follow_on ||
        site.camp_decision.report_policy != expected_policy ||
        operation_kind != operation_kind_for_report_policy( expected_policy ) ||
        !report_matches_camp_decision( site.current_scout_report, site.camp_decision ) ) {
        selection.rejection_reason = "post-report response decision is not ready";
        return selection;
    }

    int requested_party_size = 0;
    if( !site.camp_decision.target_lead_id.empty() ) {
        const camp_map_lead *lead = site.intelligence_map.find_lead(
                                        site.camp_decision.target_lead_id );
        const std::string lead_target_id = lead == nullptr ? "" :
                                           lead->target_id.empty() ? lead->lead_id : lead->target_id;
        if( lead == nullptr || lead->revision != site.camp_decision.target_lead_revision ||
            lead_target_id != site.camp_decision.target_id ||
            lead->omt != site.camp_decision.target_omt ) {
            selection.rejection_reason = "pinned response lead is missing or stale";
            return selection;
        }
        const camp_map_dispatch_decision decision = choose_camp_map_dispatch( site, *lead );
        if( decision.intent != selection.job || decision.selected_member_count <= 0 ) {
            selection.rejection_reason = "current pinned threat/reward state no longer supports the response";
            return selection;
        }
        requested_party_size = decision.selected_member_count;
        selection.threat_derived = true;
    }

    const response_party_policy_result policy = response_party_policy(
                site, selection.job, requested_party_size );
    selection.party_size = policy.party_size;
    selection.required_local_reserve = policy.required_local_reserve;
    if( !policy.eligible ) {
        selection.rejection_reason = policy.rejection_reason;
        return selection;
    }

    selection.member_ids = select_dispatch_members( site, policy.party_size );
    if( static_cast<int>( selection.member_ids.size() ) != policy.party_size ) {
        selection.member_ids.clear();
        selection.rejection_reason = "fresh response selection could not fill the policy size";
        return selection;
    }

    selection.eligible = true;
    return selection;
}

const camp_map_lead *find_camp_map_dispatch_lead_for_target( const site_record &site,
        const tripoint_abs_omt &target_omt,
        const std::string &target_id )
{
    const camp_map_lead *best_lead = nullptr;
    int best_distance = 0;
    int best_score = 0;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        if( lead.kind == camp_lead_kind::structural_bounty ||
            lead.kind == camp_lead_kind::frontier_probe ||
            lead.kind == camp_lead_kind::terrain_opportunity ||
            returned_structural_signal_lead( lead ) ) {
            continue;
        }
        if( lead.target_id.empty() && lead.lead_id.empty() ) {
            continue;
        }
        if( lead.status == camp_lead_status::invalidated ||
            lead.status == camp_lead_status::harvested ||
            lead.status == camp_lead_status::dangerous ) {
            continue;
        }

        const bool target_matches = !target_id.empty() &&
                                    ( lead.target_id == target_id || lead.lead_id == target_id );
        const int radius = std::max( 2, lead.radius_omt );
        const tripoint_abs_omt routed_target_omt = reachable_ground_dispatch_target( site, target_omt );
        const tripoint_abs_omt routed_lead_omt = reachable_ground_dispatch_target( site, lead.omt );
        const bool direct_omt_matches = lead.omt.z() == target_omt.z() &&
                                        rl_dist( lead.omt, target_omt ) <= radius;
        const bool routed_omt_matches = routed_lead_omt.z() == routed_target_omt.z() &&
                                        rl_dist( routed_lead_omt, routed_target_omt ) <= radius;
        const bool omt_matches = direct_omt_matches || routed_omt_matches;
        if( !target_matches && !omt_matches ) {
            continue;
        }

        const int distance = rl_dist( site.anchor, lead.omt );
        const int score = std::max( 0, lead.confidence ) + std::max( 0, lead.bounty ) -
                          std::max( 0, lead.threat );
        if( best_lead == nullptr || distance < best_distance ||
            ( distance == best_distance && score > best_score ) ||
            ( distance == best_distance && score == best_score &&
              lead.lead_id < best_lead->lead_id ) ) {
            best_lead = &lead;
            best_distance = distance;
            best_score = score;
        }
    }
    return best_lead;
}

structural_bounty_read classify_structural_bounty_terrain( const std::string &overmap_terrain_id )
{
    const std::string id = lowercase_copy( overmap_terrain_id );
    structural_bounty_read read;
    read.terrain_class = "open";
    read.terrain_fit_class = "unknown";
    read.summary = "no structural bounty";

    if( id.empty() ) {
        return read;
    }

    if( contains_any_token( id, { "impassable" } ) ) {
        read.terrain_fit_class = "impassable";
        return read;
    }

    if( matches_terrain_id_family( id, { "road", "highway", "bridge", "bridgehead" } ) ) {
        read.terrain_fit_class = "road";
        return read;
    }

    if( id == "field" || matches_terrain_id_family( id, { "meadow" } ) ) {
        read.terrain_fit_class = "field";
        return read;
    }

    if( contains_any_token( id, { "swamp", "wetland", "forest_water" } ) ) {
        read.terrain_class = "forest";
        read.terrain_fit_class = "swamp";
        read.bounty = 1;
        read.confidence = 1;
        read.latent_threat = 0;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "low structural swamp/wetland bounty";
        return read;
    }

    if( contains_any_token( id, { "forest_edge", "woods_edge", "forest_trail", "trailhead" } ) ) {
        read.terrain_class = "forest";
        read.terrain_fit_class = "forest_edge";
        read.bounty = 1;
        read.confidence = 1;
        read.latent_threat = 0;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "low structural forest-edge bounty";
        return read;
    }

    if( contains_any_token( id, { "forest", "woods", "wood" } ) ) {
        read.terrain_class = "forest";
        read.terrain_fit_class = "deep_forest";
        read.bounty = 1;
        read.confidence = 1;
        read.latent_threat = 0;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "low structural forest/woods bounty";
        return read;
    }

    if( contains_any_token( id, { "shelter", "cabin" } ) ) {
        read.terrain_class = "town";
        read.terrain_fit_class = "shelter";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural shelter bounty";
        return read;
    }

    if( contains_any_token( id, { "rural", "farm" } ) ) {
        read.terrain_class = "town";
        read.terrain_fit_class = "rural";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural rural bounty";
        return read;
    }

    if( contains_any_token( id, { "downtown", "city_center", "city_centre" } ) ) {
        read.terrain_class = "town";
        read.terrain_fit_class = "dense_urban";
        read.bounty = 3;
        read.confidence = 1;
        read.latent_threat = 2;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium-high structural urban bounty with latent threat";
        return read;
    }

    if( contains_any_token( id, { "town_edge", "town_outskirt", "outskirts" } ) ) {
        read.terrain_class = "town";
        read.terrain_fit_class = "town_edge";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural town-edge bounty";
        return read;
    }

    if( contains_any_token( id, { "house", "home", "building", "shop", "store", "garage",
                                  "mall", "office", "apartment" } ) ) {
        read.terrain_class = "town";
        read.terrain_fit_class = "building";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural town/building bounty";
        return read;
    }

    if( contains_any_token( id, { "town" } ) ) {
        read.terrain_class = "town";
        read.terrain_fit_class = "unknown";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural town bounty with unknown exact terrain fit";
        return read;
    }

    return read;
}

int hostile_camp_terrain_fit( const hostile_site_profile profile,
                              const std::string &terrain_fit_class )
{
    const std::string terrain = lowercase_copy( terrain_fit_class );
    if( terrain == "impassable" || profile == hostile_site_profile::none ||
        profile == hostile_site_profile::small_hostile_site ) {
        return 0;
    }
    if( terrain.empty() || terrain == "unknown" ) {
        return 333;
    }

    if( profile == hostile_site_profile::camp_style ) {
        if( terrain == "road" || terrain == "shelter" || terrain == "building" ||
            terrain == "town_edge" || terrain == "dense_urban" ) {
            return 1000;
        }
        if( terrain == "field" || terrain == "forest_edge" || terrain == "rural" ) {
            return 500;
        }
        if( terrain == "deep_forest" || terrain == "swamp" ) {
            return 250;
        }
    }

    if( profile == hostile_site_profile::cannibal_camp ) {
        if( terrain == "forest_edge" || terrain == "rural" || terrain == "shelter" ) {
            return 1000;
        }
        if( terrain == "deep_forest" || terrain == "swamp" || terrain == "field" ||
            terrain == "town_edge" || terrain == "dense_urban" ) {
            return 500;
        }
        if( terrain == "road" || terrain == "building" ) {
            return 250;
        }
    }
    return 333;
}

int structural_terrain_static_risk( const std::string &terrain_fit_class )
{
    const std::string terrain = lowercase_copy( terrain_fit_class );
    if( terrain == "impassable" ) {
        return 1000;
    }
    if( terrain == "swamp" || terrain == "dense_urban" ) {
        return 600;
    }
    if( terrain == "deep_forest" || terrain == "forest_edge" || terrain == "building" ||
        terrain == "town_edge" || terrain == "shelter" ) {
        return 400;
    }
    if( terrain == "road" || terrain == "field" ) {
        return 200;
    }
    return 300;
}

int normalize_ground_bounty_opportunity( const int bounty_units )
{
    switch( std::clamp( bounty_units, 0, max_finite_resource_units ) ) {
        case 0:
            return 0;
        case 1:
            return 333;
        case 2:
            return 667;
        default:
            return 1000;
    }
}

int hostile_camp_dispatch_drive( const int need, const int knowledge_gap,
                                 const int best_cheap_target, const int cadence )
{
    const long long weighted = 350LL * std::clamp( need, 0, 1000 ) +
                               250LL * std::clamp( knowledge_gap, 0, 1000 ) +
                               200LL * std::clamp( best_cheap_target, 0, 1000 ) +
                               200LL * std::clamp( cadence, 0, 1000 );
    return std::clamp( static_cast<int>( weighted / 1000 ), 0, 1000 );
}

bool hostile_camp_routine_score_eligible( const int score, const bool retained_target )
{
    return score >= ( retained_target ? routine_retain_score : routine_acquire_score );
}

bool hostile_camp_routine_risk_blocked( const int risk )
{
    return risk >= routine_hard_risk;
}

bool hostile_camp_routine_route_risk_eligible( const int risk, const int max_segment_risk )
{
    if( risk < 0 || risk > 1000 || max_segment_risk < 0 || max_segment_risk > 1000 ||
        hostile_camp_routine_risk_blocked( risk ) ) {
        return false;
    }
    return risk < 500 || max_segment_risk < 500;
}

routine_dispatch_evaluation evaluate_hostile_camp_routine_dispatch(
    const site_record &site, const int now_minutes, const int best_cheap_target )
{
    routine_dispatch_evaluation evaluation;
    evaluation.best_cheap_target = std::clamp( best_cheap_target, 0, 1000 );
    if( now_minutes < 0 ) {
        return evaluation;
    }

    const int living_total = std::max( 1, camp_supply_living_total( site ) );
    const int bounded_supply = std::max( 0, site.supply_units );
    if( bounded_supply >= 7 * living_total ) {
        evaluation.need = 0;
    } else if( bounded_supply >= 3 * living_total ) {
        evaluation.need = 333;
    } else if( bounded_supply >= living_total ) {
        evaluation.need = 667;
    } else {
        evaluation.need = 1000;
    }

    int stale_frontier_sectors = frontier_sector_count;
    if( frontier_memory_is_valid( site.intelligence_map ) ) {
        stale_frontier_sectors = 0;
        constexpr int frontier_knowledge_horizon_minutes = 7 * 24 * 60;
        for( const int resolved_minutes : site.intelligence_map.frontier_last_resolved_minutes ) {
            if( resolved_minutes < 0 || resolved_minutes > now_minutes ||
                now_minutes - resolved_minutes > frontier_knowledge_horizon_minutes ) {
                stale_frontier_sectors++;
            }
        }
    }
    evaluation.knowledge_gap = stale_frontier_sectors * 1000 / frontier_sector_count;

    if( site.last_routine_resolved_minutes >= 0 &&
        static_cast<long long>( now_minutes ) >
        static_cast<long long>( site.last_routine_resolved_minutes ) + 24 * 60 ) {
        const long long rising_minutes = static_cast<long long>( now_minutes ) -
                                         site.last_routine_resolved_minutes - 24 * 60;
        evaluation.cadence = std::clamp( static_cast<int>(
                std::min<long long>( 1000, rising_minutes * 1000 / ( 48 * 60 ) ) ), 0, 1000 );
    }

    if( site.routine_activated_minutes >= 0 && now_minutes >= site.routine_activated_minutes ) {
        if( site.last_routine_resolved_minutes < 0 ) {
            evaluation.force_due = now_minutes >= minutes_after_saturated(
                                       site.routine_activated_minutes,
                                       first_frontier_due_delay_minutes( site.site_id ) );
        } else {
            evaluation.force_due = now_minutes >= minutes_after_saturated(
                                       site.last_routine_resolved_minutes, 72 * 60 );
        }
    }
    evaluation.drive = hostile_camp_dispatch_drive(
                           evaluation.need, evaluation.knowledge_gap,
                           evaluation.best_cheap_target, evaluation.cadence );
    return evaluation;
}

std::string make_structural_bounty_lead_id( const std::string &site_id,
        const tripoint_abs_omt &omt, const std::string &terrain_class )
{
    std::ostringstream out;
    out << site_id << ":structural_bounty:" << omt.x() << ',' << omt.y() << ',' << omt.z()
        << ':' << ( terrain_class.empty() ? "unknown" : terrain_class );
    return out.str();
}

std::string make_terrain_opportunity_lead_id( const std::string &site_id,
        const tripoint_abs_omt &omt, const std::string &terrain_fit_class )
{
    std::ostringstream out;
    out << site_id << ":terrain_opportunity:" << omt.x() << ',' << omt.y() << ',' << omt.z()
        << ':' << terrain_fit_class;
    return out.str();
}

bool structural_bounty_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class )
{
    const std::string terrain = terrain_class.empty() ? "unknown" : terrain_class;
    for( const camp_map_lead &lead : intelligence_map.leads ) {
        if( lead.omt != omt ) {
            continue;
        }
        if( lead.kind != camp_lead_kind::structural_bounty &&
            lead.kind != camp_lead_kind::harvested_site ) {
            continue;
        }
        if( !lead.target_id.empty() && lead.target_id != terrain ) {
            continue;
        }
        if( lead.status == camp_lead_status::harvested ||
            lead.status == camp_lead_status::dangerous ) {
            return true;
        }
    }
    return false;
}

static bool structural_bounty_scan_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class, const int now_minutes )
{
    if( structural_bounty_memory_suppresses_refresh( intelligence_map, omt, terrain_class ) ) {
        return true;
    }

    constexpr int recent_structural_check_cooldown_minutes = 6 * 60;
    const std::string terrain = terrain_class.empty() ? "unknown" : terrain_class;
    for( const camp_map_lead &lead : intelligence_map.leads ) {
        if( lead.omt != omt || lead.kind != camp_lead_kind::structural_bounty ) {
            continue;
        }
        if( !lead.target_id.empty() && lead.target_id != terrain ) {
            continue;
        }
        if( lead.last_checked_minutes >= 0 && now_minutes >= 0 &&
            now_minutes - lead.last_checked_minutes < recent_structural_check_cooldown_minutes ) {
            return true;
        }
    }
    return false;
}

bool upsert_structural_bounty_lead( site_record &site, const tripoint_abs_omt &omt,
                                   const structural_bounty_read &read, const int now_minutes )
{
    if( !read.eligible || read.bounty <= 0 ) {
        return false;
    }
    if( structural_bounty_memory_suppresses_refresh( site.intelligence_map, omt,
            read.terrain_class ) ) {
        return false;
    }

    camp_map_lead lead;
    lead.lead_id = make_structural_bounty_lead_id( site.site_id, omt, read.terrain_class );
    lead.kind = camp_lead_kind::structural_bounty;
    lead.origin = camp_lead_origin::structural_routine;
    lead.status = camp_lead_status::suspected;
    lead.target_id = read.terrain_class;
    lead.omt = omt;
    lead.radius_omt = read.radius_omt;
    lead.source_key = "structural_bounty:" + read.terrain_class + ":terrain_fit:" +
                      ( read.terrain_fit_class.empty() ? "unknown" : read.terrain_fit_class );
    lead.source_summary = read.summary;
    lead.first_seen_minutes = now_minutes;
    lead.last_seen_minutes = now_minutes;
    lead.bounty = std::max( 0, read.bounty );
    lead.threat = 0;
    lead.confidence = std::max( 0, read.confidence );
    lead.threat_confirmed = false;
    lead.last_outcome = "structural_bounty_suspected";

    if( camp_map_lead *existing = site.intelligence_map.find_lead( lead.lead_id ) ) {
        lead.first_seen_minutes = existing->first_seen_minutes;
        lead.times_checked_empty = existing->times_checked_empty;
        lead.times_harvested = existing->times_harvested;
    }
    return upsert_camp_map_lead( site, std::move( lead ) );
}

bool upsert_terrain_opportunity_lead( site_record &site, const tripoint_abs_omt &omt,
                                      const structural_bounty_read &read, const int now_minutes )
{
    if( read.terrain_fit_class != "road" && read.terrain_fit_class != "field" ) {
        return false;
    }

    camp_map_lead lead;
    lead.lead_id = make_terrain_opportunity_lead_id( site.site_id, omt,
                   read.terrain_fit_class );
    if( site.intelligence_map.find_lead( lead.lead_id ) != nullptr ) {
        return false;
    }
    lead.kind = camp_lead_kind::terrain_opportunity;
    lead.origin = camp_lead_origin::structural_routine;
    lead.status = camp_lead_status::suspected;
    lead.target_id = read.terrain_fit_class;
    lead.omt = omt;
    lead.source_key = "terrain_opportunity:terrain_fit:" + read.terrain_fit_class;
    lead.source_summary = "static " + read.terrain_fit_class +
                          " terrain worth an honest paired scout check";
    lead.first_seen_minutes = now_minutes;
    lead.last_seen_minutes = now_minutes;
    lead.bounty = 0;
    lead.threat = 0;
    lead.confidence = 0;
    lead.threat_confirmed = false;
    lead.last_outcome = "terrain_opportunity_suspected";
    return upsert_camp_map_lead( site, std::move( lead ) );
}

bool record_camp_resource_estimate( site_record &site, const std::string &lead_id,
                                    const int estimated_units, const int confidence,
                                    const int observed_minutes )
{
    if( lead_id.empty() || estimated_units < 0 || estimated_units > max_finite_resource_units ||
        confidence < 0 || confidence > max_finite_resource_units || observed_minutes < 0 ) {
        return false;
    }
    camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    if( lead == nullptr || lead->kind != camp_lead_kind::structural_bounty ||
        lead->revision >= std::numeric_limits<int>::max() ||
        observed_minutes <= lead->last_checked_minutes ||
        ( lead->status == camp_lead_status::harvested && estimated_units > 0 ) ) {
        return false;
    }

    lead->bounty = estimated_units;
    lead->confidence = confidence;
    lead->last_seen_minutes = std::max( lead->last_seen_minutes, observed_minutes );
    lead->last_checked_minutes = observed_minutes;
    lead->last_scouted_minutes = std::max( lead->last_scouted_minutes, observed_minutes );
    if( estimated_units == 0 ) {
        lead->status = camp_lead_status::harvested;
        lead->last_outcome = "physical_resource_depletion_observed";
    } else {
        lead->last_outcome = "physical_resource_estimate_updated";
    }
    return advance_camp_map_lead_revision( site, *lead );
}

static void sample_structural_bounty_site( site_record &site,
        structural_bounty_scan_result &result, const int now_minutes, const int sample_cap,
        const bool allow_active_outside,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup )
{
    static const std::array<std::pair<int, int>, 12> near_offsets = { {
            { -4, 0 }, { 4, 0 }, { 0, -4 }, { 0, 4 },
            { -5, -1 }, { 5, 1 }, { -1, 5 }, { 1, -5 },
            { -6, 0 }, { 6, 0 }, { 0, -6 }, { 0, 6 },
        } };
    constexpr int near_scan_cadence_minutes = 60;
    constexpr int near_scan_radius_omt = 8;
    result.sites_considered++;
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::structural_scan_sites_considered );
    bandit_live_world_probe::record_site_service( site.site_id,
            bandit_live_world_probe::site_service::scan_considered );
    if( !supports_routine_camp_ecology( effective_profile( site ) ) ) {
        result.sites_skipped_not_camp++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_scan_sites_skipped_not_camp );
        return;
    }
    if( site.retired_empty_site ) {
        result.sites_skipped_retired++;
        return;
    }
    if( site.has_active_outside_pressure() && !allow_active_outside ) {
        result.sites_skipped_active_outside++;
        return;
    }
    const bool has_ready_home_presence = ready_at_home_member_count( site ) > 0 ||
                                         site.count_home_side_signals() > 0;
    if( !has_ready_home_presence ) {
        result.sites_skipped_no_ready_home++;
        return;
    }
    if( site.intelligence_map.next_near_tick_minutes >= 0 &&
        now_minutes >= 0 && now_minutes < site.intelligence_map.next_near_tick_minutes ) {
        result.sites_deferred_by_cadence++;
        return;
    }

    int samples_for_site = 0;
    while( samples_for_site < sample_cap && result.budget_used < result.scan_budget ) {
        const int offset_index = std::clamp( site.intelligence_map.terrain_scan_cursor, 0, 11 );
        const std::pair<int, int> &offset = near_offsets[static_cast<std::size_t>( offset_index )];
        site.intelligence_map.terrain_scan_cursor = ( offset_index + 1 ) %
                                                    static_cast<int>( near_offsets.size() );
        const tripoint_abs_omt candidate( site.anchor.x() + offset.first,
                                          site.anchor.y() + offset.second, site.anchor.z() );
        result.candidates_sampled++;
        result.budget_used++;
        samples_for_site++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_scan_candidates_sampled );
        bandit_live_world_probe::record_site_service( site.site_id,
                bandit_live_world_probe::site_service::scan_samples );

        const std::optional<std::string> terrain_id = terrain_lookup( candidate );
        if( !terrain_id ) {
            continue;
        }
        const structural_bounty_read read = classify_structural_bounty_terrain( *terrain_id );
        if( !read.eligible || read.bounty <= 0 ) {
            if( upsert_terrain_opportunity_lead( site, candidate, read, now_minutes ) ) {
                result.leads_seeded++;
            }
            continue;
        }
        if( structural_bounty_scan_memory_suppresses_refresh( site.intelligence_map, candidate,
                read.terrain_class, now_minutes ) ) {
            result.leads_suppressed_by_memory++;
            continue;
        }
        if( upsert_structural_bounty_lead( site, candidate, read, now_minutes ) ) {
            result.leads_seeded++;
        }
    }

    if( samples_for_site > 0 ) {
        site.intelligence_map.known_radius_omt = std::max( site.intelligence_map.known_radius_omt,
                near_scan_radius_omt );
        if( now_minutes >= 0 ) {
            site.intelligence_map.next_near_tick_minutes = now_minutes + near_scan_cadence_minutes;
        }
    }
}

structural_bounty_scan_result advance_structural_bounty_scan( world_state &state,
        const int now_minutes, const int scan_budget,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_scan );
    structural_bounty_scan_result result;
    result.scan_budget = std::max( 0, scan_budget );
    if( result.scan_budget == 0 ) {
        result.notes.push_back( "structural scan skipped: zero budget" );
        return result;
    }
    if( !terrain_lookup ) {
        result.notes.push_back( "structural scan skipped: no terrain lookup" );
        return result;
    }

    constexpr int per_site_near_sample_cap = 4;
    for( site_record &site : state.sites ) {
        if( result.budget_used >= result.scan_budget ) {
            result.budget_exhausted = true;
            break;
        }
        sample_structural_bounty_site( site, result, now_minutes, per_site_near_sample_cap, false,
                                       terrain_lookup );
    }
    result.budget_exhausted = result.budget_used >= result.scan_budget;

    result.notes.push_back( "structural scan bounded to near-ring per-camp samples" );
    return result;
}

namespace
{
int structural_outing_stalking_delay_minutes( const site_record &site, const camp_map_lead &lead )
{
    return structural_stalking_delay_minutes( site.anchor, lead.omt );
}

int structural_outing_arrival_delay_minutes( const site_record &site, const camp_map_lead &lead )
{
    return structural_arrival_delay_minutes( site.anchor, lead.omt );
}

int active_structural_outing_stalking_delay_minutes( const site_record &site )
{
    return structural_stalking_delay_minutes(
               site.anchor, structural_outing_travel_destination( site.active_outing ) );
}

int active_structural_outing_arrival_delay_minutes( const site_record &site )
{
    return structural_arrival_delay_minutes(
               site.anchor, structural_outing_travel_destination( site.active_outing ) );
}

int structural_known_threat_for_interest( const camp_map_lead &lead )
{
    return lead.threat_confirmed ? std::max( 0, lead.threat ) : 0;
}

int structural_effective_interest( const camp_map_lead &lead, const int threat )
{
    if( lead.kind == camp_lead_kind::terrain_opportunity ) {
        return std::max( 0, 1 - std::max( 0, threat ) );
    }
    return std::max( 0, lead.bounty ) + std::max( 0, lead.confidence ) - std::max( 0, threat );
}

std::string structural_terrain_fit_class( const camp_map_lead &lead )
{
    static const std::string marker = ":terrain_fit:";
    const std::size_t marker_at = lead.source_key.find( marker );
    if( marker_at == std::string::npos ) {
        return "unknown";
    }
    const std::string terrain = lead.source_key.substr( marker_at + marker.size() );
    return terrain.empty() ? "unknown" : terrain;
}

int structural_candidate_route_quality( const int route_cost )
{
    if( route_cost < 0 || route_cost > max_structural_route_cost_omt ) {
        return 0;
    }
    return std::clamp( 1000 - route_cost * 1000 / max_structural_route_cost_omt, 0, 1000 );
}

int structural_candidate_novelty( const camp_map_lead &lead, const int now_minutes )
{
    const int last_visit = std::max( lead.last_checked_minutes, lead.last_scouted_minutes );
    if( last_visit < 0 ) {
        return 1000;
    }
    constexpr int novelty_horizon_minutes = 7 * 24 * 60;
    const long long age = now_minutes >= last_visit ?
                          static_cast<long long>( now_minutes ) - last_visit : 0;
    return std::clamp( static_cast<int>( std::min<long long>( 1000,
                       age * 1000 / novelty_horizon_minutes ) ), 0, 1000 );
}

int structural_estimate_freshness( const camp_map_lead &lead, const int now_minutes )
{
    if( lead.last_checked_minutes < 0 ) {
        return 0;
    }
    constexpr int estimate_freshness_horizon_minutes = 14 * 24 * 60;
    const long long age = now_minutes >= lead.last_checked_minutes ?
                          static_cast<long long>( now_minutes ) - lead.last_checked_minutes : 0;
    if( age >= estimate_freshness_horizon_minutes ) {
        return 0;
    }
    return std::clamp( 1000 - static_cast<int>( age * 1000 /
                       estimate_freshness_horizon_minutes ), 0, 1000 );
}

int structural_repetition_penalty( const site_record &site, const camp_map_lead &lead )
{
    if( !lead.lead_id.empty() &&
        lead.lead_id == site.intelligence_map.last_routine_target_lead_id ) {
        return 1000;
    }
    if( !lead.lead_id.empty() &&
        lead.lead_id == site.intelligence_map.previous_routine_target_lead_id ) {
        return 500;
    }
    return 0;
}

int structural_signal_strength( const camp_map_lead &lead, const int now_minutes )
{
    if( !returned_structural_signal_lead( lead ) || lead.last_seen_minutes < 0 ||
        now_minutes < 0 ) {
        return 0;
    }
    const int horizon_minutes = lead.kind == camp_lead_kind::sound_signal ? 3 * 60 : 6 * 60;
    const int age_minutes = std::max( 0, now_minutes - lead.last_seen_minutes );
    if( age_minutes >= horizon_minutes ) {
        return 0;
    }
    const int freshness = ( horizon_minutes - age_minutes ) * 1000 / horizon_minutes;
    const int base_strength = lead.kind == camp_lead_kind::sound_signal ? 600 : 700;
    return base_strength * freshness / 1000;
}

int structural_candidate_score( const site_record &site, const camp_map_lead &lead,
                                const int now_minutes, const int route_quality,
                                const int terrain_fit, const int static_risk )
{
    const bool physically_checked = lead.last_checked_minutes >= 0;
    const int confidence = physically_checked ? std::clamp( lead.confidence * 1000 / 3, 0, 1000 ) : 0;
    const int freshness = structural_estimate_freshness( lead, now_minutes );
    const int weighted_confidence = confidence * freshness / 1000;
    const int remembered_estimate = normalize_ground_bounty_opportunity( lead.bounty );
    const int neutral_prior = 333;
    const int reward = ( weighted_confidence * remembered_estimate +
                         ( 1000 - weighted_confidence ) * neutral_prior ) / 1000;
    const int information_value = 1000 - weighted_confidence;
    const int signal = structural_signal_strength( lead, now_minutes );
    const int novelty = structural_candidate_novelty( lead, now_minutes );
    const int repetition_penalty = structural_repetition_penalty( site, lead );
    const long long weighted_score = 300LL * reward + 250LL * information_value +
                                     150LL * signal + 100LL * novelty +
                                     100LL * std::clamp( route_quality, 0, 1000 ) +
                                     100LL * std::clamp( terrain_fit, 0, 1000 ) -
                                     450LL * std::clamp( static_risk, 0, 1000 ) -
                                     150LL * repetition_penalty;
    return std::clamp( static_cast<int>( weighted_score / 1000 ), 0, 1000 );
}

bool structural_lead_recently_checked( const camp_map_lead &lead, const int now_minutes )
{
    constexpr int recent_structural_check_cooldown_minutes = 6 * 60;
    return lead.last_checked_minutes >= 0 && now_minutes >= 0 &&
           now_minutes - lead.last_checked_minutes < recent_structural_check_cooldown_minutes;
}

} // namespace

std::optional<int> release_matching_external_reservation( site_record &site,
        const std::string &expected_activity_id, const int expected_generation,
        const std::string &summary )
{
    const active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr || outing->activity_id != expected_activity_id ||
        outing->generation != expected_generation || !site.roster().valid ) {
        return std::nullopt;
    }
    const bool hostile_operation = outing->kind == outing_kind::hostile_operation;

    site_record candidate = site;
    active_outing_state *candidate_outing = candidate.active_external_outing();
    if( candidate_outing == nullptr ||
        candidate_outing->activity_id != expected_activity_id ||
        candidate_outing->generation != expected_generation ) {
        return std::nullopt;
    }
    int released_members = 0;
    for( const character_id &member_id : candidate_outing->member_ids ) {
        if( candidate_outing->member_is_resolved( member_id ) ) {
            continue;
        }
        const member_record *member = candidate.find_member( member_id );
        if( member == nullptr ||
            ( member->state != member_state::at_home &&
              member->state != member_state::outbound &&
              member->state != member_state::local_contact ) ) {
            return std::nullopt;
        }
        if( member->state != member_state::at_home &&
            !update_member_state( candidate, member_id, member_state::at_home, summary ) ) {
            return std::nullopt;
        }
        released_members++;
    }
    candidate.applied_return_generation = std::max(
            candidate.applied_return_generation, expected_generation );
    if( hostile_operation ) {
        candidate.active_hostile_operation.clear();
        if( candidate.camp_decision.state == camp_decision_state::preparing_follow_on ) {
            candidate.camp_decision.state = camp_decision_state::abandoned;
            candidate.camp_decision.next_eligible_minutes = -1;
            candidate.camp_decision.transition_reason = summary.substr(
                    0, max_camp_decision_reason_length );
        }
    } else {
        const bool matching_provisional_report = candidate.current_scout_report.provisional &&
                candidate.current_scout_report.source_activity_id == expected_activity_id &&
                candidate.current_scout_report.source_generation == expected_generation;
        candidate.active_outing.clear();
        if( matching_provisional_report ) {
            candidate.current_scout_report.clear();
        }
    }
    if( !candidate.roster().valid ) {
        return std::nullopt;
    }
    site = std::move( candidate );
    return released_members;
}

bool invalidate_site_origin( site_record &site, const origin_disposition disposition,
                             const int current_minutes, const std::string &summary )
{
    if( !terminal_origin_disposition( disposition ) ||
        terminal_origin_disposition( site.origin ) || current_minutes < 0 || summary.empty() ||
        ( site.supply_last_update_minutes >= 0 &&
          current_minutes < site.supply_last_update_minutes ) ) {
        return false;
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    candidate.origin = disposition;
    candidate.origin_changed_minutes = current_minutes;
    candidate.origin_summary = summary.substr( 0, max_sortie_summary_length );
    candidate.retired_empty_site = true;
    candidate.retirement_summary = candidate.origin_summary;
    candidate.supply_units = 0;
    candidate.supply_last_update_minutes = current_minutes;
    candidate.supply_accounted_living_total = 0;
    candidate.supply_member_minute_remainder = 0;
    if( !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

bool request_origin_recall( site_record &site,
                            const simulation_advance_cursor &expected_cursor,
                            const bool physical_signal, const int current_minutes,
                            const std::string &summary )
{
    const active_outing_state *outing = site.active_external_outing();
    if( !physical_signal || summary.empty() || outing == nullptr ||
        !simulation_cursor_matches( *outing, expected_cursor ) || current_minutes < 0 ||
        current_minutes <= outing->last_advanced_minutes ||
        outing->phase == scout_phase::returning_home || outing->phase == scout_phase::lost ||
        !site.roster().valid ||
        ( outing->owner == simulation_owner::local &&
          outing->handoff_epoch == std::numeric_limits<int>::max() ) ) {
        return false;
    }

    site_record candidate = site;
    std::optional<scout_phase> recalled_scout_previous_phase;
    if( outing->kind == outing_kind::hostile_operation ) {
        const hostile_operation_phase previous_phase =
            candidate.active_hostile_operation.phase;
        if( transition_hostile_operation_phase(
                candidate, expected_cursor, previous_phase,
                hostile_operation_phase::returning_home, current_minutes, summary ) !=
            hostile_operation_transition_result::applied ) {
            return false;
        }
    } else if( outing->kind == outing_kind::scout_sortie ) {
        const simulation_owner previous_owner = candidate.active_outing.owner;
        recalled_scout_previous_phase = candidate.active_outing.phase;
        if( transition_active_scout_phase_impl(
                candidate, expected_cursor, candidate.active_outing.phase,
                scout_phase::returning_home, current_minutes, summary, false ) !=
            scout_phase_transition_result::applied ) {
            return false;
        }
        if( previous_owner == simulation_owner::local ) {
            candidate.active_outing.owner = simulation_owner::abstract;
            candidate.active_outing.handoff_epoch++;
            for( const character_id &member_id : candidate.active_outing.member_ids ) {
                member_record *member = candidate.find_member( member_id );
                if( member != nullptr && member->state == member_state::local_contact &&
                    !candidate.active_outing.member_is_resolved( member_id ) ) {
                    member->state = member_state::outbound;
                    member->last_writeback_summary = summary;
                }
            }
        }
    } else if( outing->kind == outing_kind::structural_sortie ) {
        active_outing_state &structural = candidate.active_outing;
        if( structural.owner == simulation_owner::local ) {
            if( structural.handoff_epoch == std::numeric_limits<int>::max() ) {
                return false;
            }
            structural.owner = simulation_owner::abstract;
            structural.handoff_epoch++;
        }
        for( const character_id &member_id : structural.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member != nullptr && member->state == member_state::local_contact &&
                !structural.member_is_resolved( member_id ) ) {
                member->state = member_state::outbound;
                member->last_writeback_summary = summary;
            }
        }
        structural.phase = scout_phase::returning_home;
        structural.last_progress_minutes = current_minutes;
        structural.last_advanced_minutes = current_minutes;
    } else {
        return false;
    }

    if( !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    if( recalled_scout_previous_phase ) {
        record_scout_phase_transition_event(
            site.active_outing, *recalled_scout_previous_phase,
            scout_phase::returning_home, summary, current_minutes );
    }
    return true;
}

origin_loss_resolution_effect resolve_origin_loss_return( site_record &site,
        const std::string &expected_activity_id, const int expected_generation,
        const std::vector<active_member_observation> &observations,
        const int current_minutes, const std::string &summary )
{
    origin_loss_resolution_effect effect;
    const active_outing_state *outing = site.active_external_outing();
    if( !terminal_origin_disposition( site.origin ) || !site.retired_empty_site ||
        outing == nullptr || outing->activity_id != expected_activity_id ||
        outing->generation != expected_generation || current_minutes < 0 ||
        current_minutes <= outing->last_advanced_minutes || summary.empty() ||
        observations.size() != outing->member_ids.size() || !site.roster().valid ) {
        return effect;
    }

    std::vector<character_id> observed_ids;
    observed_ids.reserve( observations.size() );
    for( const active_member_observation &observation : observations ) {
        if( std::find( outing->member_ids.begin(), outing->member_ids.end(), observation.npc_id ) ==
            outing->member_ids.end() ||
            std::find( observed_ids.begin(), observed_ids.end(), observation.npc_id ) !=
            observed_ids.end() ||
            ( observation.state != active_member_observation_state::home &&
              observation.state != active_member_observation_state::dead &&
              observation.state != active_member_observation_state::missing ) ||
            ( observation.state == active_member_observation_state::missing &&
              ( outing->missing_deadline_minutes < 0 ||
                current_minutes < outing->missing_deadline_minutes ) ) ) {
            return effect;
        }
        observed_ids.push_back( observation.npc_id );
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    active_outing_state *candidate_outing = candidate.active_external_outing();
    if( candidate_outing == nullptr ) {
        return effect;
    }
    for( const character_id &member_id : candidate_outing->member_ids ) {
        const auto observation_iter = std::find_if( observations.begin(), observations.end(),
        [&member_id]( const active_member_observation & observation ) {
            return observation.npc_id == member_id;
        } );
        member_record *member = candidate.find_member( member_id );
        if( observation_iter == observations.end() || member == nullptr ) {
            return origin_loss_resolution_effect();
        }

        const std::string &member_summary = observation_iter->summary.empty() ?
                                            summary : observation_iter->summary;
        member_state resolved_state = member_state::orphaned;
        if( observation_iter->state == active_member_observation_state::dead ) {
            resolved_state = member_state::dead;
            effect.dead_members++;
        } else if( observation_iter->state == active_member_observation_state::missing ) {
            resolved_state = member_state::missing;
            effect.missing_members++;
        } else {
            effect.orphaned_survivors++;
        }

        if( candidate_outing->member_is_resolved( member_id ) ) {
            const bool matching_resolution =
                ( resolved_state == member_state::orphaned && member->state == member_state::at_home ) ||
                member->state == resolved_state;
            if( !matching_resolution ) {
                return origin_loss_resolution_effect();
            }
        }
        if( member->state != resolved_state &&
            !update_member_state( candidate, member_id, resolved_state, member_summary ) ) {
            return origin_loss_resolution_effect();
        }
        if( resolved_state == member_state::dead || resolved_state == member_state::missing ) {
            if( std::find( candidate_outing->casualty_ids.begin(),
                          candidate_outing->casualty_ids.end(), member_id ) ==
                candidate_outing->casualty_ids.end() ) {
                candidate_outing->casualty_ids.push_back( member_id );
            }
        }
        if( !candidate_outing->member_is_resolved( member_id ) ) {
            candidate_outing->resolved_member_ids.push_back( member_id );
        }
    }
    candidate_outing->last_progress_minutes = current_minutes;
    candidate_outing->last_advanced_minutes = current_minutes;
    if( !release_matching_external_reservation(
            candidate, expected_activity_id, expected_generation, summary ) ) {
        return origin_loss_resolution_effect();
    }
    if( !candidate.roster().valid ) {
        return origin_loss_resolution_effect();
    }

    site = std::move( candidate );
    effect.valid = true;
    effect.changed = true;
    effect.reservation_released = true;
    return effect;
}

std::optional<int> release_structural_outing_reservation( site_record &site,
        const std::string &expected_activity_id, const int expected_generation,
        const std::string &summary )
{
    if( site.active_outing.kind != outing_kind::structural_sortie ) {
        return std::nullopt;
    }
    return release_matching_external_reservation(
               site, expected_activity_id, expected_generation, summary );
}

int structural_outing_party_power( const site_record &site )
{
    const active_outing_state &outing = site.active_outing;
    if( outing.kind != outing_kind::structural_sortie || !outing.is_active() ) {
        return 0;
    }
    int party_power = 0;
    for( const character_id &member_id : outing.member_ids ) {
        if( outing.member_is_resolved( member_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
            outing.casualty_ids.end() ) {
            continue;
        }
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state == member_state::dead ||
            member->state == member_state::missing ) {
            return 0;
        }
        const routine_member_capability capability = routine_capability_for_template(
                    member->npc_template_id );
        party_power += std::clamp( capability.observer + capability.escort +
                                   capability.defender, 1, 10 );
    }
    return std::clamp( party_power, 0, 200 );
}

int structural_observer_omt_sight_range( const structural_observer_visibility_read &read )
{
    if( read.ordinary_sight_range_ms < 0 ||
        !std::isfinite( read.weather_sight_penalty ) || read.weather_sight_penalty < 1.0f ) {
        return 0;
    }
    long long sight = read.ordinary_sight_range_ms < SEEX ? 1 :
                      read.ordinary_sight_range_ms <= SEEX * 4 ? 2 : 3;
    sight += static_cast<long long>( std::max( 0, read.elevation_omt ) ) * 2;
    if( read.has_optic ) {
        sight *= 2;
    }
    sight = static_cast<long long>( std::floor( sight / read.weather_sight_penalty ) );
    return static_cast<int>( std::clamp<long long>(
                                 sight, 0, std::numeric_limits<int>::max() ) );
}

bool structural_observer_route_is_visible( int sight_points,
        const std::vector<int> &terrain_see_costs )
{
    if( sight_points < 0 ) {
        return false;
    }
    for( const int see_cost : terrain_see_costs ) {
        if( see_cost < 0 ) {
            return false;
        }
        sight_points -= see_cost;
        if( sight_points < 0 ) {
            return false;
        }
    }
    return true;
}

int structural_observer_last_known_max_age_minutes()
{
    return 60;
}

bool structural_observer_route_is_retained( const int sight_points,
        const std::vector<int> &terrain_see_costs, const int last_known_age_minutes )
{
    if( sight_points < 0 || last_known_age_minutes < 0 ||
        last_known_age_minutes > structural_observer_last_known_max_age_minutes() ) {
        return false;
    }
    const int retain_sight_points = sight_points == std::numeric_limits<int>::max() ?
                                    sight_points : sight_points + 1;
    return structural_observer_route_is_visible( retain_sight_points, terrain_see_costs );
}

bool structural_observer_retained_threat_matches(
    const structural_threat_observer_request &request, const tripoint_abs_omt &threat_omt,
    const std::vector<std::string> &stable_threat_ids )
{
    return request.retained_threat_omt &&
           request.retained_threat_age_minutes >= 0 &&
           request.retained_threat_age_minutes <=
           structural_observer_last_known_max_age_minutes() &&
           threat_omt == *request.retained_threat_omt && !stable_threat_ids.empty() &&
           std::is_sorted( stable_threat_ids.begin(), stable_threat_ids.end() ) &&
           std::adjacent_find( stable_threat_ids.begin(), stable_threat_ids.end() ) ==
           stable_threat_ids.end() &&
           stable_threat_ids == request.retained_threat_ids;
}

namespace
{
void add_structural_observer_retained_track( const site_record &site,
        structural_threat_observer_request &request, const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    const sortie_observation *latest = nullptr;
    for( const sortie_observation &observation : outing.observations ) {
        const int age_minutes = now_minutes - observation.observed_minutes;
        const bool source_is_permitted = observation.source_omt == request.current_omt ||
                                         std::find( request.visible_forward_omts.begin(),
                                                 request.visible_forward_omts.end(),
                                                 observation.source_omt ) !=
                                         request.visible_forward_omts.end();
        const bool observer_can_retain = observation.observer_id == outing.leader_id ||
                                         observation.share_state ==
                                         sortie_observation_share_state::shared;
        if( observation.record_schema_version != 1 ||
            observation.fact_key.rfind( "structural-visual:", 0 ) != 0 ||
            observation.sense != sortie_observation_sense::visual ||
            observation.target_revision != outing.target_lead_revision ||
            observation.observed_minutes < 0 || observation.observed_minutes > now_minutes ||
            observation.expiry_minutes < now_minutes || observation.defender_ids.empty() ||
            age_minutes > structural_observer_last_known_max_age_minutes() ||
            !source_is_permitted || !observer_can_retain ) {
            continue;
        }
        if( latest == nullptr ||
            std::tie( observation.observed_minutes, observation.fact_key,
                      observation.source_omt ) >
            std::tie( latest->observed_minutes, latest->fact_key, latest->source_omt ) ) {
            latest = &observation;
        }
    }
    if( latest == nullptr ) {
        return;
    }
    request.retained_threat_omt = latest->source_omt;
    request.retained_threat_ids = latest->defender_ids;
    request.retained_threat_age_minutes = now_minutes - latest->observed_minutes;
}

bool structural_abstract_threat_read_is_valid( const tripoint_abs_omt &current_omt,
        const abstract_threat_read &read )
{
    if( !read.observed ) {
        return !read.overlap && read.stable_threat_ids.empty() && read.detours.empty() &&
               read.danger_low == 0 && read.danger_high == 0;
    }
    if( read.stable_threat_ids.empty() ||
        read.stable_threat_ids.size() > max_abstract_threat_ids ||
        !std::is_sorted( read.stable_threat_ids.begin(), read.stable_threat_ids.end() ) ||
        std::adjacent_find( read.stable_threat_ids.begin(), read.stable_threat_ids.end() ) !=
        read.stable_threat_ids.end() ||
        std::any_of( read.stable_threat_ids.begin(), read.stable_threat_ids.end(),
    []( const std::string & id ) {
        return id.empty() || id.size() > max_abstract_threat_id_length;
    } ) || read.danger_low < 0 || read.danger_high < read.danger_low ||
    read.danger_high > 200 || read.visual_quality < 1 || read.visual_quality > 3 ||
    read.uncertainty_radius_omt < 0 || read.uncertainty_radius_omt > 40 ||
    read.equipment_detail < 0 || read.equipment_detail > 3 || read.detours.size() > 2 ||
    read.overlap != ( read.threat_omt == current_omt ) ) {
        return false;
    }
    std::vector<tripoint_abs_omt> checked_detours;
    for( const abstract_threat_detour_read &detour : read.detours ) {
        if( detour.omt.z() != current_omt.z() ||
            omt_chebyshev_distance( current_omt, detour.omt ) != 1 ||
            detour.omt == read.threat_omt ||
            std::find( checked_detours.begin(), checked_detours.end(), detour.omt ) !=
            checked_detours.end() ) {
            return false;
        }
        checked_detours.push_back( detour.omt );
    }
    return true;
}

std::string structural_signal_sense_name( const sortie_observation_sense sense )
{
    switch( sense ) {
        case sortie_observation_sense::smoke:
            return "smoke";
        case sortie_observation_sense::light:
            return "light";
        case sortie_observation_sense::sound:
            return "sound";
        default:
            return std::string();
    }
}

std::string structural_sound_kind_name( const structural_sound_kind kind )
{
    switch( kind ) {
        case structural_sound_kind::gunfire:
            return "gunfire";
        case structural_sound_kind::alarm:
            return "alarm";
        case structural_sound_kind::explosion:
            return "explosion";
        case structural_sound_kind::none:
            return std::string();
    }
    return std::string();
}

bool structural_signal_reads_are_valid( const structural_threat_observer_request &request,
                                        const std::vector<structural_signal_read> &reads,
                                        const int now_minutes )
{
    if( reads.size() > max_structural_signal_reads ) {
        return false;
    }
    std::vector<std::pair<sortie_observation_sense, tripoint_abs_omt>> identities;
    identities.reserve( reads.size() );
    for( const structural_signal_read &read : reads ) {
        const bool source_is_permitted = read.source_omt == request.current_omt ||
                                         std::find( request.visible_forward_omts.begin(),
                                                 request.visible_forward_omts.end(),
                                                 read.source_omt ) !=
                                         request.visible_forward_omts.end();
        const std::pair<sortie_observation_sense, tripoint_abs_omt> identity = {
            read.sense, read.source_omt
        };
        const bool sound_read = read.sense == sortie_observation_sense::sound;
        const bool sound_kind_valid = !structural_sound_kind_name( read.sound_kind ).empty();
        const bool sound_time_valid = read.emitted_minutes >= 0 &&
                                      read.emitted_minutes >= request.observation_window_start_minutes &&
                                      read.emitted_minutes <= now_minutes &&
                                      now_minutes - read.emitted_minutes <= 180;
        if( structural_signal_sense_name( read.sense ).empty() ||
            ( sound_read ? ( !sound_kind_valid || !sound_time_valid ) :
              ( read.sound_kind != structural_sound_kind::none || read.emitted_minutes != -1 ) ) ||
            !source_is_permitted ||
            read.range_cap_omt < 1 || read.range_cap_omt > 40 ||
            omt_chebyshev_distance( request.current_omt, read.source_omt ) > read.range_cap_omt ||
            read.strength < 1 || read.strength > 6 || read.confidence < 0 ||
            read.confidence > 100 || read.uncertainty_radius_omt < 1 ||
            read.uncertainty_radius_omt > 40 || read.local_reality ||
            read.summary.size() > max_sortie_summary_length ||
            std::find( identities.begin(), identities.end(), identity ) != identities.end() ) {
            return false;
        }
        identities.push_back( identity );
    }
    return true;
}

bool structural_pair_can_share_signal_observation( const site_record &site,
        const structural_signal_read &read, const structural_threat_observer_request &request )
{
    const active_outing_state &outing = site.active_outing;
    if( read.source_omt == request.current_omt || read.local_reality ||
        outing.owner != simulation_owner::abstract || outing.member_ids.size() != 2 ||
        std::find( outing.member_ids.begin(), outing.member_ids.end(), outing.leader_id ) ==
        outing.member_ids.end() ) {
        return false;
    }
    return std::all_of( outing.member_ids.begin(), outing.member_ids.end(),
    [&site, &outing]( const character_id member_id ) {
        const member_record *member = site.find_member( member_id );
        return member != nullptr && member->state == member_state::outbound &&
               !outing.member_is_resolved( member_id ) &&
               std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) ==
               outing.casualty_ids.end();
    } );
}

sortie_observation make_structural_signal_observation( const site_record &site,
        const structural_threat_observer_request &request, const structural_signal_read &read,
        const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    const std::string sense_name = structural_signal_sense_name( read.sense );
    const std::string class_name = read.sense == sortie_observation_sense::sound ?
                                   structural_sound_kind_name( read.sound_kind ) : sense_name;
    const std::string source_id = "structural-" + class_name + "@" + read.source_omt.to_string();
    const int observed_minutes = read.sense == sortie_observation_sense::sound ?
                                 read.emitted_minutes : now_minutes;
    sortie_observation observation;
    observation.fact_key = "structural-signal:" + source_id;
    observation.summary = read.summary;
    observation.confidence = read.confidence;
    observation.observed_minutes = observed_minutes;
    observation.kind = sortie_observation_kind::certainty;
    observation.state_key = "structural-" + class_name + "-signal";
    observation.record_schema_version = 1;
    observation.source_id = source_id;
    observation.sense = read.sense;
    observation.observer_id = outing.leader_id;
    observation.source_omt = read.source_omt;
    observation.receiver_omt = request.current_omt;
    observation.bucket_start_minutes = observed_minutes - observed_minutes % 30;
    observation.strength = read.strength;
    observation.simultaneity_start_minutes = observed_minutes;
    observation.simultaneity_end_minutes = observed_minutes;
    observation.target_revision = outing.target_lead_revision;
    observation.uncertainty_radius_omt = read.uncertainty_radius_omt;
    observation.expiry_minutes = minutes_after_saturated(
                                     observed_minutes, read.sense == sortie_observation_sense::sound ?
                                     3 * 60 : 6 * 60 );
    observation.share_state = structural_pair_can_share_signal_observation( site, read, request ) ?
                              sortie_observation_share_state::shared :
                              sortie_observation_share_state::observer_private;
    return observation;
}

std::vector<sortie_observation> make_structural_signal_observations( const site_record &site,
        const structural_threat_observer_request &request,
        std::vector<structural_signal_read> reads, const int now_minutes )
{
    std::sort( reads.begin(), reads.end(), [&request]( const structural_signal_read &lhs,
    const structural_signal_read &rhs ) {
        return std::make_tuple( omt_chebyshev_distance( request.current_omt, lhs.source_omt ),
                                lhs.source_omt.z(), lhs.source_omt.y(), lhs.source_omt.x(),
                                static_cast<int>( lhs.sense ) ) <
               std::make_tuple( omt_chebyshev_distance( request.current_omt, rhs.source_omt ),
                                rhs.source_omt.z(), rhs.source_omt.y(), rhs.source_omt.x(),
                                static_cast<int>( rhs.sense ) );
    } );
    std::vector<sortie_observation> result;
    for( const sortie_observation_sense sense : {
             sortie_observation_sense::smoke, sortie_observation_sense::light,
             sortie_observation_sense::sound
         } ) {
        const auto found = std::find_if( reads.begin(), reads.end(), [sense](
        const structural_signal_read & read ) {
            return read.sense == sense;
        } );
        if( found != reads.end() ) {
            result.push_back( make_structural_signal_observation( site, request, *found,
                              now_minutes ) );
        }
    }
    return result;
}

unsigned long long structural_threat_fact_hash( const abstract_threat_read &read )
{
    unsigned long long hash = 1469598103934665603ULL;
    const auto add_text = [&hash]( const std::string & text ) {
        for( const unsigned char byte : text ) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xffU;
        hash *= 1099511628211ULL;
    };
    add_text( read.threat_omt.to_string() );
    for( const std::string &id : read.stable_threat_ids ) {
        add_text( id );
    }
    return hash;
}

bool structural_pair_can_share_forward_observation( const site_record &site,
        const abstract_threat_read &read )
{
    const active_outing_state &outing = site.active_outing;
    if( read.overlap || read.local_reality || outing.owner != simulation_owner::abstract ||
        outing.member_ids.size() != 2 ||
        std::find( outing.member_ids.begin(), outing.member_ids.end(), outing.leader_id ) ==
        outing.member_ids.end() ) {
        return false;
    }
    return std::all_of( outing.member_ids.begin(), outing.member_ids.end(),
    [&site, &outing]( const character_id member_id ) {
        const member_record *member = site.find_member( member_id );
        return member != nullptr && member->state == member_state::outbound &&
               !outing.member_is_resolved( member_id ) &&
               std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) ==
               outing.casualty_ids.end();
    } );
}

sortie_observation make_structural_threat_observation( const site_record &site,
        const structural_threat_observer_request &request, const abstract_threat_read &read,
        const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    const int party_power = structural_outing_party_power( site );
    const bool hard_danger = std::min( 1000, 5 * read.danger_high ) >= routine_hard_risk ||
                             read.danger_low >= std::min( 200, 2 * party_power );
    sortie_observation observation;
    observation.fact_key = "structural-visual:" +
                           std::to_string( structural_threat_fact_hash( read ) );
    observation.summary = read.summary.substr( 0, max_sortie_summary_length );
    observation.confidence = std::clamp( 50 + 10 * read.visual_quality, 0, 100 );
    observation.observed_minutes = now_minutes;
    observation.critical = hard_danger;
    observation.kind = hard_danger ? sortie_observation_kind::hard_danger :
                       sortie_observation_kind::certainty;
    observation.state_key = read.overlap ? "structural-overlap-threat" :
                            "structural-forward-threat";
    observation.record_schema_version = 1;
    observation.source_id = read.stable_threat_ids.front();
    observation.sense = sortie_observation_sense::visual;
    observation.observer_id = outing.leader_id;
    observation.source_omt = read.threat_omt;
    observation.receiver_omt = request.current_omt;
    observation.bucket_start_minutes = now_minutes - now_minutes % 30;
    observation.strength = read.danger_high == 0 ? 0 :
                           std::clamp( ( read.danger_high + 33 ) / 34, 1, 6 );
    observation.visual_quality = read.visual_quality;
    observation.defender_ids = read.stable_threat_ids;
    observation.simultaneity_start_minutes = now_minutes;
    observation.simultaneity_end_minutes = now_minutes;
    observation.observed_power_low = read.danger_low;
    observation.observed_power_high = read.danger_high;
    observation.equipment_detail = read.equipment_detail;
    observation.target_revision = outing.target_lead_revision;
    observation.uncertainty_radius_omt = read.uncertainty_radius_omt;
    observation.expiry_minutes = minutes_after_saturated(
                                     std::max( now_minutes, outing.expected_return_minutes ), 24 * 60 );
    observation.share_state = structural_pair_can_share_forward_observation( site, read ) ?
                              sortie_observation_share_state::shared :
                              sortie_observation_share_state::observer_private;
    return observation;
}

bool structural_local_zombie_read_is_valid( const site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const structural_local_zombie_read &read, const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    const member_record *observer = site.find_member( read.observer_id );
    const camp_map_lead *lead = site.intelligence_map.find_lead( outing.target_lead_id );
    return now_minutes >= 0 && now_minutes > expected_cursor.last_advanced_minutes &&
           outing.kind == outing_kind::structural_sortie && outing.schema_version >= 8 &&
           outing.owner == simulation_owner::local &&
           simulation_owner_state_is_consistent( outing ) &&
           simulation_cursor_matches( outing, expected_cursor ) &&
           expected_cursor.owner == simulation_owner::local &&
           outing.local_handoff.is_active() && outing.local_handoff.members.size() == 2 &&
           outing.member_ids.size() == 2 && read.source_omt == outing.local_handoff.route_position &&
           lead != nullptr && lead->revision == outing.target_lead_revision &&
           lead->omt == outing.target_omt &&
           std::find( outing.shared_route.begin(), outing.shared_route.end(), read.source_omt ) !=
           outing.shared_route.end() && observer != nullptr &&
           ( observer->state == member_state::outbound ||
             observer->state == member_state::local_contact ) &&
           std::find( outing.member_ids.begin(), outing.member_ids.end(), read.observer_id ) !=
           outing.member_ids.end() && !outing.member_is_resolved( read.observer_id ) &&
           std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), read.observer_id ) ==
           outing.casualty_ids.end() && read.inspected_monsters >= 1 &&
           read.inspected_monsters <= 64 && read.visible_count >= 1 && read.visible_count <= 64 &&
           read.visible_count <= read.inspected_monsters &&
           read.danger_low > 0 && read.danger_high >= read.danger_low && read.danger_high <= 200 &&
           read.visual_quality >= 1 && read.visual_quality <= 3 &&
           !read.stable_threat_ids.empty() &&
           read.stable_threat_ids.size() <= max_abstract_threat_ids &&
           read.visible_count >= static_cast<int>( read.stable_threat_ids.size() ) &&
           std::is_sorted( read.stable_threat_ids.begin(), read.stable_threat_ids.end() ) &&
           std::adjacent_find( read.stable_threat_ids.begin(),
                               read.stable_threat_ids.end() ) == read.stable_threat_ids.end() &&
           std::all_of( read.stable_threat_ids.begin(), read.stable_threat_ids.end(),
    []( const std::string & id ) {
        return id.rfind( "local-zombie:", 0 ) == 0 &&
               id.size() > std::string_view( "local-zombie:" ).size() &&
               id.size() <= max_sortie_defender_id_length;
    } );
}

sortie_observation make_structural_local_zombie_observation( const site_record &site,
        const structural_local_zombie_read &read, const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    abstract_threat_read hash_read;
    hash_read.threat_omt = read.source_omt;
    hash_read.stable_threat_ids = read.stable_threat_ids;
    const int party_power = structural_outing_party_power( site );
    const bool hard_danger = std::min( 1000, 5 * read.danger_high ) >= routine_hard_risk ||
                             read.danger_low >= std::min( 200, 2 * party_power );
    sortie_observation observation;
    observation.fact_key = "structural-local-zombie:" +
                           std::to_string( structural_threat_fact_hash( hash_read ) );
    observation.summary = read.visible_count >= 3 ?
                          "legitimately visible local zombie group" :
                          "legitimately visible local zombie danger";
    observation.confidence = std::clamp( 50 + 10 * read.visual_quality, 0, 100 );
    observation.observed_minutes = now_minutes;
    observation.critical = hard_danger;
    observation.kind = hard_danger ? sortie_observation_kind::hard_danger :
                       sortie_observation_kind::certainty;
    observation.state_key = "structural-local-zombie-threat";
    observation.record_schema_version = 1;
    observation.source_id = read.stable_threat_ids.front();
    observation.sense = sortie_observation_sense::visual;
    observation.observer_id = read.observer_id;
    observation.source_omt = read.source_omt;
    observation.receiver_omt = outing.local_handoff.route_position;
    observation.bucket_start_minutes = now_minutes - now_minutes % 30;
    observation.strength = std::clamp( ( read.danger_high + 33 ) / 34, 1, 6 );
    observation.visual_quality = read.visual_quality;
    observation.defender_ids = read.stable_threat_ids;
    observation.simultaneity_start_minutes = now_minutes;
    observation.simultaneity_end_minutes = now_minutes;
    observation.observed_power_low = read.danger_low;
    observation.observed_power_high = read.danger_high;
    observation.target_revision = outing.target_lead_revision;
    observation.expiry_minutes = minutes_after_saturated(
                                     std::max( now_minutes, outing.expected_return_minutes ), 24 * 60 );
    observation.share_state = sortie_observation_share_state::observer_private;
    return observation;
}

bool apply_returned_structural_threat_observation( site_record &site,
        camp_map_lead &lead, const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( lead.lead_id != outing.target_lead_id || lead.omt != outing.target_omt ||
        lead.revision != outing.target_lead_revision ) {
        return false;
    }
    const sortie_observation *best = nullptr;
    for( const sortie_observation &observation : outing.observations ) {
        const bool structural_danger_fact =
            observation.fact_key.rfind( "structural-visual:", 0 ) == 0 ||
            observation.fact_key.rfind( "structural-local-zombie:", 0 ) == 0;
        const member_record *observer = site.find_member( observation.observer_id );
        const bool private_observer_returned =
            observation.share_state == sortie_observation_share_state::observer_private &&
            observer != nullptr && observer->state == member_state::outbound &&
            !outing.member_is_resolved( observation.observer_id ) &&
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(),
                       observation.observer_id ) == outing.casualty_ids.end();
        if( observation.record_schema_version != 1 ||
            observation.kind != sortie_observation_kind::hard_danger ||
            !observation.critical ||
            !structural_danger_fact ||
            observation.sense != sortie_observation_sense::visual ||
            ( observation.share_state != sortie_observation_share_state::shared &&
              !private_observer_returned ) ||
            observation.target_revision <= 0 ||
            observation.target_revision > outing.target_lead_revision ||
            observation.expiry_minutes < now_minutes ||
            std::find( outing.member_ids.begin(), outing.member_ids.end(), observation.observer_id ) ==
            outing.member_ids.end() ||
            std::find( outing.shared_route.begin(), outing.shared_route.end(),
                       observation.source_omt ) == outing.shared_route.end() ||
            std::find( outing.shared_route.begin(), outing.shared_route.end(),
                       observation.receiver_omt ) == outing.shared_route.end() ||
            std::find( observation.defender_ids.begin(), observation.defender_ids.end(),
                       observation.source_id ) == observation.defender_ids.end() ) {
            continue;
        }
        if( best == nullptr ||
            std::tie( observation.observed_minutes, observation.observed_power_high,
                      observation.fact_key ) >
            std::tie( best->observed_minutes, best->observed_power_high, best->fact_key ) ) {
            best = &observation;
        }
    }
    if( best == nullptr || lead.revision >= std::numeric_limits<int>::max() ) {
        return false;
    }
    lead.threat = std::max( lead.threat, best->observed_power_high );
    lead.threat_confirmed = true;
    lead.last_checked_minutes = best->observed_minutes;
    lead.last_scouted_minutes = best->observed_minutes;
    lead.status = camp_lead_status::dangerous;
    lead.last_outcome = "returned_shared_structural_threat_report";
    if( !best->summary.empty() ) {
        lead.source_summary = best->summary.substr( 0, max_camp_lead_summary_length );
    }
    advance_camp_map_lead_revision( site, lead );
    return true;
}

bool returned_structural_signal_observation_is_eligible( const site_record &site,
        const sortie_observation &observation, const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    const member_record *observer = site.find_member( observation.observer_id );
    const bool private_observer_returned =
        observation.share_state == sortie_observation_share_state::observer_private &&
        observer != nullptr && observer->state == member_state::outbound &&
        !outing.member_is_resolved( observation.observer_id ) &&
        std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(),
                   observation.observer_id ) == outing.casualty_ids.end();
    return observation.record_schema_version == 1 &&
           !structural_signal_sense_name( observation.sense ).empty() &&
           observation.fact_key.rfind( "structural-signal:", 0 ) == 0 &&
           ( observation.share_state == sortie_observation_share_state::shared ||
             private_observer_returned ) &&
           observation.target_revision > 0 &&
           observation.target_revision <= outing.target_lead_revision &&
           observation.observed_minutes >= 0 && observation.observed_minutes <= now_minutes &&
           observation.expiry_minutes >= now_minutes && !observation.source_id.empty() &&
           std::find( outing.member_ids.begin(), outing.member_ids.end(), observation.observer_id ) !=
           outing.member_ids.end() &&
           std::find( outing.shared_route.begin(), outing.shared_route.end(),
                      observation.source_omt ) != outing.shared_route.end() &&
           std::find( outing.shared_route.begin(), outing.shared_route.end(),
                      observation.receiver_omt ) != outing.shared_route.end();
}

bool returned_structural_signal_lead_has_support( const site_record &site,
        const camp_map_lead &lead, const int now_minutes )
{
    if( !returned_structural_signal_lead( lead ) ) {
        return false;
    }
    return std::any_of( site.active_outing.observations.begin(),
    site.active_outing.observations.end(), [&site, &lead, now_minutes](
    const sortie_observation & observation ) {
        return returned_structural_signal_observation_is_eligible(
                   site, observation, now_minutes ) &&
               signal_kind_to_camp_lead_kind(
                   structural_signal_sense_name( observation.sense ) ) == lead.kind &&
               observation.source_id == lead.target_id && observation.source_omt == lead.omt;
    } );
}

int apply_returned_structural_signal_observations( site_record &site, const int now_minutes )
{
    const active_outing_state &outing = site.active_outing;
    std::vector<camp_map_lead> learned_leads;
    for( const sortie_observation &observation : outing.observations ) {
        const std::string sense_name = structural_signal_sense_name( observation.sense );
        if( !returned_structural_signal_observation_is_eligible(
                site, observation, now_minutes ) ) {
            continue;
        }

        camp_map_lead learned;
        learned.kind = signal_kind_to_camp_lead_kind( sense_name );
        learned.origin = camp_lead_origin::returned_report;
        learned.status = camp_lead_status::suspected;
        learned.target_id = observation.source_id;
        learned.omt = observation.source_omt;
        learned.radius_omt = observation.uncertainty_radius_omt;
        learned.source_key = observation.fact_key;
        learned.source_summary = observation.summary;
        learned.first_seen_minutes = observation.observed_minutes;
        learned.last_seen_minutes = observation.observed_minutes;
        learned.last_scouted_minutes = observation.observed_minutes;
        learned.confidence = std::clamp( ( observation.confidence + 24 ) / 25, 1, 4 );
        learned.generated_by_this_camp_routine = true;
        learned.last_outcome = "returned_structural_" + sense_name + "_report";
        learned.lead_id = camp_lead_id_for( site.site_id, learned.kind, learned.target_id,
                                           learned.omt );
        learned_leads.push_back( std::move( learned ) );
    }
    if( learned_leads.empty() ) {
        return 0;
    }

    std::sort( learned_leads.begin(), learned_leads.end(), []( const camp_map_lead &lhs,
    const camp_map_lead &rhs ) {
        if( lhs.lead_id != rhs.lead_id ) {
            return lhs.lead_id < rhs.lead_id;
        }
        return std::tie( lhs.last_seen_minutes, lhs.confidence, lhs.source_key ) >
               std::tie( rhs.last_seen_minutes, rhs.confidence, rhs.source_key );
    } );
    learned_leads.erase( std::unique( learned_leads.begin(), learned_leads.end(),
    []( const camp_map_lead &lhs, const camp_map_lead &rhs ) {
        return lhs.lead_id == rhs.lead_id;
    } ), learned_leads.end() );
    site_record transaction = site;
    for( const camp_map_lead &learned : learned_leads ) {
        if( !upsert_camp_map_lead( transaction, learned ) ) {
            return 0;
        }
    }
    for( const camp_map_lead &learned : learned_leads ) {
        const camp_map_lead *retained = transaction.intelligence_map.find_lead( learned.lead_id );
        if( retained == nullptr || retained->origin != camp_lead_origin::returned_report ||
            retained->target_id != learned.target_id || retained->omt != learned.omt ||
            retained->source_key != learned.source_key ) {
            return 0;
        }
    }
    site = std::move( transaction );
    return static_cast<int>( learned_leads.size() );
}
} // namespace

abstract_threat_resolution resolve_structural_abstract_threat( site_record &site,
        const tripoint_abs_omt &current_omt, const abstract_threat_read &read,
        const int now_minutes )
{
    abstract_threat_resolution result;
    const active_outing_state &current = site.active_outing;
    if( current.kind != outing_kind::structural_sortie || current.schema_version < 8 ||
        current.owner != simulation_owner::abstract || !current.is_active() ||
        now_minutes < 0 || current.last_advanced_minutes != now_minutes ||
        current.shared_route.empty() || current.waypoint_index < 0 ||
        current.waypoint_index >= static_cast<int>( current.shared_route.size() ) ||
        current.shared_route[static_cast<std::size_t>( current.waypoint_index )] != current_omt ||
        !site.roster().valid ) {
        return result;
    }

    if( !structural_abstract_threat_read_is_valid( current_omt, read ) ) {
        return result;
    }

    site_record candidate = site;
    active_outing_state &outing = candidate.active_outing;
    abstract_encounter_state &encounter = outing.abstract_encounter;
    result.valid = true;

    if( encounter.active && current_omt != encounter.overlap_omt ) {
        const bool same_threat_overlaps = read.observed && read.overlap &&
                                          read.stable_threat_ids == encounter.stable_threat_ids;
        if( same_threat_overlaps ) {
            encounter.overlap_omt = current_omt;
            encounter.absent_segment_advances = 0;
            result.changed = true;
            result.notes.push_back( "active abstract threat followed the party into the next OMT" );
        } else if( encounter.absent_segment_advances == 0 ) {
            encounter.absent_segment_advances = 1;
            result.changed = true;
            result.notes.push_back( "abstract encounter began its one-segment clear interval" );
            if( candidate.roster().valid ) {
                site = std::move( candidate );
            }
            return result;
        } else {
            encounter.clear_active();
            result.changed = true;
            result.encounter_cleared = true;
            result.notes.push_back( "abstract encounter cleared after one complete absent segment" );
        }
    } else if( encounter.active && current_omt == encounter.overlap_omt ) {
        encounter.absent_segment_advances = 0;
    }

    if( !read.observed ) {
        if( result.changed && candidate.roster().valid ) {
            site = std::move( candidate );
        }
        return result;
    }

    if( encounter.active ) {
        if( encounter.outcome_applied ) {
            if( result.changed && candidate.roster().valid ) {
                site = std::move( candidate );
            }
            return result;
        }
        if( encounter.local_claimed || read.local_reality ) {
            result.kind = abstract_threat_resolution_kind::deferred_to_local;
            if( result.changed && candidate.roster().valid ) {
                site = std::move( candidate );
            }
            return result;
        }
    }

    const int party_power = structural_outing_party_power( candidate );
    if( party_power <= 0 ) {
        return abstract_threat_resolution();
    }
    const bool hard_danger = std::min( 1000, 5 * read.danger_high ) >= routine_hard_risk ||
                             read.danger_low >= std::min( 200, 2 * party_power );
    const auto attempt_detour = [&outing, &read, &result]() {
        for( const abstract_threat_detour_read &detour : read.detours ) {
            if( outing.abstract_detour_attempts >= 2 ) {
                break;
            }
            outing.abstract_detour_attempts++;
            result.detour_attempts++;
            if( !detour.passable ) {
                continue;
            }
            outing.has_withdrawal_detour = true;
            outing.withdrawal_detour_omt = detour.omt;
            return true;
        }
        return false;
    };

    if( !read.overlap ) {
        if( read.local_reality ) {
            result.kind = abstract_threat_resolution_kind::deferred_to_local;
            result.notes.push_back( "reality bubble deferred visible forward danger" );
            if( result.changed && candidate.roster().valid ) {
                site = std::move( candidate );
            }
            return result;
        }
        if( !hard_danger ) {
            result.kind = abstract_threat_resolution_kind::observed_below_gate;
            result.notes.push_back( "visible forward threat remained below withdrawal gates" );
            return result;
        }
        attempt_detour();
        outing.phase = scout_phase::returning_home;
        outing.last_progress_minutes = now_minutes;
        result.kind = abstract_threat_resolution_kind::withdrawal;
        result.changed = true;
        result.outcome_applied = true;
        result.notes.push_back( outing.has_withdrawal_detour ?
                                "visible forward danger selected an adjacent withdrawal detour" :
                                "visible forward danger blocked the route and forced withdrawal" );
        if( candidate.roster().valid ) {
            site = std::move( candidate );
        }
        return result;
    }

    if( !encounter.active ) {
        if( encounter.episode == std::numeric_limits<int>::max() ) {
            return abstract_threat_resolution();
        }
        encounter.active = true;
        encounter.episode++;
        encounter.overlap_omt = current_omt;
        encounter.stable_threat_ids = read.stable_threat_ids;
        encounter.danger_low = read.danger_low;
        encounter.danger_high = read.danger_high;
        encounter.absent_segment_advances = 0;
        result.changed = true;
        result.encounter_started = true;
    }

    if( read.local_reality ) {
        result.kind = abstract_threat_resolution_kind::deferred_to_local;
        result.notes.push_back( "reality bubble deferred the abstract encounter episode" );
        if( candidate.roster().valid ) {
            site = std::move( candidate );
        }
        return result;
    }
    result.changed = true;

    bool selected_detour = false;
    if( hard_danger ) {
        selected_detour = attempt_detour();
        encounter.detour_attempts = result.detour_attempts;
        if( selected_detour ) {
            encounter.has_selected_detour = true;
            encounter.selected_detour_omt = outing.withdrawal_detour_omt;
        }
    }
    if( selected_detour ) {
        outing.phase = scout_phase::returning_home;
        encounter.outcome_applied = true;
        encounter.last_applied_episode = encounter.episode;
        encounter.outcome = "detoured_withdrawal";
        outing.last_progress_minutes = now_minutes;
        result.kind = abstract_threat_resolution_kind::withdrawal;
        result.outcome_applied = true;
        result.notes.push_back( "overlapped party escaped through an adjacent withdrawal detour" );
        if( candidate.roster().valid ) {
            site = std::move( candidate );
        }
        return result;
    }

    struct member_power_record {
        character_id npc_id;
        int power = 0;
    };
    std::vector<member_power_record> surviving_members;
    for( const character_id &member_id : outing.member_ids ) {
        if( outing.member_is_resolved( member_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
            outing.casualty_ids.end() ) {
            continue;
        }
        member_record *member = candidate.find_member( member_id );
        if( member == nullptr ) {
            return abstract_threat_resolution();
        }
        const routine_member_capability capability = routine_capability_for_template(
                    member->npc_template_id );
        surviving_members.push_back( { member_id,
                std::clamp( capability.observer + capability.escort + capability.defender,
                            1, 10 ) } );
    }
    if( surviving_members.size() != 2 ) {
        return abstract_threat_resolution();
    }
    std::sort( surviving_members.begin(), surviving_members.end(),
    []( const member_power_record & lhs, const member_power_record & rhs ) {
        return std::tie( lhs.power, lhs.npc_id ) < std::tie( rhs.power, rhs.npc_id );
    } );
    member_record *lower_power_member = candidate.find_member( surviving_members[0].npc_id );
    member_record *other_member = candidate.find_member( surviving_members[1].npc_id );
    if( lower_power_member == nullptr || other_member == nullptr ) {
        return abstract_threat_resolution();
    }
    const auto mark_missing = [&candidate, &outing]( const character_id member_id ) {
        if( !update_member_state( candidate, member_id, member_state::missing,
                                 "abstract threat encounter marked member missing" ) ) {
            return false;
        }
        outing.casualty_ids.push_back( member_id );
        outing.resolved_member_ids.push_back( member_id );
        return true;
    };
    if( read.danger_high < party_power ) {
        lower_power_member->wounded_or_unready = true;
        lower_power_member->abstract_wound_until_minutes = minutes_after_saturated(
                    now_minutes, abstract_wound_recovery_minutes );
        lower_power_member->last_writeback_summary =
            "abstract threat encounter wounded the lower-power scout";
        encounter.outcome = "lower_power_member_wounded";
        result.kind = abstract_threat_resolution_kind::wounded_pair;
    } else if( read.danger_high < std::min( 200, 2 * party_power ) ) {
        if( !mark_missing( lower_power_member->npc_id ) ) {
            return abstract_threat_resolution();
        }
        other_member = candidate.find_member( surviving_members[1].npc_id );
        if( other_member == nullptr ) {
            return abstract_threat_resolution();
        }
        other_member->wounded_or_unready = true;
        other_member->abstract_wound_until_minutes = minutes_after_saturated(
                    now_minutes, abstract_wound_recovery_minutes );
        other_member->last_writeback_summary =
            "abstract threat encounter survivor returned wounded";
        encounter.outcome = "one_missing_survivor_wounded";
        result.kind = abstract_threat_resolution_kind::one_missing;
    } else {
        if( !mark_missing( surviving_members[0].npc_id ) ||
            !mark_missing( surviving_members[1].npc_id ) ) {
            return abstract_threat_resolution();
        }
        encounter.outcome = "all_members_missing";
        result.kind = abstract_threat_resolution_kind::all_missing;
    }
    encounter.outcome_applied = true;
    encounter.last_applied_episode = encounter.episode;
    outing.phase = result.kind == abstract_threat_resolution_kind::all_missing ?
                   scout_phase::lost : scout_phase::returning_home;
    outing.last_progress_minutes = now_minutes;
    result.outcome_applied = true;
    result.notes.push_back( "abstract overlap applied one non-victory member outcome" );
    if( !simulation_owner_state_is_consistent( outing ) || !candidate.roster().valid ) {
        return abstract_threat_resolution();
    }
    site = std::move( candidate );
    return result;
}

namespace
{
structural_outing_plan plan_frontier_outing_impl( const site_record &site,
        const int now_minutes, const bool solve_route, const bool require_exact_pair = true )
{
    structural_outing_plan plan;
    plan.site_id = site.site_id;
    plan.activity_id = site.site_id + "#structural";
    plan.generation = site.next_outing_generation;
    if( now_minutes < 0 || !supports_routine_camp_ecology( effective_profile( site ) ) ||
        site.routine_activated_minutes > now_minutes ||
        ( site.next_routine_dispatch_eligible_minutes >= 0 &&
          now_minutes < site.next_routine_dispatch_eligible_minutes ) ||
        site.retired_empty_site || site.has_active_outside_pressure() ||
        !camp_decision_allows_dispatch( site.camp_decision,
                                        bandit_dry_run::job_template::scout ) ||
        !frontier_memory_is_valid( site.intelligence_map ) ) {
        plan.notes.push_back( "frontier outing blocked: camp state is not eligible" );
        return plan;
    }

    plan.frontier_cursor = site.intelligence_map.frontier_sector_cursor;
    camp_map_lead scoring_lead;
    for( const int sector : least_recent_frontier_sectors( site.intelligence_map ) ) {
        const std::optional<tripoint_abs_omt> target_omt = frontier_outer_target( site.anchor, sector );
        if( !target_omt ) {
            continue;
        }
        const std::string lead_id = frontier_probe_lead_id( sector );
        const camp_map_lead *existing_lead = site.intelligence_map.find_lead( lead_id );
        if( existing_lead != nullptr ) {
            const std::optional<int> existing_sector = frontier_sector_from_lead( *existing_lead );
            if( !existing_sector || *existing_sector != sector ||
                existing_lead->omt != *target_omt ||
                existing_lead->revision >= std::numeric_limits<int>::max() ||
                existing_lead->status == camp_lead_status::active ||
                existing_lead->status == camp_lead_status::dangerous ||
                existing_lead->status == camp_lead_status::invalidated ) {
                continue;
            }
            scoring_lead = *existing_lead;
            plan.lead_revision = existing_lead->revision;
            plan.known_threat = structural_known_threat_for_interest( *existing_lead );
        } else {
            scoring_lead.lead_id = lead_id;
            scoring_lead.revision = 1;
            scoring_lead.kind = camp_lead_kind::frontier_probe;
            scoring_lead.origin = camp_lead_origin::structural_routine;
            scoring_lead.status = camp_lead_status::suspected;
            scoring_lead.target_id = frontier_probe_target_id( sector );
            scoring_lead.omt = *target_omt;
            scoring_lead.source_key = scoring_lead.target_id;
            scoring_lead.last_checked_minutes = site.intelligence_map.frontier_last_resolved_minutes[
                                                   static_cast<std::size_t>( sector )];
            scoring_lead.last_scouted_minutes = scoring_lead.last_checked_minutes;
            plan.lead_revision = 1;
        }
        plan.frontier_sector = sector;
        plan.frontier_prior_resolved_minutes =
            site.intelligence_map.frontier_last_resolved_minutes[static_cast<std::size_t>( sector )];
        plan.target_omt = *target_omt;
        plan.lead_id = lead_id;
        break;
    }
    if( plan.frontier_sector < 0 ) {
        plan.notes.push_back( "frontier outing blocked: no eligible least-recent sector" );
        return plan;
    }

    plan.effective_interest = 1;
    plan.terrain_fit = hostile_camp_terrain_fit( effective_profile( site ), "unknown" );
    plan.static_risk = structural_terrain_static_risk( "unknown" );
    plan.estimate_freshness = structural_estimate_freshness( scoring_lead, now_minutes );
    plan.repetition_penalty = structural_repetition_penalty( site, scoring_lead );
    plan.cheap_route_quality = structural_candidate_route_quality(
                                   omt_chebyshev_distance( site.anchor, plan.target_omt ) );
    plan.cheap_score = structural_candidate_score( site, scoring_lead, now_minutes,
                       plan.cheap_route_quality, plan.terrain_fit, plan.static_risk );
    if( hostile_camp_routine_risk_blocked( plan.static_risk ) ) {
        plan.notes.push_back( "frontier outing blocked: hard risk gate" );
        return plan;
    }
    if( solve_route ) {
        plan.shared_route = make_frontier_radial_route( site.anchor, plan.frontier_sector );
        plan.route_solved = true;
        if( plan.shared_route.empty() ) {
            plan.notes.push_back( "frontier outing blocked: no bounded route" );
            return plan;
        }
        plan.full_route_cost = structural_route_cost( plan.shared_route );
        plan.max_route_segment_risk = plan.static_risk;
        plan.final_route_quality = structural_candidate_route_quality(
                                       plan.full_route_cost );
        plan.final_score = structural_candidate_score( site, scoring_lead, now_minutes,
                           plan.final_route_quality, plan.terrain_fit, plan.static_risk );
    }

    const routine_scout_policy_result routine_policy = routine_scout_policy( site );
    const routine_scout_pair_selection_result pair_selection = select_routine_scout_pair( site );
    if( !routine_policy.eligible || ( require_exact_pair && !pair_selection.eligible ) ) {
        plan.notes.push_back( "frontier outing blocked: exact routine pair is ineligible" );
        return plan;
    }
    plan.job = bandit_dry_run::job_template::scout;
    if( require_exact_pair ) {
        plan.member_ids = pair_selection.member_ids;
    }
    plan.expected_stalking_minutes = minutes_after_saturated(
                                        now_minutes,
                                        structural_stalking_delay_minutes( site.anchor, plan.target_omt ) );
    plan.expected_arrival_minutes = minutes_after_saturated(
                                       now_minutes,
                                       structural_arrival_delay_minutes( site.anchor, plan.target_omt ) );
    plan.expected_return_minutes = structural_expected_return_minutes(
                                       now_minutes, site.anchor, plan.target_omt );
    plan.valid = true;
    plan.notes.push_back( "frontier outing sector=" + std::to_string( plan.frontier_sector ) +
                          " prior_resolved=" +
                          std::to_string( plan.frontier_prior_resolved_minutes ) +
                          " cheap_score=" + std::to_string( plan.cheap_score ) +
                          ( solve_route ? " route_cost=" +
                            std::to_string( structural_route_cost( plan.shared_route ) ) :
                            " route=pending" ) );
    return plan;
}

structural_outing_plan plan_structural_bounty_outing_impl( const site_record &site,
        const camp_map_lead &lead, const int now_minutes, const bool solve_route,
        const bool require_exact_pair = true )
{
    structural_outing_plan plan;
    plan.site_id = site.site_id;
    plan.activity_id = site.site_id + "#structural";
    plan.generation = site.next_outing_generation;
    plan.lead_id = lead.lead_id;
    plan.lead_revision = lead.revision;
    plan.target_omt = lead.omt;
    plan.known_threat = structural_known_threat_for_interest( lead );
    plan.effective_interest = structural_effective_interest( lead, plan.known_threat );
    const std::string terrain_fit_class = structural_terrain_fit_class( lead );
    plan.terrain_fit = hostile_camp_terrain_fit( effective_profile( site ), terrain_fit_class );
    plan.static_risk = structural_terrain_static_risk( terrain_fit_class );
    plan.estimate_freshness = structural_estimate_freshness( lead, now_minutes );
    plan.repetition_penalty = structural_repetition_penalty( site, lead );
    plan.cheap_route_quality = structural_candidate_route_quality(
                                   omt_chebyshev_distance( site.anchor, lead.omt ) );
    plan.cheap_score = structural_candidate_score( site, lead, now_minutes,
                       plan.cheap_route_quality, plan.terrain_fit, plan.static_risk );

    if( !supports_routine_camp_ecology( effective_profile( site ) ) ) {
        plan.notes.push_back( "structural outing blocked: site profile does not run routine camp ecology" );
        return plan;
    }
    if( now_minutes < 0 ) {
        plan.notes.push_back( "structural outing blocked: invalid time" );
        return plan;
    }
    if( site.routine_activated_minutes > now_minutes ) {
        plan.notes.push_back( "structural outing blocked: routine activation is in the future" );
        return plan;
    }
    if( site.next_routine_dispatch_eligible_minutes >= 0 &&
        now_minutes < site.next_routine_dispatch_eligible_minutes ) {
        plan.notes.push_back( "structural outing blocked: camp-wide routine cooldown is active" );
        return plan;
    }
    if( site.retired_empty_site ) {
        plan.notes.push_back( "structural outing blocked: retired empty site" );
        return plan;
    }
    if( site.has_active_outside_pressure() ) {
        plan.notes.push_back( "structural outing blocked: active outside group/contact blocks dogpile" );
        return plan;
    }
    if( !camp_decision_allows_dispatch( site.camp_decision,
                                        bandit_dry_run::job_template::scout ) ) {
        plan.notes.push_back( "structural outing blocked: camp mission slot is not idle" );
        return plan;
    }
    const bool structural_bounty = lead.kind == camp_lead_kind::structural_bounty;
    const bool terrain_opportunity = lead.kind == camp_lead_kind::terrain_opportunity;
    const bool signal_investigation = returned_structural_signal_lead( lead );
    if( !structural_bounty && !terrain_opportunity && !signal_investigation ) {
        plan.notes.push_back( "structural outing blocked: lead is not a routine ground candidate" );
        return plan;
    }
    if( lead.status == camp_lead_status::active || lead.status == camp_lead_status::harvested ||
        lead.status == camp_lead_status::dangerous || lead.status == camp_lead_status::invalidated ) {
        plan.notes.push_back( "structural outing blocked: lead status suppresses dispatch" );
        return plan;
    }
    if( structural_bounty && lead.bounty <= 0 ) {
        plan.notes.push_back( "structural outing blocked: no remaining structural bounty" );
        return plan;
    }
    if( signal_investigation && structural_signal_strength( lead, now_minutes ) <= 0 ) {
        plan.notes.push_back( "structural outing blocked: returned signal evidence expired" );
        return plan;
    }
    if( structural_lead_recently_checked( lead, now_minutes ) ) {
        plan.notes.push_back( "structural outing blocked: recently checked structural lead is cooling down" );
        return plan;
    }
    if( plan.effective_interest <= 0 ) {
        plan.notes.push_back( "structural outing blocked: known threat cancels bounty interest" );
        return plan;
    }
    if( hostile_camp_routine_risk_blocked( plan.static_risk ) ) {
        plan.notes.push_back( "structural outing blocked: hard risk gate" );
        return plan;
    }
    if( solve_route ) {
        plan.shared_route = make_structural_radial_route( site.anchor, lead.omt );
        plan.route_solved = true;
        if( plan.shared_route.empty() ) {
            plan.notes.push_back( "structural outing blocked: target has no bounded radial route" );
            return plan;
        }
        plan.full_route_cost = structural_route_cost( plan.shared_route );
        plan.max_route_segment_risk = plan.static_risk;
        plan.final_route_quality = structural_candidate_route_quality(
                                       plan.full_route_cost );
        plan.final_score = structural_candidate_score( site, lead, now_minutes,
                           plan.final_route_quality, plan.terrain_fit, plan.static_risk );
    }

    const routine_scout_policy_result routine_policy = routine_scout_policy( site );
    if( !routine_policy.eligible ) {
        plan.notes.push_back( "structural outing blocked: routine pair is ineligible: " +
                              routine_policy.rejection_reason );
        return plan;
    }

    plan.job = structural_bounty && lead.target_id == "forest" ?
               bandit_dry_run::job_template::scavenge :
               bandit_dry_run::job_template::scout;
    const routine_scout_pair_selection_result pair_selection = select_routine_scout_pair( site );
    if( require_exact_pair && !pair_selection.eligible ) {
        plan.notes.push_back( "structural outing blocked: " + pair_selection.rejection_reason );
        return plan;
    }
    if( require_exact_pair ) {
        plan.member_ids = pair_selection.member_ids;
    }
    plan.expected_stalking_minutes = now_minutes >= 0 ?
                                     minutes_after_saturated( now_minutes,
                                             structural_outing_stalking_delay_minutes( site, lead ) ) : -1;
    plan.expected_arrival_minutes = now_minutes >= 0 ?
                                    minutes_after_saturated( now_minutes,
                                            structural_outing_arrival_delay_minutes( site, lead ) ) : -1;
    plan.expected_return_minutes = now_minutes >= 0 ?
                                   structural_expected_return_minutes( now_minutes, site.anchor,
                                           lead.omt ) : -1;
    plan.valid = true;
    plan.notes.push_back( "structural outing candidate=" + lead.lead_id +
                          " bounty=" + std::to_string( lead.bounty ) +
                          " known_threat=" + std::to_string( plan.known_threat ) +
                          " confidence=" + std::to_string( lead.confidence ) +
                          " effective_interest=" + std::to_string( plan.effective_interest ) +
                          " terrain_fit=" + std::to_string( plan.terrain_fit ) +
                          " static_risk=" + std::to_string( plan.static_risk ) +
                          " cheap_score=" + std::to_string( plan.cheap_score ) +
                          ( solve_route ? " radial_score=" + std::to_string( plan.final_score ) :
                            " radial_score=pending" ) +
                          " decision=" + bandit_dry_run::to_string( plan.job ) );
    plan.notes.push_back( "structural outing is non-player camp routine traffic, not pursuit handoff" );
    plan.notes.push_back( "routine pair keeps local reserve=" +
                          std::to_string( routine_policy.required_local_reserve ) );
    plan.notes.push_back( "routine roles observer=" +
                          std::to_string( pair_selection.observer_id.get_value() ) +
                          " escort=" + std::to_string( pair_selection.escort_id.get_value() ) );
    return plan;
}

bool cheap_plan_precedes( const structural_outing_plan &lhs, const structural_outing_plan &rhs,
                          const site_record &site )
{
    if( lhs.cheap_score != rhs.cheap_score ) {
        return lhs.cheap_score > rhs.cheap_score;
    }
    const int lhs_distance = rl_dist( site.anchor, lhs.target_omt );
    const int rhs_distance = rl_dist( site.anchor, rhs.target_omt );
    if( lhs_distance != rhs_distance ) {
        return lhs_distance < rhs_distance;
    }
    return lhs.lead_id < rhs.lead_id;
}

std::vector<structural_outing_plan> cheap_structural_outing_candidates(
    const site_record &site, const int now_minutes, const bool require_exact_pair = true )
{
    std::vector<structural_outing_plan> candidates;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        structural_outing_plan candidate = plan_structural_bounty_outing_impl(
                site, lead, now_minutes, false, require_exact_pair );
        if( candidate.valid ) {
            candidates.push_back( std::move( candidate ) );
        }
    }
    std::sort( candidates.begin(), candidates.end(), [&site]( const structural_outing_plan &lhs,
    const structural_outing_plan &rhs ) {
        return cheap_plan_precedes( lhs, rhs, site );
    } );
    if( candidates.size() > routine_remembered_ground_candidate_cap ) {
        candidates.resize( routine_remembered_ground_candidate_cap );
    }
    return candidates;
}

bool apply_structural_route_read( const site_record &site, const int now_minutes,
                                  const structural_route_read &read,
                                  structural_outing_plan &plan )
{
    if( !plan.valid || !plan.route_solved || !read.reachable ||
        read.complete_route_cost < 0 ||
        read.complete_route_cost > max_structural_route_cost_omt ||
        !hostile_camp_routine_route_risk_eligible(
            plan.static_risk, read.max_segment_risk ) ) {
        return false;
    }

    std::optional<tripoint_abs_omt> watch_destination;
    if( read.watch_geography_supplied ) {
        const std::vector<tripoint_abs_omt> canonical_footprint =
            canonical_structural_target_footprint( read.target_footprint );
        const watch_selection_result watch_selection = select_watch_ring_candidate(
                    canonical_footprint, read.watch_candidates );
        const watch_selection_result alternate_selection =
            select_alternate_watch_ring_candidate(
                canonical_footprint, read.watch_candidates, watch_selection.omt );
        if( canonical_footprint.empty() ||
            canonical_footprint.size() > max_structural_target_footprint_omts ||
            read.watch_candidates.empty() ||
            read.watch_candidates.size() > max_structural_watch_candidates ||
            canonical_footprint != read.target_footprint ||
            std::find( canonical_footprint.begin(), canonical_footprint.end(),
                       plan.target_omt ) == canonical_footprint.end() ||
            !watch_selection.valid ||
            !structural_watch_shared_route_is_canonical(
                read.watch_shared_route, site.anchor, watch_selection.omt,
                canonical_footprint ) ||
            ( !read.alternate_watch_shared_route.empty() &&
              ( !alternate_selection.valid ||
                !structural_watch_shared_route_is_canonical(
                    read.alternate_watch_shared_route, site.anchor,
                    alternate_selection.omt, canonical_footprint ) ) ) ) {
            return false;
        }
        plan.watch_geography_supplied = true;
        plan.target_footprint = canonical_footprint;
        plan.watch_candidates = read.watch_candidates;
        plan.shared_route = read.watch_shared_route;
        plan.alternate_watch_shared_route = read.alternate_watch_shared_route;
        watch_destination = watch_selection.omt;
    } else {
        if( !read.watch_shared_route.empty() ) {
            return false;
        }
        plan.watch_geography_supplied = false;
        plan.target_footprint.clear();
        plan.watch_candidates.clear();
        plan.alternate_watch_shared_route.clear();
    }

    camp_map_lead scoring_lead;
    const camp_map_lead *lead = site.intelligence_map.find_lead( plan.lead_id );
    if( lead != nullptr ) {
        scoring_lead = *lead;
    } else if( plan.frontier_sector >= 0 ) {
        scoring_lead.lead_id = plan.lead_id;
        scoring_lead.revision = plan.lead_revision;
        scoring_lead.kind = camp_lead_kind::frontier_probe;
        scoring_lead.origin = camp_lead_origin::structural_routine;
        scoring_lead.status = camp_lead_status::suspected;
        scoring_lead.target_id = frontier_probe_target_id( plan.frontier_sector );
        scoring_lead.omt = plan.target_omt;
        scoring_lead.source_key = scoring_lead.target_id;
        scoring_lead.last_checked_minutes = plan.frontier_prior_resolved_minutes;
        scoring_lead.last_scouted_minutes = plan.frontier_prior_resolved_minutes;
    } else {
        return false;
    }

    plan.full_route_cost = read.complete_route_cost;
    plan.max_route_segment_risk = read.max_segment_risk;
    plan.final_route_quality = structural_candidate_route_quality( plan.full_route_cost );
    plan.final_score = structural_candidate_score( site, scoring_lead, now_minutes,
                       plan.final_route_quality, plan.terrain_fit, plan.static_risk );
    const tripoint_abs_omt travel_destination = watch_destination.value_or( plan.target_omt );
    plan.expected_stalking_minutes = minutes_after_saturated(
                                        now_minutes,
                                        structural_stalking_delay_minutes(
                                            site.anchor, travel_destination ) );
    plan.expected_arrival_minutes = minutes_after_saturated(
                                       now_minutes,
                                       structural_arrival_delay_minutes(
                                           site.anchor, travel_destination ) );
    plan.expected_return_minutes = structural_expected_return_minutes(
                                       now_minutes, site.anchor, travel_destination );
    if( !read.summary.empty() ) {
        plan.notes.push_back( read.summary );
    }
    return true;
}
} // namespace

structural_outing_plan plan_frontier_outing( const site_record &site,
        const int now_minutes )
{
    structural_outing_plan plan = plan_frontier_outing_impl( site, now_minutes, true );
    const structural_route_read read{ plan.route_solved, plan.full_route_cost,
                                      plan.max_route_segment_risk, "deterministic frontier route" };
    if( plan.valid && !apply_structural_route_read( site, now_minutes, read, plan ) ) {
        plan.valid = false;
    }
    return plan;
}

structural_outing_plan plan_structural_bounty_outing( const site_record &site,
        const camp_map_lead &lead, const int now_minutes )
{
    return plan_structural_bounty_outing_impl( site, lead, now_minutes, true );
}

structural_outing_plan plan_structural_bounty_outing( const site_record &site, const int now_minutes )
{
    const std::vector<structural_outing_plan> cheap_candidates =
        cheap_structural_outing_candidates( site, now_minutes );
    structural_outing_plan best;
    int route_solves = 0;
    for( const structural_outing_plan &cheap : cheap_candidates ) {
        if( route_solves >= routine_candidate_full_route_solve_cap ) {
            break;
        }
        const camp_map_lead *lead = site.intelligence_map.find_lead( cheap.lead_id );
        if( lead == nullptr ) {
            continue;
        }
        route_solves++;
        structural_outing_plan candidate = plan_structural_bounty_outing_impl(
                site, *lead, now_minutes, true );
        const structural_route_read read{ candidate.route_solved, candidate.full_route_cost,
                                          candidate.max_route_segment_risk,
                                          "deterministic structural route" };
        if( candidate.valid && !apply_structural_route_read( site, now_minutes, read, candidate ) ) {
            candidate.valid = false;
        }
        if( !candidate.valid ||
            !hostile_camp_routine_score_eligible( candidate.final_score, false ) ) {
            continue;
        }
        if( !best.valid || candidate.final_score > best.final_score ||
            ( candidate.final_score == best.final_score &&
              cheap_plan_precedes( candidate, best, site ) ) ) {
            best = std::move( candidate );
        }
    }
    if( !best.valid ) {
        best.site_id = site.site_id;
        best.notes.push_back( "structural outing planner found no routed score-eligible lead" );
    }
    return best;
}

bool apply_structural_bounty_outing_plan( site_record &site, const structural_outing_plan &plan,
        const int now_minutes )
{
    const bool frontier_plan = plan.frontier_sector >= 0;
    const watch_selection_result watch_selection = plan.watch_geography_supplied ?
            select_watch_ring_candidate( plan.target_footprint, plan.watch_candidates ) :
            watch_selection_result();
    const bool has_watch_route = plan.watch_geography_supplied && watch_selection.valid;
    const bool route_is_canonical = frontier_plan ?
                                    !has_watch_route && frontier_route_is_canonical(
                                        plan.shared_route, site.anchor, plan.frontier_sector ) :
                                    has_watch_route ? structural_watch_shared_route_is_canonical(
                                        plan.shared_route, site.anchor, watch_selection.omt,
                                        plan.target_footprint ) :
                                    structural_route_is_canonical( plan.shared_route, site.anchor,
                                            plan.target_omt );
    const tripoint_abs_omt travel_destination = has_watch_route ?
            watch_selection.omt : plan.target_omt;
    const bool route_risk_is_valid = hostile_camp_routine_route_risk_eligible(
                                         plan.static_risk, plan.max_route_segment_risk );
    if( now_minutes < 0 || !plan.valid || !plan.route_solved ||
        plan.full_route_cost < 0 || plan.full_route_cost > max_structural_route_cost_omt ||
        !route_risk_is_valid ||
        !hostile_camp_routine_score_eligible( plan.final_score, false ) ||
        hostile_camp_routine_risk_blocked( plan.static_risk ) ||
        plan.site_id != site.site_id ||
        site.routine_activated_minutes > now_minutes ||
        ( site.next_routine_dispatch_eligible_minutes >= 0 &&
          now_minutes < site.next_routine_dispatch_eligible_minutes ) ||
        plan.activity_id != site.site_id + "#structural" ||
        plan.generation <= 0 || plan.generation != site.next_outing_generation ||
        plan.lead_id.empty() ||
        plan.lead_revision <= 0 || plan.member_ids.empty() ||
        plan.member_ids.size() > max_active_outing_members ||
        !route_is_canonical ||
        ( !has_watch_route &&
          plan.target_omt != plan.shared_route[plan.shared_route.size() - 2] ) ||
        plan.expected_stalking_minutes != minutes_after_saturated(
            now_minutes, structural_stalking_delay_minutes( site.anchor, travel_destination ) ) ||
        plan.expected_arrival_minutes != minutes_after_saturated(
            now_minutes, structural_arrival_delay_minutes( site.anchor, travel_destination ) ) ||
        plan.expected_return_minutes != structural_expected_return_minutes(
            now_minutes, site.anchor, travel_destination ) ) {
        return false;
    }
    if( site.has_active_outside_pressure() ) {
        return false;
    }
    if( !camp_decision_allows_dispatch( site.camp_decision,
                                        bandit_dry_run::job_template::scout ) ) {
        return false;
    }
    camp_map_lead *lead = site.intelligence_map.find_lead( plan.lead_id );
    if( frontier_plan ) {
        structural_outing_plan expected = plan_frontier_outing_impl( site, now_minutes, true );
        const structural_route_read persisted_route{ true, plan.full_route_cost,
                plan.max_route_segment_risk, "", plan.watch_geography_supplied,
                plan.target_footprint, plan.watch_candidates,
                plan.watch_geography_supplied ? plan.shared_route :
                std::vector<tripoint_abs_omt>(),
                plan.alternate_watch_shared_route };
        if( expected.valid && !apply_structural_route_read(
                site, now_minutes, persisted_route, expected ) ) {
            expected.valid = false;
        }
        const bool lead_matches = lead == nullptr ? plan.lead_revision == 1 :
                                  ( lead->revision == plan.lead_revision &&
                                    frontier_sector_from_lead( *lead ) == plan.frontier_sector &&
                                    lead->omt == plan.target_omt );
        if( !expected.valid || !lead_matches || plan.job != bandit_dry_run::job_template::scout ||
            plan.frontier_cursor != expected.frontier_cursor ||
            plan.frontier_prior_resolved_minutes != expected.frontier_prior_resolved_minutes ||
            plan.frontier_sector != expected.frontier_sector ||
            plan.lead_id != expected.lead_id || plan.lead_revision != expected.lead_revision ||
            plan.member_ids != expected.member_ids || plan.shared_route != expected.shared_route ||
            plan.cheap_score != expected.cheap_score || plan.final_score != expected.final_score ||
            plan.static_risk != expected.static_risk || plan.terrain_fit != expected.terrain_fit ||
            plan.final_route_quality != expected.final_route_quality ||
            plan.watch_geography_supplied != expected.watch_geography_supplied ||
            plan.target_footprint != expected.target_footprint ||
            plan.watch_candidates != expected.watch_candidates ||
            plan.alternate_watch_shared_route !=
            expected.alternate_watch_shared_route ||
            plan.expected_stalking_minutes != expected.expected_stalking_minutes ||
            plan.expected_arrival_minutes != expected.expected_arrival_minutes ||
            plan.expected_return_minutes != expected.expected_return_minutes ) {
            return false;
        }
    } else {
        if( lead == nullptr || lead->revision != plan.lead_revision ||
            lead->omt != plan.target_omt ||
            lead->revision >= std::numeric_limits<int>::max() ||
            ( lead->kind != camp_lead_kind::structural_bounty &&
              lead->kind != camp_lead_kind::terrain_opportunity &&
              !returned_structural_signal_lead( *lead ) ) ||
            ( lead->kind == camp_lead_kind::structural_bounty && lead->bounty <= 0 ) ||
            lead->status == camp_lead_status::active ||
            lead->status == camp_lead_status::harvested ||
            lead->status == camp_lead_status::dangerous ||
            lead->status == camp_lead_status::invalidated ||
            structural_lead_recently_checked( *lead, now_minutes ) ) {
            return false;
        }
        structural_outing_plan expected = plan_structural_bounty_outing_impl(
                    site, *lead, now_minutes, true );
        const structural_route_read persisted_route{ true, plan.full_route_cost,
                plan.max_route_segment_risk, "", plan.watch_geography_supplied,
                plan.target_footprint, plan.watch_candidates,
                plan.watch_geography_supplied ? plan.shared_route :
                std::vector<tripoint_abs_omt>(),
                plan.alternate_watch_shared_route };
        if( expected.valid && !apply_structural_route_read(
                site, now_minutes, persisted_route, expected ) ) {
            expected.valid = false;
        }
        if( !expected.valid || plan.job != expected.job ||
            plan.member_ids != expected.member_ids || plan.shared_route != expected.shared_route ||
            plan.cheap_score != expected.cheap_score || plan.final_score != expected.final_score ||
            plan.static_risk != expected.static_risk || plan.terrain_fit != expected.terrain_fit ||
            plan.final_route_quality != expected.final_route_quality ||
            plan.watch_geography_supplied != expected.watch_geography_supplied ||
            plan.target_footprint != expected.target_footprint ||
            plan.watch_candidates != expected.watch_candidates ||
            plan.alternate_watch_shared_route !=
            expected.alternate_watch_shared_route ||
            plan.expected_stalking_minutes != expected.expected_stalking_minutes ||
            plan.expected_arrival_minutes != expected.expected_arrival_minutes ||
            plan.expected_return_minutes != expected.expected_return_minutes ||
            structural_effective_interest( *lead,
                                            structural_known_threat_for_interest( *lead ) ) <= 0 ) {
            return false;
        }
    }
    const routine_scout_policy_result routine_policy = routine_scout_policy( site );
    const routine_scout_pair_selection_result pair_selection = select_routine_scout_pair( site );
    if( !routine_policy.eligible ||
        !pair_selection.eligible || plan.member_ids != pair_selection.member_ids ) {
        return false;
    }
    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( plan.member_ids.size() );
    for( const character_id &member_id : plan.member_ids ) {
        if( std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home || member->wounded_or_unready ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    site_record candidate = site;
    if( candidate.routine_activated_minutes < 0 ) {
        candidate.routine_activated_minutes = now_minutes;
    }
    camp_map_lead *candidate_lead = candidate.intelligence_map.find_lead( plan.lead_id );
    if( frontier_plan && candidate_lead == nullptr ) {
        camp_map_lead frontier_lead;
        frontier_lead.lead_id = plan.lead_id;
        frontier_lead.revision = plan.lead_revision;
        frontier_lead.kind = camp_lead_kind::frontier_probe;
        frontier_lead.origin = camp_lead_origin::structural_routine;
        frontier_lead.status = camp_lead_status::suspected;
        frontier_lead.target_id = frontier_probe_target_id( plan.frontier_sector );
        frontier_lead.omt = plan.target_omt;
        frontier_lead.radius_omt = 0;
        frontier_lead.source_key = frontier_probe_target_id( plan.frontier_sector );
        frontier_lead.source_summary = "camp-local least-recent frontier sector";
        frontier_lead.first_seen_minutes = now_minutes;
        frontier_lead.last_seen_minutes = now_minutes;
        candidate.intelligence_map.leads.push_back( std::move( frontier_lead ) );
        candidate_lead = candidate.intelligence_map.find_lead( plan.lead_id );
    }
    if( candidate_lead == nullptr ) {
        return false;
    }
    const std::string summary = "structural " + bandit_dry_run::to_string( plan.job ) +
                                " outing toward " + plan.lead_id;
    for( const character_id &member_id : plan.member_ids ) {
        if( !update_member_state( candidate, member_id, member_state::outbound, summary ) ) {
            return false;
        }
    }
    candidate.active_outing.clear();
    candidate.active_outing.schema_version = 9;
    candidate.active_outing.kind = outing_kind::structural_sortie;
    candidate.active_outing.activity_id = plan.activity_id;
    candidate.active_outing.camp_id = candidate.site_id;
    candidate.active_outing.generation = plan.generation;
    candidate.next_outing_generation++;
    candidate.active_outing.member_ids = plan.member_ids;
    candidate.active_outing.leader_id = plan.member_ids.front();
    candidate.active_outing.shared_route = plan.shared_route;
    candidate.active_outing.waypoint_index = 0;
    candidate.active_outing.phase = scout_phase::outbound;
    candidate.active_outing.owner = simulation_owner::abstract;
    candidate.active_outing.last_advanced_minutes = now_minutes;
    candidate.active_outing.target_id = plan.lead_id;
    candidate.active_outing.target_omt = plan.target_omt;
    candidate.active_outing.target_footprint = { plan.target_omt };
    candidate.active_outing.target_lead_id = plan.lead_id;
    candidate.active_outing.target_lead_revision = plan.lead_revision;
    candidate.active_outing.job_type = bandit_dry_run::to_string( plan.job );
    candidate.active_outing.started_minutes = now_minutes;
    candidate.active_outing.local_contact_minutes = -1;
    candidate.active_outing.last_progress_minutes = now_minutes;
    candidate.active_outing.expected_return_minutes = plan.expected_return_minutes;
    candidate.active_outing.missing_deadline_minutes = minutes_after_saturated(
            plan.expected_return_minutes, scout_missing_grace_minutes );
    candidate.active_outing.return_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            candidate.active_outing.activity_id, candidate.active_outing.generation, "return" );
    candidate.active_outing.report_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            candidate.active_outing.activity_id, candidate.active_outing.generation, "report" );
    candidate.active_outing.cargo_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            candidate.active_outing.activity_id, candidate.active_outing.generation, "cargo" );
    if( plan.watch_geography_supplied ) {
        const std::optional<simulation_advance_cursor> watch_cursor =
            current_external_simulation_cursor( candidate );
        if( !watch_cursor || apply_structural_watch_route_selection(
                candidate, *watch_cursor, plan.target_footprint,
                plan.watch_candidates,
                plan.alternate_watch_shared_route ) !=
            structural_watch_route_apply_result::applied ) {
            return false;
        }
        candidate.active_outing.schema_version = 10;
    }
    if( candidate.intelligence_map.last_routine_target_lead_id != plan.lead_id ) {
        candidate.intelligence_map.previous_routine_target_lead_id =
            candidate.intelligence_map.last_routine_target_lead_id;
        candidate.intelligence_map.last_routine_target_lead_id = plan.lead_id;
    }
    candidate_lead->status = camp_lead_status::active;
    candidate_lead->last_outcome = "structural_outing_active";
    advance_camp_map_lead_revision( candidate, *candidate_lead );
    if( frontier_plan ) {
        normalize_camp_intelligence( candidate );
    }
    if( !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

namespace
{
struct structural_resource_arrival_receipt {
    int claimed_units = 0;
    int remaining_units = 0;
};

std::optional<structural_resource_arrival_receipt> apply_structural_resource_arrival(
    world_state &candidate_state, const std::size_t site_index, const int now_minutes )
{
    if( site_index >= candidate_state.sites.size() || now_minutes < 0 ) {
        return std::nullopt;
    }
    site_record &site = candidate_state.sites[site_index];
    active_outing_state &outing = site.active_outing;
    camp_map_lead *lead = site.intelligence_map.find_lead( outing.target_id );
    const int survivor_count = std::min( max_finite_resource_claim_units,
                                        site.active_outing_survivor_count() );
    if( outing.kind != outing_kind::structural_sortie ||
        outing.phase != scout_phase::observing ||
        lead == nullptr || lead->kind != camp_lead_kind::structural_bounty ||
        lead->omt != outing.target_omt || survivor_count <= 0 ||
        outing.cargo.supply_units != 0 || outing.cargo.trade_value != 0 ||
        lead->times_harvested >= std::numeric_limits<int>::max() ||
        lead->times_checked_empty >= std::numeric_limits<int>::max() ) {
        return std::nullopt;
    }

    const finite_resource_record expected = finite_resource_snapshot(
            candidate_state, outing.target_omt, lead->bounty );
    const std::string application_key = finite_resource_claim_application_key(
                                            outing.activity_id, outing.generation,
                                            outing.target_omt );
    const finite_resource_claim_result claim = claim_finite_resource_units(
                candidate_state, site.site_id, outing.target_omt, expected,
                survivor_count, outing.activity_id, outing.generation, application_key );
    if( claim.status != finite_resource_claim_status::applied &&
        claim.status != finite_resource_claim_status::depleted ) {
        return std::nullopt;
    }
    if( ( claim.status == finite_resource_claim_status::applied &&
          ( claim.claimed_units <= 0 || claim.claimed_units > survivor_count ) ) ||
        ( claim.status == finite_resource_claim_status::depleted && claim.claimed_units != 0 ) ) {
        return std::nullopt;
    }

    site_record &claimed_site = candidate_state.sites[site_index];
    camp_map_lead *claimed_lead = claimed_site.intelligence_map.find_lead( outing.target_id );
    if( claimed_lead == nullptr ) {
        return std::nullopt;
    }
    if( claim.claimed_units > 0 ) {
        claimed_lead->times_harvested++;
    } else {
        claimed_lead->times_checked_empty++;
    }
    if( !record_camp_resource_estimate( claimed_site, claimed_lead->lead_id,
                                        claim.remaining_units, max_finite_resource_units,
                                        now_minutes ) ) {
        return std::nullopt;
    }

    active_outing_state &claimed_outing = claimed_site.active_outing;
    claimed_outing.cargo.supply_units = 2 * claim.claimed_units;
    claimed_outing.waypoint_index = structural_outing_destination_waypoint( claimed_outing );
    claimed_outing.phase = scout_phase::returning_home;
    claimed_outing.last_progress_minutes = now_minutes;
    camp_map_lead *updated_lead = claimed_site.intelligence_map.find_lead(
                                      claimed_outing.target_id );
    if( updated_lead == nullptr ) {
        return std::nullopt;
    }
    if( claim.claimed_units > 0 && claim.remaining_units == 0 ) {
        updated_lead->last_outcome = "harvested_structural_bounty";
    }
    return structural_resource_arrival_receipt { claim.claimed_units,
            claim.remaining_units };
}

bool credit_structural_return_cargo( site_record &candidate, const int now_minutes )
{
    active_outing_state &outing = candidate.active_outing;
    if( outing.kind != outing_kind::structural_sortie || now_minutes < 0 ||
        ( candidate.supply_last_update_minutes >= 0 &&
          now_minutes < candidate.supply_last_update_minutes ) ||
        outing.cargo.supply_units < 0 || outing.cargo.trade_value != 0 ) {
        return false;
    }
    advance_camp_supply( candidate, now_minutes );
    if( outing.cargo.supply_units == 0 ) {
        return true;
    }
    const std::string expected_key = bandit_pursuit_handoff::make_operation_component_key(
                                         outing.activity_id, outing.generation, "cargo" );
    if( outing.generation <= candidate.applied_cargo_generation ||
        outing.cargo_application_key != expected_key ) {
        return false;
    }
    const long long credited = static_cast<long long>( candidate.supply_units ) +
                               outing.cargo.supply_units;
    candidate.supply_units = static_cast<int>( std::min<long long>(
                                 camp_supply_cap( candidate ), credited ) );
    candidate.applied_cargo_generation = outing.generation;
    candidate.last_cargo_application_key = outing.cargo_application_key;
    return true;
}

bool make_static_structural_observer_request( const site_record &site,
        structural_threat_observer_request &request )
{
    const active_outing_state &outing = site.active_outing;
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version < 8 ||
        outing.activity_id != site.site_id + "#structural" || outing.target_id.empty() ||
        outing.target_id != outing.target_lead_id || outing.target_lead_revision <= 0 ||
        outing.member_ids.size() != 2 ||
        ( outing.phase != scout_phase::outbound && outing.phase != scout_phase::observing ) ||
        outing.shared_route.size() < 3 || outing.waypoint_index < 0 ||
        static_cast<std::size_t>( outing.waypoint_index ) >= outing.shared_route.size() - 1 ) {
        return false;
    }
    const camp_map_lead *lead = site.intelligence_map.find_lead( outing.target_id );
    const std::optional<int> frontier_sector = lead != nullptr ?
            frontier_sector_from_lead( *lead ) : std::nullopt;
    if( lead == nullptr ||
        ( lead->kind != camp_lead_kind::structural_bounty &&
          lead->kind != camp_lead_kind::terrain_opportunity &&
          !returned_structural_signal_lead( *lead ) && !frontier_sector ) ||
        lead->revision != outing.target_lead_revision ||
        !structural_route_is_canonical_for_outing( outing, site.anchor, *lead ) ) {
        return false;
    }

    request.current_omt = outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
    request.observation_window_start_minutes = outing.last_advanced_minutes;
    request.party_power = structural_outing_party_power( site );
    const int target_index = structural_outing_destination_waypoint( outing );
    for( int index = outing.waypoint_index + 1;
         index <= target_index && request.visible_forward_omts.size() < 3; ++index ) {
        request.visible_forward_omts.push_back(
            outing.shared_route[static_cast<std::size_t>( index )] );
    }
    return request.party_power > 0;
}
} // namespace

bool structural_local_zombie_candidate_is_eligible( const bool alive,
        const bool hallucination, const bool zombie_species, const bool zombie_rider,
        const bool hostile, const bool visible, const bool source_on_route )
{
    return alive && !hallucination && zombie_species && !zombie_rider && hostile && visible &&
           source_on_route;
}

sortie_observation_effect record_structural_local_zombie_observation(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const structural_local_zombie_read &read, const int now_minutes )
{
    if( !structural_local_zombie_read_is_valid( site, expected_cursor, read, now_minutes ) ) {
        return {};
    }
    const sortie_observation observation = make_structural_local_zombie_observation(
                site, read, now_minutes );
    return record_active_typed_observations( site, expected_cursor, read.observer_id,
            site.active_outing.target_lead_revision, { observation }, now_minutes );
}

structural_signal_record_result record_structural_signal_observations( world_state &state,
        const int now_minutes,
        const std::function<std::vector<structural_signal_read>( const site_record &,
                const active_outing_state &,
                const structural_threat_observer_request & )> &signal_lookup )
{
    structural_signal_record_result result;
    if( !signal_lookup || now_minutes < 0 ) {
        return result;
    }
    for( site_record &site : state.sites ) {
        result.sites_considered++;
        structural_threat_observer_request request;
        if( !make_static_structural_observer_request( site, request ) ) {
            continue;
        }
        result.active_outings_considered++;
        const std::optional<simulation_advance_cursor> expected_cursor =
            current_external_simulation_cursor( site );
        if( !expected_cursor || expected_cursor->owner != simulation_owner::abstract ||
            now_minutes <= expected_cursor->last_advanced_minutes ) {
            continue;
        }

        result.callbacks_invoked++;
        std::vector<structural_signal_read> reads = signal_lookup(
                    site, site.active_outing, request );
        if( reads.empty() || !structural_signal_reads_are_valid( request, reads, now_minutes ) ) {
            continue;
        }
        std::vector<sortie_observation> observations = make_structural_signal_observations(
                    site, request, std::move( reads ), now_minutes );
        if( observations.empty() ) {
            continue;
        }

        site_record candidate = site;
        const sortie_observation_effect recorded = record_active_typed_observations(
                    candidate, *expected_cursor, candidate.active_outing.leader_id,
                    candidate.active_outing.target_lead_revision, observations, now_minutes );
        if( !recorded.valid ) {
            continue;
        }
        result.sites_recorded++;
        result.facts_recorded += recorded.inserted + recorded.replaced;
        site = std::move( candidate );
    }
    return result;
}

static bool deliver_structural_scout_assessment_report( site_record &site,
        const int delivered_minutes, const std::vector<character_id> &carrier_ids )
{
    const active_outing_state &outing = site.active_outing;
    if( outing.kind != outing_kind::structural_sortie || outing.job_type != "scout" ||
        outing.assessment.exit_reason.empty() || delivered_minutes < 0 ) {
        return false;
    }
    if( site.current_scout_report.is_present() &&
        site.current_scout_report.source_activity_id == outing.activity_id &&
        site.current_scout_report.source_generation == outing.generation ) {
        return site.current_scout_report.assessment.exit_reason ==
               outing.assessment.exit_reason;
    }
    const std::optional<int> revision = next_scout_report_revision( site );
    const camp_map_lead *lead = site.intelligence_map.find_lead(
                                    outing.target_lead_id );
    if( !revision || lead == nullptr || lead->omt != outing.target_omt ||
        lead->target_id.empty() ) {
        return false;
    }
    scout_report_record report;
    report.revision = *revision;
    report.action_policy = report_policy_for_profile( effective_profile( site ) );
    report.source_activity_id = outing.activity_id;
    report.source_generation = outing.generation;
    report.source_job_type = outing.job_type;
    report.target_id = lead->target_id;
    report.target_omt = lead->omt;
    report.target_lead_id = outing.target_lead_id;
    report.target_lead_revision = outing.target_lead_revision;
    report.application_key = outing.report_application_key;
    report.observations = make_reportable_sortie_observations(
                              outing.observations, carrier_ids );
    report.assessment = outing.assessment;
    report.casualty_ids = outing.casualty_ids;
    report.delivered_minutes = delivered_minutes;
    site.current_scout_report = std::move( report );
    site.applied_report_generation = std::max( site.applied_report_generation,
                                     outing.generation );
    return true;
}

structural_outing_result advance_structural_bounty_outings( world_state &state, const int now_minutes,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup,
        const std::function<abstract_threat_read( const site_record &, const active_outing_state &,
                const structural_threat_observer_request & )> &abstract_threat_lookup,
        const std::function<std::vector<structural_signal_read>( const site_record &,
                const active_outing_state &,
                const structural_threat_observer_request & )> &signal_lookup )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_outings );
    structural_outing_result result;
    for( std::size_t site_index = 0; site_index < state.sites.size(); ++site_index ) {
        site_record &site = state.sites[site_index];
        result.sites_considered++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_outing_sites_considered );
        bandit_live_world_probe::record_site_service( site.site_id,
                bandit_live_world_probe::site_service::outing_considered );
        if( site.active_outing.kind != outing_kind::structural_sortie ||
            ( site.active_outing.schema_version != 6 &&
              site.active_outing.schema_version != 7 &&
              site.active_outing.schema_version != 8 &&
              site.active_outing.schema_version != 9 &&
              site.active_outing.schema_version != 10 ) ||
            site.active_outing.activity_id != site.site_id + "#structural" ||
            site.active_outing.target_id.empty() ||
            site.active_outing.target_id != site.active_outing.target_lead_id ||
            site.active_outing.member_ids.size() != 2 ||
            site.active_outing.started_minutes < 0 ||
            site.active_outing.expected_return_minutes != structural_expected_return_minutes(
                site.active_outing.started_minutes, site.anchor,
                structural_outing_travel_destination( site.active_outing ) ) ) {
            continue;
        }
        const camp_map_lead *current_lead = site.intelligence_map.find_lead(
                                                site.active_outing.target_id );
        const std::optional<int> current_frontier_sector = current_lead != nullptr ?
                frontier_sector_from_lead( *current_lead ) : std::nullopt;
        const bool current_lead_kind_is_supported = current_lead != nullptr &&
                ( current_lead->kind == camp_lead_kind::structural_bounty ||
                  current_lead->kind == camp_lead_kind::terrain_opportunity ||
                  returned_structural_signal_lead( *current_lead ) ||
                  current_frontier_sector.has_value() );
        const bool current_job_is_consistent = current_lead != nullptr &&
                ( ( current_lead->kind == camp_lead_kind::structural_bounty &&
                    site.active_outing.job_type == ( current_lead->target_id == "forest" ?
                                                     "scavenge" : "scout" ) ) ||
                  ( current_lead->kind == camp_lead_kind::terrain_opportunity &&
                    site.active_outing.job_type == "scout" ) ||
                  ( returned_structural_signal_lead( *current_lead ) &&
                    site.active_outing.job_type == "scout" ) ||
                  ( current_frontier_sector && site.active_outing.job_type == "scout" ) );
        if( current_lead_kind_is_supported &&
            ( !current_job_is_consistent ||
              current_lead->omt != site.active_outing.target_omt ||
              !structural_route_is_canonical_for_outing( site.active_outing,
                      site.anchor, *current_lead ) ||
              ( current_frontier_sector &&
                !frontier_memory_is_valid( site.intelligence_map ) ) ) ) {
            continue;
        }
        const std::string expected_activity_id = site.active_outing.activity_id;
        const int expected_generation = site.active_outing.generation;
        result.active_outings_considered++;

        site_record candidate = site;
        const std::optional<simulation_advance_cursor> expected_cursor =
            current_external_simulation_cursor( candidate );
        if( !expected_cursor || expected_cursor->owner != simulation_owner::abstract ) {
            continue;
        }
        camp_map_lead *lead = candidate.intelligence_map.find_lead(
                                  candidate.active_outing.target_id );
        const std::optional<int> frontier_sector = lead != nullptr ?
                frontier_sector_from_lead( *lead ) : std::nullopt;
        if( lead == nullptr ||
            ( lead->kind != camp_lead_kind::structural_bounty &&
              lead->kind != camp_lead_kind::terrain_opportunity &&
              !returned_structural_signal_lead( *lead ) && !frontier_sector ) ) {
            const simulation_owner_transition_result advance = advance_external_simulation(
                        candidate, expected_cursor->activity_id, expected_cursor->generation,
                        expected_cursor->owner, expected_cursor->handoff_epoch,
                        expected_cursor->last_advanced_minutes,
                        expected_cursor->covert_egress_revision, now_minutes );
            if( advance != simulation_owner_transition_result::applied ) {
                continue;
            }
            if( release_structural_outing_reservation(
                    candidate, expected_activity_id, expected_generation,
                    "structural outing cleared missing structural lead" ) ) {
                site = std::move( candidate );
                result.notes.push_back( "structural outing cleared: active target lead was missing" );
            }
            continue;
        }
        if( lead->revision >= std::numeric_limits<int>::max() ) {
            const simulation_owner_transition_result advance = advance_external_simulation(
                        candidate, expected_cursor->activity_id, expected_cursor->generation,
                        expected_cursor->owner, expected_cursor->handoff_epoch,
                        expected_cursor->last_advanced_minutes,
                        expected_cursor->covert_egress_revision, now_minutes );
            if( advance != simulation_owner_transition_result::applied ) {
                continue;
            }
            if( release_structural_outing_reservation(
                    candidate, expected_activity_id, expected_generation,
                    "structural outing cleared immutable terminal lead revision" ) ) {
                site = std::move( candidate );
                result.notes.push_back(
                    "structural outing cleared: target lead revision cannot advance safely" );
            }
            continue;
        }

        active_outing_state &pre_advance_outing = candidate.active_outing;
        pre_advance_outing.schema_version = std::max( pre_advance_outing.schema_version, 8 );
        bool simulation_advance_consumed = false;
        bool waypoint_progressed = false;
        std::optional<std::pair<structural_threat_observer_request, abstract_threat_read>>
        deferred_abstract_input;
        if( ( abstract_threat_lookup || signal_lookup ) &&
            ( pre_advance_outing.phase == scout_phase::outbound ||
              pre_advance_outing.phase == scout_phase::observing ) ) {
            if( pre_advance_outing.phase == scout_phase::outbound &&
                pre_advance_outing.waypoint_index == 0 &&
                now_minutes - pre_advance_outing.started_minutes >=
                active_structural_outing_stalking_delay_minutes( candidate ) &&
                pre_advance_outing.shared_route.size() >= 3 ) {
                pre_advance_outing.waypoint_index = 1;
                waypoint_progressed = true;
            }
            if( pre_advance_outing.phase == scout_phase::observing &&
                pre_advance_outing.local_contact_minutes >= 0 &&
                now_minutes - pre_advance_outing.started_minutes >=
                active_structural_outing_arrival_delay_minutes( candidate ) &&
                pre_advance_outing.waypoint_index !=
                structural_outing_destination_waypoint( pre_advance_outing ) ) {
                pre_advance_outing.waypoint_index =
                    structural_outing_destination_waypoint( pre_advance_outing );
                waypoint_progressed = true;
            }
            structural_threat_observer_request request;
            request.current_omt = pre_advance_outing.shared_route[static_cast<std::size_t>(
                                      pre_advance_outing.waypoint_index )];
            request.observation_window_start_minutes = waypoint_progressed ? now_minutes :
                    expected_cursor->last_advanced_minutes;
            request.party_power = structural_outing_party_power( candidate );
            const int target_index = structural_outing_destination_waypoint(
                                         pre_advance_outing );
            for( int index = pre_advance_outing.waypoint_index + 1;
                 index <= target_index && request.visible_forward_omts.size() < 3; ++index ) {
                request.visible_forward_omts.push_back(
                    pre_advance_outing.shared_route[static_cast<std::size_t>( index )] );
            }
            if( pre_advance_outing.selected_watch_kind != structural_watch_kind::none &&
                request.current_omt == pre_advance_outing.selected_watch_omt ) {
                for( const tripoint_abs_omt &target_omt :
                     pre_advance_outing.target_footprint ) {
                    if( request.visible_forward_omts.size() >= 3 ) {
                        break;
                    }
                    if( target_omt.z() == request.current_omt.z() &&
                        std::find( request.visible_forward_omts.begin(),
                                   request.visible_forward_omts.end(), target_omt ) ==
                        request.visible_forward_omts.end() ) {
                        request.visible_forward_omts.push_back( target_omt );
                    }
                }
            }
            add_structural_observer_retained_track( candidate, request, now_minutes );
            abstract_threat_read abstract_read;
            if( abstract_threat_lookup ) {
                abstract_read = abstract_threat_lookup( candidate, pre_advance_outing, request );
                bool threat_omt_is_permitted = !abstract_read.observed;
                if( abstract_read.observed ) {
                    threat_omt_is_permitted = abstract_read.threat_omt == request.current_omt ||
                                              std::find( request.visible_forward_omts.begin(),
                                                      request.visible_forward_omts.end(),
                                                      abstract_read.threat_omt ) !=
                                              request.visible_forward_omts.end();
                }
                if( threat_omt_is_permitted &&
                    structural_abstract_threat_read_is_valid( request.current_omt, abstract_read ) ) {
                    deferred_abstract_input = std::make_pair( request, abstract_read );
                }
            }

            std::vector<sortie_observation> observations;
            if( signal_lookup && request.party_power > 0 ) {
                std::vector<structural_signal_read> signal_reads = signal_lookup(
                            candidate, pre_advance_outing, request );
                if( structural_signal_reads_are_valid( request, signal_reads, now_minutes ) ) {
                    observations = make_structural_signal_observations(
                                       candidate, request, std::move( signal_reads ), now_minutes );
                }
            }
            const bool records_visual_threat = deferred_abstract_input && abstract_read.observed &&
                                               !abstract_read.local_reality &&
                                               request.party_power > 0;
            if( records_visual_threat ) {
                observations.push_back( make_structural_threat_observation(
                                            candidate, request, abstract_read, now_minutes ) );
            }
            if( !observations.empty() ) {
                const sortie_observation_effect recorded = record_active_typed_observations(
                            candidate, *expected_cursor, pre_advance_outing.leader_id,
                            pre_advance_outing.target_lead_revision, observations, now_minutes );
                if( !recorded.valid ) {
                    if( records_visual_threat ) {
                        deferred_abstract_input.reset();
                    }
                } else {
                    simulation_advance_consumed = true;
                    if( waypoint_progressed ) {
                        candidate.active_outing.last_progress_minutes = now_minutes;
                    }
                    if( records_visual_threat ) {
                        const abstract_threat_resolution resolution =
                            resolve_structural_abstract_threat(
                                candidate, request.current_omt, abstract_read, now_minutes );
                        if( !resolution.valid ) {
                            continue;
                        }
                        result.notes.insert( result.notes.end(), resolution.notes.begin(),
                                             resolution.notes.end() );
                        if( resolution.changed ||
                            resolution.kind == abstract_threat_resolution_kind::deferred_to_local ||
                            resolution.kind == abstract_threat_resolution_kind::withdrawal ||
                            resolution.kind == abstract_threat_resolution_kind::wounded_pair ||
                            resolution.kind == abstract_threat_resolution_kind::one_missing ||
                            resolution.kind == abstract_threat_resolution_kind::all_missing ) {
                            site = std::move( candidate );
                            continue;
                        }
                        deferred_abstract_input.reset();
                    }
                }
            }
        }

        if( !simulation_advance_consumed ) {
            const simulation_owner_transition_result advance = advance_external_simulation(
                        candidate, expected_cursor->activity_id, expected_cursor->generation,
                        expected_cursor->owner, expected_cursor->handoff_epoch,
                        expected_cursor->last_advanced_minutes,
                        expected_cursor->covert_egress_revision, now_minutes );
            if( advance != simulation_owner_transition_result::applied ) {
                continue;
            }
            if( waypoint_progressed ) {
                candidate.active_outing.last_progress_minutes = now_minutes;
            }
        }
        lead = candidate.intelligence_map.find_lead( candidate.active_outing.target_id );
        if( lead == nullptr ) {
            continue;
        }
        active_outing_state &outing = candidate.active_outing;
        if( deferred_abstract_input ) {
            const abstract_threat_resolution resolution = resolve_structural_abstract_threat(
                        candidate, deferred_abstract_input->first.current_omt,
                        deferred_abstract_input->second, now_minutes );
            if( resolution.valid && resolution.changed ) {
                result.notes.insert( result.notes.end(), resolution.notes.begin(),
                                     resolution.notes.end() );
                site = std::move( candidate );
                continue;
            }
            if( resolution.valid &&
                ( resolution.kind == abstract_threat_resolution_kind::deferred_to_local ||
                  resolution.kind == abstract_threat_resolution_kind::withdrawal ||
                  resolution.kind == abstract_threat_resolution_kind::wounded_pair ||
                  resolution.kind == abstract_threat_resolution_kind::one_missing ||
                  resolution.kind == abstract_threat_resolution_kind::all_missing ) ) {
                site = std::move( candidate );
                continue;
            }
        }
        if( outing.phase == scout_phase::observing &&
            outing.selected_watch_kind != structural_watch_kind::none &&
            outing.waypoint_index == structural_outing_destination_waypoint( outing ) ) {
            const scout_assessment_result assessment = advance_structural_scout_assessment(
                        candidate, expected_activity_id, expected_generation,
                        outing.target_lead_revision, now_minutes );
            if( assessment == scout_assessment_result::normal_success ||
                assessment == scout_assessment_result::inconclusive ) {
                site = std::move( candidate );
                result.notes.push_back(
                    "structural outing completed its watch assessment lead=" +
                    site.active_outing.target_id );
                continue;
            }
            if( assessment == scout_assessment_result::updated ||
                assessment == scout_assessment_result::alternate_watch_started ) {
                site = std::move( candidate );
                if( assessment == scout_assessment_result::alternate_watch_started ) {
                    result.notes.push_back(
                        "structural outing moved to its persisted alternate watch lead=" +
                        site.active_outing.target_id );
                    continue;
                }
                if( waypoint_progressed ) {
                    result.arrivals_processed++;
                    result.notes.push_back(
                        "structural outing reached selected watch without resolving remote lead=" +
                        site.active_outing.target_id );
                }
                continue;
            }
        }
        if( outing.phase == scout_phase::lost ) {
            const bool all_members_confirmed_dead = !outing.member_ids.empty() &&
                    std::all_of( outing.member_ids.begin(), outing.member_ids.end(),
            [&candidate, &outing]( const character_id member_id ) {
                const member_record *member = candidate.find_member( member_id );
                return member != nullptr && member->state == member_state::dead &&
                       outing.member_is_resolved( member_id ) &&
                       std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
                       outing.casualty_ids.end();
            } );
            if( all_members_confirmed_dead || now_minutes >= outing.missing_deadline_minutes ) {
                candidate.last_routine_resolved_minutes = now_minutes;
                candidate.next_routine_dispatch_eligible_minutes = minutes_after_saturated(
                            now_minutes, routine_cooldown_delay_minutes(
                                candidate.site_id, 72 * 60 ) );
                const std::optional<int> released = release_structural_outing_reservation(
                        candidate, expected_activity_id, expected_generation,
                        all_members_confirmed_dead ?
                        "confirmed physical deaths closed a structural outing" :
                        "abstract threat encounter closed an all-missing structural outing" );
                if( released ) {
                    site = std::move( candidate );
                    result.notes.push_back( all_members_confirmed_dead ?
                                            "all-dead structural outing closed after physical confirmation" :
                                            "all-missing structural outing closed after its missing deadline" );
                }
            } else {
                site = std::move( candidate );
            }
            continue;
        }
        if( outing.phase == scout_phase::returning_report &&
            outing.owner == simulation_owner::abstract ) {
            outing.phase = scout_phase::returning_home;
            outing.last_progress_minutes = now_minutes;
            if( outing.local_handoff.is_abstract_resume() ) {
                outing.local_handoff.phase = outing.phase;
            }
            site = std::move( candidate );
            record_scout_phase_transition_event(
                site.active_outing, scout_phase::returning_report,
                scout_phase::returning_home,
                "normal watch report secured for return", now_minutes );
            result.notes.push_back(
                "structural outing secured its normal watch report and began return lead=" +
                site.active_outing.target_id );
            continue;
        }
        if( outing.phase == scout_phase::returning_home ) {
            if( now_minutes >= outing.expected_return_minutes ) {
                if( outing.local_handoff.is_abstract_resume() &&
                    !hostile_site_contains_omt( candidate,
                                                outing.local_handoff.route_position ) ) {
                    site = std::move( candidate );
                    result.notes.push_back(
                        "structural outing retained off-camp physical return ownership" );
                    continue;
                }
                const std::string lead_id = lead->lead_id;
                const bool completed_frontier_route = frontier_sector &&
                        outing.waypoint_index ==
                        static_cast<int>( outing.shared_route.size() ) - 2;
                outing.waypoint_index = static_cast<int>( outing.shared_route.size() ) - 1;
                outing.last_progress_minutes = now_minutes;
                const bool returned_shared_danger =
                    apply_returned_structural_threat_observation( candidate, *lead, now_minutes );
                if( completed_frontier_route && !returned_shared_danger ) {
                    candidate.intelligence_map.frontier_last_resolved_minutes[
                        static_cast<std::size_t>( *frontier_sector )] = now_minutes;
                    candidate.intelligence_map.frontier_sector_cursor =
                        ( *frontier_sector + 1 ) % frontier_sector_count;
                    candidate.intelligence_map.frontier_radius_omt = std::max(
                                candidate.intelligence_map.frontier_radius_omt,
                                frontier_outer_radius_omt );
                    lead->status = camp_lead_status::stale;
                    lead->last_checked_minutes = now_minutes;
                    lead->last_outcome = "frontier_route_reported_home";
                    advance_camp_map_lead_revision( candidate, *lead );
                }
                if( candidate.routine_activated_minutes < 0 ) {
                    candidate.routine_activated_minutes = candidate.active_outing.started_minutes;
                }
                const bool useful_return = !returned_shared_danger &&
                                           ( completed_frontier_route ||
                                             candidate.active_outing.cargo.supply_units > 0 ||
                                             ( !frontier_sector &&
                                               ( lead->status == camp_lead_status::harvested ||
                                                 ( lead->kind == camp_lead_kind::terrain_opportunity &&
                                                   lead->status == camp_lead_status::stale ) ) ) );
                const bool danger_withdrawal = lead->status == camp_lead_status::dangerous;
                const bool casualty_return =
                    !candidate.active_outing.casualty_ids.empty();
                candidate.last_routine_resolved_minutes = now_minutes;
                candidate.next_routine_dispatch_eligible_minutes = minutes_after_saturated(
                            now_minutes, routine_cooldown_delay_minutes(
                                candidate.site_id,
                                casualty_return ? 72 * 60 :
                                useful_return || danger_withdrawal ? 24 * 60 : 18 * 60 ) );
                if( useful_return ) {
                    candidate.routine_no_candidate_streak = 0;
                }
                if( !credit_structural_return_cargo( candidate, now_minutes ) ) {
                    continue;
                }
                const int returned_signal_leads =
                    apply_returned_structural_signal_observations( candidate, now_minutes );
                if( returned_signal_leads > 0 && !casualty_return &&
                    !returned_shared_danger && !danger_withdrawal ) {
                    candidate.next_routine_dispatch_eligible_minutes =
                        minutes_after_saturated( now_minutes, 1 );
                }
                std::vector<character_id> returned_carrier_ids;
                for( const character_id member_id : candidate.active_outing.member_ids ) {
                    const member_record *member = candidate.find_member( member_id );
                    const bool casualty = std::find(
                                              candidate.active_outing.casualty_ids.begin(),
                                              candidate.active_outing.casualty_ids.end(), member_id ) !=
                                          candidate.active_outing.casualty_ids.end();
                    if( !casualty && member != nullptr &&
                        member->state != member_state::dead &&
                        member->state != member_state::missing ) {
                        returned_carrier_ids.push_back( member_id );
                    }
                }
                if( !candidate.active_outing.assessment.exit_reason.empty() &&
                    !deliver_structural_scout_assessment_report( candidate, now_minutes,
                            returned_carrier_ids ) ) {
                    continue;
                }
                const std::optional<int> returned = release_structural_outing_reservation(
                        candidate, expected_activity_id, expected_generation,
                        "structural outing completed its shared route home" );
                if( returned ) {
                    site = std::move( candidate );
                    if( site.current_scout_report.is_present() &&
                        site.current_scout_report.source_activity_id == expected_activity_id &&
                        site.current_scout_report.source_generation == expected_generation ) {
                        accept_current_scout_report_for_assessment( site );
                    }
                    result.members_returned += *returned;
                    result.notes.push_back( "structural outing returned home lead=" + lead_id );
                    if( returned_signal_leads > 0 ) {
                        result.notes.push_back( "structural outing returned signal leads=" +
                                                std::to_string( returned_signal_leads ) );
                    }
                }
            } else {
                site = std::move( candidate );
            }
            continue;
        }

        const int elapsed = now_minutes - outing.started_minutes;
        if( outing.phase == scout_phase::outbound && outing.local_contact_minutes < 0 &&
            elapsed >= active_structural_outing_stalking_delay_minutes( candidate ) ) {
            structural_threat_read threat;
            if( threat_lookup ) {
                threat = threat_lookup( candidate, *lead );
            }
            lead->threat = std::max( 0, threat.threat );
            lead->threat_confirmed = true;
            lead->last_scouted_minutes = now_minutes;
            lead->last_checked_minutes = now_minutes;
            lead->source_summary = threat.summary.empty() ?
                                   "stalking-distance structural threat check" : threat.summary;
            outing.waypoint_index = 1;
            outing.local_contact_minutes = now_minutes;
            outing.phase = scout_phase::observing;
            outing.last_progress_minutes = now_minutes;
            result.stalking_checks_processed++;

            const int effective_interest = frontier_sector ? ( lead->threat > 0 ? 0 : 1 ) :
                                           structural_effective_interest( *lead, lead->threat );
            if( effective_interest <= 0 ) {
                const std::string lead_id = lead->lead_id;
                lead->status = lead->threat > 0 ? camp_lead_status::dangerous : camp_lead_status::stale;
                lead->last_outcome = "threat_revealed_lost_interest";
                advance_camp_map_lead_revision( candidate, *lead );
                outing.phase = scout_phase::returning_home;
                outing.last_progress_minutes = now_minutes;
                site = std::move( candidate );
                result.lost_interest_returns++;
                result.notes.push_back( "structural outing turned back before arrival lead=" +
                                        lead_id + " effective_interest=" +
                                        std::to_string( effective_interest ) );
                continue;
            }

            const std::string lead_id = lead->lead_id;
            lead->status = camp_lead_status::scout_confirmed;
            lead->last_outcome = "threat_revealed_interest_survives";
            advance_camp_map_lead_revision( candidate, *lead );
            result.notes.push_back( "structural outing stalking check kept arrival open lead=" +
                                    lead_id + " effective_interest=" +
                                    std::to_string( effective_interest ) );
            site = std::move( candidate );
            continue;
        }

        if( outing.phase == scout_phase::observing && outing.local_contact_minutes >= 0 &&
            elapsed >= active_structural_outing_arrival_delay_minutes( candidate ) ) {
            const std::string lead_id = lead->lead_id;
            if( structural_outing_uses_watch_route( outing ) ) {
                const int destination_waypoint = structural_outing_destination_waypoint( outing );
                const bool newly_arrived = waypoint_progressed ||
                                           outing.waypoint_index != destination_waypoint;
                outing.waypoint_index = destination_waypoint;
                if( newly_arrived && !waypoint_progressed ) {
                    outing.last_progress_minutes = now_minutes;
                }
                site = std::move( candidate );
                if( newly_arrived ) {
                    result.arrivals_processed++;
                    result.notes.push_back(
                        "structural outing reached selected watch without resolving remote lead=" +
                        lead_id );
                }
                continue;
            }
            const bool terrain_opportunity_arrival =
                lead->kind == camp_lead_kind::terrain_opportunity;
            const bool returned_signal_arrival = returned_structural_signal_lead( *lead );
            bool returned_signal_had_support = false;
            if( frontier_sector ) {
                lead->status = camp_lead_status::scout_confirmed;
                lead->last_scouted_minutes = now_minutes;
                lead->last_checked_minutes = now_minutes;
                lead->last_outcome = "frontier_outer_sample_complete";
            } else if( terrain_opportunity_arrival ) {
                lead->status = camp_lead_status::stale;
                lead->last_scouted_minutes = now_minutes;
                lead->last_checked_minutes = now_minutes;
                lead->last_outcome = "terrain_opportunity_physically_checked";
            } else if( returned_signal_arrival ) {
                returned_signal_had_support = returned_structural_signal_lead_has_support(
                                                  candidate, *lead, now_minutes );
                if( !returned_signal_had_support &&
                    lead->times_checked_empty >= std::numeric_limits<int>::max() ) {
                    continue;
                }
                if( !returned_signal_had_support ) {
                    lead->status = camp_lead_status::stale;
                    lead->last_scouted_minutes = now_minutes;
                    lead->last_checked_minutes = now_minutes;
                    lead->confidence = 0;
                    lead->times_checked_empty++;
                    lead->source_summary =
                        "paired scouts found no source inside the returned signal uncertainty area";
                    lead->last_outcome = "signal_investigation_empty";
                }
            } else {
                world_state transaction;
                transaction.schema_version = state.schema_version;
                transaction.sites.push_back( std::move( candidate ) );
                const auto current_resource = state.finite_resources.find(
                                                  transaction.sites.front().active_outing.target_omt );
                if( current_resource != state.finite_resources.end() ) {
                    transaction.finite_resources.emplace( current_resource->first,
                                                          current_resource->second );
                }
                const std::optional<structural_resource_arrival_receipt> receipt =
                    apply_structural_resource_arrival( transaction, 0, now_minutes );
                if( !receipt ) {
                    continue;
                }
                if( receipt->claimed_units > 0 ) {
                    const auto updated_resource = transaction.finite_resources.find(
                                                      transaction.sites.front().active_outing.target_omt );
                    if( updated_resource == transaction.finite_resources.end() ) {
                        continue;
                    }
                    state.finite_resources.insert_or_assign( updated_resource->first,
                            updated_resource->second );
                }
                state.schema_version = transaction.schema_version;
                site = std::move( transaction.sites.front() );
                result.arrivals_processed++;
                result.notes.push_back( "structural outing claimed " +
                                        std::to_string( receipt->claimed_units ) +
                                        " finite bounty units and left " +
                                        std::to_string( receipt->remaining_units ) );
                continue;
            }
            advance_camp_map_lead_revision( candidate, *lead );
            outing.waypoint_index = structural_outing_destination_waypoint( outing );
            outing.phase = scout_phase::returning_home;
            outing.last_progress_minutes = now_minutes;
            site = std::move( candidate );
            result.arrivals_processed++;
            if( frontier_sector ) {
                result.notes.push_back(
                    "frontier outing sampled outer ring and began return lead=" + lead_id );
            } else if( terrain_opportunity_arrival ) {
                result.notes.push_back(
                    "terrain opportunity was physically checked and began return lead=" + lead_id );
            } else if( returned_signal_arrival ) {
                result.notes.push_back( returned_signal_had_support ?
                                        "returned signal investigation found support and began return lead=" +
                                        lead_id :
                                        "returned signal investigation was empty and began return lead=" +
                                        lead_id );
            } else {
                result.notes.push_back(
                    "structural outing harvested and began return lead=" + lead_id );
            }
            continue;
        }

        site = std::move( candidate );
    }
    return result;
}

structural_bounty_maintenance_result advance_structural_bounty_maintenance( world_state &state,
        const int now_minutes, const int scan_budget, const int dispatch_cap,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup,
        const std::function<structural_route_read( const site_record &,
                const structural_outing_plan & )> &route_lookup,
        const std::function<abstract_threat_read( const site_record &, const active_outing_state &,
                const structural_threat_observer_request & )> &abstract_threat_lookup,
        const std::function<std::vector<structural_signal_read>( const site_record &,
                const active_outing_state &,
                const structural_threat_observer_request & )> &signal_lookup,
        const std::function<int( world_state &, std::size_t )> &materialize_for_dispatch )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_maintenance );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::structural_maintenance_updates );
    structural_bounty_maintenance_result result;
    result.scheduler_consider_cap = routine_scheduler_consider_cap;
    result.full_route_solve_cap = routine_scheduler_full_route_solve_cap;
    state.schema_version = 6;
    result.intelligence_aging = advance_camp_intelligence_aging( state, now_minutes );
    advance_world_camp_supplies( state, now_minutes );
    result.dispatch_cap = std::min( routine_scheduler_start_cap, std::max( 0, dispatch_cap ) );
    result.outing = advance_structural_bounty_outings( state, now_minutes, threat_lookup,
                    abstract_threat_lookup, signal_lookup );

    bandit_live_world_probe::scoped_section dispatch_probe_section(
        bandit_live_world_probe::section::structural_dispatch );

    if( now_minutes < 0 ) {
        result.notes.push_back( "structural maintenance dispatch skipped: invalid time" );
        return result;
    }
    result.scheduler_hour = now_minutes / 60;
    std::vector<std::size_t> routine_site_indices;
    routine_site_indices.reserve( state.sites.size() );
    for( std::size_t index = 0; index < state.sites.size(); ++index ) {
        const site_record &site = state.sites[index];
        if( supports_routine_camp_ecology( effective_profile( site ) ) &&
            !site.retired_empty_site ) {
            routine_site_indices.push_back( index );
        }
    }
    if( routine_site_indices.empty() ) {
        state.routine_scheduler_cursor = 0;
        state.routine_terrain_scan_cursor = 0;
    } else {
        state.routine_scheduler_cursor = std::max( 0, state.routine_scheduler_cursor ) %
                                         static_cast<int>( routine_site_indices.size() );
        state.routine_terrain_scan_cursor = std::max( 0, state.routine_terrain_scan_cursor ) %
                                            static_cast<int>( routine_site_indices.size() );
    }
    if( state.routine_scheduler_last_hour >= result.scheduler_hour ) {
        result.scheduler_replay_suppressed = true;
        result.scheduler_cursor_before = state.routine_scheduler_cursor;
        result.scheduler_cursor_after = state.routine_scheduler_cursor;
        result.terrain_scan_cursor_before = state.routine_terrain_scan_cursor;
        result.terrain_scan_cursor_after = state.routine_terrain_scan_cursor;
        result.notes.push_back(
            "structural maintenance dispatch skipped: scheduler hour already processed" );
        return result;
    }
    state.routine_scheduler_last_hour = result.scheduler_hour;
    if( routine_site_indices.empty() ) {
        result.scan.scan_budget = std::max( 0, scan_budget );
        result.scan.notes.push_back( "structural scan skipped: no eligible routine camps" );
        return result;
    }

    result.terrain_scan_cursor_before = state.routine_terrain_scan_cursor;
    result.scan.scan_budget = std::max( 0, scan_budget );
    if( result.scan.scan_budget == 0 ) {
        result.scan.notes.push_back( "structural scan skipped: zero budget" );
    } else if( !terrain_lookup ) {
        result.scan.notes.push_back( "structural scan skipped: no terrain lookup" );
    } else {
        bandit_live_world_probe::scoped_section scan_probe_section(
            bandit_live_world_probe::section::structural_scan );
        result.terrain_scan_sites_selected = std::min( result.scan.scan_budget,
                                             static_cast<int>( routine_site_indices.size() ) );
        for( int offset = 0; offset < result.terrain_scan_sites_selected; ++offset ) {
            const int eligible_index = ( result.terrain_scan_cursor_before + offset ) %
                                       static_cast<int>( routine_site_indices.size() );
            site_record &site = state.sites[routine_site_indices[static_cast<std::size_t>(
                                               eligible_index )]];
            // Static home-side terrain discovery is not another outing and does not dogpile the
            // pair already outside.  Remaining ready home occupants may still make this one
            // bounded terrain read; dispatch gates continue to block a second active party.
            sample_structural_bounty_site( site, result.scan, now_minutes, 1, true,
                                           terrain_lookup );
        }
        state.routine_terrain_scan_cursor =
            ( result.terrain_scan_cursor_before + result.terrain_scan_sites_selected ) %
            static_cast<int>( routine_site_indices.size() );
        result.scan.budget_exhausted = result.scan.budget_used >= result.scan.scan_budget;
        result.scan.notes.push_back(
            "structural scan rotated one persisted near-ring sample per selected camp" );
    }
    result.terrain_scan_cursor_after = state.routine_terrain_scan_cursor;

    result.scheduler_cursor_before = state.routine_scheduler_cursor;
    const int sites_to_consider = std::min( routine_scheduler_consider_cap,
                                           static_cast<int>( routine_site_indices.size() ) );
    const bool defer_exact_pair = static_cast<bool>( materialize_for_dispatch );
    struct urgent_signal_site {
        std::size_t site_index;
        int expiry_minutes;
    };
    std::vector<urgent_signal_site> urgent_signal_sites;
    urgent_signal_sites.reserve( routine_site_indices.size() );
    for( const std::size_t site_index : routine_site_indices ) {
        const site_record &site = state.sites[site_index];
        if( ( site.next_routine_dispatch_eligible_minutes >= 0 &&
              now_minutes < site.next_routine_dispatch_eligible_minutes ) ||
            site.has_active_outside_pressure() ||
            !camp_decision_allows_dispatch( site.camp_decision,
                                            bandit_dry_run::job_template::scout ) ||
            !routine_scout_policy( site ).eligible ) {
            continue;
        }
        int earliest_expiry = std::numeric_limits<int>::max();
        for( const camp_map_lead &lead : site.intelligence_map.leads ) {
            if( !returned_structural_signal_lead( lead ) ||
                structural_signal_strength( lead, now_minutes ) <= 0 ||
                lead.status == camp_lead_status::active ||
                lead.status == camp_lead_status::harvested ||
                lead.status == camp_lead_status::dangerous ||
                lead.status == camp_lead_status::invalidated ||
                structural_lead_recently_checked( lead, now_minutes ) ) {
                continue;
            }
            const int horizon_minutes = lead.kind == camp_lead_kind::sound_signal ?
                                        3 * 60 : 6 * 60;
            earliest_expiry = std::min( earliest_expiry,
                                        minutes_after_saturated( lead.last_seen_minutes,
                                                horizon_minutes ) );
        }
        if( earliest_expiry != std::numeric_limits<int>::max() ) {
            urgent_signal_sites.push_back( { site_index, earliest_expiry } );
        }
    }
    std::sort( urgent_signal_sites.begin(), urgent_signal_sites.end(), [&state](
    const urgent_signal_site & lhs, const urgent_signal_site & rhs ) {
        if( lhs.expiry_minutes != rhs.expiry_minutes ) {
            return lhs.expiry_minutes < rhs.expiry_minutes;
        }
        return state.sites[lhs.site_index].site_id < state.sites[rhs.site_index].site_id;
    } );
    if( urgent_signal_sites.size() >
        static_cast<std::size_t>( routine_scheduler_urgent_signal_cap ) ) {
        urgent_signal_sites.resize( routine_scheduler_urgent_signal_cap );
    }

    std::vector<std::size_t> dispatch_site_indices;
    dispatch_site_indices.reserve( static_cast<std::size_t>( sites_to_consider ) );
    for( const urgent_signal_site &urgent : urgent_signal_sites ) {
        if( dispatch_site_indices.size() >= static_cast<std::size_t>( sites_to_consider ) ) {
            break;
        }
        dispatch_site_indices.push_back( urgent.site_index );
    }
    int normal_sites_selected = 0;
    for( int offset = 0;
         offset < static_cast<int>( routine_site_indices.size() ) &&
         dispatch_site_indices.size() < static_cast<std::size_t>( sites_to_consider ); ++offset ) {
        const int eligible_index = ( result.scheduler_cursor_before + offset ) %
                                   static_cast<int>( routine_site_indices.size() );
        const std::size_t site_index = routine_site_indices[static_cast<std::size_t>(
                                           eligible_index )];
        if( std::find( dispatch_site_indices.begin(), dispatch_site_indices.end(), site_index ) !=
            dispatch_site_indices.end() ) {
            continue;
        }
        dispatch_site_indices.push_back( site_index );
        normal_sites_selected++;
    }
    struct routine_dispatch_candidate {
        std::size_t site_index;
        std::vector<structural_outing_plan> cheap_plans;
        routine_dispatch_evaluation evaluation;
        int overdue_bonus;
        bool frontier_due;
        bool urgent_signal;
        int urgent_signal_expiry_minutes;
    };
    std::vector<routine_dispatch_candidate> dispatch_candidates;
    dispatch_candidates.reserve( static_cast<std::size_t>( sites_to_consider ) );
    for( const std::size_t site_index : dispatch_site_indices ) {
        site_record &site = state.sites[site_index];
        const bool urgent_signal = std::any_of( urgent_signal_sites.begin(),
        urgent_signal_sites.end(), [site_index]( const urgent_signal_site & urgent ) {
            return urgent.site_index == site_index;
        } );
        const auto urgent_site = std::find_if( urgent_signal_sites.begin(),
        urgent_signal_sites.end(), [site_index]( const urgent_signal_site & urgent ) {
            return urgent.site_index == site_index;
        } );
        const int urgent_signal_expiry_minutes = urgent_site == urgent_signal_sites.end() ?
                                                  std::numeric_limits<int>::max() :
                                                  urgent_site->expiry_minutes;
        result.sites_considered_for_dispatch++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_dispatch_sites_considered );
        bandit_live_world_probe::record_site_service( site.site_id,
                bandit_live_world_probe::site_service::dispatch_considered );
        if( site.routine_activated_minutes < 0 ) {
            site.routine_activated_minutes = now_minutes;
        }
        const bool frontier_due = frontier_dispatch_is_due( site, now_minutes );
        if( site.next_routine_dispatch_eligible_minutes >= 0 &&
            now_minutes < site.next_routine_dispatch_eligible_minutes ) {
            continue;
        }
        if( result.dispatch_cap == 0 ) {
            continue;
        }
        const routine_scout_policy_result routine_policy = routine_scout_policy( site );
        const routine_scout_pair_selection_result pair_selection =
            select_routine_scout_pair( site );
        if( site.has_active_outside_pressure() ||
            !camp_decision_allows_dispatch( site.camp_decision,
                                            bandit_dry_run::job_template::scout ) ||
            !routine_policy.eligible || ( !defer_exact_pair && !pair_selection.eligible ) ) {
            continue;
        }
        const bool has_structural_candidate_source = std::any_of(
                    site.intelligence_map.leads.begin(), site.intelligence_map.leads.end(),
        []( const camp_map_lead & lead ) {
            return lead.kind == camp_lead_kind::structural_bounty ||
                   lead.kind == camp_lead_kind::terrain_opportunity ||
                   returned_structural_signal_lead( lead );
        } );
        if( !frontier_due && !has_structural_candidate_source ) {
            continue;
        }
        std::vector<structural_outing_plan> cheap_plans =
            cheap_structural_outing_candidates( site, now_minutes, !defer_exact_pair );
        if( frontier_due ) {
            structural_outing_plan frontier = plan_frontier_outing_impl(
                    site, now_minutes, false, !defer_exact_pair );
            if( frontier.valid ) {
                cheap_plans.push_back( std::move( frontier ) );
            }
        }
        if( cheap_plans.empty() ) {
            routine_dispatch_evaluation evaluation = evaluate_hostile_camp_routine_dispatch(
                        site, now_minutes, 0 );
            evaluation.force_due = evaluation.force_due || frontier_due;
            if( !evaluation.force_due && evaluation.drive < 500 ) {
                result.notes.push_back( "routine dispatch drive below threshold with no candidate site=" +
                                        site.site_id + " drive=" +
                                        std::to_string( evaluation.drive ) );
                continue;
            }
            site.routine_no_candidate_streak = site.routine_no_candidate_streak >= 2 ? 3 :
                                               std::max( 0, site.routine_no_candidate_streak ) + 1;
            const int base_delay = routine_no_candidate_base_delay_minutes(
                                       site.routine_no_candidate_streak );
            site.next_routine_dispatch_eligible_minutes = minutes_after_saturated(
                        now_minutes, routine_cooldown_delay_minutes( site.site_id, base_delay ) );
            result.notes.push_back( "routine dispatch found no cheap candidate site=" +
                                    site.site_id + " streak=" +
                                    std::to_string( site.routine_no_candidate_streak ) );
            continue;
        }

        std::sort( cheap_plans.begin(), cheap_plans.end(), [&site, frontier_due, urgent_signal](
        const structural_outing_plan & lhs, const structural_outing_plan & rhs ) {
            if( urgent_signal ) {
                const camp_map_lead *lhs_lead = site.intelligence_map.find_lead( lhs.lead_id );
                const camp_map_lead *rhs_lead = site.intelligence_map.find_lead( rhs.lead_id );
                const bool lhs_signal = lhs_lead != nullptr &&
                                        returned_structural_signal_lead( *lhs_lead );
                const bool rhs_signal = rhs_lead != nullptr &&
                                        returned_structural_signal_lead( *rhs_lead );
                if( lhs_signal != rhs_signal ) {
                    return lhs_signal;
                }
                if( lhs_signal ) {
                    const int lhs_horizon = lhs_lead->kind == camp_lead_kind::sound_signal ?
                                            3 * 60 : 6 * 60;
                    const int rhs_horizon = rhs_lead->kind == camp_lead_kind::sound_signal ?
                                            3 * 60 : 6 * 60;
                    const int lhs_expiry = minutes_after_saturated(
                                               lhs_lead->last_seen_minutes, lhs_horizon );
                    const int rhs_expiry = minutes_after_saturated(
                                               rhs_lead->last_seen_minutes, rhs_horizon );
                    if( lhs_expiry != rhs_expiry ) {
                        return lhs_expiry < rhs_expiry;
                    }
                }
            }
            if( frontier_due && ( lhs.frontier_sector >= 0 ) != ( rhs.frontier_sector >= 0 ) ) {
                return lhs.frontier_sector >= 0;
            }
            return cheap_plan_precedes( lhs, rhs, site );
        } );
        const int best_cheap_score = std::max_element(
                                         cheap_plans.begin(), cheap_plans.end(),
        []( const structural_outing_plan & lhs, const structural_outing_plan & rhs ) {
            return lhs.cheap_score < rhs.cheap_score;
        } )->cheap_score;
        routine_dispatch_evaluation evaluation = evaluate_hostile_camp_routine_dispatch(
                    site, now_minutes, best_cheap_score );
        evaluation.force_due = evaluation.force_due || frontier_due;
        if( !evaluation.force_due && evaluation.drive < 500 ) {
            result.notes.push_back( "routine dispatch drive below threshold site=" + site.site_id +
                                    " drive=" + std::to_string( evaluation.drive ) );
            continue;
        }
        const int wait_minutes = routine_dispatch_wait_minutes( site, now_minutes,
                                 frontier_due );
        const int overdue_bonus = 250 * std::clamp( wait_minutes, 0, 72 * 60 ) / ( 72 * 60 );
        dispatch_candidates.push_back( { site_index, std::move( cheap_plans ), evaluation,
                                         overdue_bonus, frontier_due, urgent_signal,
                                         urgent_signal_expiry_minutes } );
    }

    std::sort( dispatch_candidates.begin(), dispatch_candidates.end(), [&state](
    const routine_dispatch_candidate & lhs, const routine_dispatch_candidate & rhs ) {
        if( lhs.urgent_signal != rhs.urgent_signal ) {
            return lhs.urgent_signal;
        }
        if( lhs.urgent_signal &&
            lhs.urgent_signal_expiry_minutes != rhs.urgent_signal_expiry_minutes ) {
            return lhs.urgent_signal_expiry_minutes < rhs.urgent_signal_expiry_minutes;
        }
        const int lhs_priority = lhs.evaluation.drive + lhs.overdue_bonus;
        const int rhs_priority = rhs.evaluation.drive + rhs.overdue_bonus;
        if( lhs_priority != rhs_priority ) {
            return lhs_priority > rhs_priority;
        }
        return state.sites[lhs.site_index].site_id < state.sites[rhs.site_index].site_id;
    } );

    struct routine_dispatch_contender {
        std::size_t site_index;
        structural_outing_plan plan;
    };
    std::vector<routine_dispatch_contender> contenders;
    contenders.reserve( dispatch_candidates.size() );
    for( routine_dispatch_candidate &candidate : dispatch_candidates ) {
        if( result.full_route_solves >= result.full_route_solve_cap ) {
            result.notes.push_back( "routine dispatch global route budget exhausted" );
            break;
        }
        site_record &site = state.sites[candidate.site_index];
        structural_outing_plan best_routed;
        int site_route_solves = 0;
        for( const structural_outing_plan &cheap : candidate.cheap_plans ) {
            if( site_route_solves >= routine_candidate_full_route_solve_cap ||
                result.full_route_solves >= result.full_route_solve_cap ) {
                break;
            }
            site_route_solves++;
            result.full_route_solves++;
            structural_outing_plan routed;
            if( cheap.frontier_sector >= 0 ) {
                routed = plan_frontier_outing_impl(
                             site, now_minutes, true, !defer_exact_pair );
            } else {
                const camp_map_lead *lead = site.intelligence_map.find_lead( cheap.lead_id );
                if( lead != nullptr ) {
                    routed = plan_structural_bounty_outing_impl(
                                 site, *lead, now_minutes, true, !defer_exact_pair );
                }
            }
            if( routed.valid ) {
                structural_route_read read;
                if( route_lookup ) {
                    read = route_lookup( site, routed );
                } else {
                    read.reachable = routed.route_solved;
                    read.complete_route_cost = routed.full_route_cost;
                    read.max_segment_risk = routed.max_route_segment_risk;
                    read.summary = "deterministic bounded route fallback";
                }
                if( !apply_structural_route_read( site, now_minutes, read, routed ) ) {
                    routed.valid = false;
                }
            }
            if( !routed.valid || hostile_camp_routine_risk_blocked( routed.static_risk ) ||
                !hostile_camp_routine_score_eligible( routed.final_score, false ) ) {
                continue;
            }
            const camp_map_lead *routed_lead = site.intelligence_map.find_lead( routed.lead_id );
            if( candidate.urgent_signal && routed_lead != nullptr &&
                returned_structural_signal_lead( *routed_lead ) ) {
                best_routed = std::move( routed );
                break;
            }
            if( !best_routed.valid ||
                ( candidate.frontier_due && routed.frontier_sector >= 0 &&
                  best_routed.frontier_sector < 0 ) ||
                ( !( candidate.frontier_due && best_routed.frontier_sector >= 0 ) &&
                  ( routed.final_score > best_routed.final_score ||
                    ( routed.final_score == best_routed.final_score &&
                      cheap_plan_precedes( routed, best_routed, site ) ) ) ) ) {
                best_routed = std::move( routed );
            }
        }
        if( !best_routed.valid ) {
            const bool global_budget_truncated_site =
                result.full_route_solves >= result.full_route_solve_cap &&
                site_route_solves < std::min( routine_candidate_full_route_solve_cap,
                                              static_cast<int>( candidate.cheap_plans.size() ) );
            if( global_budget_truncated_site ) {
                result.notes.push_back( "routine dispatch route evaluation deferred by global budget site=" +
                                        site.site_id );
                continue;
            }
            site.routine_no_candidate_streak = site.routine_no_candidate_streak >= 2 ? 3 :
                                               std::max( 0, site.routine_no_candidate_streak ) + 1;
            const int base_delay = routine_no_candidate_base_delay_minutes(
                                       site.routine_no_candidate_streak );
            site.next_routine_dispatch_eligible_minutes = minutes_after_saturated(
                        now_minutes, routine_cooldown_delay_minutes( site.site_id, base_delay ) );
            result.notes.push_back( "routine dispatch found no routed score-eligible candidate site=" +
                                    site.site_id + " streak=" +
                                    std::to_string( site.routine_no_candidate_streak ) );
            continue;
        }
        result.dispatches_planned++;
        contenders.push_back( { candidate.site_index, std::move( best_routed ) } );
    }

    for( routine_dispatch_contender &contender : contenders ) {
        if( result.dispatches_applied >= result.dispatch_cap ) {
            result.dispatch_cap_reached = true;
            break;
        }
        structural_outing_plan dispatch_plan = contender.plan;
        if( defer_exact_pair ) {
            const routine_scout_pair_selection_result existing_pair =
                select_routine_scout_pair( state.sites[contender.site_index] );
            if( !existing_pair.eligible ) {
                if( result.materialization_attempts >= result.dispatch_cap ) {
                    result.dispatches_blocked++;
                    result.notes.push_back(
                        "structural maintenance materialization cap blocked site=" +
                        state.sites[contender.site_index].site_id );
                    continue;
                }
                result.materialization_attempts++;
                result.members_materialized += std::max(
                                                   0, materialize_for_dispatch( state,
                                                           contender.site_index ) );
            }

            site_record &materialized_site = state.sites[contender.site_index];
            structural_outing_plan exact_plan;
            if( contender.plan.frontier_sector >= 0 ) {
                exact_plan = plan_frontier_outing_impl(
                                 materialized_site, now_minutes, true, true );
            } else if( const camp_map_lead *lead =
                       materialized_site.intelligence_map.find_lead( contender.plan.lead_id ) ) {
                exact_plan = plan_structural_bounty_outing_impl(
                                 materialized_site, *lead, now_minutes, true, true );
            }
            const structural_route_read cached_route{
                true, contender.plan.full_route_cost, contender.plan.max_route_segment_risk,
                "scheduler-owned route retained across exact-pair materialization",
                contender.plan.watch_geography_supplied, contender.plan.target_footprint,
                contender.plan.watch_candidates,
                contender.plan.watch_geography_supplied ? contender.plan.shared_route :
                std::vector<tripoint_abs_omt>()
            };
            if( exact_plan.valid && !apply_structural_route_read(
                    materialized_site, now_minutes, cached_route, exact_plan ) ) {
                exact_plan.valid = false;
            }
            if( !exact_plan.valid || exact_plan.lead_id != contender.plan.lead_id ||
                exact_plan.lead_revision != contender.plan.lead_revision ||
                exact_plan.target_omt != contender.plan.target_omt ||
                exact_plan.frontier_sector != contender.plan.frontier_sector ) {
                result.dispatches_blocked++;
                result.notes.push_back(
                    "structural maintenance exact-pair materialization blocked site=" +
                    materialized_site.site_id );
                continue;
            }
            dispatch_plan = std::move( exact_plan );
        }

        site_record &site = state.sites[contender.site_index];
        for( const std::string &note : dispatch_plan.notes ) {
            result.notes.push_back( note );
        }
        if( apply_structural_bounty_outing_plan( site, dispatch_plan, now_minutes ) ) {
            result.dispatches_applied++;
            result.notes.push_back( "structural maintenance dispatched site=" + site.site_id +
                                    " lead=" + dispatch_plan.lead_id );
        } else {
            result.dispatches_blocked++;
            result.notes.push_back( "structural maintenance dispatch apply blocked site=" + site.site_id +
                                    " lead=" + dispatch_plan.lead_id );
        }
    }

    state.routine_scheduler_cursor = ( result.scheduler_cursor_before + normal_sites_selected ) %
                                     static_cast<int>( routine_site_indices.size() );
    result.scheduler_cursor_after = state.routine_scheduler_cursor;

    return result;
}

std::string render_structural_bounty_maintenance_report(
    const structural_bounty_maintenance_result &result )
{
    std::ostringstream out;
    out << "bandit_live_world structural maintenance:"
        << " intelligence_sites_considered=" << result.intelligence_aging.sites_considered
        << " intelligence_sites_cleaned=" << result.intelligence_aging.sites_cleaned
        << " intelligence_leads_considered=" << result.intelligence_aging.leads_considered
        << " intelligence_leads_aged=" << result.intelligence_aging.leads_aged
        << " intelligence_leads_pruned=" << result.intelligence_aging.leads_pruned
        << " scan_budget=" << result.scan.scan_budget
        << " budget_used=" << result.scan.budget_used
        << " budget_exhausted=" << ( result.scan.budget_exhausted ? "yes" : "no" )
        << " sites_scanned=" << result.scan.sites_considered
        << " candidates_sampled=" << result.scan.candidates_sampled
        << " leads_seeded=" << result.scan.leads_seeded
        << " leads_suppressed=" << result.scan.leads_suppressed_by_memory
        << " scheduler_hour=" << result.scheduler_hour
        << " scheduler_cursor_before=" << result.scheduler_cursor_before
        << " scheduler_cursor_after=" << result.scheduler_cursor_after
        << " terrain_scan_cursor_before=" << result.terrain_scan_cursor_before
        << " terrain_scan_cursor_after=" << result.terrain_scan_cursor_after
        << " terrain_scan_sites_selected=" << result.terrain_scan_sites_selected
        << " scheduler_consider_cap=" << result.scheduler_consider_cap
        << " scheduler_replay_suppressed=" <<
        ( result.scheduler_replay_suppressed ? "yes" : "no" )
        << " full_route_solve_cap=" << result.full_route_solve_cap
        << " full_route_solves=" << result.full_route_solves
        << " dispatch_cap=" << result.dispatch_cap
        << " dispatches_planned=" << result.dispatches_planned
        << " dispatches_applied=" << result.dispatches_applied
        << " materialization_attempts=" << result.materialization_attempts
        << " members_materialized=" << result.members_materialized
        << " dispatch_cap_reached=" << ( result.dispatch_cap_reached ? "yes" : "no" )
        << " active_outings=" << result.outing.active_outings_considered
        << " stalking_checks=" << result.outing.stalking_checks_processed
        << " turnbacks=" << result.outing.lost_interest_returns
        << " arrivals=" << result.outing.arrivals_processed
        << " members_returned=" << result.outing.members_returned << '\n';
    for( const std::string &note : result.outing.notes ) {
        out << "- " << note << '\n';
    }
    for( const std::string &note : result.scan.notes ) {
        out << "- " << note << '\n';
    }
    for( const std::string &note : result.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

std::string render_evidence_debug_report( const world_state &state, const int current_minutes )
{
    static constexpr std::size_t site_cap = 8;
    static constexpr std::size_t lead_cap = 8;
    static constexpr std::size_t observation_cap = 8;
    static constexpr std::size_t token_cap = 96;

    const auto token = []( const std::string &raw ) {
        std::string result;
        result.reserve( std::min( raw.size(), token_cap ) );
        for( const char character : raw ) {
            if( result.size() == token_cap ) {
                break;
            }
            const unsigned char byte = static_cast<unsigned char>( character );
            result.push_back( byte <= 0x20 || byte == 0x7f || character == '"' ||
                              character == '\\' || character == '=' ? '_' : character );
        }
        if( raw.size() > token_cap && !result.empty() ) {
            result.back() = '~';
        }
        return result.empty() ? std::string( "-" ) : result;
    };
    const auto omt = []( const tripoint_abs_omt &point ) {
        return "(" + std::to_string( point.x() ) + "," + std::to_string( point.y() ) + "," +
               std::to_string( point.z() ) + ")";
    };
    const auto signed_delta = []( const int lhs, const int rhs ) {
        const long long delta = static_cast<long long>( lhs ) - rhs;
        return std::string( delta >= 0 ? "+" : "" ) + std::to_string( delta );
    };
    const auto age = [&signed_delta, current_minutes]( const int observed_minutes ) {
        return observed_minutes < 0 ? std::string( "unknown" ) :
               signed_delta( current_minutes, observed_minutes );
    };

    struct site_view {
        const site_record *site;
        std::size_t persisted_index;
    };
    std::vector<site_view> sites;
    for( std::size_t index = 0; index < state.sites.size(); ++index ) {
        const site_record &site = state.sites[index];
        if( !site.intelligence_map.leads.empty() ||
            !site.active_outing.observations.empty() ||
            !site.current_scout_report.observations.empty() ) {
            sites.push_back( { &site, index } );
        }
    }
    std::sort( sites.begin(), sites.end(), []( const site_view &lhs, const site_view &rhs ) {
        return std::tie( lhs.site->site_id, lhs.site->anchor, lhs.persisted_index ) <
               std::tie( rhs.site->site_id, rhs.site->anchor, rhs.persisted_index );
    } );

    const std::size_t rendered_sites = std::min( sites.size(), site_cap );
    const int rotation_hour = std::max( 0, current_minutes ) / 60;
    const std::size_t start = sites.empty() ? 0 :
                              static_cast<std::size_t>( rotation_hour ) % sites.size();
    const std::size_t rotation_cycle = sites.empty() ? 0 :
                                       static_cast<std::size_t>( rotation_hour ) / sites.size();
    std::ostringstream out;
    out << "bandit_live_world evidence debug:"
        << " now_minutes=" << current_minutes
        << " site_cap=" << site_cap
        << " lead_cap=" << lead_cap
        << " observation_cap=" << observation_cap
        << " rotation_hour=" << rotation_hour
        << " rotation_cycle=" << rotation_cycle
        << " evidence_sites_total=" << sites.size()
        << " sites_rendered=" << rendered_sites
        << " sites_omitted=" << sites.size() - rendered_sites
        << " start=" << start << '\n';

    for( std::size_t site_offset = 0; site_offset < rendered_sites; ++site_offset ) {
        const site_record &site = *sites[( start + site_offset ) % sites.size()].site;
        std::vector<const camp_map_lead *> leads;
        leads.reserve( site.intelligence_map.leads.size() );
        for( const camp_map_lead &lead : site.intelligence_map.leads ) {
            leads.push_back( &lead );
        }
        std::sort( leads.begin(), leads.end(), []( const camp_map_lead *lhs,
        const camp_map_lead *rhs ) {
            return std::make_tuple( lhs->lead_id, lhs->revision, lhs->omt.x(), lhs->omt.y(),
                                    lhs->omt.z(), lhs->origin, lhs->source_key,
                                    lhs->last_seen_minutes ) <
                   std::make_tuple( rhs->lead_id, rhs->revision, rhs->omt.x(), rhs->omt.y(),
                                    rhs->omt.z(), rhs->origin, rhs->source_key,
                                    rhs->last_seen_minutes );
        } );

        struct observation_view {
            std::string_view scope;
            const sortie_observation *observation;
        };
        std::vector<observation_view> observations;
        observations.reserve( site.active_outing.observations.size() +
                              site.current_scout_report.observations.size() );
        for( const sortie_observation &observation : site.active_outing.observations ) {
            observations.push_back( { "active", &observation } );
        }
        for( const sortie_observation &observation : site.current_scout_report.observations ) {
            observations.push_back( { "report", &observation } );
        }
        std::sort( observations.begin(), observations.end(),
        []( const observation_view &lhs, const observation_view &rhs ) {
            const sortie_observation &left = *lhs.observation;
            const sortie_observation &right = *rhs.observation;
            return std::make_tuple( lhs.scope, left.fact_key, left.observed_minutes,
                                    left.source_id, left.observer_id.get_value(),
                                    left.source_omt.x(), left.source_omt.y(), left.source_omt.z(),
                                    left.receiver_omt.x(), left.receiver_omt.y(),
                                    left.receiver_omt.z(), left.record_schema_version,
                                    left.sense, left.share_state ) <
                   std::make_tuple( rhs.scope, right.fact_key, right.observed_minutes,
                                    right.source_id, right.observer_id.get_value(),
                                    right.source_omt.x(), right.source_omt.y(), right.source_omt.z(),
                                    right.receiver_omt.x(), right.receiver_omt.y(),
                                    right.receiver_omt.z(), right.record_schema_version,
                                    right.sense, right.share_state );
        } );

        const std::size_t rendered_leads = std::min( leads.size(), lead_cap );
        const std::size_t rendered_observations = std::min( observations.size(), observation_cap );
        const std::size_t lead_start = leads.empty() ? 0 : rotation_cycle % leads.size();
        const std::size_t observation_start = observations.empty() ? 0 :
                                              rotation_cycle % observations.size();
        out << "site id=" << token( site.site_id )
            << " anchor=" << omt( site.anchor )
            << " leads_total=" << leads.size()
            << " leads_rendered=" << rendered_leads
            << " leads_omitted=" << leads.size() - rendered_leads
            << " lead_start=" << lead_start
            << " observations_total=" << observations.size()
            << " observations_rendered=" << rendered_observations
            << " observations_omitted=" << observations.size() - rendered_observations
            << " observation_start=" << observation_start << '\n';

        for( std::size_t lead_offset = 0; lead_offset < rendered_leads; ++lead_offset ) {
            const camp_map_lead &lead = *leads[( lead_start + lead_offset ) % leads.size()];
            out << " lead id=" << token( lead.lead_id )
                << " revision=" << lead.revision
                << " last_known_omt=" << omt( lead.omt )
                << " origin=" << to_string( lead.origin )
                << " source_key=" << token( lead.source_key )
                << " last_seen_minutes=";
            if( lead.last_seen_minutes < 0 ) {
                out << "unknown";
            } else {
                out << lead.last_seen_minutes;
            }
            out << " age_minutes=" << age( lead.last_seen_minutes );
            if( returned_structural_signal_lead( lead ) && lead.last_seen_minutes >= 0 ) {
                const int horizon_minutes = lead.kind == camp_lead_kind::sound_signal ?
                                            3 * 60 : 6 * 60;
                const int expiry_minutes = minutes_after_saturated( lead.last_seen_minutes,
                                           horizon_minutes );
                const char *state_name = lead.last_seen_minutes > current_minutes ? "future" :
                                         expiry_minutes <= current_minutes ? "expired" : "fresh";
                out << " expiry_minutes=" << expiry_minutes
                    << " remaining_minutes=" << signed_delta( expiry_minutes, current_minutes )
                    << " state=" << state_name << '\n';
            } else if( returned_structural_signal_lead( lead ) ) {
                out << " expiry_minutes=unknown remaining_minutes=unknown state=unknown\n";
            } else {
                out << " expiry_minutes=none remaining_minutes=none state=unbounded\n";
            }
        }

        for( std::size_t observation_offset = 0;
             observation_offset < rendered_observations; ++observation_offset ) {
            const observation_view &view = observations[( observation_start + observation_offset ) %
                                           observations.size()];
            const sortie_observation &observation = *view.observation;
            std::string freshness;
            if( observation.expiry_minutes < 0 ) {
                freshness = "unbounded";
            } else if( observation.observed_minutes > current_minutes ) {
                freshness = "future";
            } else if( observation.expiry_minutes < current_minutes ) {
                freshness = "expired";
            } else {
                freshness = "fresh";
            }
            out << " observation scope=" << view.scope
                << " fact_key=" << token( observation.fact_key )
                << " source_omt=" << omt( observation.source_omt )
                << " receiver_omt=" << omt( observation.receiver_omt )
                << " schema=" << observation.record_schema_version
                << " source_id=" << token( observation.source_id )
                << " observer=" << observation.observer_id.get_value()
                << " sense=" << sortie_observation_sense_to_string( observation.sense )
                << " share=" << sortie_observation_share_state_to_string( observation.share_state )
                << " observed_minutes=";
            if( observation.observed_minutes < 0 ) {
                out << "unknown";
            } else {
                out << observation.observed_minutes;
            }
            out << " age_minutes=" << age( observation.observed_minutes )
                << " expiry_minutes=";
            if( observation.expiry_minutes < 0 ) {
                out << "unknown remaining_minutes=unknown";
            } else {
                out << observation.expiry_minutes
                    << " remaining_minutes=" << signed_delta( observation.expiry_minutes,
                            current_minutes );
            }
            out << " state=" << freshness << '\n';
        }
    }
    return out.str();
}

tripoint_abs_omt reachable_ground_dispatch_target( const site_record &site,
        const tripoint_abs_omt &target_omt )
{
    if( effective_profile( site ) == hostile_site_profile::none ) {
        return target_omt;
    }

    int route_z = site.anchor.z();
    for( const tripoint_abs_omt &foot : site.footprint ) {
        if( foot.z() == 0 ) {
            route_z = 0;
            break;
        }
    }
    if( route_z > 0 ) {
        route_z = 0;
    }

    return tripoint_abs_omt( target_omt.x(), target_omt.y(), route_z );
}

dispatch_plan plan_site_dispatch_from_camp_map_lead( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.profile = rules.profile;
    plan.target_id = lead.target_id.empty() ? lead.lead_id : lead.target_id;
    plan.target_lead_id = lead.lead_id;
    plan.target_lead_revision = lead.revision;
    const tripoint_abs_omt original_target_omt = lead.omt;
    plan.target_omt = reachable_ground_dispatch_target( site, original_target_omt );

    if( returned_structural_signal_lead( lead ) ) {
        plan.notes.push_back(
            "camp-map dispatch blocked: returned signal belongs to bounded routine investigation" );
        return plan;
    }
    if( plan.target_id.empty() ) {
        plan.notes.push_back( "camp-map dispatch blocked: missing remembered target id" );
        return plan;
    }

    const camp_map_dispatch_decision decision = choose_camp_map_dispatch( site, lead, pressure );
    plan.notes = decision.notes;
    plan.notes.push_back( "camp-map dispatch lead=" + ( lead.lead_id.empty() ? plan.target_id : lead.lead_id ) +
                          " status=" + to_string( lead.status ) +
                          " bounty=" + std::to_string( lead.bounty ) +
                          " threat=" + std::to_string( lead.threat ) +
                          " confidence=" + std::to_string( lead.confidence ) +
                          " selected=" + bandit_dry_run::to_string( decision.intent ) );
    if( decision.intent == bandit_dry_run::job_template::hold_chill ||
        decision.selected_member_count <= 0 ) {
        plan.notes.push_back( "camp-map dispatch blocked: remembered risk/reward decision held pressure" );
        return plan;
    }
    if( !camp_decision_allows_dispatch( site.camp_decision, decision.intent ) ) {
        plan.notes.push_back( "camp-map dispatch blocked: camp decision state " +
                              to_string( site.camp_decision.state ) );
        return plan;
    }

    int selected_member_count = decision.selected_member_count;
    if( is_routine_scout_job( decision.intent ) ) {
        const routine_scout_policy_result routine_policy = routine_scout_policy( site );
        const bool micro_site = effective_profile( site ) ==
                                hostile_site_profile::small_hostile_site;
        const int required_routine_members = micro_site ? 1 : routine_policy.party_size;
        if( ( !routine_policy.eligible && !micro_site ) ||
            selected_member_count != required_routine_members ) {
            plan.notes.push_back( "camp-map dispatch blocked: routine pair policy is not eligible" );
            return plan;
        }
        selected_member_count = required_routine_members;
    } else {
        const response_party_policy_result response_policy = response_party_policy(
                    site, decision.intent, selected_member_count );
        if( !response_policy.eligible ) {
            plan.notes.push_back( "camp-map dispatch blocked: response party policy: " +
                                  response_policy.rejection_reason );
            return plan;
        }
        selected_member_count = response_policy.party_size;
    }

    if( is_routine_scout_job( decision.intent ) ) {
        const routine_scout_pair_selection_result pair_selection =
            select_routine_scout_pair( site );
        if( !pair_selection.eligible ) {
            plan.notes.push_back( "camp-map dispatch blocked: " +
                                  pair_selection.rejection_reason );
            return plan;
        }
        plan.member_ids = pair_selection.member_ids;
        plan.notes.push_back( "routine roles observer=" +
                              std::to_string( pair_selection.observer_id.get_value() ) +
                              " escort=" + std::to_string( pair_selection.escort_id.get_value() ) );
    } else {
        plan.member_ids = select_dispatch_members( site, selected_member_count );
    }
    if( static_cast<int>( plan.member_ids.size() ) != selected_member_count ) {
        plan.notes.push_back( "camp-map dispatch blocked: not enough at-home members survived selection" );
        return plan;
    }

    bandit_dry_run::candidate_debug winner = make_camp_map_dispatch_candidate( lead, decision );
    if( !bandit_pursuit_handoff::supports_pursuit_handoff( winner ) ) {
        plan.notes.push_back( "camp-map dispatch blocked: remembered decision stayed outside bounded scout/stalk handoff" );
        return plan;
    }

    plan.group = make_dispatch_group( site, plan.member_ids, plan.target_id );
    plan.group.current_threat_estimate = std::max( 0, lead.threat );
    plan.group.current_bounty_estimate = std::max( 0, lead.bounty );
    plan.group.confidence = std::max( plan.group.confidence, lead.confidence );
    bandit_pursuit_handoff::entry_context context;
    context.contact = rl_dist( site.anchor, plan.target_omt ) <= 4 ?
                      bandit_pursuit_handoff::contact_certainty::localized :
                      bandit_pursuit_handoff::contact_certainty::broad;
    plan.entry = bandit_pursuit_handoff::build_entry_payload( plan.group, winner, context );
    plan.notes = plan.entry.notes;
    if( plan.target_omt != original_target_omt ) {
        plan.notes.push_back( "vertical dispatch fallback: target " + original_target_omt.to_string() +
                              " routes via reachable ground " + plan.target_omt.to_string() );
    }
    if( !plan.entry.valid ) {
        plan.notes.push_back( "camp-map dispatch blocked: entry payload stayed outside the bounded handoff contract" );
        return plan;
    }

    plan.valid = true;
    plan.notes.push_back( "camp-map dispatch ready: " + bandit_dry_run::to_string( decision.intent ) +
                          " toward " + plan.target_id +
                          " members=" + std::to_string( plan.member_ids.size() ) +
                          " reserve=" + std::to_string( decision.hard_home_reserve ) +
                          " dispatchable=" + std::to_string( decision.dispatchable ) );
    plan.notes.push_back( "profile " + rules.id + ": " + rules.writeback_expectation );
    return plan;
}

dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::live_dispatch_plan );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::live_dispatch_plans );
    bandit_live_world_probe::record_site_service( site.site_id,
            bandit_live_world_probe::site_service::live_dispatch_plan );
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.profile = rules.profile;
    plan.target_id = target_id;
    const tripoint_abs_omt original_target_omt = target_omt;
    plan.target_omt = reachable_ground_dispatch_target( site, original_target_omt );
    if( const camp_map_lead *remembered_lead = find_camp_map_dispatch_lead_for_target(
            site, target_omt, target_id ) ) {
        plan.target_lead_id = remembered_lead->lead_id;
        plan.target_lead_revision = remembered_lead->revision;
    }

    if( target_id.empty() ) {
        plan.notes.push_back( "dispatch blocked: missing target id" );
        return plan;
    }

    if( site.retired_empty_site ) {
        plan.notes.push_back( "dispatch blocked: retired_empty_site" );
        return plan;
    }

    if( site.has_active_outside_pressure() ) {
        plan.notes.push_back( "dispatch blocked: site already has an active outside group/contact" );
        return plan;
    }

    bandit_dry_run::camp_input camp = make_dispatch_camp_input( site );
    const routine_scout_policy_result routine_policy = routine_scout_policy( site );
    if( routine_policy.eligible ) {
        camp.available_manpower = std::max( camp.available_manpower,
                                            routine_policy.party_size );
        if( camp.available_manpower == routine_policy.party_size ) {
            camp.shortage = bandit_dry_run::shortage_band::low;
        }
    }
    if( camp.available_manpower <= 0 ) {
        plan.notes.push_back( "dispatch blocked: no dispatchable at-home members remain after home reserve" );
        return plan;
    }

    const bandit_dry_run::lead_input lead = make_nearby_target_lead( site, plan.target_omt, target_id );
    plan.evaluation = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug &winner = plan.evaluation.candidates[plan.evaluation.winner_index];
    if( !camp_decision_allows_dispatch( site.camp_decision, winner.job ) ) {
        plan.notes.push_back( "dispatch blocked: camp decision state " +
                              to_string( site.camp_decision.state ) );
        return plan;
    }
    if( !bandit_pursuit_handoff::supports_pursuit_handoff( winner ) ) {
        plan.notes.push_back( "dispatch blocked: " + plan.evaluation.winner_reason );
        return plan;
    }

    int required_members = 0;
    int selected_reserve = 0;
    if( is_routine_scout_job( winner.job ) ) {
        const bool micro_site = rules.profile == hostile_site_profile::small_hostile_site;
        if( !routine_policy.eligible && !micro_site ) {
            plan.notes.push_back( "dispatch blocked: routine pair policy: " +
                                  routine_policy.rejection_reason );
            return plan;
        }
        required_members = micro_site ? 1 : routine_policy.party_size;
        selected_reserve = micro_site ? 0 : routine_policy.required_local_reserve;
    } else {
        const response_party_policy_result response_policy = response_party_policy( site, winner.job );
        if( !response_policy.eligible ) {
            plan.notes.push_back( "dispatch blocked: response party policy: " +
                                  response_policy.rejection_reason );
            return plan;
        }
        required_members = response_policy.party_size;
        selected_reserve = response_policy.required_local_reserve;
    }

    if( is_routine_scout_job( winner.job ) && rules.profile !=
        hostile_site_profile::small_hostile_site ) {
        const routine_scout_pair_selection_result pair_selection =
            select_routine_scout_pair( site );
        if( !pair_selection.eligible ) {
            plan.notes.push_back( "dispatch blocked: " + pair_selection.rejection_reason );
            return plan;
        }
        plan.member_ids = pair_selection.member_ids;
        plan.notes.push_back( "routine roles observer=" +
                              std::to_string( pair_selection.observer_id.get_value() ) +
                              " escort=" + std::to_string( pair_selection.escort_id.get_value() ) );
    } else {
        plan.member_ids = select_dispatch_members( site, required_members );
    }
    if( static_cast<int>( plan.member_ids.size() ) != required_members ) {
        plan.notes.push_back( "dispatch blocked: not enough at-home members survived selection" );
        return plan;
    }

    plan.group = make_dispatch_group( site, plan.member_ids, target_id );
    bandit_pursuit_handoff::entry_context context;
    context.contact = rl_dist( site.anchor, plan.target_omt ) <= 4 ?
                      bandit_pursuit_handoff::contact_certainty::localized :
                      bandit_pursuit_handoff::contact_certainty::broad;
    plan.entry = bandit_pursuit_handoff::build_entry_payload( plan.group, winner, context );
    plan.notes = plan.entry.notes;
    if( plan.target_omt != original_target_omt ) {
        plan.notes.push_back( "vertical dispatch fallback: target " + original_target_omt.to_string() +
                              " routes via reachable ground " + plan.target_omt.to_string() );
    }
    if( !plan.entry.valid ) {
        plan.notes.push_back( "dispatch blocked: entry payload stayed outside the bounded handoff contract" );
        return plan;
    }

    plan.valid = true;
    plan.notes.push_back( "profile " + rules.id + ": reserve " +
                          std::to_string( selected_reserve ) +
                          ", retreat_floor " + std::to_string( rules.retreat_bias_floor ) +
                          ", return_clock_floor " + std::to_string( rules.return_clock_floor ) );
    plan.notes.push_back( "profile writeback: " + rules.writeback_expectation );
    if( rules.profile == hostile_site_profile::cannibal_camp ) {
        plan.notes.push_back( "cannibal_camp pack pressure: pack_size " +
                              std::to_string( plan.member_ids.size() ) +
                              ", available_after_reserve " + std::to_string( camp.available_manpower ) );
    }
    plan.notes.push_back( "dispatch ready: " + bandit_dry_run::to_string( winner.job ) + " toward " + target_id );
    return plan;
}

hostile_operation_plan plan_hostile_operation( const site_record &site,
        const hostile_operation_kind operation_kind,
        const std::vector<tripoint_abs_omt> &route,
        const tripoint_abs_omt &rally_omt, const int current_minutes )
{
    hostile_operation_plan plan;
    const camp_report_policy expected_policy = report_policy_for_profile(
                effective_profile( site ) );
    const hostile_operation_kind expected_kind = operation_kind_for_report_policy(
                site.camp_decision.report_policy );
    if( site.retired_empty_site || site.has_active_outside_pressure() ||
        site.camp_decision.state != camp_decision_state::preparing_follow_on ||
        site.camp_decision.report_policy != expected_policy ||
        !report_matches_camp_decision( site.current_scout_report, site.camp_decision ) ||
        operation_kind != expected_kind || current_minutes < 0 ||
        current_minutes < site.camp_decision.last_transition_minutes ) {
        plan.notes.push_back( "hostile operation blocked: camp decision/report/profile is not ready" );
        return plan;
    }
    const response_party_selection_result selection = select_fresh_response_party(
                site, operation_kind );
    if( !selection.eligible ) {
        plan.notes.push_back( "hostile operation blocked: " + selection.rejection_reason );
        return plan;
    }
    if( route.size() < 2 || route.size() > max_active_outing_route_steps ||
        route.front() != site.anchor || route.back() != site.camp_decision.target_omt ||
        std::find( route.begin(), route.end(), rally_omt ) == route.end() ) {
        plan.notes.push_back( "hostile operation blocked: route/rally does not connect camp to report target" );
        return plan;
    }

    hostile_operation_state &operation = plan.operation;
    active_outing_state &reservation = operation.reservation;
    operation.operation_kind = operation_kind;
    operation.phase = hostile_operation_phase::assembling;
    operation.source_report_revision = site.current_scout_report.revision;
    operation.source_report_generation = site.current_scout_report.source_generation;
    operation.source_report_activity_id = site.current_scout_report.source_activity_id;
    operation.source_report_application_key = site.current_scout_report.application_key;
    operation.has_rally = true;
    operation.rally_omt = rally_omt;
    operation.last_transition_reason = "fresh hostile operation reserved from final scout report";
    reservation.kind = outing_kind::hostile_operation;
    reservation.activity_id = site.site_id + "#hostile:" +
                              std::to_string( site.next_outing_generation );
    reservation.camp_id = site.site_id;
    reservation.generation = site.next_outing_generation;
    reservation.member_ids = selection.member_ids;
    reservation.leader_id = selection.member_ids.front();
    reservation.shared_route = route;
    reservation.waypoint_index = 0;
    reservation.target_id = site.camp_decision.target_id;
    reservation.target_omt = site.camp_decision.target_omt;
    reservation.job_type = operation_kind == hostile_operation_kind::raid ? "raid" : "toll";
    reservation.target_lead_id = site.camp_decision.target_lead_id;
    reservation.target_lead_revision = site.camp_decision.target_lead_revision;
    reservation.phase = scout_phase::assembling;
    reservation.started_minutes = current_minutes;
    reservation.last_progress_minutes = current_minutes;
    reservation.last_advanced_minutes = current_minutes;
    reservation.owner = simulation_owner::abstract;
    reservation.return_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            reservation.activity_id, reservation.generation, "return" );
    reservation.report_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            reservation.activity_id, reservation.generation, "report" );
    reservation.cargo_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            reservation.activity_id, reservation.generation, "cargo" );
    plan.valid = true;
    plan.notes.push_back( "hostile operation ready: fresh report-pinned party at rally " +
                          rally_omt.to_string() );
    return plan;
}

bool apply_hostile_operation_plan( site_record &site, const hostile_operation_plan &plan )
{
    const hostile_operation_state &operation = plan.operation;
    const active_outing_state &reservation = operation.reservation;
    const camp_report_policy expected_policy = report_policy_for_profile(
                effective_profile( site ) );
    const hostile_operation_kind expected_kind = operation_kind_for_report_policy(
                site.camp_decision.report_policy );
    const std::string expected_activity_id = site.site_id + "#hostile:" +
            std::to_string( site.next_outing_generation );
    const bool rally_is_on_route = operation.has_rally &&
                                   std::find( reservation.shared_route.begin(),
                                           reservation.shared_route.end(),
                                           operation.rally_omt ) != reservation.shared_route.end();
    const response_party_selection_result selection = select_fresh_response_party(
                site, operation.operation_kind );
    if( !plan.valid || site.retired_empty_site || site.has_active_outside_pressure() ||
        site.camp_decision.state != camp_decision_state::preparing_follow_on ||
        site.camp_decision.report_policy != expected_policy ||
        !report_matches_camp_decision( site.current_scout_report, site.camp_decision ) ||
        !report_matches_hostile_operation( site.current_scout_report, operation ) ||
        operation.operation_kind != expected_kind || operation.legacy_unpinned ||
        operation.phase != hostile_operation_phase::assembling ||
        reservation.kind != outing_kind::hostile_operation ||
        reservation.phase != scout_phase::assembling || reservation.camp_id != site.site_id ||
        reservation.activity_id != expected_activity_id ||
        reservation.generation != site.next_outing_generation ||
        !selection.eligible || reservation.member_ids != selection.member_ids ||
        reservation.member_ids.size() < 2 ||
        reservation.member_ids.size() > max_hostile_operation_members ||
        !hostile_operation_party_preserves_home( site, reservation.member_ids.size() ) ||
        reservation.shared_route.size() < 2 ||
        reservation.shared_route.size() > max_active_outing_route_steps ||
        reservation.shared_route.front() != site.anchor ||
        reservation.shared_route.back() != site.camp_decision.target_omt ||
        !rally_is_on_route || reservation.target_id != site.camp_decision.target_id ||
        reservation.target_omt != site.camp_decision.target_omt ||
        reservation.target_lead_id != site.camp_decision.target_lead_id ||
        reservation.target_lead_revision != site.camp_decision.target_lead_revision ||
        !hostile_operation_job_matches( operation.operation_kind, reservation.job_type ) ||
        reservation.started_minutes < site.camp_decision.last_transition_minutes ||
        reservation.started_minutes != reservation.last_progress_minutes ||
        reservation.started_minutes != reservation.last_advanced_minutes ||
        reservation.owner != simulation_owner::abstract || reservation.handoff_epoch != 0 ||
        reservation.return_application_key !=
        bandit_pursuit_handoff::make_operation_component_key(
            reservation.activity_id, reservation.generation, "return" ) ||
        reservation.report_application_key !=
        bandit_pursuit_handoff::make_operation_component_key(
            reservation.activity_id, reservation.generation, "report" ) ||
        reservation.cargo_application_key !=
        bandit_pursuit_handoff::make_operation_component_key(
            reservation.activity_id, reservation.generation, "cargo" ) ||
        !reservation.observations.empty() || !reservation.casualty_ids.empty() ||
        !reservation.resolved_member_ids.empty() || reservation.cargo.supply_units != 0 ||
        reservation.cargo.trade_value != 0 ) {
        return false;
    }

    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( reservation.member_ids.size() );
    for( const character_id &member_id : reservation.member_ids ) {
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home ||
            member->wounded_or_unready ||
            std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    site_record candidate = site;
    candidate.active_hostile_operation = operation;
    candidate.next_outing_generation++;
    const camp_map_lead *lead = find_camp_map_dispatch_lead_for_target(
                                    candidate, reservation.target_omt,
                                    reservation.target_id );
    if( lead != nullptr ) {
        candidate.remembered_target_or_mark = reservation.target_id;
        candidate.remembered_threat_estimate = std::max( 0, lead->threat );
        candidate.remembered_bounty_estimate = std::max( 0, lead->bounty );
        push_unique_mark( candidate.known_recent_marks, reservation.target_id );
    }
    site = std::move( candidate );
    return true;
}

bool apply_dispatch_plan( site_record &site, const dispatch_plan &plan )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::live_dispatch_apply );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::live_dispatch_applies );
    const camp_map_lead *referenced_lead = plan.target_lead_id.empty() ? nullptr :
            site.intelligence_map.find_lead( plan.target_lead_id );
    const bool referenced_lead_is_current = plan.target_lead_id.empty() ?
                                            plan.target_lead_revision == 0 :
                                            referenced_lead != nullptr &&
                                            referenced_lead->revision == plan.target_lead_revision;
    const routine_scout_policy_result routine_policy = routine_scout_policy( site );
    const routine_scout_pair_selection_result pair_selection = select_routine_scout_pair( site );
    const bool routine_party_size_is_valid = routine_policy.applies ?
            routine_policy.eligible && pair_selection.eligible &&
            plan.member_ids == pair_selection.member_ids :
            effective_profile( site ) == hostile_site_profile::small_hostile_site &&
            plan.member_ids.size() == 1;
    if( !plan.valid || !referenced_lead_is_current || !routine_party_size_is_valid ||
        plan.entry.job_type != bandit_dry_run::job_template::scout ||
        plan.site_id != site.site_id || plan.member_ids.empty() ||
        plan.member_ids.size() > max_active_outing_members ||
        !camp_decision_allows_dispatch( site.camp_decision, plan.entry.job_type ) ||
        site.has_active_outside_pressure() || plan.group.activity_generation != site.next_outing_generation ||
        plan.entry.activity_generation != plan.group.activity_generation ||
        plan.group.handoff_epoch != 0 ||
        plan.entry.handoff_epoch != plan.group.handoff_epoch ||
        plan.group.activity_generation <= site.applied_return_generation ||
        plan.entry.return_application_key !=
        bandit_pursuit_handoff::make_operation_component_key(
            plan.entry.group_id, plan.entry.activity_generation, "return" ) ||
        plan.entry.report_application_key !=
        bandit_pursuit_handoff::make_operation_component_key(
            plan.entry.group_id, plan.entry.activity_generation, "report" ) ||
        plan.entry.cargo_application_key !=
        bandit_pursuit_handoff::make_operation_component_key(
            plan.entry.group_id, plan.entry.activity_generation, "cargo" ) ||
        plan.entry.return_application_key != plan.group.return_application_key ||
        plan.entry.report_application_key != plan.group.report_application_key ||
        plan.entry.cargo_application_key != plan.group.cargo_application_key ) {
        return false;
    }

    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( plan.member_ids.size() );
    for( const character_id &member_id : plan.member_ids ) {
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home || member->wounded_or_unready ||
            std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    site_record candidate = site;
    const std::string summary = "dispatch " + bandit_dry_run::to_string( plan.entry.job_type ) +
                                " toward " + plan.target_id;
    for( const character_id &member_id : plan.member_ids ) {
        if( !update_member_state( candidate, member_id, member_state::outbound, summary ) ) {
            return false;
        }
    }
    candidate.active_outing.clear();
    candidate.active_outing.kind = outing_kind::scout_sortie;
    candidate.active_outing.activity_id = plan.entry.group_id;
    candidate.active_outing.camp_id = candidate.site_id;
    candidate.active_outing.generation = plan.entry.activity_generation;
    candidate.active_outing.member_ids = plan.member_ids;
    candidate.active_outing.leader_id = plan.member_ids.front();
    candidate.active_outing.phase = scout_phase::outbound;
    candidate.active_outing.owner = simulation_owner::abstract;
    candidate.active_outing.handoff_epoch = plan.entry.handoff_epoch;
    candidate.active_outing.return_application_key = plan.entry.return_application_key;
    candidate.active_outing.report_application_key = plan.entry.report_application_key;
    candidate.active_outing.cargo_application_key = plan.entry.cargo_application_key;
    candidate.next_outing_generation++;
    candidate.active_outing.target_id = plan.target_id;
    candidate.active_outing.target_omt = plan.target_omt;
    candidate.active_outing.target_lead_id = plan.target_lead_id;
    candidate.active_outing.target_lead_revision = plan.target_lead_revision;
    candidate.active_outing.job_type = bandit_dry_run::to_string( plan.entry.job_type );
    candidate.active_outing.started_minutes = -1;
    candidate.active_outing.local_contact_minutes = -1;
    candidate.active_outing.last_progress_minutes = -1;
    candidate.remembered_target_or_mark = plan.entry.current_target_or_mark;
    candidate.remembered_threat_estimate = plan.group.current_threat_estimate;
    candidate.remembered_bounty_estimate = plan.group.current_bounty_estimate;
    candidate.remembered_retreat_bias = plan.group.retreat_bias;
    candidate.remembered_return_clock = plan.group.return_clock;
    candidate.remembered_pressure = plan.group.remaining_pressure;
    candidate.known_recent_marks = plan.group.known_recent_marks;
    if( !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

local_gate_decision choose_local_gate_posture( const site_record &site,
        const local_gate_input &input )
{
    local_gate_decision decision;
    const hostile_site_profile profile = effective_profile( site );
    const active_outing_state *outing = site.active_external_outing();
    decision.dispatch_strength = site.active_outing_survivor_count();
    decision.pressure_margin = decision.dispatch_strength + input.local_opportunity - input.local_threat;

    if( outing == nullptr || !outing->is_active() || outing->member_ids.empty() ) {
        decision.notes.push_back( "local gate blocked: no active owned outing is present" );
        return decision;
    }

    decision.valid = true;
    decision.notes.push_back( "active owned outing " + outing->activity_id + " toward " +
                              outing->target_id );
    decision.notes.push_back( "inputs: strength " + std::to_string( decision.dispatch_strength ) +
                              ", threat " + std::to_string( input.local_threat ) +
                              ", opportunity " + std::to_string( input.local_opportunity ) +
                              ", margin " + std::to_string( decision.pressure_margin ) );
    const std::optional<bandit_dry_run::job_template> active_job =
        job_template_from_string( outing->job_type );
    const bool cannibal_attack_intent = profile == hostile_site_profile::cannibal_camp &&
                                        site.active_hostile_operation.is_active() &&
                                        outing == &site.active_hostile_operation.reservation &&
                                        active_job.has_value() &&
                                        cannibal_job_requires_attack_pack( *active_job );
    decision.shakedown_capable = profile != hostile_site_profile::cannibal_camp &&
                                  !input.rolling_travel_scene && decision.dispatch_strength >= 1 &&
                                  decision.pressure_margin >= 2;
    if( decision.shakedown_capable ) {
        decision.notes.push_back(
            "bandit pressure is shakedown-capable once local contact is established" );
    }
    if( input.smoke_obscured_lead ) {
        decision.notes.push_back( std::string( "smoke-obscured lead: watcher_tile=" ) +
                                  ( input.smoke_on_watcher_tile ? "yes" : "no" ) +
                                  " sightline=" +
                                  ( input.smoke_between_watcher_and_camp ? "yes" : "no" ) );
    }

    if( input.rolling_travel_scene ) {
        if( profile == hostile_site_profile::cannibal_camp &&
            ( !cannibal_attack_intent || decision.dispatch_strength < 2 ) ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back( "rolling travel cannibal scout/probe contact stays below attack until a pack attack intent exists" );
            return decision;
        }
        if( decision.pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::attack_now;
            decision.combat_forward = true;
            decision.notes.push_back( "rolling travel scene skips polite shakedown and reads as an ambush window" );
            return decision;
        }
        decision.posture = local_gate_posture::probe;
        decision.notes.push_back( "rolling travel scene is tempting but still too protected for an immediate attack" );
        return decision;
    }

    if( site.last_shakedown_outcome == "fight_unresolved" && input.local_contact_established ) {
        decision.posture = local_gate_posture::attack_now;
        decision.combat_forward = true;
        decision.notes.push_back( "fight branch is already selected; active shakedown pressure commits to hostile contact instead of reopening the demand" );
        return decision;
    }

    if( decision.pressure_margin <= -3 ) {
        decision.posture = local_gate_posture::abort;
        decision.valid = false;
        decision.notes.push_back( "local gate aborts because local threat overwhelms dispatched pressure" );
        return decision;
    }

    if( input.smoke_obscured_lead ) {
        if( profile == hostile_site_profile::cannibal_camp ) {
            if( input.basecamp_or_camp_scene ) {
                decision.posture = local_gate_posture::hold_off;
                decision.notes.push_back(
                    "smoke/sight obscurity makes cannibal camp hold off instead of camping the smoked tile" );
                return decision;
            }
            if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
                decision.posture = local_gate_posture::probe;
                decision.notes.push_back(
                    "smoke-obscured cannibal lead probes around the concealment without opening a shakedown" );
                return decision;
            }
            decision.posture = local_gate_posture::stalk;
            decision.notes.push_back(
                "smoke-obscured cannibal lead stays cautious until the killing window is clearer" );
            return decision;
        }
        if( input.basecamp_or_camp_scene ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back(
                "smoke-obscured defended-camp watcher backs off/waits instead of camping the smoked tile" );
            return decision;
        }
        if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back(
                "smoke-obscured bandit lead becomes uncertain probe-around pressure rather than a clear beeline" );
            return decision;
        }
        decision.posture = local_gate_posture::stalk;
        decision.notes.push_back(
            "smoke-obscured bandit lead stays in cautious stalking until the lead clears" );
        return decision;
    }

    if( profile == hostile_site_profile::cannibal_camp ) {
        const int cannibal_pressure_margin = decision.pressure_margin +
                                             ( input.darkness_or_concealment ? 1 : 0 );
        if( input.darkness_or_concealment ) {
            decision.notes.push_back( "darkness/concealment improves the cannibal killing window without overriding pack or threat gates" );
        }
        if( input.basecamp_or_camp_scene && ( input.current_exposure || input.recent_exposure ) ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back( "sight/exposure makes the cannibal camp hold off instead of continuing a visible beeline" );
            return decision;
        }
        if( input.local_contact_established &&
            ( !cannibal_attack_intent || decision.dispatch_strength < 2 ) ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back(
                "cannibal camp refuses to turn scout/probe contact or a lone hunter into the whole attack pack" );
            return decision;
        }
        if( input.local_contact_established && cannibal_attack_intent && decision.dispatch_strength >= 2 &&
            cannibal_pressure_margin >= 1 ) {
            decision.posture = local_gate_posture::attack_now;
            decision.combat_forward = true;
            decision.notes.push_back( "cannibal camp pressure does not negotiate; favorable pack contact becomes attack-to-kill pressure" );
            return decision;
        }
        if( input.basecamp_or_camp_scene && !input.darkness_or_concealment &&
            decision.pressure_margin <= 0 ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back( "daylight/no-cover camp pressure holds off instead of becoming a suicide rush" );
            return decision;
        }
        if( input.local_opportunity > 0 && cannibal_pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back( "cannibal camp probes for a killing window instead of opening a shakedown" );
            return decision;
        }
        decision.posture = local_gate_posture::stalk;
        decision.notes.push_back( "cannibal camp pressure stalks until the kill window improves" );
        return decision;
    }

    if( input.basecamp_or_camp_scene &&
        ( input.current_exposure || input.recent_exposure || decision.pressure_margin <= 0 ) ) {
        decision.posture = local_gate_posture::hold_off;
        decision.notes.push_back( "camp-adjacent pressure holds off instead of collapsing onto the player tile" );
        return decision;
    }

    if( input.local_contact_established && decision.dispatch_strength >= 1 &&
        decision.pressure_margin >= 2 ) {
        decision.posture = local_gate_posture::open_shakedown;
        decision.opens_shakedown_surface = true;
        decision.notes.push_back( "contact is established and pressure is strong enough to open the later shakedown surface" );
        return decision;
    }

    if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
        decision.posture = local_gate_posture::probe;
        decision.notes.push_back( "opportunity is real but not yet strong enough for the robbery surface" );
        return decision;
    }

    decision.posture = local_gate_posture::stalk;
    decision.notes.push_back( "pressure stays readable as stalking until the scene changes" );
    return decision;
}

std::optional<int> target_footprint_watch_distance(
    const tripoint_abs_omt &observer_omt,
    const std::vector<tripoint_abs_omt> &target_footprint )
{
    std::optional<int> nearest_distance;
    for( const tripoint_abs_omt &target_omt : target_footprint ) {
        if( target_omt.z() != observer_omt.z() ) {
            continue;
        }
        const int distance = omt_chebyshev_distance( observer_omt, target_omt );
        if( !nearest_distance || distance < *nearest_distance ) {
            nearest_distance = distance;
        }
    }
    return nearest_distance;
}

std::optional<tripoint_abs_omt> nearest_target_footprint_omt(
    const tripoint_abs_omt &observer_omt,
    const std::vector<tripoint_abs_omt> &target_footprint )
{
    std::optional<tripoint_abs_omt> nearest;
    for( const tripoint_abs_omt &target_omt : target_footprint ) {
        if( target_omt.z() != observer_omt.z() ) {
            continue;
        }
        if( !nearest ||
            std::make_tuple( omt_chebyshev_distance( observer_omt, target_omt ),
                             target_omt.z(), target_omt.y(), target_omt.x() ) <
            std::make_tuple( omt_chebyshev_distance( observer_omt, *nearest ),
                             nearest->z(), nearest->y(), nearest->x() ) ) {
            nearest = target_omt;
        }
    }
    return nearest;
}

watch_selection_result select_exact_watch_ring_candidate(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates )
{
    watch_selection_result result;
    for( const watch_selection_candidate &candidate : candidates ) {
        if( !candidate.reachable || !candidate.concealed ||
            !candidate.two_intervening_omts_clear || candidate.route_cost < 0 ) {
            continue;
        }
        const std::optional<int> distance = target_footprint_watch_distance(
                candidate.omt, target_footprint );
        if( !distance || *distance != 3 ) {
            continue;
        }
        if( !result.valid ||
            std::make_tuple( candidate.route_cost, candidate.omt.z(), candidate.omt.y(),
                             candidate.omt.x() ) <
            std::make_tuple( result.route_cost, result.omt.z(), result.omt.y(), result.omt.x() ) ) {
            result.valid = true;
            result.omt = candidate.omt;
            result.footprint_distance = *distance;
            result.route_cost = candidate.route_cost;
            result.outcome = watch_selection_outcome::selected_exact;
        }
    }
    return result;
}

watch_selection_result select_watch_ring_candidate(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates )
{
    watch_selection_result result;
    if( target_footprint.empty() ) {
        result.outcome = watch_selection_outcome::abandoned_empty_target_footprint;
        return result;
    }

    result = select_exact_watch_ring_candidate( target_footprint, candidates );
    if( result.valid ) {
        return result;
    }

    constexpr int minimum_fallback_distance = 4;
    constexpr int maximum_fallback_distance = 5;
    for( const watch_selection_candidate &candidate : candidates ) {
        if( !candidate.reachable || !candidate.concealed ||
            !candidate.two_intervening_omts_clear || candidate.route_cost < 0 ) {
            continue;
        }
        const std::optional<int> distance = target_footprint_watch_distance(
                candidate.omt, target_footprint );
        if( !distance || *distance < minimum_fallback_distance ||
            *distance > maximum_fallback_distance ) {
            continue;
        }
        if( !result.valid ||
            std::make_tuple( *distance, candidate.route_cost, candidate.omt.z(),
                             candidate.omt.y(), candidate.omt.x() ) <
            std::make_tuple( result.footprint_distance, result.route_cost, result.omt.z(),
                             result.omt.y(), result.omt.x() ) ) {
            result.valid = true;
            result.omt = candidate.omt;
            result.footprint_distance = *distance;
            result.route_cost = candidate.route_cost;
            result.outcome = watch_selection_outcome::selected_fallback;
        }
    }
    return result;
}

watch_selection_result select_alternate_watch_ring_candidate(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates,
    const tripoint_abs_omt &selected_watch_omt )
{
    std::vector<watch_selection_candidate> alternatives;
    alternatives.reserve( candidates.size() );
    std::copy_if( candidates.begin(), candidates.end(),
                  std::back_inserter( alternatives ),
    [&selected_watch_omt]( const watch_selection_candidate & candidate ) {
        return candidate.omt != selected_watch_omt;
    } );
    return select_watch_ring_candidate( target_footprint, alternatives );
}

bool structural_watch_route_avoids_target_footprint(
    const std::vector<tripoint_abs_omt> &route,
    const std::vector<tripoint_abs_omt> &target_footprint )
{
    return !route.empty() && !target_footprint.empty() &&
           std::none_of( route.begin(), route.end(), [&target_footprint](
    const tripoint_abs_omt & omt ) {
        return std::find( target_footprint.begin(), target_footprint.end(), omt ) !=
               target_footprint.end();
    } );
}

bool structural_watch_shared_route_is_canonical(
    const std::vector<tripoint_abs_omt> &route,
    const tripoint_abs_omt &anchor, const tripoint_abs_omt &watch_omt,
    const std::vector<tripoint_abs_omt> &target_footprint )
{
    if( route.size() != 5 || route.front() != anchor || route.back() != anchor ||
        route[2] != watch_omt || route[1] != route[3] ||
        omt_chebyshev_distance( route[1], watch_omt ) != 1 ||
        watch_omt == anchor || target_footprint.empty() ||
        !structural_watch_route_avoids_target_footprint( route, target_footprint ) ) {
        return false;
    }
    for( std::size_t index = 1; index < route.size(); ++index ) {
        if( route[index] == route[index - 1] || route[index].z() != anchor.z() ) {
            return false;
        }
    }
    return true;
}

std::vector<tripoint_abs_omt> make_structural_watch_shared_route(
    const tripoint_abs_omt &anchor, const tripoint_abs_omt &watch_omt,
    const std::vector<tripoint_abs_omt> &reverse_path,
    const std::vector<tripoint_abs_omt> &target_footprint )
{
    if( reverse_path.size() < 3 || reverse_path.front() != watch_omt ||
        reverse_path.back() != anchor ||
        !structural_watch_route_avoids_target_footprint( reverse_path, target_footprint ) ) {
        return {};
    }
    const tripoint_abs_omt &approach_omt = reverse_path[1];
    std::vector<tripoint_abs_omt> route = {
        anchor, approach_omt, watch_omt, approach_omt, anchor
    };
    return structural_watch_shared_route_is_canonical(
               route, anchor, watch_omt, target_footprint ) ? route :
           std::vector<tripoint_abs_omt>();
}

structural_watch_geography_read read_structural_watch_geography(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const tripoint_abs_omt &route_origin,
    const std::function<structural_watch_terrain_read( const tripoint_abs_omt &,
            const std::vector<tripoint_abs_omt> & )> &terrain_lookup,
    const std::function<structural_watch_route_read( const tripoint_abs_omt & )> &route_lookup )
{
    structural_watch_geography_read read;
    if( target_footprint.empty() ||
        target_footprint.size() > max_structural_target_footprint_omts ||
        !terrain_lookup || !route_lookup ) {
        read.selection.outcome = watch_selection_outcome::abandoned_empty_target_footprint;
        return read;
    }

    read.target_footprint = canonical_structural_target_footprint( target_footprint );
    if( read.target_footprint.empty() ||
        std::any_of( read.target_footprint.begin(), read.target_footprint.end(),
    [&read]( const tripoint_abs_omt & omt ) {
        return omt.z() != read.target_footprint.front().z();
    } ) ) {
        read.target_footprint.clear();
        read.selection.outcome = watch_selection_outcome::abandoned_empty_target_footprint;
        return read;
    }
    read.valid_input = true;

    std::vector<tripoint_abs_omt> candidate_omts;
    candidate_omts.reserve( read.target_footprint.size() * 64 );
    for( const tripoint_abs_omt &target : read.target_footprint ) {
        for( int dy = -5; dy <= 5; ++dy ) {
            for( int dx = -5; dx <= 5; ++dx ) {
                candidate_omts.emplace_back( target.x() + dx, target.y() + dy, target.z() );
            }
        }
    }
    std::sort( candidate_omts.begin(), candidate_omts.end(), structural_watch_omt_precedes );
    candidate_omts.erase( std::unique( candidate_omts.begin(), candidate_omts.end() ),
                          candidate_omts.end() );
    candidate_omts.erase( std::remove_if( candidate_omts.begin(), candidate_omts.end(),
    [&read]( const tripoint_abs_omt & omt ) {
        const std::optional<int> distance = target_footprint_watch_distance(
                                                omt, read.target_footprint );
        return !distance || *distance < 3 || *distance > 5;
    } ), candidate_omts.end() );
    std::stable_sort( candidate_omts.begin(), candidate_omts.end(),
    [&read]( const tripoint_abs_omt & lhs, const tripoint_abs_omt & rhs ) {
        const int lhs_distance = *target_footprint_watch_distance(
                                     lhs, read.target_footprint );
        const int rhs_distance = *target_footprint_watch_distance(
                                     rhs, read.target_footprint );
        return lhs_distance != rhs_distance ? lhs_distance < rhs_distance :
               structural_watch_omt_precedes( lhs, rhs );
    } );
    const std::size_t unbounded_candidate_count = candidate_omts.size();
    std::vector<tripoint_abs_omt> bounded_candidate_omts;
    bounded_candidate_omts.reserve( max_structural_watch_candidates );
    const auto append_distance_band = [&read, &candidate_omts, &bounded_candidate_omts](
    const int distance, const std::size_t cap ) {
        std::size_t appended = 0;
        for( const tripoint_abs_omt &candidate : candidate_omts ) {
            if( appended >= cap ) {
                break;
            }
            if( target_footprint_watch_distance( candidate, read.target_footprint ) == distance ) {
                bounded_candidate_omts.push_back( candidate );
                appended++;
            }
        }
    };
    append_distance_band( 3, max_structural_watch_exact_terrain_reads );
    append_distance_band( 4, max_structural_watch_distance_four_terrain_reads );
    append_distance_band( 5, max_structural_watch_distance_five_terrain_reads );
    candidate_omts = std::move( bounded_candidate_omts );
    read.candidate_enumeration_truncated =
        unbounded_candidate_count > candidate_omts.size();
    read.candidate_omts_considered = static_cast<int>( candidate_omts.size() );

    std::vector<std::pair<tripoint_abs_omt, structural_watch_terrain_read>> qualified;
    qualified.reserve( candidate_omts.size() );
    for( const tripoint_abs_omt &candidate : candidate_omts ) {
        const structural_watch_terrain_read terrain = terrain_lookup(
                    candidate, read.target_footprint );
        read.terrain_reads++;
        if( terrain.concealed && terrain.intervening_omts_clear ) {
            qualified.emplace_back( candidate, terrain );
        }
    }

    const auto route_distance_group = [&read, &qualified, &route_lookup, &route_origin](
    const int distance, const int cap ) {
        std::vector<const std::pair<tripoint_abs_omt, structural_watch_terrain_read> *> group;
        for( const auto &entry : qualified ) {
            if( target_footprint_watch_distance( entry.first, read.target_footprint ) == distance ) {
                group.push_back( &entry );
            }
        }
        std::sort( group.begin(), group.end(), [&route_origin]( const auto *lhs, const auto *rhs ) {
            const auto origin_distance = [&route_origin]( const tripoint_abs_omt & omt ) {
                return std::max( { std::abs( route_origin.x() - omt.x() ),
                                   std::abs( route_origin.y() - omt.y() ),
                                   std::abs( route_origin.z() - omt.z() ) } );
            };
            const int lhs_distance = origin_distance( lhs->first );
            const int rhs_distance = origin_distance( rhs->first );
            return lhs_distance != rhs_distance ? lhs_distance < rhs_distance :
                   structural_watch_omt_precedes( lhs->first, rhs->first );
        } );
        int group_reads = 0;
        for( const auto *entry : group ) {
            if( group_reads >= cap ) {
                break;
            }
            const structural_watch_route_read route = route_lookup( entry->first );
            read.route_reads++;
            group_reads++;
            watch_selection_candidate candidate;
            candidate.omt = entry->first;
            candidate.reachable = route.reachable && route.route_cost >= 0;
            candidate.concealed = entry->second.concealed;
            candidate.two_intervening_omts_clear = entry->second.intervening_omts_clear;
            candidate.route_cost = route.route_cost;
            read.routed_candidates.push_back( candidate );
        }
    };

    route_distance_group( 3, max_structural_watch_exact_route_reads );
    read.selection = select_watch_ring_candidate(
                         read.target_footprint, read.routed_candidates );
    if( !read.selection.valid ) {
        route_distance_group( 4, max_structural_watch_distance_four_route_reads );
        read.selection = select_watch_ring_candidate(
                             read.target_footprint, read.routed_candidates );
    }
    if( !read.selection.valid ) {
        route_distance_group( 5, max_structural_watch_distance_five_route_reads );
        read.selection = select_watch_ring_candidate(
                             read.target_footprint, read.routed_candidates );
    }
    return read;
}

structural_watch_route_apply_result apply_structural_watch_route_selection(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates,
    const std::vector<tripoint_abs_omt> &alternate_watch_shared_route )
{
    const active_outing_state &current = site.active_outing;
    if( current.kind != outing_kind::structural_sortie || current.schema_version < 8 ||
        current.schema_version > 10 || !simulation_cursor_matches( current, expected_cursor ) ||
        ( current.phase != scout_phase::outbound && current.phase != scout_phase::searching ) ||
        target_footprint.empty() ||
        target_footprint.size() > max_structural_target_footprint_omts || candidates.empty() ||
        candidates.size() > max_structural_watch_candidates ) {
        return structural_watch_route_apply_result::rejected;
    }

    const std::vector<tripoint_abs_omt> canonical_footprint =
        canonical_structural_target_footprint( target_footprint );
    if( canonical_footprint.empty() ||
        std::find( canonical_footprint.begin(), canonical_footprint.end(),
                   current.target_omt ) == canonical_footprint.end() ||
        std::any_of( canonical_footprint.begin(), canonical_footprint.end(),
    [&current]( const tripoint_abs_omt & omt ) {
        return omt.z() != current.target_omt.z();
    } ) ) {
        return structural_watch_route_apply_result::rejected;
    }
    const watch_selection_result selection = select_watch_ring_candidate(
                canonical_footprint, candidates );
    if( !selection.valid || selection.route_cost < 0 ||
        ( selection.outcome != watch_selection_outcome::selected_exact &&
          selection.outcome != watch_selection_outcome::selected_fallback ) ) {
        return structural_watch_route_apply_result::rejected;
    }
    const std::optional<int> footprint_distance = target_footprint_watch_distance(
                selection.omt, canonical_footprint );
    const structural_watch_kind selected_kind =
        selection.outcome == watch_selection_outcome::selected_exact ?
        structural_watch_kind::exact : structural_watch_kind::fallback;
    const bool distance_matches_kind = footprint_distance &&
                                       selection.footprint_distance == *footprint_distance &&
            ( ( selected_kind == structural_watch_kind::exact && *footprint_distance == 3 ) ||
              ( selected_kind == structural_watch_kind::fallback &&
                *footprint_distance >= 4 && *footprint_distance <= 5 ) );
    if( !distance_matches_kind ) {
        return structural_watch_route_apply_result::rejected;
    }
    const watch_selection_result alternate_selection =
        select_alternate_watch_ring_candidate(
            canonical_footprint, candidates, selection.omt );
    const bool has_alternate_route = alternate_selection.valid &&
                                     !alternate_watch_shared_route.empty();
    const structural_watch_kind alternate_kind = !has_alternate_route ?
            structural_watch_kind::none :
            alternate_selection.outcome == watch_selection_outcome::selected_exact ?
            structural_watch_kind::exact : structural_watch_kind::fallback;
    if( ( !alternate_watch_shared_route.empty() && !alternate_selection.valid ) ||
        ( has_alternate_route &&
          !structural_watch_shared_route_is_canonical(
              alternate_watch_shared_route, site.anchor, alternate_selection.omt,
              canonical_footprint ) ) ) {
        return structural_watch_route_apply_result::rejected;
    }

    if( current.schema_version >= 9 &&
        current.selected_watch_kind != structural_watch_kind::none ) {
        return current.target_footprint == canonical_footprint &&
               current.selected_watch_kind == selected_kind &&
               current.selected_watch_omt == selection.omt &&
               current.selected_watch_route_cost == selection.route_cost &&
               current.alternate_watch_kind == alternate_kind &&
               current.alternate_watch_omt == ( has_alternate_route ?
                       alternate_selection.omt : tripoint_abs_omt() ) &&
               current.alternate_watch_route_cost == ( has_alternate_route ?
                       alternate_selection.route_cost : -1 ) &&
               current.alternate_watch_shared_route == alternate_watch_shared_route ?
               structural_watch_route_apply_result::unchanged :
               structural_watch_route_apply_result::rejected;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    next.schema_version = std::max( next.schema_version,
                                   has_alternate_route ? 10 : 9 );
    next.target_footprint = canonical_footprint;
    next.selected_watch_kind = selected_kind;
    next.selected_watch_omt = selection.omt;
    next.selected_watch_route_cost = selection.route_cost;
    next.alternate_watch_kind = alternate_kind;
    next.alternate_watch_omt = has_alternate_route ? alternate_selection.omt :
                               tripoint_abs_omt();
    next.alternate_watch_route_cost = has_alternate_route ?
                                      alternate_selection.route_cost : -1;
    next.alternate_watch_shared_route = alternate_watch_shared_route;
    if( has_alternate_route && next.started_minutes >= 0 ) {
        next.expected_return_minutes = structural_expected_return_minutes(
                                           next.started_minutes, candidate.anchor,
                                           selection.omt );
        next.missing_deadline_minutes = minutes_after_saturated(
                                            next.expected_return_minutes,
                                            scout_missing_grace_minutes );
    }
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return structural_watch_route_apply_result::rejected;
    }
    site = std::move( candidate );
    return structural_watch_route_apply_result::applied;
}

int ordinary_scout_watch_standoff_omt()
{
    return 5;
}

int minimum_hold_off_standoff_omt()
{
    return ordinary_scout_watch_standoff_omt();
}

tripoint_abs_omt choose_hold_off_standoff_goal( const tripoint_abs_omt &site_anchor,
        const tripoint_abs_omt &player_omt, const int requested_distance )
{
    const int desired_distance = std::max( requested_distance, minimum_hold_off_standoff_omt() );
    const int dx = ( site_anchor.x() > player_omt.x() ) -
                   ( site_anchor.x() < player_omt.x() );
    const int dy = ( site_anchor.y() > player_omt.y() ) -
                   ( site_anchor.y() < player_omt.y() );
    if( dx == 0 && dy == 0 ) {
        return player_omt;
    }
    return tripoint_abs_omt( player_omt.x() + dx * desired_distance,
                             player_omt.y() + dy * desired_distance, player_omt.z() );
}

bool hot_defended_doorstep_blocks_pickup( const site_record &site,
        const local_gate_input &input, const local_gate_decision &decision,
        const character_id &member_id )
{
    const active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr || !outing->is_active() || outing->member_ids.empty() ) {
        return false;
    }
    if( std::find( outing->member_ids.begin(), outing->member_ids.end(), member_id ) ==
        outing->member_ids.end() ) {
        return false;
    }
    if( !input.basecamp_or_camp_scene || input.rolling_travel_scene ) {
        return false;
    }
    if( decision.combat_forward || decision.opens_shakedown_surface ) {
        return false;
    }
    if( decision.posture != local_gate_posture::stalk &&
        decision.posture != local_gate_posture::hold_off ) {
        return false;
    }
    const bool hot_by_sight_smoke_or_doorstep = input.current_exposure || input.recent_exposure ||
            input.smoke_obscured_lead || input.standoff_distance <= 1;
    return hot_by_sight_smoke_or_doorstep;
}

std::string render_local_gate_report( const site_record &site, const local_gate_input &input,
                                      const local_gate_decision &decision )
{
    const active_outing_state *outing = site.active_external_outing();
    const active_outing_state empty_outing;
    if( outing == nullptr ) {
        outing = &empty_outing;
    }
    std::ostringstream out;
    out << "local_gate site=" << site.site_id
        << " active_group=" << outing->activity_id
        << " target=" << outing->target_id
        << " live_dispatch_goal=" << outing->target_omt.x() << ',' << outing->target_omt.y() << ','
        << outing->target_omt.z()
        << " active_job=" << ( outing->job_type.empty() ? "unknown" : outing->job_type )
        << " profile=" << to_string( effective_profile( site ) )
        << " posture=" << to_string( decision.posture )
        << " strength=" << decision.dispatch_strength
        << " pack_size=" << decision.dispatch_strength
        << " threat=" << input.local_threat
        << " opportunity=" << input.local_opportunity
        << " margin=" << decision.pressure_margin
        << " darkness_or_concealment=" << ( input.darkness_or_concealment ? "yes" : "no" )
        << " standoff_distance=" << input.standoff_distance
        << " basecamp_or_camp=" << ( input.basecamp_or_camp_scene ? "yes" : "no" )
        << " current_exposure=" << ( input.current_exposure ? "yes" : "no" )
        << " recent_exposure=" << ( input.recent_exposure ? "yes" : "no" )
        << " sight_exposure=" << ( input.current_exposure ? "current" :
                                      ( input.recent_exposure ? "recent" : "none" ) )
        << " smoke_obscured=" << ( input.smoke_obscured_lead ? "yes" : "no" )
        << " smoke_on_watcher=" << ( input.smoke_on_watcher_tile ? "yes" : "no" )
        << " smoke_sightline=" << ( input.smoke_between_watcher_and_camp ? "yes" : "no" )
        << " local_contact=" << ( input.local_contact_established ? "yes" : "no" )
        << " rolling_travel=" << ( input.rolling_travel_scene ? "yes" : "no" )
        << " shakedown_capable=" << ( decision.shakedown_capable ? "yes" : "no" )
        << " shakedown=" << ( decision.opens_shakedown_surface ? "yes" : "no" )
        << " combat_forward=" << ( decision.combat_forward ? "yes" : "no" )
        << '\n';
    for( const std::string &note : decision.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

sight_avoid_decision choose_sight_avoid_reposition( const tripoint_abs_ms &current_tile,
        const bool current_exposure, const bool recent_exposure,
        const std::vector<sight_avoid_candidate> &candidates,
        const bool current_smoke_obscured )
{
    sight_avoid_decision decision;
    decision.valid = true;
    decision.destination = current_tile;

    if( !current_exposure && !recent_exposure && !current_smoke_obscured ) {
        decision.reason = "still stalking";
        decision.notes.push_back(
            "sight_avoid: no current/recent exposure or smoke-obscured tile, no reposition needed" );
        return decision;
    }

    int best_score = -1000000;
    std::optional<sight_avoid_candidate> best_candidate;
    for( const sight_avoid_candidate &candidate : candidates ) {
        if( !candidate.passable || candidate.tile == current_tile || rl_dist( candidate.tile, current_tile ) > 1 ) {
            continue;
        }
        int score = candidate.cover_score;
        if( !candidate.visible_to_player ) {
            score += 80;
        }
        if( !candidate.visible_to_camp ) {
            score += 40;
        }
        if( current_smoke_obscured && !candidate.smoke_obscured ) {
            score += 60;
        }
        if( candidate.smoke_obscured ) {
            score -= 40;
        }
        if( candidate.visible_to_player && candidate.visible_to_camp ) {
            score -= 60;
        }
        if( score > best_score ) {
            best_score = score;
            best_candidate = candidate;
        }
    }

    if( !best_candidate.has_value() ) {
        decision.reason = current_smoke_obscured ?
                          "blocked: smoke-obscured no adjacent passable reposition candidate" :
                          "blocked: exposed no adjacent passable reposition candidate";
        decision.notes.push_back(
            current_smoke_obscured ?
            "sight_avoid: smoke-obscured but no adjacent passable local reposition candidate" :
            "sight_avoid: exposed but no adjacent passable local reposition candidate" );
        return decision;
    }

    const bool breaks_player_sight = !best_candidate->visible_to_player;
    const bool breaks_camp_sight = !best_candidate->visible_to_camp;
    const bool clears_smoke = current_smoke_obscured && !best_candidate->smoke_obscured;
    if( !breaks_player_sight && !breaks_camp_sight && !clears_smoke && best_candidate->cover_score <= 0 &&
        !current_smoke_obscured ) {
        decision.reason = "still stalking";
        decision.notes.push_back(
            "sight_avoid: exposed but adjacent candidates do not improve cover or line of sight" );
        return decision;
    }

    decision.repositions = true;
    decision.destination = best_candidate->tile;
    decision.reason = current_smoke_obscured ?
                      "repositioning because smoke obscures lead" : "repositioning because exposed";
    decision.notes.push_back( current_smoke_obscured ?
                              "sight_avoid: smoke-obscured -> bounded adjacent reposition" :
                              "sight_avoid: exposed -> bounded adjacent reposition" );
    decision.notes.push_back( std::string( "sight_avoid: breaks_player_los=" ) +
                              ( breaks_player_sight ? "yes" : "no" ) +
                              " breaks_camp_los=" + ( breaks_camp_sight ? "yes" : "no" ) +
                              " clears_smoke=" + ( clears_smoke ? "yes" : "no" ) +
                              " cover_score=" + std::to_string( best_candidate->cover_score ) );
    return decision;
}

bool note_active_sortie_started( site_record &site,
                                 const simulation_advance_cursor &expected_cursor,
                                 const int current_minutes )
{
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.member_ids.empty() || current_minutes < 0 ) {
        return false;
    }
    const scout_phase original_phase = site.active_outing.phase;
    site_record candidate = site;
    bool changed = false;
    if( candidate.active_outing.started_minutes < 0 ) {
        if( candidate.active_outing.kind == outing_kind::scout_sortie &&
            !is_valid_scout_phase_transition( candidate.active_outing.phase,
                                              scout_phase::outbound ) ) {
            return false;
        }
        if( candidate.active_outing.kind == outing_kind::scout_sortie ) {
            const scout_phase_transition_result transition = transition_active_scout_phase_impl(
                        candidate, expected_cursor, candidate.active_outing.phase,
                        scout_phase::outbound, current_minutes, "sortie departed", false );
            if( transition == scout_phase_transition_result::rejected ) {
                return false;
            }
        } else {
            candidate.active_outing.phase = scout_phase::outbound;
            candidate.active_outing.last_progress_minutes = current_minutes;
            candidate.active_outing.last_advanced_minutes = current_minutes;
        }
        candidate.active_outing.started_minutes = current_minutes;
        changed = true;
    }
    if( candidate.active_outing.expected_return_minutes < 0 ) {
        candidate.active_outing.expected_return_minutes = minutes_after_saturated(
                candidate.active_outing.started_minutes,
                ordinary_scout_sortie_limit_minutes() + scout_return_cohesion_minutes );
        changed = true;
    }
    if( candidate.active_outing.missing_deadline_minutes <
        candidate.active_outing.expected_return_minutes ) {
        candidate.active_outing.missing_deadline_minutes = minutes_after_saturated(
                candidate.active_outing.expected_return_minutes, scout_missing_grace_minutes );
        changed = true;
    }
    if( changed ) {
        if( current_minutes <= site.active_outing.last_advanced_minutes ) {
            return false;
        }
        candidate.active_outing.last_advanced_minutes = current_minutes;
        site = std::move( candidate );
        if( site.active_outing.phase != original_phase ) {
            record_scout_phase_transition_event( site.active_outing, original_phase,
                                                 site.active_outing.phase, "sortie departed",
                                                 current_minutes );
        }
    }
    return changed;
}

bool note_active_sortie_local_contact( site_record &site,
                                       const simulation_advance_cursor &expected_cursor,
                                       const character_id contact_member_id,
                                       const int current_minutes )
{
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.member_ids.empty() || current_minutes < 0 ||
        current_minutes < site.active_outing.last_advanced_minutes ||
        site.active_outing.member_is_resolved( contact_member_id ) ||
        std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                   contact_member_id ) == site.active_outing.member_ids.end() ) {
        return false;
    }
    const scout_phase original_phase = site.active_outing.phase;
    site_record candidate = site;
    const bool first_local_contact = site.active_outing.local_contact_minutes < 0;
    const bool same_minute_initial_handoff =
        first_local_contact &&
        current_minutes == site.active_outing.last_advanced_minutes &&
        site.active_outing.owner == simulation_owner::abstract &&
        site.active_outing.started_minutes == current_minutes;
    if( current_minutes == site.active_outing.last_advanced_minutes &&
        !same_minute_initial_handoff ) {
        return false;
    }
    member_record *contact_member = candidate.find_member( contact_member_id );
    if( contact_member == nullptr ||
        ( contact_member->state != member_state::outbound &&
          contact_member->state != member_state::local_contact ) ||
        ( !first_local_contact &&
          contact_member->state == member_state::local_contact ) ) {
        return false;
    }
    if( first_local_contact &&
        candidate.active_outing.kind == outing_kind::scout_sortie &&
        !is_valid_scout_phase_transition( candidate.active_outing.phase,
                                          scout_phase::observing ) ) {
        return false;
    }
    if( first_local_contact &&
        candidate.active_outing.kind == outing_kind::scout_sortie ) {
        if( same_minute_initial_handoff ) {
            if( !is_valid_scout_phase_transition( candidate.active_outing.phase,
                                                  scout_phase::observing ) ) {
                return false;
            }
            candidate.active_outing.phase = scout_phase::observing;
            candidate.active_outing.last_progress_minutes = current_minutes;
            candidate.active_outing.last_advanced_minutes = current_minutes;
        } else {
            const scout_phase_transition_result transition = transition_active_scout_phase_impl(
                        candidate, expected_cursor, candidate.active_outing.phase,
                        scout_phase::observing, current_minutes, "first local contact", false );
            if( transition == scout_phase_transition_result::rejected ) {
                return false;
            }
        }
    } else if( first_local_contact ) {
        candidate.active_outing.phase = scout_phase::observing;
        candidate.active_outing.last_progress_minutes = current_minutes;
        candidate.active_outing.last_advanced_minutes = current_minutes;
    }
    if( first_local_contact ) {
        candidate.active_outing.local_contact_minutes = current_minutes;
    } else {
        candidate.active_outing.last_progress_minutes = current_minutes;
        candidate.active_outing.last_advanced_minutes = current_minutes;
    }
    if( !update_member_state( candidate, contact_member_id, member_state::local_contact,
                              "local contact near " + candidate.active_outing.target_id ) ) {
        return false;
    }
    if( first_local_contact ) {
        candidate.active_outing.expected_return_minutes = minutes_after_saturated(
                current_minutes,
                ordinary_scout_sortie_limit_minutes() + scout_return_cohesion_minutes );
        candidate.active_outing.missing_deadline_minutes = minutes_after_saturated(
                candidate.active_outing.expected_return_minutes,
                scout_missing_grace_minutes );
    }
    if( candidate.active_outing.owner != simulation_owner::local ) {
        const simulation_owner_transition_result handoff =
            transition_external_simulation_owner(
                candidate, candidate.active_outing.activity_id,
                candidate.active_outing.generation, candidate.active_outing.owner,
                simulation_owner::local, candidate.active_outing.handoff_epoch,
                candidate.active_outing.last_advanced_minutes,
                current_minutes );
        if( handoff != simulation_owner_transition_result::applied ) {
            return false;
        }
    }
    site = std::move( candidate );
    if( site.active_outing.phase != original_phase ) {
        record_scout_phase_transition_event( site.active_outing, original_phase,
                                             site.active_outing.phase, "first local contact",
                                             current_minutes );
    }
    return true;
}

static sortie_observation_effect record_active_sortie_observations_impl( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<sortie_observation> &observations,
        const int current_minutes, const bool typed_observations )
{
    sortie_observation_effect effect;
    const bool supported_outing = typed_observations ?
                                  ( site.active_outing.kind == outing_kind::scout_sortie ||
                                    site.active_outing.kind == outing_kind::structural_sortie ) :
                                  site.active_outing.kind == outing_kind::scout_sortie;
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        !supported_outing ||
        ( site.active_outing.job_type != "scout" &&
          site.active_outing.job_type != "scavenge" ) ||
        site.active_outing.member_ids.empty() || observations.empty() ||
        observations.size() > max_sortie_observation_batch || current_minutes < 0 ||
        current_minutes <= site.active_outing.last_advanced_minutes ||
        current_minutes < site.active_outing.last_progress_minutes ) {
        return effect;
    }

    std::vector<sortie_observation> normalized_inputs;
    normalized_inputs.reserve( observations.size() );
    for( sortie_observation observation : observations ) {
        if( observation.record_schema_version != ( typed_observations ? 1 : 0 ) ) {
            return effect;
        }
        if( typed_observations ) {
            if( !typed_sortie_observation_is_valid( observation ) ||
                observation.observed_minutes > current_minutes ) {
                return effect;
            }
        } else {
            normalize_sortie_observation( observation );
            if( observation.fact_key.empty() || observation.observed_minutes > current_minutes ) {
                return effect;
            }
            if( observation.observed_minutes < 0 ) {
                observation.observed_minutes = current_minutes;
            }
        }
        normalized_inputs.push_back( std::move( observation ) );
    }

    const std::vector<sortie_observation> previous =
        make_bounded_sortie_observations( site.active_outing.observations );
    std::vector<sortie_observation> combined = previous;
    combined.insert( combined.end(), normalized_inputs.begin(), normalized_inputs.end() );
    const std::vector<sortie_observation> retained = make_bounded_sortie_observations( combined );

    for( const sortie_observation &observation : retained ) {
        const auto previous_iter = std::find_if( previous.begin(), previous.end(),
        [&observation]( const sortie_observation & prior ) {
            return sortie_observation_identity_matches( prior, observation );
        } );
        if( previous_iter == previous.end() ) {
            effect.inserted++;
        } else if( !sortie_observations_equal( *previous_iter, observation ) ) {
            effect.replaced++;
        }

        const auto input_iter = std::find_if( normalized_inputs.begin(), normalized_inputs.end(),
        [&observation]( const sortie_observation & input ) {
            return sortie_observation_identity_matches( input, observation ) &&
                   input.kind == observation.kind && input.state_key == observation.state_key;
        } );
        const bool typed_progress = observation.record_schema_version == 1 &&
                                    ( previous_iter == previous.end() ||
                                      !sortie_observations_equal( *previous_iter, observation ) );
        const bool legacy_progress = observation.record_schema_version == 0 &&
                                     sortie_observation_counts_as_progress( observation.kind ) &&
                                     ( previous_iter == previous.end() ||
                                       previous_iter->kind != observation.kind ||
                                       previous_iter->state_key != observation.state_key );
        if( input_iter != normalized_inputs.end() && ( typed_progress || legacy_progress ) ) {
            effect.progress = true;
        }
    }
    for( const sortie_observation &observation : previous ) {
        if( std::none_of( retained.begin(), retained.end(),
        [&observation]( const sortie_observation & current ) {
            return sortie_observation_identity_matches( current, observation );
        } ) ) {
            effect.evicted++;
        }
    }

    site_record candidate = site;
    candidate.active_outing.observations = retained;
    candidate.active_outing.last_advanced_minutes = current_minutes;
    consume_local_pair_resume_receipt( candidate.active_outing );
    if( effect.progress ) {
        candidate.active_outing.last_progress_minutes = current_minutes;
    }
    site = std::move( candidate );
    effect.valid = true;
    effect.changed = true;
    return effect;
}

sortie_observation_effect record_active_sortie_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<sortie_observation> &observations,
        const int current_minutes )
{
    return record_active_sortie_observations_impl( site, expected_cursor, observations,
            current_minutes, false );
}

sortie_observation_effect record_active_typed_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor, const character_id observer_id,
        const int expected_target_revision,
        const std::vector<sortie_observation> &observations,
        const int current_minutes )
{
    sortie_observation_effect effect;
    const active_outing_state &outing = site.active_outing;
    const member_record *observer = site.find_member( observer_id );
    if( expected_target_revision <= 0 ||
        outing.target_lead_revision != expected_target_revision || observer == nullptr ||
        ( observer->state != member_state::outbound &&
          observer->state != member_state::local_contact ) ||
        std::find( outing.member_ids.begin(), outing.member_ids.end(), observer_id ) ==
        outing.member_ids.end() || outing.member_is_resolved( observer_id ) ||
        std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), observer_id ) !=
        outing.casualty_ids.end() ||
        std::any_of( observations.begin(), observations.end(),
    [observer_id, expected_target_revision]( const sortie_observation & observation ) {
        return observation.observer_id != observer_id ||
               observation.target_revision != expected_target_revision ||
               observation.share_state == sortie_observation_share_state::reported ||
               !typed_sortie_observation_is_valid( observation );
    } ) ) {
        return effect;
    }
    return record_active_sortie_observations_impl( site, expected_cursor, observations,
            current_minutes, true );
}

bool scout_assessment_readiness_after_certainty(
    const scout_assessment_threshold_class threshold_class,
    const bool readiness_latched, const int certainty )
{
    switch( threshold_class ) {
        case scout_assessment_threshold_class::normal:
            return certainty >= ( readiness_latched ? 60 : 70 );
        case scout_assessment_threshold_class::burned:
            return certainty >= ( readiness_latched ? 50 : 60 );
        case scout_assessment_threshold_class::none:
            return false;
    }
    return false;
}

scout_report_effective_state evaluate_scout_report_at(
    const scout_report_record &report, const int current_minutes )
{
    scout_report_effective_state result;
    if( !report.is_present() || report.delivered_minutes < 0 ||
        current_minutes < report.delivered_minutes ) {
        return result;
    }

    result.valid = true;
    result.age_minutes = current_minutes - report.delivered_minutes;
    const int certainty_penalty = result.age_minutes >= 24 * 60 ? 20 :
                                  result.age_minutes >= 12 * 60 ? 10 : 0;
    result.certainty = std::max( 0, report.assessment.certainty - certainty_penalty );
    result.attack_authorization_usable = result.age_minutes < 48 * 60;
    result.assessment_ready = result.attack_authorization_usable &&
                              report.assessment.readiness_latched &&
                              scout_assessment_readiness_after_certainty(
                                  report.assessment.threshold_class, true,
                                  result.certainty );

    result.target_alert = report.assessment.target_alert;
    if( result.target_alert <= 0 ) {
        return result;
    }
    result.latest_contact_minutes = report.assessment.burned_minutes >= 0 &&
                                    report.assessment.burned_minutes <= report.delivered_minutes ?
                                    report.assessment.burned_minutes : -1;
    for( const sortie_observation &observation : report.observations ) {
        if( observation.record_schema_version == 1 && observation.observed_minutes >= 0 &&
            observation.observed_minutes <= report.delivered_minutes &&
            ( observation.kind == sortie_observation_kind::burn ||
              observation.kind == sortie_observation_kind::alert ) ) {
            result.latest_contact_minutes = std::max(
                                                result.latest_contact_minutes,
                                                observation.observed_minutes );
        }
    }
    if( result.latest_contact_minutes < 0 ) {
        // Legacy assessment packets persisted alert strength before an explicit contact fact.
        // Delivery is the latest conservative anchor they can prove without inventing history.
        result.latest_contact_minutes = report.delivered_minutes;
    }
    result.contact_age_minutes = current_minutes - result.latest_contact_minutes;
    const int decay_steps = result.contact_age_minutes / ( 12 * 60 );
    result.target_alert = std::max( 0, result.target_alert - decay_steps * 10 );
    return result;
}

namespace
{

bool scout_assessment_states_equal( const scout_assessment_state &lhs,
                                    const scout_assessment_state &rhs )
{
    return lhs.schema_version == rhs.schema_version &&
           lhs.observation_started_minutes == rhs.observation_started_minutes &&
           lhs.last_progress_minutes == rhs.last_progress_minutes &&
           lhs.burned_minutes == rhs.burned_minutes &&
           lhs.burn_origin_omt == rhs.burn_origin_omt &&
           lhs.certainty == rhs.certainty &&
           lhs.readiness_latched == rhs.readiness_latched &&
           lhs.threshold_class == rhs.threshold_class &&
           lhs.strong_visual_windows == rhs.strong_visual_windows &&
           lhs.defenders_low == rhs.defenders_low &&
           lhs.defenders_high == rhs.defenders_high &&
           lhs.danger_low == rhs.danger_low &&
           lhs.danger_high == rhs.danger_high &&
           lhs.target_alert == rhs.target_alert &&
           lhs.pinned_target_revision == rhs.pinned_target_revision &&
           lhs.next_eligible_minutes == rhs.next_eligible_minutes &&
           lhs.exit_reason == rhs.exit_reason;
}

scout_assessment_state summarize_normal_scout_assessment(
    const active_outing_state &outing )
{
    scout_assessment_state summary = outing.assessment;
    summary.schema_version = 1;
    summary.pinned_target_revision = outing.target_lead_revision;
    summary.certainty = 0;
    summary.strong_visual_windows = 0;
    summary.defenders_low = 0;
    summary.defenders_high = 0;
    summary.danger_low = 0;
    summary.danger_high = 0;

    int visual_certainty = 0;
    int signal_certainty = 0;
    int contradiction_penalty = 0;
    bool confirmed_presence = false;
    bool equipment_detail = false;
    bool defender_bounds_started = false;
    bool danger_bounds_started = false;
    std::set<int> strong_visual_buckets;
    std::set<sortie_observation_sense> signal_senses;
    int latest_progress = std::max( summary.observation_started_minutes,
                                    summary.last_progress_minutes );
    for( const sortie_observation &observation : outing.observations ) {
        if( observation.record_schema_version != 1 ||
            observation.target_revision != outing.target_lead_revision ||
            observation.observed_minutes < summary.observation_started_minutes ||
            observation.share_state == sortie_observation_share_state::observer_private ) {
            continue;
        }
        const bool target_evidence =
            std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                       observation.source_omt ) != outing.target_footprint.end();
        if( !target_evidence && observation.kind != sortie_observation_kind::burn ) {
            continue;
        }
        latest_progress = std::max( latest_progress, observation.observed_minutes );
        if( observation.kind == sortie_observation_kind::contradiction ) {
            contradiction_penalty = std::min( 30, contradiction_penalty + 15 );
            continue;
        }
        if( observation.sense != sortie_observation_sense::visual ) {
            signal_senses.insert( observation.sense );
            continue;
        }
        confirmed_presence = confirmed_presence ||
                             observation.kind == sortie_observation_kind::burn;
        const int visual_value = observation.visual_quality == 3 ? 20 :
                                 observation.visual_quality == 2 ? 15 :
                                 observation.visual_quality == 1 ? 5 : 0;
        visual_certainty = std::min( 60, visual_certainty + visual_value );
        if( observation.visual_quality >= 2 ) {
            strong_visual_buckets.insert( observation.bucket_start_minutes );
        }
        if( !observation.defender_ids.empty() ) {
            confirmed_presence = true;
            const int defenders = static_cast<int>( observation.defender_ids.size() );
            if( !defender_bounds_started ) {
                summary.defenders_low = defenders;
                summary.defenders_high = defenders;
                defender_bounds_started = true;
            } else {
                summary.defenders_low = std::min( summary.defenders_low, defenders );
                summary.defenders_high = std::max( summary.defenders_high, defenders );
            }
        }
        if( observation.observed_power_high > 0 ) {
            if( !danger_bounds_started ) {
                summary.danger_low = observation.observed_power_low;
                summary.danger_high = observation.observed_power_high;
                danger_bounds_started = true;
            } else {
                summary.danger_low = std::min( summary.danger_low,
                                              observation.observed_power_low );
                summary.danger_high = std::max( summary.danger_high,
                                               observation.observed_power_high );
            }
        }
        equipment_detail = equipment_detail || observation.equipment_detail > 0;
    }
    signal_certainty = std::min( 10, 5 * static_cast<int>( signal_senses.size() ) );
    summary.certainty = std::clamp( visual_certainty + signal_certainty +
                                   ( confirmed_presence ? 10 : 0 ) +
                                   ( equipment_detail ? 10 : 0 ) - contradiction_penalty,
                                   0, 95 );
    summary.strong_visual_windows = std::min( 3,
                                    static_cast<int>( strong_visual_buckets.size() ) );
    summary.last_progress_minutes = latest_progress;
    if( summary.threshold_class == scout_assessment_threshold_class::normal &&
        !scout_assessment_readiness_after_certainty(
            scout_assessment_threshold_class::normal,
            summary.readiness_latched, summary.certainty ) ) {
        summary.readiness_latched = false;
        summary.threshold_class = scout_assessment_threshold_class::none;
    }
    return summary;
}

} // namespace

scout_assessment_result advance_structural_scout_assessment(
    site_record &site, const std::string &expected_activity_id,
    const int expected_generation, const int expected_target_revision,
    const int current_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version != 10 ||
        outing.phase != scout_phase::observing ||
        outing.activity_id != expected_activity_id ||
        outing.generation != expected_generation ||
        outing.target_lead_revision != expected_target_revision ||
        expected_target_revision <= 0 || current_minutes < 0 ||
        outing.last_advanced_minutes > current_minutes ||
        outing.selected_watch_kind == structural_watch_kind::none ||
        outing.waypoint_index != structural_outing_destination_waypoint( outing ) ||
        !simulation_owner_state_is_consistent( outing ) ) {
        return scout_assessment_result::rejected;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    const bool clock_advanced = next.last_advanced_minutes < current_minutes;
    const bool assessment_initialized =
        next.assessment.observation_started_minutes < 0;
    next.last_advanced_minutes = current_minutes;
    if( assessment_initialized ) {
        next.assessment.observation_started_minutes = current_minutes;
        next.assessment.last_progress_minutes = current_minutes;
        next.assessment.pinned_target_revision = expected_target_revision;
    }
    if( next.assessment.pinned_target_revision != expected_target_revision ||
        next.assessment.observation_started_minutes > current_minutes ) {
        return scout_assessment_result::rejected;
    }

    const scout_assessment_state before = next.assessment;
    next.assessment = summarize_normal_scout_assessment( next );
    const bool normal_readiness = scout_assessment_readiness_after_certainty(
                                      scout_assessment_threshold_class::normal,
                                      next.assessment.readiness_latched,
                                      next.assessment.certainty );
    const bool normal_success =
        !next.alternate_watch_reposition_pending &&
        current_minutes - next.assessment.observation_started_minutes >= 120 &&
        next.assessment.strong_visual_windows >= 3 &&
        normal_readiness &&
        next.assessment.defenders_high - next.assessment.defenders_low <= 2;
    if( normal_success ) {
        next.assessment.readiness_latched = true;
        next.assessment.threshold_class = scout_assessment_threshold_class::normal;
        next.assessment.next_eligible_minutes = minutes_after_saturated(
                current_minutes, 48 * 60 );
        next.assessment.exit_reason = "normal watch assessment complete";
        next.phase = scout_phase::returning_report;
        next.last_progress_minutes = current_minutes;
        if( next.local_handoff.is_active() ) {
            next.local_handoff.phase = next.phase;
        }
        if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
            return scout_assessment_result::rejected;
        }
        site = std::move( candidate );
        record_scout_phase_transition_event( site.active_outing, scout_phase::observing,
                                             scout_phase::returning_report,
                                             "normal watch assessment complete",
                                             current_minutes );
        return scout_assessment_result::normal_success;
    }
    const int no_progress_deadline = minutes_after_saturated(
                                         next.assessment.last_progress_minutes,
                                         2 * 60 );
    const bool no_progress_window_elapsed =
        current_minutes >= no_progress_deadline;
    const bool local_alternate_reposition_required = no_progress_window_elapsed &&
            !next.alternate_watch_reposition_pending &&
            !next.alternate_watch_attempted &&
            next.alternate_watch_kind != structural_watch_kind::none &&
            !next.alternate_watch_shared_route.empty() &&
            next.owner == simulation_owner::local &&
            next.local_handoff.is_active();
    if( local_alternate_reposition_required ) {
        if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
            return scout_assessment_result::rejected;
        }
        site = std::move( candidate );
        return scout_assessment_result::alternate_watch_reposition_required;
    }
    const bool can_start_abstract_alternate = no_progress_window_elapsed &&
            !next.alternate_watch_attempted &&
            next.alternate_watch_kind != structural_watch_kind::none &&
            !next.alternate_watch_shared_route.empty() &&
            next.owner == simulation_owner::abstract &&
            !next.local_handoff.is_active() &&
            !next.local_handoff.is_abstract_resume();
    if( can_start_abstract_alternate ) {
        std::swap( next.shared_route, next.alternate_watch_shared_route );
        std::swap( next.selected_watch_kind, next.alternate_watch_kind );
        std::swap( next.selected_watch_omt, next.alternate_watch_omt );
        std::swap( next.selected_watch_route_cost,
                   next.alternate_watch_route_cost );
        next.alternate_watch_attempted = true;
        next.waypoint_index = structural_outing_destination_waypoint( next );
        next.last_progress_minutes = no_progress_deadline;
        next.assessment.last_progress_minutes = no_progress_deadline;
        next.expected_return_minutes = structural_expected_return_minutes(
                                           next.started_minutes, candidate.anchor,
                                           next.selected_watch_omt );
        next.missing_deadline_minutes = minutes_after_saturated(
                                            next.expected_return_minutes,
                                            scout_missing_grace_minutes );
        const int second_watch_deadline = minutes_after_saturated(
                                              no_progress_deadline, 2 * 60 );
        const bool second_watch_elapsed = current_minutes >= second_watch_deadline;
        if( second_watch_elapsed ) {
            next.assessment.readiness_latched = false;
            next.assessment.threshold_class = scout_assessment_threshold_class::none;
            next.assessment.last_progress_minutes = second_watch_deadline;
            next.assessment.next_eligible_minutes = minutes_after_saturated(
                    second_watch_deadline, 12 * 60 );
            next.assessment.exit_reason =
                "second watch made no assessment progress";
            next.phase = scout_phase::returning_report;
            next.last_progress_minutes = second_watch_deadline;
        }
        if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
            return scout_assessment_result::rejected;
        }
        site = std::move( candidate );
        record_scout_phase_transition_event(
            site.active_outing, scout_phase::observing, scout_phase::observing,
            "no-progress window moved to persisted alternate watch",
            no_progress_deadline );
        if( second_watch_elapsed ) {
            record_scout_phase_transition_event(
                site.active_outing, scout_phase::observing,
                scout_phase::returning_report,
                "second watch made no assessment progress",
                second_watch_deadline );
            return scout_assessment_result::inconclusive;
        }
        return scout_assessment_result::alternate_watch_started;
    }
    if( no_progress_window_elapsed && next.alternate_watch_attempted ) {
        next.assessment.readiness_latched = false;
        next.assessment.threshold_class = scout_assessment_threshold_class::none;
        next.assessment.next_eligible_minutes = minutes_after_saturated(
                no_progress_deadline, 12 * 60 );
        next.assessment.exit_reason =
            "second watch made no assessment progress";
        next.phase = scout_phase::returning_report;
        next.last_progress_minutes = no_progress_deadline;
        next.assessment.last_progress_minutes = no_progress_deadline;
        if( next.local_handoff.is_active() ) {
            next.local_handoff.phase = next.phase;
        }
        if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
            return scout_assessment_result::rejected;
        }
        site = std::move( candidate );
        record_scout_phase_transition_event(
            site.active_outing, scout_phase::observing,
            scout_phase::returning_report,
            "second watch made no assessment progress", no_progress_deadline );
        return scout_assessment_result::inconclusive;
    }
    const bool watch_expired =
        !next.alternate_watch_reposition_pending &&
        current_minutes - next.assessment.observation_started_minutes >= 8 * 60;
    if( watch_expired ) {
        next.assessment.readiness_latched = false;
        next.assessment.threshold_class = scout_assessment_threshold_class::none;
        next.assessment.next_eligible_minutes = minutes_after_saturated(
                current_minutes, 12 * 60 );
        next.assessment.exit_reason =
            "maximum watch duration reached without a complete assessment";
        next.phase = scout_phase::returning_report;
        next.last_progress_minutes = current_minutes;
        if( next.local_handoff.is_active() ) {
            next.local_handoff.phase = next.phase;
        }
        if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
            return scout_assessment_result::rejected;
        }
        site = std::move( candidate );
        record_scout_phase_transition_event( site.active_outing, scout_phase::observing,
                                             scout_phase::returning_report,
                                             "watch assessment inconclusive",
                                             current_minutes );
        return scout_assessment_result::inconclusive;
    }
    if( scout_assessment_states_equal( before, next.assessment ) && !clock_advanced &&
        !assessment_initialized ) {
        return scout_assessment_result::unchanged;
    }
    site = std::move( candidate );
    return scout_assessment_result::updated;
}

int ordinary_scout_sortie_limit_minutes()
{
    return 720;
}

bool scout_sortie_should_return_home( const site_record &site, const int current_minutes,
                                      const int sortie_limit_minutes )
{
    if( !site.active_outing.is_active() || site.active_outing.member_ids.empty() ||
        sortie_limit_minutes <= 0 || current_minutes < 0 ||
        site.last_shakedown_outcome == "fight_unresolved" ) {
        return false;
    }

    const std::optional<bandit_dry_run::job_template> active_job =
        job_template_from_string( site.active_outing.job_type );
    if( active_job.has_value() && *active_job != bandit_dry_run::job_template::scout &&
        *active_job != bandit_dry_run::job_template::hold_chill ) {
        return false;
    }

    const int anchor_minutes = site.active_outing.local_contact_minutes >= 0 ?
                               site.active_outing.local_contact_minutes : site.active_outing.started_minutes;
    return anchor_minutes >= 0 && current_minutes - anchor_minutes >= sortie_limit_minutes;
}

shakedown_surface build_shakedown_surface( const site_record &site, const local_gate_input &input,
        const local_gate_decision &decision, const shakedown_goods_pool &goods_pool )
{
    shakedown_surface surface;

    if( !decision.valid || !decision.opens_shakedown_surface ||
        decision.posture != local_gate_posture::open_shakedown ||
        !site.active_outing.is_active() ) {
        surface.notes.push_back( "shakedown blocked: local gate did not open the robbery surface" );
        return surface;
    }

    if( effective_profile( site ) == hostile_site_profile::cannibal_camp ) {
        surface.notes.push_back( "shakedown blocked: cannibal camp profile attacks to kill instead of extorting" );
        return surface;
    }

    if( input.rolling_travel_scene ) {
        surface.notes.push_back( "shakedown blocked: rolling travel scene remains a direct-ambush context" );
        return surface;
    }

    surface.includes_basecamp_inventory = input.basecamp_or_camp_scene ||
                                          goods_pool.basecamp_or_camp_scene;
    surface.includes_vehicle_inventory = !surface.includes_basecamp_inventory;
    surface.reachable_goods_value = goods_pool.player_carried_value +
                                    goods_pool.companion_carried_value;
    if( surface.includes_basecamp_inventory ) {
        surface.reachable_goods_value += goods_pool.reachable_basecamp_value;
        surface.notes.push_back( "pool includes player, nearby companion, and reachable Basecamp goods" );
    } else {
        surface.reachable_goods_value += goods_pool.vehicle_carried_value;
        surface.notes.push_back( "pool includes player, companion, and current vehicle goods only" );
    }

    if( surface.reachable_goods_value <= 0 ) {
        surface.notes.push_back( "shakedown blocked: no honest reachable goods are present" );
        return surface;
    }

    const shakedown_opening_beat opening = choose_shakedown_opening_beat( site, input, decision );
    surface.opening_id = opening.id;
    surface.opening_summary = opening.summary;
    surface.bark = opening.bark;

    surface.valid = true;
    surface.pay_available = true;
    surface.fight_available = true;
    const int base_demanded_value = std::max( 1, ( surface.reachable_goods_value * 35 + 99 ) / 100 );
    const int demand_modifier_percent = shakedown_demand_modifier_percent( site );
    surface.demanded_value = ( base_demanded_value * demand_modifier_percent + 99 ) / 100;
    surface.demanded_value = std::clamp( surface.demanded_value, 1, surface.reachable_goods_value );
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        surface.notes.push_back( "renegotiation reopen: previous defender loss raises this one bounded demand" );
    } else if( demand_modifier_percent < 100 ) {
        surface.notes.push_back( "aftermath caution: previous bandit loss cools or shrinks this demand" );
    }
    surface.notes.push_back( "scenic opening beat: " + surface.opening_summary );
    surface.notes.push_back( "visible responses are Pay/Fight only; backout enters the fight/refusal branch" );
    surface.notes.push_back( "pay branch opens the NPC trade UI with the demanded toll as debt before any goods are surrendered" );
    surface.notes.push_back( "fight branch stays explicit whenever this surface is invoked" );
    surface.notes.push_back( "source site " + site.site_id + " opened the surface from " +
                             site.active_outing.activity_id );
    return surface;
}

std::string render_shakedown_surface_report( const site_record &site,
        const shakedown_surface &surface )
{
    std::ostringstream out;
    out << "shakedown_surface site=" << site.site_id
        << " active_group=" << site.active_outing.activity_id
        << " profile=" << to_string( effective_profile( site ) )
        << " posture=open_shakedown"
        << " valid=" << ( surface.valid ? "yes" : "no" )
        << " pay_option=" << ( surface.pay_available ? "yes" : "no" )
        << " fight_option=" << ( surface.fight_available ? "yes" : "no" )
        << " visible_responses=pay/fight"
        << " payment_surface=npc_trade_ui"
        << " reachable_goods=" << surface.reachable_goods_value
        << " demanded_toll=" << surface.demanded_value
        << " basecamp_inventory=" << ( surface.includes_basecamp_inventory ? "yes" : "no" )
        << " vehicle_inventory=" << ( surface.includes_vehicle_inventory ? "yes" : "no" )
        << " opening=" << ( surface.opening_id.empty() ? "none" : surface.opening_id )
        << " bark=\"" << surface.bark << "\"\n";
    for( const std::string &note : surface.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

bool is_active_shakedown_parley_member( const world_state &state, const character_id npc_id )
{
    for( const site_record &site : state.sites ) {
        if( site.retired_empty_site || !site.active_outing.is_active() ||
            site.active_outing.owner != simulation_owner::local ||
            site.active_outing.member_ids.empty() || site.active_outing.job_type != "toll" ) {
            continue;
        }
        if( site.last_shakedown_outcome.rfind( "fight", 0 ) == 0 ) {
            continue;
        }
        if( std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                      npc_id ) == site.active_outing.member_ids.end() ) {
            continue;
        }
        const member_record *member = site.find_member( npc_id );
        if( member == nullptr || member->state == member_state::dead ||
            member->state == member_state::missing ) {
            continue;
        }
        return true;
    }
    return false;
}

static std::optional<covert_scout_relationship_read> read_active_covert_scout_member_impl(
    const world_state &state, const character_id npc_id,
    const bool include_ordinary_returning_home )
{
    const auto accepted_phase = [include_ordinary_returning_home](
    const active_outing_state &outing ) {
        return outing.phase == scout_phase::searching ||
               outing.phase == scout_phase::observing ||
               outing.phase == scout_phase::burned_withdrawal ||
               outing.phase == scout_phase::returning_exposed ||
               outing.phase == scout_phase::returning_report ||
               ( outing.phase == scout_phase::returning_home &&
                 ( include_ordinary_returning_home ||
                   outing.local_handoff.cohesion_abort_return ) );
    };

    std::optional<covert_scout_relationship_read> result;
    for( const site_record &site : state.sites ) {
        const active_outing_state &outing = site.active_outing;
        if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
            outing.owner != simulation_owner::local ||
            std::find( outing.member_ids.begin(), outing.member_ids.end(), npc_id ) ==
            outing.member_ids.end() ) {
            continue;
        }
        // Stable member IDs are unique across sites at world load/claim time.  Still reject a
        // malformed in-memory duplicate of this actor without preflighting every unrelated owner.
        if( result ) {
            return std::nullopt;
        }
        if( site.retired_empty_site || outing.schema_version != 10 ||
            outing.member_ids.size() != 2 ||
            !simulation_owner_state_is_consistent( outing ) ||
            !outing.local_handoff.is_active() || outing.local_handoff.members.size() != 2 ||
            outing.selected_watch_kind == structural_watch_kind::none ||
            !accepted_phase( outing ) || outing.local_handoff.phase != outing.phase ||
            outing.member_is_resolved( npc_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), npc_id ) !=
            outing.casualty_ids.end() ) {
            return std::nullopt;
        }
        const member_record *member = site.find_member( npc_id );
        if( member == nullptr ||
            ( member->state != member_state::outbound &&
              member->state != member_state::local_contact ) ) {
            return std::nullopt;
        }
        const auto local_member = std::find_if(
                                      outing.local_handoff.members.begin(),
                                      outing.local_handoff.members.end(),
        [npc_id]( const local_handoff_member_snapshot & candidate ) {
            return candidate.npc_id == npc_id;
        } );
        if( local_member == outing.local_handoff.members.end() || local_member->dead ) {
            return std::nullopt;
        }
        const std::optional<int> minimum_target_distance =
            target_footprint_watch_distance(
                outing.selected_watch_omt, outing.target_footprint );
        if( !minimum_target_distance ) {
            return std::nullopt;
        }
        const bool terminal_homeward = outing.phase == scout_phase::returning_exposed ||
                                       outing.phase == scout_phase::returning_report ||
                                       outing.phase == scout_phase::returning_home;
        std::vector<tripoint_abs_omt> forbidden_route_omts;
        if( terminal_homeward ) {
            forbidden_route_omts.push_back( outing.selected_watch_omt );
            forbidden_route_omts.insert( forbidden_route_omts.end(),
                                         outing.failed_covert_egress_omts.begin(),
                                         outing.failed_covert_egress_omts.end() );
            forbidden_route_omts.insert( forbidden_route_omts.end(),
                                         outing.failed_covert_egress_route_omts.begin(),
                                         outing.failed_covert_egress_route_omts.end() );
            std::sort( forbidden_route_omts.begin(), forbidden_route_omts.end(),
            []( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) {
                return std::make_tuple( lhs.z(), lhs.y(), lhs.x() ) <
                       std::make_tuple( rhs.z(), rhs.y(), rhs.x() );
            } );
            forbidden_route_omts.erase(
                std::unique( forbidden_route_omts.begin(), forbidden_route_omts.end() ),
                forbidden_route_omts.end() );
        }
        result = covert_scout_relationship_read{
            outing.phase, outing.target_footprint,
            terminal_homeward ? site.anchor : outing.local_handoff.egress_omt,
            *minimum_target_distance, std::move( forbidden_route_omts )
        };
    }
    return result;
}

std::optional<covert_scout_relationship_read> read_active_covert_scout_member(
    const world_state &state, const character_id npc_id )
{
    return read_active_covert_scout_member_impl( state, npc_id, false );
}

std::optional<covert_scout_relationship_read> read_active_covert_scout_homeward_member(
    const world_state &state, const character_id npc_id )
{
    return read_active_covert_scout_member_impl( state, npc_id, true );
}

bool is_active_covert_scout_member( const world_state &state, const character_id npc_id )
{
    return read_active_covert_scout_member( state, npc_id ).has_value();
}

int covert_scout_burn_observer_cap()
{
    return 16;
}

int covert_scout_egress_route_omt_cap()
{
    return static_cast<int>( max_covert_egress_route_omts );
}

static bool covert_scout_egress_candidates_are_valid(
    const tripoint_abs_omt &burn_origin,
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<covert_scout_egress_candidate> &candidates,
    const std::optional<tripoint_abs_omt> &route_floor_origin = std::nullopt )
{
    const tripoint_abs_omt route_floor = route_floor_origin.value_or( burn_origin );
    if( burn_origin.is_invalid() || route_floor.is_invalid() || target_footprint.empty() ||
        candidates.size() > max_covert_burn_egress_candidates ||
        !target_footprint_watch_distance( burn_origin, target_footprint ) ||
        !target_footprint_watch_distance( route_floor, target_footprint ) ) {
        return false;
    }
    const int origin_distance = *target_footprint_watch_distance(
                                    burn_origin, target_footprint );
    const int route_floor_distance = *target_footprint_watch_distance(
                                         route_floor, target_footprint );
    std::vector<tripoint_abs_omt> candidate_omts;
    candidate_omts.reserve( candidates.size() );
    for( const covert_scout_egress_candidate &candidate : candidates ) {
        const std::optional<int> candidate_distance = target_footprint_watch_distance(
                    candidate.omt, target_footprint );
        if( candidate.omt.is_invalid() || candidate.omt.z() != burn_origin.z() ||
            omt_chebyshev_distance( candidate.omt, burn_origin ) != 1 ||
            !candidate_distance ||
            *candidate_distance < origin_distance ||
            candidate.soft_danger < 0 || candidate.soft_danger > 200 ||
            ( candidate.reachable && candidate.route_cost < 0 ) ||
            candidate.route_omts.size() > max_covert_egress_route_omts ||
            ( candidate.reachable &&
              std::find( candidate.route_omts.begin(), candidate.route_omts.end(),
                         candidate.omt ) == candidate.route_omts.end() ) ||
            std::find( candidate_omts.begin(), candidate_omts.end(), candidate.omt ) !=
            candidate_omts.end() ) {
            return false;
        }
        std::vector<tripoint_abs_omt> route_omts;
        for( const tripoint_abs_omt &route_omt : candidate.route_omts ) {
            const std::optional<int> route_distance = target_footprint_watch_distance(
                        route_omt, target_footprint );
            if( route_omt.is_invalid() || route_omt.z() != burn_origin.z() ||
                !route_distance || *route_distance < route_floor_distance ||
                std::find( route_omts.begin(), route_omts.end(), route_omt ) !=
                route_omts.end() ) {
                return false;
            }
            route_omts.push_back( route_omt );
        }
        candidate_omts.push_back( candidate.omt );
    }
    return true;
}

std::optional<covert_scout_egress_candidate> select_covert_scout_egress(
    const tripoint_abs_omt &burn_origin,
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<covert_scout_egress_candidate> &candidates,
    const std::optional<tripoint_abs_omt> &route_floor_origin )
{
    if( !covert_scout_egress_candidates_are_valid(
            burn_origin, target_footprint, candidates, route_floor_origin ) ) {
        return std::nullopt;
    }
    const std::optional<int> origin_distance = target_footprint_watch_distance(
                burn_origin, target_footprint );
    if( !origin_distance ) {
        return std::nullopt;
    }

    std::optional<covert_scout_egress_candidate> selected_safe;
    std::optional<covert_scout_egress_candidate> selected_hard;
    for( const covert_scout_egress_candidate &candidate : candidates ) {
        const std::optional<int> candidate_distance = target_footprint_watch_distance(
                    candidate.omt, target_footprint );
        if( !candidate.reachable || candidate.hard_danger ) {
            if( !candidate.reachable ) {
                continue;
            }
        }
        std::optional<covert_scout_egress_candidate> &selected = candidate.hard_danger ?
                selected_hard : selected_safe;
        const int candidate_outward_rank = *candidate_distance > *origin_distance ? 0 : 1;
        const std::optional<int> selected_distance = selected ?
                target_footprint_watch_distance( selected->omt, target_footprint ) : std::nullopt;
        const int selected_outward_rank = selected_distance &&
                                          *selected_distance > *origin_distance ? 0 : 1;
        if( !selected ||
            std::make_tuple( candidate.soft_danger, candidate.concealed ? 0 : 1,
                             candidate_outward_rank, candidate.route_cost, candidate.omt.z(),
                             candidate.omt.y(), candidate.omt.x() ) <
            std::make_tuple( selected->soft_danger, selected->concealed ? 0 : 1,
                             selected_outward_rank, selected->route_cost, selected->omt.z(),
                             selected->omt.y(), selected->omt.x() ) ) {
            selected = candidate;
        }
    }
    // Hard danger is a survival override only after every reachable safe exit failed.  A trapped
    // pair must take the least-dangerous real route instead of remaining in observation forever.
    return selected_safe ? selected_safe : selected_hard;
}

bool covert_scout_egress_route_respects_retry_memory(
    const active_outing_state &outing, const tripoint_abs_omt &member_start,
    const std::vector<tripoint_abs_omt> &route, const bool current_route_failed )
{
    const tripoint_abs_omt route_origin = outing.covert_egress_attempts > 0 &&
                                          current_route_failed ?
                                          outing.local_handoff.egress_omt :
                                          outing.selected_watch_omt;
    const std::optional<int> origin_distance = target_footprint_watch_distance(
                route_origin, outing.target_footprint );
    const std::optional<int> destination_distance = route.empty() ? std::nullopt :
            target_footprint_watch_distance( route.front(), outing.target_footprint );
    const std::optional<int> start_distance = target_footprint_watch_distance(
                member_start, outing.target_footprint );
    if( !origin_distance || !destination_distance || !start_distance ||
        route.back() != member_start ||
        *destination_distance < *origin_distance ||
        ( outing.covert_egress_attempts > 0 && current_route_failed &&
          *destination_distance <= *origin_distance ) ) {
        return false;
    }
    int previous_distance = *start_distance;
    for( std::size_t reverse_index = route.size(); reverse_index > 0; --reverse_index ) {
        const std::size_t index = reverse_index - 1;
        const tripoint_abs_omt &route_omt = route[index];
        const std::optional<int> route_distance = target_footprint_watch_distance(
                    route_omt, outing.target_footprint );
        if( !route_distance || *route_distance < previous_distance ) {
            return false;
        }
        previous_distance = *route_distance;
        const bool remembered_footing = route_omt == outing.selected_watch_omt ||
                std::find( outing.failed_covert_egress_omts.begin(),
                           outing.failed_covert_egress_omts.end(), route_omt ) !=
                outing.failed_covert_egress_omts.end() ||
                ( current_route_failed &&
                  std::find( outing.current_covert_egress_route_omts.begin(),
                             outing.current_covert_egress_route_omts.end(), route_omt ) !=
                  outing.current_covert_egress_route_omts.end() ) ||
                std::find( outing.failed_covert_egress_route_omts.begin(),
                           outing.failed_covert_egress_route_omts.end(), route_omt ) !=
                outing.failed_covert_egress_route_omts.end();
        const bool current_start = index + 1 == route.size() && route_omt == member_start;
        if( outing.covert_egress_attempts > 0 && remembered_footing && !current_start ) {
            return false;
        }
        if( outing.covert_egress_attempts > 0 && !current_route_failed && !current_start &&
            std::find( outing.current_covert_egress_route_omts.begin(),
                       outing.current_covert_egress_route_omts.end(), route_omt ) ==
            outing.current_covert_egress_route_omts.end() ) {
            return false;
        }
    }
    return true;
}

static std::optional<covert_scout_egress_candidate> select_covert_scout_survival_direction(
    const tripoint_abs_omt &burn_origin,
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<covert_scout_egress_candidate> &candidates )
{
    if( !covert_scout_egress_candidates_are_valid(
            burn_origin, target_footprint, candidates ) ) {
        return std::nullopt;
    }
    const std::optional<int> origin_distance = target_footprint_watch_distance(
                burn_origin, target_footprint );
    if( !origin_distance ) {
        return std::nullopt;
    }

    std::optional<covert_scout_egress_candidate> selected_safe;
    std::optional<covert_scout_egress_candidate> selected_hard;
    for( const covert_scout_egress_candidate &candidate : candidates ) {
        std::optional<covert_scout_egress_candidate> &selected = candidate.hard_danger ?
                selected_hard : selected_safe;
        const std::optional<int> candidate_distance = target_footprint_watch_distance(
                    candidate.omt, target_footprint );
        const std::optional<int> selected_distance = selected ?
                target_footprint_watch_distance( selected->omt, target_footprint ) : std::nullopt;
        const int candidate_outward_rank = candidate_distance &&
                                           *candidate_distance > *origin_distance ? 0 : 1;
        const int selected_outward_rank = selected_distance &&
                                          *selected_distance > *origin_distance ? 0 : 1;
        if( !selected ||
            std::make_tuple( candidate.soft_danger, candidate.concealed ? 0 : 1,
                             candidate_outward_rank, candidate.omt.z(), candidate.omt.y(),
                             candidate.omt.x() ) <
            std::make_tuple( selected->soft_danger, selected->concealed ? 0 : 1,
                             selected_outward_rank, selected->omt.z(), selected->omt.y(),
                             selected->omt.x() ) ) {
            selected = candidate;
        }
    }
    return selected_safe ? selected_safe : selected_hard;
}

static std::optional<std::vector<sortie_observation>> make_atomic_covert_burn_observations(
    const site_record &site, const simulation_advance_cursor &expected_cursor,
    const sortie_observation &burn,
    const std::optional<structural_local_zombie_read> &danger_read,
    const int current_minutes )
{
    if( !typed_sortie_observation_is_valid( burn ) ) {
        return std::nullopt;
    }
    std::optional<sortie_observation> danger;
    if( danger_read ) {
        if( current_minutes <= expected_cursor.last_advanced_minutes ||
            !structural_local_zombie_read_is_valid(
                site, expected_cursor, *danger_read, current_minutes ) ) {
            return std::nullopt;
        }
        danger = make_structural_local_zombie_observation(
                     site, *danger_read, current_minutes );
        if( !typed_sortie_observation_is_valid( *danger ) ) {
            return std::nullopt;
        }
    }

    std::vector<sortie_observation> combined = site.active_outing.observations;
    if( danger ) {
        combined.push_back( *danger );
    }
    combined.push_back( burn );
    std::vector<sortie_observation> retained = make_bounded_sortie_observations( combined );
    const auto observation_was_retained = [&retained]( const sortie_observation &expected ) {
        return std::any_of( retained.begin(), retained.end(),
        [&expected]( const sortie_observation & observation ) {
            return sortie_observation_identity_matches( observation, expected ) &&
                   sortie_observations_equal( observation, expected );
        } );
    };
    if( !observation_was_retained( burn ) ||
        ( danger && !observation_was_retained( *danger ) ) ) {
        return std::nullopt;
    }
    return retained;
}

static covert_scout_burn_effect apply_covert_scout_burn_impl(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_burn_read> &member_reads,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    const int current_minutes,
    const std::optional<structural_local_zombie_read> &danger_read,
    const bool record_transition_event )
{
    covert_scout_burn_effect effect;
    const active_outing_state &outing = site.active_outing;
    const bool pending_alternate_reposition =
        outing.alternate_watch_reposition_pending;
    const std::optional<int> pending_reposition_ring_distance =
        pending_alternate_reposition ? target_footprint_watch_distance(
            outing.selected_watch_omt, outing.target_footprint ) : std::nullopt;
    if( site.retired_empty_site || !simulation_cursor_matches( outing, expected_cursor ) ||
        outing.schema_version != 10 || outing.kind != outing_kind::structural_sortie ||
        outing.owner != simulation_owner::local || outing.phase != scout_phase::observing ||
        outing.local_handoff.phase != outing.phase || !outing.local_handoff.is_active() ||
        !outing.local_handoff.cohesion_assembled ||
        outing.local_handoff.cohesion_abort_return || outing.member_ids.size() != 2 ||
        outing.local_handoff.members.size() != 2 || outing.target_footprint.empty() ||
        outing.selected_watch_kind == structural_watch_kind::none ||
        outing.waypoint_index != 2 || outing.local_handoff.waypoint_index != 2 ||
        !structural_watch_shared_route_is_canonical(
            outing.shared_route, site.anchor, outing.selected_watch_omt,
            outing.target_footprint ) ||
        outing.selected_watch_omt.is_invalid() || outing.local_handoff.egress_omt.is_invalid() ||
        current_minutes < 0 || current_minutes < outing.last_advanced_minutes ||
        outing.target_lead_revision <= 0 ||
        std::any_of( outing.observations.begin(), outing.observations.end(),
    []( const sortie_observation & observation ) {
        return observation.kind == sortie_observation_kind::burn;
    } ) ) {
        return effect;
    }

    const covert_scout_burn_read *exposure = nullptr;
    std::set<character_id> matched_reads;
    const auto pending_position_is_valid =
    [&outing, &pending_reposition_ring_distance]( const tripoint_abs_omt & position ) {
        const std::optional<int> distance = target_footprint_watch_distance(
                                                position, outing.target_footprint );
        return pending_reposition_ring_distance && distance &&
               *distance >= *pending_reposition_ring_distance;
    };
    for( const character_id member_id : outing.member_ids ) {
        const member_record *member = site.find_member( member_id );
        const auto handoff_member = std::find_if(
                                        outing.local_handoff.members.begin(),
                                        outing.local_handoff.members.end(),
        [member_id]( const local_handoff_member_snapshot & candidate ) {
            return candidate.npc_id == member_id;
        } );
        const auto read = std::find_if( member_reads.begin(), member_reads.end(),
        [member_id]( const covert_scout_burn_read & candidate ) {
            return candidate.npc_id == member_id;
        } );
        if( member == nullptr ||
            ( member->state != member_state::outbound &&
              member->state != member_state::local_contact ) ||
            outing.member_is_resolved( member_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
            outing.casualty_ids.end() || handoff_member == outing.local_handoff.members.end() ||
            handoff_member->dead || read == member_reads.end() ||
            ( !read->present && !pending_alternate_reposition ) ||
            ( !pending_alternate_reposition &&
              read->position != outing.selected_watch_omt ) ||
            ( pending_alternate_reposition &&
              !pending_position_is_valid( read->position ) ) ||
            !matched_reads.emplace( member_id ).second ||
            std::count_if( member_reads.begin(), member_reads.end(),
        [member_id]( const covert_scout_burn_read & candidate ) {
            return candidate.npc_id == member_id;
        } ) != 1 ) {
            return effect;
        }
        std::vector<tripoint_abs_omt> perceived_positions;
        for( const tripoint_abs_omt &position : read->perceived_target_observer_positions ) {
            if( position.is_invalid() ||
                std::find( perceived_positions.begin(), perceived_positions.end(), position ) !=
                perceived_positions.end() ) {
                return effect;
            }
            perceived_positions.push_back( position );
        }
        if( perceived_positions.size() >
            static_cast<std::size_t>( covert_scout_burn_observer_cap() ) ) {
            return effect;
        }
        const bool reciprocal = read->target_saw_scout && read->scout_saw_target;
        if( reciprocal ) {
            if( !read->present ) {
                return effect;
            }
            if( exposure == nullptr && !read->target_observer_id.empty() &&
                read->target_observer_id.size() <= max_sortie_source_id_length &&
                !read->target_observer_position.is_invalid() ) {
                exposure = &*read;
            } else if( read->target_observer_id.empty() ||
                       read->target_observer_id.size() > max_sortie_source_id_length ||
                       read->target_observer_position.is_invalid() ) {
                return effect;
            }
        }
    }
    if( member_reads.size() != matched_reads.size() ) {
        return effect;
    }
    if( exposure == nullptr ) {
        effect.result = covert_scout_burn_result::unchanged;
        return effect;
    }
    if( !pending_alternate_reposition &&
        !covert_scout_egress_candidates_are_valid(
            exposure->position, outing.target_footprint, egress_candidates ) ) {
        return effect;
    }
    const std::optional<covert_scout_egress_candidate> selected_egress =
        pending_alternate_reposition ? std::nullopt :
        select_covert_scout_egress( exposure->position, outing.target_footprint,
                                    egress_candidates );
    const std::optional<covert_scout_egress_candidate> survival_direction = selected_egress ?
            selected_egress : select_covert_scout_survival_direction(
                exposure->position, outing.target_footprint,
                pending_alternate_reposition ?
                std::vector<covert_scout_egress_candidate>() : egress_candidates );

    sortie_observation burn;
    burn.fact_key = "burn:" + std::to_string( exposure->npc_id.get_value() );
    burn.summary = "scout cover broken by reciprocal ordinary visual contact";
    burn.confidence = 100;
    burn.observed_minutes = current_minutes;
    burn.critical = true;
    burn.kind = sortie_observation_kind::burn;
    burn.state_key = "burned";
    burn.record_schema_version = 1;
    burn.source_id = exposure->target_observer_id;
    burn.sense = sortie_observation_sense::visual;
    burn.observer_id = exposure->npc_id;
    burn.source_omt = exposure->target_observer_position;
    burn.receiver_omt = exposure->position;
    burn.bucket_start_minutes = current_minutes - current_minutes % 30;
    burn.strength = 6;
    burn.visual_quality = 3;
    burn.simultaneity_start_minutes = current_minutes;
    burn.simultaneity_end_minutes = current_minutes;
    burn.observed_power_low = 1;
    burn.observed_power_high = 1;
    burn.target_revision = outing.target_lead_revision;
    burn.expiry_minutes = minutes_after_saturated( current_minutes, 6 * 60 );
    burn.share_state = sortie_observation_share_state::shared;

    site_record candidate = site;
    const std::optional<std::vector<sortie_observation>> observations =
        make_atomic_covert_burn_observations(
            site, expected_cursor, burn, danger_read, current_minutes );
    if( !observations ) {
        return effect;
    }
    candidate.active_outing.observations = *observations;
    const scout_phase burn_phase = survival_direction ? scout_phase::burned_withdrawal :
                                   scout_phase::returning_exposed;
    candidate.active_outing.alternate_watch_reposition_pending = false;
    candidate.active_outing.phase = burn_phase;
    candidate.active_outing.local_handoff.phase = burn_phase;
    candidate.active_outing.covert_egress_chain_version =
        current_covert_egress_chain_version;
    candidate.active_outing.covert_egress_attempts = survival_direction ? 1 : 0;
    candidate.active_outing.covert_egress_revision = 1;
    candidate.active_outing.failed_covert_egress_omts.clear();
    candidate.active_outing.current_covert_egress_route_omts = survival_direction ?
            survival_direction->route_omts : std::vector<tripoint_abs_omt>();
    candidate.active_outing.failed_covert_egress_route_omts.clear();
    if( candidate.active_outing.assessment.observation_started_minutes < 0 ) {
        candidate.active_outing.assessment.observation_started_minutes = current_minutes;
    }
    candidate.active_outing.assessment = summarize_normal_scout_assessment(
            candidate.active_outing );
    candidate.active_outing.assessment.last_progress_minutes = current_minutes;
    candidate.active_outing.assessment.burned_minutes = current_minutes;
    candidate.active_outing.assessment.burn_origin_omt = exposure->position;
    candidate.active_outing.assessment.target_alert = 100;
    candidate.active_outing.assessment.certainty = std::min(
                95, candidate.active_outing.assessment.certainty + 30 );
    const bool burned_readiness_latched =
        candidate.active_outing.assessment.threshold_class ==
        scout_assessment_threshold_class::burned &&
        candidate.active_outing.assessment.readiness_latched;
    candidate.active_outing.assessment.threshold_class =
        scout_assessment_threshold_class::burned;
    candidate.active_outing.assessment.readiness_latched =
        scout_assessment_readiness_after_certainty(
            scout_assessment_threshold_class::burned,
            burned_readiness_latched,
            candidate.active_outing.assessment.certainty );
    candidate.active_outing.assessment.next_eligible_minutes = minutes_after_saturated(
                current_minutes, 48 * 60 );
    candidate.active_outing.assessment.exit_reason =
        candidate.active_outing.assessment.readiness_latched ?
        "burned watch assessment ready" : "burned watch assessment partial";
    if( survival_direction ) {
        candidate.active_outing.local_handoff.egress_omt = survival_direction->omt;
    }
    candidate.active_outing.last_progress_minutes = current_minutes;
    candidate.active_outing.last_advanced_minutes = current_minutes;
    consume_local_pair_resume_receipt( candidate.active_outing );
    if( !simulation_owner_state_is_consistent( candidate.active_outing ) ||
        !candidate.roster().valid ) {
        return effect;
    }

    effect.result = covert_scout_burn_result::applied;
    effect.observer_id = exposure->npc_id;
    effect.target_observer_id = exposure->target_observer_id;
    effect.burn_origin_omt = exposure->position;
    effect.egress_omt = candidate.active_outing.local_handoff.egress_omt;
    effect.rally_omt = candidate.active_outing.local_handoff.egress_omt;
    site = std::move( candidate );
    if( record_transition_event ) {
        record_scout_phase_transition_event(
            site.active_outing, scout_phase::observing, burn_phase,
            selected_egress ? "reciprocal ordinary visual exposure" :
            survival_direction ? "reciprocal exposure chose a bounded local survival direction" :
            "reciprocal exposure with no legal local survival direction", current_minutes );
    }
    return effect;
}

covert_scout_burn_effect apply_covert_scout_burn(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_burn_read> &member_reads,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    const int current_minutes,
    const std::optional<structural_local_zombie_read> &danger_read )
{
    return apply_covert_scout_burn_impl(
               site, expected_cursor, member_reads, egress_candidates,
               current_minutes, danger_read, true );
}

static bool reconcile_covert_scout_unreachable_return_candidate(
    site_record &candidate,
    const std::vector<active_member_observation> &observations,
    int current_minutes );

local_structural_watch_exit_plan plan_local_structural_watch_exit(
    const site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_burn_read> &member_reads,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    const structural_local_zombie_read &danger_read, const int current_minutes,
    const bool home_routes_ready,
    const std::vector<active_member_observation> &unreachable_observations )
{
    local_structural_watch_exit_plan plan;
    plan.expected_cursor = expected_cursor;

    site_record planned_site = site;
    const covert_scout_burn_effect burn = apply_covert_scout_burn_impl(
            planned_site, expected_cursor, member_reads, egress_candidates,
            current_minutes, danger_read, false );
    if( burn.result != covert_scout_burn_result::applied ) {
        return plan;
    }
    const auto hard_danger = std::find_if(
                                 planned_site.active_outing.observations.begin(),
                                 planned_site.active_outing.observations.end(),
    [&danger_read, current_minutes]( const sortie_observation & observation ) {
        return observation.fact_key.rfind( "structural-local-zombie:", 0 ) == 0 &&
               observation.kind == sortie_observation_kind::hard_danger &&
               observation.critical && observation.observer_id == danger_read.observer_id &&
               observation.source_omt == danger_read.source_omt &&
               observation.observed_minutes == current_minutes &&
               observation.observed_power_low == danger_read.danger_low &&
               observation.observed_power_high == danger_read.danger_high &&
               observation.defender_ids == danger_read.stable_threat_ids;
    } );
    if( hard_danger == planned_site.active_outing.observations.end() ) {
        return plan;
    }

    plan.applicable = true;
    active_outing_state &next = planned_site.active_outing;
    next.phase = scout_phase::returning_home;
    next.local_handoff.phase = scout_phase::returning_home;
    next.alternate_watch_reposition_pending = false;
    next.current_covert_egress_route_omts.clear();
    next.assessment.readiness_latched = false;
    next.assessment.threshold_class = scout_assessment_threshold_class::none;
    next.assessment.last_progress_minutes = current_minutes;
    next.assessment.next_eligible_minutes = minutes_after_saturated(
            current_minutes, 24 * 60 );
    next.assessment.exit_reason.clear();
    next.last_progress_minutes = current_minutes;
    next.last_advanced_minutes = current_minutes;
    next.local_handoff.committed_minutes = current_minutes;
    if( !simulation_owner_state_is_consistent( next ) ||
        !planned_site.roster().valid ) {
        return plan;
    }

    plan.kind = home_routes_ready ?
                local_structural_watch_exit_kind::hard_danger_return :
                local_structural_watch_exit_kind::hard_danger_unreachable;
    plan.expected_site_id = site.site_id;
    plan.expected_target_revision = site.active_outing.target_lead_revision;
    plan.expected_member_ids = site.active_outing.member_ids;
    plan.expected_watch_omt = site.active_outing.selected_watch_omt;
    plan.next_outing = next;
    plan.unreachable_observations = unreachable_observations;
    plan.committed_minutes = current_minutes;
    if( !home_routes_ready ) {
        site_record closure_candidate = site;
        closure_candidate.active_outing = plan.next_outing;
        if( !reconcile_covert_scout_unreachable_return_candidate(
                closure_candidate, unreachable_observations, current_minutes ) ) {
            return plan;
        }
    }
    plan.valid = true;
    return plan;
}

bool complete_covert_scout_burned_egress(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_member_acquire_read> &member_reads,
    const int current_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( !simulation_cursor_matches( outing, expected_cursor ) ||
        outing.schema_version != 10 || outing.kind != outing_kind::structural_sortie ||
        outing.owner != simulation_owner::local ||
        outing.phase != scout_phase::burned_withdrawal ||
        outing.local_handoff.phase != outing.phase || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ) {
        return false;
    }

    bool has_unresolved_survivor = false;
    bool concealed_rally_reached = true;
    std::set<character_id> matched_reads;
    for( const character_id member_id : outing.member_ids ) {
        if( outing.member_is_resolved( member_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
            outing.casualty_ids.end() ) {
            continue;
        }
        has_unresolved_survivor = true;
        const auto read = std::find_if( member_reads.begin(), member_reads.end(),
        [member_id]( const covert_scout_member_acquire_read & candidate ) {
            return candidate.npc_id == member_id;
        } );
        if( read == member_reads.end() || !matched_reads.emplace( member_id ).second ||
            !read->position_known || read->position != outing.local_handoff.egress_omt ||
            !read->mutual_target_visibility_evaluated ||
            std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                       read->position ) != outing.target_footprint.end() ||
            std::count_if( member_reads.begin(), member_reads.end(),
        [member_id]( const covert_scout_member_acquire_read & candidate ) {
            return candidate.npc_id == member_id;
        } ) != 1 ) {
            return false;
        }
        concealed_rally_reached &= !read->mutual_target_visibility;
    }
    if( !has_unresolved_survivor || matched_reads.size() != member_reads.size() ) {
        return false;
    }
    if( !concealed_rally_reached ) {
        return false;
    }

    site_record candidate = site;
    const scout_phase next_phase = scout_phase_after_burned_evacuation( true );
    candidate.active_outing.phase = next_phase;
    candidate.active_outing.local_handoff.phase = next_phase;
    candidate.active_outing.last_progress_minutes = current_minutes;
    candidate.active_outing.last_advanced_minutes = current_minutes;
    if( !simulation_owner_state_is_consistent( candidate.active_outing ) ||
        !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    record_scout_phase_transition_event(
        site.active_outing, scout_phase::burned_withdrawal, next_phase,
        "burned pair reached concealed rally", current_minutes );
    return true;
}

covert_scout_egress_failure_effect resolve_covert_scout_burned_egress_failure(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    const int current_minutes )
{
    covert_scout_egress_failure_effect effect;
    const active_outing_state &outing = site.active_outing;
    if( !simulation_cursor_matches( outing, expected_cursor ) ||
        outing.schema_version != 10 || outing.kind != outing_kind::structural_sortie ||
        outing.owner != simulation_owner::local ||
        outing.phase != scout_phase::burned_withdrawal ||
        outing.local_handoff.phase != outing.phase || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ||
        outing.covert_egress_revision == std::numeric_limits<int>::max() ||
        !covert_scout_egress_candidates_are_valid(
            outing.local_handoff.egress_omt, outing.target_footprint, egress_candidates,
            outing.selected_watch_omt ) ) {
        return effect;
    }

    site_record candidate = site;
    active_outing_state &next = candidate.active_outing;
    next.covert_egress_revision++;
    effect.failed_egress_omt = next.local_handoff.egress_omt;
    next.failed_covert_egress_omts.push_back( effect.failed_egress_omt );
    for( const tripoint_abs_omt &route_omt : next.current_covert_egress_route_omts ) {
        if( std::find( next.failed_covert_egress_route_omts.begin(),
                      next.failed_covert_egress_route_omts.end(), route_omt ) ==
            next.failed_covert_egress_route_omts.end() ) {
            next.failed_covert_egress_route_omts.push_back( route_omt );
        }
    }
    next.current_covert_egress_route_omts.clear();

    std::vector<covert_scout_egress_candidate> alternatives;
    alternatives.reserve( egress_candidates.size() );
    const std::optional<int> failed_distance = target_footprint_watch_distance(
                effect.failed_egress_omt, next.target_footprint );
    std::copy_if( egress_candidates.begin(), egress_candidates.end(),
                  std::back_inserter( alternatives ), [&next, &failed_distance](
                      const covert_scout_egress_candidate &candidate_egress ) {
        const std::optional<int> candidate_distance = target_footprint_watch_distance(
                    candidate_egress.omt, next.target_footprint );
        return failed_distance && candidate_distance &&
               *candidate_distance > *failed_distance &&
               std::find( next.failed_covert_egress_omts.begin(),
                          next.failed_covert_egress_omts.end(), candidate_egress.omt ) ==
               next.failed_covert_egress_omts.end() &&
               std::none_of( candidate_egress.route_omts.begin(),
                             candidate_egress.route_omts.end(),
        [&next]( const tripoint_abs_omt &route_omt ) {
            return route_omt == next.selected_watch_omt ||
                   std::find( next.failed_covert_egress_omts.begin(),
                              next.failed_covert_egress_omts.end(), route_omt ) !=
                   next.failed_covert_egress_omts.end() ||
                   std::find( next.failed_covert_egress_route_omts.begin(),
                              next.failed_covert_egress_route_omts.end(), route_omt ) !=
                   next.failed_covert_egress_route_omts.end();
        } );
    } );
    const std::optional<covert_scout_egress_candidate> selected =
        next.covert_egress_chain_version == current_covert_egress_chain_version &&
        next.covert_egress_attempts < max_covert_egress_attempts ?
        select_covert_scout_egress(
            effect.failed_egress_omt, next.target_footprint, alternatives,
            next.selected_watch_omt ) : std::nullopt;
    if( selected ) {
        next.covert_egress_attempts++;
        next.local_handoff.egress_omt = selected->omt;
        next.current_covert_egress_route_omts = selected->route_omts;
        effect.egress_omt = selected->omt;
        effect.result = covert_scout_egress_failure_result::retried;
    } else {
        next.phase = scout_phase::returning_exposed;
        next.local_handoff.phase = scout_phase::returning_exposed;
        effect.egress_omt = next.local_handoff.egress_omt;
        effect.result = covert_scout_egress_failure_result::exhausted;
    }
    candidate.active_outing.last_progress_minutes = current_minutes;
    candidate.active_outing.last_advanced_minutes = current_minutes;
    if( !simulation_owner_state_is_consistent( candidate.active_outing ) ||
        !candidate.roster().valid ) {
        return covert_scout_egress_failure_effect();
    }
    site = std::move( candidate );
    if( effect.result == covert_scout_egress_failure_result::exhausted ) {
        record_scout_phase_transition_event(
            site.active_outing, scout_phase::burned_withdrawal,
            scout_phase::returning_exposed,
            "burned pair exhausted its bounded remembered egress routes",
            current_minutes );
    }
    return effect;
}

static bool record_active_outing_casualty_unchecked( site_record &site,
        character_id npc_id, member_state casualty_state, int current_minutes,
        const std::string &summary );

static bool reconcile_covert_scout_unreachable_return_candidate(
    site_record &candidate,
    const std::vector<active_member_observation> &observations,
    const int current_minutes )
{
    const active_outing_state &outing = candidate.active_outing;
    if( observations.size() != outing.member_ids.size() ) {
        return false;
    }
    std::set<character_id> observed_ids;
    for( const active_member_observation &observation : observations ) {
        if( std::find( outing.member_ids.begin(), outing.member_ids.end(), observation.npc_id ) ==
            outing.member_ids.end() || !observed_ids.emplace( observation.npc_id ).second ) {
            return false;
        }
    }

    const active_outing_state closed_outing = candidate.active_outing;
    bool member_closed = false;
    bool physical_survivor_home = false;
    for( const character_id member_id : closed_outing.member_ids ) {
        const auto observation = std::find_if( observations.begin(), observations.end(),
        [member_id]( const active_member_observation & candidate_observation ) {
            return candidate_observation.npc_id == member_id;
        } );
        if( observation == observations.end() ) {
            return false;
        }
        if( closed_outing.member_is_resolved( member_id ) ||
            std::find( closed_outing.casualty_ids.begin(), closed_outing.casualty_ids.end(),
                       member_id ) != closed_outing.casualty_ids.end() ) {
            const member_record *resolved_member = candidate.find_member( member_id );
            const bool resolution_matches = resolved_member != nullptr &&
                    ( ( resolved_member->state == member_state::at_home &&
                        observation->state == active_member_observation_state::home ) ||
                      ( resolved_member->state == member_state::dead &&
                        observation->state == active_member_observation_state::dead ) ||
                      ( resolved_member->state == member_state::missing &&
                        observation->state == active_member_observation_state::missing ) );
            if( !resolution_matches ) {
                return false;
            }
            physical_survivor_home |= resolved_member->state == member_state::at_home;
            continue;
        }
        member_record *member = candidate.find_member( member_id );
        if( member == nullptr ||
            ( member->state != member_state::outbound &&
              member->state != member_state::local_contact ) ) {
            return false;
        }
        switch( observation->state ) {
            case active_member_observation_state::home:
                if( member->state == member_state::local_contact &&
                    !update_member_state(
                        candidate, member_id, member_state::outbound,
                        "covert scout reached home with physical return writeback pending" ) ) {
                    return false;
                }
                physical_survivor_home = true;
                break;
            case active_member_observation_state::dead:
            case active_member_observation_state::missing: {
                const member_state casualty_state =
                    observation->state == active_member_observation_state::dead ?
                    member_state::dead : member_state::missing;
                if( !record_active_outing_casualty_unchecked(
                        candidate, member_id, casualty_state, current_minutes,
                        observation->summary ) ) {
                    return false;
                }
                break;
            }
            case active_member_observation_state::local_contact:
            case active_member_observation_state::returning_home:
                if( !update_member_state(
                        candidate, member_id, member_state::orphaned,
                        "covert scout stranded after authoritative return route failed" ) ) {
                    return false;
                }
                candidate.active_outing.resolved_member_ids.push_back( member_id );
                break;
            case active_member_observation_state::unresolved:
                return false;
        }
        member_closed = true;
    }
    if( !member_closed ) {
        return false;
    }

    advance_camp_supply( candidate, current_minutes );
    if( candidate.routine_activated_minutes < 0 ) {
        candidate.routine_activated_minutes = current_minutes;
        candidate.routine_no_candidate_streak = 0;
    }
    candidate.last_routine_resolved_minutes = current_minutes;
    candidate.next_routine_dispatch_eligible_minutes = minutes_after_saturated(
                current_minutes, 6 * 60 );
    if( physical_survivor_home ) {
        if( camp_map_lead *lead = candidate.intelligence_map.find_lead(
                                     closed_outing.target_lead_id ) ) {
            apply_returned_structural_threat_observation( candidate, *lead, current_minutes );
        }
        apply_returned_structural_signal_observations( candidate, current_minutes );
        if( !credit_structural_return_cargo( candidate, current_minutes ) ) {
            return false;
        }
    }
    if( !release_structural_outing_reservation(
            candidate, closed_outing.activity_id, closed_outing.generation,
            "authoritative failed return reconciled physical arrivals and casualties" ) ||
        !candidate.roster().valid ) {
        return false;
    }
    return true;
}

bool abandon_covert_scout_unreachable_return(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<active_member_observation> &observations,
    const int current_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( !simulation_cursor_matches( outing, expected_cursor ) ||
        outing.schema_version != 10 || outing.kind != outing_kind::structural_sortie ||
        outing.owner != simulation_owner::local ||
        ( outing.phase != scout_phase::returning_exposed &&
          outing.phase != scout_phase::returning_report &&
          outing.phase != scout_phase::returning_home ) ||
        outing.local_handoff.phase != outing.phase || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ) {
        return false;
    }
    const active_outing_state closed_outing = outing;
    site_record candidate = site;
    if( !reconcile_covert_scout_unreachable_return_candidate(
            candidate, observations, current_minutes ) ) {
        return false;
    }
    site = std::move( candidate );
    record_scout_phase_transition_event(
        closed_outing, closed_outing.phase, scout_phase::lost,
        "authoritative return route failed; arrivals, casualties, and stranded scouts reconciled",
        current_minutes );
    return true;
}

local_handoff_commit_result commit_local_structural_watch_exit(
    site_record &site, const local_structural_watch_exit_plan &plan,
    const std::function<bool( character_id )> &prepare_member,
    const std::function<void( character_id )> &rollback_member )
{
    if( !plan.applicable || !plan.valid || !prepare_member || !rollback_member ||
        plan.kind == local_structural_watch_exit_kind::none ||
        plan.committed_minutes < 0 ) {
        return local_handoff_commit_result::rejected;
    }
    const active_outing_state &current = site.active_outing;
    if( site.site_id != plan.expected_site_id ||
        !simulation_cursor_matches( current, plan.expected_cursor ) ||
        current.schema_version != 10 ||
        current.kind != outing_kind::structural_sortie ||
        current.owner != simulation_owner::local ||
        current.phase != scout_phase::observing ||
        current.local_handoff.phase != current.phase ||
        current.target_lead_revision != plan.expected_target_revision ||
        current.member_ids != plan.expected_member_ids ||
        current.selected_watch_omt != plan.expected_watch_omt ||
        plan.next_outing.activity_id != current.activity_id ||
        plan.next_outing.generation != current.generation ||
        plan.next_outing.owner != current.owner ||
        plan.next_outing.handoff_epoch != current.handoff_epoch ||
        plan.next_outing.phase != scout_phase::returning_home ||
        plan.next_outing.local_handoff.phase != scout_phase::returning_home ||
        plan.next_outing.last_advanced_minutes != plan.committed_minutes ) {
        return local_handoff_commit_result::rejected;
    }

    site_record candidate = site;
    candidate.active_outing = plan.next_outing;
    if( plan.kind == local_structural_watch_exit_kind::hard_danger_unreachable &&
        !reconcile_covert_scout_unreachable_return_candidate(
            candidate, plan.unreachable_observations, plan.committed_minutes ) ) {
        return local_handoff_commit_result::rejected;
    }
    if( plan.kind == local_structural_watch_exit_kind::hard_danger_return &&
        ( !simulation_owner_state_is_consistent( candidate.active_outing ) ||
          !candidate.roster().valid ) ) {
        return local_handoff_commit_result::rejected;
    }

    std::vector<character_id> prepared_members;
    const auto rollback_prepared = [&prepared_members, &rollback_member]() {
        for( auto iter = prepared_members.rbegin(); iter != prepared_members.rend(); ++iter ) {
            try {
                rollback_member( *iter );
            } catch( ... ) {
                // Continue restoring the rest of the exact pair.
            }
        }
    };
    try {
        for( const character_id member_id : plan.expected_member_ids ) {
            prepared_members.push_back( member_id );
            if( !prepare_member( member_id ) ) {
                rollback_prepared();
                return local_handoff_commit_result::rolled_back;
            }
        }
    } catch( ... ) {
        rollback_prepared();
        return local_handoff_commit_result::rolled_back;
    }

    const active_outing_state closed_outing = site.active_outing;
    site = std::move( candidate );
    const scout_phase next_phase =
        plan.kind == local_structural_watch_exit_kind::hard_danger_return ?
        scout_phase::returning_home : scout_phase::lost;
    record_scout_phase_transition_event(
        closed_outing, scout_phase::observing, next_phase,
        plan.kind == local_structural_watch_exit_kind::hard_danger_return ?
        "overwhelming local danger outranked simultaneous exposure" :
        "overwhelming local danger and simultaneous exposure found no home route; pair reconciled",
        plan.committed_minutes );
    return local_handoff_commit_result::applied;
}

bool covert_scout_party_cleared_target_acquire_range(
    const active_outing_state &outing,
    const std::vector<covert_scout_member_acquire_read> &member_reads )
{
    if( outing.kind != outing_kind::structural_sortie || outing.schema_version != 10 ||
        outing.target_footprint.empty() || outing.member_ids.size() != 2 ) {
        return false;
    }

    bool has_unresolved_survivor = false;
    std::set<character_id> matched_reads;
    for( const character_id member_id : outing.member_ids ) {
        if( outing.member_is_resolved( member_id ) ||
            std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
            outing.casualty_ids.end() ) {
            continue;
        }
        has_unresolved_survivor = true;
        const auto read = std::find_if( member_reads.begin(), member_reads.end(),
        [member_id]( const covert_scout_member_acquire_read & candidate ) {
            return candidate.npc_id == member_id;
        } );
        if( read == member_reads.end() || !matched_reads.emplace( member_id ).second ||
            !read->position_known || !read->returning_home ||
            !read->mutual_target_visibility_evaluated || read->mutual_target_visibility ||
            std::find( outing.target_footprint.begin(), outing.target_footprint.end(),
                       read->position ) != outing.target_footprint.end() ) {
            return false;
        }
        if( std::count_if( member_reads.begin(), member_reads.end(),
        [member_id]( const covert_scout_member_acquire_read & candidate ) {
            return candidate.npc_id == member_id;
        } ) != 1 ) {
            return false;
        }
    }
    return has_unresolved_survivor;
}

bool release_covert_cohesion_abort_after_target_clear(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_member_acquire_read> &member_reads )
{
    const active_outing_state &outing = site.active_outing;
    if( !simulation_cursor_matches( outing, expected_cursor ) ||
        outing.schema_version != 10 || outing.kind != outing_kind::structural_sortie ||
        outing.owner != simulation_owner::local ||
        outing.phase != scout_phase::returning_home ||
        !outing.local_handoff.cohesion_abort_return ||
        !covert_scout_party_cleared_target_acquire_range( outing, member_reads ) ) {
        return false;
    }

    site_record candidate = site;
    candidate.active_outing.local_handoff.cohesion_assembled = true;
    candidate.active_outing.local_handoff.cohesion_abort_return = false;
    candidate.active_outing.local_handoff.cohesion_deadline_minutes = -1;
    candidate.active_outing.local_handoff.cohesion_reroutes_used = 0;
    if( !simulation_owner_state_is_consistent( candidate.active_outing ) ||
        !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

shakedown_aftermath_effect apply_shakedown_outcome( site_record &site,
        const shakedown_outcome &outcome )
{
    shakedown_aftermath_effect effect;
    if( ( !outcome.paid && !outcome.fought ) || outcome.demanded_value <= 0 ) {
        effect.notes.push_back( "shakedown aftermath ignored: no concrete paid/fought outcome" );
        return effect;
    }

    effect.valid = true;
    site.last_shakedown_outcome = shakedown_outcome_label( outcome );
    site.shakedown_last_demanded_value = outcome.demanded_value;
    site.shakedown_last_surrendered_value = outcome.surrendered_value;
    site.shakedown_last_reachable_value = outcome.reachable_goods_value;

    if( outcome.paid ) {
        site.shakedown_loot_value += std::max( 0, outcome.surrendered_value );
        site.remembered_bounty_estimate += std::max( 1, outcome.surrendered_value / 1000 );
        effect.notes.push_back( "paid shakedown writes surrendered value into abstract bounty" );
    }

    if( outcome.fought ) {
        site.shakedown_anger += 1 + outcome.defender_losses;
        site.remembered_threat_estimate = std::max( 0,
                                          site.remembered_threat_estimate - outcome.defender_losses );
        effect.notes.push_back( "fight outcome writes anger and changed local threat into site memory" );
    }

    if( outcome.basecamp_or_camp_scene && outcome.defender_losses > 0 ) {
        site.shakedown_defender_losses += outcome.defender_losses;
        if( !site.shakedown_reopen_used ) {
            site.shakedown_reopen_available = true;
            effect.stronger_reopen = true;
            effect.notes.push_back( "defender loss opens exactly one stronger renegotiation demand" );
        }
    }

    if( outcome.bandit_losses > 0 || outcome.extraction_failed ) {
        site.shakedown_bandit_losses += outcome.bandit_losses;
        site.shakedown_caution += std::max( 1, outcome.bandit_losses );
        site.remembered_retreat_bias += std::max( 1, outcome.bandit_losses );
        site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
        effect.cools_later_pressure = true;
        effect.notes.push_back( "bandit loss or failed extraction cools later pressure instead of only escalating" );
    }

    effect.demand_modifier_percent = shakedown_demand_modifier_percent( site );
    return effect;
}


void begin_shakedown_basecamp_defender_observation( site_record &site, const int live_defenders )
{
    site.shakedown_basecamp_defenders_at_fight = std::max( 0, live_defenders );
    site.shakedown_basecamp_defender_observation_pending =
        site.shakedown_basecamp_defenders_at_fight > 0;
}

shakedown_aftermath_effect apply_shakedown_basecamp_defender_observation( site_record &site,
        const int live_defenders )
{
    if( !site.shakedown_basecamp_defender_observation_pending ) {
        shakedown_aftermath_effect effect;
        effect.notes.push_back( "basecamp defender observation ignored: no pending shakedown fight" );
        return effect;
    }

    const int current_live_defenders = std::max( 0, live_defenders );
    const int defender_losses = site.shakedown_basecamp_defenders_at_fight - current_live_defenders;
    if( defender_losses <= 0 ) {
        shakedown_aftermath_effect effect;
        effect.notes.push_back( "basecamp defender observation unchanged: no defender strength drop" );
        return effect;
    }

    shakedown_outcome outcome;
    outcome.fought = true;
    outcome.basecamp_or_camp_scene = true;
    outcome.demanded_value = std::max( 1, site.shakedown_last_demanded_value );
    outcome.reachable_goods_value = site.shakedown_last_reachable_value;
    outcome.defender_losses = defender_losses;
    site.shakedown_basecamp_defender_observation_pending = false;
    return apply_shakedown_outcome( site, outcome );
}

bool mark_shakedown_reopen_used( site_record &site )
{
    if( !site.shakedown_reopen_available || site.shakedown_reopen_used ) {
        return false;
    }
    site.shakedown_reopen_used = true;
    site.shakedown_reopen_available = false;
    return true;
}

std::string render_empty_site_retirement_report( const site_record &site )
{
    int spawn_tile_assigned_living_total = 0;
    for( const spawn_tile_record &spawn_tile : site.spawn_tiles ) {
        spawn_tile_assigned_living_total += std::max( 0, spawn_tile.assigned_living_total );
    }

    std::ostringstream out;
    out << "bandit_live_world retired_empty_site: site=" << site.site_id
        << " site_kind=" << to_string( site.site_kind )
        << " living_total=" << site.living_total
        << " at_home=" << site.count_members_in_state( member_state::at_home )
        << " spawn_tile_assigned_living_total=" << spawn_tile_assigned_living_total
        << " active_group=" << ( !site.active_outing.is_active() ? "no" : site.active_outing.activity_id )
        << " active_member_ids=" << site.active_outing.member_ids.size()
        << " outbound=" << site.count_members_in_state( member_state::outbound )
        << " local_contact=" << site.count_members_in_state( member_state::local_contact )
        << " home_side_signals=" << site.count_home_side_signals()
        << " active_outside=" << ( site.has_active_outside_pressure() ? "yes" : "no" );
    return out.str();
}

int retire_empty_hostile_sites( world_state &state, std::vector<std::string> *reports )
{
    int retired_count = 0;
    for( site_record &site : state.sites ) {
        if( !site.eligible_for_empty_site_retirement() ) {
            continue;
        }
        site.retired_empty_site = true;
        site.retirement_summary = render_empty_site_retirement_report( site );
        if( reports != nullptr ) {
            reports->push_back( site.retirement_summary );
        }
        retired_count++;
    }
    return retired_count;
}

bool apply_return_packet( site_record &site, const bandit_pursuit_handoff::return_packet &packet )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::live_return_apply );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::live_return_applies );
    bandit_live_world_probe::record_site_service( site.site_id,
            bandit_live_world_probe::site_service::live_return_apply );
    const bool scout_return = site.active_outing.kind == outing_kind::scout_sortie;
    const bool assessment_scout_return = scout_return &&
                                         site.active_outing.job_type == "scout";
    const bandit_pursuit_handoff::abstract_group_state packet_group =
        make_site_memory_group( site );
    if( !packet.valid || packet.source_camp_id != site.site_id ||
        !site.active_outing.is_active() ||
        site.active_outing.kind == outing_kind::structural_sortie ||
        site.active_outing.return_application_key.empty() ||
        site.active_outing.cargo_application_key.empty() ||
        ( scout_return && site.active_outing.report_application_key.empty() ) ||
        packet.group_id != site.active_outing.activity_id ||
        packet.activity_generation != site.active_outing.generation ||
        packet.handoff_epoch != site.active_outing.handoff_epoch ||
        packet.return_application_key != site.active_outing.return_application_key ||
        packet.report_application_key != site.active_outing.report_application_key ||
        packet.cargo_application_key != site.active_outing.cargo_application_key ||
        !bandit_pursuit_handoff::return_packet_matches_group( packet_group, packet ) ||
        packet.activity_generation <= site.applied_return_generation ||
        packet.activity_generation <= site.applied_cargo_generation ||
        packet.activity_generation <= site.applied_report_generation ||
        bandit_dry_run::to_string( packet.job_type ) != site.active_outing.job_type ||
        site.active_outing.member_ids.empty() ) {
        return false;
    }

    const auto status_for_member = [&packet]( character_id member_id ) -> std::string {
        const std::string member_token = std::to_string( member_id.get_value() );
        const auto iter = std::find_if( packet.anchored_identity_updates.begin(),
        packet.anchored_identity_updates.end(), [&member_token]( const bandit_pursuit_handoff::anchored_identity_state & update ) {
            return update.id == member_token;
        } );
        return iter != packet.anchored_identity_updates.end() ? iter->status : "alive";
    };

    std::vector<std::string> active_member_tokens;
    active_member_tokens.reserve( site.active_outing.member_ids.size() );
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const std::string member_token = std::to_string( member_id.get_value() );
        if( site.find_member( member_id ) == nullptr ||
            std::find( active_member_tokens.begin(), active_member_tokens.end(), member_token ) !=
            active_member_tokens.end() ) {
            return false;
        }
        active_member_tokens.push_back( member_token );
    }

    std::vector<std::string> update_tokens;
    update_tokens.reserve( packet.anchored_identity_updates.size() );
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        const bool known_status = update.status == "alive" || update.status == "wounded" ||
                                  update.status == "dead" || update.status == "missing";
        if( !known_status ||
            std::find( active_member_tokens.begin(), active_member_tokens.end(), update.id ) ==
            active_member_tokens.end() ||
            std::find( update_tokens.begin(), update_tokens.end(), update.id ) != update_tokens.end() ) {
            return false;
        }
        update_tokens.push_back( update.id );
    }

    for( const character_id &resolved_member_id : site.active_outing.resolved_member_ids ) {
        const member_record *resolved_member = site.find_member( resolved_member_id );
        const bool casualty = std::find( site.active_outing.casualty_ids.begin(),
                                        site.active_outing.casualty_ids.end(), resolved_member_id ) !=
                              site.active_outing.casualty_ids.end();
        const std::string resolved_status = status_for_member( resolved_member_id );
        if( resolved_member == nullptr ||
            ( casualty && resolved_member->state != member_state::dead &&
              resolved_member->state != member_state::missing ) ||
            ( !casualty && ( resolved_member->state != member_state::at_home ||
                             resolved_status == "dead" || resolved_status == "missing" ) ) ) {
            return false;
        }
    }

    for( const character_id &casualty_id : site.active_outing.casualty_ids ) {
        const member_record *casualty = site.find_member( casualty_id );
        const std::string casualty_status = status_for_member( casualty_id );
        if( std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                      casualty_id ) == site.active_outing.member_ids.end() ||
            casualty == nullptr ||
            ( casualty->state != member_state::dead && casualty->state != member_state::missing ) ||
            ( casualty->state == member_state::dead && casualty_status != "dead" ) ||
            ( casualty->state == member_state::missing && casualty_status != "missing" ) ) {
            return false;
        }
    }

    int expected_survivors = static_cast<int>( site.active_outing.member_ids.size() );
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        if( update.status == "dead" || update.status == "missing" ) {
            expected_survivors--;
        }
    }
    if( packet.survivors_remaining != expected_survivors ) {
        return false;
    }
    std::optional<int> scout_report_revision;
    if( scout_return && packet.survivors_remaining > 0 ) {
        scout_report_revision = next_scout_report_revision( site );
        if( !scout_report_revision ) {
            return false;
        }
    }

    const std::string target_label = packet.current_target_or_mark.empty() ? site.active_outing.target_id :
                                     packet.current_target_or_mark;
    const std::string base_summary = "return " + bandit_pursuit_handoff::to_string( packet.result ) +
                                     " from " + target_label;
    bool shakedown_fight_outing = site.last_shakedown_outcome == "fight_unresolved";
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const member_record *member = site.find_member( member_id );
        shakedown_fight_outing |= member != nullptr &&
                                  member->last_writeback_summary.find( "shakedown_surface fight" ) != std::string::npos;
    }
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const std::string status = status_for_member( member_id );
        member_state new_state = member_state::at_home;
        std::string summary = base_summary;
        if( status == "dead" ) {
            new_state = member_state::dead;
            summary += " (dead)";
        } else if( status == "missing" ) {
            new_state = member_state::missing;
            summary += " (missing)";
        } else {
            if( status != "alive" ) {
                summary += " (" + status + ")";
            }
        }
        if( !update_member_state( site, member_id, new_state, summary ) ) {
            return false;
        }
    }

    int bandit_losses = 0;
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        if( update.status == "dead" || update.status == "missing" ) {
            bandit_losses++;
        }
    }
    if( shakedown_fight_outing && bandit_losses > 0 ) {
        shakedown_outcome outcome;
        outcome.fought = true;
        outcome.extraction_failed = true;
        outcome.demanded_value = std::max( 1, site.shakedown_last_demanded_value );
        outcome.reachable_goods_value = site.shakedown_last_reachable_value;
        outcome.bandit_losses = bandit_losses;
        apply_shakedown_outcome( site, outcome );
    }

    record_scout_return_lead( site, packet, bandit_losses );

    bandit_pursuit_handoff::abstract_group_state remembered_group = packet_group;
    bandit_pursuit_handoff::apply_return_packet( remembered_group, packet );
    apply_group_memory( site, remembered_group );

    if( scout_return && packet.survivors_remaining > 0 ) {
        scout_report_record report;
        report.revision = *scout_report_revision;
        report.action_policy = report_policy_for_profile( effective_profile( site ) );
        report.source_activity_id = site.active_outing.activity_id;
        report.source_generation = site.active_outing.generation;
        report.source_job_type = site.active_outing.job_type;
        report.target_id = site.active_outing.target_id;
        report.target_omt = site.active_outing.target_omt;
        report.target_lead_id = site.active_outing.target_lead_id;
        report.target_lead_revision = site.active_outing.target_lead_revision;
        report.application_key = site.active_outing.report_application_key;
        std::vector<character_id> carrier_ids;
        for( const character_id member_id : site.active_outing.member_ids ) {
            const member_record *member = site.find_member( member_id );
            if( member != nullptr && member->state == member_state::at_home ) {
                carrier_ids.push_back( member_id );
            }
        }
        report.observations = make_reportable_sortie_observations(
                                  site.active_outing.observations, carrier_ids );
        report.casualty_ids = site.active_outing.casualty_ids;
        for( const bandit_pursuit_handoff::anchored_identity_state &update :
             packet.anchored_identity_updates ) {
            if( update.status != "dead" && update.status != "missing" ) {
                continue;
            }
            const auto member_token = std::find( active_member_tokens.begin(), active_member_tokens.end(),
                                                 update.id );
            if( member_token == active_member_tokens.end() ) {
                continue;
            }
            const std::size_t member_index = static_cast<std::size_t>(
                    std::distance( active_member_tokens.begin(), member_token ) );
            const character_id casualty_id = site.active_outing.member_ids[member_index];
            if( std::find( report.casualty_ids.begin(), report.casualty_ids.end(), casualty_id ) ==
                report.casualty_ids.end() ) {
                report.casualty_ids.push_back( casualty_id );
            }
        }
        report.delivered_minutes = std::max( { site.active_outing.started_minutes,
                                     site.active_outing.local_contact_minutes,
                                     site.active_outing.last_progress_minutes,
                                     site.active_outing.last_advanced_minutes } );
        site.current_scout_report = std::move( report );
    }
    site.applied_report_generation = scout_return ? packet.activity_generation :
                                     site.applied_report_generation;

    if( packet.survivors_remaining > 0 ) {
        const long long supply_total = static_cast<long long>( site.returned_cargo_stock.supply_units ) +
                                       std::max( 0, site.active_outing.cargo.supply_units );
        const long long trade_total = static_cast<long long>( site.returned_cargo_stock.trade_value ) +
                                      std::max( 0, site.active_outing.cargo.trade_value );
        site.returned_cargo_stock.supply_units = static_cast<int>( std::min<long long>(
                    supply_total, std::numeric_limits<int>::max() ) );
        site.returned_cargo_stock.trade_value = static_cast<int>( std::min<long long>(
                    trade_total, std::numeric_limits<int>::max() ) );
    }
    site.applied_cargo_generation = packet.activity_generation;
    site.last_cargo_application_key = site.active_outing.cargo_application_key;
    site.applied_return_generation = packet.activity_generation;
    site.active_outing.clear();
    if( assessment_scout_return && packet.survivors_remaining > 0 ) {
        accept_current_scout_report_for_assessment( site );
    }
    return true;
}

static bool record_active_outing_casualty_unchecked( site_record &site,
        const character_id npc_id, const member_state casualty_state,
        const int current_minutes, const std::string &summary )
{
    if( !site.active_outing.is_active() ||
        ( casualty_state != member_state::dead && casualty_state != member_state::missing ) ||
        std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                   npc_id ) == site.active_outing.member_ids.end() ) {
        return false;
    }
    if( casualty_state == member_state::missing &&
        ( site.active_outing.missing_deadline_minutes < 0 ||
          current_minutes < site.active_outing.missing_deadline_minutes ) ) {
        return false;
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    member_record *member = candidate.find_member( npc_id );
    if( member == nullptr ) {
        return false;
    }
    const bool state_changed = member->state != casualty_state;
    const bool casualty_was_recorded = std::find(
            candidate.active_outing.casualty_ids.begin(),
            candidate.active_outing.casualty_ids.end(), npc_id ) !=
        candidate.active_outing.casualty_ids.end();
    if( !update_member_state( candidate, npc_id, casualty_state, summary ) ) {
        return false;
    }
    advance_camp_supply( candidate, current_minutes );
    if( !casualty_was_recorded ) {
        candidate.active_outing.casualty_ids.push_back( npc_id );
    }
    const bool resolution_was_recorded =
        candidate.active_outing.member_is_resolved( npc_id );
    if( !resolution_was_recorded ) {
        candidate.active_outing.resolved_member_ids.push_back( npc_id );
    }
    candidate.active_outing.last_progress_minutes = std::max(
                candidate.active_outing.last_progress_minutes, current_minutes );
    candidate.active_outing.last_advanced_minutes = current_minutes;
    if( candidate.active_outing.casualty_ids.size() >=
        candidate.active_outing.member_ids.size() ) {
        candidate.active_outing.phase = scout_phase::lost;
    }
    site = std::move( candidate );
    return state_changed || !casualty_was_recorded || !resolution_was_recorded;
}

scout_resolution_effect apply_active_scout_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<active_member_observation> &observations,
        const int current_minutes )
{
    scout_resolution_effect effect;
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        expected_cursor.owner != simulation_owner::local ||
        site.active_outing.kind != outing_kind::scout_sortie ||
        site.active_outing.job_type != "scout" || current_minutes < 0 ||
        current_minutes <= site.active_outing.last_advanced_minutes ||
        observations.size() != site.active_outing.member_ids.size() ) {
        return effect;
    }

    std::vector<character_id> observed_member_ids;
    observed_member_ids.reserve( observations.size() );
    for( const active_member_observation &observation : observations ) {
        if( std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                      observation.npc_id ) == site.active_outing.member_ids.end() ||
            std::find( observed_member_ids.begin(), observed_member_ids.end(), observation.npc_id ) !=
            observed_member_ids.end() ) {
            return effect;
        }
        observed_member_ids.push_back( observation.npc_id );
    }

    site_record candidate = site;
    std::vector<character_id> newly_returned_ids;
    int newly_resolved = 0;
    for( const character_id &member_id : candidate.active_outing.member_ids ) {
        const auto observation_iter = std::find_if( observations.begin(), observations.end(),
        [&member_id]( const active_member_observation & observation ) {
            return observation.npc_id == member_id;
        } );
        if( observation_iter == observations.end() ) {
            return effect;
        }

        member_record *member = candidate.find_member( member_id );
        if( member == nullptr ) {
            return effect;
        }
        if( candidate.active_outing.member_is_resolved( member_id ) ) {
            const bool observation_matches_resolution =
                ( member->state == member_state::at_home &&
                  observation_iter->state == active_member_observation_state::home ) ||
                ( member->state == member_state::dead &&
                  observation_iter->state == active_member_observation_state::dead ) ||
                ( member->state == member_state::missing &&
                  observation_iter->state == active_member_observation_state::missing );
            if( !observation_matches_resolution ) {
                return effect;
            }
            continue;
        }

        switch( observation_iter->state ) {
            case active_member_observation_state::unresolved:
            case active_member_observation_state::local_contact:
            case active_member_observation_state::returning_home:
                break;
            case active_member_observation_state::home:
                if( !update_member_state( candidate, member_id, member_state::at_home,
                                         observation_iter->summary ) ) {
                    return effect;
                }
                candidate.active_outing.resolved_member_ids.push_back( member_id );
                newly_returned_ids.push_back( member_id );
                newly_resolved++;
                break;
            case active_member_observation_state::dead:
            case active_member_observation_state::missing: {
                const member_state casualty_state =
                    observation_iter->state == active_member_observation_state::dead ?
                    member_state::dead : member_state::missing;
                if( !record_active_outing_casualty_unchecked(
                        candidate, member_id, casualty_state, current_minutes,
                        observation_iter->summary ) ) {
                    return effect;
                }
                newly_resolved++;
                break;
            }
        }
    }

    effect.valid = true;
    effect.newly_resolved = newly_resolved;
    effect.newly_returned = static_cast<int>( newly_returned_ids.size() );
    const bool all_members_resolved =
        candidate.active_outing.resolved_member_ids.size() ==
        candidate.active_outing.member_ids.size();
    if( newly_resolved == 0 && !all_members_resolved ) {
        return effect;
    }
    if( newly_resolved > 0 ) {
        candidate.active_outing.last_progress_minutes = std::max(
                    candidate.active_outing.last_progress_minutes, current_minutes );
        candidate.active_outing.last_advanced_minutes = std::max(
                    candidate.active_outing.last_advanced_minutes, current_minutes );
    }
    if( all_members_resolved ) {
        const std::optional<bandit_pursuit_handoff::return_packet> packet =
            resolve_active_group_aftermath( candidate, observations );
        if( !packet || !apply_return_packet( candidate, *packet ) ) {
            return scout_resolution_effect();
        }
        site = std::move( candidate );
        effect.changed = true;
        effect.completed = true;
        return effect;
    }

    if( !newly_returned_ids.empty() ) {
        const std::optional<int> report_revision = next_scout_report_revision( candidate );
        if( !report_revision ) {
            return scout_resolution_effect();
        }
        scout_report_record report;
        report.revision = *report_revision;
        report.action_policy = report_policy_for_profile( effective_profile( candidate ) );
        report.source_activity_id = candidate.active_outing.activity_id;
        report.source_generation = candidate.active_outing.generation;
        report.source_job_type = candidate.active_outing.job_type;
        report.target_id = candidate.active_outing.target_id;
        report.target_omt = candidate.active_outing.target_omt;
        report.target_lead_id = candidate.active_outing.target_lead_id;
        report.target_lead_revision = candidate.active_outing.target_lead_revision;
        report.application_key = provisional_report_application_key( candidate );
        report.observations = make_reportable_sortie_observations(
                                  candidate.active_outing.observations,
                                  newly_returned_ids );
        report.casualty_ids = candidate.active_outing.casualty_ids;
        report.delivered_minutes = current_minutes;
        report.provisional = true;
        candidate.current_scout_report = std::move( report );
        effect.provisional_report_applied = true;

        const int carried_supply = std::max( 0, candidate.active_outing.cargo.supply_units );
        const int carried_trade = std::max( 0, candidate.active_outing.cargo.trade_value );
        const long long supply_total =
            static_cast<long long>( candidate.returned_cargo_stock.supply_units ) + carried_supply;
        const long long trade_total =
            static_cast<long long>( candidate.returned_cargo_stock.trade_value ) + carried_trade;
        candidate.returned_cargo_stock.supply_units = static_cast<int>( std::min<long long>(
                    supply_total, std::numeric_limits<int>::max() ) );
        candidate.returned_cargo_stock.trade_value = static_cast<int>( std::min<long long>(
                    trade_total, std::numeric_limits<int>::max() ) );
        candidate.active_outing.cargo = sortie_cargo();
        effect.cargo_credited = carried_supply > 0 || carried_trade > 0;
    }

    site = std::move( candidate );
    effect.changed = true;
    return effect;
}

std::optional<bandit_pursuit_handoff::return_packet> resolve_active_group_aftermath(
    const site_record &site, const std::vector<active_member_observation> &observations )
{
    if( !site.active_outing.is_active() || site.active_outing.member_ids.empty() ||
        observations.size() != site.active_outing.member_ids.size() ) {
        return std::nullopt;
    }

    bandit_pursuit_handoff::return_packet packet;
    packet.valid = true;
    packet.group_id = site.active_outing.activity_id;
    packet.source_camp_id = site.site_id;
    packet.activity_generation = site.active_outing.generation;
    packet.handoff_epoch = site.active_outing.handoff_epoch;
    packet.return_application_key = site.active_outing.return_application_key;
    packet.report_application_key = site.active_outing.report_application_key;
    packet.cargo_application_key = site.active_outing.cargo_application_key;
    packet.job_type = job_template_from_string( site.active_outing.job_type ).value_or(
                          bandit_dry_run::job_template::hold_chill );
    packet.current_target_or_mark = site.active_outing.target_id;
    packet.result = bandit_pursuit_handoff::mission_result::withdrawn;
    packet.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    packet.posture = bandit_pursuit_handoff::return_posture::escape_home;
    packet.remaining_pressure = rules_for_profile( effective_profile( site ) ).default_remaining_pressure;

    bool saw_loss = false;
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const std::string member_token = std::to_string( member_id.get_value() );
        packet.member_return_receipts.push_back( {
            member_token,
            bandit_pursuit_handoff::make_operation_component_key(
                site.active_outing.activity_id, site.active_outing.generation,
                "return", member_token )
        } );
        const auto iter = std::find_if( observations.begin(), observations.end(),
        [&member_id]( const active_member_observation &observation ) {
            return observation.npc_id == member_id;
        } );
        if( iter == observations.end() ) {
            return std::nullopt;
        }
        switch( iter->state ) {
            case active_member_observation_state::unresolved:
            case active_member_observation_state::local_contact:
            case active_member_observation_state::returning_home:
                return std::nullopt;
            case active_member_observation_state::home:
                packet.survivors_remaining++;
                break;
            case active_member_observation_state::dead:
                saw_loss = true;
                packet.anchored_identity_updates.push_back( { std::to_string( member_id.get_value() ), "dead" } );
                break;
            case active_member_observation_state::missing:
                saw_loss = true;
                packet.anchored_identity_updates.push_back( { std::to_string( member_id.get_value() ), "missing" } );
                break;
        }
    }

    if( saw_loss ) {
        packet.result = packet.survivors_remaining > 0 ?
                        bandit_pursuit_handoff::mission_result::repelled :
                        bandit_pursuit_handoff::mission_result::broken;
        packet.posture = packet.survivors_remaining > 0 ?
                         bandit_pursuit_handoff::return_posture::broken_flee :
                         bandit_pursuit_handoff::return_posture::escape_safe;
        packet.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    }

    packet.notes.push_back( "resolved live aftermath observations for active owned outing" );
    return packet;
}

bool update_member_state( site_record &site, character_id npc_id, member_state new_state,
                          const std::string &summary )
{
    member_record *member = site.find_member( npc_id );
    if( member == nullptr ) {
        return false;
    }

    const bool old_live = counts_toward_live_headcount( member->state );
    const bool new_live = counts_toward_live_headcount( new_state );
    member->state = new_state;
    if( old_live != new_live ) {
        site.living_total = std::max( 0, site.living_total + ( new_live ? 1 : -1 ) );
        if( spawn_tile_record *spawn_record = site.find_spawn_tile( member->home_spawn_tile ) ) {
            spawn_record->assigned_living_total = std::max( 0,
                    spawn_record->assigned_living_total + ( new_live ? 1 : -1 ) );
        }
        site.supply_units = std::min( site.supply_units, camp_supply_cap( site ) );
    }

    member->last_writeback_summary = summary;
    return true;
}

bool record_matching_external_outing_casualty( site_record &site,
        const std::string &expected_activity_id, const int expected_generation,
        const character_id npc_id, const member_state casualty_state,
        const int current_minutes, const std::string &summary )
{
    const active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr || outing->activity_id != expected_activity_id ||
        outing->generation != expected_generation ||
        ( casualty_state != member_state::dead && casualty_state != member_state::missing ) ||
        current_minutes < 0 || current_minutes <= outing->last_advanced_minutes ||
        std::find( outing->member_ids.begin(), outing->member_ids.end(), npc_id ) ==
        outing->member_ids.end() || outing->member_is_resolved( npc_id ) ||
        ( casualty_state == member_state::missing &&
          ( outing->missing_deadline_minutes < 0 ||
            current_minutes < outing->missing_deadline_minutes ) ) || !site.roster().valid ) {
        return false;
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    active_outing_state *candidate_outing = candidate.active_external_outing();
    if( candidate_outing == nullptr ||
        candidate_outing->activity_id != expected_activity_id ||
        candidate_outing->generation != expected_generation ||
        !update_member_state( candidate, npc_id, casualty_state, summary ) ) {
        return false;
    }
    candidate_outing->casualty_ids.push_back( npc_id );
    candidate_outing->resolved_member_ids.push_back( npc_id );
    candidate_outing->last_progress_minutes = std::max(
            candidate_outing->last_progress_minutes, current_minutes );
    candidate_outing->last_advanced_minutes = current_minutes;
    advance_camp_supply( candidate, current_minutes );
    if( candidate_outing->casualty_ids.size() == candidate_outing->member_ids.size() ) {
        candidate_outing->phase = scout_phase::lost;
        if( candidate_outing->kind == outing_kind::hostile_operation ) {
            candidate.active_hostile_operation.phase = hostile_operation_phase::lost;
            candidate.active_hostile_operation.last_transition_reason = summary.substr(
                    0, max_camp_decision_reason_length );
        }
    }
    if( !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}

bool record_active_outing_casualty( site_record &site,
                                    const simulation_advance_cursor &expected_cursor,
                                    const character_id npc_id,
                                    const member_state casualty_state,
                                    const int current_minutes,
                                    const std::string &summary )
{
    if( !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        expected_cursor.owner != simulation_owner::local ||
        current_minutes <= site.active_outing.last_advanced_minutes ) {
        return false;
    }
    const scout_phase previous_phase = site.active_outing.phase;
    if( !record_matching_external_outing_casualty(
            site, expected_cursor.activity_id, expected_cursor.generation,
            npc_id, casualty_state, current_minutes, summary ) ) {
        return false;
    }
    if( site.active_outing.phase != previous_phase ) {
        record_scout_phase_transition_event(
            site.active_outing, previous_phase, site.active_outing.phase,
            "all scout members resolved as casualties", current_minutes );
    }
    return true;
}
} // namespace bandit_live_world

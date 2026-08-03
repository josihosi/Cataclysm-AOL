#include "bandit_live_world.h"

#include <algorithm>
#include <array>
#include <cctype>
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
#include "json.h"

namespace
{
using bandit_live_world::anchor_source_kind;
using bandit_live_world::camp_decision_state;
using bandit_live_world::camp_lead_kind;
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

constexpr std::size_t max_active_outing_members = 16;
constexpr std::size_t max_active_outing_route_steps = 256;
constexpr std::size_t max_hostile_operation_members = 6;
constexpr std::size_t max_active_outing_observations = 16;
constexpr std::size_t max_sortie_observation_batch = 64;
constexpr std::size_t max_active_outing_casualties = 16;
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

bool structural_route_is_canonical_for_lead( const std::vector<tripoint_abs_omt> &route,
        const tripoint_abs_omt &anchor, const camp_map_lead &lead )
{
    const std::optional<int> frontier_sector = frontier_sector_from_lead( lead );
    if( frontier_sector ) {
        return route.size() >= 2 && lead.omt == route[route.size() - 2] &&
               frontier_route_is_canonical( route, anchor, *frontier_sector );
    }
    return ( lead.kind == camp_lead_kind::structural_bounty ||
             lead.kind == camp_lead_kind::terrain_opportunity ) &&
           structural_route_is_canonical( route, anchor, lead.omt );
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

bool sortie_observation_is_better_retention(
    const bandit_live_world::sortie_observation &lhs,
    const bandit_live_world::sortie_observation &rhs )
{
    const int lhs_rank = sortie_observation_retention_rank( lhs );
    const int rhs_rank = sortie_observation_retention_rank( rhs );
    return std::tie( lhs_rank, lhs.observed_minutes, lhs.confidence, lhs.state_key,
                     lhs.kind, lhs.critical, lhs.summary ) >
           std::tie( rhs_rank, rhs.observed_minutes, rhs.confidence, rhs.state_key,
                     rhs.kind, rhs.critical, rhs.summary );
}

bool sortie_observation_is_better_for_cap(
    const bandit_live_world::sortie_observation &lhs,
    const bandit_live_world::sortie_observation &rhs )
{
    const int lhs_rank = sortie_observation_retention_rank( lhs );
    const int rhs_rank = sortie_observation_retention_rank( rhs );
    return std::tie( lhs_rank, lhs.observed_minutes, lhs.confidence,
                     lhs.fact_key, lhs.state_key, lhs.kind, lhs.critical, lhs.summary ) >
           std::tie( rhs_rank, rhs.observed_minutes, rhs.confidence,
                     rhs.fact_key, rhs.state_key, rhs.kind, rhs.critical, rhs.summary );
}

void normalize_sortie_observation( bandit_live_world::sortie_observation &observation )
{
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
           lhs.state_key == rhs.state_key;
}

std::vector<bandit_live_world::sortie_observation> make_bounded_sortie_observations(
            const std::vector<bandit_live_world::sortie_observation> &observations )
{
    std::vector<bandit_live_world::sortie_observation> deduplicated;
    deduplicated.reserve( std::min( observations.size(), max_active_outing_observations ) );
    for( bandit_live_world::sortie_observation observation : observations ) {
        normalize_sortie_observation( observation );
        if( observation.fact_key.empty() ) {
            continue;
        }
        const auto existing = std::find_if( deduplicated.begin(), deduplicated.end(),
        [&observation]( const bandit_live_world::sortie_observation & retained ) {
            return retained.fact_key == observation.fact_key;
        } );
        if( existing == deduplicated.end() ) {
            deduplicated.push_back( std::move( observation ) );
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
                         lhs.confidence, lhs.critical ) <
               std::tie( rhs.observed_minutes, rhs.fact_key, rhs.state_key, rhs.kind, rhs.summary,
                         rhs.confidence, rhs.critical );
    } );
    return deduplicated;
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
    return lhs.lead_id == rhs.lead_id && lhs.kind == rhs.kind && lhs.status == rhs.status &&
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
        outing.target_lead_revision = new_revision;
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

bool upsert_camp_map_lead( bandit_live_world::site_record &site, camp_map_lead lead )
{
    bound_camp_map_lead_strings( lead );
    if( lead.lead_id.empty() ) {
        return false;
    }
    const std::string lead_id = lead.lead_id;

    bandit_live_world::site_record candidate = site;
    if( camp_map_lead *existing = candidate.intelligence_map.find_lead( lead.lead_id ) ) {
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
    upsert_camp_map_lead( site, lead );
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
    upsert_camp_map_lead( site, lead );
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
    if( ( outing.owner == simulation_owner::local && !snapshot.is_active() ) ||
        snapshot.activity_id != outing.activity_id ||
        snapshot.activity_generation != outing.generation ||
        snapshot.handoff_epoch != outing.handoff_epoch ||
        snapshot.committed_minutes > outing.last_advanced_minutes ||
        snapshot.waypoint_index < 0 ||
        snapshot.waypoint_index >= static_cast<int>( outing.shared_route.size() ) ||
        snapshot.route_position != outing.shared_route[static_cast<std::size_t>(
                                       snapshot.waypoint_index )] ) {
        return false;
    }
    const tripoint_abs_omt expected_approach = snapshot.waypoint_index == 0 ?
            snapshot.route_position :
            outing.shared_route[static_cast<std::size_t>( snapshot.waypoint_index - 1 )];
    const tripoint_abs_omt expected_egress =
        snapshot.waypoint_index + 1 < static_cast<int>( outing.shared_route.size() ) ?
        outing.shared_route[static_cast<std::size_t>( snapshot.waypoint_index + 1 )] :
        snapshot.route_position;
    if( snapshot.approach_from != expected_approach || snapshot.egress_omt != expected_egress ) {
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
    std::vector<tripoint_abs_ms> exit_positions;
    snapshot_member_ids.reserve( snapshot.members.size() );
    entry_positions.reserve( snapshot.members.size() );
    staging_positions.reserve( snapshot.members.size() );
    exit_positions.reserve( snapshot.members.size() );
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
            project_to<coords::omt>( member.entry_position ) != snapshot.route_position ||
            std::find( entry_positions.begin(), entry_positions.end(), member.entry_position ) !=
            entry_positions.end() ||
            project_to<coords::omt>( member.staging_position ) != snapshot.route_position ||
            std::find( staging_positions.begin(), staging_positions.end(),
                       member.staging_position ) != staging_positions.end() ||
            project_to<coords::omt>( member.exit_position ) != snapshot.route_position ||
            std::find( exit_positions.begin(), exit_positions.end(), member.exit_position ) !=
            exit_positions.end() ||
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
        exit_positions.push_back( member.exit_position );
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
           local_handoff_snapshot_matches_outing( outing );
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
                                  ( ( schema_version == 6 || schema_version == 7 ) &&
                                    kind == "structural_sortie" );
    const bool complete_structural_route = schema_version < 6 ||
            ( owner_json.has_member( "shared_route" ) &&
              owner_json.has_member( "waypoint_index" ) &&
              owner_json.has_member( "expected_return_minutes" ) &&
              owner_json.has_member( "missing_deadline_minutes" ) );
    const bool complete_local_handoff = schema_version != 7 ||
                                        owner_json.has_member( "local_handoff" );
    return supported_schema && complete_structural_route && complete_local_handoff &&
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
           outing.last_advanced_minutes == cursor.last_advanced_minutes;
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

simulation_owner_transition_result advance_external_simulation(
    site_record &site, const std::string &expected_activity_id,
    const int expected_generation, const simulation_owner expected_owner,
    const int expected_handoff_epoch, const int expected_last_advanced_minutes,
    const int current_minutes )
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
    if( current_minutes == current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }

    site_record candidate = site;
    active_outing_state *next = candidate.active_external_outing();
    if( next == nullptr ) {
        return simulation_owner_transition_result::rejected;
    }
    next->last_advanced_minutes = current_minutes;
    if( next->kind == outing_kind::structural_sortie && next->schema_version >= 7 &&
        next->owner == simulation_owner::abstract && next->local_handoff.is_abstract_resume() ) {
        next->local_handoff.clear();
    }
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
    if( surviving_member_ids.size() != 2 || member_reads.size() != 2 ) {
        plan.notes.push_back( "local handoff blocked: complete surviving pair is unavailable" );
        return plan;
    }

    local_handoff_snapshot snapshot;
    snapshot.activity_id = outing.activity_id;
    snapshot.activity_generation = outing.generation;
    snapshot.handoff_epoch = outing.handoff_epoch + 1;
    snapshot.waypoint_index = outing.waypoint_index;
    snapshot.phase = outing.phase;
    snapshot.route_position = outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
    snapshot.approach_from = outing.waypoint_index == 0 ? snapshot.route_position :
                             outing.shared_route[static_cast<std::size_t>( outing.waypoint_index - 1 )];
    snapshot.egress_omt = outing.waypoint_index + 1 < static_cast<int>( outing.shared_route.size() ) ?
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
    for( const character_id &member_id : surviving_member_ids ) {
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
    if( rl_dist( entry_positions[0], entry_positions[1] ) > local_pair_cohesion_radius_ms ) {
        plan.notes.push_back( "local handoff blocked: pair entry is outside cohesion radius" );
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
    next.schema_version = 7;
    next.owner = simulation_owner::local;
    next.handoff_epoch = plan.snapshot.handoff_epoch;
    next.last_advanced_minutes = plan.snapshot.committed_minutes;
    next.last_progress_minutes = std::max( next.last_progress_minutes,
                                          plan.snapshot.committed_minutes );
    next.local_handoff = plan.snapshot;
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
    snapshot.schema_version = 3;
    snapshot.handoff_epoch = outing.handoff_epoch + 1;
    snapshot.waypoint_index = outing.waypoint_index;
    snapshot.phase = outing.phase;
    snapshot.route_position = outing.shared_route[static_cast<std::size_t>( outing.waypoint_index )];
    snapshot.approach_from = outing.waypoint_index == 0 ? snapshot.route_position :
                             outing.shared_route[static_cast<std::size_t>( outing.waypoint_index - 1 )];
    snapshot.egress_omt = outing.waypoint_index + 1 < static_cast<int>( outing.shared_route.size() ) ?
                          outing.shared_route[static_cast<std::size_t>( outing.waypoint_index + 1 )] :
                          snapshot.route_position;
    snapshot.cargo = cargo;
    snapshot.casualty_ids = outing.casualty_ids;
    snapshot.committed_minutes = current_minutes;

    std::vector<character_id> read_member_ids;
    std::vector<tripoint_abs_ms> exit_positions;
    std::vector<tripoint_abs_ms> surviving_exit_positions;
    for( local_handoff_member_snapshot &member_snapshot : snapshot.members ) {
        const auto read_iter = std::find_if( member_reads.begin(), member_reads.end(),
        [&member_snapshot]( const local_dematerialization_member_read & read ) {
            return read.npc_id == member_snapshot.npc_id;
        } );
        if( read_iter == member_reads.end() || !read_iter->readable ||
            std::find( read_member_ids.begin(), read_member_ids.end(), read_iter->npc_id ) !=
            read_member_ids.end() ||
            ( read_iter->dead ? read_iter->hp_percent != 0 :
              read_iter->hp_percent <= 0 || read_iter->hp_percent > 100 ) ||
            project_to<coords::omt>( read_iter->current_position ) != snapshot.route_position ||
            std::find( exit_positions.begin(), exit_positions.end(), read_iter->current_position ) !=
            exit_positions.end() ) {
            plan.notes.push_back( "local dematerialization blocked: member read is partial or contradictory" );
            return plan;
        }
        const bool casualty_was_recorded = std::find( snapshot.casualty_ids.begin(),
                                           snapshot.casualty_ids.end(), read_iter->npc_id ) !=
                                           snapshot.casualty_ids.end();
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
            if( snapshot.cohesion_assembled && !snapshot.cohesion_abort_return &&
                rl_dist( read_iter->current_position, member_snapshot.staging_position ) > 1 ) {
                plan.notes.push_back(
                    "local dematerialization blocked: survivor left assembled staging" );
                return plan;
            }
            surviving_exit_positions.push_back( read_iter->current_position );
        }
        read_member_ids.push_back( read_iter->npc_id );
        exit_positions.push_back( read_iter->current_position );
    }
    if( snapshot.cohesion_assembled && surviving_exit_positions.size() == 2 &&
        rl_dist( surviving_exit_positions[0], surviving_exit_positions[1] ) >
        local_pair_cohesion_radius_ms ) {
        plan.notes.push_back( "local dematerialization blocked: assembled pair separated" );
        return plan;
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

bool record_local_pair_member_death( site_record &site,
                                     const simulation_advance_cursor &expected_cursor,
                                     const character_id member_id,
                                     const tripoint_abs_ms &death_position,
                                     const int current_minutes )
{
    const active_outing_state &outing = site.active_outing;
    if( !outing.is_active() || outing.kind != outing_kind::structural_sortie ||
        outing.schema_version < 7 || outing.owner != simulation_owner::local ||
        !simulation_cursor_matches( outing, expected_cursor ) ||
        !outing.local_handoff.is_active() || current_minutes < 0 ||
        current_minutes < outing.last_advanced_minutes ||
        project_to<coords::omt>( death_position ) != outing.local_handoff.route_position ) {
        return false;
    }
    const auto snapshot_iter = std::find_if( outing.local_handoff.members.begin(),
    outing.local_handoff.members.end(), [&member_id]( const local_handoff_member_snapshot & member ) {
        return member.npc_id == member_id;
    } );
    if( snapshot_iter == outing.local_handoff.members.end() || snapshot_iter->dead ||
        outing.member_is_resolved( member_id ) ||
        std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
        outing.casualty_ids.end() ) {
        return false;
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    if( !update_member_state( candidate, member_id, member_state::dead,
            "local handoff member died under physical simulation" ) ) {
        return false;
    }
    active_outing_state &next = candidate.active_outing;
    next.casualty_ids.push_back( member_id );
    next.resolved_member_ids.push_back( member_id );
    next.last_progress_minutes = std::max( next.last_progress_minutes, current_minutes );
    next.last_advanced_minutes = current_minutes;
    next.local_handoff.cargo = next.cargo;
    next.local_handoff.casualty_ids = next.casualty_ids;
    next.local_handoff.committed_minutes = current_minutes;
    local_handoff_member_snapshot *death_snapshot = nullptr;
    for( local_handoff_member_snapshot &member : next.local_handoff.members ) {
        if( member.npc_id == member_id ) {
            death_snapshot = &member;
            break;
        }
    }
    if( death_snapshot == nullptr ) {
        return false;
    }
    death_snapshot->exit_position = death_position;
    death_snapshot->hp_percent = 0;
    death_snapshot->dead = true;
    if( next.leader_id == member_id ) {
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
        plan.reroute_needed || plan.leader_id != outing.leader_id ) {
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
    } else if( next.local_handoff.cohesion_assembled ) {
        next.last_progress_minutes = std::max( next.last_progress_minutes,
                                              next.local_handoff.committed_minutes );
    }
    next.last_advanced_minutes = next.local_handoff.committed_minutes;
    if( local_handoff_snapshots_equal( outing.local_handoff, next.local_handoff ) &&
        outing.leader_id == next.leader_id && outing.phase == next.phase ) {
        return false;
    }
    if( !simulation_owner_state_is_consistent( next ) || !candidate.roster().valid ) {
        return false;
    }
    site = std::move( candidate );
    return true;
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
        if( site.active_outing.is_active() ||
            decision.report_policy != report_policy_for_profile( effective_profile( site ) ) ||
            !report_matches_camp_decision( report, decision ) ) {
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

    resolve_camp_lead_reference( site.active_outing, site.intelligence_map.leads );
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

void sortie_observation::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "fact_key", fact_key.substr( 0, max_sortie_fact_key_length ) );
    json.member( "summary", summary.substr( 0, max_sortie_summary_length ) );
    json.member( "confidence", std::clamp( confidence, 0, 100 ) );
    json.member( "observed_minutes", std::max( -1, observed_minutes ) );
    json.member( "critical", critical );
    if( kind != sortie_observation_kind::routine ) {
        json.member( "kind", to_string( kind ) );
    }
    if( !state_key.empty() ) {
        json.member( "state_key", state_key.substr( 0, max_sortie_state_key_length ) );
    }
    json.end_object();
}

void sortie_observation::deserialize( const JsonObject &jo )
{
    sortie_observation candidate;
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
    if( kind_was_present && !parsed_kind ) {
        candidate.critical = true;
    }
    jo.read( "state_key", candidate.state_key );
    normalize_sortie_observation( candidate );
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
    json.member( "schema_version", 4 );
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
    jo.read( "schema_version", candidate.schema_version );
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
    candidate.schema_version = 4;
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
    if( loaded_schema_version >= 6 && loaded_schema_version <= 7 &&
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
        jo.throw_error( "schema-v7 structural outing is missing local handoff state" );
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
                     loaded_schema_version != 7 ) ||
                   ( loaded_schema_version >= 6 &&
                     candidate.kind != outing_kind::structural_sortie ) ||
                   candidate.return_application_key != expected_return_key ||
                   candidate.report_application_key != expected_report_key ||
                   candidate.cargo_application_key != expected_cargo_key ) {
            jo.throw_error( "active outing has non-canonical component application keys" );
        }
        candidate.schema_version = loaded_schema_version >= 6 ? loaded_schema_version : 5;
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
    const int structural_target_waypoint = active_outing.shared_route.size() >= 2 ?
            static_cast<int>( active_outing.shared_route.size() ) - 2 : -1;
    const bool structural_phase_is_consistent =
        active_outing.kind != outing_kind::structural_sortie ||
        ( ( active_outing.phase == scout_phase::outbound &&
            active_outing.local_contact_minutes == -1 &&
            active_outing.waypoint_index == 0 ) ||
          ( active_outing.phase == scout_phase::observing &&
            active_outing.local_contact_minutes >= active_outing.started_minutes &&
            active_outing.waypoint_index == 1 ) ||
          ( active_outing.phase == scout_phase::returning_home &&
            ( active_outing.waypoint_index == 0 || active_outing.waypoint_index == 1 ||
              active_outing.waypoint_index == structural_target_waypoint ) ) );
    const bool structural_identity_is_consistent =
        active_outing.kind != outing_kind::structural_sortie ||
        ( active_outing.activity_id == site_id + "#structural" &&
          ( active_outing.job_type == "scout" || active_outing.job_type == "scavenge" ) &&
          active_outing.member_ids.size() == 2 &&
          ( active_outing.schema_version == 6 || active_outing.schema_version == 7 ) &&
          active_outing.started_minutes >= 0 &&
          active_outing.target_id == active_outing.target_lead_id &&
          active_outing.target_lead_revision > 0 && structural_lead != nullptr &&
          ( ( structural_lead->kind == camp_lead_kind::structural_bounty &&
              active_outing.job_type == ( structural_lead->target_id == "forest" ?
                                          "scavenge" : "scout" ) ) ||
            ( structural_lead->kind == camp_lead_kind::terrain_opportunity &&
              active_outing.job_type == "scout" ) ||
            ( structural_frontier_sector && active_outing.job_type == "scout" ) ) &&
          structural_lead->omt == active_outing.target_omt &&
          structural_route_is_canonical_for_lead( active_outing.shared_route, anchor,
                  *structural_lead ) &&
          active_outing.expected_return_minutes == structural_expected_return_minutes(
              active_outing.started_minutes, anchor, active_outing.target_omt ) &&
          active_outing.missing_deadline_minutes == minutes_after_saturated(
              active_outing.expected_return_minutes, scout_missing_grace_minutes ) &&
          structural_phase_is_consistent );
    const bool active_outing_schema_is_consistent =
        active_outing.kind == outing_kind::structural_sortie ?
        ( active_outing.schema_version == 6 || active_outing.schema_version == 7 ) :
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
        if( lead.kind == camp_lead_kind::frontier_probe ||
            lead.kind == camp_lead_kind::terrain_opportunity ) {
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
    const int signal = 0;
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

namespace
{
structural_outing_plan plan_frontier_outing_impl( const site_record &site,
        const int now_minutes, const bool solve_route )
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
    if( !routine_policy.eligible || !pair_selection.eligible ) {
        plan.notes.push_back( "frontier outing blocked: exact routine pair is ineligible" );
        return plan;
    }
    plan.job = bandit_dry_run::job_template::scout;
    plan.member_ids = pair_selection.member_ids;
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
        const camp_map_lead &lead, const int now_minutes, const bool solve_route )
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
    if( !structural_bounty && !terrain_opportunity ) {
        plan.notes.push_back( "structural outing blocked: lead is not a routine terrain candidate" );
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
    if( !pair_selection.eligible ) {
        plan.notes.push_back( "structural outing blocked: " + pair_selection.rejection_reason );
        return plan;
    }
    plan.member_ids = pair_selection.member_ids;
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
    const site_record &site, const int now_minutes )
{
    std::vector<structural_outing_plan> candidates;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        structural_outing_plan candidate = plan_structural_bounty_outing_impl(
                site, lead, now_minutes, false );
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

    camp_map_lead scoring_lead;
    const camp_map_lead *lead = site.intelligence_map.find_lead( plan.lead_id );
    if( lead != nullptr ) {
        scoring_lead = *lead;
    } else if( plan.frontier_sector >= 0 ) {
        scoring_lead.lead_id = plan.lead_id;
        scoring_lead.revision = plan.lead_revision;
        scoring_lead.kind = camp_lead_kind::frontier_probe;
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
    const bool route_is_canonical = frontier_plan ?
                                    frontier_route_is_canonical( plan.shared_route, site.anchor,
                                            plan.frontier_sector ) :
                                    structural_route_is_canonical( plan.shared_route, site.anchor,
                                            plan.target_omt );
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
        plan.target_omt != plan.shared_route[plan.shared_route.size() - 2] ||
        plan.expected_stalking_minutes != minutes_after_saturated(
            now_minutes, structural_stalking_delay_minutes( site.anchor, plan.target_omt ) ) ||
        plan.expected_arrival_minutes != minutes_after_saturated(
            now_minutes, structural_arrival_delay_minutes( site.anchor, plan.target_omt ) ) ||
        plan.expected_return_minutes != structural_expected_return_minutes(
            now_minutes, site.anchor, plan.target_omt ) ) {
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
                plan.max_route_segment_risk, "" };
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
              lead->kind != camp_lead_kind::terrain_opportunity ) ||
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
                plan.max_route_segment_risk, "" };
        if( expected.valid && !apply_structural_route_read(
                site, now_minutes, persisted_route, expected ) ) {
            expected.valid = false;
        }
        if( !expected.valid || plan.job != expected.job ||
            plan.member_ids != expected.member_ids || plan.shared_route != expected.shared_route ||
            plan.cheap_score != expected.cheap_score || plan.final_score != expected.final_score ||
            plan.static_risk != expected.static_risk || plan.terrain_fit != expected.terrain_fit ||
            plan.final_route_quality != expected.final_route_quality ||
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
    candidate.active_outing.schema_version = 7;
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
    claimed_outing.waypoint_index = static_cast<int>( claimed_outing.shared_route.size() ) - 2;
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
} // namespace

structural_outing_result advance_structural_bounty_outings( world_state &state, const int now_minutes,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup )
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
              site.active_outing.schema_version != 7 ) ||
            site.active_outing.activity_id != site.site_id + "#structural" ||
            site.active_outing.target_id.empty() ||
            site.active_outing.target_id != site.active_outing.target_lead_id ||
            site.active_outing.member_ids.size() != 2 ||
            site.active_outing.started_minutes < 0 ||
            site.active_outing.expected_return_minutes != structural_expected_return_minutes(
                site.active_outing.started_minutes, site.anchor,
                site.active_outing.target_omt ) ) {
            continue;
        }
        const camp_map_lead *current_lead = site.intelligence_map.find_lead(
                                                site.active_outing.target_id );
        const std::optional<int> current_frontier_sector = current_lead != nullptr ?
                frontier_sector_from_lead( *current_lead ) : std::nullopt;
        const bool current_lead_kind_is_supported = current_lead != nullptr &&
                ( current_lead->kind == camp_lead_kind::structural_bounty ||
                  current_lead->kind == camp_lead_kind::terrain_opportunity ||
                  current_frontier_sector.has_value() );
        const bool current_job_is_consistent = current_lead != nullptr &&
                ( ( current_lead->kind == camp_lead_kind::structural_bounty &&
                    site.active_outing.job_type == ( current_lead->target_id == "forest" ?
                                                     "scavenge" : "scout" ) ) ||
                  ( current_lead->kind == camp_lead_kind::terrain_opportunity &&
                    site.active_outing.job_type == "scout" ) ||
                  ( current_frontier_sector && site.active_outing.job_type == "scout" ) );
        if( current_lead_kind_is_supported &&
            ( !current_job_is_consistent ||
              current_lead->omt != site.active_outing.target_omt ||
              !structural_route_is_canonical_for_lead( site.active_outing.shared_route,
                      site.anchor, *current_lead ) ||
              ( current_frontier_sector &&
                !frontier_memory_is_valid( site.intelligence_map ) ) ) ) {
            continue;
        }
        const std::string expected_activity_id = site.active_outing.activity_id;
        const int expected_generation = site.active_outing.generation;
        result.active_outings_considered++;

        site_record candidate = site;
        const simulation_owner_transition_result advance = advance_external_simulation(
                    candidate, candidate.active_outing.activity_id,
                    candidate.active_outing.generation, simulation_owner::abstract,
                    candidate.active_outing.handoff_epoch,
                    candidate.active_outing.last_advanced_minutes, now_minutes );
        if( advance != simulation_owner_transition_result::applied ) {
            continue;
        }
        camp_map_lead *lead = candidate.intelligence_map.find_lead(
                                  candidate.active_outing.target_id );
        const std::optional<int> frontier_sector = lead != nullptr ?
                frontier_sector_from_lead( *lead ) : std::nullopt;
        if( lead == nullptr ||
            ( lead->kind != camp_lead_kind::structural_bounty &&
              lead->kind != camp_lead_kind::terrain_opportunity && !frontier_sector ) ) {
            if( release_structural_outing_reservation(
                    candidate, expected_activity_id, expected_generation,
                    "structural outing cleared missing structural lead" ) ) {
                site = std::move( candidate );
                result.notes.push_back( "structural outing cleared: active target lead was missing" );
            }
            continue;
        }
        if( lead->revision >= std::numeric_limits<int>::max() ) {
            if( release_structural_outing_reservation(
                    candidate, expected_activity_id, expected_generation,
                    "structural outing cleared immutable terminal lead revision" ) ) {
                site = std::move( candidate );
                result.notes.push_back(
                    "structural outing cleared: target lead revision cannot advance safely" );
            }
            continue;
        }

        active_outing_state &outing = candidate.active_outing;
        if( outing.phase == scout_phase::returning_home ) {
            if( now_minutes >= outing.expected_return_minutes ) {
                const std::string lead_id = lead->lead_id;
                const bool completed_frontier_route = frontier_sector &&
                        outing.waypoint_index == static_cast<int>( outing.shared_route.size() ) - 2;
                outing.waypoint_index = static_cast<int>( outing.shared_route.size() ) - 1;
                outing.last_progress_minutes = now_minutes;
                if( completed_frontier_route ) {
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
                const bool useful_return = completed_frontier_route ||
                                           candidate.active_outing.cargo.supply_units > 0 ||
                                           ( !frontier_sector &&
                                             ( lead->status == camp_lead_status::harvested ||
                                               ( lead->kind == camp_lead_kind::terrain_opportunity &&
                                                 lead->status == camp_lead_status::stale ) ) );
                const bool danger_withdrawal = lead->status == camp_lead_status::dangerous;
                candidate.last_routine_resolved_minutes = now_minutes;
                candidate.next_routine_dispatch_eligible_minutes = minutes_after_saturated(
                            now_minutes, routine_cooldown_delay_minutes(
                                candidate.site_id,
                                useful_return || danger_withdrawal ? 24 * 60 : 18 * 60 ) );
                if( useful_return ) {
                    candidate.routine_no_candidate_streak = 0;
                }
                if( !credit_structural_return_cargo( candidate, now_minutes ) ) {
                    continue;
                }
                const std::optional<int> returned = release_structural_outing_reservation(
                        candidate, expected_activity_id, expected_generation,
                        "structural outing completed its shared route home" );
                if( returned ) {
                    site = std::move( candidate );
                    result.members_returned += *returned;
                    result.notes.push_back( "structural outing returned home lead=" + lead_id );
                }
            } else {
                site = std::move( candidate );
            }
            continue;
        }

        const int elapsed = now_minutes - outing.started_minutes;
        if( outing.phase == scout_phase::outbound && outing.local_contact_minutes < 0 &&
            elapsed >= structural_outing_stalking_delay_minutes( candidate, *lead ) ) {
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
            elapsed >= structural_outing_arrival_delay_minutes( candidate, *lead ) ) {
            const std::string lead_id = lead->lead_id;
            if( frontier_sector ) {
                lead->status = camp_lead_status::scout_confirmed;
                lead->last_scouted_minutes = now_minutes;
                lead->last_checked_minutes = now_minutes;
                lead->last_outcome = "frontier_outer_sample_complete";
            } else if( lead->kind == camp_lead_kind::terrain_opportunity ) {
                lead->status = camp_lead_status::stale;
                lead->last_scouted_minutes = now_minutes;
                lead->last_checked_minutes = now_minutes;
                lead->last_outcome = "terrain_opportunity_physically_checked";
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
            outing.waypoint_index = static_cast<int>( outing.shared_route.size() ) - 2;
            outing.phase = scout_phase::returning_home;
            outing.last_progress_minutes = now_minutes;
            site = std::move( candidate );
            result.arrivals_processed++;
            if( frontier_sector ) {
                result.notes.push_back(
                    "frontier outing sampled outer ring and began return lead=" + lead_id );
            } else if( lead->kind == camp_lead_kind::terrain_opportunity ) {
                result.notes.push_back(
                    "terrain opportunity was physically checked and began return lead=" + lead_id );
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
                const structural_outing_plan & )> &route_lookup )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_maintenance );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::structural_maintenance_updates );
    structural_bounty_maintenance_result result;
    result.scheduler_consider_cap = routine_scheduler_consider_cap;
    result.full_route_solve_cap = routine_scheduler_full_route_solve_cap;
    state.schema_version = 6;
    advance_world_camp_supplies( state, now_minutes );
    result.dispatch_cap = std::min( routine_scheduler_start_cap, std::max( 0, dispatch_cap ) );
    result.outing = advance_structural_bounty_outings( state, now_minutes, threat_lookup );

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
    struct routine_dispatch_candidate {
        std::size_t site_index;
        std::vector<structural_outing_plan> cheap_plans;
        routine_dispatch_evaluation evaluation;
        int overdue_bonus;
        bool frontier_due;
    };
    std::vector<routine_dispatch_candidate> dispatch_candidates;
    dispatch_candidates.reserve( static_cast<std::size_t>( sites_to_consider ) );
    for( int offset = 0; offset < sites_to_consider; ++offset ) {
        const int eligible_index = ( result.scheduler_cursor_before + offset ) %
                                   static_cast<int>( routine_site_indices.size() );
        const std::size_t site_index = routine_site_indices[static_cast<std::size_t>(
                                           eligible_index )];
        site_record &site = state.sites[site_index];
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
            !routine_policy.eligible || !pair_selection.eligible ) {
            continue;
        }
        const bool has_structural_candidate_source = std::any_of(
                    site.intelligence_map.leads.begin(), site.intelligence_map.leads.end(),
        []( const camp_map_lead & lead ) {
            return lead.kind == camp_lead_kind::structural_bounty ||
                   lead.kind == camp_lead_kind::terrain_opportunity;
        } );
        if( !frontier_due && !has_structural_candidate_source ) {
            continue;
        }
        std::vector<structural_outing_plan> cheap_plans =
            cheap_structural_outing_candidates( site, now_minutes );
        if( frontier_due ) {
            structural_outing_plan frontier = plan_frontier_outing_impl( site, now_minutes, false );
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

        std::sort( cheap_plans.begin(), cheap_plans.end(), [&site, frontier_due](
        const structural_outing_plan & lhs, const structural_outing_plan & rhs ) {
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
                                         overdue_bonus, frontier_due } );
    }

    std::sort( dispatch_candidates.begin(), dispatch_candidates.end(), [&state](
    const routine_dispatch_candidate & lhs, const routine_dispatch_candidate & rhs ) {
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
                routed = plan_frontier_outing_impl( site, now_minutes, true );
            } else {
                const camp_map_lead *lead = site.intelligence_map.find_lead( cheap.lead_id );
                if( lead != nullptr ) {
                    routed = plan_structural_bounty_outing_impl( site, *lead, now_minutes, true );
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
        site_record &site = state.sites[contender.site_index];
        for( const std::string &note : contender.plan.notes ) {
            result.notes.push_back( note );
        }
        if( apply_structural_bounty_outing_plan( site, contender.plan, now_minutes ) ) {
            result.dispatches_applied++;
            result.notes.push_back( "structural maintenance dispatched site=" + site.site_id +
                                    " lead=" + contender.plan.lead_id );
        } else {
            result.dispatches_blocked++;
            result.notes.push_back( "structural maintenance dispatch apply blocked site=" + site.site_id +
                                    " lead=" + contender.plan.lead_id );
        }
    }

    state.routine_scheduler_cursor = ( result.scheduler_cursor_before + sites_to_consider ) %
                                     static_cast<int>( routine_site_indices.size() );
    result.scheduler_cursor_after = state.routine_scheduler_cursor;

    return result;
}

std::string render_structural_bounty_maintenance_report(
    const structural_bounty_maintenance_result &result )
{
    std::ostringstream out;
    out << "bandit_live_world structural maintenance:"
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

sortie_observation_effect record_active_sortie_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<sortie_observation> &observations,
        const int current_minutes )
{
    sortie_observation_effect effect;
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.kind != outing_kind::scout_sortie ||
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
        normalize_sortie_observation( observation );
        if( observation.fact_key.empty() || observation.observed_minutes > current_minutes ) {
            return effect;
        }
        if( observation.observed_minutes < 0 ) {
            observation.observed_minutes = current_minutes;
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
            return prior.fact_key == observation.fact_key;
        } );
        if( previous_iter == previous.end() ) {
            effect.inserted++;
        } else if( !sortie_observations_equal( *previous_iter, observation ) ) {
            effect.replaced++;
        }

        const auto input_iter = std::find_if( normalized_inputs.begin(), normalized_inputs.end(),
        [&observation]( const sortie_observation & input ) {
            return input.fact_key == observation.fact_key && input.kind == observation.kind &&
                   input.state_key == observation.state_key;
        } );
        if( input_iter != normalized_inputs.end() &&
            sortie_observation_counts_as_progress( observation.kind ) &&
            ( previous_iter == previous.end() || previous_iter->kind != observation.kind ||
              previous_iter->state_key != observation.state_key ) ) {
            effect.progress = true;
        }
    }
    for( const sortie_observation &observation : previous ) {
        if( std::none_of( retained.begin(), retained.end(),
        [&observation]( const sortie_observation & current ) {
            return current.fact_key == observation.fact_key;
        } ) ) {
            effect.evicted++;
        }
    }

    site_record candidate = site;
    candidate.active_outing.observations = retained;
    candidate.active_outing.last_advanced_minutes = current_minutes;
    if( effect.progress ) {
        candidate.active_outing.last_progress_minutes = current_minutes;
    }
    site = std::move( candidate );
    effect.valid = true;
    effect.changed = true;
    return effect;
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

bool record_live_signal_mark( site_record &site, const live_signal_mark &mark )
{
    if( site.retired_empty_site || mark.mark_id.empty() || mark.range_cap_omt <= 0 ) {
        return false;
    }
    const std::string mark_id = mark.mark_id.substr( 0, max_live_signal_mark_length );

    bool changed = site.remembered_target_or_mark != mark_id;
    site.remembered_target_or_mark = mark_id;
    const int old_bounty = site.remembered_bounty_estimate;
    const int old_threat = site.remembered_threat_estimate;
    site.remembered_bounty_estimate = std::max( site.remembered_bounty_estimate, mark.bounty_add );
    site.remembered_threat_estimate = std::max( site.remembered_threat_estimate, mark.threat_add );
    changed |= site.remembered_bounty_estimate != old_bounty;
    changed |= site.remembered_threat_estimate != old_threat;

    if( std::find( site.known_recent_marks.begin(), site.known_recent_marks.end(), mark_id ) ==
        site.known_recent_marks.end() ) {
        if( site.known_recent_marks.size() >= max_live_signal_marks ) {
            site.known_recent_marks.erase( site.known_recent_marks.begin() );
        }
        site.known_recent_marks.push_back( mark_id );
        changed = true;
    }

    camp_map_lead lead;
    lead.kind = signal_kind_to_camp_lead_kind( mark.kind );
    lead.status = camp_lead_status::suspected;
    lead.target_id = mark_id;
    lead.omt = mark.source_omt;
    lead.radius_omt = mark.range_cap_omt;
    lead.source_key = mark_id;
    lead.source_summary = "live " + mark.kind + " signal mark";
    if( mark.kind == "smoke" ) {
        lead.source_summary += " (obscured/uncertain lead)";
    }
    lead.last_seen_minutes = -1;
    lead.bounty = std::max( 0, mark.bounty_add );
    lead.threat = std::max( 0, mark.threat_add );
    lead.confidence = std::max( 1, mark.confidence );
    lead.threat_confirmed = lead.threat > 0;
    lead.generated_by_this_camp_routine = true;
    lead.last_outcome = "live_signal";
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    const camp_map_lead *old_lead = site.intelligence_map.find_lead( lead.lead_id );
    changed |= old_lead == nullptr || old_lead->bounty != lead.bounty ||
               old_lead->threat != lead.threat || old_lead->confidence != lead.confidence ||
               old_lead->radius_omt != lead.radius_omt ||
               old_lead->source_summary != lead.source_summary;
    upsert_camp_map_lead( site, std::move( lead ) );

    return changed;
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
        report.observations = make_bounded_sortie_observations( site.active_outing.observations );
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
        report.observations = make_bounded_sortie_observations(
                                  candidate.active_outing.observations );
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

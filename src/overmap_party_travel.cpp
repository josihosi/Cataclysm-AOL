#include "overmap_party_travel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <limits>

#include "current_map.h"
#include "json.h"
#include "map.h"
#include "line.h"

namespace overmap_party_travel
{
bool omt_intersects_loaded_map( const tripoint_abs_omt &omt )
{
    map &loaded = get_map();
    const auto corner = loaded.get_bub( project_to<coords::ms>( omt ) );
    const int last = loaded.getmapsize() * SEEX - 1;
    const int edge = coords::map_squares_per( coords::omt ) - 1;
    return loaded.inbounds( tripoint_bub_ms( 0, 0, corner.z() ) ) &&
           corner.x() <= last && corner.x() + edge >= 0 &&
           corner.y() <= last && corner.y() + edge >= 0;
}

namespace
{
thread_local route_search_counts *current_route_search_counts = nullptr;
}

scoped_route_search_counter::scoped_route_search_counter( route_search_counts &counts ) :
    previous( current_route_search_counts )
{
    current_route_search_counts = &counts;
}

scoped_route_search_counter::~scoped_route_search_counter()
{
    current_route_search_counts = previous;
}

void route_state::clear()
{
    *this = route_state{};
}

route_result next_route_step( route_state &state, const tripoint_abs_omt &origin,
                              const tripoint_abs_omt &destination, const int now,
                              const int retry_delay, const std::size_t retained_edges,
                              const route_finder &find, const edge_validator &valid,
                              const std::optional<int> destination_cost )
{
    if( !state.target || *state.target != destination ) {
        auto exclusions = std::move( state.policy_exclusions );
        state.clear();
        state.policy_exclusions = std::move( exclusions );
        state.target = destination;
    }
    if( state.destination_cost && destination_cost &&
        *state.destination_cost != *destination_cost ) {
        state.path.clear();
        state.retry_after = -1;
        state.failed_origin.reset();
    }
    state.destination_cost = destination_cost;
    if( state.failed_origin && *state.failed_origin != origin ) {
        state.failed_origin.reset();
        state.retry_after = -1;
    }
    if( origin == destination ) {
        state.path.clear();
        state.retry_after = -1;
        return { route_status::reached, std::nullopt };
    }
    while( !state.path.empty() && state.path.back() == origin ) {
        state.path.pop_back();
    }
    if( !state.path.empty() && !valid( origin, state.path.back() ) ) {
        state.path.clear();
        state.retry_after = -1;
    }
    if( state.path.empty() ) {
        if( state.retry_after >= now ) {
            return { route_status::retry_pending, std::nullopt };
        }
        // A known impassable destination cannot produce a legal route. Avoid
        // exploring the whole overmap until that terrain or policy changes.
        if( destination_cost && *destination_cost < 0 ) {
            state.path.clear();
        } else {
            if( current_route_search_counts ) {
                ++current_route_search_counts->searches;
            }
            state.path = find( origin, destination );
        }
        if( state.path.empty() || state.path.front() != destination || state.path.back() != origin ) {
            state.path.clear();
            state.failed_origin = origin;
            const int delay = std::max( retry_delay, 0 );
            state.retry_after = now > std::numeric_limits<int>::max() - delay ?
                                std::numeric_limits<int>::max() : now + delay;
            return { route_status::unavailable, std::nullopt };
        }
        state.failed_origin.reset();
        state.retry_after = -1;
        const std::size_t keep = std::max<std::size_t>( retained_edges, 1 );
        if( state.path.size() - 1 > keep ) {
            state.path.erase( state.path.begin(), state.path.end() - keep - 1 );
        }
        state.path.pop_back();
    }
    if( state.path.empty() || !valid( origin, state.path.back() ) ) {
        state.path.clear();
        state.failed_origin = origin;
        const int delay = std::max( retry_delay, 0 );
        state.retry_after = now > std::numeric_limits<int>::max() - delay ?
                            std::numeric_limits<int>::max() : now + delay;
        return { route_status::unavailable, std::nullopt };
    }
    return { route_status::step, state.path.back() };
}

void work::clear()
{
    *this = work{};
}

void work::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "work_millis", work_millis );
    json.member( "serviced_active_turns", serviced_active_turns );
    json.member( "edge_bound", edge_bound );
    if( !actor_ids.empty() ) {
        json.member( "actor_ids", actor_ids );
        json.member( "actor_epochs", actor_epochs );
        json.member( "operation_generation", operation_generation );
    }
    if( edge_bound ) {
        json.member( "edge_cost_millis", edge_cost_millis );
        json.member( "from", from );
        json.member( "to", to );
    }
    json.end_object();
}

void work::deserialize( const JsonObject &jo )
{
    work candidate;
    jo.read( "work_millis", candidate.work_millis );
    jo.read( "serviced_active_turns", candidate.serviced_active_turns );
    jo.read( "edge_bound", candidate.edge_bound );
    if( jo.has_member( "actor_ids" ) || jo.has_member( "actor_epochs" ) ||
        jo.has_member( "operation_generation" ) ) {
        if( !jo.has_member( "actor_ids" ) || !jo.has_member( "actor_epochs" ) ||
            !jo.has_member( "operation_generation" ) ) {
            jo.throw_error( "incomplete coarse party identity binding" );
        }
        jo.read( "actor_ids", candidate.actor_ids );
        jo.read( "actor_epochs", candidate.actor_epochs );
        jo.read( "operation_generation", candidate.operation_generation );
        if( candidate.actor_ids.empty() || candidate.actor_ids.size() != candidate.actor_epochs.size() ||
            candidate.operation_generation == 0 ||
            std::any_of( candidate.actor_ids.begin(), candidate.actor_ids.end(),
        []( const std::string &id ) { return id.empty(); } ) ) {
            jo.throw_error( "invalid coarse party identity binding" );
        }
    }
    if( candidate.edge_bound ) {
        if( !jo.has_member( "edge_cost_millis" ) || !jo.has_member( "from" ) ||
            !jo.has_member( "to" ) ) {
            jo.throw_error( "incomplete coarse party edge work" );
        }
        jo.read( "edge_cost_millis", candidate.edge_cost_millis );
        jo.read( "from", candidate.from );
        jo.read( "to", candidate.to );
    }
    if( candidate.work_millis < 0 || candidate.serviced_active_turns < -1 ||
        ( candidate.edge_bound && ( candidate.edge_cost_millis <= 0 ||
                                    candidate.work_millis >= candidate.edge_cost_millis ||
                                    candidate.from.empty() ||
                                    candidate.from.size() != candidate.to.size() ) ) ||
        ( !candidate.edge_bound && candidate.work_millis > 300000 ) ) {
        jo.throw_error( "invalid coarse party edge work" );
    }
    for( std::size_t i = 0; i < candidate.from.size(); ++i ) {
        if( rl_dist( candidate.from[i], candidate.to[i] ) != 1 ) {
            jo.throw_error( "invalid coarse party edge" );
        }
    }
    *this = candidate;
}

transit_result prepare_transit( work &state, const std::vector<actor_input> &actors,
                                const transit_policy &policy )
{
    transit_result result;
    if( policy.movement_phase == phase::local ) {
        result.status = transit_status::local_handoff;
        return result;
    }
    if( actors.empty() ) {
        return result;
    }
    std::vector<tripoint_abs_omt> from;
    std::vector<tripoint_abs_omt> to;
    std::vector<std::string> actor_ids;
    std::vector<std::uint64_t> actor_epochs;
    std::uint64_t generation = 0;
    int maximum_weight = 0;
    result.next.reserve( actors.size() );
    result.routes.reserve( actors.size() );
    for( const actor_input &actor : actors ) {
        if( actor.actor_id.empty() || ( policy.active_millis && actor.operation_generation == 0 ) ||
            !actor.route || !actor.find || !actor.valid || !actor.terrain_weight || !actor.gate ) {
            return result;
        }
        if( generation != 0 && generation != actor.operation_generation ) {
            return result;
        }
        if( std::find( actor_ids.begin(), actor_ids.end(), actor.actor_id ) != actor_ids.end() ) {
            return result;
        }
        generation = actor.operation_generation;
        actor_ids.push_back( actor.actor_id );
        actor_epochs.push_back( actor.lease_epoch );
        auto &route = *actor.route;
        if( route.actor_id != actor.actor_id ||
            route.operation_generation != actor.operation_generation ||
            route.lease_epoch != actor.lease_epoch ) {
            auto exclusions = std::move( route.policy_exclusions );
            route.clear();
            route.policy_exclusions = std::move( exclusions );
        }
        const auto selected = next_route_step( route, actor.origin, actor.destination,
                              policy.now, policy.retry_delay, policy.retained_edges,
                              actor.find, actor.valid, actor.destination_cost );
        // next_route_step may reset a changed destination; bind its owner only
        // after that reset, including negative results.
        route.actor_id = actor.actor_id;
        route.operation_generation = actor.operation_generation;
        route.lease_epoch = actor.lease_epoch;
        if( selected.status == route_status::reached ) {
            result.next.emplace_back();
            result.routes.push_back( { actor.origin } );
            continue;
        }
        if( !selected.next ) {
            result.status = selected.status == route_status::retry_pending ?
                            transit_status::retry_pending : transit_status::unavailable;
            return result;
        }
        const edge_gate gate = actor.gate( *selected.next );
        if( gate != edge_gate::distant ) {
            result.status = gate == edge_gate::local_handoff ?
                            transit_status::local_handoff : transit_status::unavailable;
            return result;
        }
        const int weight = actor.terrain_weight( *selected.next );
        if( weight <= 0 ) {
            return result;
        }
        maximum_weight = std::max( maximum_weight, weight );
        from.push_back( actor.origin );
        to.push_back( *selected.next );
        result.next.push_back( selected.next );
        auto full_route = route.path;
        full_route.push_back( actor.origin );
        result.routes.push_back( std::move( full_route ) );
    }
    if( from.empty() ) {
        result.status = transit_status::reached;
        return result;
    }
    if( !policy.active_millis ) {
        result.status = transit_status::route_ready;
        return result;
    }
    if( policy.collapse_shared_edge && from.size() > 1 &&
        std::all_of( from.begin() + 1, from.end(), [&]( const auto &p ) { return p == from.front(); } ) &&
        std::all_of( to.begin() + 1, to.end(), [&]( const auto &p ) { return p == to.front(); } ) ) {
        from.resize( 1 );
        to.resize( 1 );
    }
    work next_work = state;
    if( !next_work.actor_ids.empty() &&
        ( next_work.actor_ids != actor_ids || next_work.actor_epochs != actor_epochs ||
          next_work.operation_generation != generation ) ) {
        const auto claimed = next_work.serviced_active_turns;
        next_work.clear();
        next_work.serviced_active_turns = claimed;
    }
    next_work.actor_ids = std::move( actor_ids );
    next_work.actor_epochs = std::move( actor_epochs );
    next_work.operation_generation = generation;
    bool moved = false;
    if( !service_set( next_work, from, to, maximum_weight, policy.slowest_speed,
                      *policy.active_millis, moved, policy.fixed_edge_cost_millis ) ) {
        return result;
    }
    state = std::move( next_work );
    result.status = moved ? transit_status::step : transit_status::waiting;
    return result;
}

std::optional<std::int64_t> edge_cost_millis( int terrain_weight, int slowest_speed )
{
    if( terrain_weight <= 0 || slowest_speed <= 0 ) {
        return std::nullopt;
    }
    const std::int64_t numerator = static_cast<std::int64_t>( terrain_weight ) * 30000000;
    const std::int64_t denominator = static_cast<std::int64_t>( slowest_speed ) * 24;
    return ( numerator + denominator - 1 ) / denominator;
}

bool service( work &state, const tripoint_abs_omt &from, const tripoint_abs_omt &to,
              int terrain_weight, int slowest_speed, int active_millis, bool &move )
{
    return service_set( state, { from }, { to }, terrain_weight, slowest_speed, active_millis, move );
}

bool service_set( work &state, const std::vector<tripoint_abs_omt> &from,
                  const std::vector<tripoint_abs_omt> &to, const int terrain_weight,
                  const int slowest_speed, const int active_millis, bool &move,
                  const std::optional<std::int64_t> fixed_edge_cost_millis )
{
    move = false;
    if( terrain_weight <= 0 || slowest_speed <= 0 || active_millis <= 0 ||
        active_millis > 300000 || from.empty() || from.size() != to.size() ||
        state.work_millis < 0 ||
        ( state.edge_bound && ( state.edge_cost_millis <= 0 ||
                                state.work_millis >= state.edge_cost_millis ) ) ||
        ( !state.edge_bound && state.work_millis > 300000 ) ) {
        return false;
    }
    for( std::size_t i = 0; i < from.size(); ++i ) {
        if( rl_dist( from[i], to[i] ) != 1 ) {
            return false;
        }
    }
    // 5 minutes * terrain weight / 24 * 100 / slowest speed.
    const auto price = fixed_edge_cost_millis ? fixed_edge_cost_millis :
                       edge_cost_millis( terrain_weight, slowest_speed );
    if( !price || *price <= 0 ) {
        return false;
    }
    const std::int64_t cost = *price;
    work next = state;
    if( next.edge_bound ) {
        if( next.from != from || next.to != to ) {
            next.work_millis = 0;
        } else if( next.edge_cost_millis != cost ) {
            next.work_millis = static_cast<std::int64_t>( std::floor(
                                   static_cast<long double>( next.work_millis ) * cost /
                                   next.edge_cost_millis ) );
        }
    }
    next.from = from;
    next.to = to;
    next.edge_cost_millis = cost;
    next.edge_bound = true;
    next.work_millis += active_millis;
    if( next.work_millis >= cost ) {
        next.work_millis = std::min<std::int64_t>( next.work_millis - cost, 300000 );
        next.edge_bound = false;
        next.edge_cost_millis = 0;
        move = true;
    }
    state = next;
    return true;
}
} // namespace overmap_party_travel

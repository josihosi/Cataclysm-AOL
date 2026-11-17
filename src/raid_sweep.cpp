#include "raid_sweep.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <utility>

#include "active_time.h"
#include "field_type.h"
#include "json.h"
#include "line.h"
#include "map.h"
#include "mapdata.h"

namespace raid_sweep
{
namespace
{
void next( progress &state )
{
    const int previous = state.cursor;
    while( state.cursor < static_cast<int>( state.candidates.size() ) &&
           state.candidates[state.cursor].state != disposition::pending ) {
        ++state.cursor;
    }
    if( previous != state.cursor ) {
        state.candidate_started = state.active_elapsed;
        state.movement.clear();
        state.retry_after_active = state.active_elapsed + 30 * 60;
    }
}
} // namespace

bool home_rally_member::working() const
{
    return outcome == home_rally_outcome::homeward;
}

void home_rally_member::evaluate( int now, bool eligible, int native_body_anchor )
{
    if( now < 0 || native_body_anchor > now || now < last_world ) {
        return;
    }
    eligible = eligible && working();
    if( last_world < 0 ) {
        // Old valid bodies adopt now. Their untracked absence is not recall
        // work or biological catch-up credit.
        last_world = now;
        active = eligible;
        body_anchor = native_body_anchor;
        body_pause = native_body_anchor < 0 ? 0 : now - native_body_anchor;
        return;
    }
    const int elapsed = now - last_world;
    if( elapsed > 0 ) {
        active_delta_turns = active && eligible ? elapsed : 0;
        active_turns += active_delta_turns;
        suspended_turns += elapsed - active_delta_turns;
        if( body_anchor >= 0 && body_anchor == native_body_anchor ) {
            body_pause += elapsed - active_delta_turns;
        }
        last_world = now;
    } else if( !eligible ) {
        active_delta_turns = 0;
    }
    if( body_anchor != native_body_anchor ) {
        // A real native body callback settled/advanced its own anchor. Never
        // reapply suspension belonging to an already consumed body interval.
        body_anchor = native_body_anchor;
        body_pause = 0;
    }
    active = eligible;
}

std::int64_t home_rally_member::suspension_at( int now ) const
{
    return suspended_turns + ( !active && last_world >= 0 ?
                              std::max( 0, now - last_world ) : 0 );
}

std::int64_t home_rally_member::suspended_body_turns( int anchor ) const
{
    return anchor >= 0 && anchor == body_anchor ? body_pause : 0;
}

void home_rally_member::settle_body( int anchor )
{
    body_anchor = anchor;
    body_pause = 0;
}

int home_rally_member::claim_motor_turns()
{
    const auto elapsed = working() && active ? active_turns - motor_active : 0;
    motor_active = active_turns;
    return static_cast<int>( std::clamp<std::int64_t>( elapsed, 0,
                             std::numeric_limits<int>::max() ) );
}

void home_rally_member::observe( const tripoint_abs_ms &position, bool physically_home )
{
    if( !working() ) {
        return;
    }
    if( last_position && *last_position != position ) {
        last_progress = active_turns;
    }
    last_position = position;
    if( physically_home ) {
        outcome = home_rally_outcome::arrived;
    }
}

bool home_rally_member::exhausted() const
{
    return working() && ( allowance_turns <= 0 || no_progress_turns <= 0 ||
                         active_turns >= allowance_turns ||
                         active_turns - last_progress >= no_progress_turns );
}

void home_rally_member::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "member", member_id );
    json.member( "outcome", static_cast<int>( outcome ) );
    json.member( "last_world", last_world );
    json.member( "active", active );
    json.member( "active_turns", active_turns );
    json.member( "suspended_turns", suspended_turns );
    json.member( "body_anchor", body_anchor );
    json.member( "body_pause", body_pause );
    json.member( "allowance_turns", allowance_turns );
    json.member( "no_progress_turns", no_progress_turns );
    json.member( "last_progress", last_progress );
    json.member( "motor_active", motor_active );
    json.member( "last_position", last_position );
    json.end_object();
}

void home_rally_member::deserialize( const JsonObject &json )
{
    *this = home_rally_member();
    json.read( "member", member_id );
    const int state = json.get_int( "outcome", 0 );
    if( member_id.empty() || member_id.size() > 128 || state < 0 ||
        state > static_cast<int>( home_rally_outcome::expired ) ) {
        json.throw_error( "invalid home rally participant" );
    }
    outcome = static_cast<home_rally_outcome>( state );
    json.read( "last_world", last_world );
    json.read( "active", active );
    json.read( "active_turns", active_turns );
    json.read( "suspended_turns", suspended_turns );
    json.read( "body_anchor", body_anchor );
    json.read( "body_pause", body_pause );
    json.read( "allowance_turns", allowance_turns );
    json.read( "no_progress_turns", no_progress_turns );
    json.read( "last_progress", last_progress );
    json.read( "motor_active", motor_active );
    json.read( "last_position", last_position );
    if( active_turns < 0 || suspended_turns < 0 || body_pause < 0 ||
        last_progress < 0 || last_progress > active_turns ||
        motor_active < 0 || motor_active > active_turns ) {
        json.throw_error( "invalid home rally active interval" );
    }
}

home_rally_member *home_rally::find( const std::string &id )
{
    const auto found = std::find_if( members.begin(), members.end(), [&]( const auto &member ) {
        return member.member_id == id;
    } );
    return found == members.end() ? nullptr : &*found;
}

const home_rally_member *home_rally::find( const std::string &id ) const
{
    return const_cast<home_rally *>( this )->find( id );
}

bool home_rally::pending() const
{
    return std::any_of( members.begin(), members.end(), []( const auto &member ) {
        return member.working();
    } );
}

bool home_rally::add( home_rally_member member )
{
    if( members.size() >= 16 || member.member_id.empty() || member.member_id.size() > 128 ||
        find( member.member_id ) ) {
        return false;
    }
    members.push_back( std::move( member ) );
    return true;
}

void home_rally::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "source", source );
    json.member( "episode", episode );
    json.member( "generation", generation );
    json.member( "members", members );
    json.end_object();
}

void home_rally::deserialize( const JsonObject &json )
{
    *this = home_rally();
    json.read( "source", source );
    json.read( "episode", episode );
    json.read( "generation", generation );
    if( json.has_array( "members" ) ) {
        for( JsonObject value : json.get_array( "members" ) ) {
            home_rally_member member;
            member.deserialize( value );
            if( !add( std::move( member ) ) ) {
                json.throw_error( "duplicate or oversized home rally roster" );
            }
        }
    }
}

bool fire_approach( const map &here, const tripoint_bub_ms &p )
{
    static const field_type_id fire( "fd_fire" );
    for( const auto &cell : here.points_in_radius( p, 1 ) ) {
        if( here.get_field_intensity( cell, fire ) > 0 &&
            ( cell == p || !here.has_flag( ter_furn_flag::TFLAG_FIRE_CONTAINER, cell ) ) ) {
            return true;
        }
    }
    return false;
}

std::uint64_t fire_revision( const map &here,
                             const std::vector<tripoint_bub_ms> &approach )
{
    std::uint64_t revision = 14695981039346656037ULL;
    bool present = false;
    for( const auto &cell : approach ) {
        for( const auto &nearby : here.points_in_radius( cell, 1 ) ) {
            const int intensity = here.get_field_intensity( nearby, field_type_id( "fd_fire" ) );
            if( intensity <= 0 ) {
                continue;
            }
            present = true;
            const auto facts = here.get_abs( nearby ).to_string_writable() + ":" +
                               std::to_string( intensity ) + ":" +
                               std::to_string( here.has_flag( ter_furn_flag::TFLAG_FIRE_CONTAINER, nearby ) );
            for( unsigned char value : facts ) {
                revision = ( revision ^ value ) * 1099511628211ULL;
            }
        }
    }
    return present ? ( revision & 0x7fffffffffffffffULL ) | 1 : 0;
}

void progress::add( const tripoint_abs_ms &p )
{
    const auto area = project_to<coords::omt>( p );
    if( std::abs( area.x() - source.x() ) > 1 || std::abs( area.y() - source.y() ) > 1 ||
    std::any_of( candidates.begin(), candidates.end(), [&]( const auto & candidate ) {
    return candidate.position == p;
} ) ) {
        return;
    }
    candidates.push_back( {p} );
}

void progress::add_floor( const tripoint_abs_omt &floor )
{
    static const std::array<point, 5> cross = {{{12, 12}, {6, 12}, {18, 12}, {12, 6}, {12, 18}}};
    for( const auto &offset : cross ) {
        add( project_to<coords::ms>( floor ) + offset );
    }
}

void progress::begin( const tripoint_abs_omt &region, std::uint64_t operation_generation,
                      const std::optional<tripoint_abs_ms> &exact_source )
{
    *this = progress();
    source = region;
    generation = operation_generation;
    if( exact_source && project_to<coords::omt>( *exact_source ) == source ) {
        add( *exact_source );
    }
    static const std::array<point, 9> ring = {{{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1},
            {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
        }
    };
    for( const auto &offset : ring ) {
        add_floor( tripoint_abs_omt( source.xy() + offset, 0 ) );
        if( source.z() != 0 ) {
            add_floor( source + offset );
        }
    }
}

void progress::advance( int world_turn, int active_turns )
{
    if( world_turn <= last_world ) {
        return;
    }
    active_elapsed += active_time::clipped_turns( world_turn, last_world, active_turns );
    last_world = world_turn;
}

void progress::made_progress()
{
    last_progress = active_elapsed;
    retry_after_active = active_elapsed + 30 * 60;
}

void progress::approach( const tripoint_abs_ms &position, const std::string &member )
{
    if( pending() ) {
        auto &witness = movement[member];
        const bool moved = witness.position && *witness.position != position;
        const bool new_cell = witness.visited.insert( position ).second;
        witness.position = position;
        if( moved && new_cell ) {
            made_progress();
        }
    }
}

bool progress::obstructed() const
{
    return active_elapsed >= retry_after_active;
}

std::optional<tripoint_abs_ms> progress::pending() const
{
    if( cursor < 0 || cursor >= static_cast<int>( candidates.size() ) ) {
        return std::nullopt;
    }
    return candidates[cursor].position;
}

bool progress::arrive( const tripoint_abs_ms &position, bool eligible_observation )
{
    if( !eligible_observation || pending() != position ) {
        return false;
    }
    candidates[cursor].state = disposition::searched;
    made_progress();
    next( *this );
    return true;
}

bool progress::exclude( disposition reason, std::uint64_t geometry_revision,
                        std::vector<std::string> available_members )
{
    if( !pending() || ( reason != disposition::hazardous && reason != disposition::unreachable ) ) {
        return false;
    }
    candidates[cursor].state = reason;
    candidates[cursor].geometry_revision = geometry_revision;
    std::sort( available_members.begin(), available_members.end() );
    available_members.erase( std::unique( available_members.begin(), available_members.end() ),
                             available_members.end() );
    candidates[cursor].available_members_at_exclusion = std::move( available_members );
    // Exclusion is knowledge about a candidate, not physical search progress.
    next( *this );
    return true;
}

bool progress::reconsider( const tripoint_abs_ms &position, std::uint64_t revision,
                           const std::string &member, bool physically_reopened )
{
    for( size_t i = 0; i < candidates.size(); ++i ) {
        auto &candidate = candidates[i];
        const bool changed = candidate.state == disposition::unreachable && !member.empty() ?
                             physically_reopened ||
                             std::find( candidate.available_members_at_exclusion.begin(),
                                        candidate.available_members_at_exclusion.end(), member ) ==
                             candidate.available_members_at_exclusion.end() :
                             revision != candidate.geometry_revision;
        if( candidate.position == position &&
            ( candidate.state == disposition::hazardous || candidate.state == disposition::unreachable ) &&
            !candidate.reconsidered && changed ) {
            candidate.state = disposition::pending;
            candidate.reconsidered = true;
            candidate.geometry_revision = revision;
            cursor = std::min( cursor, static_cast<int>( i ) );
            candidate_started = active_elapsed;
            movement.clear();
            retry_after_active = active_elapsed + 30 * 60;
            return true;
        }
    }
    return false;
}

std::string progress::outcome() const
{
    if( pending() ) {
        return "pending";
    }
    if( !candidates.empty() && std::all_of( candidates.begin(), candidates.end(), []( const auto & c ) {
    return c.state == disposition::hazardous;
} ) ) {
        return "hazard_blocked";
    }
    return std::all_of( candidates.begin(), candidates.end(), []( const auto & c ) {
        return c.state == disposition::searched;
    } ) ? "searched" : "incomplete_sweep";
}

void candidate::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "position", position );
    json.member( "state", static_cast<int>( state ) );
    json.member( "geometry_revision", geometry_revision );
    json.member( "reconsidered", reconsidered );
    json.member( "available_members_at_exclusion", available_members_at_exclusion );
    json.end_object();
}

void candidate::deserialize( const JsonObject &json )
{
    *this = candidate();
    json.read( "position", position );
    state = static_cast<disposition>( std::clamp( json.get_int( "state", 0 ), 0, 3 ) );
    json.read( "geometry_revision", geometry_revision );
    json.read( "reconsidered", reconsidered );
    json.read( "available_members_at_exclusion", available_members_at_exclusion );
    if( state == disposition::unreachable && !json.has_member( "available_members_at_exclusion" ) ) {
        // An old exclusion has no approach witness. Keep it unsearched;
        // reload cannot invent a newly opened route or a fresh allowance.
        reconsidered = true;
    }
}

void movement_witness::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "position", position );
    json.member( "visited", visited );
    json.end_object();
}

void movement_witness::deserialize( const JsonObject &json )
{
    *this = movement_witness();
    json.read( "position", position );
    json.read( "visited", visited );
}

void progress::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "source", source );
    json.member( "generation", generation );
    json.member( "candidates", candidates );
    json.member( "cursor", cursor );
    json.member( "active_elapsed", active_elapsed );
    json.member( "last_progress", last_progress );
    json.member( "candidate_started", candidate_started );
    json.member( "last_world", last_world );
    json.member( "movement", movement );
    json.member( "retry_after_active", retry_after_active );
    json.end_object();
}

void progress::deserialize( const JsonObject &json )
{
    *this = progress();
    json.read( "source", source );
    json.read( "generation", generation );
    json.read( "candidates", candidates );
    json.read( "cursor", cursor );
    cursor = std::clamp( cursor, 0, static_cast<int>( candidates.size() ) );
    json.read( "active_elapsed", active_elapsed );
    json.read( "last_progress", last_progress );
    json.read( "candidate_started", candidate_started );
    json.read( "last_world", last_world );
    json.read( "movement", movement );
    retry_after_active = std::max( last_progress, candidate_started ) + 30 * 60;
    json.read( "retry_after_active", retry_after_active );
    next( *this );
}
namespace
{
tripoint_abs_omt source_area( const tripoint_abs_omt &p )
{
    return tripoint_abs_omt( p.xy(), 0 );
}

std::uint64_t memory_hash( const std::string &key )
{
    std::uint64_t result = 14695981039346656037ULL;
    for( unsigned char c : key ) {
        result = ( result ^ c ) * 1099511628211ULL;
    }
    return result;
}

bool remembered( const source_memory &memory, const std::string &key )
{
    const auto hash = memory_hash( key );
    const int a = hash % 504;
    const int b = ( hash >> 32 ) % 504;
    return ( memory.prior[a / 63] & ( 1ULL << ( a % 63 ) ) ) &&
           ( memory.prior[b / 63] & ( 1ULL << ( b % 63 ) ) );
}

void remember( source_memory &memory, const std::string &key )
{
    const auto hash = memory_hash( key );
    const int a = hash % 504;
    const int b = ( hash >> 32 ) % 504;
    memory.prior[a / 63] |= 1ULL << ( a % 63 );
    memory.prior[b / 63] |= 1ULL << ( b % 63 );
}
}

std::string fingerprint( std::vector<std::string> facts )
{
    std::sort( facts.begin(), facts.end() );
    facts.erase( std::unique( facts.begin(), facts.end() ), facts.end() );
    std::string canonical;
    for( const auto &fact : facts ) {
        canonical += std::to_string( fact.size() ) + ":" + fact;
    }
    return std::to_string( memory_hash( canonical ) );
}

void source_memory::advance( int world_turn, int active_turns )
{
    if( world_turn <= last_world ) {
        return;
    }
    active_elapsed += active_time::clipped_turns( world_turn, last_world, active_turns );
    last_world = world_turn;
}

bool source_memory::ready( const tripoint_abs_omt &p, const std::string &evidence ) const
{
    const auto area = source_area( p );
    const auto found = std::find_if( outcomes.begin(), outcomes.end(), [&]( const auto & old ) {
        return old.area == area;
    } );
    if( found != outcomes.end() ) {
        return found->generation == 0 ||
               ( active_elapsed >= found->retry_after_active && evidence != found->evidence );
    }
    const auto key = area.to_string_writable();
    if( remembered( *this, key + ":" + evidence ) ||
        ( remembered( *this, key ) && active_elapsed < last_eviction_active + 24 * 60 * 60 ) ) {
        return false;
    }
    return outcomes.size() < 4 ||
    std::any_of( outcomes.begin(), outcomes.end(), [&]( const auto & old ) {
        return old.retry_after_active <= active_elapsed;
    } );
}

bool source_memory::reserve( const tripoint_abs_omt &p )
{
    const auto area = source_area( p );
    auto found = std::find_if( outcomes.begin(), outcomes.end(), [&]( const auto & old ) {
        return old.area == area;
    } );
    if( found == outcomes.end() ) {
        if( outcomes.size() >= 4 ) {
            auto oldest = outcomes.end();
            for( auto it = outcomes.begin(); it != outcomes.end(); ++it ) {
                if( it->retry_after_active <= active_elapsed &&
                    ( oldest == outcomes.end() || it->finished_active < oldest->finished_active ) ) {
                    oldest = it;
                }
            }
            if( oldest == outcomes.end() ) {
                return false;
            }
            const auto key = oldest->area.to_string_writable();
            remember( *this, key );
            remember( *this, key + ":" + oldest->evidence );
            last_eviction_active = active_elapsed;
            outcomes.erase( oldest );
        }
        outcomes.emplace_back();
        found = std::prev( outcomes.end() );
        found->area = area;
    }
    return true;
}


source_outcome *source_memory::find( const tripoint_abs_omt &p )
{
    const auto area = source_area( p );
    const auto it = std::find_if( outcomes.begin(), outcomes.end(), [&]( const auto &entry ) {
        return entry.area == area;
    } );
    return it == outcomes.end() ? nullptr : &*it;
}

const source_outcome *source_memory::find( const tripoint_abs_omt &p ) const
{
    return const_cast<source_memory *>( this )->find( p );
}

bool source_memory::rally_history_known( const tripoint_abs_omt &p, std::uint64_t episode ) const
{
    const auto *entry = find( p );
    // An evicted area's missing facts are uncertainty, never a fresh trip.
    // Actual extinction followed by a later receipt can establish a new
    // episode even when the previous compact facts were evicted.
    return ( entry && entry->rally_episode > 0 ) || episode > 1 ||
           !remembered( *this, source_area( p ).to_string_writable() );
}

std::uint64_t source_memory::next_episode( const tripoint_abs_omt &p, int signal_world ) const
{
    const auto area = source_area( p );
    for( const auto &old : outcomes ) {
        if( old.area == area ) {
            return old.episode + ( !old.source_active && signal_world > old.absent_world ? 1 : 0 );
        }
    }
    return 1;
}

void source_memory::adopt_signal( const tripoint_abs_omt &p, int signal_world )
{
    const auto area = source_area( p );
    for( auto &old : outcomes ) {
        if( old.area == area && !old.source_active && signal_world > old.absent_world ) {
            ++old.episode;
            old.source_active = true;
            return;
        }
    }
}

void source_memory::observe_absence( const tripoint_abs_omt &p, int observed_world )
{
    const auto area = source_area( p );
    for( auto &old : outcomes ) {
        if( old.area == area ) {
            old.source_active = false;
            old.absent_world = std::max( old.absent_world, observed_world );
            return;
        }
    }
}

bool source_memory::finish( const tripoint_abs_omt &p, std::uint64_t generation,
                            const std::string &evidence, const std::string &result )
{
    if( generation == 0 || !reserve( p ) ) {
        return false;
    }
    const auto area = source_area( p );
    auto found = std::find_if( outcomes.begin(), outcomes.end(), [&]( const auto & old ) {
        return old.area == area;
    } );
    if( generation <= found->generation ) {
        return false;
    }
    found->generation = generation;
    found->evidence = evidence;
    found->outcome = result;
    found->finished_active = active_elapsed;
    found->retry_after_active = active_elapsed + 24 * 60 * 60;
    return true;
}

void source_outcome::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "area", area );
    json.member( "generation", generation );
    json.member( "episode", episode );
    json.member( "source_active", source_active );
    json.member( "absent_world", absent_world );
    if( rally_episode > 0 || !rally_outcomes.empty() ) {
        json.member( "rally_episode", rally_episode );
        json.member( "rally_outcomes", rally_outcomes );
    }
    json.member( "evidence", evidence );
    json.member( "outcome", outcome );
    json.member( "finished_active", finished_active );
    json.member( "retry_after_active", retry_after_active );
    json.end_object();
}

void source_outcome::deserialize( const JsonObject &json )
{
    *this = source_outcome();
    json.read( "area", area );
    area = source_area( area );
    json.read( "generation", generation );
    json.read( "episode", episode );
    json.read( "source_active", source_active );
    json.read( "absent_world", absent_world );
    json.read( "rally_episode", rally_episode );
    json.read( "rally_outcomes", rally_outcomes );
    if( rally_episode > episode || ( rally_episode == 0 && !rally_outcomes.empty() ) ||
        rally_outcomes.size() > 16 || std::any_of( rally_outcomes.begin(), rally_outcomes.end(),
    []( const auto &entry ) {
        return entry.first.empty() || entry.first.size() > 128 || entry.second < 0 ||
               entry.second > static_cast<int>( home_rally_outcome::expired );
    } ) ) {
        json.throw_error( "invalid home rally source facts" );
    }
    json.read( "evidence", evidence );
    json.read( "outcome", outcome );
    json.read( "finished_active", finished_active );
    json.read( "retry_after_active", retry_after_active );
}

void source_memory::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "outcomes", outcomes );
    json.member( "active_elapsed", active_elapsed );
    json.member( "last_world", last_world );
    json.member( "prior", prior );
    json.member( "last_eviction_active", last_eviction_active );
    json.end_object();
}

void source_memory::deserialize( const JsonObject &json )
{
    *this = source_memory();
    json.read( "outcomes", outcomes );
    json.read( "active_elapsed", active_elapsed );
    json.read( "last_world", last_world );
    json.read( "prior", prior );
    json.read( "last_eviction_active", last_eviction_active );
}

} // namespace raid_sweep

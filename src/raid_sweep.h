#pragma once
#ifndef CATA_SRC_RAID_SWEEP_H
#define CATA_SRC_RAID_SWEEP_H

#include <cstdint>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "coordinates.h"

class JsonObject;
class JsonOut;
class map;

// Physical search progress shared by species-specific callers. This owns no
// actors, perception, hostility, dispatch permission or transport.
namespace raid_sweep
{
// A finite individual trip to an existing home. This value owns elapsed
// allowance and pause debt only; callers authenticate the actual native body,
// range, footprint and duty before evaluating or recording an arrival.
enum class home_rally_outcome : int { homeward, arrived, transferred, unavailable, expired };
struct home_rally_member {
    std::string member_id;
    home_rally_outcome outcome = home_rally_outcome::homeward;
    int last_world = -1;
    bool active = false;
    std::int64_t active_turns = 0;
    std::int64_t suspended_turns = 0;
    int active_delta_turns = 0; // transient, never reload credit
    int body_anchor = -1;
    std::int64_t body_pause = 0;
    std::int64_t allowance_turns = 0;
    std::int64_t no_progress_turns = 0;
    std::int64_t last_progress = 0;
    std::int64_t motor_active = 0;
    std::optional<tripoint_abs_ms> last_position;

    bool working() const;
    void evaluate( int world_turn, bool eligible, int native_body_anchor = -1 );
    std::int64_t suspension_at( int world_turn ) const;
    std::int64_t suspended_body_turns( int native_body_anchor ) const;
    void settle_body( int native_body_anchor );
    // Read/planning never claims a motor interval. The existing movement owner
    // claims once, including failed/blocked opportunities: no banked catch-up.
    int claim_motor_turns();
    void observe( const tripoint_abs_ms &actual_position, bool physically_home );
    bool exhausted() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct home_rally {
    tripoint_abs_omt source;
    std::uint64_t episode = 0;
    std::uint64_t generation = 0;
    std::vector<home_rally_member> members;
    home_rally_member *find( const std::string &member_id );
    const home_rally_member *find( const std::string &member_id ) const;
    bool pending() const;
    bool add( home_rally_member member );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

enum class disposition : int { pending, searched, hazardous, unreachable };

// Actual field cells, including an uncontained adjacent fire. A contained
// fire excludes its own square, not the safe ground around the container.
bool fire_approach( const map &here, const tripoint_bub_ms &position );
std::uint64_t fire_revision( const map &here,
                             const std::vector<tripoint_bub_ms> &approach );

struct candidate {
    tripoint_abs_ms position;
    disposition state = disposition::pending;
    std::uint64_t geometry_revision = 0;
    bool reconsidered = false;
    std::vector<std::string> available_members_at_exclusion = {};
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct movement_witness {
    std::optional<tripoint_abs_ms> position;
    std::set<tripoint_abs_ms> visited;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct progress {
    tripoint_abs_omt source;
    std::uint64_t generation = 0;
    std::vector<candidate> candidates;
    int cursor = 0;
    std::int64_t active_elapsed = 0;
    std::int64_t last_progress = 0;
    std::int64_t candidate_started = 0;
    int last_world = -1;
    std::map<std::string, movement_witness> movement;
    std::int64_t retry_after_active = 30 * 60;
    void begin( const tripoint_abs_omt &region, std::uint64_t operation_generation,
                const std::optional<tripoint_abs_ms> &exact_source = std::nullopt );
    void add( const tripoint_abs_ms &position );
    void add_floor( const tripoint_abs_omt &floor );
    // Callers supply legitimate active elapsed; a world timestamp is never
    // catch-up credit. An old/absent clock initializes without backlog.
    void advance( int world_turn, int active_turns );
    void made_progress();
    // Actual movement to a new cell counts even on an off-axis detour.
    // A first sample, stationary peers and revisiting cells do not extend time.
    void approach( const tripoint_abs_ms &actual_position, const std::string &member = {} );
    bool obstructed() const;
    std::optional<tripoint_abs_ms> pending() const;
    bool arrive( const tripoint_abs_ms &actual_position, bool eligible_observation );
    bool exclude( disposition reason, std::uint64_t geometry_revision,
                  std::vector<std::string> available_members = {} );
    bool reconsider( const tripoint_abs_ms &position, std::uint64_t geometry_revision,
                     const std::string &member = {}, bool physically_reopened = false );
    std::string outcome() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

// Dispatch history is about a physical source area and material evidence,
// never packet IDs. Species keep their separate casualty/capability caution.
struct source_outcome {
    tripoint_abs_omt area;
    std::uint64_t generation = 0;
    std::uint64_t episode = 1;
    bool source_active = true;
    int absent_world = -1;
    // Compact spent facts for this source's current recall episode. The
    // active rally remains the sole interval/body owner.
    std::uint64_t rally_episode = 0;
    std::map<std::string, int> rally_outcomes;
    std::string evidence;
    std::string outcome;
    std::int64_t finished_active = 0;
    std::int64_t retry_after_active = 0;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct source_memory {
    std::vector<source_outcome> outcomes;
    std::int64_t active_elapsed = 0;
    int last_world = -1;
    std::int64_t last_eviction_active = -1;
    // Conservative fingerprints of evicted areas and their completed facts.
    // A collision refuses a retry; it can never invent permission or a loss.
    std::array<std::uint64_t, 8> prior{};
    void advance( int world_turn, int active_turns );
    bool ready( const tripoint_abs_omt &area, const std::string &evidence ) const;
    bool reserve( const tripoint_abs_omt &area );
    source_outcome *find( const tripoint_abs_omt &area );
    const source_outcome *find( const tripoint_abs_omt &area ) const;
    bool rally_history_known( const tripoint_abs_omt &area, std::uint64_t episode ) const;
    std::uint64_t next_episode( const tripoint_abs_omt &area, int signal_world ) const;
    void adopt_signal( const tripoint_abs_omt &area, int signal_world );
    void observe_absence( const tripoint_abs_omt &area, int observed_world );
    bool finish( const tripoint_abs_omt &area, std::uint64_t generation,
                 const std::string &evidence, const std::string &outcome );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

std::string fingerprint( std::vector<std::string> facts );
} // namespace raid_sweep
#endif

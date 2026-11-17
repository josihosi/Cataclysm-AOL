#pragma once
#ifndef CATA_SRC_MIGO_TOWER_AI_H
#define CATA_SRC_MIGO_TOWER_AI_H

#include <optional>
#include <map>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

#include "coordinates.h"
#include "damage.h"
#include "raid_sweep.h"
#include "physical_navigation.h"
#include "migo_population_identity.h"
#include "migo_population.h"
#include "overmap_party_travel.h"

class JsonObject;
class JsonOut;

namespace migo_tower
{
struct melee_option {
    damage_instance damage;
    int cost = 100;
    double hit = 0;
    bool dodgeable = true;
    int range = 1;
    bool no_adjacent = false;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};
// Inputs are supplied only after a native ownership/perception check. For our
// members hp is current; enemy hp/armor/attacks are conservative species or
// visible-equipment estimates, never reads of an unseen actor or inventory.
struct combat_profile {
    bool known = false;
    double hp = 0;
    double speed = 0;
    double hit = 0;
    double dodge = 0;
    int melee_cost = 100;
    damage_instance melee;
    std::vector<melee_option> melee_options;
    resistances armor;
    int ranged_cost = 100;
    int ranged_range = 0;
    damage_instance ranged;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

using threat_scenarios = std::vector<std::vector<combat_profile>>;
std::string capability_key( const std::vector<combat_profile> &profiles );
void remember_observation( threat_scenarios &memory, const std::vector<combat_profile> &profiles );

struct candidate {
    std::string member_id;
    std::string type;
    combat_profile capability;
};

struct capability_projection {
    bool known = false;
    bool sufficient = false;
    int casualties = 0;
    double defeat_seconds = 0;
    double party_damage_per_second = 0;
    double incoming_damage_per_second = 0;
};

struct admission_decision {
    int required_home_reserve = 2;
    std::vector<std::string> members;
    capability_projection projection;
    std::string reason;
};

bool capable( const combat_profile &profile );

// A bounded analytic damage/survival estimate, not a combat rollout. Incoming
// damage remains at full strength until defeat; armor, action rate and ranged
// exposure can therefore reject even the largest available response.
capability_projection project( const std::vector<combat_profile> &party,
                               const std::vector<combat_profile> &enemies,
                               int separation, bool require_no_casualties );
admission_decision select_party( const std::vector<candidate> &available,
                                 const std::vector<combat_profile> &observed_enemies,
                                 int separation, bool require_no_casualties, int home_reserve = 2 );
admission_decision select_response( const std::vector<candidate> &available,
                                    const threat_scenarios &observed_scenarios,
                                    int separation, bool require_no_casualties, int home_reserve,
                                    const std::vector<combat_profile> &must_outperform = {} );

// A received emitter is a location hypothesis. It contains no actor identity,
// inventory or threat facts; those require a separate physical observer read.
struct signal_record {
    std::string sample_id;
    std::string kind;
    tripoint_abs_omt source_omt;
    std::optional<tripoint_abs_ms> source_ms;
    std::string observer_member;
    tripoint_abs_ms observer_position;
    int observed_turn = -1;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct source_history {
    tripoint_abs_omt area;
    std::uint64_t episode = 1;
    std::uint64_t episode_at_outcome = 0;
    bool observed_active = true;
    int extinct_world = -1;
    std::uint64_t rally_episode = 0;
    std::map<std::string, int> rally_outcomes;
    std::map<std::string, std::string> observed_access;
    // Cache eviction cannot grant the ordinary blind first-response baseline.
    // This is uncertainty about history, never an invented casualty fact.
    bool prior_outcome_unavailable = false;
    int casualty_attempts = 0;
    int overdue_attempts = 0;
    std::int64_t retry_after_active = 0;
    std::int64_t finished_active = 0;
    std::uint64_t completed_generation = 0;
    std::string outcome;
    std::string completed_evidence;
    std::string latest_evidence;
    std::vector<combat_profile> previous_party;
    threat_scenarios opposition;
    std::vector<std::string> last_witnessed_losses;
    int caution_level() const;
    bool requires_stronger() const;
    bool retry_ready( std::int64_t now_active, const std::vector<combat_profile> &party,
                      const admission_decision &decision, bool current_recognition ) const;
    bool finish( std::uint64_t generation, std::int64_t now_active,
                 const std::string &result, const std::vector<std::string> &witnessed_losses,
                 bool overdue_missing, const std::vector<combat_profile> &party );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

enum class member_phase : int { outbound, searching, returning, settled };

// Sparse native payload state, copied with the actual monster at handoff.
// The population token remains the sole site/member/epoch lease. A global
// operation merely references these members and never stores another body.
struct member_intent {
    std::uint64_t generation = 0;
    member_phase phase = member_phase::outbound;
    tripoint_abs_ms home;
    std::optional<tripoint_abs_ms> waypoint;
    std::vector<std::string> witnessed_losses;
    threat_scenarios observed_threats;
    int last_energy_world = -1;
    int last_native_process_world = -1;
    int last_combat_world = -1;
    int last_observed_enemy_world = -1;
    int last_refused_world = -1;
    bool activity_bound = false;
    std::uint64_t activity_generation = 0;
    std::int64_t activity_suspension = 0;
    // Paid coarse work follows this exact body through native ownership and
    // save/load. Route values are disposable and never confer movement.
    overmap_party_travel::work coarse_work;
    std::optional<tripoint_abs_ms> coarse_work_position;
    overmap_party_travel::route_state coarse_route;
    // Capability binding accompanies paid work; route contents stay transient.
    std::optional<bool> coarse_can_cross_water;
    std::optional<bool> coarse_all_fly;
    // Disposable routing knowledge follows the actual prepared payload copy.
    // Never serialized; no movement/clock credit or second body owner.
    physical_navigation::segment navigation;
    physical_navigation::failure_cache navigation_failures;
    migo_population::identity navigation_identity;
    std::uint64_t navigation_generation = 0;
    member_phase navigation_phase = member_phase::outbound;
    int navigation_owner = -1;
    int navigation_world = -1;
    bool active() const {
        return generation > 0 && phase != member_phase::settled;
    }
    void retreat();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct operation_member {
    std::string member_id;
    bool settled = false;
    bool returned = false;
    std::optional<tripoint_abs_ms> last_position = std::nullopt;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct operation_record {
    migo_population::activity_clock activity;
    std::uint64_t generation = 0;
    signal_record source;
    std::vector<operation_member> members;
    raid_sweep::progress sweep;
    std::int64_t overdue_after_active = 0;
    std::vector<std::string> communicated_losses;
    std::vector<combat_profile> party_at_departure;
    bool resolved = false;
    bool contains( const std::string &member_id ) const;
    void advance( int world_turn, int active_turns );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct site_state {
    std::string site_id;
    tripoint_abs_omt anchor;
    int last_sense_turn = -1;
    std::optional<signal_record> signal;
    // Latest physically grounded estimate. This is not itself a reservation
    // or a dispatch; the native transaction must revalidate exact leases.
    int recognized_enemies = 0;
    admission_decision decision;
    std::uint64_t next_generation = 1;
    std::optional<operation_record> operation;
    std::optional<raid_sweep::home_rally> response_home_rally;
    int last_service_world = -1;
    std::int64_t active_elapsed = 0;
    std::vector<source_history> sources;
    // Fixed-size conservative membership filter for evicted source outcomes.
    // A positive result denies blind replay; it conveys no occupant facts.
    std::array<std::uint64_t, 8> prior_sources{};
    void advance( int world_turn, int active_turns );
    source_history *find_source( const tripoint_abs_omt &source );
    const source_history *find_source( const tripoint_abs_omt &source ) const;
    source_history *remember_source( const tripoint_abs_omt &source );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

struct world_state {
    // Cadence is persisted: query/reload cannot create another observation.
    int last_signal_scan = -1;
    std::vector<site_state> sites;
    site_state *find( const std::string &site_id );
    const site_state *find( const std::string &site_id ) const;
    void clear();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
    private:
        std::size_t indexed_site_count = 0;
        std::map<std::string, std::size_t> site_index;
};
} // namespace migo_tower
#endif

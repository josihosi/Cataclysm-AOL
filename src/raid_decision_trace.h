#pragma once
#ifndef CATA_SRC_RAID_DECISION_TRACE_H
#define CATA_SRC_RAID_DECISION_TRACE_H

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bandit_live_world
{
struct site_record;
}
class Creature;
class Character;

namespace raid_decision_trace
{

// All four values must come from the same harness launch.  The opt-in value
// itself is the run ID, so an inherited flag cannot enable a later game.
struct binding {
    std::string opt_in_run_id;
    std::string run_id;
    std::string semantic_run_id;
    std::string path;

    bool valid() const;
    bool operator==( const binding &other ) const;
};

struct counts {
    std::size_t decisions = 0;
    std::size_t written_rows = 0;
    std::size_t suppressed_repeats = 0;
    bool truncated = false;
};

// This is set only around Creature::deal_damage's call to the actual HP owner.
// Direct damage (fields, effects, explosions) remains explicitly unspecified.
class damage_kind_scope
{
    public:
        damage_kind_scope( const Creature &victim, std::string_view kind );
        ~damage_kind_scope();
    private:
        const Creature *previous_victim;
        std::string_view previous_kind;
};

// The scenario identity and numeric IDs are diagnostic selectors, never NPC
// orders.  An empty/invalid selector cannot broaden the recorder's scope.
struct selected_npc_scope {
    std::string group_id;
    std::vector<int> actor_ids;
    std::optional<int> from_turn;
    std::optional<int> to_turn;

    bool valid() const;
    bool includes( int numeric_npc_id ) const;
    bool includes_turn( int turn ) const;
};

// The caller supplies a JSON object fragment without event/run/turn.  It is
// also the change key: time and move-point refill never prevent compaction.
class recorder
{
    public:
        explicit recorder( binding source, std::size_t row_budget = 1024 );
        ~recorder();
        bool enabled() const;
        void record( const std::string &key, const std::string &event, int turn,
                     const std::string &payload, std::string_view change_signature = {} );
        void record_edge( const std::string &event, int turn, const std::string &payload );
        void closeout();
        counts totals() const;

    private:
        struct last_decision {
            std::string event;
            std::string payload;
            std::string signature;
            int base_turn = -1;
            int first_repeat_turn = -1;
            int last_repeat_turn = -1;
            std::size_t repeats = 0;
        };

        binding source;
        std::size_t row_budget;
        counts tally;
        std::map<std::string, last_decision> last;
        bool closed = false;
        bool normal_truncated = false;
        bool critical_truncated = false;
        std::size_t normal_rows = 0;
        void write( const std::string &row, bool critical = false );
        void flush_repeats( const std::string &key, last_decision &entry );
};

std::string quote( std::string_view value );
bool reserved_local_actor( const bandit_live_world::site_record &site, int numeric_npc_id );
binding environment_binding();
selected_npc_scope environment_selected_npcs();
recorder &native_recorder();
void record_applied_damage( const Creature *source, const Creature &victim,
                            std::string_view body_part, int hp_before, int hp_after );
void record_confirmed_death( const Creature &victim, const Creature *killer );
void record_sleep_edge( const Character &actor, std::string_view edge,
                        std::string_view available_reason );

} // namespace raid_decision_trace

#endif // CATA_SRC_RAID_DECISION_TRACE_H

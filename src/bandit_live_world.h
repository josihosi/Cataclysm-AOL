#pragma once

#include "bandit_dry_run.h"
#include "bandit_pursuit_handoff.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "character_id.h"
#include "coordinates.h"

class JsonObject;
class JsonOut;

namespace bandit_live_world
{
enum class anchor_source_kind {
    none,
    overmap_special,
    map_extra,
};

enum class owned_site_kind {
    none,
    bandit_camp,
    bandit_work_camp,
    bandit_cabin,
    looters,
    bandits_block,
};

enum class hostile_site_profile {
    none,
    camp_style,
    small_hostile_site,
};

enum class member_state {
    at_home,
    outbound,
    local_contact,
    dead,
    missing,
};

enum class active_member_observation_state {
    unresolved,
    local_contact,
    returning_home,
    home,
    dead,
    missing,
};

struct active_member_observation {
    character_id npc_id;
    active_member_observation_state state = active_member_observation_state::unresolved;
    std::string summary;
};

struct member_record {
    character_id npc_id;
    std::string npc_template_id;
    tripoint_abs_ms home_spawn_tile;
    member_state state = member_state::at_home;
    std::string last_writeback_summary;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct spawn_tile_record {
    tripoint_abs_ms tile;
    int headcount = 0;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct site_record {
    std::string site_id;
    anchor_source_kind source_kind = anchor_source_kind::none;
    owned_site_kind site_kind = owned_site_kind::none;
    hostile_site_profile profile = hostile_site_profile::none;
    std::string source_id;
    tripoint_abs_omt anchor;
    int headcount = 0;
    std::vector<tripoint_abs_omt> footprint;
    std::vector<member_record> members;
    std::vector<spawn_tile_record> spawn_tiles;
    std::string active_group_id;
    std::string active_target_id;
    std::vector<character_id> active_member_ids;
    std::string remembered_target_or_mark;
    int remembered_threat_estimate = 0;
    int remembered_bounty_estimate = 0;
    int remembered_retreat_bias = 0;
    int remembered_return_clock = 0;
    bandit_pursuit_handoff::remaining_return_pressure_state remembered_pressure =
        bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    std::vector<std::string> known_recent_marks;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    bool has_member( character_id npc_id ) const;
    member_record *find_member( character_id npc_id );
    const member_record *find_member( character_id npc_id ) const;
    spawn_tile_record *find_spawn_tile( const tripoint_abs_ms &tile );
    const spawn_tile_record *find_spawn_tile( const tripoint_abs_ms &tile ) const;
    int count_members_in_state( member_state state ) const;
    int count_live_members() const;
    int dispatchable_member_capacity() const;
};

struct world_state {
    std::string owner_id = "hells_raiders_live_owner_v0";
    std::vector<site_record> sites;

    void clear();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    site_record *find_site( const std::string &site_id );
    const site_record *find_site( const std::string &site_id ) const;
};

struct footprint_snapshot {
    tripoint_abs_omt anchor;
    std::vector<tripoint_abs_omt> footprint;
};

struct dispatch_plan {
    bool valid = false;
    std::string site_id;
    hostile_site_profile profile = hostile_site_profile::none;
    std::string target_id;
    tripoint_abs_omt target_omt;
    std::vector<character_id> member_ids;
    bandit_dry_run::evaluation_result evaluation;
    bandit_pursuit_handoff::abstract_group_state group;
    bandit_pursuit_handoff::entry_payload entry;
    std::vector<std::string> notes;
};

bool is_tracked_bandit_template( const std::string &npc_template_id );
std::optional<owned_site_kind> classify_tracked_source( anchor_source_kind source_kind,
        const std::string &source_id );
hostile_site_profile profile_for_site_kind( owned_site_kind site_kind );
footprint_snapshot make_special_footprint( const std::string &special_id,
        const tripoint_abs_omt &origin,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup );
std::string make_site_id( anchor_source_kind source_kind, const std::string &source_id,
                          const tripoint_abs_omt &anchor );
bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup );
dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id );
bool apply_dispatch_plan( site_record &site, const dispatch_plan &plan );
bool apply_return_packet( site_record &site, const bandit_pursuit_handoff::return_packet &packet );
std::optional<bandit_pursuit_handoff::return_packet> resolve_active_group_aftermath(
    const site_record &site, const std::vector<active_member_observation> &observations );
bool update_member_state( site_record &site, character_id npc_id, member_state new_state,
                          const std::string &summary );

std::string to_string( anchor_source_kind source_kind );
std::string to_string( owned_site_kind site_kind );
std::string to_string( hostile_site_profile profile );
std::string to_string( member_state state );
std::string to_string( active_member_observation_state state );
} // namespace bandit_live_world

#include "catch/catch.hpp"

#include <string>

#include "ecology_debug_intervention.h"

namespace
{

ecology_debug::immutable_entity_token token()
{
    ecology_debug::immutable_entity_token result;
    result.world_identity = "world";
    result.canonical_id = "dispatch/site/activity/4";
    result.generation = 4;
    result.owner = "local";
    result.authority_token = "bandit_live_world/site/3";
    return result;
}

bandit_live_world::simulation_advance_cursor cursor()
{
    bandit_live_world::simulation_advance_cursor result;
    result.activity_id = "activity";
    result.generation = 4;
    result.owner = bandit_live_world::simulation_owner::local;
    result.handoff_epoch = 2;
    result.last_advanced_minutes = 90;
    return result;
}

ecology_debug::dispatch_member_edit_guard guard()
{
    ecology_debug::dispatch_member_edit_guard result;
    result.entity = token();
    result.member_id = character_id( 17 );
    result.entity_omt = tripoint_abs_omt( 10, 11, 0 );
    result.member_omt = tripoint_abs_omt( 10, 12, 0 );
    result.hp_percent = 72;
    result.cursor = cursor();
    return result;
}

ecology_debug::authoritative_dispatch_member_read read()
{
    ecology_debug::authoritative_dispatch_member_read result;
    result.entity = token();
    result.kind = ecology_debug::entity_kind::bandit_dispatch;
    result.entity_omt = tripoint_abs_omt( 10, 11, 0 );
    result.structural_sortie = true;
    result.local_handoff_active = true;
    result.cursor = cursor();
    result.member_id = character_id( 17 );
    result.member_omt = tripoint_abs_omt( 10, 12, 0 );
    result.hp_percent = 72;
    result.loaded = true;
    result.alive = true;
    return result;
}

ecology_debug::intervention_receipt receipt( int turn )
{
    ecology_debug::intervention_receipt result;
    result.entity = token();
    result.incident.turn = turn;
    result.incident.timestamp = "day 1 00:00";
    result.incident.entity_id = result.entity.canonical_id;
    result.incident.action = "wound member=17 target_hp=50";
    result.incident.before_summary = "member=17 hp=72";
    result.incident.after_summary = "member=17 hp=50";
    result.incident.debug_intervention = true;
    return result;
}

} // namespace

TEST_CASE( "ecology dispatch edit guard rejects every stale authority dimension",
           "[ecology_debug][ecology_intervention]" )
{
    const ecology_debug::dispatch_member_edit_guard expected = guard();
    ecology_debug::authoritative_dispatch_member_read current = read();
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ).empty() );

    current.kind = ecology_debug::entity_kind::cannibal_dispatch;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ).empty() );
    current.kind = ecology_debug::entity_kind::bandit_camp;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "selected_entity_is_not_dispatch" );

    current = read();
    current.structural_sortie = false;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "dispatch_is_inspect_only" );
    current = read();
    current.local_handoff_active = false;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "dispatch_is_inspect_only" );
    current = read();
    current.cursor.handoff_epoch++;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "outing_cursor_stale" );
    current = read();
    current.cursor.activity_id = "replacement";
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "outing_cursor_stale" );
    current = read();
    current.cursor.generation++;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "outing_cursor_stale" );
    current = read();
    current.cursor.last_advanced_minutes++;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "outing_cursor_stale" );
    current = read();
    current.entity.generation++;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "entity_token_stale" );
    current = read();
    current.entity_omt = tripoint_abs_omt( 11, 11, 0 );
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "dispatch_location_stale" );
    current = read();
    current.member_id = character_id( 18 );
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "member_identity_stale" );
    current = read();
    current.loaded = false;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "member_load_or_life_state_stale" );
    current = read();
    current.alive = false;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "member_load_or_life_state_stale" );
    current = read();
    current.member_omt = tripoint_abs_omt( 10, 13, 0 );
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "member_location_stale" );
    current = read();
    current.hp_percent--;
    CHECK( ecology_debug::validate_dispatch_member_edit( expected, current ) ==
           "member_hp_stale" );
}

TEST_CASE( "ecology intervention ledger is bounded deterministic and entity scoped",
           "[ecology_debug][ecology_intervention]" )
{
    ecology_debug::intervention_ledger ledger;
    CHECK( ledger.append( ecology_debug::intervention_receipt() ) == 0 );

    for( size_t index = 0; index < ecology_debug::ecology_intervention_receipt_cap + 3; ++index ) {
        CHECK( ledger.append( receipt( static_cast<int>( index ) ) ) == index + 1 );
    }
    CHECK( ledger.dropped_count() == 3 );
    CHECK( ledger.latest_sequence() == ecology_debug::ecology_intervention_receipt_cap + 3 );

    const std::vector<ecology_debug::incident_intervention> retained =
        ledger.matching_after( token(), ecology_debug::ecology_intervention_receipt_cap );
    REQUIRE( retained.size() == 3 );
    CHECK( retained.front().sequence == ecology_debug::ecology_intervention_receipt_cap + 1 );
    CHECK( retained.back().sequence == ecology_debug::ecology_intervention_receipt_cap + 3 );
    CHECK( ledger.contains_at( "world", token().canonical_id,
                               static_cast<int>( ecology_debug::ecology_intervention_receipt_cap + 2 ) ) );
    CHECK_FALSE( ledger.contains_at( "world", token().canonical_id, 1 ) );

    ecology_debug::immutable_entity_token other = token();
    other.canonical_id += "/other";
    CHECK( ledger.matching_after( other, 0 ).empty() );
}

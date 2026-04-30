#include <optional>

#include "calendar.h"
#include "cata_catch.h"
#include "damage.h"
#include "mongroup.h"
#include "mtype.h"
#include "type_id.h"
#include "writhing_stalker_ai.h"

static const damage_type_id damage_cut( "cut" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );

TEST_CASE( "writhing_stalker_monster_footing", "[writhing_stalker][monster]" )
{
    const mtype &stalker = *mon_writhing_stalker;

    CHECK( stalker.in_species( species_ZOMBIE ) );
    CHECK( stalker.in_species( species_HUMAN ) );
    CHECK( stalker.hp >= 85 );
    CHECK( stalker.hp <= 100 );
    CHECK( stalker.speed >= 115 );
    CHECK( stalker.speed <= 125 );
    CHECK( stalker.sk_dodge >= 4 );
    CHECK( stalker.sk_dodge <= 5 );
    CHECK( stalker.melee_damage.type_damage( damage_cut ) >= 4.0f );
    CHECK( stalker.has_special_attack( "scratch" ) );
    CHECK( stalker.has_special_attack( "bite" ) );
    CHECK( stalker.has_flag( mon_flag_KEENNOSE ) );
    CHECK( stalker.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( stalker.has_flag( mon_flag_HARDTOSHOOT ) );
    CHECK( stalker.has_flag( mon_flag_HIT_AND_RUN ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GRABS ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GROUP_BASH ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::STALK ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::HOSTILE_WEAK ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::HURT ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::BRIGHT_LIGHT ) );
    CHECK_FALSE( stalker.upgrades );
}

TEST_CASE( "writhing_stalker_spawn_footing_is_rare_singleton", "[writhing_stalker][monster][mongroup]" )
{
    const MonsterGroup &group = GROUP_ZOMBIE.obj();

    std::optional<MonsterGroupEntry> stalker_entry;
    int direct_entries = 0;
    int total_direct_weight = 0;
    for( const MonsterGroupEntry &entry : group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        total_direct_weight += entry.frequency;
        if( entry.mtype == mon_writhing_stalker ) {
            stalker_entry = entry;
            direct_entries++;
        }
    }

    REQUIRE( direct_entries == 1 );
    REQUIRE( stalker_entry.has_value() );
    CHECK( stalker_entry->frequency == 1 );
    CHECK( stalker_entry->cost_multiplier >= 50 );
    CHECK( stalker_entry->pack_minimum == 1 );
    CHECK( stalker_entry->pack_maximum == 1 );
    CHECK( stalker_entry->starts == 0_turns );
    CHECK( total_direct_weight > 9000 );
    CHECK( stalker_entry->frequency * 5000 < total_direct_weight );
}

TEST_CASE( "writhing_stalker_interest_sources_are_ranked_without_bandit_economy",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    const interest_report empty = evaluate_interest( interest_context{} );
    interest_context human_ctx;
    human_ctx.recent_human_evidence = true;
    human_ctx.evidence_age_minutes = 4;
    const interest_report human = evaluate_interest( human_ctx );

    CHECK( human.source == interest_source::human );
    CHECK( human.can_latch );
    CHECK( human.confidence >= 4 );
    CHECK( human.score > empty.score );
    CHECK( human.reason == "recent_human_evidence" );

    interest_context smoke_ctx;
    smoke_ctx.smoke = true;
    const interest_report smoke = evaluate_interest( smoke_ctx );
    CHECK( smoke.source == interest_source::smoke );
    CHECK_FALSE( smoke.can_latch );
    CHECK( smoke.score < human.score / 4 );
    CHECK( smoke.reason == "smoke_indirect_human_hint" );

    interest_context light_ctx;
    light_ctx.exposed_night_light = true;
    const interest_report light = evaluate_interest( light_ctx );
    CHECK( light.source == interest_source::light );
    CHECK_FALSE( light.can_latch );
    CHECK( light.score >= 50 );
    CHECK( light.score > smoke.score * 3 );

    interest_context terrain_ctx;
    terrain_ctx.forest_or_building_edge = true;
    const interest_report terrain = evaluate_interest( terrain_ctx );
    CHECK( terrain.source == interest_source::terrain );
    CHECK( terrain.score > empty.score );
    CHECK_FALSE( terrain.can_latch );
}

TEST_CASE( "writhing_stalker_latch_is_bounded_and_refresh_requires_evidence",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    interest_context human_ctx;
    human_ctx.recent_human_evidence = true;
    human_ctx.evidence_age_minutes = 3;
    latch_update latched = advance_latch( latch_context{ latch_state{},
            evaluate_interest( human_ctx ), 0, 12, false } );

    REQUIRE( latched.state.active );
    CHECK( latched.next == decision::shadow );
    CHECK( latched.state.leash_tiles_remaining == 60 );
    CHECK( latched.reason == "latch_created_by_believable_evidence" );

    latch_update continuing = advance_latch( latch_context{ latched.state,
            interest_report{}, 12, 20, false } );
    CHECK( continuing.state.active );
    CHECK( continuing.state.age_minutes == 12 );
    CHECK( continuing.reason == "latch_continues_without_refresh" );

    latch_update timed_out = advance_latch( latch_context{ continuing.state,
            interest_report{}, 20, 20, false } );
    CHECK_FALSE( timed_out.state.active );
    CHECK( timed_out.next == decision::cooling_off );
    CHECK( timed_out.state.cooldown_minutes > 0 );
    CHECK( timed_out.reason == "latch_timed_out" );

    human_ctx.evidence_age_minutes = 90;
    latch_update stale = advance_latch( latch_context{ latch_state{},
            evaluate_interest( human_ctx ), 0, 10, false } );
    CHECK_FALSE( stale.state.active );
    CHECK( stale.next == decision::interested );
    CHECK( stale.reason == "interest_without_latch" );
}

TEST_CASE( "writhing_stalker_approach_prefers_cover_and_holds_at_exposure",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    approach_context cover_ctx;
    cover_ctx.latch = latch;
    cover_ctx.cover_route_available = true;
    cover_ctx.direct_open_route_available = true;
    const approach_report cover = choose_approach( cover_ctx );
    CHECK( cover.route == approach_class::cover_shadow );
    CHECK( cover.next == decision::shadow );
    CHECK( cover.avoids_direct_line );
    CHECK( cover.reason == "cover_route_preferred" );

    approach_context exposed_ctx;
    exposed_ctx.latch = latch;
    exposed_ctx.cover_route_available = true;
    exposed_ctx.bright_exposure = true;
    const approach_report exposed = choose_approach( exposed_ctx );
    CHECK( exposed.route == approach_class::hold_exposed );
    CHECK( exposed.next == decision::hold );
    CHECK( exposed.avoids_direct_line );
    CHECK( exposed.reason == "bright_exposure_holds_shadow" );

    approach_context forced_ctx;
    forced_ctx.latch = latch;
    forced_ctx.direct_open_route_available = true;
    forced_ctx.forced_no_cover = true;
    const approach_report forced = choose_approach( forced_ctx );
    CHECK( forced.route == approach_class::direct_forced );
    CHECK_FALSE( forced.avoids_direct_line );
    CHECK( forced.reason == "direct_route_only_when_forced_by_map" );
}

TEST_CASE( "writhing_stalker_opportunity_uses_zombies_without_magic_tracking",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state no_latch;
    opportunity_context zombies_without_latch;
    zombies_without_latch.latch = no_latch;
    zombies_without_latch.zombie_pressure = 6;
    zombies_without_latch.player_bleeding = true;
    const opportunity_report ignored = evaluate_opportunity( zombies_without_latch );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "no_latch_no_strike" );

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    opportunity_context calm_ctx;
    calm_ctx.latch = latch;
    calm_ctx.near_cover_or_clutter = true;
    const opportunity_report calm = evaluate_opportunity( calm_ctx );
    CHECK( calm.next == decision::hold );

    opportunity_context zombie_ctx = calm_ctx;
    zombie_ctx.zombie_pressure = 3;
    const opportunity_report zombie = evaluate_opportunity( zombie_ctx );
    CHECK( zombie.zombie_distraction > calm.zombie_distraction );
    CHECK( zombie.opportunity > calm.opportunity );
    CHECK( zombie.next != decision::strike );

    opportunity_context vulnerable_ctx = zombie_ctx;
    vulnerable_ctx.player_bleeding = true;
    vulnerable_ctx.player_hurt = true;
    vulnerable_ctx.player_low_stamina = true;
    vulnerable_ctx.player_distracted = true;
    const opportunity_report vulnerable = evaluate_opportunity( vulnerable_ctx );
    CHECK( vulnerable.vulnerability > calm.vulnerability );
    CHECK( vulnerable.next == decision::strike );
    CHECK( vulnerable.reason == "vulnerability_window_strike" );
}

TEST_CASE( "writhing_stalker_withdrawal_and_cooldown_block_repeat_spam",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    opportunity_context exposed_ctx;
    exposed_ctx.latch = latch;
    exposed_ctx.bright_exposure = true;
    exposed_ctx.player_focused = true;
    const opportunity_report exposed = evaluate_opportunity( exposed_ctx );
    CHECK( exposed.next == decision::withdraw );
    CHECK( exposed.reason == "exposed_and_focused_withdraw" );

    opportunity_context hurt_ctx;
    hurt_ctx.latch = latch;
    hurt_ctx.player_bleeding = true;
    hurt_ctx.player_hurt = true;
    hurt_ctx.player_low_stamina = true;
    hurt_ctx.player_distracted = true;
    hurt_ctx.stalker_hurt = true;
    const opportunity_report hurt = evaluate_opportunity( hurt_ctx );
    CHECK( hurt.next == decision::withdraw );
    CHECK( hurt.reason == "stalker_hurt_withdraw" );

    latch.cooldown_minutes = 8;
    opportunity_context cooldown_ctx;
    cooldown_ctx.latch = latch;
    cooldown_ctx.player_bleeding = true;
    cooldown_ctx.player_hurt = true;
    cooldown_ctx.player_low_stamina = true;
    cooldown_ctx.player_distracted = true;
    const opportunity_report cooldown = evaluate_opportunity( cooldown_ctx );
    CHECK( cooldown.next == decision::cooling_off );
    CHECK( cooldown.reason == "cooldown_blocks_repeat_strike" );
}

TEST_CASE( "writhing_stalker_live_response_requires_evidence_and_uses_cooldown",
           "[writhing_stalker][ai][live]" )
{
    using namespace writhing_stalker;

    live_context no_evidence;
    no_evidence.distance_to_target = 8;
    no_evidence.player_bleeding = true;
    no_evidence.player_hurt = true;
    no_evidence.player_low_stamina = true;
    no_evidence.player_distracted = true;
    const live_response ignored = evaluate_live_response( no_evidence );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "live_no_believable_evidence" );

    live_context shadow_ctx;
    shadow_ctx.has_believable_local_evidence = true;
    shadow_ctx.distance_to_target = 8;
    shadow_ctx.cover_route_available = true;
    shadow_ctx.near_cover_or_clutter = true;
    const live_response shadow = evaluate_live_response( shadow_ctx );
    CHECK( shadow.next == decision::shadow );
    CHECK( shadow.route == approach_class::cover_shadow );
    CHECK( shadow.reason == "live_shadowing_believable_evidence" );

    live_context strike_ctx = shadow_ctx;
    strike_ctx.player_bleeding = true;
    strike_ctx.player_hurt = true;
    strike_ctx.player_low_stamina = true;
    strike_ctx.player_distracted = true;
    strike_ctx.zombie_pressure = 2;
    const live_response strike = evaluate_live_response( strike_ctx );
    CHECK( strike.next == decision::strike );
    CHECK( strike.reason == "live_vulnerability_window_strike" );

    live_context cooldown_ctx = strike_ctx;
    cooldown_ctx.on_cooldown = true;
    const live_response cooldown = evaluate_live_response( cooldown_ctx );
    CHECK( cooldown.next == decision::cooling_off );
    CHECK( cooldown.reason == "live_latch_cooldown_blocks_relatched_pressure" );
}

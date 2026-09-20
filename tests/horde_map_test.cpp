#include "horde_map.h"

#include "cata_catch.h"
#include "coordinates.h"
#include "effect.h"
#include "map.h"
#include "monster.h"
#include "mtype.h"
#include "rng.h"

static const mtype_id mon_pseudo_dormant_zombie( "mon_pseudo_dormant_zombie" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const mtype_id mon_zombie_rider( "mon_zombie_rider" );
static const efftype_id effect_blind( "blind" );

static tripoint_om_ms random_location()
{
    return tripoint_om_ms( rng( 0, 180 ), rng( 0, 180 ), 0 );
}

static tripoint_abs_ms random_abs_location( horde_map &test_horde )
{
    return project_combine( test_horde.get_location(), random_location() );
}

// Simple dumb placement code because these tests mostly don't care where the entities end up.
static tripoint_om_ms pick_available_location( horde_map &test_horde )
{
    tripoint_om_ms candidate;
    do {
        candidate = random_location();
    } while( test_horde.entity_at( candidate ) != nullptr );
    return candidate;
}

static int count_entities( horde_map &test_horde, int filter )
{
    int entity_count = 0;
    for( [[maybe_unused]]std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde.get_view(
             filter ) ) {
        entity_count++;
    }
    return entity_count;
}

static void place_entity( horde_map &test_horde, mtype_id id )
{
    tripoint_om_ms monster_relative = pick_available_location( test_horde );
    tripoint_abs_ms monster_location( project_combine( test_horde.get_location(), monster_relative ) );
    test_horde.spawn_entity( monster_location, id );
    REQUIRE( test_horde.entity_at( monster_relative ) != nullptr );
}

static void place_monster_as_entity( horde_map &test_horde, monster &mon )
{
    tripoint_om_ms monster_relative = pick_available_location( test_horde );
    tripoint_abs_ms monster_location( project_combine( test_horde.get_location(), monster_relative ) );
    test_horde.spawn_entity( monster_location, mon );
    REQUIRE( test_horde.entity_at( monster_relative ) != nullptr );
}

TEST_CASE( "predator horde payload is lazy and retains its opaque identity", "[hordes][predator]" )
{
    horde_map test_horde;
    test_horde.set_location( point_abs_om( 42, 42 ) );
    const tripoint_abs_ms stalker_pos = random_abs_location( test_horde );
    point_abs_om ignored_om;
    tripoint_om_ms local_stalker_pos;
    std::tie( ignored_om, local_stalker_pos ) = project_remain<coords::om>( stalker_pos );
    REQUIRE( test_horde.spawn_entity( stalker_pos, mon_writhing_stalker ) );
    horde_entity *stalker = test_horde.entity_at( local_stalker_pos );
    REQUIRE( stalker != nullptr );
    CHECK( stalker->monster_data == nullptr );

    stalker->ensure_predator_payload( stalker_pos );
    REQUIRE( stalker->monster_data != nullptr );
    const std::string actor_id = stalker->monster_data->predator_state().actor_id;
    CHECK_FALSE( actor_id.empty() );
    CHECK( stalker->monster_data->pos_abs() == stalker_pos );

    monster rider( mon_zombie_rider );
    const tripoint_abs_ms rider_pos = random_abs_location( test_horde );
    rider.set_moves( 37 );
    rider.set_hp( 113 );
    rider.ammo[itype_id( "zombie_rider_tainted_bone_arrow" )] = 3;
    rider.add_effect( effect_blind, 10_turns );
    rider.zombie_rider_pursuit_state().observe( 42, rider_pos, 77 );
    rider.predator_state().ammo_initialization_version = 5;
    rider.predator_state().band_reference = "opaque-band-cache";
    rider.predator_state().band_revision_cache = 12;
    const std::string rider_id = rider.predator_state().actor_id;
    REQUIRE( test_horde.spawn_entity( rider_pos, rider ) );
    tripoint_om_ms local_rider_pos;
    std::tie( ignored_om, local_rider_pos ) = project_remain<coords::om>( rider_pos );
    horde_entity *heavy_rider = test_horde.entity_at( local_rider_pos );
    REQUIRE( heavy_rider != nullptr );
    REQUIRE( heavy_rider->monster_data != nullptr );
    CHECK( heavy_rider->monster_data->predator_state().actor_id == rider_id );
    CHECK( heavy_rider->monster_data->get_moves() == 37 );
    CHECK( heavy_rider->monster_data->get_hp() == 113 );
    CHECK( heavy_rider->monster_data->ammo[itype_id( "zombie_rider_tainted_bone_arrow" )] == 3 );
    CHECK( heavy_rider->monster_data->has_effect( effect_blind ) );
    const zombie_rider_overmap_ai::rider_pursuit_state &rider_state =
        heavy_rider->monster_data->zombie_rider_pursuit_state();
    CHECK( rider_state.phase == zombie_rider_overmap_ai::pursuit_phase::pursuing );
    CHECK( rider_state.target_identity == 42 );
    CHECK( rider_state.has_last_observed_position );
    CHECK( rider_state.last_observed_position == rider_pos );
    CHECK( rider_state.last_observed_turn == 77 );
    CHECK( rider_state.has_movement_waypoint );
    CHECK( rider_state.movement_waypoint == rider_pos );
    CHECK( heavy_rider->monster_data->predator_state().ammo_initialization_version == 5 );
    CHECK( heavy_rider->monster_data->predator_state().band_reference == "opaque-band-cache" );
    CHECK( heavy_rider->monster_data->predator_state().band_revision_cache == 12 );

    stalker->light_source = stalker_pos + point_rel_ms( 12, 0 );
    stalker->light_sample_id = "legitimate-light-sample";
    stalker->light_observed = calendar::turn;
    stalker->light_expires = calendar::turn + 10_turns;
    CHECK( stalker->advance_predator_intent( stalker_pos, to_turn<int>( calendar::turn ) ) );
    CHECK( stalker->destination == stalker->light_source );
    CHECK( stalker->monster_data->writhing_stalker_state().has_light_observed_position );
    // The lifecycle stamp makes a repeated delivery in the same game turn a
    // no-op; the common scheduler's last_processed guard supplies the same
    // protection for the actual movement budget.
    const int original_interest = stalker->tracking_intensity;
    CHECK( stalker->advance_predator_intent( stalker_pos, to_turn<int>( calendar::turn ) ) );
    CHECK( stalker->tracking_intensity == original_interest );

    // A heavy rider uses the same legitimate finite horde light observation
    // to progress while abstract.  It remains an area investigation, never a
    // fabricated direct prey observation.
    heavy_rider->light_source = rider_pos + point_rel_ms( 12, 0 );
    heavy_rider->light_sample_id = "legitimate-rider-light-sample";
    heavy_rider->light_observed = calendar::turn;
    heavy_rider->light_expires = calendar::turn + 10_turns;
    CHECK( heavy_rider->advance_predator_intent( rider_pos, to_turn<int>( calendar::turn ) ) );
    CHECK( heavy_rider->destination == heavy_rider->light_source );
    CHECK( heavy_rider->tracking_intensity > 0 );
    CHECK( heavy_rider->monster_data->zombie_rider_pursuit_state().phase ==
           zombie_rider_overmap_ai::pursuit_phase::searching );
    CHECK( heavy_rider->monster_data->zombie_rider_pursuit_state().has_movement_waypoint );
    CHECK( heavy_rider->monster_data->zombie_rider_pursuit_state().movement_waypoint ==
           heavy_rider->light_source );
}

TEST_CASE( "ordinary upgradeable horde evolution remains lightweight", "[hordes][evolution]" )
{
    horde_map test_horde;
    test_horde.set_location( point_abs_om( 42, 42 ) );
    horde_entity ordinary( mon_zombie );
    REQUIRE( ordinary.get_type() != nullptr );
    REQUIRE( ordinary.get_type()->upgrades );
    CHECK( ordinary.monster_data == nullptr );
    CHECK_FALSE( ordinary.advance_evolution( random_abs_location( test_horde ) ) );
    CHECK( ordinary.monster_data == nullptr );
}

TEST_CASE( "predator abstract owner rejects a duplicate handoff identity", "[hordes][predator]" )
{
    horde_map test_horde;
    test_horde.set_location( point_abs_om( 42, 42 ) );

    for( const mtype_id &type : { mon_writhing_stalker, mon_zombie_rider } ) {
        monster source( type );
        const std::string actor_id = source.predator_state().actor_id;
        const tripoint_abs_ms first = random_abs_location( test_horde );
        const tripoint_abs_ms second = random_abs_location( test_horde );
        REQUIRE( first != second );
        REQUIRE( test_horde.spawn_entity( first, source ) );

        // A repeated local->abstract delivery has the same durable ID and
        // epoch.  It must leave the first abstract owner intact rather than
        // acknowledge a second owner at another position.
        CHECK_FALSE( test_horde.spawn_entity( second, source ) );
        point_abs_om ignored_om;
        tripoint_om_ms first_local;
        std::tie( ignored_om, first_local ) = project_remain<coords::om>( first );
        horde_entity *owner = test_horde.entity_at( first_local );
        REQUIRE( owner != nullptr );
        REQUIRE( owner->monster_data != nullptr );
        CHECK( owner->monster_data->predator_state().actor_id == actor_id );
        CHECK( count_entities( test_horde, horde_map_flavors::active |
                               horde_map_flavors::idle | horde_map_flavors::dormant |
                               horde_map_flavors::immobile ) == 1 );
        test_horde.clear();
    }
}

TEST_CASE( "predator failed local handoff leaves its abstract owner uncommitted",
           "[hordes][predator]" )
{
    horde_map test_horde;
    test_horde.set_location( point_abs_om( 42, 42 ) );

    for( const mtype_id &type : { mon_writhing_stalker, mon_zombie_rider } ) {
        const tripoint_abs_ms source = random_abs_location( test_horde );
        const tripoint_abs_ms destination = source + point_rel_ms( 1, 0 );
        monster original( type );
        original.set_moves( 47 );
        REQUIRE( test_horde.spawn_entity( source, original ) );
        point_abs_om ignored_om;
        tripoint_om_ms source_local;
        std::tie( ignored_om, source_local ) = project_remain<coords::om>( source );
        horde_entity *retained = test_horde.entity_at( source_local );
        REQUIRE( retained != nullptr );
        retained->tracking_intensity = 9;
        retained->destination = destination;
        retained->ensure_predator_payload( source );
        REQUIRE( retained->monster_data != nullptr );
        const std::string actor_id = retained->monster_data->predator_state().actor_id;
        const std::uint64_t epoch = retained->monster_data->predator_state().handoff_epoch;
        const int moves = retained->moves;

        // This is the same prepare/reserve work as the local exact-placement
        // route.  If placement is rejected, only this disposable copy is
        // discarded; the retained node remains the sole owner.
        horde_entity prepared( *retained );
        --prepared.tracking_intensity;
        prepared.moves += prepared.type_id->speed - 100;
        prepared.synchronize_payload( destination );
        ++prepared.monster_data->predator_state().handoff_epoch;

        CHECK( retained->tracking_intensity == 9 );
        CHECK( retained->moves == moves );
        CHECK( retained->monster_data->pos_abs() == source );
        CHECK( retained->monster_data->predator_state().actor_id == actor_id );
        CHECK( retained->monster_data->predator_state().handoff_epoch == epoch );
        CHECK( prepared.monster_data->predator_state().actor_id == actor_id );
        CHECK( prepared.monster_data->predator_state().handoff_epoch == epoch + 1 );
        test_horde.clear();
    }
}

/*
 * The main things to test here are the automatic filtering based on monster attributes and
 * simple round-tripping.
 * Specifically dormant monsters, idle monsters (with no current destination) and
 * active monsters (that DO have a current destination) are handled separately.
*/
TEST_CASE( "horde_map_insertion_and_retrieval", "[hordes]" )
{
    horde_map test_horde;
    point_abs_om om_origin( 42, 42 );
    test_horde.set_location( om_origin );

    monster wandering_monster( mon_zombie );
    wandering_monster.wander_to( random_abs_location( test_horde ), 100 );
    place_monster_as_entity( test_horde, wandering_monster );

    monster targeted_monster( mon_zombie );
    targeted_monster.set_dest( random_abs_location( test_horde ) );
    place_monster_as_entity( test_horde, targeted_monster );

    monster idle_monster( mon_zombie );
    place_monster_as_entity( test_horde, idle_monster );

    place_entity( test_horde, mon_zombie );
    place_entity( test_horde, mon_zombie );
    place_entity( test_horde, mon_pseudo_dormant_zombie );

    int entity_count = 0;
    for( [[maybe_unused]]std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde ) {
        entity_count++;
    }
    CHECK( entity_count == 6 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 2 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 3 );
    CHECK( count_entities( test_horde, horde_map_flavors::dormant ) == 1 );
    CHECK( count_entities( test_horde, horde_map_flavors::active | horde_map_flavors::idle ) == 5 );
    CHECK( count_entities( test_horde, horde_map_flavors::active | horde_map_flavors::dormant ) == 3 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle | horde_map_flavors::dormant ) == 4 );

    // Remove an idle and a dormant entity, give them a goal, and re-insert them.
    // TODO: This stays dormant! Need to add support for them changing.
    horde_map::node_type dormant_node = test_horde.extract( test_horde.get_view(
                                            horde_map_flavors::dormant ).begin() );
    dormant_node.mapped().tracking_intensity = 100;
    dormant_node.mapped().destination = random_abs_location( test_horde );;
    test_horde.insert( std::move( dormant_node ) );

    horde_map::node_type idle_node = test_horde.extract( test_horde.get_view(
                                         horde_map_flavors::idle ).begin() );
    idle_node.mapped().tracking_intensity = 100;
    idle_node.mapped().destination = random_abs_location( test_horde );;
    test_horde.insert( std::move( idle_node ) );

    entity_count = 0;
    for( [[maybe_unused]]std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde ) {
        entity_count++;
    }
    CHECK( entity_count == 6 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 3 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 2 );
    CHECK( count_entities( test_horde, horde_map_flavors::dormant ) == 1 );
    CHECK( count_entities( test_horde, horde_map_flavors::active | horde_map_flavors::idle ) == 5 );
    CHECK( count_entities( test_horde, horde_map_flavors::active | horde_map_flavors::dormant ) == 4 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle | horde_map_flavors::dormant ) == 3 );
}

TEST_CASE( "horde_map_refreshes_restored_route_into_active_bucket", "[hordes][persistence]" )
{
    horde_map test_horde;
    test_horde.set_location( point_abs_om( 42, 42 ) );
    const tripoint_abs_ms location = random_abs_location( test_horde );
    REQUIRE( test_horde.spawn_entity( location, mon_writhing_stalker ) );
    REQUIRE( count_entities( test_horde, horde_map_flavors::idle ) == 1 );
    point_abs_om ignored_om;
    tripoint_om_ms local;
    std::tie( ignored_om, local ) = project_remain<coords::om>( location );
    horde_entity *restored = test_horde.entity_at( local );
    REQUIRE( restored != nullptr );
    restored->destination = location + point_rel_ms( 12, 0 );
    restored->tracking_intensity = 1000;
    test_horde.refresh_entity_bucket( location );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 1 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 0 );
}

TEST_CASE( "horde_map_corner_cases", "[hordes]" )
{
    // Make sure iterator handling is ok with empty container.
    horde_map test_horde;
    for( [[maybe_unused]]std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde ) {
        FAIL( "Unreachable loop entered, should not happen with empty horde_map." );
    }
    // Populated container but accessed in a way that filters out everything.
    place_entity( test_horde, mon_zombie );
    for( [[maybe_unused]]std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde.get_view(
             horde_map_flavors::active ) ) {
        FAIL( "Unreachable loop entered, should not happen with empty horde_map." );
    }

}

TEST_CASE( "horde_map_signal_entities_culls_empty_idle_buckets", "[hordes]" )
{
    horde_map test_horde;
    const point_abs_om om_origin( 42, 42 );
    test_horde.set_location( om_origin );
    const tripoint_om_ms relative_source( 10, 10, 0 );
    const tripoint_abs_ms signal_source = project_combine( om_origin, relative_source );

    test_horde.spawn_entity( signal_source, mon_zombie );
    REQUIRE( count_entities( test_horde, horde_map_flavors::idle ) == 1 );
    REQUIRE( count_entities( test_horde, horde_map_flavors::active ) == 0 );

    test_horde.signal_entities( signal_source, 100 );

    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 0 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 1 );
    CHECK( count_entities( test_horde, horde_map_flavors::active | horde_map_flavors::idle ) == 1 );
}

TEST_CASE( "horde_map_light_interest_uses_a_source_envelope_without_sound_signalling",
           "[hordes][light]" )
{
    horde_map test_horde;
    const point_abs_om om_origin( 42, 42 );
    test_horde.set_location( om_origin );
    const tripoint_abs_ms source = project_combine( om_origin, tripoint_om_ms( 60, 60, 0 ) );
    const tripoint_abs_ms near = source + point_rel_ms( 24, 0 );
    const tripoint_abs_ms far = source + point_rel_ms( 60, 0 );
    const tripoint_abs_ms dormant = source + point_rel_ms( 12, 0 );
    const tripoint_abs_ms friendly = source + point_rel_ms( 18, 0 );
    const tripoint_abs_ms blind = source + point_rel_ms( 0, 18 );
    const tripoint_abs_ms occluded = source + point_rel_ms( 0, 24 );

    test_horde.spawn_entity( near, mon_zombie );
    test_horde.spawn_entity( far, mon_zombie );
    test_horde.spawn_entity( dormant, mon_pseudo_dormant_zombie );
    monster friendly_monster( mon_zombie );
    friendly_monster.friendly = 1;
    test_horde.spawn_entity( friendly, friendly_monster );
    monster blind_monster( mon_zombie );
    blind_monster.add_effect( effect_blind, 10_turns );
    test_horde.spawn_entity( blind, blind_monster );
    test_horde.spawn_entity( occluded, mon_zombie );
    // These are deliberately ordinary eligible hordes, but their buckets are
    // far outside the source envelope.  A source-local scan must never count
    // or inspect this population.
    for( int i = 0; i < 64; ++i ) {
        test_horde.spawn_entity( source + point_rel_ms( 240 + i * 12, 120 ), mon_zombie );
    }

    int candidates = 0;
    const auto visible = [&occluded]( const tripoint_abs_ms &position ) {
        return position != occluded;
    };
    CHECK( test_horde.attract_entities_to_light( source, 6, 4, "light@40", calendar::turn,
            visible, &candidates ) == 1 );
    CHECK( candidates == 4 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 1 );
    // The far, friendly, blind, occluded, and 64 distant controls remain idle.
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 68 );
    CHECK( count_entities( test_horde, horde_map_flavors::dormant ) == 1 );

    horde_entity *attracted = nullptr;
    for( std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde ) {
        if( entity.first == near ) {
            attracted = &entity.second;
        }
    }
    REQUIRE( attracted != nullptr );
    CHECK( attracted->destination == source );
    CHECK( attracted->tracking_intensity == 4 );
    CHECK( attracted->light_sample_id == "light@40" );
    CHECK( attracted->light_source == source );
    CHECK( attracted->light_expires > calendar::turn );
    // Replaying the same physical sample does not refresh a stronger or equal
    // interest record.  It also leaves the out-of-envelope and dormant
    // controls untouched.
    CHECK( test_horde.attract_entities_to_light( source, 6, 4, "light@40", calendar::turn,
            visible, &candidates ) == 0 );
    CHECK( attracted->tracking_intensity == 4 );
    // Source removal/depletion/unload produces no new sample.  Normal horde
    // processing expires this finite record rather than refreshing its old
    // location or adding a light-only movement pass.
    test_horde.expire_light_interest( attracted->light_expires );
    horde_entity *expired = nullptr;
    for( std::pair<const tripoint_abs_ms, horde_entity> &entity : test_horde ) {
        if( entity.first == near ) {
            expired = &entity.second;
        }
    }
    REQUIRE( expired != nullptr );
    CHECK( expired->tracking_intensity == 0 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 0 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 69 );
}

TEST_CASE( "horde_map_light_interest_leaves_no_eligible_observer_idle", "[hordes][light]" )
{
    horde_map test_horde;
    const point_abs_om om_origin( 42, 42 );
    test_horde.set_location( om_origin );
    const tripoint_abs_ms source = project_combine( om_origin, tripoint_om_ms( 60, 60, 0 ) );
    monster friendly_monster( mon_zombie );
    friendly_monster.friendly = 1;
    monster blind_monster( mon_zombie );
    blind_monster.add_effect( effect_blind, 10_turns );
    test_horde.spawn_entity( source + point_rel_ms( 12, 0 ), friendly_monster );
    test_horde.spawn_entity( source + point_rel_ms( 0, 12 ), blind_monster );
    test_horde.spawn_entity( source + point_rel_ms( 18, 0 ), mon_pseudo_dormant_zombie );

    int candidates = 0;
    CHECK( test_horde.attract_entities_to_light( source, 6, 4, "light@none", calendar::turn,
            {}, &candidates ) == 0 );
    CHECK( candidates == 2 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 0 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 2 );
    CHECK( count_entities( test_horde, horde_map_flavors::dormant ) == 1 );
}

TEST_CASE( "horde_map_light_interest_excludes_a_dead_heavy_entity", "[hordes][light]" )
{
    horde_map test_horde;
    const point_abs_om om_origin( 42, 42 );
    test_horde.set_location( om_origin );
    const tripoint_abs_ms source = project_combine( om_origin, tripoint_om_ms( 60, 60, 0 ) );
    monster dead_monster( mon_zombie );
    dead_monster.die( &get_map(), nullptr );
    REQUIRE( dead_monster.is_dead() );
    test_horde.spawn_entity( source + point_rel_ms( 12, 0 ), dead_monster );

    CHECK( test_horde.attract_entities_to_light( source, 6, 4, "light@dead", calendar::turn ) == 0 );
    CHECK( count_entities( test_horde, horde_map_flavors::active ) == 0 );
    CHECK( count_entities( test_horde, horde_map_flavors::idle ) == 1 );
}

TEST_CASE( "horde_map_sound_signal_does_not_create_light_memory", "[hordes][light][sound]" )
{
    horde_map test_horde;
    const point_abs_om om_origin( 42, 42 );
    test_horde.set_location( om_origin );
    const tripoint_om_ms local_position( 60, 60, 0 );
    const tripoint_abs_ms position = project_combine( om_origin, local_position );
    test_horde.spawn_entity( position, mon_zombie );

    test_horde.signal_entities( position + point_rel_ms( 24, 0 ), 100 );
    horde_entity *signalled = test_horde.entity_at( local_position );
    REQUIRE( signalled != nullptr );
    CHECK( signalled->tracking_intensity > 0 );
    CHECK( signalled->light_sample_id.empty() );
    CHECK( signalled->light_expires == calendar::turn_zero );
    CHECK( signalled->light_interest_strength == 0 );
}

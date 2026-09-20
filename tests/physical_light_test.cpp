#include <algorithm>
#include <stdexcept>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "do_turn.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "mapdata.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster_helpers.h"
#include "mtype.h"
#include "physical_light.h"
#include "player_helpers.h"
#include "pocket_type.h"
#include "vehicle.h"
#include "veh_type.h"

namespace
{
item powered_flashlight( const int charges = 56 )
{
    item battery( itype_id( "medium_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), charges );
    item light( itype_id( "flashlight_on" ) );
    light.put_in( battery, pocket_type::MAGAZINE_WELL );
    return light;
}

void setup_dark_native_lightmap()
{
    clear_avatar();
    clear_map_without_vision();
    calendar::turn = calendar::turn_zero;
    get_avatar().recalc_sight_limits();
}

void rebuild_native_lightmap()
{
    map &here = get_map();
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
}
}

TEST_CASE( "physical_light_collects_carried_and_stationary_sources_independently", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    here.add_item( origin + tripoint::east, powered_flashlight() );

    item_location carried = guy.try_add( powered_flashlight() );
    REQUIRE( carried != item_location::nowhere );
    const int before = carried->ammo_remaining();
    const std::vector<physical_light::emitter> emitters =
        physical_light::collect_item_emitters( guy, here, 3 );

    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::carried_item;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::ground_item;
    } ) == 1 );
    CHECK( carried->ammo_remaining() == before );
}

TEST_CASE( "physical_light_loaded_z_index_finds_remote_loaded_level_and_excludes_depleted_source",
           "[physical_light][continuity]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms remote_loaded_source = guy.pos_bub() + tripoint_rel_ms( 4, 0, 1 );
    REQUIRE( here.inbounds( remote_loaded_source ) );
    const tripoint_bub_ms depleted_source = guy.pos_bub() + tripoint_rel_ms( 5, 0, 1 );
    // clear_map_without_vision() intentionally clears only through z=0; this
    // test owns z=1 items and must remove them for repeated Catch fuzz runs.
    here.i_clear( remote_loaded_source );
    here.i_clear( depleted_source );
    on_out_of_scope source_cleanup( [&here, remote_loaded_source, depleted_source]() {
        here.i_clear( remote_loaded_source );
        here.i_clear( depleted_source );
    } );
    here.add_item( remote_loaded_source, powered_flashlight() );

    item depleted( itype_id( "flashlight_on" ) );
    item empty_battery( itype_id( "medium_battery_cell" ) );
    depleted.put_in( empty_battery, pocket_type::MAGAZINE_WELL );
    here.add_item( depleted_source, depleted );

    const physical_light::loaded_z_source_index index =
        physical_light::index_loaded_z_sources( guy, here );
    const tripoint_abs_ms remote_abs = here.get_abs( remote_loaded_source );
    const tripoint_abs_ms depleted_abs = here.get_abs( depleted_source );
    CHECK( std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
    [&remote_abs]( const physical_light::emitter &emitter ) {
        return emitter.position == remote_abs;
    } ) == 1 );
    CHECK( std::none_of( index.item_emitters.begin(), index.item_emitters.end(),
    [&depleted_abs]( const physical_light::emitter &emitter ) {
        return emitter.position == depleted_abs;
    } ) );
}

TEST_CASE( "physical_light_loaded_z_index_tracks_beside_onto_and_away_movement",
           "[physical_light][continuity]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms beside = origin + tripoint::east;
    const tripoint_bub_ms away = origin + tripoint::west;
    const tripoint_abs_ms fixture_abs = here.get_abs( origin );
    ter_t &terrain = const_cast<ter_t &>( here.ter( origin ).obj() );
    restore_on_out_of_scope terrain_restore( terrain.light_emitted );
    on_out_of_scope fixture_cleanup( [&here, &origin]() {
        here.i_clear( origin );
    } );
    terrain.light_emitted = 31;
    here.add_item( origin, powered_flashlight() );
    guy.setpos( here, beside );

    const auto beside_index = physical_light::index_loaded_z_sources( guy, here );
    const auto check_fixture = [&fixture_abs]( const physical_light::loaded_z_source_index &index ) {
        const auto terrain_source = std::find_if( index.stationary_emitters.begin(),
        index.stationary_emitters.end(), [&fixture_abs]( const physical_light::emitter &emitter ) {
            return emitter.kind == physical_light::source_kind::terrain &&
                   emitter.position == fixture_abs;
        } );
        REQUIRE( terrain_source != index.stationary_emitters.end() );
        CHECK( terrain_source->provenance.rfind( "terrain:", 0 ) == 0 );
        CHECK( terrain_source->luminance == 31 );
        return std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
        [&fixture_abs]( const physical_light::emitter &emitter ) {
            return emitter.kind == physical_light::source_kind::ground_item &&
                   emitter.position == fixture_abs && emitter.provenance == "flashlight_on";
        } );
    };
    const auto check_both_discovery_routes = [&here, &origin, &fixture_abs,
            &check_fixture]( const physical_light::loaded_z_source_index &index ) {
        CHECK( check_fixture( index ) == 1 );
        const auto selected = physical_light::collect_stationary_emitters( here, origin, 0 );
        const auto terrain_source = std::find_if( selected.begin(), selected.end(),
        [&fixture_abs]( const physical_light::emitter &emitter ) {
            return emitter.kind == physical_light::source_kind::terrain &&
                   emitter.position == fixture_abs;
        } );
        REQUIRE( terrain_source != selected.end() );
        CHECK( terrain_source->provenance.rfind( "terrain:", 0 ) == 0 );
        CHECK( terrain_source->luminance == 31 );
    };
    check_both_discovery_routes( beside_index );

    // Stand directly on the shared fixture: the old carrier-tile continue
    // dropped terrain and the lamp's map enumeration at exactly this point.
    guy.setpos( here, origin );
    const auto onto = physical_light::index_loaded_z_sources( guy, here );
    check_both_discovery_routes( onto );

    guy.setpos( here, away );
    const auto away_index = physical_light::index_loaded_z_sources( guy, here );
    check_both_discovery_routes( away_index );
}

TEST_CASE( "physical_light_loaded_z_index_keeps_player_tile_stationary_sources_and_restores_fixtures",
           "[physical_light][continuity]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    ter_t &terrain = const_cast<ter_t &>( here.ter( origin ).obj() );
    furn_t &furniture = const_cast<furn_t &>( here.furn( origin ).obj() );
    const int original_terrain_light = terrain.light_emitted;
    const int original_furniture_light = furniture.light_emitted;
    {
        restore_on_out_of_scope terrain_restore( terrain.light_emitted );
        restore_on_out_of_scope furniture_restore( furniture.light_emitted );
        terrain.light_emitted = 31;
        furniture.light_emitted = 37;
        const auto index = physical_light::index_loaded_z_sources( guy, here );
        CHECK( std::count_if( index.stationary_emitters.begin(), index.stationary_emitters.end(),
        [&here, &origin]( const physical_light::emitter &emitter ) {
            return emitter.position == here.get_abs( origin ) &&
                   ( emitter.kind == physical_light::source_kind::terrain ||
                     emitter.kind == physical_light::source_kind::furniture );
        } ) == 2 );
    }
    CHECK( terrain.light_emitted == original_terrain_light );
    CHECK( furniture.light_emitted == original_furniture_light );
    const auto throw_with_fixture = [&terrain, &furniture]() {
        restore_on_out_of_scope terrain_restore( terrain.light_emitted );
        restore_on_out_of_scope furniture_restore( furniture.light_emitted );
        terrain.light_emitted = 41;
        furniture.light_emitted = 43;
        throw std::runtime_error( "fixture restoration probe" );
    };
    try {
        throw_with_fixture();
    } catch( const std::runtime_error & ) {
    }
    CHECK( terrain.light_emitted == original_terrain_light );
    CHECK( furniture.light_emitted == original_furniture_light );
}

TEST_CASE( "physical_light_samples_once_per_real_advancing_turn",
           "[physical_light][continuity]" )
{
    physical_light::turn_sample_gate gate;
    // Non-advancing callers (render, inventory, debug, and scheduler probes)
    // cannot turn a current state query into a physical observation.
    CHECK_FALSE( gate.begin_advancing_turn( 40, false ) );
    CHECK( gate.begin_advancing_turn( 40, true ) );
    CHECK_FALSE( gate.begin_advancing_turn( 40, true ) );
    // Every distinct real turn is admitted, including turns between the old
    // five-minute maintenance boundaries.  Re-entry in that same turn is a
    // no-op, and a timeline rewind starts a fresh sequence.
    CHECK_FALSE( gate.begin_advancing_turn( 41, false ) );
    CHECK( gate.begin_advancing_turn( 41, true ) );
    CHECK( gate.begin_advancing_turn( 42, true ) );
    CHECK_FALSE( gate.begin_advancing_turn( 42, true ) );
    CHECK( gate.begin_advancing_turn( 7, true ) );
}

TEST_CASE( "physical_light_production_facade_records_real_delivery_order",
           "[physical_light][continuity][production_order]" )
{
    clear_avatar();
    clear_map_without_vision();
    reset_live_light_sample_cache();
    run_live_light_delivery_for_test();
    CHECK( live_light_sample_is_current_for_test() );
    CHECK( live_light_delivery_trace_for_test() == std::vector<live_light_delivery_stage>{
        live_light_delivery_stage::rider_reconciliation,
        live_light_delivery_stage::rider_memory_aging,
        live_light_delivery_stage::fresh_sampling,
        live_light_delivery_stage::horde_delivery,
        live_light_delivery_stage::stalker_delivery,
        live_light_delivery_stage::rider_delivery,
        live_light_delivery_stage::cannibal_delivery,
    } );
    // Re-entry in the same real turn does not resample.  The packet remains
    // current for the later cadence owner, without a query/redraw refresh.
    run_live_light_delivery_for_test();
    CHECK( live_light_sample_is_current_for_test() );
    CHECK( live_light_delivery_trace_for_test().empty() );
    run_live_light_staffed_observer_for_test();
    CHECK( live_light_sample_is_current_for_test() );
    CHECK( live_light_delivery_trace_for_test().back() ==
           live_light_delivery_stage::staffed_observer_recording );
}

TEST_CASE( "live_light_discovers_each_advancing_turn_without_same_turn_replay",
           "[physical_light][continuity][production_order]" )
{
    live_light::reset();
    int reconciliations = 0;
    int memory_aging = 0;
    int discoveries = 0;
    int deliveries = 0;
    live_light::callbacks callbacks;
    callbacks.reconcile_riders = [&reconciliations]() {
        ++reconciliations;
    };
    callbacks.age_rider_memory = [&memory_aging]() {
        ++memory_aging;
    };
    callbacks.discover = [&discoveries]() {
        ++discoveries;
        live_bandit_signal_observation sample;
        sample.has_light_projection = true;
        sample.sample_id = "cadence-fixture";
        return std::vector<live_bandit_signal_observation> { sample };
    };
    const auto count_delivery = [&deliveries]( const std::vector<live_bandit_signal_observation> & ) {
        ++deliveries;
    };
    callbacks.deliver_hordes = count_delivery;
    callbacks.deliver_stalkers = count_delivery;
    callbacks.deliver_riders = count_delivery;
    callbacks.deliver_cannibals = count_delivery;

    const time_point first_turn = calendar::turn_zero + 100_turns;
    live_light::run_advancing_turn( first_turn, callbacks );
    CHECK( reconciliations == 1 );
    CHECK( memory_aging == 1 );
    CHECK( discoveries == 1 );
    CHECK( deliveries == 4 );
    REQUIRE( live_light::samples_for_turn( first_turn ).size() == 1 );

    // A duplicate call in one real turn is a no-op, but the next advancing
    // turn must discover and deliver independently; this is intentionally not
    // the staffed observer's five-minute cadence.
    live_light::run_advancing_turn( first_turn, callbacks );
    CHECK( reconciliations == 1 );
    CHECK( memory_aging == 1 );
    CHECK( discoveries == 1 );
    CHECK( deliveries == 4 );
    CHECK( live_light::delivery_trace().empty() );

    const time_point second_turn = first_turn + 1_turns;
    live_light::run_advancing_turn( second_turn, callbacks );
    CHECK( reconciliations == 2 );
    CHECK( memory_aging == 2 );
    CHECK( discoveries == 2 );
    CHECK( deliveries == 8 );
    CHECK( live_light::sample_is_current( second_turn ) );
    CHECK( live_light::delivery_trace() == std::vector<live_light_delivery_stage> {
        live_light_delivery_stage::rider_reconciliation,
        live_light_delivery_stage::rider_memory_aging,
        live_light_delivery_stage::fresh_sampling,
        live_light_delivery_stage::horde_delivery,
        live_light_delivery_stage::stalker_delivery,
        live_light_delivery_stage::rider_delivery,
        live_light_delivery_stage::cannibal_delivery,
    } );
}

TEST_CASE( "physical_light_respects_depletion_and_movement", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    item depleted( itype_id( "flashlight_on" ) );
    item dead_battery( itype_id( "medium_battery_cell" ) );
    depleted.put_in( dead_battery, pocket_type::MAGAZINE_WELL );
    here.add_item( origin + tripoint::east, depleted );
    CHECK( physical_light::collect_item_emitters( guy, here, 3 ).empty() );

    here.add_item( origin + tripoint::north, powered_flashlight() );
    const auto moved = physical_light::collect_item_emitters( guy, here, 3 );
    REQUIRE( moved.size() == 1 );
    CHECK( moved.front().position == here.get_abs( origin + tripoint::north ) );
}

TEST_CASE( "physical_light_item_power_states_match_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms source_pos = guy.pos_bub() + tripoint::east * 3;
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );

    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( source_pos );
    here.add_item( source_pos, powered_flashlight() );
    const auto full = physical_light::collect_item_emitters( guy, here, 4 );
    const auto full_source = std::find_if( full.begin(), full.end(),
    [&source_abs]( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::ground_item && e.position == source_abs;
    } );
    REQUIRE( full_source != full.end() );
    const int full_luminance = full_source->luminance;
    rebuild_native_lightmap();
    const float full_native = here.ambient_light_at( source_pos );
    CHECK( here.light_at( source_pos ) == lit_level::BRIGHT );
    CHECK( full_native > baseline_native );

    // The same source remains source-bound when its battery is in the native
    // CHARGEDIM region; neither side may silently retain the full value.
    clear_map_without_vision();
    here.add_item( source_pos, powered_flashlight( 1 ) );
    const auto dimmed = physical_light::collect_item_emitters( guy, here, 4 );
    REQUIRE( dimmed.size() == 1 );
    REQUIRE( dimmed.front().position == source_abs );
    CHECK( dimmed.front().luminance > 0 );
    CHECK( dimmed.front().luminance < full_luminance );
    rebuild_native_lightmap();
    const float dimmed_native = here.ambient_light_at( source_pos );
    CHECK( dimmed_native > baseline_native );
    CHECK( dimmed_native < full_native );

    // Turning the item off and removing its battery are negative controls for
    // both the collector and native map lightmap.
    clear_map_without_vision();
    item off( itype_id( "flashlight" ) );
    off.put_in( item( itype_id( "medium_battery_cell" ) ), pocket_type::MAGAZINE_WELL );
    here.add_item( source_pos, off );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );

    clear_map_without_vision();
    item depleted( itype_id( "flashlight_on" ) );
    depleted.put_in( item( itype_id( "medium_battery_cell" ) ), pocket_type::MAGAZINE_WELL );
    here.add_item( source_pos, depleted );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );

    clear_map_without_vision();
    here.add_item( source_pos, powered_flashlight() );
    REQUIRE_FALSE( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    here.i_clear( source_pos );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );
}

TEST_CASE( "physical_light_containment_matches_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms source_pos = guy.pos_bub() + tripoint::east * 3;
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );

    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( source_pos );
    item transparent( itype_id( "bag_plastic" ) );
    transparent.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( source_pos, transparent );
    const auto escaped = physical_light::collect_item_emitters( guy, here, 4 );
    REQUIRE( escaped.size() == 1 );
    CHECK( escaped.front().position == source_abs );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) > baseline_native );

    clear_map_without_vision();
    item opaque( itype_id( "backpack" ) );
    opaque.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( source_pos, opaque );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );
}

TEST_CASE( "physical_light_carried_record_matches_native_character_light", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_abs_ms source_abs = here.get_abs( guy.pos_bub() );
    const tripoint_bub_ms neighbour_pos = guy.pos_bub() + tripoint::east;
    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( neighbour_pos );
    REQUIRE( guy.try_add( powered_flashlight() ) != item_location::nowhere );

    const auto carried = physical_light::collect_item_emitters( guy, here, 1 );
    const auto carried_source = std::find_if( carried.begin(), carried.end(),
    [&source_abs]( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::carried_item && e.position == source_abs;
    } );
    REQUIRE( carried_source != carried.end() );
    rebuild_native_lightmap();
    CHECK( here.light_at( guy.pos_bub() ) == lit_level::BRIGHT );
    CHECK( here.ambient_light_at( neighbour_pos ) > baseline_native );
}

TEST_CASE( "physical_light_native_same_omt_sources_remain_independent", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms bright_pos = guy.pos_bub() + tripoint::east * 3;
    const tripoint_bub_ms dim_pos = guy.pos_bub() + tripoint::west * 3;
    REQUIRE( coords::project_to<coords::omt>( here.get_abs( bright_pos ) ) ==
             coords::project_to<coords::omt>( here.get_abs( dim_pos ) ) );
    rebuild_native_lightmap();
    const float dim_baseline = here.ambient_light_at( dim_pos );

    here.add_item( bright_pos, powered_flashlight() );
    here.add_item( dim_pos, powered_flashlight( 1 ) );
    const auto combined = physical_light::collect_item_emitters( guy, here, 4 );
    REQUIRE( combined.size() == 2 );
    CHECK( std::count_if( combined.begin(), combined.end(),
    [&here, &bright_pos]( const physical_light::emitter &e ) {
        return e.position == here.get_abs( bright_pos );
    } ) == 1 );
    CHECK( std::count_if( combined.begin(), combined.end(),
    [&here, &dim_pos]( const physical_light::emitter &e ) {
        return e.position == here.get_abs( dim_pos );
    } ) == 1 );
    rebuild_native_lightmap();
    CHECK( here.light_at( bright_pos ) == lit_level::BRIGHT );
    CHECK( here.ambient_light_at( dim_pos ) > dim_baseline );

    // Removing the bright source leaves the dim source's own native result;
    // its cache observation cannot be borrowed from the other OMT source.
    here.i_clear( bright_pos );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).size() == 1 );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( dim_pos ) > dim_baseline );

    // The reciprocal control binds the bright result to its own source.
    here.i_clear( dim_pos );
    here.add_item( bright_pos, powered_flashlight() );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).size() == 1 );
    rebuild_native_lightmap();
    CHECK( here.light_at( bright_pos ) == lit_level::BRIGHT );
}

TEST_CASE( "physical_light_respects_container_transparency", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    item bag( itype_id( "bag_plastic" ) );
    bag.put_in( powered_flashlight(), pocket_type::CONTAINER );
    REQUIRE( guy.try_add( bag ) != item_location::nowhere );
    const auto emitters = physical_light::collect_item_emitters( guy, here, 1 );
    REQUIRE( emitters.size() == 1 );
    CHECK( emitters.front().provenance == "flashlight_on" );
}

TEST_CASE( "physical_light_handles_ground_container_escape", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms p = guy.pos_bub() + tripoint::east;
    item transparent( itype_id( "bag_plastic" ) );
    transparent.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( p, transparent );
    CHECK( physical_light::collect_item_emitters( guy, here, 2 ).size() == 1 );

    item opaque( itype_id( "backpack" ) );
    opaque.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( p + tripoint::east, opaque );
    const auto emitters = physical_light::collect_item_emitters( guy, here, 3 );
    CHECK( emitters.size() == 1 );
}

TEST_CASE( "physical_light_weapon_mount_has_one_source", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    item gun( itype_id( "glock_19" ) );
    item mounted( itype_id( "mounted_flashlight_on" ) );
    item mount_battery( itype_id( "light_battery_cell" ) );
    mount_battery.ammo_set( mount_battery.ammo_default(), 56 );
    REQUIRE( mounted.put_in( mount_battery, pocket_type::MAGAZINE_WELL ).success() );
    REQUIRE( gun.put_in( mounted, pocket_type::MOD ).success() );
    REQUIRE( guy.wield( gun ) );
    const auto emitters = physical_light::collect_item_emitters( guy, here, 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::weapon_mounted;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.provenance == "glock_19";
    } ) == 0 );
}

TEST_CASE( "physical_light_loaded_z_index_handles_ground_gun_light_mod", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms source_pos = guy.pos_bub() + tripoint::east;

    // This is the topology from the advancing-turn crash: a ground gun whose
    // powered mounted light is reached while index_loaded_z_sources traverses
    // every loaded z-level.  The forwarding root must not be emitted twice.
    item gun( itype_id( "glock_19" ) );
    item mounted( itype_id( "mounted_flashlight_on" ) );
    item battery( itype_id( "light_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), 56 );
    REQUIRE( mounted.put_in( battery, pocket_type::MAGAZINE_WELL ).success() );
    REQUIRE( gun.put_in( mounted, pocket_type::MOD ).success() );
    here.add_item( source_pos, gun );

    const physical_light::loaded_z_source_index index =
        physical_light::index_loaded_z_sources( guy, here );
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );
    CHECK( std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
    [&source_abs]( const physical_light::emitter &emitter ) {
        return emitter.position == source_abs && emitter.kind == physical_light::source_kind::ground_item &&
               emitter.provenance == "mounted_flashlight_on";
    } ) == 1 );
    CHECK( std::none_of( index.item_emitters.begin(), index.item_emitters.end(),
    [&source_abs]( const physical_light::emitter &emitter ) {
        return emitter.position == source_abs && emitter.provenance == "glock_19";
    } ) );
}

TEST_CASE( "physical_light_weapon_mount_matches_native_lightmap", "[physical_light][lightmap]" )
{
    const auto mounted_gun = []( const bool powered ) {
        item gun( itype_id( "glock_19" ) );
        item mounted( itype_id( "mounted_flashlight_on" ) );
        item battery( itype_id( "light_battery_cell" ) );
        if( powered ) {
            battery.ammo_set( battery.ammo_default(), 56 );
        }
        REQUIRE( mounted.put_in( battery, pocket_type::MAGAZINE_WELL ).success() );
        REQUIRE( gun.put_in( mounted, pocket_type::MOD ).success() );
        return gun;
    };

    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms neighbour = guy.pos_bub() + tripoint::east;
    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( neighbour );
    item powered_gun = mounted_gun( true );
    REQUIRE( guy.wield( powered_gun ) );

    const auto powered = physical_light::collect_item_emitters( guy, here, 1 );
    const auto source_abs = here.get_abs( guy.pos_bub() );
    const auto mounted_source = std::find_if( powered.begin(), powered.end(),
    [&source_abs]( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::weapon_mounted &&
               e.position == source_abs;
    } );
    REQUIRE( mounted_source != powered.end() );
    CHECK( mounted_source->provenance == "mounted_flashlight_on" );
    CHECK( mounted_source->luminance > 0 );
    rebuild_native_lightmap();
    CHECK( here.light_at( guy.pos_bub() ) == lit_level::BRIGHT );
    CHECK( here.ambient_light_at( neighbour ) > baseline_native );
}

TEST_CASE( "physical_light_weapon_mount_depleted_matches_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &depleted_guy = get_avatar();
    map &depleted_here = get_map();
    const tripoint_bub_ms depleted_neighbour = depleted_guy.pos_bub() + tripoint::east;
    rebuild_native_lightmap();
    const float depleted_baseline = depleted_here.ambient_light_at( depleted_neighbour );
    item depleted_gun( itype_id( "glock_19" ) );
    item depleted_mount( itype_id( "mounted_flashlight_on" ) );
    item depleted_battery( itype_id( "light_battery_cell" ) );
    REQUIRE( depleted_mount.put_in( depleted_battery, pocket_type::MAGAZINE_WELL ).success() );
    REQUIRE( depleted_gun.put_in( depleted_mount, pocket_type::MOD ).success() );
    REQUIRE( depleted_guy.wield( depleted_gun ) );
    CHECK( physical_light::collect_item_emitters( depleted_guy, depleted_here, 1 ).empty() );
    rebuild_native_lightmap();
    CHECK( depleted_here.ambient_light_at( depleted_neighbour ) == Approx( depleted_baseline ) );
}

TEST_CASE( "physical_light_detects_each_escaped_source_without_terrain_recognition",
           "[physical_light][optics]" )
{
    const auto detected = []( int distance, int vertical, bool exposed, bool vertical_clear,
    std::vector<int> attenuation = {} ) {
        physical_light::route route;
        route.distance_omt = distance;
        route.vertical_offset = vertical;
        route.source_exposed = exposed;
        route.vertical_sightline = vertical_clear;
        route.brightness_range_omt = 5;
        route.terrain_see_costs = std::move( attenuation );
        return physical_light::detect( route );
    };

    // This query accepts no observer terrain-recognition range.  The light is
    // a location clue only, never player/camp identification.
    const physical_light::detection ground_exposed = detected( 2, 0, true, true );
    CHECK( ground_exposed.visible );
    CHECK_FALSE( ground_exposed.recognizes_identity );

    // Exposed elevation is visible in both directions when its cross-level
    // route is clear.  A bright source is not rejected merely for z mismatch.
    CHECK( detected( 2, 1, true, true ).visible ); // observer below
    CHECK( detected( 2, -1, true, true ).visible ); // observer above

    // A parapet/solid wall, a sealed upper room, opaque curtain or shutter,
    // and a deep hall without a local aperture all prevent detection.
    CHECK_FALSE( detected( 2, 1, true, true, { -1 } ).visible ); // parapet
    CHECK_FALSE( detected( 2, 1, false, false ).visible ); // sealed upper room/floor
    CHECK_FALSE( detected( 2, 0, true, true, { -1 } ).visible ); // opaque covering
    CHECK_FALSE( detected( 2, 0, false, true ).visible ); // deep interior corner

    // Clear glass attenuates but transmits; it is not equivalent to a curtain.
    CHECK( detected( 2, 0, true, true, { 1 } ).visible );
}

TEST_CASE( "physical_light_keeps_same_omt_escape_and_position_per_source", "[physical_light][optics]" )
{
    physical_light::route window_lamp;
    window_lamp.distance_omt = 1;
    window_lamp.source_exposed = true;
    window_lamp.vertical_sightline = true;
    window_lamp.brightness_range_omt = 3;

    physical_light::route sealed_lamp = window_lamp;
    sealed_lamp.source_exposed = false;

    const physical_light::detection window_result = physical_light::detect( window_lamp );
    const physical_light::detection sealed_result = physical_light::detect( sealed_lamp );
    CHECK( window_result.visible );
    CHECK_FALSE( sealed_result.visible );
    CHECK_FALSE( window_result.recognizes_identity );
    CHECK_FALSE( sealed_result.recognizes_identity );
}

TEST_CASE( "physical_light_collects_stationary_native_owners", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms field_pos = origin + tripoint::east;
    here.add_field( field_pos, field_type_id( "fd_bile_adv" ), 2 );

    monster &luminous = spawn_test_monster( "mon_fungal_blossom", origin + tripoint::north );
    REQUIRE( luminous.type->luminance > 0 );

    vehicle *veh = here.add_vehicle( vproto_id( "car" ), origin + tripoint::south, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    bool enabled_light = false;
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = true;
            enabled_light = true;
        }
    }
    REQUIRE( enabled_light );

    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::field;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::luminous_monster;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::vehicle_part;
    } ) >= 1 );
}

TEST_CASE( "physical_light_stationary_records_are_source_bound", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms terrain_pos = origin + tripoint::east;
    const tripoint_bub_ms furniture_pos = origin + tripoint::west;
    const tripoint_bub_ms field_a = origin + tripoint::north;
    const tripoint_bub_ms field_b = origin + tripoint::south;

    ter_t &terrain = const_cast<ter_t &>( here.ter( terrain_pos ).obj() );
    furn_t &furniture = const_cast<furn_t &>( here.furn( furniture_pos ).obj() );
    restore_on_out_of_scope terrain_light_restore( terrain.light_emitted );
    restore_on_out_of_scope furniture_light_restore( furniture.light_emitted );
    terrain.light_emitted = 17;
    furniture.light_emitted = 23;
    here.add_field( field_a, field_type_id( "fd_bile_adv" ), 2 );
    here.add_field( field_b, field_type_id( "fd_bile_adv" ), 3 );

    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 2 );
    const auto find = [&emitters]( physical_light::source_kind kind, const tripoint_abs_ms &pos ) {
        return std::find_if( emitters.begin(), emitters.end(), [kind, &pos]( const physical_light::emitter &e ) {
            return e.kind == kind && e.position == pos;
        } );
    };
    const auto terrain_source = find( physical_light::source_kind::terrain, here.get_abs( terrain_pos ) );
    const auto furniture_source = find( physical_light::source_kind::furniture, here.get_abs( furniture_pos ) );
    REQUIRE( terrain_source != emitters.end() );
    REQUIRE( furniture_source != emitters.end() );
    CHECK( terrain_source->provenance == "terrain:" + here.ter( terrain_pos ).obj().id.str() );
    CHECK( terrain_source->luminance == 17 );
    CHECK( furniture_source->provenance == "furniture:" + here.furn( furniture_pos ).obj().id.str() );
    CHECK( furniture_source->luminance == 23 );
    CHECK( terrain_source->position == here.get_abs( terrain_pos ) );
    CHECK( furniture_source->position == here.get_abs( furniture_pos ) );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::field;
    } ) == 2 );
    CHECK( coords::project_to<coords::omt>( here.get_abs( field_a ) ) ==
           coords::project_to<coords::omt>( here.get_abs( field_b ) ) );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::field && e.position.x() != 0;
    } ) == 2 );

}

TEST_CASE( "physical_light_stationary_records_match_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms terrain_pos = origin + tripoint::east * 3;
    const tripoint_bub_ms field_pos = origin + tripoint::north * 3;
    const tripoint_bub_ms monster_pos = origin + tripoint::west * 3;

    rebuild_native_lightmap();
    const float terrain_baseline = here.ambient_light_at( terrain_pos );
    const float field_baseline = here.ambient_light_at( field_pos );
    const float monster_baseline = here.ambient_light_at( monster_pos );
    here.ter_set( terrain_pos, ter_str_id( "t_test_red_light" ).id() );
    here.add_field( field_pos, field_type_id( "fd_test_green_glow" ), 1 );
    monster &luminous = spawn_test_monster( "mon_fungal_blossom", monster_pos );
    REQUIRE( luminous.type->luminance > 0 );

    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 4 );
    const auto has_source = [&emitters]( physical_light::source_kind kind,
    const tripoint_abs_ms &position ) {
        return std::find_if( emitters.begin(), emitters.end(),
        [kind, &position]( const physical_light::emitter &e ) {
            return e.kind == kind && e.position == position && e.luminance > 0;
        } ) != emitters.end();
    };
    REQUIRE( has_source( physical_light::source_kind::terrain, here.get_abs( terrain_pos ) ) );
    REQUIRE( has_source( physical_light::source_kind::field, here.get_abs( field_pos ) ) );
    REQUIRE( has_source( physical_light::source_kind::luminous_monster,
                         here.get_abs( monster_pos ) ) );

    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( terrain_pos ) > terrain_baseline );
    CHECK( here.ambient_light_at( field_pos ) > field_baseline );
    CHECK( here.ambient_light_at( monster_pos ) > monster_baseline );
}

TEST_CASE( "physical_light_excludes_disabled_or_unavailable_vehicle_parts", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    vehicle *veh = here.add_vehicle( vproto_id( "car" ), origin + tripoint::south, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = false;
        }
    }
    const auto disabled = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::none_of( disabled.begin(), disabled.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::vehicle_part;
    } ) );
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = true;
            item broken_base = part.get_base();
            broken_base.force_set_damage( broken_base.max_damage() );
            part.set_base( std::move( broken_base ) );
        }
    }
    const auto unavailable = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::none_of( unavailable.begin(), unavailable.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::vehicle_part;
    } ) );
}

TEST_CASE( "physical_light_vehicle_parts_match_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    vehicle *veh = here.add_vehicle( vproto_id( "car" ), origin + tripoint::south, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        if( ref.part().is_light() ) {
            ref.part().enabled = true;
        }
    }

    const std::vector<vehicle_part *> lights = veh->lights();
    REQUIRE_FALSE( lights.empty() );
    const vehicle_part *chosen = lights.front();
    const tripoint_bub_ms source_pos = veh->bub_part_pos( here, *chosen );
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );
    const std::string provenance = "vehicle:" + chosen->info().id.str();
    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 6 );
    const auto source = std::find_if( emitters.begin(), emitters.end(),
    [&source_abs, &provenance]( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::vehicle_part &&
               e.position == source_abs && e.provenance == provenance;
    } );
    REQUIRE( source != emitters.end() );
    CHECK( source->luminance == chosen->info().bonus );

    rebuild_native_lightmap();
    const float lit_native = here.ambient_light_at( source_pos );
    CHECK( lit_native > 0.0f );

    for( const vpart_reference &ref : veh->get_all_parts() ) {
        if( ref.part().is_light() ) {
            ref.part().enabled = false;
        }
    }
    CHECK( physical_light::collect_stationary_emitters( here, origin, 6 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) < lit_native );

    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = true;
            item broken = part.get_base();
            broken.force_set_damage( broken.max_damage() );
            part.set_base( std::move( broken ) );
        }
    }
    CHECK( physical_light::collect_stationary_emitters( here, origin, 6 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) < lit_native );
}

TEST_CASE( "physical_light_stationary_excludes_stale_and_hallucinated_sources", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    monster &hallucination = spawn_test_monster( "mon_fungal_blossom", origin + tripoint::east );
    hallucination.hallucination = true;
    monster &real = spawn_test_monster( "mon_fungal_blossom", origin + tripoint::west );
    REQUIRE( real.type->luminance > 0 );
    const auto before = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::count_if( before.begin(), before.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::luminous_monster;
    } ) == 1 );
    g->remove_zombie( real );
    const auto after = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::none_of( after.begin(), after.end(), []( const physical_light::emitter &e ) {
        return e.kind == physical_light::source_kind::luminous_monster;
    } ) );
}

#include "cata_catch.h"
#include "native_performance_extrema.h"

TEST_CASE( "native_performance_extrema_windows_are_bounded_and_nonoverlapping", "[performance][extrema]" )
{
    native_performance_extrema extrema;
    native_performance_sample sample;
    sample.process_cpu_seconds = .001;
    sample.input_seconds = .002;
    sample.save_seconds = .003;
    for( int turn = 1; turn <= 100; ++turn ) {
        sample.turn_id = turn;
        sample.game_turn = 1000 + turn;
        sample.start_sequence = 2 * turn - 1;
        sample.end_sequence = 2 * turn;
        sample.simulation_seconds = turn == 49 ? 10 : .01;
        extrema.add( sample );
    }
    REQUIRE( extrema.individual() );
    REQUIRE( extrema.window() );
    CHECK( extrema.individual()->turn_id == 49 );
    CHECK( extrema.window()->first.turn_id == 1 );
    CHECK( extrema.window()->last.turn_id == 100 );
    CHECK( extrema.window()->dominant.turn_id == 49 );
    CHECK( extrema.window()->mean_simulation_seconds_per_turn == Approx( .1099 ) );
    CHECK( *extrema.window()->total_process_cpu_seconds == Approx( .1 ) );
    for( int turn = 101; turn <= 200; ++turn ) {
        sample.turn_id = turn;
        sample.simulation_seconds = .2;
        sample.process_cpu_seconds.reset();
        extrema.add( sample );
    }
    REQUIRE( extrema.window() );
    CHECK( extrema.window()->first.turn_id == 101 );
    CHECK( extrema.window()->last.turn_id == 200 );
    CHECK( extrema.window()->dominant.turn_id == 101 ); // first tie wins
    CHECK( extrema.window()->mean_simulation_seconds_per_turn == Approx( .2 ) );
    CHECK_FALSE( extrema.window()->total_process_cpu_seconds );
    CHECK( *extrema.window()->total_input_seconds == Approx( .2 ) );
    CHECK( extrema.individual()->turn_id == 49 );
}

TEST_CASE( "native_performance_extrema_rejects_incomplete_gapped_windows", "[performance][extrema]" )
{
    native_performance_extrema extrema;
    native_performance_sample sample;
    sample.simulation_seconds = 1;
    for( int turn = 1; turn <= 99; ++turn ) {
        sample.turn_id = turn;
        extrema.add( sample );
    }
    sample.turn_id = 101;
    extrema.add( sample );
    CHECK_FALSE( extrema.window() );
    for( int turn = 102; turn <= 200; ++turn ) {
        sample.turn_id = turn;
        extrema.add( sample );
    }
    REQUIRE( extrema.window() );
    CHECK( extrema.window()->first.turn_id == 101 );
    CHECK( extrema.window()->last.turn_id == 200 );
    CHECK_FALSE( extrema.window()->total_input_seconds );
    CHECK_FALSE( extrema.window()->total_save_seconds );
}

TEST_CASE( "native_performance_extrema_rewind_starts_new_epoch", "[performance][extrema]" )
{
    native_performance_extrema extrema;
    native_performance_sample sample;
    sample.turn_id = 1;
    sample.game_turn = 200;
    sample.simulation_seconds = 9;
    REQUIRE( extrema.add( sample ) );
    sample.turn_id = 2;
    sample.game_turn = 100;
    sample.simulation_seconds = .1;
    REQUIRE( extrema.add( sample ) );
    CHECK( extrema.epoch() == 2 );
    REQUIRE( extrema.individual() );
    CHECK( extrema.individual()->turn_id == 2 );
    CHECK_FALSE( extrema.window() );
}

TEST_CASE( "native_performance_extrema_capture_candidates_follow_window_and_epoch", "[performance][extrema]" )
{
    native_performance_extrema extrema;
    native_performance_sample row;
    row.turn_id = 1;
    row.game_turn = 100;
    row.simulation_seconds = 4;
    CHECK( extrema.needs_detail( row ) );
    extrema.add( row );
    row.turn_id = 2;
    row.game_turn = 101;
    row.simulation_seconds = 4;
    CHECK_FALSE( extrema.needs_detail( row ) );
    extrema.add( row );
    row.turn_id = 4;
    row.game_turn = 102;
    row.simulation_seconds = 1;
    CHECK( extrema.needs_detail( row ) );
    extrema.add( row );
    row.turn_id = 5;
    row.game_turn = 50;
    CHECK( extrema.needs_detail( row ) );
}

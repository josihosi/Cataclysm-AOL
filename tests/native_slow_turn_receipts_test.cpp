#include "cata_catch.h"
#include "native_slow_turn_receipts.h"

#include <algorithm>

TEST_CASE( "slow turn receipts coalesce and rate limit without losing failures", "[performance][slow_receipts]" )
{
    native_slow_turn_receipts receipts;
    slow_turn_candidate row;
    row.alarm_kind = 1;
    row.dominant_stage = 2;
    row.first.turn_id = row.last.turn_id = row.dominant.turn_id = 1;
    row.simulation_seconds = 1;
    receipts.add( row );
    CHECK( receipts.should_publish( 1 ) );
    receipts.published( 1, false );
    CHECK( receipts.pending() );
    CHECK( receipts.publication_sequence() == 0 );
    CHECK_FALSE( receipts.should_publish( 2 ) );
    CHECK( receipts.should_publish( 31 ) );
    receipts.published( 31, true );
    CHECK_FALSE( receipts.pending() );
    for( int i = 2; i <= 10000; ++i ) {
        row.first.turn_id = row.last.turn_id = row.dominant.turn_id = i;
        row.simulation_seconds = i / 1000.0;
        receipts.add( row );
        CHECK_FALSE( receipts.should_publish( 32 ) );
    }
    const auto &record = receipts.records()[0];
    CHECK( record.count == 10000 );
    CHECK( record.first.turn_id == 1 );
    CHECK( record.last.turn_id == 10000 );
    CHECK( record.worst.dominant.turn_id == 10000 );
    CHECK( receipts.should_publish( 61 ) );
    CHECK( receipts.should_publish( 32, true ) );
    receipts.published( 61, true );
    CHECK( receipts.publication_sequence() == 2 );
}

TEST_CASE( "slow turn receipts cap keys and report evicted or dropped alarms", "[performance][slow_receipts]" )
{
    native_slow_turn_receipts receipts;
    slow_turn_candidate row;
    row.alarm_kind = 1;
    for( int i = 0; i < 8; ++i ) {
        row.dominant_stage = i;
        row.simulation_seconds = i + 1;
        receipts.add( row );
    }
    row.dominant_stage = 9;
    row.simulation_seconds = 0.5;
    receipts.add( row );
    CHECK( receipts.dropped() == 1 );
    row.simulation_seconds = 9;
    receipts.add( row );
    CHECK( receipts.dropped() == 2 );
    CHECK( receipts.records()[0].worst.dominant_stage == 9 );
    CHECK( sizeof( receipts ) < 32768 );
}

TEST_CASE( "slow turn detector uses selected thresholds and waiting crossings", "[performance][slow_receipts]" )
{
    native_slow_turn_detector detector;
    slow_turn_candidate sample;
    sample.dominant_stage = 7;
    sample.sample_count = 1;
    sample.simulation_seconds = 0.2;
    sample.dominant_simulation_seconds = sample.simulation_seconds;
    for( int i = 1; i <= 100; ++i ) {
        sample.first.turn_id = sample.last.turn_id = sample.dominant.turn_id = i;
        detector.observe( sample, 99, std::nullopt, std::nullopt );
    }
    const auto &waiting = detector.receipts().records()[0];
    REQUIRE( waiting.used );
    CHECK( waiting.worst.alarm_kind == 3 );
    CHECK( waiting.worst.sample_count == 100 );
    CHECK( waiting.first.turn_id == 1 );
    CHECK( waiting.last.turn_id == 100 );
    CHECK( waiting.worst.dominant.turn_id == 1 );
    for( int i = 101; i <= 200; ++i ) {
        sample.first.turn_id = sample.last.turn_id = sample.dominant.turn_id = i;
        detector.observe( sample, 99, std::nullopt, std::nullopt );
    }
    CHECK( detector.receipts().records()[0].count == 1 );
    // A missing config never creates generic turn alarms.
    sample.first.turn_id = sample.last.turn_id = sample.dominant.turn_id = 201;
    detector.observe( sample, 0, std::nullopt, std::nullopt );
    CHECK_FALSE( detector.receipts().records()[1].used );
    sample.first.turn_id = sample.last.turn_id = sample.dominant.turn_id = 202;
    detector.observe( sample, 0, 0.1, 0.15 );
    CHECK( detector.receipts().records()[1].worst.alarm_kind == 1 );
    CHECK( detector.receipts().records()[2].worst.alarm_kind == 2 );
    detector.observe( sample, 0, 0.1, 0.15 );
    CHECK( detector.receipts().records()[1].count == 1 );
}

TEST_CASE( "sustained windows stay bounded and preserve exact membership", "[performance][slow_receipts]" )
{
    native_slow_turn_detector detector;
    slow_turn_candidate sample;
    sample.sample_count = 1;
    sample.simulation_seconds = sample.dominant_simulation_seconds = 0.2;
    sample.dominant_stage = 3;
    for( int i = 1; i <= 1000; ++i ) {
        sample.first.turn_id = sample.last.turn_id = sample.dominant.turn_id = i;
        detector.observe( sample, 0, 0.1, 1.0, 256 );
    }
    const auto &records = detector.receipts().records();
    REQUIRE( records[0].worst.alarm_kind == 2 );
    REQUIRE( records[1].worst.alarm_kind == 4 );
    CHECK( records[0].count == 1000 );
    std::uint64_t window_count = 0;
    std::uint64_t last_window_turn = 0;
    for( const auto &record : records ) {
        if( record.used && record.worst.alarm_kind == 4 ) {
            window_count += record.count;
            last_window_turn = std::max( last_window_turn, record.last.turn_id );
        }
    }
    CHECK( window_count == 745 );
    CHECK( records[1].first.turn_id == 1 );
    CHECK( last_window_turn == 1000 );
    CHECK( records[1].worst.first.turn_id == 1 );
    CHECK( records[1].worst.last.turn_id == 256 );
    CHECK( records[1].worst.dominant.turn_id == 1 );
    CHECK( sizeof( detector ) <= 32768 );
}

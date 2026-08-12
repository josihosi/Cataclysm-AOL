#include "cata_catch.h"
#include "harness_world.h"

TEST_CASE( "harness_world_options_accepts_raw_uint32_seed" )
{
    std::string error;
    const auto parsed = parse_harness_world_options( "natural-r002", "830205018", &error );
    REQUIRE( parsed.has_value() );
    CHECK( parsed->world_name == "natural-r002" );
    CHECK( parsed->raw_seed == 830205018U );
    CHECK( error.empty() );
}

TEST_CASE( "harness_world_options_accepts_production_feasibility_seed" )
{
    std::string error;
    const auto parsed = parse_harness_world_options( "natural-r002-m97", "830205385", &error );
    REQUIRE( parsed );
    CHECK( parsed->world_name == "natural-r002-m97" );
    CHECK( parsed->raw_seed == 830205385U );
    CHECK( error.empty() );
}

TEST_CASE( "harness_world_options_rejects_invalid_requests" )
{
    std::string error;
    CHECK_FALSE( parse_harness_world_options( "", "1", &error ) );
    CHECK( error == "harness world name cannot be empty" );
    CHECK_FALSE( parse_harness_world_options( "natural-r002", "-1", &error ) );
    CHECK( error == "harness raw seed must be an unsigned decimal integer" );
    CHECK_FALSE( parse_harness_world_options( "natural-r002", "0", &error ) );
    CHECK( error == "harness raw seed must be non-zero" );
    CHECK_FALSE( parse_harness_world_options( "natural-r002", "4294967296", &error ) );
    CHECK( error == "harness raw seed is outside the uint32 range" );
}

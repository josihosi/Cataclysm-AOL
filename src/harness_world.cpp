#include "harness_world.h"

#include <charconv>
#include <limits>

std::optional<harness_world_options> parse_harness_world_options(
    const std::string_view world_name, const std::string_view raw_seed, std::string *error )
{
    const auto fail = [error]( const char *message ) -> std::optional<harness_world_options> {
        if( error != nullptr ) {
            *error = message;
        }
        return std::nullopt;
    };

    if( world_name.empty() ) {
        return fail( "harness world name cannot be empty" );
    }
    if( raw_seed.empty() ) {
        return fail( "harness raw seed cannot be empty" );
    }
    for( const char ch : raw_seed ) {
        if( ch < '0' || ch > '9' ) {
            return fail( "harness raw seed must be an unsigned decimal integer" );
        }
    }

    std::uint32_t parsed_seed = 0;
    const char *const begin = raw_seed.data();
    const char *const end = begin + raw_seed.size();
    const auto parsed = std::from_chars( begin, end, parsed_seed, 10 );
    if( parsed.ec == std::errc::result_out_of_range || parsed.ptr != end ) {
        return fail( "harness raw seed is outside the uint32 range" );
    }
    if( parsed_seed == 0 ) {
        return fail( "harness raw seed must be non-zero" );
    }

    return harness_world_options{ std::string( world_name ), parsed_seed };
}

#pragma once

#include <string_view>

// Only these declared fixtures own the rider-specific lamp and diagnostics.
inline bool harness_rider_light_fixture( const char *scenario )
{
    return scenario && ( std::string_view( scenario ) ==
                         "zombie_rider.live_light_memory_continuity_mcw" ||
                         std::string_view( scenario ) ==
                         "zombie_rider.live_light_memory_continuity_isolated_mcw" );
}

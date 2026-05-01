#pragma once

#include <string>
#include <vector>

namespace flesh_raptor
{

enum class decision {
    fallback,
    orbit,
    swoop
};

struct orbit_candidate {
    int rel_x = 0;
    int rel_y = 0;
    int distance_to_target = 0;
    int distance_from_raptor = 0;
    int crowding = 0;
    bool held = false;
    std::string label;
};

struct orbit_context {
    int current_rel_x = 0;
    int current_rel_y = 0;
    int distance_to_target = 0;
    bool retreating = false;
    int cadence_phase = 1;
};

struct orbit_response {
    decision next = decision::fallback;
    orbit_candidate chosen;
    int score = 0;
    bool has_candidate = false;
    std::string reason;
};

int score_orbit_candidate( const orbit_context &ctx, const orbit_candidate &candidate );
orbit_response choose_orbit_destination( const orbit_context &ctx,
        const std::vector<orbit_candidate> &candidates );

} // namespace flesh_raptor

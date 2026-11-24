#pragma once
#ifndef CATA_SRC_HOME_ADMISSION_TIMING_H
#define CATA_SRC_HOME_ADMISSION_TIMING_H

#include <chrono>

struct home_admission_body_timing {
    std::chrono::nanoseconds elapsed{};
    int calls = 0;
};

// Optional in-memory measurement for the native test runner. The turn timing
// pass leaves this unset, and the game never enables it in ordinary play.
void set_home_admission_body_timing_for_test( home_admission_body_timing *timing );

#endif // CATA_SRC_HOME_ADMISSION_TIMING_H

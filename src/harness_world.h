#pragma once
#ifndef CATA_SRC_HARNESS_WORLD_H
#define CATA_SRC_HARNESS_WORLD_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct harness_world_options {
    std::string world_name;
    std::uint32_t raw_seed = 0;
};

/** Parse the hidden harness-only named-world request without changing --seed semantics. */
std::optional<harness_world_options> parse_harness_world_options(
    std::string_view world_name, std::string_view raw_seed, std::string *error = nullptr );

#endif // CATA_SRC_HARNESS_WORLD_H

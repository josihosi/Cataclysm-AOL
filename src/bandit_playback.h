#pragma once

#include "bandit_dry_run.h"

#include <string>
#include <vector>

namespace bandit_playback
{
struct scenario_frame {
    int tick = 0;
    std::string phase;
    bandit_dry_run::camp_input camp;
    std::vector<bandit_dry_run::lead_input> leads;
    std::vector<std::string> notes;
};

struct scenario_definition {
    std::string id;
    std::string title;
    std::vector<int> default_checkpoints;
    std::vector<std::string> questions;
    std::vector<scenario_frame> frames;
};

struct checkpoint_result {
    int tick = 0;
    std::string phase;
    std::vector<std::string> notes;
    bandit_dry_run::evaluation_result evaluation;
};

struct playback_result {
    std::string scenario_id;
    std::string title;
    std::vector<std::string> questions;
    std::vector<checkpoint_result> checkpoints;
};

const std::vector<scenario_definition> &reference_scenarios();
const scenario_definition *find_reference_scenario( const std::string &id );
playback_result run_scenario( const scenario_definition &scenario,
                              const std::vector<int> &checkpoints = {} );
std::string render_report( const playback_result &result );
} // namespace bandit_playback

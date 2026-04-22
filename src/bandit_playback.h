#pragma once

#include "bandit_dry_run.h"
#include "bandit_mark_generation.h"
#include "bandit_pursuit_handoff.h"

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
    bandit_mark_generation::cadence_tier cadence = bandit_mark_generation::cadence_tier::nearby_active;
    std::vector<bandit_mark_generation::signal_input> mark_signals;
    std::vector<bandit_mark_generation::smoke_packet> smoke_packets;
    std::vector<bandit_mark_generation::light_packet> light_packets;
    std::vector<bandit_mark_generation::human_route_packet> human_route_packets;

    scenario_frame() = default;
    scenario_frame( int tick_, std::string phase_, bandit_dry_run::camp_input camp_,
                    std::vector<bandit_dry_run::lead_input> leads_,
                    std::vector<std::string> notes_,
                    bandit_mark_generation::cadence_tier cadence_,
                    std::vector<bandit_mark_generation::signal_input> mark_signals_,
                    std::vector<bandit_mark_generation::smoke_packet> smoke_packets_ = {},
                    std::vector<bandit_mark_generation::light_packet> light_packets_ = {},
                    std::vector<bandit_mark_generation::human_route_packet> human_route_packets_ = {} ) :
        tick( tick_ ),
        phase( std::move( phase_ ) ),
        camp( std::move( camp_ ) ),
        leads( std::move( leads_ ) ),
        notes( std::move( notes_ ) ),
        cadence( cadence_ ),
        mark_signals( std::move( mark_signals_ ) ),
        smoke_packets( std::move( smoke_packets_ ) ),
        light_packets( std::move( light_packets_ ) ),
        human_route_packets( std::move( human_route_packets_ ) )
    {
    }
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
    bandit_mark_generation::ledger_state generated_marks;
    std::vector<bandit_dry_run::lead_input> generated_leads;
    bandit_dry_run::evaluation_result evaluation;
};

struct playback_result {
    std::string scenario_id;
    std::string title;
    std::vector<std::string> questions;
    std::vector<checkpoint_result> checkpoints;
};

struct proof_packet_result {
    std::string packet_id;
    std::vector<int> checkpoints;
    std::vector<playback_result> scenarios;
};

struct handoff_packet_scenario_result {
    std::string scenario_id;
    std::string title;
    std::string playback_scenario_id;
    std::vector<std::string> questions;
    playback_result playback;
    bandit_pursuit_handoff::entry_payload entry;
    bandit_pursuit_handoff::return_packet local_return;
    bandit_pursuit_handoff::abstract_group_state returned_group;
};

struct handoff_packet_result {
    std::string packet_id;
    std::vector<int> checkpoints;
    std::vector<handoff_packet_scenario_result> scenarios;
};

struct benchmark_check_result {
    int tick = 0;
    std::string label;
    bool passed = false;
    std::string details;
};

struct benchmark_metric {
    std::string name;
    std::string value;
};

struct benchmark_scenario_result {
    std::string benchmark_id;
    std::string benchmark_title;
    playback_result playback;
    std::vector<benchmark_metric> metrics;
    std::vector<benchmark_check_result> benchmark_100;
    std::vector<benchmark_check_result> benchmark_500;
};

struct benchmark_suite_result {
    std::string packet_id;
    std::vector<benchmark_scenario_result> scenarios;
};

struct checkpoint_budget {
    int tick = 0;
    std::string phase;
    bandit_dry_run::evaluation_metrics metrics;
    size_t iterations = 0;
    long long total_runtime_us = 0;
    double average_runtime_us = 0.0;
    size_t checksum = 0;
};

struct scenario_budget {
    std::string scenario_id;
    std::string title;
    std::vector<std::string> questions;
    std::vector<checkpoint_budget> checkpoints;
};

struct persistence_budget_line {
    std::string label;
    size_t bytes = 0;
    size_t count = 0;
    std::string notes;
};

struct persistence_budget {
    std::string sample_shape;
    std::vector<persistence_budget_line> lines;
    std::vector<std::string> assumptions;
    size_t sample_total_bytes = 0;
    std::string verdict;
};

struct reference_suite_budget {
    std::vector<scenario_budget> scenarios;
    persistence_budget persistence;
};

const std::vector<scenario_definition> &reference_scenarios();
const scenario_definition *find_reference_scenario( const std::string &id );
playback_result run_scenario( const scenario_definition &scenario,
                              const std::vector<int> &checkpoints = {} );
proof_packet_result run_first_500_turn_playback_proof();
proof_packet_result run_long_range_directional_light_proof_packet();
proof_packet_result run_elevated_light_z_level_visibility_packet();
proof_packet_result run_overmap_local_pressure_rewrite_proof_packet();
handoff_packet_result run_overmap_local_handoff_interaction_packet();
benchmark_suite_result run_overmap_benchmark_suite_packet();
scenario_budget measure_scenario_budget( const scenario_definition &scenario,
        size_t iterations_per_checkpoint = 1,
        const std::vector<int> &checkpoints = {} );
persistence_budget estimate_v0_persistence_budget();
reference_suite_budget measure_reference_suite_budget( size_t iterations_per_checkpoint = 1 );
std::string render_report( const playback_result &result );
std::string render_first_500_turn_playback_proof( const proof_packet_result &result );
std::string render_long_range_directional_light_proof_packet( const proof_packet_result &result );
std::string render_elevated_light_z_level_visibility_packet( const proof_packet_result &result );
std::string render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result );
std::string render_overmap_local_handoff_interaction_packet( const handoff_packet_result &result );
std::string render_overmap_benchmark_suite_packet( const benchmark_suite_result &result );
std::string render_budget_report( const reference_suite_budget &result );
} // namespace bandit_playback

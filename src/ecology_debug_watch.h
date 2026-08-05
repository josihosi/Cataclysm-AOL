#pragma once
#ifndef CATA_SRC_ECOLOGY_DEBUG_WATCH_H
#define CATA_SRC_ECOLOGY_DEBUG_WATCH_H

#include <optional>
#include <string>
#include <string_view>

#include "ecology_debug_delta.h"

class JsonOut;

namespace ecology_debug
{

enum class watch_preset {
    selected_phase_change,
    evidence_acquired,
    exposure_or_burn,
    casualty,
    return_or_completion,
    no_progress_by_deadline,
};

enum class watch_status {
    watching,
    triggered,
    timed_out,
    consumed,
    completed,
    died,
    anomaly,
    invalid,
};

struct watch_spec {
    watch_preset preset = watch_preset::selected_phase_change;
    trigger_disposition disposition = trigger_disposition::continue_capture;
    // Required for no_progress_by_deadline and optional as a guard for every
    // other preset. This is an absolute game minute.
    std::optional<int> absolute_deadline_minutes;
};

struct watch_input {
    std::optional<selected_projection> prior;
    std::optional<selected_projection> current;
    int now_minutes = -1;
    int armed_minutes = -1;
    // The most recent meaningful transition known by the caller. Callers reset the
    // deadline when this advances; the evaluator never keeps hidden state.
    int last_progress_minutes = -1;
    bool already_triggered = false;
};

struct watch_result {
    watch_status status = watch_status::watching;
    trigger_disposition disposition = trigger_disposition::continue_capture;
    bool newly_triggered = false;
    bool meaningful_progress = false;
    std::string reason;
};

watch_result evaluate_watch( const watch_spec &spec, const watch_input &input );

std::string serialize_watch_json( const watch_spec &spec, const watch_input &input,
                                  const watch_result &result );
void write_watch_json( JsonOut &json, const watch_spec &spec, const watch_input &input,
                       const watch_result &result );
std::string to_string( watch_preset preset );
std::optional<watch_preset> watch_preset_from_string( std::string_view value );
std::string to_string( watch_status status );

} // namespace ecology_debug

#endif // CATA_SRC_ECOLOGY_DEBUG_WATCH_H

#pragma once

#include "coordinates.h"

#include <string>
#include <vector>

class npc;

namespace llm_intent
{
void enqueue_request( npc &listener, const std::string &player_utterance );
void enqueue_request( const npc &listener, const std::string &player_utterance );
void enqueue_ambient_request( npc &listener, const std::string &player_utterance );
void enqueue_requests( const std::vector<npc *> &listeners,
                       const std::string &player_utterance );
void prewarm();
void process_responses();
void enqueue_random_requests();
void log_event( const std::string &message );
std::string build_snapshot_for_test( npc &listener,
                                     const std::string &player_utterance,
                                     const std::string &request_id );
std::string build_action_prompt_for_test( const std::string &npc_name,
        const std::string &player_utterance,
        const std::string &snapshot );
bool parse_move_field_for_test( const std::string &field, point &delta,
                                std::string &terminal_state,
                                std::string &error );
tripoint_abs_ms resolve_move_target_for_test( const tripoint_abs_ms &origin,
        const point &delta );
} // namespace llm_intent

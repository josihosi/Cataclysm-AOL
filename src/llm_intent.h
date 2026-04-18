#pragma once

#include "coordinates.h"

#include <functional>
#include <string>
#include <vector>

class npc;

namespace llm_intent
{
struct dialogue_chat_tool {
    std::string id;
    std::string label;
    std::string hint;
    std::string note;
    bool available = true;
    std::string unavailable_reason;
};

struct dialogue_chat_action_surface {
    std::vector<dialogue_chat_tool> sandbox_actions;
    std::vector<dialogue_chat_tool> branch_actions;
};

struct dialogue_chat_result {
    bool ok = false;
    std::string say;
    std::string tool;
    std::string raw;
    std::string error;
};

void enqueue_request( npc &listener, const std::string &player_utterance );
void enqueue_request( const npc &listener, const std::string &player_utterance );
void enqueue_ambient_request( npc &listener, const std::string &player_utterance );
void enqueue_requests( const std::vector<npc *> &listeners,
                       const std::string &player_utterance );
void prewarm();
void process_responses();
void enqueue_random_requests();
void log_event( const std::string &message );
dialogue_chat_result request_dialogue_chat( npc &listener,
        const std::string &player_utterance,
        const std::string &authored_npc_line,
        const dialogue_chat_action_surface &action_surface,
        const std::string &relationship_memory,
        bool opening_turn,
        const std::function<void( const std::string & )> &on_partial_say = nullptr );
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

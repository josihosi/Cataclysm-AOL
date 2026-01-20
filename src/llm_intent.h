#pragma once

#include <string>
#include <vector>

class npc;

namespace llm_intent
{
void enqueue_request( npc &listener, const std::string &player_utterance );
void enqueue_request( const npc &listener, const std::string &player_utterance );
void enqueue_requests( const std::vector<npc *> &listeners,
                       const std::string &player_utterance );
void prewarm();
void process_responses();
void log_event( const std::string &message );
} // namespace llm_intent

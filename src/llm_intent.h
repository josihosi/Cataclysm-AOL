#pragma once

#include <string>

class npc;

namespace llm_intent
{
void enqueue_request( npc &listener, const std::string &player_utterance );
void enqueue_request( const npc &listener, const std::string &player_utterance );
void prewarm();
void process_responses();
} // namespace llm_intent

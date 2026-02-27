#include "llm_intent.h"

#include "npc.h"

namespace llm_intent
{
void enqueue_request( npc &, const std::string & )
{
}

void enqueue_request( const npc &, const std::string & )
{
}

void enqueue_requests( const std::vector<npc *> &, const std::string & )
{
}

void prewarm()
{
}

void process_responses()
{
}

void enqueue_random_requests()
{
}

void log_event( const std::string & )
{
}
} // namespace llm_intent

#pragma once
#ifndef CATA_SRC_NPC_INSPECTION_H
#define CATA_SRC_NPC_INSPECTION_H

#include <map>
#include <string>
#include <vector>

#include "character_id.h"
#include "semantic_surface.h"

class avatar;
class npc;

std::string npc_inspection_actor_id( const npc &actor );
std::vector<semantic_action_descriptor> npc_inspection_world_actions( avatar &viewer );
std::vector<semantic_action_descriptor> npc_inspection_current_camp_actions( avatar &viewer );
npc *resolve_npc_inspection_actor( avatar &viewer, const std::string &actor_id );
std::map<std::string, std::string> npc_inspection_payload( npc &actor, avatar &viewer );
// An empty payload means this UID is no longer owned by this exact actor.
std::map<std::string, std::string> npc_inspection_item_payload( npc &actor,
        const std::string &item_uid );
void show_npc_inspection( character_id actor_id );
// Assigned camp workers can be outside the loaded local map while they are
// performing an actual camp order.  Their owning npc_ptr is retained by the
// caller for the duration of this diagnostic UI, so this overload may inspect
// that validated object without treating its absence from the visible set as
// a stale identity.
void show_npc_inspection( npc &actor, bool allow_offbubble );

#endif // CATA_SRC_NPC_INSPECTION_H

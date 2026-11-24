#pragma once
#ifndef CATA_SRC_NPC_HOME_BODY_H
#define CATA_SRC_NPC_HOME_BODY_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "creature.h"
#include "effect_source.h"
#include "type_id.h"
#include "units.h"

class JsonObject;
class JsonOut;

enum class npc_physiological_death_cause {
    stimulant_overdose,
    respiratory_overdose,
    jetinjector_overdose,
    adrenaline_overdose,
    alcohol_overdose,
    starvation,
    dehydration
};

std::string_view npc_physiological_death_id( npc_physiological_death_cause cause );
std::optional<npc_physiological_death_cause> npc_physiological_death_from_id( std::string_view id );

// Persist home work that needs the real local map. Selected deaths and
// interrupted effects share the pending_home_death save field.
struct npc_home_pending_death {
    tripoint_abs_ms position = tripoint_abs_ms::zero;
    time_point applied_until = calendar::turn_zero;
    time_point work_end = calendar::turn_zero;
    bool from_body_interval = false;
    std::optional<time_point> resume_at;
    std::string cause;
    std::vector<std::string> additional_physiological_causes;
    std::string effect_phase;
    efftype_id effect;
    bodypart_id effect_bodypart;
    int next_effect_index = 0;
    std::vector<efftype_id> pending_removals;
    std::vector<bodypart_id> pending_removal_parts;
    std::vector<scheduled_effect> queued_additions;
    std::vector<terminating_effect> queued_removals;
    effect_source source;

    bool needs_local_effects() const {
        return effect_phase == "local_effect_turn" || effect_phase == "local_effect_decay" ||
               effect_phase == "local_effect_queue";
    }
    bool has_physiological_causes() const;
    bool has_supported_queued_effects() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &data );
};

class Character;
class npc;
struct npc_home_body_context;

// Scoped only around one authenticated offscreen home effect step.
class scoped_npc_home_effects
{
    public:
        scoped_npc_home_effects( const Character &actor, std::string causal_key,
                                 bool capture_fatal_effect = false, npc_home_body_context *body = nullptr );
        ~scoped_npc_home_effects();
        scoped_npc_home_effects( const scoped_npc_home_effects & ) = delete;
        scoped_npc_home_effects &operator=( const scoped_npc_home_effects & ) = delete;
        const Character *actor;
        std::string causal_key;
        int sequence = 0;
        bool capture_fatal_effect = false;
        npc_home_body_context *body;
    private:
        scoped_npc_home_effects *previous;
};
bool npc_home_effects_active( const Character &actor );
npc_home_body_context *npc_home_effect_context( const Character &actor );
bool npc_home_body_effect_supported( const efftype_id &id, bool expedition );
bool npc_home_capture_fatal_effect( const Character &actor );
// One-shot fixture input at the first native effect turn of this resident.
void set_first_npc_home_effect_turn_for_test( character_id id, void ( *prepare )( const npc & ) );
void npc_home_queue_fungal_spore( const Character &actor, const tripoint_abs_ms &origin );
void npc_home_queue_tindalos_rift( const Character &actor, const tripoint_abs_ms &origin );

namespace bandit_live_world
{
enum class hostile_site_profile;
}

// A dispatch-time estimate, not evidence about a particular generated tile
// and not a persistent sleep effect on the resident.
struct npc_home_body_environment {
    int sleep_comfort = 0;
    float sleeping_rest_quality = 0.0f;
    bool sheltered = false;
    bool seating = false;
    bool terrain_known = false;
};

// Transient, caller-owned physiology inputs; never serialized as NPC sleep.
struct npc_home_body_context {
    npc_home_body_environment environment;
    units::temperature area_temperature;
    bool asleep = true;
    std::vector<npc_physiological_death_cause> *physiological_deaths = nullptr;
    bool lethal_fungal_damage = false;
    bool deferred_effect_queue = false;
    bool expedition = false;

    float estimated_light( const Character &actor ) const;
};

std::optional<npc_home_body_environment> estimate_npc_home_body_environment(
    bandit_live_world::hostile_site_profile profile, const oter_id &terrain );

#endif // CATA_SRC_NPC_HOME_BODY_H

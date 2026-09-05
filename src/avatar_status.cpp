#include "avatar_status.h"

#include <sstream>
#include <utility>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "color.h"
#include "display.h"
#include "effect.h"
#include "item.h"
#include "item_location.h"
#include "json.h"

std::map<std::string, std::string> avatar_status_payload( const avatar &viewer )
{
    const std::string actor_id = "character:" + std::to_string( viewer.getID().get_value() );
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    std::ostringstream status;
    JsonOut json( status );
    json.start_object();
    json.member( "schema", "caol-avatar-status-v1" );
    json.member( "actor_id", actor_id );
    json.member( "observed_turn", turn );
    json.member( "provenance" );
    json.start_object();
    json.member( "health", "Native Character body-part HP getters used by sidebar widgets; exact native values, not a claim that the current medical view displays numbers." );
    json.member( "stamina", "Character::get_stamina/get_stamina_max, as used by sidebar widgets." );
    json.member( "needs", "Native display::*_text_color labels and colors; blank text remains blank." );
    json.member( "weapon", "Character::get_wielded_item and weapname_simple/mode/ammo; ammo text preserves native firing-mode and pocket handling." );
    json.end_object();
    json.member( "health" );
    json.start_object();
    json.member( "unit", "hp" );
    json.member( "body_parts" );
    json.start_object();
    for( const bodypart_id &part : viewer.get_all_body_parts() ) {
        json.member( part.id().str() );
        json.start_object();
        json.member( "name", body_part_name_as_heading( part, 1 ) );
        json.member( "current", viewer.get_part_hp_cur( part ) );
        json.member( "maximum", viewer.get_part_hp_max( part ) );
        json.end_object();
    }
    json.end_object();
    json.end_object();
    json.member( "stamina" );
    json.start_object();
    json.member( "current", viewer.get_stamina() );
    json.member( "maximum", viewer.get_stamina_max() );
    json.member( "unit", "native_stamina_points" );
    json.end_object();
    json.member( "needs" );
    json.start_object();
    const auto label = [&]( const std::string &name, const std::pair<std::string, nc_color> &value ) {
        json.member( name );
        json.start_object();
        json.member( "text", value.first );
        json.member( "color", string_from_color( value.second ) );
        json.end_object();
    };
    label( "hunger", display::hunger_text_color( viewer ) );
    label( "thirst", display::thirst_text_color( viewer ) );
    label( "sleepiness", display::sleepiness_text_color( viewer ) );
    label( "pain", display::pain_text_color( viewer ) );
    label( "weariness", display::weariness_text_color( viewer ) );
    label( "body_temperature", display::temp_text_color( viewer ) );
    json.end_object();
    json.member( "weapon" );
    json.start_object();
    const item_location weapon = viewer.get_wielded_item();
    if( weapon ) {
        json.member( "item_uid", std::to_string( weapon->uid().get_value() ) );
        json.member( "type_id", weapon->typeId().str() );
    } else {
        json.member( "item_uid" );
        json.write_null();
        json.member( "type_id" );
        json.write_null();
    }
    json.member( "name", viewer.weapname_simple() );
    json.member( "mode", viewer.weapname_mode() );
    json.member( "ammo", viewer.weapname_ammo() );
    json.end_object();
    json.end_object();

    // IDs preserve separate occurrences on different limbs, even when labels
    // match.  This is the named current-effect list, not disp_info's additional
    // synthesized pain, starvation, withdrawal and enchantment entries.
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> effects;
    for( const effect &entry : viewer.get_effects() ) {
        const std::string name = entry.disp_name();
        if( name.empty() ) {
            continue;
        }
        const std::string part = entry.get_bp() == bodypart_str_id::NULL_ID() ?
                                 "whole_body" : entry.get_bp().id().str();
        effects[entry.get_id().str()][part] = {
            { "name", name }, { "description", entry.disp_desc() },
            { "mod_source", entry.disp_mod_source_info() }
        };
    }
    std::ostringstream effect_text;
    JsonOut effect_json( effect_text );
    effect_json.start_object();
    effect_json.member( "schema", "caol-avatar-named-effects-v1" );
    effect_json.member( "actor_id", actor_id );
    effect_json.member( "observed_turn", turn );
    effect_json.member( "provenance", "Creature::get_effects with nonempty effect::disp_name, disp_desc and disp_mod_source_info, as read by player info. Named current effects only; excludes synthesized Effects-tab entries." );
    effect_json.member( "entries", effects );
    effect_json.end_object();
    return { { "avatar_status", status.str() }, { "avatar_effects", effect_text.str() } };
}

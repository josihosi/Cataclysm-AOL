#include <string>

#include "avatar.h"
#include "avatar_status.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_catch.h"
#include "color.h"
#include "display.h"
#include "effect.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "type_id.h"

TEST_CASE( "avatar status retains native health needs and named limb effects without a turn",
           "[semantic_surface][avatar_status]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const bodypart_id torso = bodypart_str_id( "torso" ).id();
    const bodypart_id left = bodypart_str_id( "arm_l" ).id();
    const bodypart_id right = bodypart_str_id( "arm_r" ).id();
    you.set_part_hp_cur( torso, 37 );
    you.set_stamina( 1234 );
    you.set_thirst( 250 );
    you.set_sleepiness( 0 );
    you.set_pain( 15 );
    you.add_effect( efftype_id( "bleed" ), 10_minutes, left );
    you.add_effect( efftype_id( "bleed" ), 10_minutes, right );
    const int moves = you.get_moves();
    const time_point turn = calendar::turn;
    const auto facts = avatar_status_payload( you );
    CHECK( facts == avatar_status_payload( you ) );
    CHECK( you.get_moves() == moves );
    CHECK( calendar::turn == turn );

    const JsonObject status = json_loader::from_string( facts.at( "avatar_status" ) );
    status.allow_omitted_members();
    CHECK( status.get_string( "actor_id" ) == "character:" + std::to_string( you.getID().get_value() ) );
    CHECK( status.get_int( "observed_turn" ) == to_turns<int>( turn - calendar::turn_zero ) );
    const JsonObject health = status.get_object( "health" );
    CHECK( health.get_string( "unit" ) == "hp" );
    const JsonObject parts = health.get_object( "body_parts" );
    parts.allow_omitted_members();
    const JsonObject torso_status = parts.get_object( "torso" );
    CHECK( torso_status.get_string( "name" ) == body_part_name_as_heading( torso, 1 ) );
    CHECK( torso_status.get_int( "current" ) == 37 );
    CHECK( torso_status.get_int( "maximum" ) == you.get_part_hp_max( torso ) );
    const JsonObject stamina = status.get_object( "stamina" );
    CHECK( stamina.get_int( "current" ) == 1234 );
    CHECK( stamina.get_int( "maximum" ) == you.get_stamina_max() );
    CHECK( stamina.get_string( "unit" ) == "native_stamina_points" );
    const JsonObject needs = status.get_object( "needs" );
    needs.allow_omitted_members();
    const JsonObject thirst = needs.get_object( "thirst" );
    const auto thirst_label = display::thirst_text_color( you );
    CHECK( thirst.get_string( "text" ) == thirst_label.first );
    CHECK( thirst.get_string( "color" ) == string_from_color( thirst_label.second ) );
    const JsonObject sleepiness = needs.get_object( "sleepiness" );
    sleepiness.allow_omitted_members();
    CHECK( sleepiness.get_string( "text" ).empty() );
    const JsonObject pain = needs.get_object( "pain" );
    pain.allow_omitted_members();
    CHECK( pain.get_string( "text" ) == display::pain_text_color( you ).first );

    const JsonObject effects = json_loader::from_string( facts.at( "avatar_effects" ) );
    effects.allow_omitted_members();
    CHECK( effects.get_string( "provenance" ).find( "excludes synthesized" ) != std::string::npos );
    const JsonObject entries = effects.get_object( "entries" );
    entries.allow_omitted_members();
    const JsonObject bleeding = entries.get_object( "bleed" );
    for( const bodypart_id &part : { left, right } ) {
        const JsonObject entry = bleeding.get_object( part.id().str() );
        const effect &native = you.get_effect( efftype_id( "bleed" ), part );
        CHECK( entry.get_string( "name" ) == native.disp_name() );
        CHECK( entry.get_string( "description" ) == native.disp_desc() );
        CHECK( entry.get_string( "mod_source" ) == native.disp_mod_source_info() );
    }
    you.set_stamina( 4321 );
    CHECK( avatar_status_payload( you ) != facts );
}

TEST_CASE( "avatar status follows the actual wielded item and native ammunition formatter",
           "[semantic_surface][avatar_status]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const JsonObject unarmed_status = json_loader::from_string(
                                         avatar_status_payload( you ).at( "avatar_status" ) );
    unarmed_status.allow_omitted_members();
    const JsonObject unarmed = unarmed_status.get_object( "weapon" );
    unarmed.allow_omitted_members();
    CHECK( unarmed.get_member( "item_uid" ).test_null() );
    CHECK( unarmed.get_member( "type_id" ).test_null() );
    CHECK( unarmed.get_string( "name" ) == you.weapname_simple() );
    CHECK( unarmed.get_string( "ammo" ).empty() );

    item gun( itype_id( "glock_19" ) );
    gun.ammo_set( gun.ammo_default(), 7 );
    REQUIRE( you.wield( gun ) );
    const item_location wielded = you.get_wielded_item();
    REQUIRE( wielded );
    REQUIRE( wielded->ammo_remaining() == 7 );
    const JsonObject armed_status = json_loader::from_string(
                                       avatar_status_payload( you ).at( "avatar_status" ) );
    armed_status.allow_omitted_members();
    const JsonObject armed = armed_status.get_object( "weapon" );
    CHECK( armed.get_string( "item_uid" ) == std::to_string( wielded->uid().get_value() ) );
    CHECK( armed.get_string( "type_id" ) == "glock_19" );
    CHECK( armed.get_string( "name" ) == you.weapname_simple() );
    CHECK( armed.get_string( "mode" ) == you.weapname_mode() );
    CHECK( armed.get_string( "ammo" ) == you.weapname_ammo() );
    CHECK_FALSE( armed.get_string( "ammo" ).empty() );
}

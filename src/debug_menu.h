#pragma once
#ifndef CATA_SRC_DEBUG_MENU_H
#define CATA_SRC_DEBUG_MENU_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "coordinates.h"  // IWYU pragma: keep
#include "type_id.h"
#include "debug_menu_types.h"  // IWYU pragma: export

class Character;
class Creature;
struct mongroup;

namespace debug_menu
{

struct overmap_spawn_option {
    std::string id;
    std::string label;
    mongroup_id group;
    int population = 1;
    int distance_omt = 10;
    int hotkey = 0;
};

struct debug_item_spawn_request {
    itype_id type;
    int quantity = 0;
    int charges = 0;
    int damage = 0;
    faction_id owner;
    tripoint_bub_ms destination;
    std::string transaction_id;
};

struct debug_item_spawn_identity {
    int ordinal = 0;
    itype_id type;
    int charges = 0;
    int damage = 0;
    faction_id owner;
};

struct debug_item_spawn_receipt {
    bool accepted = false;
    bool audit_passed = false;
    bool zero_credit = true;
    std::string transaction_id;
    std::string provenance = "debug_item_spawn_transaction: zero-credit setup mutation";
    std::string failure;
    std::vector<debug_item_spawn_identity> identities;
};

struct debug_item_spawn_cleanup_receipt {
    bool accepted = false;
    bool audit_passed = false;
    bool zero_credit = true;
    std::string transaction_id;
    std::string provenance = "debug_item_spawn_transaction cleanup: zero-credit setup mutation";
    std::string failure;
    int removed = 0;
    int retained_untagged = 0;
};

std::vector<overmap_spawn_option> overmap_spawn_options();
tripoint_abs_sm overmap_spawn_destination( const tripoint_abs_ms &player_abs_ms, int distance_omt );
void spawn_overmap_threat( const overmap_spawn_option &option );

void wisheffect( Creature &p );
void wishitem( Character *you = nullptr );
void wishitem( Character *you, const tripoint_bub_ms & );
debug_item_spawn_receipt debug_item_spawn_transaction( const debug_item_spawn_request &request );
debug_item_spawn_cleanup_receipt debug_item_spawn_transaction_cleanup(
    const debug_item_spawn_request &request );
// Shows a menu to debug item groups. Spawns items if test is false, otherwise displays would be spawned items.
void wishitemgroup( bool test );
void wishmonster( const std::optional<tripoint_bub_ms> &p );
void wishmonstergroup( tripoint_abs_omt &loc );
void wishmonstergroup_mon_selection( mongroup &group );
void wishmutate( Character *you );
void wishbionics( Character *you );
/*
 * Set skill on any Character object; player character or NPC
 * Can change skill theory level
 */
void wishskill( Character *you, bool change_theory = false );
/*
 * Set proficiency on any Character object; player character or NPC
 */
void wishproficiency( Character *you );

void debug();
std::optional<debug_menu_index> choose_action();
void execute_action( debug_menu_index action );
void open_console();

void export_save_archive_and_game_report();

void do_debug_quick_setup( bool flag_dirty = false );

/* Splits a string by @param delimiter and push_back's the elements into _Container */
template<typename Container>
Container string_to_iterable( const std::string_view str, const std::string_view delimiter )
{
    Container res;

    size_t pos = 0;
    size_t start = 0;
    while( ( pos = str.find( delimiter, start ) ) != std::string::npos ) {
        if( pos > start ) {
            res.emplace_back( str.substr( start, pos - start ) );
        }
        start = pos + delimiter.length();
    }
    if( start != str.length() ) {
        res.emplace_back( str.substr( start, str.length() - start ) );
    }

    return res;
}

bool is_debug_character();
void prompt_map_reveal( const std::optional<tripoint_abs_omt> &p = std::nullopt );
void map_reveal( int reveal_level_int, const std::optional<tripoint_abs_omt> &p = std::nullopt );

/* Merges iterable elements into std::string with
 * @param delimiter between them
 * @param f is callable that is called to transform each value
 * */
template<typename Container, typename Mapper>
std::string iterable_to_string( const Container &values, const std::string_view delimiter,
                                const Mapper &f )
{
    std::string res;
    for( auto iter = values.begin(); iter != values.end(); ++iter ) {
        if( iter != values.begin() ) {
            res += delimiter;
        }
        res += f( *iter );
    }
    return res;
}

template<typename Container>
std::string iterable_to_string( const Container &values, const std::string_view delimiter )
{
    return iterable_to_string( values, delimiter, []( const std::string_view f ) {
        return f;
    } );
}

} // namespace debug_menu

#endif // CATA_SRC_DEBUG_MENU_H

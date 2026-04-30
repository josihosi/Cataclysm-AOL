#include <optional>

#include "calendar.h"
#include "cata_catch.h"
#include "damage.h"
#include "mongroup.h"
#include "mtype.h"
#include "type_id.h"

static const damage_type_id damage_cut( "cut" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );

TEST_CASE( "writhing_stalker_monster_footing", "[writhing_stalker][monster]" )
{
    const mtype &stalker = *mon_writhing_stalker;

    CHECK( stalker.in_species( species_ZOMBIE ) );
    CHECK( stalker.in_species( species_HUMAN ) );
    CHECK( stalker.hp >= 85 );
    CHECK( stalker.hp <= 100 );
    CHECK( stalker.speed >= 115 );
    CHECK( stalker.speed <= 125 );
    CHECK( stalker.sk_dodge >= 4 );
    CHECK( stalker.sk_dodge <= 5 );
    CHECK( stalker.melee_damage.type_damage( damage_cut ) >= 4.0f );
    CHECK( stalker.has_special_attack( "scratch" ) );
    CHECK( stalker.has_special_attack( "bite" ) );
    CHECK( stalker.has_flag( mon_flag_KEENNOSE ) );
    CHECK( stalker.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( stalker.has_flag( mon_flag_HARDTOSHOOT ) );
    CHECK( stalker.has_flag( mon_flag_HIT_AND_RUN ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GRABS ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GROUP_BASH ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::STALK ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::HOSTILE_WEAK ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::HURT ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::BRIGHT_LIGHT ) );
    CHECK_FALSE( stalker.upgrades );
}

TEST_CASE( "writhing_stalker_spawn_footing_is_rare_singleton", "[writhing_stalker][monster][mongroup]" )
{
    const MonsterGroup &group = GROUP_ZOMBIE.obj();

    std::optional<MonsterGroupEntry> stalker_entry;
    int direct_entries = 0;
    int total_direct_weight = 0;
    for( const MonsterGroupEntry &entry : group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        total_direct_weight += entry.frequency;
        if( entry.mtype == mon_writhing_stalker ) {
            stalker_entry = entry;
            direct_entries++;
        }
    }

    REQUIRE( direct_entries == 1 );
    REQUIRE( stalker_entry.has_value() );
    CHECK( stalker_entry->frequency == 1 );
    CHECK( stalker_entry->cost_multiplier >= 50 );
    CHECK( stalker_entry->pack_minimum == 1 );
    CHECK( stalker_entry->pack_maximum == 1 );
    CHECK( stalker_entry->starts == 0_turns );
    CHECK( total_direct_weight > 9000 );
    CHECK( stalker_entry->frequency * 5000 < total_direct_weight );
}

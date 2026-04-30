#include <optional>
#include <string>

#include "calendar.h"
#include "cata_catch.h"
#include "mongroup.h"
#include "mtype.h"
#include "type_id.h"

static const itype_id arrow_wood( "arrow_wood" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mongroup_id GROUP_ZOMBIE_UPGRADE( "GROUP_ZOMBIE_UPGRADE" );
static const mtype_id mon_zombie_hunter( "mon_zombie_hunter" );
static const mtype_id mon_zombie_predator( "mon_zombie_predator" );
static const mtype_id mon_zombie_rider( "mon_zombie_rider" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );

TEST_CASE( "zombie_rider_monster_footing", "[zombie_rider][monster]" )
{
    const mtype &rider = *mon_zombie_rider;
    const std::string exact_description =
        "Up on, what can only be described as, a six legged horse - or is it a spider? - a towering figure, with eyes the color of blood, holds a gory bow of wet bones and sinews.\n"
        "It's movement ferocious, as the tumbling feet hasten across the terrain.\n"
        "Running is out of the question.";

    CHECK( rider.nname() == "zombie rider" );
    CHECK( rider.get_description() == exact_description );
    CHECK( rider.in_species( species_ZOMBIE ) );
    CHECK_FALSE( rider.in_species( species_HUMAN ) );
    CHECK( rider.size > creature_size::medium );
    CHECK( rider.speed >= 150 );
    CHECK( rider.speed <= 200 );
    CHECK( rider.hp >= 200 );
    CHECK( rider.has_special_attack( "zombie_rider_bone_bow_shot" ) );
    CHECK( rider.starting_ammo.count( arrow_wood ) == 1 );
    CHECK( rider.starting_ammo.at( arrow_wood ) >= 12 );
    CHECK( rider.has_flag( mon_flag_RANGED_ATTACKER ) );
    CHECK( rider.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( rider.has_flag( mon_flag_HARDTOSHOOT ) );
    CHECK_FALSE( rider.has_flag( mon_flag_BASHES ) );
    CHECK_FALSE( rider.has_flag( mon_flag_GROUP_BASH ) );
    CHECK_FALSE( rider.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( rider.upgrades );
}

TEST_CASE( "zombie_rider_endpoint_spawn_and_evolution_gate", "[zombie_rider][monster][mongroup]" )
{
    constexpr time_duration mature_world_gate = 730_days;
    const mtype &hunter = *mon_zombie_hunter;
    const mtype &predator = *mon_zombie_predator;

    REQUIRE( hunter.upgrades );
    CHECK( hunter.upgrade_into == mon_zombie_predator );
    CHECK_FALSE( predator.upgrades );
    CHECK( predator.upgrade_into != mon_zombie_rider );
    CHECK_FALSE( GROUP_ZOMBIE_UPGRADE->IsMonsterInGroup( mon_zombie_rider ) );

    const MonsterGroup &zombie_group = GROUP_ZOMBIE.obj();
    std::optional<MonsterGroupEntry> rider_entry;
    int direct_entries = 0;
    for( const MonsterGroupEntry &entry : zombie_group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        if( entry.mtype == mon_zombie_rider ) {
            rider_entry = entry;
            direct_entries++;
        }
    }

    REQUIRE( direct_entries == 1 );
    REQUIRE( rider_entry.has_value() );
    CHECK( rider_entry->frequency == 1 );
    CHECK( rider_entry->cost_multiplier >= 80 );
    CHECK( rider_entry->pack_minimum == 1 );
    CHECK( rider_entry->pack_maximum == 1 );
    CHECK( rider_entry->starts >= mature_world_gate );

    int all_direct_entries = 0;
    for( const auto &group_pair : MonsterGroupManager::Get_all_Groups() ) {
        for( const MonsterGroupEntry &entry : group_pair.second.monsters ) {
            if( entry.is_group() || entry.mtype != mon_zombie_rider ) {
                continue;
            }
            all_direct_entries++;
            CHECK( entry.starts >= mature_world_gate );
            CHECK( entry.pack_minimum == 1 );
            CHECK( entry.pack_maximum == 1 );
        }
    }
    CHECK( all_direct_entries == 1 );
}

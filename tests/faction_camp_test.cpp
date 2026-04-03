#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "basecamp.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "clzones.h"
#include "coordinates.h"
#include "faction.h"
#include "item.h"
#include "item_components.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "point.h"
#include "stomach.h"
#include "type_id.h"
#include "value_ptr.h"

static const itype_id itype_test_100_kcal( "test_100_kcal" );
static const itype_id itype_test_200_kcal( "test_200_kcal" );
static const itype_id itype_test_500_kcal( "test_500_kcal" );

static const vitamin_id vitamin_mutagen( "mutagen" );
static const vitamin_id vitamin_mutant_toxin( "mutant_toxin" );

static const zone_type_id zone_type_CAMP_FOOD( "CAMP_FOOD" );
static const zone_type_id zone_type_CAMP_STORAGE( "CAMP_STORAGE" );

TEST_CASE( "camp_calorie_counting", "[camp]" )
{
    clear_avatar();
    clear_map_without_vision();
    map &m = get_map();
    const tripoint_abs_ms zone_loc = m.get_abs( tripoint_bub_ms{ 5, 5, 0 } );
    REQUIRE( m.inbounds( zone_loc ) );
    mapgen_place_zone( zone_loc, zone_loc, zone_type_CAMP_FOOD, your_fac, {},
                       "food" );
    mapgen_place_zone( zone_loc, zone_loc, zone_type_CAMP_STORAGE, your_fac, {},
                       "storage" );
    faction *camp_faction = get_player_character().get_faction();
    const tripoint_abs_omt this_omt = project_to<coords::omt>( zone_loc );
    m.add_camp( this_omt, "faction_camp" );
    std::optional<basecamp *> bcp = overmap_buffer.find_camp( this_omt.xy() );
    REQUIRE( !!bcp );
    basecamp *test_camp = *bcp;
    test_camp->set_owner( your_fac );
    WHEN( "a base item is added to larder" ) {
        camp_faction->empty_food_supply();
        item test_100_kcal( itype_test_100_kcal );
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
        m.add_item_or_charges( zone_local, test_100_kcal );
        REQUIRE( m.has_items( zone_local ) );
        test_camp->distribute_food();
        CHECK( camp_faction->food_supply().kcal() == 100 );
    }

    WHEN( "an item with inherited components is added to larder" ) {
        camp_faction->empty_food_supply();
        item test_100_kcal( itype_test_100_kcal );
        item test_200_kcal( itype_test_200_kcal );
        item_components made_of;
        made_of.add( test_100_kcal );
        made_of.add( test_100_kcal );
        // Setting the actual components. This will return 185 unless it's actually made up of two 100kcal components!
        test_200_kcal.components = made_of;
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
        m.add_item_or_charges( zone_local, test_200_kcal );
        test_camp->distribute_food();
        CHECK( camp_faction->food_supply().kcal() == 200 );
    }

    WHEN( "an item with vitamins is added to larder" ) {
        camp_faction->empty_food_supply();
        item test_500_kcal( itype_test_500_kcal );
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
        m.add_item_or_charges( zone_local, test_500_kcal );
        test_camp->distribute_food();
        REQUIRE( camp_faction->food_supply().kcal() == 500 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 100 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 200 );
    }

    WHEN( "a larder with stored calories and vitamins has food withdrawn" ) {
        camp_faction->empty_food_supply();
        std::map<time_point, nutrients> added_food;
        added_food[calendar::turn_zero].calories = 100000;
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutant_toxin, 100 );
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutagen, 200 );
        camp_faction->add_to_food_supply( added_food );
        REQUIRE( camp_faction->food_supply().kcal() == 100 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 100 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 200 );
        // Now withdraw 15% of the total calories, this should also draw out 15% of the stored vitamins.
        test_camp->camp_food_supply( -15 );
        CHECK( camp_faction->food_supply().kcal() == 85 );
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 85 );
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 170 );
    }

    WHEN( "a larder with perishable food passes the expiry date" ) {
        restore_on_out_of_scope restore_calendar_turn( calendar::turn );
        camp_faction->empty_food_supply();
        // non-perishable food
        std::map<time_point, nutrients> added_food;
        added_food[calendar::turn_zero].calories = 100000;
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutant_toxin, 100 );
        added_food[calendar::turn_zero].set_vitamin( vitamin_mutagen, 200 );

        camp_faction->add_to_food_supply( added_food );
        REQUIRE( camp_faction->food_supply().kcal() == 100 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 100 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 200 );

        // remove non-perishable from added
        added_food.erase( added_food.begin() );
        // perishable food
        added_food[calendar::turn + 7_days].calories = 150000;
        added_food[calendar::turn + 7_days].set_vitamin( vitamin_mutant_toxin, 200 );
        added_food[calendar::turn + 7_days].set_vitamin( vitamin_mutagen, 100 );

        camp_faction->add_to_food_supply( added_food );
        REQUIRE( camp_faction->food_supply().kcal() == 250 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 300 );
        REQUIRE( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 300 );

        // advance time
        calendar::turn += 14_days;

        CHECK( camp_faction->food_supply().kcal() == 100 );
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutant_toxin ) == 100 );
        CHECK( camp_faction->food_supply().get_vitamin( vitamin_mutagen ) == 200 );
    }

    WHEN( "an item that expires is added to larder" ) {
        restore_on_out_of_scope restore_calendar_turn( calendar::turn );
        camp_faction->empty_food_supply();
        item test_100_kcal( itype_test_100_kcal );
        tripoint_bub_ms zone_local = m.get_bub( zone_loc );
        m.i_clear( zone_local );
        m.add_item_or_charges( zone_local, test_100_kcal );
        test_camp->distribute_food();
        REQUIRE( camp_faction->food_supply().kcal() == 100 );
        REQUIRE( test_100_kcal.type->comestible != nullptr );
        REQUIRE( test_100_kcal.type->comestible->spoils == 360_days );

        calendar::turn += 365_days;

        CHECK( camp_faction->food_supply().kcal() == 0 );
    }
    overmap_buffer.clear_camps( this_omt.xy() );
}
TEST_CASE( "camp_request_speech_parsing", "[camp][basecamp_ai]" )
{
    using basecamp_ai::parse_heard_camp_approval_query;
    using basecamp_ai::parse_heard_camp_cancel_query;
    using basecamp_ai::parse_heard_camp_craft_order;
    using basecamp_ai::match_camp_craft_query;
    using basecamp_ai::parse_heard_camp_status_query;
    using basecamp_ai::score_camp_recipe_query;

    SECTION( "craft orders parse quantity words and direct object" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_heard_camp_craft_order( "please craft me five bandages" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 5 );
        CHECK( parsed->item_query == "bandages" );
    }

    SECTION( "spoken craft intake requires the exact standalone word craft" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_heard_camp_craft_order( "craft knife" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 1 );
        CHECK( parsed->item_query == "knife" );
    }

    SECTION( "spoken craft intake rejects non craft verbs and partial-word matches" ) {
        CHECK_FALSE( parse_heard_camp_craft_order( "make knife" ).has_value() );
        CHECK_FALSE( parse_heard_camp_craft_order( "build knife" ).has_value() );
        CHECK_FALSE( parse_heard_camp_craft_order( "witchcraft knife" ).has_value() );
    }

    SECTION( "approval commands parse explicit request numbers" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_approval_query( "approve request #12 please" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->has_request_id );
        CHECK( parsed->request_id == 12 );
        CHECK_FALSE( parsed->all_requests );
    }

    SECTION( "approval commands can target all ready work orders" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_approval_query( "launch all ready work orders" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "status queries can ask for the whole board" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_status_query( "what's on the board" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "cancel commands keep the work-order subject" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_cancel_query( "cancel the work order for long string please" );
        REQUIRE( parsed.has_value() );
        CHECK_FALSE( parsed->has_request_id );
        CHECK( parsed->query == "long string" );
    }

    SECTION( "craft router prefers specific phrase matches over generic noun matches" ) {
        const recipe &plain_bandages = recipe_id( "bandages" ).obj();
        const recipe &boiled_bandages = recipe_id( "bandages_makeshift_boiled" ).obj();

        CHECK( score_camp_recipe_query( boiled_bandages, "boiled bandages" ) >
               score_camp_recipe_query( plain_bandages, "boiled bandages" ) );
        CHECK( score_camp_recipe_query( plain_bandages, "bandages" ) >
               score_camp_recipe_query( boiled_bandages, "bandages" ) );
    }

    SECTION( "craft router uses exact words instead of partial-word substring matches" ) {
        const recipe &plain_bandages = recipe_id( "bandages" ).obj();
        CHECK( score_camp_recipe_query( plain_bandages, "band" ) == 0 );
        CHECK( score_camp_recipe_query( plain_bandages, "bandages" ) > 0 );
    }

    SECTION( "craft router reports ambiguous top subject matches instead of guessing" ) {
        const std::unordered_set<recipe_id> recipes = {
            recipe_id( "bandages_makeshift_boiled" ),
            recipe_id( "potato_boiled" )
        };

        const basecamp_ai::camp_craft_recipe_match match = match_camp_craft_query( recipes, "boiled" );
        CHECK( match.score >= 650 );
        CHECK( match.recipe_ids.size() == 2 );
        CHECK( match.subjects == std::vector<std::string> { "boiled makeshift bandage", "boiled potato" } );
    }
}

// TODO: Tests for: Check calorie display at various activity levels, camp crafting works as expected (consumes inputs, returns outputs+byproducts, costs calories)

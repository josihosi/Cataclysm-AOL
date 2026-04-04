#include <algorithm>
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
    using basecamp_ai::camp_request_subject_for_display;
    using basecamp_ai::camp_job_token_kind;
    using basecamp_ai::collect_blocked_camp_request_ids;
    using basecamp_ai::collect_ready_camp_request_ids;
    using basecamp_ai::match_camp_craft_query;
    using basecamp_ai::match_camp_request_reference;
    using basecamp_ai::matches_assigned_camp_request_worker;
    using basecamp_ai::parse_heard_camp_approval_query;
    using basecamp_ai::parse_heard_camp_cancel_query;
    using basecamp_ai::parse_heard_camp_clear_query;
    using basecamp_ai::parse_heard_camp_craft_order;
    using basecamp_ai::parse_heard_camp_status_query;
    using basecamp_ai::parse_overmap_movement_token;
    using basecamp_ai::parse_relative_omt_delta;
    using basecamp_ai::parse_structured_camp_craft_order;
    using basecamp_ai::parse_structured_camp_job_token;
    using basecamp_ai::parsed_overmap_movement_intent;
    using basecamp_ai::resolve_camp_craft_query;
    using basecamp_ai::resolve_overmap_movement_target;
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

    SECTION( "structured craft tokens parse quantity and direct object" ) {
        const std::optional<basecamp_ai::parsed_camp_craft_order> parsed =
            parse_structured_camp_craft_order( " craft=the five bandages please " );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->count == 5 );
        CHECK( parsed->item_query == "bandages" );
    }

    SECTION( "structured craft tokens reject malformed payloads and unknown prefixes" ) {
        CHECK_FALSE( parse_structured_camp_craft_order( "craft=" ).has_value() );
        CHECK_FALSE( parse_structured_camp_craft_order( "craft = bandages" ).has_value() );
        CHECK_FALSE( parse_structured_camp_craft_order( "job=12" ).has_value() );
    }

    SECTION( "structured camp job tokens parse exact request ids" ) {
        const std::optional<basecamp_ai::parsed_camp_job_token> action =
            parse_structured_camp_job_token( " job=12 " );
        REQUIRE( action.has_value() );
        CHECK( action->kind == camp_job_token_kind::act_on_job );
        CHECK( action->request_id == 12 );

        const std::optional<basecamp_ai::parsed_camp_job_token> deletion =
            parse_structured_camp_job_token( " DELETE_JOB=7 " );
        REQUIRE( deletion.has_value() );
        CHECK( deletion->kind == camp_job_token_kind::delete_job );
        CHECK( deletion->request_id == 7 );
    }

    SECTION( "structured camp job tokens reject malformed ids and unknown prefixes" ) {
        CHECK_FALSE( parse_structured_camp_job_token( "job=0" ).has_value() );
        CHECK_FALSE( parse_structured_camp_job_token( "job=-2" ).has_value() );
        CHECK_FALSE( parse_structured_camp_job_token( "job=twelve" ).has_value() );
        CHECK_FALSE( parse_structured_camp_job_token( "job=12 please" ).has_value() );
        CHECK_FALSE( parse_structured_camp_job_token( "work=12" ).has_value() );
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

    SECTION( "approval commands accept ready-request synonyms" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> requests =
            parse_heard_camp_approval_query( "approve all ready requests" );
        REQUIRE( requests.has_value() );
        CHECK( requests->all_requests );
        CHECK_FALSE( requests->all_blocked_requests );
        CHECK_FALSE( requests->has_request_id );

        const std::optional<basecamp_ai::parsed_camp_request_reference> jobs =
            parse_heard_camp_approval_query( "launch all ready jobs" );
        REQUIRE( jobs.has_value() );
        CHECK( jobs->all_requests );
        CHECK_FALSE( jobs->all_blocked_requests );
        CHECK_FALSE( jobs->has_request_id );
    }

    SECTION( "approval commands can target all blocked work orders for retry" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> requests =
            parse_heard_camp_approval_query( "retry all blocked requests" );
        REQUIRE( requests.has_value() );
        CHECK_FALSE( requests->all_requests );
        CHECK( requests->all_blocked_requests );
        CHECK_FALSE( requests->has_request_id );

        const std::optional<basecamp_ai::parsed_camp_request_reference> jobs =
            parse_heard_camp_approval_query( "resume all blocked jobs" );
        REQUIRE( jobs.has_value() );
        CHECK_FALSE( jobs->all_requests );
        CHECK( jobs->all_blocked_requests );
        CHECK_FALSE( jobs->has_request_id );
    }

    SECTION( "ready-request collector excludes blocked and archived entries" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 2, .status = "awaiting_approval" },
            camp_llm_request{ .request_id = 4, .status = "blocked" },
            camp_llm_request{ .request_id = 5, .status = "in_progress" },
            camp_llm_request{ .request_id = 7, .status = "awaiting_approval" },
            camp_llm_request{ .request_id = 9, .status = "completed" },
            camp_llm_request{ .request_id = 11, .status = "cancelled" }
        };

        CHECK( collect_ready_camp_request_ids( requests ) == std::vector<int> { 2, 7 } );
    }

    SECTION( "blocked-request collector excludes ready live work and archives" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 2, .status = "awaiting_approval" },
            camp_llm_request{ .request_id = 4, .status = "blocked" },
            camp_llm_request{ .request_id = 5, .status = "in_progress" },
            camp_llm_request{ .request_id = 7, .status = "blocked" },
            camp_llm_request{ .request_id = 9, .status = "completed" },
            camp_llm_request{ .request_id = 11, .status = "cancelled" }
        };

        CHECK( collect_blocked_camp_request_ids( requests ) == std::vector<int> { 4, 7 } );
    }

    SECTION( "status queries can ask for the whole board" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_status_query( "what's on the board" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "clear commands can target archived paperwork in bulk" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_clear_query( "clear archived requests" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->all_requests );
        CHECK_FALSE( parsed->has_request_id );
    }

    SECTION( "clear commands keep the archived work-order subject" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_clear_query( "clear the work order for long string please" );
        REQUIRE( parsed.has_value() );
        CHECK_FALSE( parsed->has_request_id );
        CHECK( parsed->query == "long string" );
    }

    SECTION( "clear commands parse explicit request numbers" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_clear_query( "clear request #12 please" );
        REQUIRE( parsed.has_value() );
        CHECK( parsed->has_request_id );
        CHECK( parsed->request_id == 12 );
        CHECK_FALSE( parsed->all_requests );
    }

    SECTION( "cancel commands keep the work-order subject" ) {
        const std::optional<basecamp_ai::parsed_camp_request_reference> parsed =
            parse_heard_camp_cancel_query( "cancel the work order for long string please" );
        REQUIRE( parsed.has_value() );
        CHECK_FALSE( parsed->has_request_id );
        CHECK( parsed->query == "long string" );
    }

    SECTION( "request matcher prefers explicit ids and respects the active predicate" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 7, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "awaiting_approval" },
            camp_llm_request{ .request_id = 12, .requested_item_query = "knife", .chosen_recipe_name = "knife", .status = "cancelled" }
        };
        basecamp_ai::parsed_camp_request_reference reference;
        reference.request_id = 12;
        reference.has_request_id = true;

        const basecamp_ai::camp_request_match_result archived = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        CHECK_FALSE( archived.found );
        CHECK( archived.request_id == 0 );

        reference.request_id = 7;
        const basecamp_ai::camp_request_match_result active = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        CHECK( active.found );
        CHECK( active.request_id == 7 );
        CHECK( active.score == 1000 );
    }

    SECTION( "request matcher reports ambiguous live board matches instead of guessing" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 3, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "awaiting_approval", .assigned_worker_name = "Alice" },
            camp_llm_request{ .request_id = 4, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "blocked", .assigned_worker_name = "Bob" },
            camp_llm_request{ .request_id = 5, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "cancelled", .assigned_worker_name = "Cara" }
        };
        basecamp_ai::parsed_camp_request_reference reference;
        reference.query = "bandages";

        const basecamp_ai::camp_request_match_result match = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        CHECK_FALSE( match.found );
        CHECK( match.score >= 650 );
        CHECK( match.ambiguous_matches.size() == 2 );
        CHECK( std::any_of( match.ambiguous_matches.begin(), match.ambiguous_matches.end(),
        []( const std::string & summary ) {
            return summary.find( "#3" ) != std::string::npos;
        } ) );
        CHECK( std::any_of( match.ambiguous_matches.begin(), match.ambiguous_matches.end(),
        []( const std::string & summary ) {
            return summary.find( "#4" ) != std::string::npos;
        } ) );
    }

    SECTION( "request matcher can target archived work orders for clear commands" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 3, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "awaiting_approval", .assigned_worker_name = "Alice" },
            camp_llm_request{ .request_id = 4, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "cancelled", .assigned_worker_name = "Bob" },
            camp_llm_request{ .request_id = 5, .requested_item_query = "knife", .chosen_recipe_name = "knife", .status = "completed", .assigned_worker_name = "Cara" }
        };
        basecamp_ai::parsed_camp_request_reference reference;
        reference.query = "bob bandages";

        const basecamp_ai::camp_request_match_result match = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & request ) {
            return request.status == "completed" || request.status == "cancelled";
        } );
        CHECK( match.found );
        CHECK( match.request_id == 4 );
        CHECK( match.score > 1000 );
        CHECK( match.ambiguous_matches.empty() );
    }

    SECTION( "request matcher can disambiguate duplicate work orders by worker name" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 3, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "awaiting_approval", .assigned_worker_name = "Alice" },
            camp_llm_request{ .request_id = 4, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "awaiting_approval", .assigned_worker_name = "Bob" }
        };
        basecamp_ai::parsed_camp_request_reference reference;
        reference.query = "bob bandages";

        const basecamp_ai::camp_request_match_result match = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        CHECK( match.found );
        CHECK( match.request_id == 4 );
        CHECK( match.score > 1000 );
        CHECK( match.ambiguous_matches.empty() );
    }

    SECTION( "request matcher can disambiguate duplicate work orders by status words" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 3, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "awaiting_approval", .approval_state = "waiting_player", .assigned_worker_name = "Alice" },
            camp_llm_request{ .request_id = 4, .requested_item_query = "bandages", .chosen_recipe_name = "bandages", .status = "blocked", .assigned_worker_name = "Bob" }
        };
        basecamp_ai::parsed_camp_request_reference reference;
        reference.query = "blocked bandages";

        const basecamp_ai::camp_request_match_result match = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & request ) {
            return request.status != "completed" && request.status != "cancelled";
        } );
        CHECK( match.found );
        CHECK( match.request_id == 4 );
        CHECK( match.score > 1000 );
        CHECK( match.ambiguous_matches.empty() );
    }

    SECTION( "request matcher can resolve spoken subject text through the source utterance" ) {
        const std::vector<camp_llm_request> requests = {
            camp_llm_request{ .request_id = 8, .requested_item_query = "heavy cable", .chosen_recipe_name = "heavy cable", .source_utterance = "please craft me a long string", .status = "awaiting_approval" }
        };
        basecamp_ai::parsed_camp_request_reference reference;
        reference.query = "long string";

        const basecamp_ai::camp_request_match_result match = match_camp_request_reference( requests,
                reference, []( const camp_llm_request & ) {
            return true;
        } );
        CHECK( match.found );
        CHECK( match.request_id == 8 );
        CHECK( match.score >= 650 );
        CHECK( match.ambiguous_matches.empty() );
    }

    SECTION( "request display subject prefers the heard query over the resolved recipe" ) {
        const camp_llm_request request{
            .requested_item_query = "bandages",
            .requested_count = 5,
            .chosen_recipe_name = "sterile bandage"
        };

        CHECK( camp_request_subject_for_display( request ) == "5 × bandages" );
    }

    SECTION( "request display subject falls back to the resolved recipe when needed" ) {
        const camp_llm_request request{
            .requested_count = 2,
            .chosen_recipe_name = "boiled makeshift bandage"
        };

        CHECK( camp_request_subject_for_display( request ) == "2 × boiled makeshift bandage" );
    }

    SECTION( "request summary subject can append the resolved recipe when it differs" ) {
        const camp_llm_request request{
            .requested_item_query = "bandages",
            .requested_count = 5,
            .chosen_recipe_name = "sterile bandage"
        };

        CHECK( camp_request_subject_for_display( request, true ) ==
               "5 × bandages (matched sterile bandage)" );
    }

    SECTION( "request summary subject does not duplicate the resolved recipe when it matches" ) {
        const camp_llm_request request{
            .requested_item_query = "bandages",
            .requested_count = 5,
            .chosen_recipe_name = "bandages"
        };

        CHECK( camp_request_subject_for_display( request, true ) == "5 × bandages" );
    }

    SECTION( "request display subject falls back to a generic label when empty" ) {
        const camp_llm_request request;

        CHECK( camp_request_subject_for_display( request ) == "crafting request" );
    }

    SECTION( "craft recall matching prefers the assigned worker id over the name" ) {
        camp_llm_request request;
        request.assigned_worker_npc_id = character_id( 7 );
        request.assigned_worker_name = "Alice";

        CHECK( matches_assigned_camp_request_worker( request, character_id( 7 ), "Not Alice" ) );
        CHECK_FALSE( matches_assigned_camp_request_worker( request, character_id( 8 ), "Alice" ) );
    }

    SECTION( "craft recall matching falls back to the worker name when legacy ids are missing" ) {
        camp_llm_request request;
        request.assigned_worker_name = "Alice";

        CHECK( matches_assigned_camp_request_worker( request, character_id(), "Alice" ) );
        CHECK_FALSE( matches_assigned_camp_request_worker( request, character_id(), "Bob" ) );
    }

    SECTION( "overmap delta parser keeps signed dx and dy values" ) {
        point_rel_omt delta = point_rel_omt::zero;
        std::string error;

        CHECK( parse_relative_omt_delta( "4", "-2", delta, error ) );
        CHECK( delta == point_rel_omt( 4, -2 ) );
        CHECK( error.empty() );

        CHECK( parse_relative_omt_delta( " -1 ", " +3 ", delta, error ) );
        CHECK( delta == point_rel_omt( -1, 3 ) );
        CHECK( error.empty() );
    }

    SECTION( "overmap delta parser rejects malformed fields" ) {
        point_rel_omt delta = point_rel_omt( 9, 9 );
        std::string error;

        CHECK_FALSE( parse_relative_omt_delta( "east", "2", delta, error ) );
        CHECK( error == "Overmap delta dx is invalid." );
        CHECK( delta == point_rel_omt::zero );

        CHECK_FALSE( parse_relative_omt_delta( "2", "north", delta, error ) );
        CHECK( error == "Overmap delta dy is invalid." );
        CHECK( delta == point_rel_omt::zero );
    }

    SECTION( "overmap movement parser accepts stay and signed delta intents" ) {
        parsed_overmap_movement_intent intent;
        std::string error;

        CHECK( parse_overmap_movement_token( " stay ", intent, error ) );
        CHECK( intent.stay );
        CHECK( intent.delta == point_rel_omt::zero );
        CHECK( error.empty() );

        CHECK( parse_overmap_movement_token( "move_omt dx=4 dy=-2", intent, error ) );
        CHECK_FALSE( intent.stay );
        CHECK( intent.delta == point_rel_omt( 4, -2 ) );
        CHECK( error.empty() );

        CHECK( parse_overmap_movement_token( "move_omt   dx=-1    dy=+3", intent, error ) );
        CHECK_FALSE( intent.stay );
        CHECK( intent.delta == point_rel_omt( -1, 3 ) );
        CHECK( error.empty() );
    }

    SECTION( "overmap movement parser rejects malformed tokens with safe fallback state" ) {
        parsed_overmap_movement_intent intent;
        std::string error;

        CHECK_FALSE( parse_overmap_movement_token( "move_omt 4 -2", intent, error ) );
        CHECK( error == "Overmap move token must use 'stay' or 'move_omt dx=<signed_int> dy=<signed_int>'." );
        CHECK( intent.stay );
        CHECK( intent.delta == point_rel_omt::zero );

        CHECK_FALSE( parse_overmap_movement_token( "move_omt dx=east dy=2", intent, error ) );
        CHECK( error == "Overmap delta dx is invalid." );
        CHECK( intent.stay );
        CHECK( intent.delta == point_rel_omt::zero );

        CHECK_FALSE( parse_overmap_movement_token( "move_omt dx=2", intent, error ) );
        CHECK( error == "Overmap move token must use 'stay' or 'move_omt dx=<signed_int> dy=<signed_int>'." );
        CHECK( intent.stay );
        CHECK( intent.delta == point_rel_omt::zero );
    }

    SECTION( "overmap movement resolver keeps stay in place and uses the shared signed axis convention" ) {
        const tripoint_abs_omt origin( 10, 20, 0 );

        CHECK( resolve_overmap_movement_target( origin,
                                                parsed_overmap_movement_intent{} ) == origin );

        parsed_overmap_movement_intent southeast;
        southeast.stay = false;
        southeast.delta = point_rel_omt( 4, -2 );
        CHECK( resolve_overmap_movement_target( origin, southeast ) == tripoint_abs_omt( 14, 22, 0 ) );

        parsed_overmap_movement_intent northwest;
        northwest.stay = false;
        northwest.delta = point_rel_omt( -1, 3 );
        CHECK( resolve_overmap_movement_target( origin, northwest ) == tripoint_abs_omt( 9, 17, 0 ) );
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

    SECTION( "craft router can singularize the final plural query word as a fallback" ) {
        const recipe &makeshift_bandage = recipe_id( "bandages_makeshift" ).obj();
        CHECK( score_camp_recipe_query( makeshift_bandage, "makeshift bandages" ) >= 650 );
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

    SECTION( "shared craft resolver keeps blocked matches and their blockers" ) {
        const std::unordered_set<recipe_id> recipes = { recipe_id( "bandages" ) };

        const basecamp_ai::camp_craft_resolution resolution = resolve_camp_craft_query( recipes, "bandages",
        []( const recipe & ) {
            basecamp_ai::camp_craft_recipe_candidate candidate;
            candidate.blockers = { "No stationed worker can take this recipe right now." };
            return candidate;
        } );

        CHECK( resolution.match.score >= 650 );
        REQUIRE( resolution.choice.has_value() );
        CHECK( resolution.choice->recipe_id == recipe_id( "bandages" ) );
        CHECK( resolution.choice->candidate.worker == nullptr );
        CHECK( resolution.choice->candidate.blockers ==
               std::vector<std::string> { "No stationed worker can take this recipe right now." } );
    }
}

// TODO: Tests for: Check calorie display at various activity levels, camp crafting works as expected (consumes inputs, returns outputs+byproducts, costs calories)

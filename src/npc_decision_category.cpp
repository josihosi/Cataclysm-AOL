#include "npc_decision_category.h"

#include <unordered_map>

const char *category_name( const decision_category cat )
{
    switch( cat ) {
        case decision_category::combat:
            return "combat";
        case decision_category::investigate:
            return "investigate";
        case decision_category::needs:
            return "needs";
        case decision_category::follow:
            return "follow";
        case decision_category::order:
            return "order";
        case decision_category::duty:
            return "duty";
        case decision_category::camp_work:
            return "camp_work";
        case decision_category::camp_travel:
            return "camp_travel";
        case decision_category::free_time:
            return "free_time";
        case decision_category::idle:
            return "idle";
        case decision_category::unmodeled:
            return "unmodeled";
    }

    return "unmodeled";
}

decision_category bt_goal_to_category( const std::string &goal )
{
    static const std::unordered_map<std::string, decision_category> categories = {
        { "fight", decision_category::combat },
        { "flee", decision_category::combat },
        { "investigate_sound", decision_category::investigate },
        { "drink_water", decision_category::needs },
        { "eat_food", decision_category::needs },
        { "go_to_sleep", decision_category::needs },
        { "start_fire", decision_category::needs },
        { "seek_warmth", decision_category::needs },
        { "follow_player", decision_category::follow },
        { "follow_embarked", decision_category::follow },
        { "goto_ordered_position", decision_category::order },
        { "return_to_guard_pos", decision_category::duty },
        { "camp_work", decision_category::camp_work },
        { "return_to_camp", decision_category::camp_travel },
        { "free_time", decision_category::free_time },
        { "idle", decision_category::idle },
    };

    const auto it = categories.find( goal );
    if( it != categories.end() ) {
        return it->second;
    }

    return decision_category::unmodeled;
}

const char *classify_comparison( const decision_category bt, const decision_category cascade )
{
    if( bt == decision_category::unmodeled || cascade == decision_category::unmodeled ) {
        return "unmodeled";
    }

    return bt == cascade ? "converged" : "DIVERGED";
}

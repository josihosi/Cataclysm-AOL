#include "ecology_debug_view.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <tuple>
#include <utility>

#include "bandit_live_world.h"

namespace ecology_debug
{
namespace
{

struct candidate_read {
    const bandit_live_world::site_record *site = nullptr;
    const bandit_live_world::active_outing_state *outing = nullptr;
    entity_kind kind = entity_kind::bandit_camp;
    entity_faction faction = entity_faction::bandit;
    tripoint_abs_omt omt;
    std::string id;
    bool has_loaded_read = false;
    bool loaded = false;
};

entity_faction faction_for( const bandit_live_world::site_record &site )
{
    return site.profile == bandit_live_world::hostile_site_profile::cannibal_camp ||
           site.site_kind == bandit_live_world::owned_site_kind::cannibal_camp ?
           entity_faction::cannibal : entity_faction::bandit;
}

bool is_live_member( const bandit_live_world::member_record &member )
{
    return member.state != bandit_live_world::member_state::dead &&
           member.state != bandit_live_world::member_state::missing;
}

bool is_supported_camp( const bandit_live_world::site_record &site )
{
    return site.profile == bandit_live_world::hostile_site_profile::camp_style ||
           site.profile == bandit_live_world::hostile_site_profile::cannibal_camp;
}

bool is_unresolved_outing_survivor( const bandit_live_world::site_record &site,
                                    const bandit_live_world::active_outing_state &outing,
                                    character_id member_id )
{
    if( outing.member_is_resolved( member_id ) ||
        std::find( outing.casualty_ids.begin(), outing.casualty_ids.end(), member_id ) !=
        outing.casualty_ids.end() ) {
        return false;
    }
    const bandit_live_world::member_record *member = site.find_member( member_id );
    return member != nullptr && is_live_member( *member );
}

bool outing_has_unresolved_survivor( const bandit_live_world::site_record &site,
                                    const bandit_live_world::active_outing_state &outing )
{
    return std::any_of( outing.member_ids.begin(), outing.member_ids.end(),
    [&site, &outing]( const character_id & member_id ) {
        return is_unresolved_outing_survivor( site, outing, member_id );
    } );
}

bool outing_is_terminal( const bandit_live_world::site_record &site,
                         const bandit_live_world::active_outing_state &outing )
{
    return outing.phase == bandit_live_world::scout_phase::lost ||
           ( &outing == &site.active_hostile_operation.reservation &&
             site.active_hostile_operation.phase ==
             bandit_live_world::hostile_operation_phase::lost );
}

tripoint_abs_omt outing_position( const bandit_live_world::active_outing_state &outing )
{
    if( outing.owner == bandit_live_world::simulation_owner::local &&
        outing.local_handoff.is_active() ) {
        return outing.local_handoff.route_position;
    }
    if( !outing.shared_route.empty() ) {
        const int index = std::clamp( outing.waypoint_index, 0,
                                     static_cast<int>( outing.shared_route.size() ) - 1 );
        return outing.shared_route[index];
    }
    return outing.target_omt;
}

bool point_in_region( const tripoint_abs_omt &point, const query_region &region )
{
    if( !region.enabled ) {
        return true;
    }
    return point.x() >= region.minimum.x() && point.x() <= region.maximum.x() &&
           point.y() >= region.minimum.y() && point.y() <= region.maximum.y() &&
           point.z() >= region.minimum.z() && point.z() <= region.maximum.z();
}

int kind_rank( entity_kind kind )
{
    return static_cast<int>( kind );
}

bool candidate_less( const candidate_read &lhs, const candidate_read &rhs )
{
    return std::make_tuple( lhs.omt.z(), lhs.omt.y(), lhs.omt.x(), kind_rank( lhs.kind ), lhs.id ) <
           std::make_tuple( rhs.omt.z(), rhs.omt.y(), rhs.omt.x(), kind_rank( rhs.kind ), rhs.id );
}

bool marker_less( const entity_marker &lhs, const entity_marker &rhs )
{
    return std::make_tuple( lhs.omt.z(), lhs.omt.y(), lhs.omt.x(), kind_rank( lhs.kind ), lhs.id ) <
           std::make_tuple( rhs.omt.z(), rhs.omt.y(), rhs.omt.x(), kind_rank( rhs.kind ), rhs.id );
}

std::string camp_id( const bandit_live_world::site_record &site )
{
    return "camp/" + site.site_id;
}

std::string dispatch_id( const bandit_live_world::site_record &site,
                         const bandit_live_world::active_outing_state &outing )
{
    return "dispatch/" + site.site_id + "/" + outing.activity_id + "/" +
           std::to_string( outing.generation );
}

std::string short_alias( entity_kind kind, const std::string &id )
{
    uint32_t hash = 2166136261u;
    for( const unsigned char byte : id ) {
        hash ^= byte;
        hash *= 16777619u;
    }
    const char *prefix = "BC";
    switch( kind ) {
        case entity_kind::bandit_camp:
            prefix = "BC";
            break;
        case entity_kind::cannibal_camp:
            prefix = "CC";
            break;
        case entity_kind::bandit_dispatch:
            prefix = "BD";
            break;
        case entity_kind::cannibal_dispatch:
            prefix = "CD";
            break;
    }
    std::ostringstream out;
    out << prefix << '-' << std::uppercase << std::hex << std::setw( 6 ) << std::setfill( '0' )
        << ( hash & 0x00ffffffu );
    return out.str();
}

bool callback_loaded( const query_request &request, character_id member_id )
{
    return request.member_is_loaded && request.member_is_loaded( member_id );
}

bool candidate_is_loaded( const candidate_read &candidate, const query_request &request )
{
    if( candidate.outing != nullptr ) {
        return std::any_of( candidate.outing->member_ids.begin(), candidate.outing->member_ids.end(),
        [&candidate, &request]( const character_id & member_id ) {
            return is_unresolved_outing_survivor( *candidate.site, *candidate.outing, member_id ) &&
                   callback_loaded( request, member_id );
        } );
    }
    return std::any_of( candidate.site->members.begin(), candidate.site->members.end(),
    [&request]( const bandit_live_world::member_record & member ) {
        return member.state == bandit_live_world::member_state::at_home &&
               callback_loaded( request, member.npc_id );
    } );
}

entity_marker make_marker( const candidate_read &candidate, const query_request &request )
{
    entity_marker marker;
    marker.id = candidate.id;
    marker.alias = short_alias( candidate.kind, candidate.id );
    marker.kind = candidate.kind;
    marker.faction = candidate.faction;
    marker.omt = candidate.omt;
    marker.loaded = candidate.has_loaded_read ? candidate.loaded :
                    candidate_is_loaded( candidate, request );
    marker.provenance = request.provenance_for_entity ?
                        request.provenance_for_entity( marker.id ) : entity_provenance::natural;
    if( candidate.outing == nullptr ) {
        marker.owner = "abstract";
        marker.state = bandit_live_world::to_string( candidate.site->origin );
    } else {
        marker.owner = bandit_live_world::to_string( candidate.outing->owner );
        marker.state = bandit_live_world::to_string( candidate.outing->phase );
        marker.generation = candidate.outing->generation;
        if( candidate.outing == &candidate.site->active_hostile_operation.reservation ) {
            marker.state = bandit_live_world::to_string(
                               candidate.site->active_hostile_operation.phase );
        }
    }
    return marker;
}

int minimum_nonnegative( std::initializer_list<int> values )
{
    int result = -1;
    for( const int value : values ) {
        if( value >= 0 && ( result < 0 || value < result ) ) {
            result = value;
        }
    }
    return result;
}

std::string blocked_reason_for( const bandit_live_world::site_record &site,
                                const bandit_live_world::active_outing_state &outing,
                                int now_minutes )
{
    if( outing.shared_route.empty() ) {
        return "missing_route";
    }
    if( outing.owner == bandit_live_world::simulation_owner::local &&
        outing.local_handoff.is_active() && !outing.local_handoff.cohesion_assembled ) {
        return "awaiting_local_cohesion";
    }
    if( &outing == &site.active_hostile_operation.reservation &&
        site.active_hostile_operation.phase ==
        bandit_live_world::hostile_operation_phase::waiting_night ) {
        return "waiting_for_night";
    }
    if( outing.phase == bandit_live_world::scout_phase::assembling ) {
        return "assembling";
    }
    if( now_minutes >= 0 && outing.missing_deadline_minutes >= 0 &&
        now_minutes >= outing.missing_deadline_minutes ) {
        return "missing_deadline_elapsed";
    }
    if( now_minutes >= 0 && outing.expected_return_minutes >= 0 &&
        now_minutes >= outing.expected_return_minutes ) {
        return "return_overdue";
    }
    return {};
}

tripoint_abs_omt destination_for( const bandit_live_world::site_record &site,
                                  const bandit_live_world::active_outing_state &outing )
{
    switch( outing.phase ) {
        case bandit_live_world::scout_phase::burned_withdrawal:
        case bandit_live_world::scout_phase::returning_exposed:
        case bandit_live_world::scout_phase::returning_report:
        case bandit_live_world::scout_phase::returning_home:
            return site.anchor;
        default:
            return outing.target_omt;
    }
}

selected_detail make_selected_detail( const candidate_read &candidate,
                                      const query_request &request )
{
    selected_detail detail;
    detail.entity_id = candidate.id;
    detail.source_camp_id = camp_id( *candidate.site );
    if( candidate.outing == nullptr ) {
        detail.phase = bandit_live_world::to_string( candidate.site->origin );
        detail.last_transition_minutes = candidate.site->origin_changed_minutes;
        detail.last_transition_reason = candidate.site->origin_summary;
        detail.next_deadline_minutes = candidate.site->next_routine_dispatch_eligible_minutes;
        detail.destination = candidate.site->anchor;
    } else {
        const bandit_live_world::active_outing_state &outing = *candidate.outing;
        detail.phase = bandit_live_world::to_string( outing.phase );
        detail.last_transition_minutes = outing.last_progress_minutes;
        if( detail.last_transition_minutes >= 0 ) {
            detail.last_transition_reason = "outing_progress";
        }
        detail.blocked_reason = blocked_reason_for( *candidate.site, outing,
                                request.now_minutes );
        detail.next_deadline_minutes = minimum_nonnegative( {
            outing.expected_return_minutes,
            outing.missing_deadline_minutes,
            outing.local_handoff.cohesion_deadline_minutes
        } );
        detail.destination = destination_for( *candidate.site, outing );
        detail.route = outing.shared_route;
        if( &outing == &candidate.site->active_hostile_operation.reservation ) {
            detail.phase = bandit_live_world::to_string(
                               candidate.site->active_hostile_operation.phase );
            detail.last_transition_reason =
                candidate.site->active_hostile_operation.last_transition_reason;
        }
        if( !outing.observations.empty() ) {
            const bandit_live_world::sortie_observation *latest = &outing.observations.front();
            for( const bandit_live_world::sortie_observation &observation : outing.observations ) {
                if( observation.observed_minutes > latest->observed_minutes ) {
                    latest = &observation;
                }
            }
            detail.evidence_reason = latest->summary;
            if( request.now_minutes >= 0 && latest->observed_minutes >= 0 ) {
                detail.evidence_age_minutes = std::max( 0,
                                              request.now_minutes - latest->observed_minutes );
            }
        }
    }

    const std::vector<character_id> member_ids = candidate.outing == nullptr ?
            std::vector<character_id>() : candidate.outing->member_ids;
    if( candidate.outing == nullptr ) {
        detail.members.reserve( candidate.site->members.size() );
        for( const bandit_live_world::member_record &member : candidate.site->members ) {
            if( is_live_member( member ) ) {
                member_detail row;
                row.npc_id = member.npc_id;
                row.status = bandit_live_world::to_string( member.state );
                if( request.read_selected_member ) {
                    const std::optional<runtime_member_read> runtime =
                        request.read_selected_member( member.npc_id );
                    if( runtime ) {
                        row.name = runtime->name;
                        row.hp_percent = runtime->hp_percent;
                        row.loaded = runtime->loaded;
                    }
                }
                detail.members.push_back( std::move( row ) );
            }
        }
    } else {
        detail.members.reserve( member_ids.size() );
        for( const character_id &member_id : member_ids ) {
            member_detail row;
            row.npc_id = member_id;
            const bandit_live_world::member_record *member = candidate.site->find_member( member_id );
            row.status = member == nullptr ? "unknown" : bandit_live_world::to_string( member->state );
            if( request.read_selected_member ) {
                const std::optional<runtime_member_read> runtime =
                    request.read_selected_member( member_id );
                if( runtime ) {
                    row.name = runtime->name;
                    row.hp_percent = runtime->hp_percent;
                    row.loaded = runtime->loaded;
                }
            }
            if( !row.hp_percent && candidate.outing->local_handoff.is_active() ) {
                const auto found = std::find_if( candidate.outing->local_handoff.members.begin(),
                candidate.outing->local_handoff.members.end(), [&member_id](
                const bandit_live_world::local_handoff_member_snapshot & snapshot ) {
                    return snapshot.npc_id == member_id;
                } );
                if( found != candidate.outing->local_handoff.members.end() ) {
                    row.hp_percent = found->hp_percent;
                }
            }
            detail.members.push_back( std::move( row ) );
        }
    }
    std::sort( detail.members.begin(), detail.members.end(), []( const member_detail &lhs,
    const member_detail &rhs ) {
        return lhs.npc_id.get_value() < rhs.npc_id.get_value();
    } );
    return detail;
}

} // namespace

view_snapshot query_bandit_ecology( const bandit_live_world::world_state &world,
                                    const query_request &request )
{
    view_snapshot result;
    if( !request.enabled ) {
        return result;
    }
    const auto started = std::chrono::steady_clock::now();
    const auto heap_compare = []( const candidate_read &lhs, const candidate_read &rhs ) {
        return candidate_less( lhs, rhs );
    };
    std::priority_queue<candidate_read, std::vector<candidate_read>, decltype( heap_compare )>
    retained_candidates( heap_compare );
    std::optional<candidate_read> selected_candidate;
    size_t candidate_count = 0;
    auto retain_candidate = [&]( candidate_read candidate ) {
        if( request.filters.loaded_only ) {
            candidate.loaded = candidate_is_loaded( candidate, request );
            candidate.has_loaded_read = true;
            if( !candidate.loaded ) {
                return;
            }
        }
        candidate_count++;
        if( candidate.id == request.selected_id ) {
            selected_candidate = candidate;
        }
        if( retained_candidates.size() < candidate_cap ) {
            retained_candidates.push( std::move( candidate ) );
        } else if( candidate_less( candidate, retained_candidates.top() ) ) {
            retained_candidates.pop();
            retained_candidates.push( std::move( candidate ) );
        }
    };
    for( const bandit_live_world::site_record &site : world.sites ) {
        if( !is_supported_camp( site ) ) {
            continue;
        }
        const entity_faction faction = faction_for( site );
        const bool faction_enabled = faction == entity_faction::bandit ?
                                     request.filters.bandits : request.filters.cannibals;
        if( !faction_enabled ) {
            continue;
        }
        const bandit_live_world::roster_view roster = site.roster();
        if( request.filters.camps && !site.retired_empty_site && roster.valid &&
            site.origin == bandit_live_world::origin_disposition::active_hostile &&
            roster.living_total > 0 && point_in_region( site.anchor, request.region ) ) {
            candidate_read camp;
            camp.site = &site;
            camp.kind = faction == entity_faction::bandit ? entity_kind::bandit_camp :
                        entity_kind::cannibal_camp;
            camp.faction = faction;
            camp.omt = site.anchor;
            camp.id = camp_id( site );
            retain_candidate( std::move( camp ) );
        }
        const bandit_live_world::active_outing_state *outing = site.active_external_outing();
        if( request.filters.dispatches && outing != nullptr && outing->is_active() &&
            !outing_is_terminal( site, *outing ) &&
            outing_has_unresolved_survivor( site, *outing ) ) {
            const tripoint_abs_omt omt = outing_position( *outing );
            if( point_in_region( omt, request.region ) ) {
                candidate_read dispatch;
                dispatch.site = &site;
                dispatch.outing = outing;
                dispatch.kind = faction == entity_faction::bandit ?
                                entity_kind::bandit_dispatch : entity_kind::cannibal_dispatch;
                dispatch.faction = faction;
                dispatch.omt = omt;
                dispatch.id = dispatch_id( site, *outing );
                retain_candidate( std::move( dispatch ) );
            }
        }
    }
    std::vector<candidate_read> candidates;
    candidates.reserve( retained_candidates.size() );
    while( !retained_candidates.empty() ) {
        candidates.push_back( std::move( retained_candidates.top() ) );
        retained_candidates.pop();
    }
    std::sort( candidates.begin(), candidates.end(), candidate_less );
    if( selected_candidate &&
        std::none_of( candidates.begin(), candidates.end(), [&selected_candidate](
    const candidate_read & candidate ) {
        return candidate.id == selected_candidate->id;
    } ) ) {
        if( candidates.size() == candidate_cap ) {
            candidates.back() = std::move( *selected_candidate );
        } else {
            candidates.push_back( std::move( *selected_candidate ) );
        }
        std::sort( candidates.begin(), candidates.end(), candidate_less );
    }
    result.metadata.candidate_count = candidate_count;

    size_t selected_index = candidates.size();
    if( !request.selected_id.empty() ) {
        const auto selected = std::find_if( candidates.begin(), candidates.end(),
        [&request]( const candidate_read & candidate ) {
            return candidate.id == request.selected_id;
        } );
        selected_index = static_cast<size_t>( std::distance( candidates.begin(), selected ) );
    }
    const size_t considered = std::min( candidate_cap, candidates.size() );
    std::vector<const candidate_read *> bounded;
    bounded.reserve( considered );
    for( size_t index = 0; index < considered; ++index ) {
        bounded.push_back( &candidates[index] );
    }
    if( selected_index >= considered && selected_index < candidates.size() && !bounded.empty() ) {
        bounded.back() = &candidates[selected_index];
        std::sort( bounded.begin(), bounded.end(), []( const candidate_read * lhs,
        const candidate_read * rhs ) {
            return candidate_less( *lhs, *rhs );
        } );
    }
    result.metadata.considered_count = bounded.size();

    std::vector<std::pair<entity_marker, const candidate_read *>> markers;
    markers.reserve( bounded.size() );
    for( const candidate_read *candidate : bounded ) {
        entity_marker marker = make_marker( *candidate, request );
        markers.emplace_back( std::move( marker ), candidate );
    }
    std::sort( markers.begin(), markers.end(), []( const auto & lhs, const auto & rhs ) {
        return marker_less( lhs.first, rhs.first );
    } );

    size_t selected_marker_index = markers.size();
    if( !request.selected_id.empty() ) {
        const auto selected = std::find_if( markers.begin(), markers.end(),
        [&request]( const auto & row ) {
            return row.first.id == request.selected_id;
        } );
        selected_marker_index = static_cast<size_t>( std::distance( markers.begin(), selected ) );
    }
    const size_t emitted = std::min( marker_cap, markers.size() );
    std::vector<size_t> emitted_indices;
    emitted_indices.reserve( emitted );
    for( size_t index = 0; index < emitted; ++index ) {
        emitted_indices.push_back( index );
    }
    if( selected_marker_index >= emitted && selected_marker_index < markers.size() &&
        !emitted_indices.empty() ) {
        emitted_indices.back() = selected_marker_index;
        std::sort( emitted_indices.begin(), emitted_indices.end(), [&markers]( size_t lhs,
        size_t rhs ) {
            return marker_less( markers[lhs].first, markers[rhs].first );
        } );
    }
    result.entities.reserve( emitted_indices.size() );
    for( const size_t index : emitted_indices ) {
        result.entities.push_back( markers[index].first );
        if( markers[index].first.id == request.selected_id ) {
            result.selected = make_selected_detail( *markers[index].second, request );
        }
    }
    result.metadata.emitted_count = result.entities.size();
    result.metadata.dropped_count = result.metadata.candidate_count - result.metadata.emitted_count;
    result.metadata.truncated = result.metadata.dropped_count > 0;
    result.metadata.query_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started ).count();
    return result;
}

std::string to_string( entity_kind kind )
{
    switch( kind ) {
        case entity_kind::bandit_camp:
            return "bandit_camp";
        case entity_kind::cannibal_camp:
            return "cannibal_camp";
        case entity_kind::bandit_dispatch:
            return "bandit_dispatch";
        case entity_kind::cannibal_dispatch:
            return "cannibal_dispatch";
    }
    return "bandit_camp";
}

std::string to_string( entity_faction faction )
{
    return faction == entity_faction::cannibal ? "cannibal" : "bandit";
}

std::string to_string( entity_provenance provenance )
{
    return provenance == entity_provenance::debug_intervention ?
           "debug_intervention" : "natural";
}

} // namespace ecology_debug

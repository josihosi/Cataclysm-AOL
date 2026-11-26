#include "migo_population.h"

#include <algorithm>
#include <limits>
#include <set>

#include "calendar.h"
#include "json.h"
#include "mtype.h"
#include "rng.h"

namespace migo_population
{
void activity_clock::evaluate( int now, bool eligible )
{
    if( last_world_turn < 0 ) {
        adopted_world_turn = last_world_turn = now;
        active = eligible;
        active_delta_turns = 0;
        return;
    }
    if( now < last_world_turn ) {
        return;
    }
    if( now == last_world_turn ) {
        // A same-turn boundary change cannot mint a second interval.
        active = eligible;
        if( !active ) {
            active_delta_turns = 0;
        }
        return;
    }
    const int elapsed = now - last_world_turn;
    active_delta_turns = active && eligible ? elapsed : 0;
    suspended_turns += elapsed - active_delta_turns;
    last_world_turn = now;
    active = eligible;
}

std::int64_t activity_clock::suspension_at( int now ) const
{
    return suspended_turns + ( !active && last_world_turn >= 0 ?
                               std::max( 0, now - last_world_turn ) : 0 );
}

void activity_clock::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "adopted_world_turn", adopted_world_turn );
    json.member( "last_world_turn", last_world_turn );
    json.member( "active", active );
    json.member( "suspended_turns", suspended_turns );
    json.end_object();
}

void activity_clock::deserialize( const JsonObject &json )
{
    *this = activity_clock();
    json.read( "adopted_world_turn", adopted_world_turn );
    json.read( "last_world_turn", last_world_turn );
    json.read( "active", active );
    json.read( "suspended_turns", suspended_turns );
    suspended_turns = std::max<std::int64_t>( 0, suspended_turns );
}

void identity::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "site", site_id );
    json.member( "member", member_id );
    json.member( "epoch", epoch );
    json.member( "anchor", site_anchor );
    json.end_object();
}

void identity::deserialize( const JsonObject &json )
{
    json.read( "site", site_id );
    json.read( "member", member_id );
    json.read( "epoch", epoch );
    json.read( "anchor", site_anchor );
}

void member_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "identity", token );
    json.member( "type", type );
    json.member( "home", home );
    json.member( "owner", static_cast<int>( owner ) );
    json.member( "duty", static_cast<int>( assignment ) );
    json.member( "hp", hp );
    json.member( "birth_sequence", birth_sequence );
    json.member( "current_position", current_position );
    json.end_object();
}

void member_record::deserialize( const JsonObject &json )
{
    json.read( "identity", token );
    json.read( "type", type );
    json.read( "home", home );
    owner = static_cast<representation>( json.get_int( "owner", 0 ) );
    assignment = static_cast<duty>( json.get_int( "duty", 0 ) );
    json.read( "hp", hp );
    json.read( "birth_sequence", birth_sequence );
    current_position.reset();
    json.read( "current_position", current_position );
}

bool site_record::commit_seed( const std::vector<member_record> &slots,
                              std::optional<tripoint_abs_ms> source_position,
                              const std::string &furniture, int unproven_occupancy,
                              bool occupancy_known, bool ordinary_new_tower )
{
    if( seed_committed || id.empty() || unproven_occupancy < 0 ||
        ( source_position && furniture.empty() ) ) {
        return false;
    }
    std::set<std::string> ids;
    int ordinary = 0;
    int scouts = 0;
    for( const member_record &m : slots ) {
        if( m.token.empty() || m.token.site_id != id ||
            !ids.insert( m.token.member_id ).second ) {
            return false;
        }
        ordinary += m.type == mtype_id( "mon_mi_go" );
        scouts += m.type == mtype_id( "mon_mi_go_scout" );
    }
    if( ordinary_new_tower && ( slots.size() != 8 || ordinary != 6 || scouts != 2 ) ) {
        return false;
    }
    members = slots;
    source = source_position;
    source_furniture = furniture;
    occupancy_verified = occupancy_known;
    ambient_occupancy = unproven_occupancy;
    ceiling = tower ? 8 : std::max<int>( 8, members.size() );
    seed_committed = true;
    return true;
}

member_record *site_record::find( const std::string &member_id )
{
    const auto it = std::find_if( members.begin(), members.end(), [&]( const member_record & m ) {
        return m.token.member_id == member_id;
    } );
    return it == members.end() ? nullptr : &*it;
}

const member_record *site_record::find( const std::string &member_id ) const
{
    return const_cast<site_record *>( this )->find( member_id );
}

int site_record::living_count() const
{
    return std::count_if( members.begin(), members.end(), []( const member_record & m ) {
        return m.owner != representation::dead;
    } );
}

bool site_record::owns( const identity &token, representation owner ) const
{
    const member_record *m = find( token.member_id );
    return seed_committed && token.site_id == id && m && m->token.epoch == token.epoch &&
           m->owner == owner;
}

bool site_record::transfer( const identity &token, representation from, representation to )
{
    if( from == representation::dead || to == representation::dead || from == to ||
        !owns( token, from ) || token.epoch == std::numeric_limits<std::uint64_t>::max() ) {
        return false;
    }
    member_record &m = *find( token.member_id );
    ++m.token.epoch;
    m.owner = to;
    return true;
}

bool site_record::record_death( const identity &token )
{
    member_record *m = find( token.member_id );
    if( token.site_id != id || !m || m->owner == representation::dead ||
        token.epoch != m->token.epoch ) {
        return false;
    }
    m->owner = representation::dead;
    m->hp = 0;
    return true;
}

bool site_record::observe_health( const identity &token, int hp )
{
    member_record *m = find( token.member_id );
    if( token.site_id != id || !m || m->owner == representation::dead ||
        token.epoch != m->token.epoch ) {
        return false;
    }
    m->hp = hp;
    return true;
}

std::vector<identity> site_record::reserve( int count,
        const std::function<bool( const member_record & )> &healthy )
{
    std::vector<identity> selected;
    if( !seed_committed || count <= 0 ) {
        return selected;
    }
    for( const member_record &m : members ) {
        if( m.owner != representation::dead && m.owner != representation::slot &&
            m.assignment == duty::available && m.hp > 0 &&
            healthy( m ) ) {
            selected.push_back( m.token );
            if( static_cast<int>( selected.size() ) == count ) {
                break;
            }
        }
    }
    return static_cast<int>( selected.size() ) == count ? reserve_exact( selected, healthy ) :
           std::vector<identity>();
}

std::vector<identity> site_record::reserve_exact( const std::vector<identity> &selected,
        const std::function<bool( const member_record & )> &healthy,
        int response_home_reserve )
{
    if( !seed_committed || selected.empty() || response_home_reserve < -1 ||
        ( response_home_reserve >= 0 && ( selected.size() < 3 ||
          ( response_home_reserve != 0 && response_home_reserve != defensive_reserve ) ) ) ) {
        return {};
    }
    const int reserve = response_home_reserve < 0 ? defensive_reserve : response_home_reserve;
    std::set<std::string> available;
    for( const member_record &m : members ) {
        if( m.owner != representation::dead && m.owner != representation::slot &&
            m.assignment == duty::available && m.hp > 0 && healthy( m ) ) {
            available.insert( m.token.member_id );
        }
    }
    if( available.size() < selected.size() + static_cast<size_t>( std::max( 0,
            reserve ) ) ) {
        return {};
    }
    std::set<std::string> unique;
    for( const identity &token : selected ) {
        const member_record *m = find( token.member_id );
        if( !m || token.site_id != id || token.epoch != m->token.epoch ||
            available.count( token.member_id ) == 0 || !unique.insert( token.member_id ).second ) {
            return {};
        }
    }
    for( const identity &token : selected ) {
        find( token.member_id )->assignment = duty::dispatched;
    }
    return selected;
}

bool site_record::returning( const identity &token )
{
    member_record *m = find( token.member_id );
    if( token.site_id != id || !m || m->owner == representation::dead ||
        m->token.epoch != token.epoch || m->assignment != duty::dispatched ) {
        return false;
    }
    m->assignment = duty::returning;
    return true;
}

bool site_record::returned( const identity &token, const tripoint_abs_ms &actual_position )
{
    member_record *m = find( token.member_id );
    if( token.site_id != id || !m || m->owner == representation::dead ||
        m->token.epoch != token.epoch || m->assignment != duty::returning ||
        project_to<coords::omt>( actual_position ) != project_to<coords::omt>( m->home ) ) {
        return false;
    }
    m->assignment = duty::available;
    return true;
}

bool site_record::advance( int world_turn, int active_elapsed, std::optional<bool> source_exists,
                           const std::function<bool( const member_record & )> &place )
{
    // Repeated/reloaded service cannot spend the same active interval twice.
    // World time identifies the observation; the caller separately supplies
    // admitted active time. This does not implement the activation system.
    if( world_turn <= last_service_world ) {
        return false;
    }
    const int admitted = last_service_world < 0 ? 0 :
                         std::min( std::max( 0, active_elapsed ), world_turn - last_service_world );
    last_service_world = world_turn;
    if( source_exists && !*source_exists ) {
        source_destroyed = true;
        production_elapsed = 0;
    }
    if( !seed_committed || !source || source_destroyed || !source_exists.value_or( false ) ||
        !occupancy_verified ) {
        return false;
    }
    if( living_count() + ambient_occupancy >= ceiling ) {
        production_elapsed = 0;
        return false;
    }
    // A retained due event is earned progress, not permission to create
    // while frozen or on the first zero-elapsed service after a load gap.
    if( admitted == 0 ) {
        return false;
    }
    constexpr std::int64_t interval = to_turns<std::int64_t>( 7_days );
    production_elapsed += std::min<std::int64_t>( admitted,
                          interval - production_elapsed );
    if( production_elapsed < interval || birth_sequence == std::numeric_limits<std::uint64_t>::max() ) {
        return false;
    }
    // An empty placement callback accrues a single due event without
    // allocating an identity or consuming RNG before physical placement is possible.
    if( !place ) {
        return false;
    }
    member_record birth;
    birth.token = { id, random_string( 32 ), 0, anchor };
    birth.birth_sequence = birth_sequence + 1;
    const int scouts = std::count_if( members.begin(), members.end(), []( const member_record & m ) {
        return m.owner != representation::dead && m.type == mtype_id( "mon_mi_go_scout" );
    } );
    const int mix = std::count_if( members.begin(), members.end(), []( const member_record & m ) {
        return m.owner != representation::dead &&
               ( m.type == mtype_id( "mon_mi_go" ) || m.type == mtype_id( "mon_mi_go_scout" ) );
    } );
    birth.type = mtype_id( scouts * 4 < mix ? "mon_mi_go_scout" : "mon_mi_go" );
    birth.hp = birth.type->hp;
    birth.home = *source;
    birth.owner = representation::abstract;
    if( !place( birth ) ) {
        return false;
    }
    members.push_back( birth );
    birth_sequence = birth.birth_sequence;
    production_elapsed = 0;
    return true;
}

void site_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "id", id );
    json.member( "anchor", anchor );
    json.member( "tower", tower );
    json.member( "new_content", new_content );
    json.member( "seed_committed", seed_committed );
    json.member( "ceiling", ceiling );
    json.member( "defensive_reserve", defensive_reserve );
    json.member( "source", source );
    json.member( "source_furniture", source_furniture );
    json.member( "source_destroyed", source_destroyed );
    json.member( "occupancy_verified", occupancy_verified );
    json.member( "ambient_occupancy", ambient_occupancy );
    json.member( "production_elapsed", production_elapsed );
    json.member( "birth_sequence", birth_sequence );
    json.member( "last_service_world", last_service_world );
    json.member( "activity", activity );
    json.member( "members", members );
    json.end_object();
}

void site_record::deserialize( const JsonObject &json )
{
    json.read( "id", id );
    json.read( "anchor", anchor );
    json.read( "tower", tower );
    json.read( "new_content", new_content );
    json.read( "seed_committed", seed_committed );
    json.read( "ceiling", ceiling );
    json.read( "defensive_reserve", defensive_reserve );
    json.read( "source", source );
    json.read( "source_furniture", source_furniture );
    json.read( "source_destroyed", source_destroyed );
    json.read( "occupancy_verified", occupancy_verified );
    json.read( "ambient_occupancy", ambient_occupancy );
    json.read( "production_elapsed", production_elapsed );
    json.read( "birth_sequence", birth_sequence );
    json.read( "last_service_world", last_service_world );
    activity = activity_clock();
    json.read( "activity", activity );
    json.read( "members", members );
    production_elapsed = std::clamp<std::int64_t>( production_elapsed, 0,
                         to_turns<std::int64_t>( 7_days ) );
}

site_record *world_state::find( const std::string &site_id )
{
    if( indexed_site_count > sites.size() ) {
        site_index.clear();
        indexed_site_count = 0;
    }
    // Native site IDs are stable and the ledger is append-only until clear.
    // New discovery updates this ID lookup without walking distant history.
    while( indexed_site_count < sites.size() ) {
        site_index.emplace( sites[indexed_site_count].id, indexed_site_count );
        ++indexed_site_count;
    }
    const auto found = site_index.find( site_id );
    return found == site_index.end() ? nullptr : &sites[found->second];
}

const site_record *world_state::find( const std::string &site_id ) const
{
    // A genuinely const snapshot may read an existing index, but must not
    // mutate it through the live owner's lazy indexing path.
    const auto cached = site_index.find( site_id );
    if( cached != site_index.end() && cached->second < sites.size() &&
        sites[cached->second].id == site_id ) {
        return &sites[cached->second];
    }
    const auto found = std::find_if( sites.begin(), sites.end(), [&]( const auto &site ) {
        return site.id == site_id;
    } );
    return found == sites.end() ? nullptr : &*found;
}

void world_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "sites", sites );
    json.end_object();
}

void world_state::deserialize( const JsonObject &json )
{
    clear();
    json.read( "sites", sites );
}

void world_state::clear()
{
    sites.clear();
    position_index.clear();
    indexed_positions.clear();
    positions_dirty = true;
    positions_bootstrapped = false;
    activation_initialized = false;
    active_sites.clear();
    settlement_sites.clear();
    indexed_site_count = 0;
    site_index.clear();
}

void world_state::invalidate_positions()
{
    positions_dirty = true;
    indexed_site_count = 0;
    site_index.clear();
}

void world_state::refresh_site( const std::string &id )
{
    if( positions_dirty ) {
        return; // the single cold rebuild will include this committed state
    }
    const auto old = indexed_positions.find( id );
    if( old != indexed_positions.end() ) {
        for( const auto &p : old->second ) {
            index_position( p, id, -1 );
        }
        indexed_positions.erase( old );
    }
    const auto *site = find( id );
    if( !site ) {
        return;
    }
    auto &positions = indexed_positions[id];
    const auto add = [&]( const point_abs_omt & p ) {
        positions.push_back( p );
        index_position( p, id, 1 );
    };
    add( site->anchor.xy() );
    for( const member_record &member : site->members ) {
        if( member.owner != representation::dead && member.hp > 0 &&
            member.assignment != duty::available && member.current_position ) {
            add( project_to<coords::omt>( *member.current_position ).xy() );
        }
    }
}

bool world_state::confirm_position( const identity &token, representation owner,
                                    const tripoint_abs_ms &position )
{
    site_record *site = find( token.site_id );
    member_record *member = site ? site->find( token.member_id ) : nullptr;
    if( !member || !site->owns( token, owner ) || member->hp <= 0 ) {
        return false;
    }
    if( member->current_position != position ) {
        member->current_position = position;
        refresh_site( site->id );
    }
    return true;
}

std::set<std::string> world_state::nearby_sites( const point_abs_omt &center, int radius )
{
    if( positions_dirty ) {
        position_index.clear();
        indexed_positions.clear();
        positions_dirty = false;
        for( const site_record &site : sites ) {
            refresh_site( site.id );
        }
    }
    std::set<std::string> result;
    if( radius < 0 ) {
        return result;
    }
    for( auto x = position_index.lower_bound( center.x() - radius );
         x != position_index.end() && x->first <= center.x() + radius; ++x ) {
        for( auto y = x->second.lower_bound( center.y() - radius );
             y != x->second.end() && y->first <= center.y() + radius; ++y ) {
            for( const auto &site : y->second ) {
                result.insert( site.first );
            }
        }
    }
    return result;
}

void world_state::index_position( const point_abs_omt &p, const std::string &site, int delta )
{
    auto &column = position_index[p.x()];
    auto &cell = column[p.y()];
    int &count = cell[site];
    count += delta;
    if( count <= 0 ) {
        cell.erase( site );
    }
    if( cell.empty() ) {
        column.erase( p.y() );
    }
    if( column.empty() ) {
        position_index.erase( p.x() );
    }
}
} // namespace migo_population

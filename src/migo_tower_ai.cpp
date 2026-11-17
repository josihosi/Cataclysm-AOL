#include "migo_tower_ai.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <map>
#include <sstream>
#include <tuple>

#include "active_time.h"
#include "json.h"

namespace migo_tower
{
namespace
{
void write_damage( JsonOut &json, const damage_instance &damage )
{
    auto units = damage.damage_units;
    const auto key = []( const damage_unit &unit ) {
        return std::make_tuple( unit.type.str(), unit.amount, unit.res_pen, unit.res_mult,
                                unit.damage_multiplier, unit.unconditional_res_mult,
                                unit.unconditional_damage_mult );
    };
    std::sort( units.begin(), units.end(), [&]( const auto &a, const auto &b ) {
        return key( a ) < key( b );
    } );
    json.start_array();
    for( const auto &unit : units ) {
        json.start_object();
        json.member( "type", unit.type );
        json.member( "amount", unit.amount );
        json.member( "penetration", unit.res_pen );
        json.member( "armor_multiplier", unit.res_mult );
        json.member( "damage_multiplier", unit.damage_multiplier );
        json.member( "unconditional_armor_multiplier", unit.unconditional_res_mult );
        json.member( "unconditional_damage_multiplier", unit.unconditional_damage_mult );
        json.end_object();
    }
    json.end_array();
}

bool read_damage( const JsonObject &json, const std::string &member, damage_instance &damage )
{
    damage.clear();
    bool known = true;
    if( json.has_array( member ) ) {
        for( JsonObject unit : json.get_array( member ) ) {
            const damage_type_id type( unit.get_string( "type" ) );
            if( !type.is_valid() ) {
                known = false;
                continue;
            }
            damage.damage_units.emplace_back( type, unit.get_float( "amount", 0 ),
                    unit.get_float( "penetration", 0 ), unit.get_float( "armor_multiplier", 1 ),
                    unit.get_float( "damage_multiplier", 1 ),
                    unit.get_float( "unconditional_armor_multiplier", 1 ),
                    unit.get_float( "unconditional_damage_multiplier", 1 ) );
        }
    }
    return known;
}

tripoint_abs_omt source_area( const tripoint_abs_omt &p )
{
    // Floors in one source column share casualty caution; changing emitter
    // kind or lighting another floor cannot erase an earlier costly response.
    return tripoint_abs_omt( p.xy(), 0 );
}

std::uint64_t area_hash( const tripoint_abs_omt &p )
{
    std::uint64_t result = 14695981039346656037ULL;
    for( const unsigned char value : source_area( p ).to_string_writable() ) {
        result = ( result ^ value ) * 1099511628211ULL;
    }
    return result;
}

bool prior_source( const site_state &site, const tripoint_abs_omt &p )
{
    const auto hash = area_hash( p );
    // The repository JSON integer reader uses signed 64-bit storage. Leave
    // the sign bit unused so the persistent filter cannot lose an outcome.
    const int a = hash % 504;
    const int b = ( hash >> 32 ) % 504;
    return ( site.prior_sources[a / 63] & ( 1ULL << ( a % 63 ) ) ) &&
           ( site.prior_sources[b / 63] & ( 1ULL << ( b % 63 ) ) );
}

void note_source( site_state &site, const tripoint_abs_omt &p )
{
    const auto hash = area_hash( p );
    const int a = hash % 504;
    const int b = ( hash >> 32 ) % 504;
    site.prior_sources[a / 63] |= 1ULL << ( a % 63 );
    site.prior_sources[b / 63] |= 1ULL << ( b % 63 );
}

bool usable( const combat_profile &p )
{
    return p.known && std::isfinite( p.hp ) && p.hp > 0 &&
           std::isfinite( p.speed ) && p.speed > 0 && p.melee_cost > 0 && p.ranged_cost > 0;
}

double mitigated( const damage_instance &attack, const resistances &armor )
{
    double damage = 0;
    for( const damage_unit &unit : attack.damage_units ) {
        damage += std::max( 0.0, static_cast<double>( unit.amount - armor.get_effective_resist( unit ) ) ) *
                  unit.damage_multiplier * unit.unconditional_damage_mult;
    }
    return damage;
}

double damage_rate( const combat_profile &attacker, const combat_profile &defender,
                    bool ranged_only, int separation = 1 )
{
    // This exposes the estimate's hit assumption, rather than borrowing the
    // size-based power_rating or pretending that every native attack hits.
    const double hit = std::clamp( 0.5 + ( attacker.hit - defender.dodge ) / 10.0, 0.1, 0.95 );
    double melee = ranged_only ? 0 : mitigated( attacker.melee, defender.armor ) * hit *
                   attacker.speed / attacker.melee_cost;
    const auto exposure = [&]( int range ) {
        return ranged_only && separation > 1 ? std::min( range, separation - 1 ) /
               static_cast<double>( separation - 1 ) : 1.0;
    };
    for( const auto &option : attacker.melee_options ) {
        if( ( ranged_only && option.range > 1 ) || ( !ranged_only && !option.no_adjacent ) ) {
            const double chance = option.dodgeable ?
                                  std::clamp( 0.5 + ( option.hit - defender.dodge ) / 10.0, 0.1, 0.95 ) : 1.0;
            melee = std::max( melee, mitigated( option.damage, defender.armor ) * chance *
                              attacker.speed / std::max( 1, option.cost ) * exposure( option.range ) );
        }
    }
    const double ranged = attacker.ranged_range > 0 ?
                          mitigated( attacker.ranged, defender.armor ) * 0.75 *
                          attacker.speed / attacker.ranged_cost * exposure( attacker.ranged_range ) : 0;
    // Both attacks spend the same native move budget; do not double it.
    return std::max( melee, ranged );
}
} // namespace

void melee_option::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "damage" );
    write_damage( json, damage );
    json.member( "cost", cost );
    json.member( "hit", hit );
    json.member( "dodgeable", dodgeable );
    json.member( "range", range );
    json.member( "no_adjacent", no_adjacent );
    json.end_object();
}

void melee_option::deserialize( const JsonObject &json )
{
    *this = melee_option();
    const bool known = read_damage( json, "damage", damage );
    json.read( "cost", cost );
    json.read( "hit", hit );
    json.read( "dodgeable", dodgeable );
    json.read( "range", range );
    json.read( "no_adjacent", no_adjacent );
    if( !known ) {
        cost = 0;
    }
}

void combat_profile::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "known", known );
    json.member( "hp", hp );
    json.member( "speed", speed );
    json.member( "hit", hit );
    json.member( "dodge", dodge );
    json.member( "melee_cost", melee_cost );
    json.member( "melee" );
    write_damage( json, melee );
    json.member( "melee_options", melee_options );
    std::map<std::string, float> ordered_armor;
    for( const auto &value : armor.resist_vals ) {
        ordered_armor.emplace( value.first.str(), value.second );
    }
    json.member( "armor", ordered_armor );
    json.member( "ranged_cost", ranged_cost );
    json.member( "ranged_range", ranged_range );
    json.member( "ranged" );
    write_damage( json, ranged );
    json.end_object();
}

void combat_profile::deserialize( const JsonObject &json )
{
    *this = combat_profile();
    json.read( "known", known );
    json.read( "hp", hp );
    json.read( "speed", speed );
    json.read( "hit", hit );
    json.read( "dodge", dodge );
    json.read( "melee_cost", melee_cost );
    known = read_damage( json, "melee", melee ) && known;
    json.read( "melee_options", melee_options );
    known = known && std::all_of( melee_options.begin(), melee_options.end(), []( const auto &option ) {
        return option.cost > 0;
    } );
    if( json.has_object( "armor" ) ) {
        const auto values = json.get_object( "armor" );
        for( const JsonMember member : values ) {
            const damage_type_id type( member.name() );
            if( type.is_valid() ) {
                armor.set_resist( type, member.get_float() );
            } else {
                known = false;
            }
        }
    }
    json.read( "ranged_cost", ranged_cost );
    json.read( "ranged_range", ranged_range );
    known = read_damage( json, "ranged", ranged ) && known;
}

std::string capability_key( const std::vector<combat_profile> &profiles )
{
    std::vector<std::string> ordered;
    for( const auto &profile : profiles ) {
        std::ostringstream value;
        { JsonOut json( value ); profile.serialize( json ); }
        ordered.push_back( value.str() );
    }
    std::sort( ordered.begin(), ordered.end() );
    std::ostringstream value;
    { JsonOut json( value ); json.write( ordered ); }
    return value.str();
}

void remember_observation( threat_scenarios &memory, const std::vector<combat_profile> &profiles )
{
    if( profiles.empty() ) {
        return; // lack of a current recognition never erases a known threat
    }
    const std::string key = capability_key( profiles );
    if( std::any_of( memory.begin(), memory.end(), [&]( const auto &scenario ) {
        return capability_key( scenario ) == key;
    } ) ) {
        return;
    }
    if( memory.size() >= 4 ) {
        // Bounded lost detail becomes explicit uncertainty, never zero danger.
        // Only a genuinely completed physical sweep can clear this memory.
        memory = {{combat_profile()}};
    } else {
        memory.push_back( profiles );
    }
}

bool capable( const combat_profile &profile )
{
    return usable( profile ) && ( profile.melee.total_damage() > 0 ||
                                 ( profile.ranged_range > 0 && profile.ranged.total_damage() > 0 ) ||
    std::any_of( profile.melee_options.begin(), profile.melee_options.end(), []( const auto &option ) {
        return option.cost > 0 && option.damage.total_damage() > 0 &&
               ( !option.no_adjacent || option.range > 1 );
    } ) );
}

capability_projection project( const std::vector<combat_profile> &party,
                               const std::vector<combat_profile> &enemies,
                               int separation, bool require_no_casualties )
{
    capability_projection out;
    if( party.empty() || !std::all_of( party.begin(), party.end(), usable ) ||
        !std::all_of( enemies.begin(), enemies.end(), usable ) ) {
        return out;
    }
    out.known = true;
    if( enemies.empty() ) {
        out.sufficient = true;
        return out;
    }
    const int allowed = require_no_casualties ? 0 : static_cast<int>( party.size() ) / 3;
    std::vector<size_t> alive( party.size() );
    std::iota( alive.begin(), alive.end(), 0 );
    std::vector<double> health;
    double slowest = std::numeric_limits<double>::max();
    for( const auto &p : party ) {
        health.push_back( p.hp );
        slowest = std::min( slowest, p.speed );
    }
    auto incoming = [&]( size_t victim, bool ranged ) {
        double total = 0;
        for( const auto &enemy : enemies ) {
            // Entering a shorter weapon's range still earns exposure during
            // the remaining approach; starting outside it is not immunity.
            total += damage_rate( enemy, party[victim], ranged, separation );
        }
        return total;
    };
    auto weakest_first = [&]( bool ranged ) {
        std::stable_sort( alive.begin(), alive.end(), [&]( size_t a, size_t b ) {
            const double da = incoming( a, ranged );
            const double db = incoming( b, ranged );
            const double ta = da > 0 ? health[a] / da : std::numeric_limits<double>::max();
            const double tb = db > 0 ? health[b] / db : std::numeric_limits<double>::max();
            return ta < tb;
        } );
    };
    // Conservatively give no party damage credit during its physical closing
    // interval. Exposed defenders keep their current visible ranged capability.
    double exposure = std::max( 0, separation - 1 ) * 100.0 / slowest;
    weakest_first( true );
    while( exposure > 0 && !alive.empty() ) {
        const size_t victim = alive.front();
        const double rate = incoming( victim, true );
        if( rate <= 0 ) {
            break;
        }
        const double until_death = health[victim] / rate;
        const double spent = std::min( exposure, until_death );
        health[victim] -= rate * spent;
        exposure -= spent;
        out.defeat_seconds += spent;
        if( health[victim] <= 0 ) {
            ++out.casualties;
            alive.erase( alive.begin() );
            if( out.casualties > allowed ) {
                return out;
            }
        }
    }
    double enemy_health = 0;
    for( const auto &e : enemies ) {
        enemy_health += e.hp;
    }
    weakest_first( false );
    while( !alive.empty() ) {
        double output = 0;
        for( size_t member : alive ) {
            double least = std::numeric_limits<double>::max();
            for( const auto &enemy : enemies ) {
                least = std::min( least, damage_rate( party[member], enemy, false ) );
            }
            output += least;
        }
        out.party_damage_per_second = output;
        if( output <= 0 ) {
            return out;
        }
        const size_t victim = alive.front();
        const double rate = incoming( victim, false );
        out.incoming_damage_per_second = rate;
        const double until_victory = enemy_health / output;
        const double until_death = rate > 0 ? health[victim] / rate :
                                   std::numeric_limits<double>::max();
        if( until_victory < until_death ) {
            out.defeat_seconds += until_victory;
            out.sufficient = true;
            return out;
        }
        out.defeat_seconds += until_death;
        enemy_health -= output * until_death;
        ++out.casualties;
        alive.erase( alive.begin() );
        if( out.casualties > allowed ) {
            return out;
        }
    }
    return out;
}

admission_decision select_party( const std::vector<candidate> &available,
                                 const std::vector<combat_profile> &observed_enemies,
                                 int separation, bool require_no_casualties, int home_reserve )
{
    return select_response( available, {observed_enemies}, separation, require_no_casualties,
                            home_reserve );
}

admission_decision select_response( const std::vector<candidate> &available,
                                    const threat_scenarios &observed_scenarios,
                                    int separation, bool require_no_casualties, int home_reserve,
                                    const std::vector<combat_profile> &must_outperform )
{
    admission_decision out;
    std::vector<const candidate *> capable;
    for( const auto &c : available ) {
        if( migo_tower::capable( c.capability ) ) {
            capable.push_back( &c );
        }
    }
    if( capable.size() < 3 ) {
        out.reason = "insufficient_capable_members_and_home_reserve";
        return out;
    }
    if( !std::all_of( observed_scenarios.begin(), observed_scenarios.end(), []( const auto &scene ) {
        return std::all_of( scene.begin(), scene.end(), usable );
    } ) ) {
        out.reason = "observed_capability_unknown";
        return out;
    }
    // Deterministic native member order resolves equal alternatives. Prefer
    // the ordinary two + one scout composition without creating either type.
    std::stable_sort( capable.begin(), capable.end(), []( const auto * a, const auto * b ) {
        return a->member_id < b->member_id;
    } );
    std::vector<const candidate *> preferred;
    for( const auto *c : capable ) {
        if( c->type == "mon_mi_go" && preferred.size() < 2 ) {
            preferred.push_back( c );
        }
    }
    const auto scout = std::find_if( capable.begin(), capable.end(), []( const auto * c ) {
        return c->type == "mon_mi_go_scout";
    } );
    if( preferred.size() == 2 && scout != capable.end() ) {
        preferred.push_back( *scout );
    } else {
        preferred.assign( capable.begin(), capable.begin() + 3 );
    }
    auto consider = [&]( const std::vector<const candidate *> &group ) {
        std::vector<combat_profile> profiles;
        for( const auto *c : group ) {
            profiles.push_back( c->capability );
        }
        capability_projection worst;
        worst.known = true;
        worst.sufficient = true;
        bool previous_would_fail = false;
        for( const auto &scene : observed_scenarios ) {
            const auto estimate = project( profiles, scene, separation, require_no_casualties );
            if( !estimate.sufficient ) {
                out.projection = estimate;
                return false;
            }
            if( estimate.casualties > worst.casualties || estimate.defeat_seconds > worst.defeat_seconds ) {
                worst = estimate;
            }
            if( !must_outperform.empty() &&
                !project( must_outperform, scene, separation, true ).sufficient ) {
                previous_would_fail = true;
            }
        }
        out.projection = worst;
        if( !must_outperform.empty() && profiles.size() <= must_outperform.size() && !previous_would_fail ) {
            out.reason = "requires_stronger_response";
            return false;
        }
        out.members.clear();
        for( const auto *c : group ) {
            out.members.push_back( c->member_id );
        }
        const bool uncertain_source = std::all_of( observed_scenarios.begin(), observed_scenarios.end(),
        []( const auto &scene ) { return scene.empty(); } );
        out.reason = uncertain_source ? "uncertain_signal_ordinary_party" :
                     "observed_threat_survival_margin";
        return true;
    };
    int comparisons = 1;
    for( const int reserve : { std::max( 2, home_reserve ), 0 } ) {
        const int maximum = static_cast<int>( capable.size() ) - reserve;
        if( maximum < 3 ) {
            continue;
        }
        out.required_home_reserve = reserve;
        if( consider( preferred ) ) {
            return out;
        }
        for( int count = 3; count <= maximum; ++count ) {
            std::vector<const candidate *> group;
            std::function<bool( size_t )> choose = [&]( size_t begin ) {
                if( static_cast<int>( group.size() ) == count ) {
                    if( ++comparisons > 4096 ) {
                        return true;
                    }
                    return consider( group );
                }
                for( size_t i = begin; i < capable.size(); ++i ) {
                    if( capable.size() - i < static_cast<size_t>( count ) - group.size() ) {
                        break;
                    }
                    group.push_back( capable[i] );
                    if( choose( i + 1 ) ) {
                        return true;
                    }
                    group.pop_back();
                }
                return false;
            };
            if( choose( 0 ) ) {
                if( out.members.empty() ) {
                    out.reason = "capability_selection_budget_exhausted";
                }
                return out;
            }
        }
    }
    if( out.reason != "requires_stronger_response" ) {
        out.reason = "largest_party_fails_survival_margin";
    }
    return out;
}

void signal_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "sample_id", sample_id );
    json.member( "kind", kind );
    json.member( "source_omt", source_omt );
    json.member( "source_ms", source_ms );
    json.member( "observer_member", observer_member );
    json.member( "observer_position", observer_position );
    json.member( "observed_turn", observed_turn );
    json.end_object();
}

void signal_record::deserialize( const JsonObject &json )
{
    json.read( "sample_id", sample_id );
    json.read( "kind", kind );
    json.read( "source_omt", source_omt );
    json.read( "source_ms", source_ms );
    json.read( "observer_member", observer_member );
    json.read( "observer_position", observer_position );
    json.read( "observed_turn", observed_turn );
}

int source_history::caution_level() const
{
    return prior_outcome_unavailable ? 2 : std::min( 2, casualty_attempts + overdue_attempts );
}

bool source_history::requires_stronger() const
{
    return caution_level() == 1 &&
           ( !last_witnessed_losses.empty() || outcome == "overdue_missing" );
}

bool source_history::retry_ready( std::int64_t now, const std::vector<combat_profile> &party,
                                 const admission_decision &decision, bool current_recognition ) const
{
    if( completed_generation == 0 && !prior_outcome_unavailable ) {
        return true;
    }
    if( now < retry_after_active ) {
        return false;
    }
    const bool source_changed = latest_evidence != completed_evidence;
    const bool party_changed = capability_key( party ) != capability_key( previous_party );
    if( caution_level() == 0 ) {
        // New residents/health/time cannot reopen an unchanged empty/hazard
        // sweep. This requires a real changed source observation.
        return source_changed;
    }
    if( !decision.projection.known || !decision.projection.sufficient ||
        decision.projection.casualties != 0 || opposition.empty() ) {
        return false;
    }
    if( prior_outcome_unavailable ) {
        // An evicted outcome cannot mint another blind ordinary raid. A
        // positive filter match is conservative uncertainty, not a loss fact.
        return episode > episode_at_outcome && current_recognition && source_changed;
    }
    return source_changed || party_changed;
}

bool source_history::finish( std::uint64_t generation, std::int64_t now,
                             const std::string &result, const std::vector<std::string> &losses,
                             bool overdue_missing, const std::vector<combat_profile> &party )
{
    const bool resolves_overdue = generation == completed_generation && outcome == "overdue_missing" &&
                                  ( !losses.empty() || !overdue_missing );
    if( generation == 0 || generation < completed_generation ||
        ( generation == completed_generation && !resolves_overdue ) ) {
        return false;
    }
    if( resolves_overdue && overdue_attempts > 0 ) {
        // Later genuine communication resolves this same attempt's unknown
        // fate. It cannot count the overdue warning as another casualty raid.
        --overdue_attempts;
    }
    completed_generation = generation;
    finished_active = now;
    previous_party = party;
    last_witnessed_losses = losses;
    if( !losses.empty() ) {
        ++casualty_attempts;
        outcome = "witnessed_losses";
    } else if( overdue_missing ) {
        ++overdue_attempts;
        outcome = "overdue_missing";
    } else {
        outcome = result;
    }
    prior_outcome_unavailable = false; // a new real outcome is now retained
    episode_at_outcome = episode;
    completed_evidence = latest_evidence;
    retry_after_active = now + ( caution_level() >= 2 ? 7 * 24 * 60 * 60 : 24 * 60 * 60 );
    return true;
}

void source_history::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "area", area );
    json.member( "episode", episode );
    json.member( "episode_at_outcome", episode_at_outcome );
    json.member( "observed_active", observed_active );
    json.member( "extinct_world", extinct_world );
    if( rally_episode > 0 || !rally_outcomes.empty() ) {
        json.member( "rally_episode", rally_episode );
        json.member( "rally_outcomes", rally_outcomes );
    }
    json.member( "observed_access", observed_access );
    json.member( "prior_outcome_unavailable", prior_outcome_unavailable );
    json.member( "casualty_attempts", casualty_attempts );
    json.member( "overdue_attempts", overdue_attempts );
    json.member( "retry_after_active", retry_after_active );
    json.member( "finished_active", finished_active );
    json.member( "completed_generation", completed_generation );
    json.member( "outcome", outcome );
    json.member( "completed_evidence", completed_evidence );
    json.member( "latest_evidence", latest_evidence );
    json.member( "previous_party", previous_party );
    json.member( "opposition", opposition );
    json.member( "last_witnessed_losses", last_witnessed_losses );
    json.end_object();
}

void source_history::deserialize( const JsonObject &json )
{
    *this = source_history();
    json.read( "area", area );
    area = source_area( area );
    json.read( "episode", episode );
    json.read( "episode_at_outcome", episode_at_outcome );
    json.read( "observed_active", observed_active );
    json.read( "extinct_world", extinct_world );
    json.read( "rally_episode", rally_episode );
    json.read( "rally_outcomes", rally_outcomes );
    if( rally_episode > episode || ( rally_episode == 0 && !rally_outcomes.empty() ) ||
        rally_outcomes.size() > 16 || std::any_of( rally_outcomes.begin(), rally_outcomes.end(),
    []( const auto &entry ) {
        return entry.first.empty() || entry.first.size() > 128 || entry.second < 0 ||
               entry.second > static_cast<int>( raid_sweep::home_rally_outcome::expired );
    } ) ) {
        json.throw_error( "invalid home rally source facts" );
    }
    json.read( "observed_access", observed_access );
    json.read( "prior_outcome_unavailable", prior_outcome_unavailable );
    json.read( "casualty_attempts", casualty_attempts );
    json.read( "overdue_attempts", overdue_attempts );
    json.read( "retry_after_active", retry_after_active );
    json.read( "finished_active", finished_active );
    json.read( "completed_generation", completed_generation );
    json.read( "outcome", outcome );
    json.read( "completed_evidence", completed_evidence );
    json.read( "latest_evidence", latest_evidence );
    json.read( "previous_party", previous_party );
    json.read( "opposition", opposition );
    json.read( "last_witnessed_losses", last_witnessed_losses );
}

void member_intent::retreat()
{
    if( active() && phase != member_phase::returning ) {
        phase = member_phase::returning;
        // A previous approach/search segment cannot own a later withdrawal.
        waypoint.reset();
        navigation.clear();
        navigation_failures.clear();
        coarse_work.clear();
        coarse_work_position.reset();
        coarse_route.clear();
        coarse_can_cross_water.reset();
        coarse_all_fly.reset();
    }
}

void member_intent::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "generation", generation );
    json.member( "phase", static_cast<int>( phase ) );
    json.member( "home", home );
    json.member( "waypoint", waypoint );
    json.member( "witnessed_losses", witnessed_losses );
    json.member( "observed_threats", observed_threats );
    json.member( "last_energy_world", last_energy_world );
    json.member( "last_native_process_world", last_native_process_world );
    json.member( "last_combat_world", last_combat_world );
    json.member( "last_observed_enemy_world", last_observed_enemy_world );
    json.member( "last_refused_world", last_refused_world );
    json.member( "activity_bound", activity_bound );
    json.member( "activity_generation", activity_generation );
    json.member( "activity_suspension", activity_suspension );
    if( coarse_work.edge_bound || coarse_work.work_millis != 0 ) {
        json.member( "coarse_work", coarse_work );
        json.member( "coarse_work_position", coarse_work_position );
        if( coarse_can_cross_water && coarse_all_fly ) {
            json.member( "coarse_can_cross_water", *coarse_can_cross_water );
            json.member( "coarse_all_fly", *coarse_all_fly );
        }
    }
    json.end_object();
}

void member_intent::deserialize( const JsonObject &json )
{
    *this = member_intent();
    json.read( "generation", generation );
    phase = static_cast<member_phase>( std::clamp( json.get_int( "phase", 0 ), 0, 3 ) );
    json.read( "home", home );
    json.read( "waypoint", waypoint );
    json.read( "witnessed_losses", witnessed_losses );
    json.read( "observed_threats", observed_threats );
    json.read( "last_energy_world", last_energy_world );
    json.read( "last_native_process_world", last_native_process_world );
    json.read( "last_combat_world", last_combat_world );
    json.read( "last_observed_enemy_world", last_observed_enemy_world );
    json.read( "last_refused_world", last_refused_world );
    json.read( "activity_bound", activity_bound );
    json.read( "activity_generation", activity_generation );
    json.read( "activity_suspension", activity_suspension );
    if( json.has_member( "coarse_work" ) ) {
        json.read( "coarse_work", coarse_work );
        json.read( "coarse_work_position", coarse_work_position );
        if( json.has_member( "coarse_can_cross_water" ) != json.has_member( "coarse_all_fly" ) ) {
            json.throw_error( "incomplete Mi-go coarse travel capability" );
        }
        if( json.has_member( "coarse_can_cross_water" ) ) {
            coarse_can_cross_water = json.get_bool( "coarse_can_cross_water" );
            coarse_all_fly = json.get_bool( "coarse_all_fly" );
        }
        if( coarse_work.edge_bound && coarse_work.from.size() != 1 ) {
            json.throw_error( "invalid Mi-go member coarse work" );
        }
        if( !coarse_work_position ) {
            // An unbound historical or malformed work balance cannot be lent
            // to a body at a position that was never authenticated.
            coarse_work.clear();
        }
    }
}

void operation_member::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "member_id", member_id );
    json.member( "settled", settled );
    json.member( "returned", returned );
    json.member( "last_position", last_position );
    json.end_object();
}

void operation_member::deserialize( const JsonObject &json )
{
    *this = operation_member();
    json.read( "member_id", member_id );
    json.read( "settled", settled );
    json.read( "returned", returned );
    json.read( "last_position", last_position );
}

bool operation_record::contains( const std::string &id ) const
{
    return std::any_of( members.begin(), members.end(), [&]( const auto &member ) {
        return member.member_id == id;
    } );
}

void operation_record::advance( int world_turn, int active_turns )
{
    sweep.advance( world_turn, resolved ? 0 : active_turns );
}

void operation_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "activity", activity );
    json.member( "generation", generation );
    json.member( "source", source );
    json.member( "members", members );
    json.member( "sweep", sweep );
    json.member( "overdue_after_active", overdue_after_active );
    json.member( "communicated_losses", communicated_losses );
    json.member( "party_at_departure", party_at_departure );
    json.member( "resolved", resolved );
    json.end_object();
}

void operation_record::deserialize( const JsonObject &json )
{
    *this = operation_record();
    json.read( "activity", activity );
    json.read( "generation", generation );
    json.read( "source", source );
    json.read( "members", members );
    json.read( "sweep", sweep );
    json.read( "overdue_after_active", overdue_after_active );
    json.read( "communicated_losses", communicated_losses );
    json.read( "party_at_departure", party_at_departure );
    json.read( "resolved", resolved );
}

void site_state::advance( int world_turn, int active_turns )
{
    if( world_turn <= last_service_world ) {
        return;
    }
    active_elapsed += active_time::clipped_turns( world_turn, last_service_world, active_turns );
    last_service_world = world_turn;
}

source_history *site_state::find_source( const tripoint_abs_omt &p )
{
    const auto area = source_area( p );
    const auto found = std::find_if( sources.begin(), sources.end(), [&]( const auto &source ) {
        return source.area == area;
    } );
    return found == sources.end() ? nullptr : &*found;
}

const source_history *site_state::find_source( const tripoint_abs_omt &p ) const
{
    const auto area = source_area( p );
    const auto found = std::find_if( sources.begin(), sources.end(), [&]( const auto &source ) {
        return source.area == area;
    } );
    return found == sources.end() ? nullptr : &*found;
}

source_history *site_state::remember_source( const tripoint_abs_omt &p )
{
    if( auto *found = find_source( p ) ) {
        return found;
    }
    if( sources.size() >= 4 ) {
        auto oldest = sources.end();
        for( auto it = sources.begin(); it != sources.end(); ++it ) {
            if( it->retry_after_active > active_elapsed ||
                ( operation && !operation->resolved && source_area( operation->source.source_omt ) == it->area ) ) {
                continue;
            }
            if( oldest == sources.end() || it->finished_active < oldest->finished_active ) {
                oldest = it;
            }
        }
        if( oldest == sources.end() ) {
            return nullptr;
        }
        if( oldest->completed_generation > 0 || !oldest->opposition.empty() ||
            !oldest->rally_outcomes.empty() ||
            oldest->prior_outcome_unavailable ) {
            note_source( *this, oldest->area );
        }
        sources.erase( oldest );
    }
    source_history fresh;
    fresh.area = source_area( p );
    fresh.prior_outcome_unavailable = prior_source( *this, p );
    if( fresh.prior_outcome_unavailable ) {
        fresh.episode_at_outcome = fresh.episode;
        fresh.retry_after_active = active_elapsed + 7 * 24 * 60 * 60;
        fresh.outcome = "prior_source_history_unavailable";
    }
    sources.push_back( std::move( fresh ) );
    return &sources.back();
}

void site_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "site_id", site_id );
    json.member( "anchor", anchor );
    json.member( "last_sense_turn", last_sense_turn );
    json.member( "signal", signal );
    json.member( "recognized_enemies", recognized_enemies );
    json.member( "decision_members", decision.members );
    json.member( "decision_reason", decision.reason );
    json.member( "decision_home_reserve", decision.required_home_reserve );
    json.member( "projection_known", decision.projection.known );
    json.member( "projection_sufficient", decision.projection.sufficient );
    json.member( "projection_casualties", decision.projection.casualties );
    json.member( "projection_defeat_seconds", decision.projection.defeat_seconds );
    json.member( "projection_party_dps", decision.projection.party_damage_per_second );
    json.member( "projection_incoming_dps", decision.projection.incoming_damage_per_second );
    json.member( "next_generation", next_generation );
    json.member( "operation", operation );
    if( response_home_rally ) {
        json.member( "response_home_rally", response_home_rally );
    }
    json.member( "last_service_world", last_service_world );
    json.member( "active_elapsed", active_elapsed );
    json.member( "sources", sources );
    json.member( "prior_sources", prior_sources );
    json.end_object();
}

void site_state::deserialize( const JsonObject &json )
{
    *this = site_state();
    json.read( "site_id", site_id );
    json.read( "anchor", anchor );
    json.read( "last_sense_turn", last_sense_turn );
    json.read( "signal", signal );
    json.read( "recognized_enemies", recognized_enemies );
    json.read( "decision_members", decision.members );
    json.read( "decision_reason", decision.reason );
    json.read( "decision_home_reserve", decision.required_home_reserve );
    json.read( "projection_known", decision.projection.known );
    json.read( "projection_sufficient", decision.projection.sufficient );
    json.read( "projection_casualties", decision.projection.casualties );
    json.read( "projection_defeat_seconds", decision.projection.defeat_seconds );
    json.read( "projection_party_dps", decision.projection.party_damage_per_second );
    json.read( "projection_incoming_dps", decision.projection.incoming_damage_per_second );
    json.read( "next_generation", next_generation );
    json.read( "operation", operation );
    json.read( "response_home_rally", response_home_rally );
    if( response_home_rally && response_home_rally->generation >= next_generation ) {
        next_generation = response_home_rally->generation == std::numeric_limits<std::uint64_t>::max() ?
                          response_home_rally->generation : response_home_rally->generation + 1;
    }
    if( operation && operation->generation >= next_generation ) {
        next_generation = operation->generation == std::numeric_limits<std::uint64_t>::max() ?
                          operation->generation : operation->generation + 1;
    }
    json.read( "last_service_world", last_service_world );
    json.read( "active_elapsed", active_elapsed );
    json.read( "sources", sources );
    json.read( "prior_sources", prior_sources );
}

site_state *world_state::find( const std::string &id )
{
    if( indexed_site_count > sites.size() ) {
        site_index.clear();
        indexed_site_count = 0;
    }
    while( indexed_site_count < sites.size() ) {
        site_index.emplace( sites[indexed_site_count].site_id, indexed_site_count );
        ++indexed_site_count;
    }
    const auto found = site_index.find( id );
    return found == site_index.end() ? nullptr : &sites[found->second];
}

const site_state *world_state::find( const std::string &id ) const
{
    // A genuinely const snapshot may read an existing index, but must not
    // mutate it through the live owner's lazy indexing path.
    const auto cached = site_index.find( id );
    if( cached != site_index.end() && cached->second < sites.size() &&
        sites[cached->second].site_id == id ) {
        return &sites[cached->second];
    }
    const auto found = std::find_if( sites.begin(), sites.end(), [&]( const auto &site ) {
        return site.site_id == id;
    } );
    return found == sites.end() ? nullptr : &*found;
}

void world_state::clear()
{
    last_signal_scan = -1;
    sites.clear();
    site_index.clear();
    indexed_site_count = 0;
}

void world_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "last_signal_scan", last_signal_scan );
    json.member( "sites", sites );
    json.end_object();
}

void world_state::deserialize( const JsonObject &json )
{
    clear();
    json.read( "last_signal_scan", last_signal_scan );
    json.read( "sites", sites );
}
} // namespace migo_tower

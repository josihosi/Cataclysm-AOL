#include "npctrade.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "avatar.h"
#include "bandit_live_world.h"
#include "game.h"
#include "json.h"
#include "overmapbuffer.h"
#include "visitable.h"
#include <sstream>
#include <tuple>
#include "character.h"
#include "character_attire.h"
#include "debug.h"
#include "enums.h"
#include "faction.h"
#include "item.h"
#include "item_category.h" // IWYU pragma: keep
#include "item_contents.h"
#include "item_location.h"
#include "item_pocket.h"
#include "map.h"
#include "mapdata.h"
#include "output.h"
#include "npc.h"
#include "npc_opinion.h"
#include "npctrade_utils.h"
#include "pocket_type.h"
#include "ret_val.h"
#include "skill.h"
#include "trade_ui.h"
#include "type_id.h"
#include "ui_manager.h"
#include "units.h"

static const flag_id json_flag_NO_UNWIELD( "NO_UNWIELD" );
static const skill_id skill_speech( "speech" );

std::list<item> npc_trading::transfer_items( trade_selector::select_t &stuff, Character &giver,
        Character &receiver, std::list<item_location *> &from_map, bool use_escrow )
{
    // escrow is used only when the npc is the destination. Item transfer to the npc is deferred.
    std::list<item> escrow = std::list<item>();

    for( trade_selector::entry_t &ip : stuff ) {
        if( ip.first.get_item() == nullptr ) {
            DebugLog( D_ERROR, D_NPC ) << "Null item being traded in npc_trading::transfer_items";
            continue;
        }
        item gift = *ip.first.get_item();

        npc const *npc = nullptr;
        std::function<bool( item_location const &, int )> f_wants;
        if( giver.is_npc() ) {
            npc = giver.as_npc();
            f_wants = [npc]( item_location const & it, int price ) {
                return npc->wants_to_sell( it, price ).success();
            };
        } else if( receiver.is_npc() ) {
            npc = receiver.as_npc();
            f_wants = [npc]( item_location const & it, int price ) {
                return npc->wants_to_buy( *it, price ).success();
            };
        }
        // spill contained, unwanted items
        if( f_wants && gift.is_container() ) {
            for( item *it : gift.get_contents().all_items_top() ) {
                int const price =
                    trading_price( giver, receiver, { item_location{ giver, it }, 1 } );
                if( !f_wants( item_location{ ip.first, it }, price ) ) {
                    giver.i_add_or_drop( *it, 1, ip.first.get_item() );
                    gift.remove_item( *it );
                }
            }
        }

        gift.set_owner( receiver );

        // Items are moving to escrow.
        if( use_escrow && ip.first->count_by_charges() ) {
            gift.charges = ip.second;
            escrow.emplace_back( gift );
        } else if( use_escrow ) {
            std::fill_n( std::back_inserter( escrow ), ip.second, gift );
            // No escrow in use. Items moving from giver to receiver.
        } else if( ip.first->count_by_charges() ) {
            gift.charges = ip.second;
            item newit = item( gift );
            ret_val<item_location> ret = receiver.i_add_or_fill( newit, true, nullptr, &gift,
                                         /*allow_drop=*/true, /*allow_wield=*/true, false );
        } else {
            for( int i = 0; i < ip.second; i++ ) {
                receiver.i_add( gift );
            }
        }

        if( ip.first.held_by( giver ) ) {
            if( ip.first->count_by_charges() ) {
                giver.use_charges( gift.typeId(), ip.second );
            } else if( ip.second > 0 ) {
                giver.remove_items_with( [&ip]( const item & i ) {
                    return &i == ip.first.get_item();
                }, ip.second );
            }
        } else {
            if( ip.first->count_by_charges() ) {
                ip.first.get_item()->set_var( "trade_charges", ip.second );
            } else {
                ip.first.get_item()->set_var( "trade_amount", 1 );
            }
            from_map.push_back( &ip.first );
        }
    }
    return escrow;
}

std::vector<item_pricing> npc_trading::init_selling( npc &np )
{
    std::vector<item_pricing> result;
    const std::vector<item *> inv_all = np.items_with( []( const item & it ) {
        return !it.made_of( phase_id::LIQUID );
    } );
    for( item *i : inv_all ) {
        item &it = *i;

        int val = np.value( it );
        // FIXME: this item_location is a hack
        if( np.wants_to_sell( item_location{ np, i }, val ).success() ) {
            result.emplace_back( np, it, val, static_cast<int>( it.count() ) );
        }
    }

    item_location weapon = np.get_wielded_item();
    if(
        np.will_exchange_items_freely() && weapon && !weapon->has_flag( json_flag_NO_UNWIELD )
    ) {
        result.emplace_back( np, *weapon, np.value( *weapon ), false );
    }

    return result;
}

double npc_trading::net_price_adjustment( const Character &buyer, const Character &seller )
{
    // Adjust the prices based on your social skill.
    // cap adjustment so nothing is ever sold below value
    ///\EFFECT_INT_NPC slightly increases bartering price changes, relative to your INT

    ///\EFFECT_BARTER_NPC increases bartering price changes, relative to your BARTER

    ///\EFFECT_INT slightly increases bartering price changes, relative to NPC INT

    ///\EFFECT_BARTER increases bartering price changes, relative to NPC BARTER
    int const int_diff = seller.get_int() - buyer.get_int();
    double const int_adj = 1 + 0.05 * std::min( 19, std::abs( int_diff ) );
    double const soc_adj = price_adjustment( round( seller.get_skill_level( skill_speech ) -
                           buyer.get_skill_level( skill_speech ) ) );
    double const adjust = int_diff >= 0 ? int_adj * soc_adj : soc_adj / int_adj;
    return seller.is_npc() ? adjust : -1 / adjust;
}

int npc_trading::bionic_install_price( Character &installer, Character &patient,
                                       item_location const &bionic )
{
    return bionic->price( true ) * 2 +
           ( bionic->is_owned_by( patient )
             ? 0
             : npc_trading::trading_price( patient, installer, { bionic, 1 } ) );
}

int npc_trading::adjusted_price( item const *it, int amount, Character const &buyer,
                                 Character const &seller )
{
    npc const *faction_party = buyer.is_npc() ? buyer.as_npc() : seller.as_npc();
    faction_price_rule const *const fpr = faction_party->get_price_rules( *it );

    double price = it->price_no_contents( true, fpr != nullptr ? fpr->price : std::nullopt );
    if( fpr != nullptr ) {
        price *= fpr->premium;
        if( seller.is_npc() ) {
            price *= fpr->markup;
        }
    }
    if( it->count_by_charges() && amount >= 0 ) {
        price *= static_cast<double>( amount ) / it->charges;
    }
    if( buyer.is_npc() ) {
        price = buyer.as_npc()->value( *it, price );
    } else if( seller.is_npc() ) {
        price = seller.as_npc()->value( *it, price );
    }

    if( fpr != nullptr && fpr->fixed_adj.has_value() ) {
        double const fixed_adj = fpr->fixed_adj.value();
        price *= 1 + ( seller.is_npc() ? fixed_adj : -fixed_adj );
    } else {
        double const adjust = npc_trading::net_price_adjustment( buyer, seller );
        price *= 1 + 0.25 * adjust;
    }

    return static_cast<int>( std::ceil( price ) );
}

namespace
{
int _trading_price( Character const &buyer, Character const &seller, item_location const &it,
                    int amount )
{
    if( seller.is_npc() ) {
        if( !seller.as_npc()->wants_to_sell( it, 1 ).success() ) {
            return 0;
        }
    } else if( buyer.is_npc() ) {
        if( !buyer.as_npc()->wants_to_buy( *it, 1 ).success() ) {
            return 0;
        }
    }
    int ret = npc_trading::adjusted_price( it.get_item(), amount, buyer, seller );
    for( item_pocket const *pk : it->get_standard_pockets() ) {
        for( item const *pkit : pk->all_items_top() ) {
            ret += _trading_price( buyer, seller, item_location{ it, const_cast<item *>( pkit ) },
                                   -1 );
        }
    }
    return ret;
}
} // namespace

int npc_trading::trading_price( Character const &buyer, Character const &seller,
                                trade_selector::entry_t const &it )
{
    return _trading_price( buyer, seller, it.first, it.second );
}

void item_pricing::set_values( int ip_count )
{
    const item *i_p = loc.get_item();
    is_container = i_p->is_container() || i_p->is_ammo_container();
    vol = i_p->volume();
    weight = i_p->weight();
    if( is_container || i_p->count() == 1 ) {
        count = ip_count;
    } else {
        charges = i_p->count();
        if( charges > 0 ) {
            price /= charges;
            vol /= charges;
            weight /= charges;
        } else {
            debugmsg( "item %s has zero or negative charges", i_p->typeId().str() );
        }
    }
}

// Returns how much the NPC will owe you after this transaction.
// You must also check if they will accept the trade.
int npc_trading::calc_npc_owes_you( const npc &np, int your_balance )
{
    // Friends don't hold debts against friends.
    if( np.will_exchange_items_freely() ) {
        return 0;
    }

    // If they're going to owe you more than before, and it's more than they're willing
    // to owe, then cap the amount owed at the present level or their willingness to owe
    // (whichever is bigger).
    //
    // When could they owe you more than max_willing_to_owe? It could be from quest rewards,
    // when they were less angry, or from when you were better friends.
    if( your_balance > np.op_of_u.owed && your_balance > np.max_willing_to_owe() ) {
        return std::max( np.op_of_u.owed, np.max_willing_to_owe() );
    }

    // Fair's fair. NPC will remember this debt (or credit they've extended)
    return your_balance;
}
void npc_trading::update_npc_owed( npc &np, int your_balance, int your_sale_value )
{
    np.op_of_u.owed = calc_npc_owes_you( np, your_balance );
    np.op_of_u.sold += your_sale_value;
}

// Oh my aching head
// op_of_u.owed is the positive when the NPC owes the player, and negative if the player owes the
// NPC
// cost is positive when the player owes the NPC money for a service to be performed
bool npc_trading::trade( npc &np, int cost, const std::string &deal,
                         const int you_nearby_item_radius, const int you_nearby_ally_radius,
                         basecamp *you_basecamp, Character *payer, const bool encounter_only )
{
    np.shop_restock();
    //np.drop_items( np.weight_carried() - np.weight_capacity(),
    //               np.volume_carried() - np.volume_capacity() );
    np.drop_invalid_inventory();

    Character &physical_payer = payer != nullptr ? *payer : static_cast<Character &>( get_avatar() );
    // Cover retained activity popups before constructing both trade panes and
    // the header.  Nested trade prompts remain above this modal guard.
    ui_adaptor modal( ui_adaptor::disable_uis_below{} );
    std::unique_ptr<trade_ui> tradeui = std::make_unique<trade_ui>( physical_payer, np, cost, deal,
                                      you_nearby_item_radius, you_nearby_ally_radius,
                                      you_basecamp, encounter_only );
    trade_ui::trade_result_t trade_result = tradeui->perform_trade();
    tradeui.reset();
    return complete_trade( np, physical_payer, trade_result );
}

namespace
{
const bandit_live_world::site_record *payment_site( const npc &trader, const Character &payer )
{
    const auto *site = bandit_live_world::active_operation_duty_site_for(
                           overmap_buffer.global_state.bandit_live_world, trader.getID() );
    if( site == nullptr ) { return nullptr; }
    const auto &operation = site->active_hostile_operation;
    if( !operation.is_active() || operation.operation_kind != bandit_live_world::hostile_operation_kind::shakedown ||
        !operation.shakedown_contact_established || operation.shakedown_receiver_id != payer.getID() ||
        operation.shakedown_receiver_is_avatar != payer.is_avatar() ||
        ( !operation.shakedown_pending_branch.empty() && operation.shakedown_pending_branch != "pay_waiting_route" ) ||
        trader.is_dead() || !trader.is_active() || payer.is_dead_state() ||
        ( payer.is_npc() && !payer.as_npc()->is_active() ) ) {
        return nullptr;
    }
    return site;
}

struct witnessed_transfer {
    character_id witness;
    npc_property_theft fact;
};

std::vector<witnessed_transfer> observe_property_transfer(
    const npc &trader, const Character &payer, const trade_selector::select_t &selected,
    const bandit_live_world::site_record *site )
{
    std::vector<witnessed_transfer> witnessed;
    if( site == nullptr ) { return witnessed; }
    const auto &outing = site->active_hostile_operation.reservation;
    const map &here = get_map();
    for( npc &observer : g->all_npcs() ) {
        if( &observer == &trader || observer.is_fake() || observer.is_hallucination() ||
            observer.is_dead() ||
            !observer.is_active() || observer.in_sleep_state() || observer.is_blind() ||
            observer.has_effect( efftype_id( "narcosis" ) ) || observer.has_effect( efftype_id( "npc_suspend" ) ) ||
            !observer.sees( here, payer ) ) { continue; }
        npc_property_theft fact;
        fact.site_id = site->site_id;
        fact.operation_id = outing.activity_id;
        fact.generation = outing.generation;
        fact.collector_id = trader.getID();
        fact.receiver_id = payer.getID();
        fact.wronged_faction = observer.get_fac_id();
        fact.when = calendar::turn;
        fact.incident_key = "shakedown:" + site->site_id + ":" + outing.activity_id + ":" +
                            std::to_string( outing.generation ) + ":" + observer.get_fac_id().str();
        for( const auto id : outing.member_ids ) {
            const npc *actor = g->find_npc( id );
            if( actor && !actor->is_fake() && !actor->is_hallucination() && !actor->is_dead() &&
                !outing.member_is_resolved( id ) &&
                actor->get_fac_id() == trader.get_fac_id() && observer.sees( here, *actor ) ) {
                fact.culprit_ids.push_back( id );
            }
        }
        if( fact.culprit_ids.empty() ) { continue; }
        for( const auto &entry : selected ) {
            const auto position = here.get_abs( entry.first.pos_bub( here ) );
            if( !observer.sees( here, entry.first.pos_bub( here ) ) ||
                ( entry.first.where_recursive() == item_location::type::map &&
                  !here.sees_some_items( entry.first.pos_bub( here ), observer ) ) ) { continue; }
            std::function<void( const item * )> observe = [&]( const item *goods ) {
                const auto owner = goods->get_owner();
                bool wronged = owner == fact.wronged_faction && owner != payer.get_faction_id() &&
                               owner != trader.get_fac_id();
                // Remembered voluntary taking is evidence of involvement. An
                // old_owner tag alone, being nearby or controlling Pay is not.
                // Native permission/return resolves that tag; history alone
                // cannot accuse a later coerced receiver again.
                for( const auto &prior : observer.known_property_thefts ) {
                    if( !prior.voluntary_pickup || prior.wronged_faction != fact.wronged_faction ||
                        goods->get_old_owner() != fact.wronged_faction ) { continue; }
                    if( std::any_of( prior.property.begin(), prior.property.end(), [&]( const auto &source ) {
                        return source.source_uid == goods->uid().get_value();
                    } ) ) {
                        wronged = true;
                        for( const auto id : prior.culprit_ids ) {
                            if( std::find( fact.culprit_ids.begin(), fact.culprit_ids.end(), id ) == fact.culprit_ids.end() ) {
                                fact.culprit_ids.push_back( id );
                            }
                        }
                    }
                }
                if( wronged ) {
                    fact.property.push_back( { goods->uid().get_value(), goods->typeId(),
                        goods == entry.first.get_item() && goods->count_by_charges() ? entry.second :
                        goods->count_by_charges() ? goods->charges : 1,
                        fact.wronged_faction, position } );
                }
                // Selecting a physical child transfers that child, not its
                // containing bag. When a container itself is handed over,
                // native transparent/open pockets alone expose its contents.
                for( const item *visible : goods->all_known_contents() ) {
                    observe( visible );
                }
            };
            observe( entry.first.get_item() );
        }
        if( fact.valid() ) { witnessed.push_back( { observer.getID(), std::move( fact ) } ); }
    }
    return witnessed;
}
} // namespace

std::string npc_trading::encounter_payment_identity( const npc &trader, const Character &payer )
{
    const auto *site = payment_site( trader, payer );
    if( site == nullptr ) { return {}; }
    const auto &operation = site->active_hostile_operation;
    const auto &outing = operation.reservation;
    std::ostringstream stream;
    stream << site->site_id << '/' << outing.activity_id << '/' << outing.generation << '/'
           << site->anchor.to_string() << '/' << operation.source_report_application_key << '/'
           << trader.getID() << '/' << trader.get_fac_id().str() << '/' << payer.getID() << '/'
           << payer.get_faction_id().str() << '/' << payer.is_avatar();
    for( const auto id : outing.member_ids ) {
        stream << '/' << id << ':' << outing.member_is_resolved( id );
    }
    return stream.str();
}

bool npc_trading::trade_to_stash( npc &np, Character &payer, const tripoint_abs_omt &home,
                                  const int cost, const std::string &deal,
                                  const int nearby_item_radius, const int nearby_ally_radius,
                                  basecamp *payer_basecamp, const bool encounter_only, const int nearby_z_radius )
{
    np.shop_restock();
    np.drop_invalid_inventory();
    ui_adaptor modal( ui_adaptor::disable_uis_below{} );
    const auto make_ui = [&]() {
        return std::make_unique<trade_ui>( payer, np, cost, deal, nearby_item_radius, nearby_ally_radius,
                                          payer_basecamp, encounter_only, true, nearby_z_radius );
    };
    auto tradeui = make_ui();
    while( true ) {
        trade_ui::trade_result_t result = tradeui->perform_trade();
        if( !result.traded ) {
            return false;
        }
        stash_trade_failure failure;
        if( complete_trade_to_stash( np, payer, result, home, nearby_item_radius,
                                    nearby_ally_radius, nearby_z_radius, &failure ) ) {
            return true;
        }
        if( failure == stash_trade_failure::source ) {
            // Discard stale selector locations before drawing/retrying them.
            tradeui.reset();
            popup( _( "The selected goods are no longer eligible or accessible.  No goods changed hands.  Choose a new offer or cancel." ) );
            tradeui = make_ui();
        } else {
            popup( _( "The bandit home camp cannot store this payment.  No goods changed hands.  Change the offer or cancel." ) );
        }
        // perform_trade resets only the exit/confirmation flags. It preserves
        // the offer and publishes a fresh authenticated native selector.
    }
}

bool npc_trading::complete_trade_to_stash( npc &np, Character &payer,
        trade_ui::trade_result_t &result, const tripoint_abs_omt &home,
        const int nearby_item_radius, const int nearby_ally_radius, const int nearby_z_radius,
        stash_trade_failure *failure )
{
    if( failure ) {
        *failure = stash_trade_failure::source;
    }
    if( !result.traded ) {
        return false;
    }
    const auto *site = payment_site( np, payer );
    if( result.encounter_payment &&
        ( result.encounter_identity != encounter_payment_identity( np, payer ) ||
          ( site && site->anchor != home ) ) ) { return false; }
    for( const auto &snapshot : result.source_snapshots ) {
        if( !snapshot.location || get_map().get_abs( snapshot.location.pos_bub( get_map() ) ) != snapshot.position ) {
            return false;
        }
        std::vector<std::tuple<std::int64_t, itype_id, faction_id, int>> current;
        snapshot.location->visit_items( [&]( const item *goods, const item * ) {
            current.emplace_back( goods->uid().get_value(), goods->typeId(), goods->get_owner(), goods->charges );
            return VisitResponse::NEXT;
        } );
        if( current != snapshot.items ) { return false; }
    }
    // Native selections name each discrete item once (charged items carry a
    // quantity). Reject stale, duplicate and overlapping parent/child sources
    // before loading or changing the destination or debiting anything.
    std::vector<const item *> sources;
    for( const auto *selection : { &result.items_you, &result.items_trader } ) {
        for( const auto &entry : *selection ) {
            if( !entry.first || entry.second <= 0 ||
                ( entry.first->count_by_charges() ? entry.second > entry.first->charges : entry.second != 1 ) ) {
                return false;
            }
            const item *source = entry.first.get_item();
            if( std::find( sources.begin(), sources.end(), source ) != sources.end() ) {
                return false;
            }
            for( const item *prior : sources ) {
                const auto prior_contents = prior->all_items_ptr();
                const auto contents = source->all_items_ptr();
                if( std::find( prior_contents.begin(), prior_contents.end(), source ) != prior_contents.end() ||
                    std::find( contents.begin(), contents.end(), prior ) != contents.end() ) {
                    return false;
                }
            }
            sources.push_back( source );
        }
    }

    const auto payer_sources = encounter_trade_items( payer, np, nearby_item_radius,
                               nearby_ally_radius, nearby_z_radius );
    const auto trader_sources = encounter_trade_items( np, payer, np.is_player_ally() ? -1 : 1, 0, 0, false );
    const auto eligible = []( const trade_selector::select_t &selected,
    const std::vector<item_location> &current ) {
        return std::all_of( selected.begin(), selected.end(), [&]( const auto &entry ) {
            return std::find( current.begin(), current.end(), entry.first ) != current.end();
        } );
    };
    if( !eligible( result.items_you, payer_sources ) || !eligible( result.items_trader, trader_sources ) ) {
        return false;
    }
    // Capture observable source/actor facts while every original location and
    // owner still exists. No accusation or relationship change occurs yet.
    const auto witnesses = observe_property_transfer( np, payer, result.items_you, site );
    if( failure ) {
        *failure = stash_trade_failure::storage;
    }

    const tripoint_abs_ms origin = project_to<coords::ms>( home );
    const tripoint_abs_ms far_corner = origin + tripoint( SEEX * 2 - 1, SEEY * 2 - 1, 0 );
    std::unique_ptr<tinymap> remote;
    map *destination = &get_map();
    if( !destination->inbounds( origin ) || !destination->inbounds( far_corner ) ) {
        remote = std::make_unique<tinymap>();
        remote->load( home, false );
        destination = remote->cast_to_map();
    }
    map &stash = *destination;
    struct stash_tile {
        tripoint_bub_ms position;
        units::volume free_volume;
        std::size_t slots;
    };
    std::vector<stash_tile> tiles;
    const tripoint_abs_ms centre = origin + tripoint( SEEX, SEEY, 0 );
    for( const tripoint_abs_ms &absolute : closest_points_first( centre, std::max( SEEX, SEEY ) ) ) {
        if( project_to<coords::omt>( absolute ) != home ) {
            continue;
        }
        const tripoint_bub_ms tile = stash.get_bub( absolute );
        if( stash.inbounds( tile ) && stash.passable( tile ) && stash.can_put_items_ter_furn( tile ) &&
            !stash.has_flag( ter_furn_flag::TFLAG_DESTROY_ITEM, tile ) &&
            !stash.has_flag( ter_furn_flag::TFLAG_SWIMMABLE, tile ) ) {
            const auto stack = stash.i_at( tile );
            tiles.push_back( { tile, stack.free_volume(), stack.size() < MAX_ITEM_IN_SQUARE ? MAX_ITEM_IN_SQUARE - stack.size() : 0 } );
        }
    }
    struct deposit {
        tripoint_bub_ms position;
        item goods;
    };
    std::vector<deposit> deposits;
    for( const auto &entry : result.items_you ) {
        item goods = *entry.first;
        if( goods.count_by_charges() ) {
            goods.charges = entry.second;
        }
        if( site ) {
            std::ostringstream stream;
            JsonOut json( stream );
            json.start_object();
            json.member( "site_id", site->site_id );
            json.member( "operation_id", site->active_hostile_operation.reservation.activity_id );
            json.member( "generation", site->active_hostile_operation.reservation.generation );
            json.member( "collector_id", np.getID() );
            json.member( "receiver_id", payer.getID() );
            json.member( "collector_faction", np.get_fac_id() );
            json.member( "responsible_members", site->active_hostile_operation.reservation.member_ids );
            json.member( "when", calendar::turn );
            json.member( "source_position", get_map().get_abs( entry.first.pos_bub( get_map() ) ) );
            json.member( "quantity", entry.second );
            json.member( "property" ); json.start_array();
            entry.first->visit_items( [&]( const item *part, const item * ) {
                npc_taken_property source{ part->uid().get_value(), part->typeId(),
                    part == entry.first.get_item() && part->count_by_charges() ? entry.second :
                    part->count_by_charges() ? part->charges : 1,
                    part->get_owner(), get_map().get_abs( entry.first.pos_bub( get_map() ) ) };
                source.serialize( json ); return VisitResponse::NEXT;
            } );
            json.end_array(); json.end_object();
            goods.set_var( "shakedown_source", stream.str() );
        }
        goods.set_owner( np );
        const auto available = std::find_if( tiles.begin(), tiles.end(), [&]( const stash_tile & tile ) {
            return tile.slots > 0 && tile.free_volume >= goods.volume();
        } );
        if( available == tiles.end() ) {
            return false;
        }
        available->free_volume -= goods.volume();
        --available->slots;
        deposits.push_back( { available->position, std::move( goods ) } );
    }
    // Storage insertion intentionally does not invoke drop actions or merge
    // charges. Every new object has a reversible location until the full basket
    // is placed; failed insertion removes only these objects, never old stock.
    std::vector<std::pair<tripoint_bub_ms, item *>> placed;
    for( deposit &entry : deposits ) {
        item &added = stash.add_item( entry.position, std::move( entry.goods ) );
        if( added.is_null() ) {
            for( const auto &prior : placed ) {
                stash.i_rem( prior.first, prior.second );
            }
            return false;
        }
        placed.emplace_back( entry.position, &added );
    }
    // Native avatar theft recovery held a source pointer. Retire that exact
    // recovery intent before removing its object; durable witnessed facts above
    // retain attribution without redirecting NPC theft toward the avatar.
    for( npc &observer : g->all_npcs() ) {
        if( !observer.known_stolen_item ) { continue; }
        for( const auto &entry : result.items_you ) {
            if( entry.first->count_by_charges() && entry.second < entry.first->charges ) { continue; }
            const auto contained = entry.first->all_items_ptr();
            if( observer.known_stolen_item == entry.first.get_item() ||
                std::find( contained.begin(), contained.end(), observer.known_stolen_item ) != contained.end() ) {
                observer.known_stolen_item = nullptr;
                if( observer.get_attitude() == NPCATT_RECOVER_GOODS ) { observer.set_attitude( NPCATT_NULL ); }
                break;
            }
        }
    }
    for( auto &entry : result.items_you ) {
        if( entry.first->count_by_charges() && entry.second < entry.first->charges ) {
            entry.first->charges -= entry.second;
            entry.first.on_contents_changed();
        } else {
            entry.first.remove_item();
        }
    }
    result.items_you.clear(); // No collector inventory copy in complete_trade.
    // Keep the ordinary other-side transfer, bank/debt and speech bookkeeping.
    const bool completed = complete_trade( np, payer, result );
    stash.save();
    if( completed ) {
        for( const auto &observed : witnesses ) {
            npc *observer = g->find_npc( observed.witness );
            if( observer && observer->learn_property_theft( observed.fact ) ) {
                observer->announce_property_theft( observed.fact.incident_key );
            }
        }
    }
    DebugLog( D_INFO, DC_ALL ) << "shakedown_stash deposited=" << placed.size()
                               << " home=" << home.to_string()
                               << " first_tile=" << ( placed.empty() ? "none" :
                                       stash.get_abs( placed.front().first ).to_string() );
    if( failure ) {
        *failure = stash_trade_failure::none;
    }
    return completed;
}

bool npc_trading::complete_trade( npc &np, Character &player_character,
                                trade_ui::trade_result_t &trade_result )
{
    const bool traded = trade_result.traded;

    if( trade_result.traded ) {
        std::list<item_location *> from_map;

        std::list<item> escrow;
        // Movement of items in 3 steps: player to escrow - npc to player - escrow to npc.
        escrow = npc_trading::transfer_items( trade_result.items_you, player_character, np, from_map,
                                              true );
        npc_trading::transfer_items( trade_result.items_trader, np, player_character, from_map, false );
        // Now move items from escrow to the npc. Keep the weapon wielded.
        if( np.is_shopkeeper() ) {
            distribute_items_to_npc_zones( escrow, np );
        } else {
            for( const item &i : escrow ) {
                np.i_add( i, true, nullptr, nullptr, true, false );
            }
        }

        for( item_location *loc_ptr : from_map ) {
            if( !loc_ptr ) {
                continue;
            }
            item *it = loc_ptr->get_item();
            if( !it ) {
                continue;
            }
            if( it->has_var( "trade_charges" ) && it->count_by_charges() ) {
                it->charges -= static_cast<int>( it->get_var( "trade_charges", 0 ) );
                if( it->charges <= 0 ) {
                    loc_ptr->remove_item();
                } else {
                    it->erase_var( "trade_charges" );
                }
            } else if( it->has_var( "trade_amount" ) ) {
                loc_ptr->remove_item();
            }
        }

        // NPCs will remember debts, to the limit that they'll extend credit or previous debts
        if( !np.will_exchange_items_freely() ) {
            player_character.cash -= trade_result.delta_bank;
            update_npc_owed( np, trade_result.balance, trade_result.value_you );
            player_character.practice( skill_speech, trade_result.value_you / 10000 );
        }
    }
    trade_result.traded = false;
    return traded;
}

// Will the NPC accept the trade that's currently on offer?
bool npc_trading::npc_will_accept_trade( npc const &np, int your_balance )
{
    return np.will_exchange_items_freely() || your_balance + np.max_credit_extended() >= 0;
}
bool npc_trading::npc_can_fit_items( npc const &np, trade_selector::select_t const &to_trade )
{
    std::vector<item> avail_pockets = np.worn.available_pockets();

    if( !to_trade.empty() && avail_pockets.empty() ) {
        return false;
    }
    for( trade_selector::entry_t const &it : to_trade ) {
        bool item_stored = false;
        for( item &pkt : avail_pockets ) {
            const units::volume pvol = pkt.max_containable_volume();
            const item &i = *it.first;
            if( pkt.can_holster( i ) || ( pkt.can_contain( i ).success() && pvol > i.volume() ) ) {
                pkt.put_in( i, pocket_type::CONTAINER );
                item_stored = true;
                break;
            }
        }
        if( !item_stored ) {
            return false;
        }
    }
    return true;
}

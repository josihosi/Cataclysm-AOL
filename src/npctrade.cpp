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

bool npc_trading::trade_to_stash( npc &np, Character &payer, const tripoint_abs_omt &home,
                                  const int cost, const std::string &deal,
                                  const int nearby_item_radius, const int nearby_ally_radius,
                                  basecamp *payer_basecamp, const bool encounter_only )
{
    np.shop_restock();
    np.drop_invalid_inventory();
    ui_adaptor modal( ui_adaptor::disable_uis_below{} );
    trade_ui tradeui( payer, np, cost, deal, nearby_item_radius, nearby_ally_radius,
                      payer_basecamp, encounter_only, true );
    while( true ) {
        trade_ui::trade_result_t result = tradeui.perform_trade();
        if( !result.traded ) {
            return false;
        }
        if( complete_trade_to_stash( np, payer, result, home ) ) {
            return true;
        }
        popup( _( "The bandit home camp cannot store this payment.  No goods changed hands.  Change the offer or cancel." ) );
        // perform_trade resets only the exit/confirmation flags. It preserves
        // the offer and publishes a fresh authenticated native selector.
    }
}

bool npc_trading::complete_trade_to_stash( npc &np, Character &payer,
        trade_ui::trade_result_t &result, const tripoint_abs_omt &home )
{
    if( !result.traded ) {
        return false;
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
    DebugLog( D_INFO, DC_ALL ) << "shakedown_stash deposited=" << placed.size()
                               << " home=" << home.to_string()
                               << " first_tile=" << ( placed.empty() ? "none" :
                                       stash.get_abs( placed.front().first ).to_string() );
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

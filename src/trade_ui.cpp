
#include "trade_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <set>
#include <unordered_set>

#include "avatar.h"
#include "basecamp.h"
#include "character.h"
#include "clzones.h"
#include "color.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_category.h"
#include "map.h"
#include "npc.h"
#include "npc_opinion.h"
#include "npctrade.h"
#include "npctrade_utils.h"
#include "options.h"
#include "output.h"
#include "pathfinding.h"
#include "point.h"
#include "ret_val.h"
#include "string_formatter.h"
#include "trap.h"
#include "type_id.h"
#include "vehicle.h"

static const faction_id faction_your_followers( "your_followers" );

static const flag_id json_flag_NO_UNWIELD( "NO_UNWIELD" );

static const item_category_id item_category_ITEMS_WORN( "ITEMS_WORN" );
static const item_category_id item_category_WEAPON_HELD( "WEAPON_HELD" );

static const trait_id trait_TRADE_BACKEND( "TRADE_BACKEND" );

namespace
{
point _pane_orig( int side )
{
    return { side > 0 ? TERMX / 2 : 0, trade_ui::header_size };
}

point _pane_size()
{
    return { TERMX / 2, TERMY - _pane_orig( 0 ).y };
}

} // namespace

trade_preset::trade_preset( Character const &you, Character const &trader )
    : _u( you ), _trader( trader )
{
    save_state = &inventory_ui_default_state;
    append_cell(
    [&]( item_location const & loc ) {
        return format_money( npc_trading::trading_price( _trader, _u, { loc, 1 } ) );
    },
    _( "Unit price" ) );
}

bool trade_preset::is_shown( item_location const &loc ) const
{
    return !loc->has_var( VAR_TRADE_IGNORE ) && inventory_selector_preset::is_shown( loc ) &&
           loc->is_owned_by( _u ) && loc->made_of( phase_id::SOLID ) && !loc->is_frozen_liquid() &&
           ( !_u.is_wielding( *loc ) || !loc->has_flag( json_flag_NO_UNWIELD ) );
}

std::string trade_preset::get_denial( const item_location &loc ) const
{
    int const price = npc_trading::trading_price( _trader, _u, { loc, 1 } );

    if( _u.is_npc() ) {
        npc const &np = *_u.as_npc();
        ret_val<void> const ret = np.wants_to_sell( loc, price );
        if( !ret.success() ) {
            std::string refused_text = ret.str();
            if( refused_text.empty() ) {
                return string_format( _( "%s does not want to sell this" ), np.get_name() );
            }
            parse_tags( refused_text, _trader, _u );
            return refused_text;
        }
    } else if( _trader.is_npc() ) {
        npc const &np = *_trader.as_npc();
        ret_val<void> const ret = np.wants_to_buy( *loc, price );
        if( !ret.success() ) {
            if( ret.str().empty() ) {
                return string_format( _( "%s does not want to buy this" ), np.get_name() );
            }
            return np.replace_with_npc_name( ret.str() );
        }
    }

    if( _u.is_worn( *loc ) ) {
        ret_val<void> const ret = const_cast<Character &>( _u ).can_takeoff( *loc );
        if( !ret.success() ) {
            return _u.replace_with_npc_name( ret.str() );
        }
    }

    return inventory_selector_preset::get_denial( loc );
}

bool trade_preset::cat_sort_compare( const inventory_entry &lhs, const inventory_entry &rhs ) const
{
    // sort worn and held categories last we likely don't want to trade them
    auto const fudge_rank = []( inventory_entry const & e ) -> int {
        item_category_id const cat = e.get_category_ptr()->get_id();
        int const rank = e.get_category_ptr()->sort_rank();
        return cat != item_category_ITEMS_WORN && cat != item_category_WEAPON_HELD ? rank : rank + 10000;
    };
    return fudge_rank( lhs ) < fudge_rank( rhs );
}

namespace
{
// Use native route ownership, not a visibility ray through a floor. The route
// stays inside this encounter and may open usable doors, but may not bash,
// climb a wall, cross an unsafe tile or load a remote store.
bool encounter_goods_reachable( Character &payer, const tripoint_bub_ms &target,
                                int radius, int z_radius )
{
    map &here = get_map();
    const auto origin = payer.pos_bub();
    const auto in_area = [&]( const tripoint_bub_ms &p ) {
        return here.inbounds( p ) && ( here.supports_zlevels() || p.z() == origin.z() ) &&
               here.ter( p ) != ter_str_id::NULL_ID() &&
               rl_dist( origin.xy(), p.xy() ) <= radius && std::abs( p.z() - origin.z() ) <= z_radius;
    };
    if( !in_area( origin ) || !in_area( target ) ) {
        return false;
    }
    const auto native_avoid = payer.get_path_avoid();
    const auto avoid = [&]( const tripoint_bub_ms &p ) {
        const trap &candidate = here.tr_at( p );
        return !in_area( p ) || native_avoid( p ) ||
               payer.is_dangerous_fields( here.field_at( p ) ) ||
               ( candidate.can_see( p, payer ) && !candidate.is_benign() );
    };
    // Ordinary item interaction may reach from an adjacent tile on its level.
    const auto can_access = [&]( const tripoint_bub_ms &p ) {
        return p.z() == target.z() && rl_dist( p, target ) <= 1 &&
               ( p == target || here.clear_path( p, target, 1, 1, 100 ) );
    };
    if( can_access( origin ) ) {
        return true;
    }
    pathfinding_settings settings = payer.get_pathfinding_settings();
    settings.bash_strength.clear();
    settings.climb_cost = 0;
    settings.avoid_traps = true;
    settings.avoid_dangerous_fields = true;
    // Native radius targets include adjacent z-levels. Use native point targets
    // on the goods' level so stopping underneath a floor is not access.
    for( const tripoint_bub_ms &interaction : closest_points_first( target, 1 ) ) {
        if( !in_area( interaction ) || avoid( interaction ) || !can_access( interaction ) ) {
            continue;
        }
        const auto route = here.route( origin, pathfinding_target::point( interaction ), settings, avoid );
        if( !route.empty() && std::none_of( route.begin(), route.end(), avoid ) ) {
            return true;
        }
    }
    return false;
}

class encounter_item_selector : public inventory_selector
{
    public:
        using inventory_selector::inventory_selector;
        std::vector<item_location> eligible_items() const {
            std::vector<item_location> result;
            std::set<const item *> seen;
            for( const auto *column : get_all_columns() ) {
                for( const auto *entry : column->get_entries( []( const inventory_entry &e ) {
                    return e.is_item() && !e.is_collation_header();
                }, true ) ) {
                    for( const item_location &loc : entry->locations ) {
                        if( loc && preset.is_shown( loc ) && preset.get_denial( loc ).empty() &&
                            seen.insert( loc.get_item() ).second ) {
                            result.push_back( loc );
                        }
                    }
                }
            }
            return result;
        }
};
} // namespace

void add_encounter_trade_sources( inventory_selector &selector, Character &payer,
                                  const Character &buyer, int item_radius, int ally_radius, int z_radius )
{
    map &here = get_map();
    if( payer.is_dead_state() || buyer.is_dead_state() || !here.inbounds( payer.pos_bub() ) ||
        ( payer.is_npc() && !payer.as_npc()->is_active() ) ) {
        return;
    }
    selector.add_character_items( payer );
    if( item_radius >= 0 ) {
        for( const auto &tile : here.points_in_radius( payer.pos_bub(), item_radius, z_radius ) ) {
            if( !here.inbounds( tile ) ||
                ( !here.supports_zlevels() && tile.z() != payer.pos_bub().z() ) ) {
                continue;
            }
            const auto cargo = here.veh_at( tile ).cargo();
            // Run a route only for an actual source, rather than every empty cell.
            if( here.i_at( tile ).empty() && ( !cargo || cargo->items().empty() ) ) {
                continue;
            }
            if( encounter_goods_reachable( payer, tile, item_radius, z_radius ) ) {
                selector.add_map_items( tile );
                selector.add_vehicle_items( tile );
            }
        }
    }
    if( ally_radius > 0 ) {
        for( npc &ally : g->all_npcs() ) {
            if( &ally != &payer && &ally != &buyer && ally.is_player_ally() &&
                ally.is_active() && !ally.is_dead() &&
                encounter_goods_reachable( payer, ally.pos_bub(), ally_radius, z_radius ) ) {
                selector.add_character_items( ally );
            }
        }
    }
}

std::vector<item_location> encounter_trade_items( Character &payer, const Character &buyer,
        int item_radius, int ally_radius, int z_radius )
{
    const trade_preset preset( payer, buyer );
    encounter_item_selector selector( payer, preset );
    add_encounter_trade_sources( selector, payer, buyer, item_radius, ally_radius, z_radius );
    return selector.eligible_items();
}

trade_ui::trade_ui( party_t &you, npc &trader, currency_t cost, std::string title,
                    const int you_nearby_item_radius, const int you_nearby_ally_radius,
                    basecamp *you_basecamp, const bool encounter_only, const bool goods_to_stash, const int nearby_z_radius )
    : _upreset{ you, trader }, _tpreset{ trader, you },
      _panes{ std::make_unique<pane_t>( this, trader, _tpreset, std::string(), _pane_size(),
                                        _pane_orig( -1 ) ),
              std::make_unique<pane_t>( this, you, _upreset, std::string(), _pane_size(),
                                        _pane_orig( 1 ) ) },
      _parties{ &trader, &you }, _requested_cost( cost ), _goods_to_stash( goods_to_stash ),
      _title( std::move( title ) )

{
    if( encounter_only ) {
        add_encounter_trade_sources( *_panes[_you], you, trader, you_nearby_item_radius,
                                     you_nearby_ally_radius, nearby_z_radius );
    } else {
        _panes[_you]->add_character_items( you );
        _panes[_you]->add_nearby_items( you_nearby_item_radius );
    }
    if( you_basecamp != nullptr && !encounter_only ) {
        _panes[_you]->add_basecamp_items( *you_basecamp, you_nearby_item_radius );
        std::set<character_id> added_basecamp_workers;
        const auto add_basecamp_worker_items = [&]( npc &assigned ) {
            if( &assigned == &trader || assigned.is_dead() || !assigned.is_player_ally() ) {
                return;
            }
            if( you_nearby_ally_radius >= 0 &&
                rl_dist( assigned.pos_abs(), you.pos_abs() ) <= you_nearby_ally_radius ) {
                return;
            }
            if( added_basecamp_workers.insert( assigned.getID() ).second ) {
                _panes[_you]->add_character_items( assigned );
            }
        };
        for( const npc_ptr &assigned : you_basecamp->get_npcs_assigned() ) {
            if( assigned == nullptr ) {
                continue;
            }
            add_basecamp_worker_items( *assigned );
        }
        for( npc &assigned : g->all_npcs() ) {
            const bool assigned_to_this_camp = assigned.assigned_camp &&
                                               *assigned.assigned_camp == you_basecamp->camp_omt_pos();
            const bool in_basecamp_side_pool = rl_dist( assigned.pos_abs(), you.pos_abs() ) <= 60;
            if( !assigned_to_this_camp && !in_basecamp_side_pool ) {
                continue;
            }
            add_basecamp_worker_items( assigned );
        }
    }
    if( you_nearby_ally_radius > 0 && !encounter_only ) {
        for( npc &guy : g->all_npcs() ) {
            if( &guy == &trader || &guy == &you || !guy.is_player_ally() ||
                rl_dist( guy.pos_abs(), you.pos_abs() ) > you_nearby_ally_radius ) {
                continue;
            }
            _panes[_you]->add_character_items( guy );
        }
    }
    if( !trader.has_trait( trait_TRADE_BACKEND ) ) {
        _panes[_trader]->add_character_items( trader );
    }
    if( trader.is_shopkeeper() ) {
        _panes[_trader]->categorize_map_items( true );

        add_fallback_zone( trader );

        zone_manager &zmgr = zone_manager::get_manager();

        std::unordered_set<tripoint_bub_ms> const src =
            zmgr.get_point_set_loot( trader.pos_abs(), PICKUP_RANGE, trader.get_fac_id() );

        for( tripoint_bub_ms const &pt : src ) {
            _panes[_trader]->add_map_items( pt );
            _panes[_trader]->add_vehicle_items( pt );
        }
    } else if( !trader.is_player_ally() ) {
        _panes[_trader]->add_nearby_items( 1 );
    }

    const map &here = get_map();

    for( const wrapped_vehicle &wv : get_map().get_vehicles() ) {
        if( !encounter_only && wv.v->owner == faction_your_followers ) {
            for( const tripoint_abs_ms &veh_pt : wv.v->get_points() ) {
                _panes[_you]->add_vehicle_items( here.get_bub( veh_pt ) );
            }
        }
    }

    if( trader.will_exchange_items_freely() ) {
        _cost = 0;
    } else {
        _cost = trader.op_of_u.owed - cost;
    }
    _balance = _cost;
    _bank = you.cash;
    _delta_bank = 0;
    _panes[_you]->get_active_column().on_deactivate();

    _header_ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        _header_w = catacurses::newwin( header_size, TERMX, point::zero );
        ui.position_from_window( _header_w );
        ui.invalidate_ui();
        resize();
    } );
    _header_ui.mark_resize();
    _header_ui.on_redraw( [this]( ui_adaptor const & /* ui */ ) {
        werase( _header_w );
        _draw_header();
        wnoutrefresh( _header_w );
    } );
}

void trade_ui::pushevent( event const &ev )
{
    _queue.emplace( ev );
}

trade_ui::trade_result_t trade_ui::perform_trade()
{
    _exit = false;
    _traded = false;

    while( !_exit ) {
        _panes[_cpane]->execute();

        while( !_queue.empty() ) {
            event const ev = _queue.front();
            _queue.pop();
            _process( ev );
        }
    }

    if( _traded ) {
        return { _traded,
                 _balance,
                 _delta_bank,
                 _trade_values[_you],
                 _trade_values[_trader],
                 _panes[_you]->to_trade(),
                 _panes[_trader]->to_trade() };
    }

    return { false, 0, 0, 0, 0, {}, {} };
}

std::map<std::string, std::string> trade_ui::semantic_payload() const
{
    const npc &trader = *_parties[_trader]->as_npc();
    const Character &active = *_parties[_cpane];
    const auto actor_id = []( const Character &actor ) {
        return "character:" + std::to_string( actor.getID().get_value() );
    };
    return {
        { "title", string_format( _( "Trade: %s's items" ), active.get_name() ) },
        { "deal", _title },
        { "payment_destination", _goods_to_stash ? "bandit_home_stash" : "trader" },
        { "active_party", _cpane == _you ? "player" : "npc" },
        { "active_actor_id", actor_id( active ) },
        { "player_actor_id", actor_id( *_parties[_you] ) },
        { "player_name", _parties[_you]->get_name() },
        { "trader_actor_id", actor_id( trader ) },
        { "trader_name", trader.get_name() },
        { "exchange_items_freely", trader.will_exchange_items_freely() ? "true" : "false" },
        { "balance", format_money( _balance ) },
        { "balance_label", _balance >= 0 ? _( "Credit" ) : _( "Debt" ) },
        { "balance_amount", format_money( std::abs( _balance ) ) },
        { "demand_amount", format_money( _requested_cost ) },
        { "opening_balance", format_money( _cost ) },
        { "max_credit", format_money( trader.max_credit_extended() ) },
        { "player_offer_value", format_money( _trade_values[_you] ) },
        { "trader_offer_value", format_money( _trade_values[_trader] ) },
        { "trade_values_source", "trade_ui native parties, offers and balance" }
    };
}

trade_ui::currency_t trade_ui::active_offer_value( const entry_t &entry ) const
{
    return npc_trading::trading_price( *_parties[-_cpane + 1], *_parties[_cpane], entry );
}

void trade_ui::recalc_values_cpane()
{
    _trade_values[_cpane] = 0;

    for( entry_t const &it : _panes[_cpane]->to_trade() ) {
        // FIXME: cache trading_price
        _trade_values[_cpane] += active_offer_value( it );
    }
    if( !_parties[_trader]->as_npc()->will_exchange_items_freely() ) {
        _balance = _cost + _trade_values[_you] - _trade_values[_trader] + _delta_bank;
    }
    _header_ui.invalidate_ui();
}

bool trade_ui::can_autobalance() const
{
    const int sign = _cpane == _you ? -1 : 1;
    const inventory_entry &entry = _panes[_cpane]->get_active_column().get_highlighted();
    return ( ( sign < 0 && _balance < 0 ) || ( sign > 0 && _balance > 0 ) ) &&
           entry.is_selectable() && entry.chosen_count < entry.get_available_count();
}

void trade_ui::autobalance()
{
    int const sign = _cpane == _you ? -1 : 1;
    if( ( sign < 0 && _balance < 0 ) || ( sign > 0 && _balance > 0 ) ) {
        inventory_entry &entry = _panes[_cpane]->get_active_column().get_highlighted();
        if( !entry.is_selectable() ) {
            popup( _( "%s cannot be traded." ), entry.any_item()->tname() );
            return;
        }
        size_t const avail = entry.get_available_count() - entry.chosen_count;
        double const price = npc_trading::trading_price( *_parties[-_cpane + 1], *_parties[_cpane],
                             entry_t{ entry.any_item(), 1 } ) * sign;
        double const num = _balance / price;
        double const extra = sign < 0 ? std::ceil( num ) : std::floor( num );
        _panes[_cpane]->toggle_entry( entry, entry.chosen_count +
                                      std::min( static_cast<size_t>( extra ), avail ) );
    }
}

void trade_ui::bank_balance()
{
    if( !get_option<bool>( "CAPITALISM" ) ) {
        popup( _( "Your promises of digital payment mean nothing here." ) );
        return;
    }
    _bank += _delta_bank;
    _delta_bank = -( _balance - _delta_bank );
    if( _delta_bank > 0 ) { // a withdrawal
        _delta_bank = std::min( _delta_bank, _bank );
        _delta_bank = std::max( _delta_bank, 0 );
    }
    _bank -= _delta_bank;
    recalc_values_cpane();
}

void trade_ui::resize()
{
    _panes[_you]->resize( _pane_size(), _pane_orig( 1 ) );
    _panes[_trader]->resize( _pane_size(), _pane_orig( -1 ) );
}

void trade_ui::_process( event const &ev )
{
    switch( ev ) {
        case event::TRADECANCEL: {
            _traded = false;
            _exit = true;
            break;
        }
        case event::TRADEOK: {
            _traded = _confirm_trade();
            _exit = _traded;
            break;
        }
        case event::SWITCH: {
            _panes[_cpane]->get_ui()->invalidate_ui();
            _cpane = -_cpane + 1;
            break;
        }
        case event::NEVENTS: {
            break;
        }
    }
}

bool trade_ui::_confirm_trade() const
{
    npc const &np = *_parties[_trader]->as_npc();

    if( !npc_trading::npc_will_accept_trade( np, _balance ) ) {
        if( np.max_credit_extended() == 0 ) {
            popup( _( "You'll need to offer me more than that." ) );
        } else {
            popup( _( "Sorry, I'm only willing to extend you %s in credit." ),
                   format_money( np.max_credit_extended() ) );
        }
    } else if( !_goods_to_stash && !np.is_shopkeeper() &&
               !npc_trading::npc_can_fit_items( np, _panes[_you]->to_trade() ) ) {
        popup( _( "%s doesn't have the appropriate pockets to accept that." ), np.get_name() );
    } else if( npc_trading::calc_npc_owes_you( np, _balance ) < _balance ) {
        // NPC is happy with the trade, but isn't willing to remember the whole debt.
        return query_yn(
                   _( "I'm never going to be able to pay you back for all that.  The most I'm "
                      "willing to owe you is %s.\n\nContinue with trade?" ),
                   format_money( np.max_willing_to_owe() ) );

    } else {
        return query_yn( _( "Looks like a deal!  Accept this trade?" ) );
    }

    return false;
}

void trade_ui::_draw_header()
{
    draw_border( _header_w, c_light_gray );
    center_print( _header_w, 1, c_white, _title );
    npc const &np = *_parties[_trader]->as_npc();
    nc_color const trade_color =
        npc_trading::npc_will_accept_trade( np, _balance ) ? c_green : c_red;
    std::string cost_str = _( "Exchange" );
    if( !np.will_exchange_items_freely() ) {
        cost_str = string_format( _balance >= 0 ? _( "Credit %s" ) : _( "Debt %s" ),
                                  format_money( std::abs( _balance ) ) );
    }
    center_print( _header_w, 2, trade_color, cost_str );
    const std::string &rname = get_avatar().dialogue_remote_name;
    mvwprintz( _header_w, { 1, 3 }, c_white, rname.empty() ? _parties[_trader]->get_name() : rname );
    right_print( _header_w, 3, 1, c_white, _( "You" ) );
    center_print( _header_w, header_size - 1, c_white,
                  string_format( _( "%s to switch panes" ),
                                 colorize( _panes[_you]->get_ctxt()->get_desc(
                                         trade_selector::ACTION_SWITCH_PANES ),
                                           c_yellow ) ) );
    center_print( _header_w, header_size - 2, c_white,
                  string_format( _( "%s to auto balance with highlighted item" ),
                                 colorize( _panes[_you]->get_ctxt()->get_desc(
                                         trade_selector::ACTION_AUTOBALANCE ),
                                           c_yellow ) ) );
    if( get_option<bool>( "CAPITALISM" ) ) {
        right_print( _header_w, 2, 1, c_white, string_format( _( "Bank Balance: %s" ),
                     format_money( _bank ) ) );
        right_print( _header_w, 1, 1, c_white,
                     string_format( _( "%s to balance trade with bank account balance" ),
                                    colorize( _panes[_you]->get_ctxt()->get_desc(
                                            trade_selector::ACTION_BANKBALANCE ),
                                              c_yellow ) ) );
    }

}

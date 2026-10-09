#pragma once
#ifndef CATA_SRC_NPCTRADE_H
#define CATA_SRC_NPCTRADE_H

#include <list>

#include "coordinates.h"
#include <string>
#include <utility>
#include <vector>

#include "item_location.h"
#include "trade_ui.h"
#include "units.h"

constexpr char const *VAR_TRADE_IGNORE = "trade_ignore";

class Character;
class basecamp;
class item;
class npc;

class item_pricing
{
    public:
        item_pricing( Character &c, item &it, double v, int count ) : loc( c, &it ), price( v ) {
            set_values( count );
        }

        item_pricing( item_location &&l, double v, int count ) : loc( std::move( l ) ), price( v ) {
            set_values( count );
        }
        void set_values( int ip_count );

        item_location loc;
        double price = 0;
        // Whether this is selected for trading
        bool selected = false;
        bool is_container = false;
        int count = 0;
        int charges = 0;
        int u_has = 0;
        int npc_has = 0;
        int u_charges = 0;
        int npc_charges = 0;
        units::mass weight = 0_gram;
        units::volume vol = 0_ml;
};
namespace npc_trading
{
bool pay_npc( npc &np, int cost );

int bionic_install_price( Character &installer, Character &patient, item_location const &bionic );
int adjusted_price( item const *it, int amount, Character const &buyer, Character const &seller );
int trading_price( Character const &buyer, Character const &seller,
                   trade_selector::entry_t const &it );
int calc_npc_owes_you( const npc &np, int your_balance );
bool npc_will_accept_trade( npc const &np, int your_balance );
bool npc_can_fit_items( npc const &np, trade_selector::select_t const &to_trade );
void update_npc_owed( npc &np, int your_balance, int your_sale_value );
int cash_to_favor( const npc &, int cash );

std::list<item> transfer_items( trade_selector::select_t &stuff, Character &giver,
                                Character &receiver, std::list<item_location *> &from_map,
                                bool use_escrow );
double net_price_adjustment( const Character &buyer, const Character &seller );
bool trade( npc &p, int cost, const std::string &deal, int you_nearby_item_radius = 1,
            int you_nearby_ally_radius = 0, basecamp *you_basecamp = nullptr,
            Character *payer = nullptr, bool encounter_only = false );
// Applies the selection made by the existing trade UI. The physical payer
// can be a contacted camp member while the avatar controls the response.
bool complete_trade( npc &trader, Character &payer, trade_ui::trade_result_t &result );
// Transient native transaction binding; current encounter still owns authority.
std::string encounter_payment_identity( const npc &trader, const Character &payer );
// Shakedown payments use the same selection/prices but deposit actual items at
// the validated home camp instead of testing or filling the collector's pockets.
bool trade_to_stash( npc &trader, Character &payer, const tripoint_abs_omt &home,
                     int cost, const std::string &deal, int nearby_item_radius,
                     int nearby_ally_radius, basecamp *payer_basecamp, bool encounter_only, int nearby_z_radius = 0 );
enum class stash_trade_failure { none, source, storage };

// Placement failure leaves both the accepted selection and its sources intact.
// A successful application consumes the result, just like complete_trade.
bool complete_trade_to_stash( npc &trader, Character &payer,
                              trade_ui::trade_result_t &result, const tripoint_abs_omt &home,
                              int nearby_item_radius = 1, int nearby_ally_radius = 0,
                              int nearby_z_radius = 0, stash_trade_failure *failure = nullptr );
std::vector<item_pricing> init_selling( npc &p );
} // namespace npc_trading

#endif // CATA_SRC_NPCTRADE_H

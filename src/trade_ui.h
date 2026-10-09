#pragma once
#ifndef CATA_SRC_TRADE_UI_H
#define CATA_SRC_TRADE_UI_H

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "character.h"
#include "color.h"
#include "coordinates.h"
#include "creature.h"
#include "cursesdef.h"
#include "input_context.h"
#include "inventory_ui.h"
#include "item_location.h"
#include "memory_fast.h"
#include "point.h"
#include "translations.h"
#include "ui_manager.h"
#include "units.h"

class npc;
class basecamp;
class trade_ui;

class trade_selector : public inventory_drop_selector
{
    public:
        explicit trade_selector( trade_ui *parent, Character &u,
                                 inventory_selector_preset const &preset = default_preset,
                                 std::string const &selection_column_title = _( "TRADE OFFER" ),
                                 // NOLINTNEXTLINE(cata-use-named-point-constants)
                                 point const &size = { -1, -1 }, point const &origin = { -1, -1 } );
        using entry_t = std::pair<item_location, int>;
        using select_t = std::vector<entry_t>;
        void execute();
        void on_toggle() override;
        select_t to_trade() const;
        std::map<std::string, std::string> semantic_payload() const;
        void resize( point const &size, point const &origin );
        shared_ptr_fast<ui_adaptor> get_ui() const;
        input_context const *get_ctxt() const;

        static constexpr char const *ACTION_AUTOBALANCE = "AUTO_BALANCE";
        static constexpr char const *ACTION_BANKBALANCE = "BANK_BALANCE";
        static constexpr char const *ACTION_SWITCH_PANES = "SWITCH_LISTS";
        static constexpr char const *ACTION_TRADE_OK = "CONFIRM";
        static constexpr char const *ACTION_TRADE_CANCEL = "QUIT";

    private:
        std::vector<std::pair<inventory_column *, inventory_entry *>> semantic_entries() const;
        trade_ui *_parent;
        shared_ptr_fast<ui_adaptor> _ui;
        input_context _ctxt_trade;
};

class trade_preset : public inventory_selector_preset
{
    public:
        explicit trade_preset( Character const &you, Character const &trader );

        bool is_shown( item_location const &loc ) const override;
        std::string get_denial( const item_location &loc ) const override;
        bool cat_sort_compare( const inventory_entry &lhs, const inventory_entry &rhs ) const override;

    private:
        Character const &_u, &_trader;
};

// Encounter-only source population is shared with demand and commit.
void add_encounter_trade_sources( inventory_selector &selector, Character &payer,
                                  const Character &buyer, int item_radius, int ally_radius, int z_radius );
std::vector<item_location> encounter_trade_items( Character &payer, const Character &buyer,
        int item_radius, int ally_radius, int z_radius );

class trade_ui
{
    public:
        using pane_t = trade_selector;
        using party_t = Character;
        using entry_t = trade_selector::entry_t;
        using select_t = trade_selector::select_t;
        using currency_t = int;
        struct trade_result_t {
            bool traded = false;
            currency_t balance = 0;
            currency_t delta_bank = 0;
            currency_t value_you = 0;
            currency_t value_trader = 0;
            select_t items_you;
            select_t items_trader;
        };

        enum class event { TRADECANCEL = 0, TRADEOK = 1, SWITCH = 2, NEVENTS = 3 };

        trade_ui( party_t &you, npc &trader, currency_t cost = 0, std::string title = _( "Trade" ),
                  int you_nearby_item_radius = 1, int you_nearby_ally_radius = 0,
                  basecamp *you_basecamp = nullptr, bool encounter_only = false,
                  bool goods_to_stash = false, int nearby_z_radius = 0 );

        void pushevent( event const &ev );

        trade_result_t perform_trade();
        std::map<std::string, std::string> semantic_payload() const;
        currency_t active_offer_value( const entry_t &entry ) const;
        void recalc_values_cpane();
        bool can_autobalance() const;
        void autobalance();
        void bank_balance();
        void resize();

        constexpr static int header_size = 5;

    private:
        constexpr static std::size_t npanes = 2;
        using panecont_t = std::array<std::unique_ptr<pane_t>, npanes>;
        using values_t = std::array<currency_t, npanes>;
        using parties_t = std::array<party_t *, npanes>;

        constexpr static panecont_t::size_type _trader = 0;
        constexpr static panecont_t::size_type _you = 1;

        trade_preset _upreset, _tpreset;
        panecont_t _panes;
        values_t _trade_values{};
        parties_t _parties;
        std::queue<event> _queue;
        panecont_t::size_type _cpane = 0;
        bool _exit = true;
        bool _traded = false;
        const currency_t _requested_cost;
        const bool _goods_to_stash;
        currency_t _cost = 0;
        currency_t _balance = 0;
        currency_t _bank = 0;
        currency_t _delta_bank = 0;
        std::string const _title;
        catacurses::window _header_w;
        ui_adaptor _header_ui;

        void _process( event const &ev );
        bool _confirm_trade() const;
        void _draw_header();
};

#endif // CATA_SRC_TRADE_UI_H

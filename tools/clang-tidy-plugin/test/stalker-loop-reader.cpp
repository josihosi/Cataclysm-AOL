// RUN: %check_clang_tidy %s cata-stalker-loop-reader %t -- --load=%cata_plugin --

struct monster {};
struct site_reader {};
struct monster_range {
    monster *begin();
    monster *end();
};
struct game {
    monster_range all_monsters();
};
namespace writhing_stalker
{
bool can_observe_actor( const monster &, const monster & );
bool can_observe_actor( const monster &, const monster &, site_reader & );
} // namespace writhing_stalker

void check_witnesses( game &world, monster_range &pack, const monster &target,
                      site_reader &shared_reader )
{
    using writhing_stalker::can_observe_actor;
    for( const auto &observer : world.all_monsters() ) {
        can_observe_actor( observer, target );
        // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: this observation creates a site_reader inside the monster loop. Consider sharing a reader while the lighting and map are stable [cata-stalker-loop-reader]
        can_observe_actor( observer, target, shared_reader );
    }
    for( const auto &observer : pack ) {
        can_observe_actor( observer, target );
    }
    can_observe_actor( target, target );
}

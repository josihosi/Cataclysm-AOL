#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

#include "cata_catch.h"
#include "coordinates.h"
#include "map.h"
#include "map_helpers.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "point.h"

std::vector<tripoint_bub_ms> live_bandit_boundary_perimeter_for_test( map &here, int z );
std::vector<std::vector<std::size_t>> live_bandit_boundary_neighbors_for_test(
    const std::vector<tripoint_abs_ms> &exits );

TEST_CASE( "bandit exit geometry retains exhaustive boundary and neighbor order", "[bandit][exit_cost]" )
{
    clear_map();
    map &here = get_map();
    const auto perimeter = live_bandit_boundary_perimeter_for_test( here, 0 );
    CHECK( live_bandit_boundary_perimeter_for_test( here, OVERMAP_HEIGHT + 1 ).empty() );
    CHECK( live_bandit_boundary_perimeter_for_test( here, -OVERMAP_DEPTH - 1 ).empty() );
    std::vector<tripoint_bub_ms> expected;
    for( const tripoint_bub_ms &p : here.points_on_zlevel( 0 ) ) {
        if( p.x() == 0 || p.y() == 0 || p.x() == SEEX * here.getmapsize() - 1 ||
            p.y() == SEEY * here.getmapsize() - 1 ) {
            expected.push_back( p );
        }
    }
    auto sorted_perimeter = perimeter;
    std::sort( sorted_perimeter.begin(), sorted_perimeter.end() );
    std::sort( expected.begin(), expected.end() );
    CHECK( sorted_perimeter == expected );
    CHECK( perimeter.size() == static_cast<std::size_t>( 2 * ( SEEX + SEEY ) *
                             here.getmapsize() - 4 ) );

    std::vector<tripoint_abs_ms> exits;
    for( const tripoint_bub_ms &p : perimeter ) {
        const tripoint_abs_ms departure = here.get_abs( p );
        for( int dy = -1; dy <= 1; ++dy ) {
            for( int dx = -1; dx <= 1; ++dx ) {
                if( dx == 0 && dy == 0 ) {
                    continue;
                }
                const tripoint_abs_ms exit = departure + point_rel_ms( dx, dy );
                if( !here.inbounds( exit ) ) {
                    exits.push_back( exit );
                }
            }
        }
    }
    const auto joined = live_bandit_boundary_neighbors_for_test( exits );
    REQUIRE( joined.size() == exits.size() );
    std::size_t total = 0;
    for( std::size_t i = 0; i < exits.size(); ++i ) {
        std::vector<std::size_t> reference;
        for( std::size_t j = 0; j < exits.size(); ++j ) {
            const auto delta = exits[i] - exits[j];
            if( ( delta.x() != 0 || delta.y() != 0 ) &&
                std::abs( delta.x() ) <= 1 && std::abs( delta.y() ) <= 1 ) {
                reference.push_back( j );
            }
        }
        CHECK( joined[i] == reference );
        total += joined[i].size();
    }
    std::cout << "exit_geometry boundary_cells=" << perimeter.size()
              << " candidates=" << exits.size() << " neighbor_visits=" << total
              << " exhaustive_area=" << SEEX * SEEY * here.getmapsize() * here.getmapsize()
              << " exhaustive_pairs=" << exits.size() * exits.size() << '\n';
    CHECK( total < exits.size() * exits.size() / 10 );
}

TEST_CASE( "bandit exit neighbor join retains order with gaps and duplicate exits", "[bandit][exit_cost]" )
{
    for( int mask = 0; mask < 8; ++mask ) {
        std::vector<tripoint_abs_ms> exits;
        for( int y = -3; y <= 3; ++y ) {
            for( int x = -3; x <= 3; ++x ) {
                if( ( x * x + y * y + mask ) % 3 != 0 ) {
                    exits.emplace_back( x, y, 0 );
                    if( ( x + y + mask ) % 4 == 0 ) {
                        exits.emplace_back( x, y, 0 );
                    }
                }
            }
        }
        const auto actual = live_bandit_boundary_neighbors_for_test( exits );
        REQUIRE( actual.size() == exits.size() );
        for( std::size_t i = 0; i < exits.size(); ++i ) {
            std::vector<std::size_t> expected;
            for( std::size_t j = 0; j < exits.size(); ++j ) {
                const auto delta = exits[i] - exits[j];
                if( ( delta.x() != 0 || delta.y() != 0 ) && std::abs( delta.x() ) <= 1 &&
                    std::abs( delta.y() ) <= 1 ) {
                    expected.push_back( j );
                }
            }
            CHECK( actual[i] == expected );
        }
    }
}

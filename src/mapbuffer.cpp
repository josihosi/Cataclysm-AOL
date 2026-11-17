#include "save_continuity.h"
#include "mapbuffer.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <variant>
#include <functional>
#include <fstream>
#include <cmath>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cata_path.h"
#include "calendar.h"
#include "cata_utility.h"
#include "debug.h"
#include "do_turn.h"
#include "filesystem.h"
#include "flexbuffer_json.h"
#include "game.h"
#include "input.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "output.h"
#include "overmapbuffer.h"
#include "path_info.h"
#include "point.h"
#include "popup.h"
#include "std_hash_fs_path.h"
#include "string_formatter.h"
#include "submap.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "vehicle.h"
#include "worldfactory.h"
#include "zzip.h"

#define dbg(x) DebugLog((x),D_MAP) << __FILE__ << ":" << __LINE__ << ": "

namespace
{
thread_local submap_load_counts *current_submap_load_counts = nullptr;
}

scoped_submap_load_counter::scoped_submap_load_counter( submap_load_counts &counts ) :
    previous( current_submap_load_counts )
{
    current_submap_load_counts = &counts;
}

scoped_submap_load_counter::~scoped_submap_load_counter()
{
    current_submap_load_counts = previous;
}

static std::string quad_file_name( const tripoint_abs_omt &om_addr )
{
    return string_format( "%d.%d.%d.map", om_addr.x(), om_addr.y(), om_addr.z() );
}

static cata_path find_dirname( const tripoint_abs_omt &om_addr )
{
    const tripoint_abs_seg segment_addr = project_to<coords::seg>( om_addr );
    std::string segment = string_format( "%d.%d.%d",
                                         segment_addr.x(),
                                         segment_addr.y(), segment_addr.z() );
    return PATH_INFO::current_dimension_save_path() / "maps" / segment;
}

// No borrowed submaps live here. One normal-or-checked archive/footer and
// bounded terrain metadata live only for the synchronous preparation. A failed
// checked open is remembered only until this path group/decision ends.
struct mapbuffer::existing_read_context {
    std::filesystem::path path;
    std::optional<std::variant<zzip, zzip::checked_reader>> archive;
    bool checked_attempted = false;
    overmapbuffer::existing_terrain_reader terrain{ overmap_buffer };
    std::optional<float> sunlight_penalty;
    std::map<tripoint_abs_sm, std::optional<submap_sunlight_input>> sunlight_inputs;
};

mapbuffer::existing_submap_reader::existing_submap_reader( mapbuffer &source ) :
    source( source ), context( std::make_unique<existing_read_context>() )
{
}

mapbuffer::existing_submap_reader::~existing_submap_reader() = default;

submap *mapbuffer::existing_submap_reader::lookup( const tripoint_abs_sm &p )
{
    return source.lookup_submap_existing( p, context.get() );
}

std::unique_ptr<submap> mapbuffer::existing_submap_reader::read_omitted_uniform(
    const tripoint_abs_sm &p, bool *read_error )
{
    if( current_submap_load_counts ) {
        ++current_submap_load_counts->lookup_calls;
    }
    if( read_error ) {
        *read_error = false;
    }
    // A resident override wins even when its coarse overmap tile is uniform.
    if( source.lookup_submap_cached( p ) ) {
        return nullptr;
    }
    const tripoint_abs_omt omt = project_to<coords::omt>( p );
    try {
        const cata_path directory = find_dirname( omt );
        const bool compressed = world_generator->active_world->has_compression_enabled();
        const auto path = ( compressed ? directory + zzip_suffix :
                            directory / quad_file_name( omt ) ).get_unrelative_path();
        std::error_code error;
        const auto status = std::filesystem::symlink_status( path, error );
        if( error && error != std::errc::no_such_file_or_directory ) {
            throw std::runtime_error( error.message() );
        }
        if( std::filesystem::exists( status ) ) {
            // A broken link/directory/special file is not clean absence.
            if( !std::filesystem::is_regular_file( status ) ) {
                throw std::runtime_error( "existing submap data is not a regular file" );
            }
            if( !compressed ) {
                return nullptr; // Any existing plain quad overrides uniform metadata.
            }
            // This must precede ordinary lookup: zzip::load can repair a damaged
            // archive. Recovery is not evidence of clean serialized absence.
            if( context->path != path || !context->checked_attempted ) {
                context->archive.reset();
                context->path = path;
                context->checked_attempted = true;
                auto checked = zzip::checked_reader::open( path );
                if( checked ) {
                    context->archive.emplace( std::in_place_type<zzip::checked_reader>, std::move( *checked ) );
                }
            }
            if( !context->archive || !std::holds_alternative<zzip::checked_reader>( *context->archive ) ) {
                throw std::runtime_error( "unreadable or malformed existing archive" );
            }
            if( std::get<zzip::checked_reader>( *context->archive ).has_file(
                    std::filesystem::u8path( quad_file_name( omt ) ) ) ) {
                return nullptr; // Existing quads override, including omitted siblings.
            }
        }
    } catch( const std::exception &error ) {
        if( read_error ) {
            *read_error = true;
        }
        debugmsg( "Failed to read uniform submap metadata %s: %s", p.to_string(), error.what() );
        return nullptr;
    }
    const auto terrain = context->terrain.read( omt );
    if( !terrain ) {
        return nullptr;
    }
    const auto uniform = ( *terrain )->get_uniform_terrain();
    if( !uniform ) {
        return nullptr;
    }
    auto result = std::make_unique<submap>();
    result->set_all_ter( uniform->id(), true );
    return result;
}

std::unique_ptr<submap_sunlight_input> mapbuffer::existing_submap_reader::read_sunlight_input(
    const tripoint_abs_sm &p, float outside_sight_penalty )
{
    if( !std::isfinite( outside_sight_penalty ) || outside_sight_penalty <= 0.0f ) {
        return nullptr;
    }
    if( const submap *resident = source.lookup_submap_cached( p ) ) {
        return std::make_unique<submap_sunlight_input>( resident->sunlight_input( outside_sight_penalty ) );
    }
    if( context->sunlight_penalty != outside_sight_penalty ) {
        context->sunlight_inputs.clear();
        context->sunlight_penalty = outside_sight_penalty;
    }
    const auto found = context->sunlight_inputs.find( p );
    if( found != context->sunlight_inputs.end() ) {
        return found->second ? std::make_unique<submap_sunlight_input>( *found->second ) : nullptr;
    }
    bool read_error = false;
    if( auto uniform = read_omitted_uniform( p, &read_error ) ) {
        return std::make_unique<submap_sunlight_input>( uniform->sunlight_input( outside_sight_penalty ) );
    }
    if( read_error ) {
        return nullptr; // Checked preflight precedes every possible archive lookup.
    }
    const auto omt = project_to<coords::omt>( p );
    std::map<tripoint_abs_sm, std::optional<submap_sunlight_input>> staged;
    const auto first = project_to<coords::sm>( omt );
    for( int x = 0; x < 2; ++x ) {
        for( int y = 0; y < 2; ++y ) {
            staged.emplace( first + point( x, y ), std::nullopt );
        }
    }
    try {
        std::string bytes;
        const auto name = std::filesystem::u8path( quad_file_name( omt ) );
        if( world_generator->active_world->has_compression_enabled() ) {
            const auto path = ( find_dirname( omt ) + zzip_suffix ).get_unrelative_path();
            if( context->path != path || !context->archive ||
                !std::holds_alternative<zzip::checked_reader>( *context->archive ) ) {
                throw std::runtime_error( "missing checked sunlight archive" );
            }
            auto contents = std::get<zzip::checked_reader>( *context->archive ).get_file( name,
                            ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
            if( !contents ) {
                throw std::runtime_error( "missing sunlight entry" );
            }
            bytes.assign( reinterpret_cast<const char *>( contents->data() ), contents->size() );
        } else {
            std::ifstream input( ( find_dirname( omt ) / quad_file_name( omt ) ).get_unrelative_path(),
                                 std::ios::binary );
            if( !input ) {
                throw std::runtime_error( "unreadable sunlight entry" );
            }
            std::ostringstream buffer;
            buffer << input.rdbuf();
            if( input.bad() ) {
                throw std::runtime_error( "failed sunlight entry read" );
            }
            bytes = buffer.str();
        }
        const auto document = json_loader::from_string( bytes );
        if( !document.test_array() ) {
            throw std::runtime_error( "invalid sunlight document" );
        }
        const JsonArray entries = document.get_array();
        if( entries.empty() || entries.size() > 4 ) {
            throw std::runtime_error( "invalid sunlight quad" );
        }
        std::set<tripoint_abs_sm> positions;
        for( const JsonValue saved : entries ) {
            if( !saved.test_object() ) {
                throw std::runtime_error( "invalid sunlight entry type" );
            }
            const JsonObject entry = saved.get_object();
            entry.allow_omitted_members();
            if( !entry.has_array( "coordinates" ) ) {
                throw std::runtime_error( "missing sunlight coordinates" );
            }
            const JsonArray coordinates = entry.get_array( "coordinates" );
            if( coordinates.size() != 3 || !coordinates.has_int( 0 ) ||
                !coordinates.has_int( 1 ) || !coordinates.has_int( 2 ) ) {
                throw std::runtime_error( "invalid sunlight coordinates" );
            }
            for( const JsonValue coordinate : coordinates ) {
                const int64_t value = coordinate.get_int64();
                if( value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max() ) {
                    throw std::runtime_error( "sunlight coordinate overflow" );
                }
            }
            const tripoint_abs_sm position( coordinates.get_int( 0 ), coordinates.get_int( 1 ),
                                            coordinates.get_int( 2 ) );
            if( project_to<coords::omt>( position ) != omt || !positions.insert( position ).second ) {
                throw std::runtime_error( "foreign or duplicate sunlight coordinates" );
            }
            auto summary = submap::read_sunlight_input( entry, position, outside_sight_penalty );
            if( !summary ) {
                throw std::runtime_error( "unsupported sunlight physical input" );
            }
            staged[position] = std::move( summary );
        }
    } catch( const std::exception & ) {
        for( auto &[position, value] : staged ) {
            value.reset();
        }
    }
    // A fixed native window intersects at most144 sibling-expanded positions
    // in one segment/level. Bound generic callers too without retaining raw
    // quad documents or payloads. No values survive this reader's decision.
    if( context->sunlight_inputs.size() + staged.size() > 256 ) {
        context->sunlight_inputs.clear();
    }
    context->sunlight_inputs.insert( staged.begin(), staged.end() );
    const auto &result = context->sunlight_inputs.at( p );
    return result ? std::make_unique<submap_sunlight_input>( *result ) : nullptr;
}

mapbuffer MAPBUFFER;

mapbuffer::mapbuffer() = default;
mapbuffer::~mapbuffer() = default;

void mapbuffer::clear()
{
    submaps.clear();
}

void mapbuffer::clear_outside_reality_bubble()
{
    map &here = get_map();
    auto it = submaps.begin();
    while( it != submaps.end() ) {
        if( here.inbounds( it->first ) ) {
            ++it;
        } else {
            it = submaps.erase( it );
        }
    }
}

bool mapbuffer::add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm )
{
    if( submaps.count( p ) ) {
        return false;
    }

    submap &stored = *sm;
    submaps[p] = std::move( sm );
    for( const auto &v : stored.vehicles ) {
        index_vehicle( *v, p );
    }

    return true;
}

const std::vector<mapbuffer::vehicle_part_position> &mapbuffer::vehicle_parts_at(
    const tripoint_abs_sm &p ) const
{
    static const std::vector<vehicle_part_position> empty;
    const auto it = vehicle_cells.find( p );
    return it == vehicle_cells.end() ? empty : it->second;
}

void mapbuffer::forget_vehicle( vehicle &v )
{
    const auto it = indexed_vehicles.find( &v );
    if( it != indexed_vehicles.end() ) {
        for( const auto &cell : it->second.cells ) {
            auto entry = vehicle_cells.find( cell );
            if( entry == vehicle_cells.end() ) {
                continue;
            }
            auto &parts = entry->second;
            parts.erase( std::remove_if( parts.begin(), parts.end(), [&]( const auto & part ) {
                return part.first == &v;
            } ), parts.end() );
            if( parts.empty() ) {
                vehicle_cells.erase( entry );
            }
        }
        indexed_vehicles.erase( it );
    }
    if( v.resident_index.owner == this ) {
        v.resident_index.owner = nullptr;
    }
}

void mapbuffer::index_vehicle( vehicle &v, tripoint_abs_sm owner )
{
    if( v.resident_index.owner ) {
        v.resident_index.owner->forget_vehicle( v );
    }
    v.resident_index.owner = this;
    auto &record = indexed_vehicles[&v];
    record.owner = owner;
    // Serialized submap ownership is authoritative before map::load establishes
    // sm_pos. Correct the coordinate frame in the index without moving the vehicle.
    const auto offset = project_to<coords::ms>( owner ) - project_to<coords::ms>( v.sm_pos );
    for( const vpart_reference &part : v.get_all_parts_with_fakes() ) {
        if( !part.part().removed ) {
            const auto pos = v.abs_part_pos( part.part() ) + offset;
            const auto cell = project_to<coords::sm>( pos );
            vehicle_cells[cell].emplace_back( &v, pos );
            record.cells.insert( cell );
        }
    }
}

void mapbuffer::update_vehicle_index( vehicle &v )
{
    const auto owns = [&]( const tripoint_abs_sm & owner ) {
        const submap *sm = lookup_submap_cached( owner );
        return sm && std::any_of( sm->vehicles.begin(), sm->vehicles.end(),
        [&]( const auto & candidate ) {
            return candidate.get() == &v;
        } );
    };
    if( owns( v.sm_pos ) ) {
        index_vehicle( v, v.sm_pos );
    } else {
        const auto previous = indexed_vehicles.find( &v );
        if( previous != indexed_vehicles.end() && owns( previous->second.owner ) ) {
            index_vehicle( v, previous->second.owner );
        } else {
            forget_vehicle( v );
        }
    }
}

bool mapbuffer::add_submap( const tripoint_abs_sm &p, submap *sm )
{
    // FIXME: get rid of this overload and make submap ownership semantics sane.
    std::unique_ptr<submap> temp( sm );
    bool result = add_submap( p, temp );
    if( !result ) {
        // NOLINTNEXTLINE( bugprone-unused-return-value )
        temp.release();
    }
    return result;
}

void mapbuffer::remove_submap( const tripoint_abs_sm &addr )
{
    auto m_target = submaps.find( addr );
    if( m_target == submaps.end() ) {
        debugmsg( "Tried to remove non-existing submap %s", addr.to_string() );
        return;
    }
    submaps.erase( m_target );
}

submap *mapbuffer::lookup_submap( const tripoint_abs_sm &p )
{
    if( current_submap_load_counts ) {
        ++current_submap_load_counts->lookup_calls;
    }
    dbg( D_INFO ) << "mapbuffer::lookup_submap( x[" << p.x() << "], y[" << p.y() << "], z["
                  << p.z() << "])";

    const auto iter = submaps.find( p );
    if( iter == submaps.end() ) {
        try {
            return unserialize_submaps( p );
        } catch( const std::exception &err ) {
            debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        }
        return nullptr;
    }

    return iter->second.get();
}

submap *mapbuffer::lookup_submap_existing( const tripoint_abs_sm &p )
{
    return lookup_submap_existing( p, nullptr );
}

submap *mapbuffer::lookup_submap_existing( const tripoint_abs_sm &p,
        existing_read_context *context )
{
    if( current_submap_load_counts ) {
        ++current_submap_load_counts->lookup_calls;
    }
    raid_perf_scope lookup_scope( raid_perf_stage::nav_view_lookup, raid_perf_nav_build_active() );
    const auto iter = submaps.find( p );
    if( iter != submaps.end() ) {
        raid_perf_note_nav( iter->second ? raid_perf_nav_event::resident_hit :
                            raid_perf_nav_event::resident_null );
        return iter->second.get();
    }
    raid_perf_note_nav( raid_perf_nav_event::buffer_miss );
    try {
        raid_perf_scope load_scope( raid_perf_stage::nav_view_miss_load, raid_perf_nav_lookup_active() );
        return unserialize_submaps( p, false, context );
    } catch( const std::exception &err ) {
        raid_perf_note_nav( raid_perf_nav_event::load_failures );
        debugmsg( "Failed to load existing submap %s: %s", p.to_string(), err.what() );
    }
    return nullptr;
}

bool mapbuffer::submap_exists( const tripoint_abs_sm &p )
{
    // Could so with a second check against a std::unordered_set<tripoint_abs_sm> of already checked existing but not loaded submaps before resorting to unserializing?
    const auto iter = submaps.find( p );
    if( iter == submaps.end() ) {
        try {
            return unserialize_submaps( p );
        } catch( const std::exception &err ) {
            debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        }
        return false;
    }

    return true;
}

bool mapbuffer::submap_exists_approx( const tripoint_abs_sm &p )
{
    const auto iter = submaps.find( p );
    if( iter == submaps.end() ) {
        if( current_submap_load_counts ) {
            ++current_submap_load_counts->lookup_calls;
        }
        try {
            const tripoint_abs_omt om_addr = project_to<coords::omt>( p );
            const cata_path dirname = find_dirname( om_addr );
            std::string file_name = quad_file_name( om_addr );

            if( world_generator->active_world->has_compression_enabled() ) {
                cata_path zzip_name = dirname;
                zzip_name += zzip_suffix;
                if( !file_exist( zzip_name ) ) {
                    return false;
                }
                std::optional<zzip> z = zzip::load( zzip_name.get_unrelative_path(),
                                                    ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
                return z && z->has_file( std::filesystem::u8path( file_name ) );
            } else {
                return file_exist( dirname / file_name );
            }
        } catch( const std::exception &err ) {
            debugmsg( "Failed to load submap %s: %s", p.to_string(), err.what() );
        }
        return false;
    }

    return true;
}

void mapbuffer::save( bool delete_after_save )
{
    assure_dir_exist( PATH_INFO::current_dimension_save_path() / "maps" );
    int num_saved_submaps = 0;
    int num_total_submaps = submaps.size();

    map &here = get_map();

    static_popup popup;

    // A set of already-saved submaps, in global overmap coordinates.
    std::set<tripoint_abs_omt> saved_submaps;
    std::list<tripoint_abs_sm> submaps_to_delete;
    static constexpr std::chrono::milliseconds update_interval( 500 );
    std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

    for( auto &elem : submaps ) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if( last_update + update_interval < now ) {
            popup.message( _( "Please wait as the map saves [%d/%d]" ),
                           num_saved_submaps, num_total_submaps );
            ui_manager::redraw();
            refresh_display();
            inp_mngr.pump_events();
            last_update = now;
        }
        // Whatever the coordinates of the current submap are,
        // we're saving a 2x2 quad of submaps at a time.
        // Submaps are generated in quads, so we know if we have one member of a quad,
        // we have the rest of it, if that assumption is broken we have REAL problems.
        const tripoint_abs_omt om_addr = project_to<coords::omt>( elem.first );
        if( saved_submaps.count( om_addr ) != 0 ) {
            // Already handled this one.
            continue;
        }
        saved_submaps.insert( om_addr );

        // A segment is a chunk of 32x32 submap quads.
        // We're breaking them into subdirectories so there aren't too many files per directory.
        // Might want to make a set for this one too so it's only checked once per save().
        const cata_path dirname = find_dirname( om_addr );
        const cata_path quad_path = dirname / quad_file_name( om_addr );

        bool inside_reality_bubble = here.inbounds( om_addr );
        // delete_on_save deletes everything, otherwise delete submaps
        // outside the current map.
        save_quad( dirname, quad_path, om_addr, submaps_to_delete,
                   delete_after_save || !inside_reality_bubble );
        num_saved_submaps += 4;
    }
    for( auto &elem : submaps_to_delete ) {
        remove_submap( elem );
    }
}

void mapbuffer::save_quad(
    const cata_path &dirname, const cata_path &filename, const tripoint_abs_omt &om_addr,
    std::list<tripoint_abs_sm> &submaps_to_delete, bool delete_after_save )
{
    std::vector<point_rel_sm> offsets;
    std::vector<tripoint_abs_sm> submap_addrs;
    offsets.reserve( 4 );
    submap_addrs.reserve( 4 );
    offsets.push_back( point_rel_sm::zero );
    offsets.push_back( point_rel_sm::south );
    offsets.push_back( point_rel_sm::east );
    offsets.push_back( point_rel_sm::south_east );

    bool all_uniform = true;
    bool reverted_to_uniform = false;
    bool file_exists = false;

    std::optional<zzip> z;
    cata_path zzip_name = dirname;
    zzip_name += zzip_suffix;
    // The number of uniform submaps is so enormous that the filesystem overhead
    // for this step of just checking if the quad exists approaches 70% of the
    // total cost of saving the mapbuffer, in one test save I had.
    if( world_generator->active_world->has_compression_enabled() ) {
        z = zzip::load( zzip_name.get_unrelative_path(),
                        ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
        if( !z ) {
            throw std::runtime_error( "Failed opening compressed save file " +
                                      zzip_name.get_unrelative_path().generic_u8string() );
        }
        file_exists = z->has_file( filename.get_relative_path().filename() );
    } else {
        file_exists = std::filesystem::exists( filename.get_unrelative_path() );
    }

    for( point_rel_sm &offsets_offset : offsets ) {
        tripoint_abs_sm submap_addr = project_to<coords::sm>( om_addr );
        submap_addr += offsets_offset.raw(); // TODO: Make += etc. available to relative parameters as well.
        submap_addrs.push_back( submap_addr );
        submap *sm = submaps[submap_addr].get();
        if( sm != nullptr ) {
            if( !sm->is_uniform() ) {
                all_uniform = false;
            } else if( sm->reverted ) {
                reverted_to_uniform = file_exists;
            }
        }
    }

    if( all_uniform ) {
        // Nothing to save - this quad will be regenerated faster than it would be re-read
        if( delete_after_save ) {
            for( auto &submap_addr : submap_addrs ) {
                if( submaps.count( submap_addr ) > 0 && submaps[submap_addr] != nullptr ) {
                    submaps_to_delete.push_back( submap_addr );
                }
            }
        }

        // deleting the file might fail on some platforms in some edge cases so force serialize this
        // uniform quad
        if( !reverted_to_uniform ) {
            return;
        }
    }

    std::stringstream stringout;
    JsonOut jsout( stringout );
    jsout.start_array();
    for( auto &submap_addr : submap_addrs ) {
        if( submaps.count( submap_addr ) == 0 ) {
            continue;
        }

        submap *sm = submaps[submap_addr].get();

        if( sm == nullptr ) {
            continue;
        }

        jsout.start_object();

        jsout.member( "version", savegame_version );
        jsout.member( "coordinates" );

        jsout.start_array();
        jsout.write( submap_addr.x() );
        jsout.write( submap_addr.y() );
        jsout.write( submap_addr.z() );
        jsout.end_array();

        sm->store( jsout );

        jsout.end_object();

        if( delete_after_save ) {
            submaps_to_delete.push_back( submap_addr );
        }
    }

    jsout.end_array();

    std::string s = std::move( stringout ).str();

    if( z ) {
        if( !z->add_file( filename.get_relative_path().filename(), s ) ) {
            throw std::runtime_error( "Failed writing compressed submap " + filename.generic_u8string() );
        }
    } else {
        // The plain standalone writer owns the same incomplete-publication
        // boundary as zzip, before it replaces any protected world bytes.
        save_continuity::before_file_write( filename.get_unrelative_path() );
        // Don't create the directory if it would be empty
        assure_dir_exist( dirname );
        write_to_file( filename, [&]( std::ostream & fout ) {
            fout << s;
        } );
    }

    if( all_uniform && reverted_to_uniform ) {
        if( z ) {
            if( !z->delete_files( { filename.get_relative_path().filename() } ) ) {
                throw std::runtime_error( "Failed removing reverted compressed submap" );
            }
        } else {
            if( !std::filesystem::remove( filename.get_unrelative_path() ) ) {
                throw std::runtime_error( "Failed removing reverted plain submap" );
            }
        }
    }
    if( z ) {
        cata_path tmp_path = zzip_name + ".tmp";
        if( z->compact_to( tmp_path.get_unrelative_path(), 2.0 ) ) {
            z.reset();
            if( !rename_file( tmp_path, zzip_name ) ) {
                throw std::runtime_error( "Failed replacing compacted map archive" );
            }
        }
    }
}

// We're reading in way too many entities here to mess around with creating sub-objects and
// seeking around in them, so we're using the json streaming API.
submap *mapbuffer::unserialize_submaps( const tripoint_abs_sm &p, const bool generate_uniform,
                                      existing_read_context *context )
{
    if( current_submap_load_counts ) {
        ++current_submap_load_counts->unserialize_calls;
    }
    // Map the tripoint to the submap quad that stores it.
    const tripoint_abs_omt om_addr = project_to<coords::omt>( p );
    const cata_path dirname = find_dirname( om_addr );
    std::string file_name = quad_file_name( om_addr );
    std::filesystem::path file_name_path = std::filesystem::u8path( file_name );
    cata_path quad_path = dirname / file_name;

    bool read = [&] {
        if( world_generator->active_world->has_compression_enabled() )
        {
            cata_path zzip_name = dirname;
            zzip_name += zzip_suffix;
            const auto path = zzip_name.get_unrelative_path();
            std::vector<std::byte> contents;
            if( context && context->archive && context->path == path &&
                std::holds_alternative<zzip::checked_reader>( *context->archive ) ) {
                const auto &reader = std::get<zzip::checked_reader>( *context->archive );
                if( !reader.has_file( file_name_path ) ) {
                    raid_perf_note_nav( raid_perf_nav_event::missing_quad );
                    return false;
                }
                auto bytes = reader.get_file( file_name_path,
                             ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
                if( !bytes ) {
                    throw std::runtime_error( "unreadable checked submap archive entry" );
                }
                contents = std::move( *bytes );
            } else {
                std::optional<zzip> opened;
                zzip *z = nullptr;
                if( context && context->archive && context->path == path &&
                    std::holds_alternative<zzip>( *context->archive ) ) {
                    z = &std::get<zzip>( *context->archive );
                } else {
                    if( !file_exist( zzip_name ) ) {
                        raid_perf_note_nav( raid_perf_nav_event::missing_archive );
                        return false;
                    }
                    raid_perf_note_nav( raid_perf_nav_event::archive_load_calls );
                    opened = zzip::load( path,
                                         ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
                    if( !opened ) {
                        debugmsg( _fmt( "Failed to load submaps from {0}, could not open zzip.", zzip_name ) );
                        return false;
                    }
                    if( context ) {
                        context->archive.reset();
                        context->path = path;
                        context->checked_attempted = false;
                        context->archive.emplace( std::in_place_type<zzip>, std::move( *opened ) );
                        z = &std::get<zzip>( *context->archive );
                    } else {
                        z = &*opened;
                    }
                }
                if( !z->has_file( file_name_path ) ) {
                    raid_perf_note_nav( raid_perf_nav_event::missing_quad );
                    return false;
                }
                contents = z->get_file( file_name_path );
            }
            std::string_view string_contents{ reinterpret_cast<char *>( contents.data() ), contents.size() };
            JsonValue jsin = json_loader::from_string( std::string( string_contents ) );
            try {
                deserialize( jsin, true );
            } catch( std::exception &err ) {
                debugmsg( _( "Failed to read from \"%1$s\": %2$s" ), zzip_name.generic_u8string() + ":" + file_name,
                          err.what() );
                return false;
            }
            return true;
        } else
        {
            raid_perf_note_nav( raid_perf_nav_event::plain_read_calls );
            return read_from_file_optional_json( quad_path, [this]( const JsonValue & jsin ) {
                deserialize( jsin, true );
            } );
        }
    }();

    if( !read ) {
        raid_perf_note_nav( raid_perf_nav_event::load_failures );
        return nullptr;
    }

    if( !generate_uniform ) {
        const auto iter = submaps.find( p );
        raid_perf_note_nav( iter != submaps.end() && iter->second ?
                            raid_perf_nav_event::loaded_submap : raid_perf_nav_event::omitted_submap );
        return iter == submaps.end() ? nullptr : iter->second.get();
    }

    // fill in uniform submaps that were not serialized. Note that failure as a result of it
    // not being uniform is OK and results in any missing uniform submaps being generated.
    oter_id const oid = overmap_buffer.ter( om_addr );
    // This quad has already been read. Complete omitted uniform siblings here;
    // generate_uniform_omt would recursively ask submap_exists to read this same quad.
    if( const auto terrain = oid->get_uniform_terrain() ) {
        const tripoint_abs_sm first = project_to<coords::sm>( om_addr );
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                const auto position = first + point_rel_sm( x, y );
                if( !submaps.count( position ) ) {
                    auto sm = std::make_unique<submap>();
                    sm->set_all_ter( *terrain, true );
                    sm->last_touched = calendar::turn;
                    add_submap( position, sm );
                }
            }
        }
    }
    if( submaps.count( p ) == 0 ) {
        debugmsg( "file %s did not contain the expected submap %s for non-uniform terrain %s",
                  quad_path.generic_u8string(), p.to_string(), oid.id().str() );
    }

    return submaps[ p ].get();
}

void mapbuffer::deserialize( const JsonArray &ja, const bool skip_existing )
{
    if( current_submap_load_counts ) {
        ++current_submap_load_counts->deserialize_calls;
    }
    for( JsonObject submap_json : ja ) {
        if( skip_existing && submap_json.has_array( "coordinates" ) ) {
            JsonArray coordinates = submap_json.get_array( "coordinates" );
            const tripoint_abs_sm position{ coordinates.next_int(), coordinates.next_int(),
                                            coordinates.next_int() };
            if( submaps.count( position ) ) {
                submap_json.allow_omitted_members();
                continue;
            }
        }
        std::unique_ptr<submap> sm = std::make_unique<submap>();
        tripoint_abs_sm submap_coordinates;
        int version = 0;
        // We have to read version first because the iteration order of json members is undefined.
        if( submap_json.has_int( "version" ) ) {
            version = submap_json.get_int( "version" );
        }
        for( JsonMember submap_member : submap_json ) {
            std::string submap_member_name = submap_member.name();
            if( submap_member_name == "coordinates" ) {
                JsonArray coords_array = submap_member;
                tripoint_abs_sm loc{ coords_array.next_int(), coords_array.next_int(), coords_array.next_int() };
                submap_coordinates = loc;
            } else {
                sm->load( submap_member, submap_member_name, version );
            }
        }

        if( !add_submap( submap_coordinates, sm ) ) {
            debugmsg( "submap %s was already loaded", submap_coordinates.to_string() );
        }
    }
}

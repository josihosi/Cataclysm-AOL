#include "plain_waiting_transport.h"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

#include "filesystem.h"
#include "json.h"

namespace
{
std::string trace_path()
{
    const char *path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" );
    return path == nullptr ? "" : path;
}

bool replace_state( const std::string &path, const std::string &body )
{
    const std::string temporary = path + ".tmp";
    {
        std::ofstream stream( temporary, std::ios::binary | std::ios::trunc );
        stream << body;
        stream.flush();
        if( !stream ) {
            return false;
        }
    }
    return rename_file( temporary, path );
}

std::string event_kind( const std::string &event )
{
    std::istringstream stream( event );
    TextJsonIn input( stream );
    TextJsonObject object = input.get_object();
    object.allow_omitted_members();
    return object.get_string( "event" );
}
} // namespace

bool plain_waiting_active()
{
    static bool active = false;
    if( !active ) {
        const char *mode = std::getenv( "CAOL_PLAIN_WAITING" );
        active = mode != nullptr && std::string( mode ) == "1" &&
                 !trace_path().empty() && std::ifstream( trace_path() + ".plain" ).good();
    }
    return active;
}

void plain_waiting_publish_event( const std::string &event )
{
    // Each slot is current protocol state, never an append-only history.
    static std::map<std::string, std::pair<size_t, std::string>> current;
    static size_t revision = 0;
    static bool seeded = false;
    const std::string prefix = "openclaw_harness_semantic_step: ";
    if( !seeded ) {
        std::ifstream source( trace_path() + ".seed" );
        std::string line;
        while( std::getline( source, line ) ) {
            if( line.compare( 0, prefix.size(), prefix ) == 0 ) {
                const std::string value = line.substr( prefix.size() );
                current[event_kind( value )] = { 0, value };
            }
        }
        seeded = true;
    }
    const std::string kind = event_kind( event );
    if( kind != "frame" && kind != "surface_descriptor" && kind != "receipt" &&
        kind != "surface_receipt" && kind != "native_save_completion" ) {
        return;
    }
    ++revision;
    const std::string value = event.substr( 0, event.find_last_of( '}' ) ) +
                              ",\"_source_offset\":" + std::to_string( revision ) +
                              ",\"_source_end\":" + std::to_string( revision + 1 ) + "}";
    current[kind] = { revision, value };
    std::vector<std::pair<size_t, std::string>> ordered;
    for( const auto &entry : current ) {
        ordered.push_back( entry.second );
    }
    std::stable_sort( ordered.begin(), ordered.end(), []( const auto & a, const auto & b ) {
        return a.first < b.first;
    } );
    std::ostringstream body;
    for( const auto &entry : ordered ) {
        body << prefix << entry.second << '\n';
    }
    if( replace_state( trace_path(), body.str() ) ) {
        remove_file( trace_path() + ".seed" );
    }
}

void plain_waiting_record_turn( size_t generation, double seconds, int game_turn, int game_minutes )
{
    static size_t current_generation = 0;
    static std::deque<double> window;
    static bool alarmed = false;
    static size_t alarms = 0;
    static size_t recoveries = 0;
    static size_t turns = 0;
    static double last_alarm_mean = 0;
    if( generation != current_generation ) {
        current_generation = generation;
        window.clear();
        alarmed = false;
    }
    if( generation == 0 ) {
        return;
    }
    window.push_back( seconds );
    if( window.size() > 100 ) {
        window.pop_front();
    }
    const long double total = std::accumulate( window.begin(), window.end(), 0.0L );
    const double mean = static_cast<double>( total / window.size() );
    const bool slow = window.size() == 100 && total > static_cast<long double>( 0.010 ) * 100;
    const bool changed = slow != alarmed;
    if( changed ) {
        if( slow ) {
            ++alarms;
            last_alarm_mean = mean;
        } else {
            ++recoveries;
        }
        alarmed = slow;
    }
    // Publish crossings immediately, and refresh ordinary progress once per
    // measurement window. The file is replaced, not logged per turn.
    ++turns;
    if( !changed && turns % 100 != 0 && window.size() != 1 ) {
        return;
    }
    std::ostringstream body;
    JsonOut out( body );
    out.start_object();
    const char *run = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    out.member( "run_id", run == nullptr ? "" : run );
    out.member( "generation", generation );
    out.member( "sample_count", window.size() );
    out.member( "mean_seconds", mean );
    out.member( "alarmed", alarmed );
    out.member( "alarm_count", alarms );
    out.member( "recovery_count", recoveries );
    out.member( "last_alarm_mean", last_alarm_mean );
    out.member( "game_turn", game_turn );
    out.member( "game_minutes", game_minutes );
    out.end_object();
    replace_state( trace_path() + ".performance", body.str() );
}

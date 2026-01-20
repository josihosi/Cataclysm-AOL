#include "llm_intent.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <ratio>
#include <sstream>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <exception>

#include "cata_path.h"
#include "character.h"
#include "character_id.h"
#include "cata_utility.h"
#include "ammo.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "flexbuffer_json.h"
#include "filesystem.h"
#include "itype.h"
#include "effect.h"
#include "game.h"
#include "item.h"
#include "item_location.h"
#include "line.h"
#include "json.h"
#include "map.h"
#include "messages.h"
#include "memory_fast.h"
#include "npc_opinion.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "value_ptr.h"
#include "visitable.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <csignal>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

class monster;

namespace
{
std::mutex llm_intent_log_mutex;
constexpr const char *llm_intent_log_path = "config/llm_intent.log";
constexpr std::streamoff llm_intent_log_rotate_bytes = 50 * 1024 * 1024;

void append_llm_intent_log( const std::string &payload )
{
    std::lock_guard<std::mutex> lock( llm_intent_log_mutex );
    const std::filesystem::path log_path( llm_intent_log_path );
    std::error_code ec;
    if( std::filesystem::exists( log_path, ec ) ) {
        const std::uintmax_t size = std::filesystem::file_size( log_path, ec );
        if( !ec && size >= static_cast<std::uintmax_t>( llm_intent_log_rotate_bytes ) ) {
            for( int i = 1; i <= 9999; ++i ) {
                std::filesystem::path rotated = log_path;
                rotated += "." + std::to_string( i );
                if( !std::filesystem::exists( rotated, ec ) ) {
                    std::filesystem::rename( log_path, rotated, ec );
                    break;
                }
            }
        }
    }
    std::ofstream out( log_path, std::ios::binary | std::ios::app );
    if( !out ) {
        return;
    }
    out << payload;
}

struct llm_intent_request {
    std::string request_id;
    character_id npc_id;
    std::string npc_name;
    std::string prompt;
    std::string snapshot;
    int max_tokens = 0;
    float temperature = 0.0f;
    float top_p = 0.0f;
    float repetition_penalty = 0.0f;
};

struct llm_intent_response {
    std::string request_id;
    character_id npc_id;
    std::string npc_name;
    bool ok = false;
    std::string text;
    std::string error;
    std::string raw;
};

struct runner_config {
    std::string python_path;
    std::string runner_path;
    std::string model_dir;
    std::string backend;
    std::string device;
    bool use_api = false;
    std::string api_key_env;
    std::string api_provider;
    std::string api_model;
    int max_tokens = 0;
    int max_prompt_len = 0;
    bool force_npu = false;

    bool operator==( const runner_config &other ) const {
        return python_path == other.python_path &&
               runner_path == other.runner_path &&
               model_dir == other.model_dir &&
               backend == other.backend &&
               device == other.device &&
               use_api == other.use_api &&
               api_key_env == other.api_key_env &&
               api_provider == other.api_provider &&
               api_model == other.api_model &&
               max_tokens == other.max_tokens &&
               max_prompt_len == other.max_prompt_len &&
               force_npu == other.force_npu;
    }

    bool operator!=( const runner_config &other ) const {
        return !( *this == other );
    }
};

[[maybe_unused]] std::string request_to_json( const llm_intent_request &request );
[[maybe_unused]] std::optional<llm_intent_response> response_from_json( const std::string &line,
        const llm_intent_request &request );
[[maybe_unused]] std::filesystem::path resolve_path( const std::string &path );
[[maybe_unused]] runner_config current_runner_config();
[[maybe_unused]] std::string read_log_tail( const std::filesystem::path &path,
        std::streamoff max_bytes );

std::string sanitize_text( std::string_view text )
{
    return remove_color_tags( text );
}

std::string strip_leading_article( const std::string &text )
{
    if( text.rfind( "the ", 0 ) == 0 ) {
        return text.substr( 4 );
    }
    return text;
}

std::string trim_copy( const std::string &text )
{
    size_t start = 0;
    while( start < text.size() &&
           std::isspace( static_cast<unsigned char>( text[start] ) ) ) {
        ++start;
    }
    size_t end = text.size();
    while( end > start &&
           std::isspace( static_cast<unsigned char>( text[end - 1] ) ) ) {
        --end;
    }
    return text.substr( start, end - start );
}

std::string lower_copy( const std::string &text )
{
    std::string out = text;
    std::transform( out.begin(), out.end(), out.begin(), []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    return out;
}

struct background_summary_entry {
    std::string background;
    std::string expression;
    std::string source_tag;
};

struct background_summary_cache {
    std::unordered_map<std::string, std::string> trait_to_topic;
    std::unordered_map<std::string, background_summary_entry> summary_by_topic;
    bool loaded = false;
};

void gather_traits_from_condition( const JsonObject &cond, std::vector<std::string> &out )
{
    cond.allow_omitted_members();
    if( cond.has_string( "npc_has_trait" ) ) {
        out.push_back( cond.get_string( "npc_has_trait" ) );
    }
    if( cond.has_array( "and" ) ) {
        for( const JsonObject entry : cond.get_array( "and" ) ) {
            gather_traits_from_condition( entry, out );
        }
    }
    if( cond.has_array( "or" ) ) {
        for( const JsonObject entry : cond.get_array( "or" ) ) {
            gather_traits_from_condition( entry, out );
        }
    }
}

std::string normalize_summary_line( std::string summary )
{
    summary.erase( std::remove( summary.begin(), summary.end(), '\r' ), summary.end() );
    std::replace( summary.begin(), summary.end(), '\n', ' ' );
    return trim_copy( summary );
}

background_summary_cache &get_background_summaries()
{
    static background_summary_cache cache;
    if( cache.loaded ) {
        return cache;
    }
    cache.loaded = true;

    const cata_path toc_path = PATH_INFO::datadir_path() / "json" / "npcs" / "Backgrounds" /
                               "backgrounds_table_of_contents.json";
    read_from_file_optional_json( toc_path, [&]( const JsonArray & root ) {
        for( const JsonObject entry : root ) {
            entry.allow_omitted_members();
            if( entry.get_string( "type", "" ) != "talk_topic" ) {
                continue;
            }
            if( !entry.has_array( "responses" ) ) {
                continue;
            }
            for( const JsonObject resp : entry.get_array( "responses" ) ) {
                resp.allow_omitted_members();
                if( !resp.has_string( "topic" ) || !resp.has_object( "condition" ) ) {
                    continue;
                }
                const std::string topic = resp.get_string( "topic" );
                const JsonObject cond = resp.get_object( "condition" );
                cond.allow_omitted_members();
                std::vector<std::string> traits;
                gather_traits_from_condition( cond, traits );
                for( const std::string &trait : traits ) {
                    if( cache.trait_to_topic.count( trait ) == 0 ) {
                        cache.trait_to_topic[trait] = topic;
                    }
                }
            }
        }
    } );

    const cata_path summary_root = PATH_INFO::datadir_path() / "json" / "npcs" / "Backgrounds" /
                                   "Summaries_short";
    const std::filesystem::path summary_dir = summary_root.get_unrelative_path();
    std::error_code ec;
    if( !std::filesystem::exists( summary_dir, ec ) ) {
        return cache;
    }

    for( const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(
             summary_dir,
             ec ) ) {
        if( ec ) {
            break;
        }
        if( !entry.is_regular_file( ec ) ) {
            continue;
        }
        if( entry.path().extension() != ".txt" ) {
            continue;
        }
        const std::string filename = entry.path().filename().generic_u8string();
        read_from_file_optional( summary_root / filename, [&]( std::istream & data ) {
            std::string line;
            while( std::getline( data, line ) ) {
                line = trim_copy( line );
                if( line.empty() || line[0] == '#' ) {
                    continue;
                }
                std::vector<std::string> parts;
                std::string current;
                for( char c : line ) {
                    if( c == '|' ) {
                        parts.push_back( trim_copy( current ) );
                        current.clear();
                    } else {
                        current.push_back( c );
                    }
                }
                parts.push_back( trim_copy( current ) );
                if( parts.size() < 3 ) {
                    continue;
                }
                const std::string &id = parts[0];
                if( cache.summary_by_topic.count( id ) > 0 ) {
                    continue;
                }
                background_summary_entry entry_value;
                entry_value.background = normalize_summary_line( parts[1] );
                entry_value.expression = normalize_summary_line( parts[2] );
                if( parts.size() > 3 ) {
                    entry_value.source_tag = normalize_summary_line( parts[3] );
                }
                cache.summary_by_topic[id] = entry_value;
            }
        } );
    }

    return cache;
}

background_summary_entry get_background_summary_for( const npc &listener )
{
    background_summary_cache &cache = get_background_summaries();
    if( cache.trait_to_topic.empty() || cache.summary_by_topic.empty() ) {
        return {};
    }
    for( const trait_id &trait : listener.get_mutations( true, true ) ) {
        const std::string trait_name = trait.str();
        const auto topic_it = cache.trait_to_topic.find( trait_name );
        if( topic_it == cache.trait_to_topic.end() ) {
            continue;
        }
        const auto summary_it = cache.summary_by_topic.find( topic_it->second );
        if( summary_it == cache.summary_by_topic.end() ) {
            continue;
        }
        return summary_it->second;
    }
    return {};
}

bool is_action_token( const std::string &token )
{
    if( token.empty() ) {
        return false;
    }
    for( unsigned char c : token ) {
        if( !( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' ) ) {
            return false;
        }
    }
    return true;
}

const std::vector<std::string> &allowed_actions()
{
    static const std::vector<std::string> actions = {
        "wait_here",
        "follow_player",
        "equip_gun",
        "equip_melee",
        "equip_bow",
        "look_around",
        "idle"
    };
    return actions;
}

bool is_allowed_action( const std::string &token )
{
    for( const std::string &action : allowed_actions() ) {
        if( token == action ) {
            return true;
        }
    }
    return false;
}

llm_intent_action intent_action_from_token( const std::string &token )
{
    if( token == "wait_here" ) {
        return llm_intent_action::wait_here;
    }
    if( token == "follow_player" ) {
        return llm_intent_action::follow_player;
    }
    if( token == "equip_gun" ) {
        return llm_intent_action::equip_gun;
    }
    if( token == "equip_melee" ) {
        return llm_intent_action::equip_melee;
    }
    if( token == "equip_bow" ) {
        return llm_intent_action::equip_bow;
    }
    if( token == "idle" ) {
        return llm_intent_action::none;
    }
    return llm_intent_action::none;
}

bool parse_csv_payload( const std::string &csv, std::string &speech,
                        std::vector<std::string> &actions,
                        std::string &attack_target, std::string &error )
{
    actions.clear();
    speech.clear();
    attack_target.clear();
    std::vector<std::string> fields;
    std::string current;
    for( char c : csv ) {
        if( c == '|' ) {
            fields.push_back( trim_copy( current ) );
            current.clear();
        } else {
            current.push_back( c );
        }
    }
    fields.push_back( trim_copy( current ) );
    if( fields.size() < 2 ) {
        error = "CSV must include at least one action field separated by '|'.";
        return false;
    }
    if( fields.size() > 4 ) {
        error = "CSV has too many action fields.";
        return false;
    }
    speech = trim_copy( fields[0] );
    if( speech.empty() ) {
        error = "CSV speech field missing.";
        return false;
    }
    const size_t extra_sep = speech.find( '|' );
    if( extra_sep != std::string::npos ) {
        speech = trim_copy( speech.substr( 0, extra_sep ) );
        if( speech.empty() ) {
            error = "CSV speech field missing.";
            return false;
        }
    }
    auto push_action_token = [&]( std::string token ) -> bool {
        token = trim_copy( token );
        if( token.empty() )
        {
            error = "CSV action token is invalid.";
            return false;
        }
        if( token.size() >= 2 && token.front() == '"' && token.back() == '"' )
        {
            token = trim_copy( token.substr( 1, token.size() - 2 ) );
        }
        std::string token_lower = token;
        std::transform( token_lower.begin(), token_lower.end(), token_lower.begin(), []( unsigned char c )
        {
            return static_cast<char>( std::tolower( c ) );
        } );
        if( token_lower.rfind( "attack=", 0 ) == 0 )
        {
            const std::string target_raw = token_lower.substr( 7 );
            if( target_raw.empty() ) {
                error = "CSV attack target missing.";
                return false;
            }
            size_t end = 0;
            while( end < target_raw.size() ) {
                const unsigned char c = static_cast<unsigned char>( target_raw[end] );
                if( !( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' ) ) {
                    break;
                }
                ++end;
            }
            if( end == 0 ) {
                error = "CSV attack target is invalid.";
                return false;
            }
            if( !attack_target.empty() ) {
                error = "CSV attack target repeated.";
                return false;
            }
            attack_target = target_raw.substr( 0, end );
            return true;
        }
        if( !is_action_token( token_lower ) )
        {
            error = "CSV action token is invalid.";
            return false;
        }
        if( !is_allowed_action( token_lower ) )
        {
            if( !attack_target.empty() ) {
                return true;
            }
        }
        actions.push_back( token_lower );
        if( actions.size() > 3 )
        {
            error = "CSV has too many action tokens.";
            return false;
        }
        return true;
    };

    for( size_t idx = 1; idx < fields.size(); ++idx ) {
        std::string field = trim_copy( fields[idx] );
        if( field.empty() ) {
            error = "CSV action token is invalid.";
            return false;
        }
        std::string current_token;
        for( char c : field ) {
            if( std::isspace( static_cast<unsigned char>( c ) ) ) {
                if( !current_token.empty() ) {
                    if( !push_action_token( current_token ) ) {
                        return false;
                    }
                    current_token.clear();
                }
                continue;
            }
            current_token.push_back( c );
        }
        if( !current_token.empty() ) {
            if( !push_action_token( current_token ) ) {
                return false;
            }
        }
    }
    if( actions.empty() && !attack_target.empty() ) {
        actions.push_back( "idle" );
    }
    if( actions.empty() ) {
        error = "CSV must include at least one action field.";
        return false;
    }
    return true;
}

std::string extract_attack_target_hint( const std::string &text )
{
    std::string lowered = text;
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    const std::string needle = "attack=";
    const size_t pos = lowered.find( needle );
    if( pos == std::string::npos ) {
        return {};
    }
    size_t start = pos + needle.size();
    size_t end = start;
    while( end < lowered.size() ) {
        const unsigned char c = static_cast<unsigned char>( lowered[end] );
        if( !( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' ) ) {
            break;
        }
        ++end;
    }
    if( end == start ) {
        return {};
    }
    return lowered.substr( start, end - start );
}

struct look_around_item_entry {
    std::string name;
    int quantity = 0;
    int min_distance = 0;
};

std::string xml_escape( const std::string &text )
{
    std::string out;
    out.reserve( text.size() );
    for( char c : text ) {
        switch( c ) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out.push_back( c );
                break;
        }
    }
    return out;
}

std::string strip_xml_tags( const std::string &text )
{
    std::string out;
    out.reserve( text.size() );
    bool in_tag = false;
    for( char c : text ) {
        if( c == '<' ) {
            in_tag = true;
            continue;
        }
        if( c == '>' ) {
            in_tag = false;
            continue;
        }
        if( !in_tag ) {
            out.push_back( c );
        }
    }
    return out;
}

std::vector<look_around_item_entry> collect_look_around_items( npc &listener, int radius,
        size_t max_entries )
{
    map &here = get_map();
    std::unordered_map<std::string, look_around_item_entry> entries_by_name;

    for( const tripoint_bub_ms &p : closest_points_first( listener.pos_bub(), radius ) ) {
        if( !here.sees_some_items( p, listener ) || !listener.sees( here, p ) ) {
            continue;
        }
        const int dist = rl_dist( listener.pos_bub(), p );
        for( const item &it : here.i_at( p ) ) {
            const std::string name = it.tname( 1, false );
            look_around_item_entry &entry = entries_by_name[name];
            if( entry.name.empty() ) {
                entry.name = name;
                entry.min_distance = dist;
            }
            entry.quantity += it.count();
            entry.min_distance = std::min( entry.min_distance, dist );
        }
        const optional_vpart_position vp = here.veh_at( p );
        if( !vp ) {
            continue;
        }
        const std::optional<vpart_reference> cargo = vp.cargo();
        if( !cargo || cargo->has_feature( "LOCKED" ) ) {
            continue;
        }
        for( const item &it : cargo->items() ) {
            const std::string name = it.tname( 1, false );
            look_around_item_entry &entry = entries_by_name[name];
            if( entry.name.empty() ) {
                entry.name = name;
                entry.min_distance = dist;
            }
            entry.quantity += it.count();
            entry.min_distance = std::min( entry.min_distance, dist );
        }
    }

    std::vector<look_around_item_entry> entries;
    entries.reserve( entries_by_name.size() );
    for( auto &entry : entries_by_name ) {
        entries.push_back( std::move( entry.second ) );
    }
    std::sort( entries.begin(), entries.end(),
               []( const look_around_item_entry &lhs, const look_around_item_entry &rhs ) {
        if( lhs.min_distance != rhs.min_distance ) {
            return lhs.min_distance < rhs.min_distance;
        }
        return lhs.name < rhs.name;
    } );
    if( entries.size() > max_entries ) {
        entries.resize( max_entries );
    }
    return entries;
}

std::vector<std::string> collect_compatible_ammo( npc &listener )
{
    std::set<std::string> ammo_names;
    listener.visit_items( [&]( item * it, item * ) {
        if( it == nullptr || !it->is_gun() ) {
            return VisitResponse::NEXT;
        }
        for( const ammotype &ammo_type : it->ammo_types() ) {
            ammo_names.insert( ammo_type->name() );
        }
        return VisitResponse::NEXT;
    } );
    return std::vector<std::string>( ammo_names.begin(), ammo_names.end() );
}

std::vector<std::string> collect_compatible_magazines( npc &listener )
{
    std::set<std::string> mag_names;
    listener.visit_items( [&]( item * it, item * ) {
        if( it == nullptr || !it->is_gun() ) {
            return VisitResponse::NEXT;
        }
        for( const itype_id &mag_id : it->magazine_compatible() ) {
            mag_names.insert( item::tname( mag_id, 1 ) );
        }
        return VisitResponse::NEXT;
    } );
    return std::vector<std::string>( mag_names.begin(), mag_names.end() );
}

std::string build_look_around_prompt( const std::string &player_utterance,
                                      const std::vector<std::string> &ammo,
                                      const std::vector<std::string> &magazines,
                                      const std::vector<look_around_item_entry> &items )
{
    std::ostringstream out;
    out << "<System>";
    out << "You select up to three items from the list for the NPC to pick up.";
    out << "Return up to three items exactly from the list, comma-separated.";
    out << "Use exact item names from the list only.";
    out << "</System>\n";
    out << "<UserUtterance>" << xml_escape( player_utterance ) << "</UserUtterance>\n";
    out << "<CompatibleAmmo>\n";
    for( const std::string &entry : ammo ) {
        out << "  <Ammo name=\"" << xml_escape( entry ) << "\"/>\n";
    }
    out << "</CompatibleAmmo>\n";
    out << "<CompatibleMagazines>\n";
    for( const std::string &entry : magazines ) {
        out << "  <Magazine name=\"" << xml_escape( entry ) << "\"/>\n";
    }
    out << "</CompatibleMagazines>\n";
    out << "<Items>\n";
    for( const look_around_item_entry &entry : items ) {
        out << "  <Item name=\"" << xml_escape( entry.name ) << "\" qty=\""
            << entry.quantity << "\"/>\n";
    }
    out << "</Items>\n";
    return out.str();
}

std::string normalize_csv_separators( const std::string &csv )
{
    if( csv.find( '|' ) != std::string::npos ) {
        return csv;
    }
    std::string out;
    out.reserve( csv.size() );
    bool last_sep = false;
    for( char c : csv ) {
        if( c == '+' ) {
            if( !last_sep ) {
                out.push_back( '|' );
                last_sep = true;
            }
            continue;
        }
        last_sep = false;
        out.push_back( c );
    }
    return out;
}

std::vector<std::string> parse_look_around_response( const std::string &text,
        const std::unordered_map<std::string, std::string> &allowed )
{
    std::string cleaned = trim_copy( strip_xml_tags( text ) );
    if( cleaned.empty() ) {
        return {};
    }
    std::replace( cleaned.begin(), cleaned.end(), '\n', ',' );
    std::vector<std::string> results;
    size_t start = 0;
    while( start < cleaned.size() ) {
        size_t end = cleaned.find( ',', start );
        if( end == std::string::npos ) {
            end = cleaned.size();
        }
        std::string token = trim_copy( cleaned.substr( start, end - start ) );
        start = end + 1;
        if( token.empty() ) {
            continue;
        }
        std::string lowered = lower_copy( token );
        auto it = allowed.find( lowered );
        if( it == allowed.end() ) {
            continue;
        }
        const std::string &normalized = it->second;
        if( std::find( results.begin(), results.end(), normalized ) == results.end() ) {
            results.push_back( normalized );
            if( results.size() >= 3 ) {
                break;
            }
        }
    }
    return results;
}

std::string strip_wrapping_quotes( const std::string &text )
{
    std::string trimmed = trim_copy( text );
    if( trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"' ) {
        return trim_copy( trimmed.substr( 1, trimmed.size() - 2 ) );
    }
    return trimmed;
}

std::string sanitize_llm_csv( const std::string &text )
{
    std::string out = strip_wrapping_quotes( text );
    out.erase( std::remove( out.begin(), out.end(), '\\' ), out.end() );
    return trim_copy( out );
}

std::string extract_speech_field( const std::string &csv_text )
{
    const size_t sep = csv_text.find( '|' );
    const std::string raw = sep == std::string::npos ? csv_text : csv_text.substr( 0, sep );
    return sanitize_llm_csv( raw );
}

std::string strip_speaker_prefix( const std::string &text )
{
    std::string trimmed = trim_copy( text );
    const size_t colon = trimmed.find( ':' );
    if( colon != std::string::npos && colon < 40 ) {
        size_t start = colon + 1;
        while( start < trimmed.size() && std::isspace( static_cast<unsigned char>( trimmed[start] ) ) ) {
            ++start;
        }
        return trim_copy( trimmed.substr( start ) );
    }
    return trimmed;
}

bool extract_lenient_csv( const std::string &csv, std::string &speech,
                          std::vector<std::string> &actions )
{
    speech.clear();
    actions.clear();
    std::string lowered = csv;
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    const size_t sep = csv.find( '|' );
    if( sep != std::string::npos ) {
        speech = trim_copy( csv.substr( 0, sep ) );
    } else {
        const size_t first_quote = csv.find( '"' );
        if( first_quote == std::string::npos ) {
            return false;
        }
        size_t pos = first_quote + 1;
        while( pos < csv.size() ) {
            char c = csv[pos];
            if( c == '"' ) {
                if( pos + 1 < csv.size() && csv[pos + 1] == '"' ) {
                    speech.push_back( '"' );
                    pos += 2;
                    continue;
                }
                ++pos;
                break;
            }
            speech.push_back( c );
            ++pos;
        }
    }
    if( speech.empty() ) {
        return false;
    }
    auto is_boundary = []( char c ) {
        return !std::isalnum( static_cast<unsigned char>( c ) ) && c != '_';
    };
    for( const std::string &action : allowed_actions() ) {
        size_t start = lowered.find( action );
        while( start != std::string::npos ) {
            const bool left_ok = start == 0 || is_boundary( lowered[start - 1] );
            const size_t end = start + action.size();
            const bool right_ok = end >= lowered.size() || is_boundary( lowered[end] );
            if( left_ok && right_ok ) {
                actions.push_back( action );
                return true;
            }
            start = lowered.find( action, end );
        }
    }
    actions.push_back( "idle" );
    return true;
}

std::string extract_csv_from_text( const std::string &text )
{
    std::istringstream lines( text );
    std::string cleaned;
    std::string line;
    while( std::getline( lines, line ) ) {
        const std::string trimmed = trim_copy( line );
        if( trimmed.rfind( "```", 0 ) == 0 ) {
            continue;
        }
        if( !cleaned.empty() ) {
            cleaned.push_back( '\n' );
        }
        cleaned += line;
    }
    return trim_copy( cleaned );
}

struct creature_snapshot {
    const Creature *critter = nullptr;
    int distance = 0;
};

std::vector<creature_snapshot> filter_visible( const npc &listener,
        Creature::Attitude attitude, int range )
{
    std::vector<creature_snapshot> out;
    const std::vector<Creature *> visible = listener.get_visible_creatures( range );
    for( Creature *critter : visible ) {
        if( critter == nullptr || critter == &listener ) {
            continue;
        }
        if( listener.attitude_to( *critter ) != attitude ) {
            continue;
        }
        out.push_back( { critter, rl_dist( listener.pos_bub(), critter->pos_bub() ) } );
    }
    std::sort( out.begin(), out.end(), []( const creature_snapshot & lhs,
    const creature_snapshot & rhs ) {
        return lhs.distance < rhs.distance;
    } );
    return out;
}

float threat_score_for( npc &listener, const Creature &critter, int distance )
{
    if( const monster *mon = critter.as_monster() ) {
        return listener.evaluate_monster( *mon, distance );
    }
    if( const Character *character = critter.as_character() ) {
        const bool my_gun = listener.get_wielded_item() && listener.get_wielded_item()->is_gun();
        const bool enemy = listener.attitude_to( *character ) == Creature::Attitude::HOSTILE;
        return listener.evaluate_character( *character, my_gun, enemy );
    }
    return 0.0f;
}

std::string build_snapshot_legend()
{
    return string_format(
               "- ... open area\n"
               "0 ... obstructive area (movement speed > 100)\n"
               "6 ... obstructed area\n"
               "[a - z] ... creature\n"
               "[A - Z] ... obstructed creature\n"
               "| ... You (NPC)\n" );
}

std::string build_map_legend( const std::vector<std::pair<char, std::string>> &entries )
{
    std::string out;
    for( const auto &entry : entries ) {
        if( !out.empty() ) {
            out.push_back( '\n' );
        }
        out += string_format( "%c ... %s", entry.first, entry.second );
    }
    return out;
}

struct map_snapshot {
    std::string map;
    std::string legend;
};

map_snapshot build_ascii_map_snapshot( npc &listener )
{
    map &here = get_map();
    const tripoint_bub_ms player_pos = get_player_character().pos_bub();
    const tripoint_bub_ms center = listener.pos_bub();
    static constexpr int radius = 20;
    std::string out_map;
    out_map.reserve( ( radius * 2 + 1 ) * ( radius * 2 + 2 ) );
    std::vector<std::pair<char, std::string>> legend_entries;
    std::unordered_map<const Creature *, char> letter_map;
    std::map<char, weak_ptr_fast<Creature>> legend_targets;
    const bool player_in_map = std::abs( player_pos.x() - center.x() ) <= radius &&
                               std::abs( player_pos.y() - center.y() ) <= radius &&
                               player_pos.z() == center.z();
    const bool player_letter_active = player_in_map && player_pos != center;
    char next_letter = player_letter_active ? 'b' : 'a';
    for( int dy = -radius; dy <= radius; ++dy ) {
        for( int dx = -radius; dx <= radius; ++dx ) {
            const tripoint_bub_ms p( center.x() + dx, center.y() + dy, center.z() );
            char glyph = '-';
            if( p == center ) {
                glyph = '|';
            } else if( !here.inbounds( p ) ) {
                glyph = '6';
            } else {
                const int cost = here.move_cost( p );
                const int scaled_cost = cost * 50;
                if( Creature *critter = get_creature_tracker().creature_at( p ) ) {
                    if( critter->is_avatar() && player_letter_active ) {
                        auto found = letter_map.find( critter );
                        if( found == letter_map.end() ) {
                            const char base_letter = 'a';
                            const char letter = cost > 100 ||
                                                cost <= 0 ? static_cast<char>( std::toupper( base_letter ) ) : base_letter;
                            letter_map.emplace( critter, letter );
                            legend_entries.emplace_back( letter, "player" );
                            legend_targets[letter] = g->shared_from( *critter );
                            glyph = letter;
                        } else {
                            glyph = found->second;
                        }
                    } else if( critter != &listener ) {
                        auto found = letter_map.find( critter );
                        if( found == letter_map.end() ) {
                            if( next_letter <= 'z' ) {
                                const char base_letter = next_letter;
                                const char letter = cost > 100 ||
                                                    cost <= 0 ? static_cast<char>( std::toupper( base_letter ) ) : base_letter;
                                letter_map.emplace( critter, letter );
                                legend_entries.emplace_back( letter,
                                                             strip_leading_article( sanitize_text( critter->disp_name() ) ) );
                                legend_targets[letter] = g->shared_from( *critter );
                                glyph = letter;
                                ++next_letter;
                            } else {
                                glyph = cost > 100 || cost <= 0 ? 'A' : 'a';
                            }
                        } else {
                            glyph = found->second;
                        }
                    }
                } else if( cost <= 0 ) {
                    glyph = '6';
                } else if( scaled_cost > 100 ) {
                    glyph = '0';
                }
            }
            out_map.push_back( glyph );
        }
        out_map.push_back( '\n' );
    }
    map_snapshot out;
    out.map = std::move( out_map );
    out.legend = build_map_legend( legend_entries );
    listener.set_llm_intent_legend_map( std::move( legend_targets ) );
    return out;
}

std::string build_snapshot_json( npc &listener, const std::string &player_utterance,
                                 const std::string &request_id )
{
    static constexpr int visible_range = 12;
    static constexpr size_t max_creatures = 5;
    static constexpr size_t max_effects = 6;
    static constexpr size_t max_items = 3;
    auto scale_unipolar = []( double value, double max_value ) -> int {
        if( max_value <= 0.0 )
        {
            return 0;
        }
        const double clamped = std::clamp( value, 0.0, max_value );
        return static_cast<int>( std::round( ( clamped / max_value ) * 10.0 ) );
    };
    auto scale_bipolar = []( double value, double min_value, double max_value ) -> int {
        if( max_value <= min_value )
        {
            return 0;
        }
        const double clamped = std::clamp( value, min_value, max_value );
        const double ratio = ( clamped - min_value ) / ( max_value - min_value );
        return static_cast<int>( std::round( ratio * 10.0 ) );
    };

    std::ostringstream out;
    out << "SITUATION\n";
    out << "id: " << request_id << "\n";
    out << "player_name: " << sanitize_text( get_player_character().get_name() ) << "\n";
    out << "player_utterance: " << sanitize_text( player_utterance ) << "\n\n";
    out << "your_name: " << sanitize_text( listener.get_name() ) << "\n";
    const std::string profession = sanitize_text( listener.disp_profession() );
    if( !profession.empty() ) {
        out << "your_profession: " << profession << "\n";
    }
    const background_summary_entry background_summary = get_background_summary_for( listener );
    if( !background_summary.source_tag.empty() ) {
        std::string bg_line = background_summary.source_tag;
        if( bg_line.rfind( "bg_", 0 ) == 0 ) {
            bg_line.erase( 0, 3 );
        }
        out << "your_background: " << bg_line << "\n";
    }
    if( !background_summary.background.empty() ) {
        out << "your_tone: " << background_summary.background << "\n";
    }
    if( !background_summary.expression.empty() ) {
        out << "your_example_expression: " << background_summary.expression << "\n";
    }
    const int morale_scaled = scale_bipolar( listener.get_morale_level(), -100.0, 100.0 );
    const int hunger_scaled = scale_unipolar( listener.get_hunger(), 300.0 );
    const int thirst_scaled = scale_unipolar( listener.get_thirst(), 300.0 );
    const int pain_scaled = scale_unipolar( listener.get_pain(), 100.0 );
    const int max_sleepiness = static_cast<int>( sleepiness_levels::MASSIVE_SLEEPINESS );
    const int sleepiness_scaled = scale_unipolar( listener.get_sleepiness(), max_sleepiness );
    const int hp_scaled = scale_unipolar( listener.hp_percentage(), 100.0 );
    double stamina_percent = 0.0;
    if( listener.get_stamina_max() > 0 ) {
        stamina_percent = static_cast<double>( listener.get_stamina() ) * 100.0 /
                          static_cast<double>( listener.get_stamina_max() );
    }
    const int stamina_scaled = scale_unipolar( stamina_percent, 100.0 );

    out << "your_state[0-10]: ";
    out << "morale=" << morale_scaled;
    out << " hunger=" << hunger_scaled;
    out << " thirst=" << thirst_scaled;
    out << " pain=" << pain_scaled;
    out << " stamina=" << stamina_scaled;
    out << " sleepiness=" << sleepiness_scaled;
    out << " hp_percent=" << hp_scaled;
    out << " effects=[";
    size_t effect_count = 0;
    for( const std::reference_wrapper<const effect> &eff_ref : listener.get_effects() ) {
        const effect &eff = eff_ref.get();
        if( effect_count > 0 ) {
            out << " ";
        }
        out << eff.get_id().str() << ":" << eff.get_intensity();
        if( ++effect_count >= max_effects ) {
            break;
        }
    }
    out << "]\n";

    const int danger_scaled = scale_unipolar( listener.danger_assessment(),
                              static_cast<double>( NPC_CHARACTER_DANGER_MAX ) );
    const int panic_scaled = scale_unipolar( listener.mem_combat.panic, 20.0 );
    const int confidence_scaled = scale_unipolar( listener.mem_combat.my_health, 1.0 );
    out << "your_emotions[0-10]: ";
    out << "danger_assessment=" << danger_scaled;
    out << " panic=" << panic_scaled;
    out << " confidence=" << confidence_scaled << "\n";

    out << "your_personality[0-10]: ";
    out << "aggression=" << scale_bipolar( listener.personality.aggression, -10.0, 10.0 );
    out << " bravery=" << scale_bipolar( listener.personality.bravery, -10.0, 10.0 );
    out << " collector=" << scale_bipolar( listener.personality.collector, -10.0, 10.0 );
    out << " altruism=" << scale_bipolar( listener.personality.altruism, -10.0, 10.0 ) << "\n";

    out << "your_opinion_of_player[0-10]: ";
    out << "trust=" << scale_bipolar( listener.op_of_u.trust, -10.0, 10.0 );
    out << " intimidation=" << scale_bipolar( listener.op_of_u.fear, -10.0, 10.0 );
    out << " respect=" << scale_bipolar( listener.op_of_u.value, -10.0, 10.0 );
    out << " anger=" << scale_bipolar( listener.op_of_u.anger, -10.0, 10.0 ) << "\n\n";

    const std::vector<creature_snapshot> hostile = filter_visible( listener,
            Creature::Attitude::HOSTILE,
            visible_range );
    if( hostile.empty() ) {
        out << "threats: (none)\n";
    } else {
        out << "threats: ";
        size_t count = 0;
        for( const creature_snapshot &entry : hostile ) {
            if( entry.critter == nullptr ) {
                continue;
            }
            if( count > 0 ) {
                out << ", ";
            }
            const std::string name = strip_leading_article( sanitize_text( entry.critter->disp_name() ) );
            out << name << " threat_score[0-100]=";
            out << threat_score_for( listener, *entry.critter, entry.distance );
            if( ++count >= max_creatures ) {
                break;
            }
        }
        out << "\n";
    }

    const std::vector<creature_snapshot> friendly = filter_visible( listener,
            Creature::Attitude::FRIENDLY,
            visible_range );
    if( friendly.empty() ) {
        out << "friendlies: (none)\n";
    } else {
        out << "friendlies: ";
        size_t count = 0;
        for( const creature_snapshot &entry : friendly ) {
            if( entry.critter == nullptr ) {
                continue;
            }
            if( count > 0 ) {
                out << ", ";
            }
            std::string name = sanitize_text( entry.critter->disp_name() );
            if( entry.critter->is_avatar() ) {
                name = "player";
            }
            name = strip_leading_article( name );
            out << name;
            if( ++count >= max_creatures ) {
                break;
            }
        }
        out << "\n";
    }

    out << "\n";

    item_location wielded = listener.get_wielded_item();

    out << "inventory: ";
    if( wielded ) {
        const item &weapon = *wielded;
        out << "wielded=\"" << sanitize_text( weapon.tname() ) << "\"";
    } else {
        out << "wielded=none";
    }
    out << "\n";

    std::vector<std::string> usable_items;
    std::vector<std::string> combat_guns;
    std::vector<std::string> combat_melee;
    bool bandage_possible = false;
    const auto format_gun_label = []( const item & gun ) -> std::string {
        std::string name = sanitize_text( gun.tname() );
        int capacity = 0;
        itype_id ammo_id = gun.ammo_current();
        if( ammo_id.is_null() )
        {
            ammo_id = gun.ammo_default();
        }
        if( !ammo_id.is_null() )
        {
            const itype *ammo_type = item::find_type( ammo_id );
            if( ammo_type && ammo_type->ammo ) {
                capacity = gun.ammo_capacity( ammo_type->ammo->type );
            }
        }
        const int ammo = gun.ammo_remaining();
        if( capacity > 0 )
        {
            name += " (" + std::to_string( ammo ) + "/" + std::to_string( capacity ) + ")";
        } else if( ammo > 0 )
        {
            name += " (" + std::to_string( ammo ) + ")";
        }
        return name;
    };
    listener.visit_items( [&]( item * it, item * ) {
        if( it == nullptr ) {
            return VisitResponse::NEXT;
        }
        if( usable_items.size() < max_items &&
            ( it->is_tool() || it->is_medication() || it->is_medical_tool() ) ) {
            usable_items.push_back( sanitize_text( it->tname() ) );
        }
        if( it->is_gun() ) {
            if( combat_guns.size() < max_items ) {
                combat_guns.push_back( format_gun_label( *it ) );
            }
        } else if( it->is_melee() ) {
            if( combat_melee.size() < max_items ) {
                combat_melee.push_back( sanitize_text( it->tname() ) );
            }
        }
        if( it->is_medication() || it->is_medical_tool() ) {
            bandage_possible = true;
        }
        if( usable_items.size() >= max_items &&
            combat_guns.size() >= max_items &&
            combat_melee.size() >= max_items ) {
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );

    out << "inventory_usable: [";
    for( size_t i = 0; i < usable_items.size(); ++i ) {
        if( i > 0 ) {
            out << ", ";
        }
        out << usable_items[i];
    }
    out << "]\n";

    std::vector<std::string> combat_items;
    combat_items.reserve( max_items );
    for( const std::string &gun : combat_guns ) {
        if( combat_items.size() >= max_items ) {
            break;
        }
        combat_items.push_back( gun );
    }
    for( const std::string &melee : combat_melee ) {
        if( combat_items.size() >= max_items ) {
            break;
        }
        combat_items.push_back( melee );
    }

    out << "inventory_combat: [";
    for( size_t i = 0; i < combat_items.size(); ++i ) {
        if( i > 0 ) {
            out << ", ";
        }
        out << combat_items[i];
    }
    out << "]\n";
    out << "bandage_possible: " << ( bandage_possible ? "true" : "false" ) << "\n\n";

    const map_snapshot map_data = build_ascii_map_snapshot( listener );
    out << "legend:\n" << build_snapshot_legend();
    out << "map_legend:\n";
    if( map_data.legend.empty() ) {
        out << "(none)\n";
    } else {
        out << map_data.legend << "\n";
    }
    out << "map:\n" << map_data.map;
    return out.str();
}

std::string build_prompt( const std::string &npc_name, const std::string &player_utterance,
                          const std::string &snapshot )
{
    ( void )npc_name;
    ( void )player_utterance;
    std::string action_list;
    for( const std::string &action : allowed_actions() ) {
        if( !action_list.empty() ) {
            action_list += ", ";
        }
        action_list += action;
    }
    std::string action_list_with_target = action_list;
    if( !action_list_with_target.empty() ) {
        action_list_with_target += ", ";
    }
    action_list_with_target += "attack=<target>";
    return string_format(
               "Situation:\n%s\n"
               "<System>"
               "You are controlling a human survivor NPC in a cataclysmic world, exhausted, armed, and trying not to die."
               "Return a single line only, with correct syntax, to be parsed by the game."
               "This line has two to four fields separated by ‘|’ :\n"
               "<Field 1>"
               "The first field is an answer to player_utterance."
               "You have decided to team up with the player for now, and must answer as the NPC."
               "Stick to your role, with your emotions and opinions."
               "Use a dry tone, with swear words, fit for a zombie apocalypse."
               "</Field 1>\n"
               "<Fields 2-4>"
               "Write 1-3 of the following allowed actions:"
               "%s\n"
               "<Allowed actions>"
               "wait_here to stay put, keep watch, wait, stand.\n"
               "follow_player to walk behind, follow, run.\n"
               "equip_gun to equip gun, rifle, thrower, get ready to shoot.\n"
               "equip_melee to equip melee, get ready to bash, cut, kick, stab.\n"
               "equip_bow to use bow, crossbow, stealth.\n"
               "look_around to request nearby item selection for pickup.\n"
               "attack=<target> to attack a target from your map.\n"
               "idle if none of the above.\n"
               "</Allowed actions>\n"
               "</Fields 2-4>\n"
               "Print only Fields 1-4, separated by | ."
               "If you break this format, you have failed."
               "Output a single line with an answer and actions from the allowed list, in fields separated by ‘|’ and no additional text.\n"
               "<Example Output 1>"
               "Blow me.|idle"
               "</Example Output 1>\n"
               "<Example Output 2>"
               "Lets put those fucks in the ground.|equip_melee|attack=zombie"
               "</Example Output 2>\n"
               "<Example Output 3>"
               "Providing cover!|wait_here|equip_gun"
               "</Example Output 3>\n"
               "<Example Output 4>"
               "Lets get some dinner!|equip_gun|attack=chicken"
               "</Example Output 4>\n"
               "<Example Output 5>"
               "Don't worry, I'm ready to kick some teeth in.|equip_melee"
               "</Example Output 5>\n"
               "<Example Output 6>"
               "Locked and loaded.|equip_gun"
               "</Example Output 6>\n"
               "</System>\n",
               snapshot, action_list_with_target );
}

[[maybe_unused]] std::string request_to_json( const llm_intent_request &request )
{
    std::ostringstream out;
    JsonOut jsout( out );
    jsout.start_object();
    jsout.member( "request_id", request.request_id );
    jsout.member( "prompt", request.prompt );
    jsout.member( "snapshot", request.snapshot );
    jsout.member( "max_tokens", request.max_tokens );
    jsout.member( "temperature", request.temperature );
    jsout.member( "top_p", request.top_p );
    jsout.member( "repetition_penalty", request.repetition_penalty );
    jsout.end_object();
    return out.str();
}

[[maybe_unused]] std::string read_log_tail( const std::filesystem::path &path,
        std::streamoff max_bytes )
{
    std::ifstream in( path, std::ios::binary );
    if( !in ) {
        return {};
    }
    in.seekg( 0, std::ios::end );
    std::streamoff size = in.tellg();
    if( size <= 0 ) {
        return {};
    }
    std::streamoff start = size > max_bytes ? size - max_bytes : 0;
    in.seekg( start, std::ios::beg );
    std::string data( static_cast<size_t>( size - start ), '\0' );
    in.read( data.data(), data.size() );
    return data;
}

bool should_attempt_parse( const std::string &line )
{
    const auto pos = line.find_first_not_of( " \t\r\n" );
    if( pos == std::string::npos ) {
        return false;
    }
    return line[pos] == '{';
}

[[maybe_unused]] std::optional<llm_intent_response> response_from_json( const std::string &line,
        const llm_intent_request &request )
{
    if( !should_attempt_parse( line ) ) {
        return std::nullopt;
    }
    llm_intent_response response;
    response.request_id = request.request_id;
    response.npc_id = request.npc_id;
    response.npc_name = request.npc_name;
    response.raw = line;
    try {
        std::istringstream in( line );
        TextJsonIn jsin( in );
        TextJsonObject obj = jsin.get_object();
        obj.allow_omitted_members();
        const std::string resp_id = obj.get_string( "request_id", "" );
        if( resp_id.empty() || resp_id != request.request_id ) {
            return std::nullopt;
        }
        response.ok = obj.get_bool( "ok", false );
        response.text = obj.get_string( "text", "" );
        response.error = obj.get_string( "error", "" );
    } catch( const std::exception &err ) {
        return std::nullopt;
    }
    return response;
}

[[maybe_unused]] std::filesystem::path resolve_path( const std::string &path )
{
    if( path.empty() ) {
        return std::filesystem::path();
    }
    std::filesystem::path p = std::filesystem::u8path( path );
    if( p.is_relative() ) {
        p = std::filesystem::path( PATH_INFO::base_path() ) / p;
    }
    return p;
}

[[maybe_unused]] runner_config current_runner_config()
{
    static constexpr int default_max_tokens = 20000;
    static constexpr int default_max_prompt_len = 4096;
    runner_config cfg;
    cfg.python_path = get_option<std::string>( "LLM_INTENT_PYTHON" );
    cfg.runner_path = "tools/llm_runner/runner.py";
    cfg.model_dir = get_option<std::string>( "LLM_INTENT_MODEL_DIR" );
    cfg.backend = get_option<std::string>( "LLM_INTENT_BACKEND" );
    cfg.device = get_option<std::string>( "LLM_INTENT_DEVICE" );
    cfg.use_api = get_option<bool>( "LLM_INTENT_USE_API" );
    cfg.api_key_env = get_option<std::string>( "LLM_INTENT_API_KEY_ENV" );
    cfg.api_provider = get_option<std::string>( "LLM_INTENT_API_PROVIDER" );
    cfg.api_model = get_option<std::string>( "LLM_INTENT_API_MODEL" );
    cfg.max_tokens = default_max_tokens;
    cfg.max_prompt_len = get_option<int>( "LLM_INTENT_MAX_PROMPT_LEN" );
    if( cfg.max_prompt_len <= 0 ) {
        cfg.max_prompt_len = default_max_prompt_len;
    }
    cfg.force_npu = get_option<bool>( "LLM_INTENT_FORCE_NPU" );
    return cfg;
}

#if defined(_WIN32)
std::string quote_windows_arg( const std::string &arg )
{
    if( arg.empty() ) {
        return "\"\"";
    }
    if( arg.find_first_of( " \t\"" ) == std::string::npos ) {
        return arg;
    }
    std::string out = "\"";
    for( char c : arg ) {
        if( c == '"' ) {
            out += "\\\"";
        } else {
            out += c;
        }
    }
    out += "\"";
    return out;
}

class llm_intent_runner_process
{
    public:
        ~llm_intent_runner_process() {
            shutdown();
        }

        bool ensure_running( const runner_config &config, std::string &error ) {
            if( running && config == active_config ) {
                return true;
            }
            shutdown();
            return start( config, error );
        }

        bool send_request( const llm_intent_request &request, std::string &response_line,
                           std::string &error,
                           std::chrono::milliseconds timeout ) {
            std::string payload = request_to_json( request );
            payload.push_back( '\n' );
            if( !write_all( payload, error ) ) {
                return false;
            }
            std::chrono::milliseconds effective_timeout = timeout;
            if( !warm && timeout.count() > 0 ) {
                static constexpr auto startup_grace = std::chrono::milliseconds( 120000 );
                if( effective_timeout < startup_grace ) {
                    effective_timeout = startup_grace;
                }
            }
            const bool ok = read_response_for_request( request, response_line, effective_timeout, error );
            if( ok ) {
                warm = true;
            }
            return ok;
        }

        void terminate() {
            if( !running ) {
                return;
            }
            if( child_process ) {
                TerminateProcess( child_process, 1 );
            }
            close_handles();
        }

    private:
        bool running = false;
        bool warm = false;
        runner_config active_config;
        HANDLE child_process = nullptr;
        HANDLE child_thread = nullptr;
        HANDLE stdin_write = nullptr;
        HANDLE stdout_read = nullptr;
        std::string stdout_buffer;
        std::filesystem::path runner_log_path;

        bool start( const runner_config &config, std::string &error ) {
            const std::string backend = lower_copy( config.backend );
            const bool api_configured = !config.api_provider.empty() && !config.api_model.empty();
            const bool use_api_mode = config.use_api || backend == "api";
            const bool auto_backend = backend == "auto";
            const bool needs_model = !use_api_mode && !( auto_backend && api_configured );

            if( config.python_path.empty() || config.runner_path.empty() ||
                ( config.model_dir.empty() && needs_model ) ) {
                error = "LLM runner configuration is incomplete.";
                return false;
            }

            std::filesystem::path python_path = resolve_path( config.python_path );
            std::filesystem::path runner_path = resolve_path( config.runner_path );
            std::filesystem::path model_dir = resolve_path( config.model_dir );
            std::filesystem::path log_path = PATH_INFO::config_dir_path().get_unrelative_path() /
                                             "llm_intent_runner.log";
            assure_dir_exist( PATH_INFO::config_dir() );

            std::vector<std::string> args;
            args.push_back( python_path.string() );
            args.push_back( runner_path.string() );
            if( !backend.empty() ) {
                args.push_back( "--backend" );
                args.push_back( backend );
            }
            if( use_api_mode ) {
                args.push_back( "--use-api" );
                args.push_back( "--api-provider" );
                args.push_back( config.api_provider );
                args.push_back( "--api-model" );
                args.push_back( config.api_model );
                if( !config.api_key_env.empty() ) {
                    args.push_back( "--api-key-env" );
                    args.push_back( config.api_key_env );
                }
                args.push_back( "--max-tokens" );
                args.push_back( std::to_string( config.max_tokens ) );
            } else if( !config.model_dir.empty() ) {
                std::filesystem::path cache_dir = model_dir / ".ov_cache";
                args.push_back( "--model-dir" );
                args.push_back( model_dir.string() );
                args.push_back( "--device" );
                args.push_back( config.device.empty() ? "AUTO" : config.device );
                args.push_back( "--max-tokens" );
                args.push_back( std::to_string( config.max_tokens ) );
                args.push_back( "--max-prompt-len" );
                args.push_back( std::to_string( config.max_prompt_len ) );
                args.push_back( "--cache-dir" );
                args.push_back( cache_dir.string() );
                if( config.force_npu ) {
                    args.push_back( "--force-npu" );
                }
            }
            if( !use_api_mode && auto_backend && api_configured ) {
                args.push_back( "--api-provider" );
                args.push_back( config.api_provider );
                args.push_back( "--api-model" );
                args.push_back( config.api_model );
                if( !config.api_key_env.empty() ) {
                    args.push_back( "--api-key-env" );
                    args.push_back( config.api_key_env );
                }
            }
            args.push_back( "--log-file" );
            args.push_back( log_path.string() );

            std::string cmdline;
            for( const std::string &arg : args ) {
                if( !cmdline.empty() ) {
                    cmdline.push_back( ' ' );
                }
                cmdline += quote_windows_arg( arg );
            }

            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof( SECURITY_ATTRIBUTES );
            sa.lpSecurityDescriptor = nullptr;
            sa.bInheritHandle = TRUE;

            HANDLE stdout_write = nullptr;
            HANDLE stdin_read = nullptr;
            if( !CreatePipe( &stdout_read, &stdout_write, &sa, 0 ) ) {
                error = "Failed to create stdout pipe.";
                return false;
            }
            if( !SetHandleInformation( stdout_read, HANDLE_FLAG_INHERIT, 0 ) ) {
                error = "Failed to set stdout pipe handle.";
                CloseHandle( stdout_read );
                CloseHandle( stdout_write );
                return false;
            }
            if( !CreatePipe( &stdin_read, &stdin_write, &sa, 0 ) ) {
                error = "Failed to create stdin pipe.";
                CloseHandle( stdout_read );
                CloseHandle( stdout_write );
                return false;
            }
            if( !SetHandleInformation( stdin_write, HANDLE_FLAG_INHERIT, 0 ) ) {
                error = "Failed to set stdin pipe handle.";
                CloseHandle( stdout_read );
                CloseHandle( stdout_write );
                CloseHandle( stdin_read );
                CloseHandle( stdin_write );
                return false;
            }

            STARTUPINFOA si;
            ZeroMemory( &si, sizeof( si ) );
            si.cb = sizeof( si );
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = stdout_write;
            si.hStdError = stdout_write;
            si.hStdInput = stdin_read;

            PROCESS_INFORMATION pi;
            ZeroMemory( &pi, sizeof( pi ) );

            std::vector<char> cmdline_buf( cmdline.begin(), cmdline.end() );
            cmdline_buf.push_back( '\0' );

            BOOL ok = CreateProcessA(
                          nullptr,
                          cmdline_buf.data(),
                          nullptr,
                          nullptr,
                          TRUE,
                          CREATE_NO_WINDOW,
                          nullptr,
                          nullptr,
                          &si,
                          &pi );

            CloseHandle( stdout_write );
            CloseHandle( stdin_read );

            if( !ok ) {
                error = string_format( "Failed to start runner (error %lu).", GetLastError() );
                CloseHandle( stdout_read );
                CloseHandle( stdin_write );
                return false;
            }

            child_process = pi.hProcess;
            child_thread = pi.hThread;
            running = true;
            warm = false;
            runner_log_path = log_path;
            active_config = config;
            return true;
        }

        void shutdown() {
            if( !running ) {
                return;
            }
            std::string error;
            const std::string payload = "{\"command\":\"shutdown\",\"request_id\":\"shutdown\"}\n";
            write_all( payload, error );
            std::string response_line;
            llm_intent_request dummy;
            dummy.request_id = "shutdown";
            read_response_for_request( dummy, response_line, std::chrono::milliseconds( 200 ), error );
            close_handles();
        }

        void close_handles() {
            if( stdin_write ) {
                CloseHandle( stdin_write );
                stdin_write = nullptr;
            }
            if( stdout_read ) {
                CloseHandle( stdout_read );
                stdout_read = nullptr;
            }
            if( child_thread ) {
                CloseHandle( child_thread );
                child_thread = nullptr;
            }
            if( child_process ) {
                CloseHandle( child_process );
                child_process = nullptr;
            }
            running = false;
            warm = false;
            runner_log_path.clear();
            stdout_buffer.clear();
        }

        bool write_all( const std::string &payload, std::string &error ) {
            DWORD written = 0;
            if( !WriteFile( stdin_write, payload.data(),
                            static_cast<DWORD>( payload.size() ), &written, nullptr ) ) {
                error = "Failed to write to runner stdin.";
                return false;
            }
            return true;
        }

        bool read_response_for_request( const llm_intent_request &request, std::string &out_line,
                                        std::chrono::milliseconds timeout, std::string &error ) {
            const auto start = std::chrono::steady_clock::now();
            while( true ) {
                const auto newline = stdout_buffer.find( '\n' );
                if( newline != std::string::npos ) {
                    const std::string line = stdout_buffer.substr( 0, newline );
                    stdout_buffer.erase( 0, newline + 1 );
                    std::string trimmed = line;
                    if( !trimmed.empty() && trimmed.back() == '\r' ) {
                        trimmed.pop_back();
                    }
                    if( response_from_json( trimmed, request ) ) {
                        out_line = trimmed;
                        return true;
                    }
                    continue;
                }

                if( timeout.count() > 0 &&
                    std::chrono::steady_clock::now() - start > timeout ) {
                    error = "Runner response timed out.";
                    return false;
                }

                DWORD available = 0;
                if( !PeekNamedPipe( stdout_read, nullptr, 0, nullptr, &available, nullptr ) ) {
                    error = "Runner stdout pipe failed.";
                    if( !runner_log_path.empty() && std::filesystem::exists( runner_log_path ) ) {
                        const std::string tail = read_log_tail( runner_log_path, 4096 );
                        if( !tail.empty() ) {
                            error += "\nRunner log tail:\n" + tail;
                        } else {
                            error += "\nSee config/llm_intent_runner.log for details.";
                        }
                    }
                    return false;
                }

                if( available > 0 ) {
                    char buffer[4096];
                    DWORD read = 0;
                    if( !ReadFile( stdout_read, buffer, sizeof( buffer ), &read, nullptr ) || read == 0 ) {
                        error = "Runner stdout read failed.";
                        return false;
                    }
                    stdout_buffer.append( buffer, buffer + read );
                    continue;
                }

                if( child_process ) {
                    DWORD exit_code = STILL_ACTIVE;
                    if( GetExitCodeProcess( child_process, &exit_code ) && exit_code != STILL_ACTIVE ) {
                        error = "Runner process exited.";
                        return false;
                    }
                }

                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
            }
        }
};
#else
class posix_runner_process
{
    public:
        ~posix_runner_process() {
            shutdown();
        }

        bool ensure_running( const runner_config &config, std::string &error ) {
            if( running && config == active_config ) {
                return true;
            }
            shutdown();
            return start( config, error );
        }

        bool send_request( const llm_intent_request &request, std::string &response_line,
                           std::string &error,
                           std::chrono::milliseconds timeout ) {
            std::string payload = request_to_json( request );
            payload.push_back( '\n' );
            if( !write_all( payload, error ) ) {
                return false;
            }
            std::chrono::milliseconds effective_timeout = timeout;
            if( !warm && timeout.count() > 0 ) {
                static constexpr auto startup_grace = std::chrono::milliseconds( 120000 );
                if( effective_timeout < startup_grace ) {
                    effective_timeout = startup_grace;
                }
            }
            const bool ok = read_response_for_request( request, response_line, effective_timeout, error );
            if( ok ) {
                warm = true;
            }
            return ok;
        }

        void terminate() {
            if( !running ) {
                return;
            }
            if( child_pid > 0 ) {
                kill( child_pid, SIGTERM );
            }
            close_handles();
        }

    private:
        bool running = false;
        bool warm = false;
        runner_config active_config;
        pid_t child_pid = -1;
        int stdin_write = -1;
        int stdout_read = -1;
        std::string stdout_buffer;
        std::filesystem::path runner_log_path;

        bool start( const runner_config &config, std::string &error ) {
            const std::string backend = lower_copy( config.backend );
            const bool api_configured = !config.api_provider.empty() && !config.api_model.empty();
            const bool use_api_mode = config.use_api || backend == "api";
            const bool auto_backend = backend == "auto";
            const bool needs_model = !use_api_mode && !( auto_backend && api_configured );

            if( config.python_path.empty() || config.runner_path.empty() ||
                ( config.model_dir.empty() && needs_model ) ) {
                error = "LLM runner configuration is incomplete.";
                return false;
            }

            std::filesystem::path python_path = resolve_path( config.python_path );
            std::filesystem::path runner_path = resolve_path( config.runner_path );
            std::filesystem::path model_dir = resolve_path( config.model_dir );
            std::filesystem::path log_path = PATH_INFO::config_dir_path().get_unrelative_path() /
                                             "llm_intent_runner.log";
            assure_dir_exist( PATH_INFO::config_dir() );

            std::vector<std::string> args;
            args.push_back( python_path.string() );
            args.push_back( runner_path.string() );
            if( !backend.empty() ) {
                args.push_back( "--backend" );
                args.push_back( backend );
            }
            if( use_api_mode ) {
                args.push_back( "--use-api" );
                args.push_back( "--api-provider" );
                args.push_back( config.api_provider );
                args.push_back( "--api-model" );
                args.push_back( config.api_model );
                if( !config.api_key_env.empty() ) {
                    args.push_back( "--api-key-env" );
                    args.push_back( config.api_key_env );
                }
                args.push_back( "--max-tokens" );
                args.push_back( std::to_string( config.max_tokens ) );
            } else if( !config.model_dir.empty() ) {
                std::filesystem::path cache_dir = model_dir / ".ov_cache";
                args.push_back( "--model-dir" );
                args.push_back( model_dir.string() );
                args.push_back( "--device" );
                args.push_back( config.device.empty() ? "AUTO" : config.device );
                args.push_back( "--max-tokens" );
                args.push_back( std::to_string( config.max_tokens ) );
                args.push_back( "--max-prompt-len" );
                args.push_back( std::to_string( config.max_prompt_len ) );
                args.push_back( "--cache-dir" );
                args.push_back( cache_dir.string() );
                if( config.force_npu ) {
                    args.push_back( "--force-npu" );
                }
            }
            if( !use_api_mode && auto_backend && api_configured ) {
                args.push_back( "--api-provider" );
                args.push_back( config.api_provider );
                args.push_back( "--api-model" );
                args.push_back( config.api_model );
                if( !config.api_key_env.empty() ) {
                    args.push_back( "--api-key-env" );
                    args.push_back( config.api_key_env );
                }
            }
            args.push_back( "--log-file" );
            args.push_back( log_path.string() );

            int stdout_pipe[2];
            if( pipe( stdout_pipe ) < 0 ) {
                error = "Failed to create stdout pipe.";
                return false;
            }

            int stdin_pipe[2];
            if( pipe( stdin_pipe ) < 0 ) {
                error = "Failed to create stdin pipe.";
                close( stdout_pipe[0] );
                close( stdout_pipe[1] );
                return false;
            }

            pid_t pid = fork();
            if( pid < 0 ) {
                error = "Failed to fork process.";
                close( stdout_pipe[0] );
                close( stdout_pipe[1] );
                close( stdin_pipe[0] );
                close( stdin_pipe[1] );
                return false;
            }

            if( pid == 0 ) {
                close( stdout_pipe[0] );
                close( stdin_pipe[1] );

                dup2( stdout_pipe[1], STDOUT_FILENO );
                dup2( stdout_pipe[1], STDERR_FILENO );
                dup2( stdin_pipe[0], STDIN_FILENO );
                close( stdout_pipe[1] );
                close( stdin_pipe[0] );

                std::vector<char *> argv;
                argv.reserve( args.size() + 1 );
                for( std::string &arg : args ) {
                    argv.push_back( arg.data() );
                }
                argv.push_back( nullptr );
                execv( python_path.c_str(), argv.data() );
                _exit( 127 );
            }

            close( stdout_pipe[1] );
            close( stdin_pipe[0] );

            child_pid = pid;
            stdin_write = stdin_pipe[1];
            stdout_read = stdout_pipe[0];

            int flags = fcntl( stdout_read, F_GETFL, 0 );
            fcntl( stdout_read, F_SETFL, flags | O_NONBLOCK );

            running = true;
            warm = false;
            runner_log_path = log_path;
            active_config = config;
            return true;
        }

        void shutdown() {
            if( !running ) {
                return;
            }
            std::string error;
            const std::string payload = "{\"command\":\"shutdown\",\"request_id\":\"shutdown\"}\n";
            write_all( payload, error );
            std::string response_line;
            llm_intent_request dummy;
            dummy.request_id = "shutdown";
            read_response_for_request( dummy, response_line, std::chrono::milliseconds( 200 ), error );
            close_handles();
        }

        void close_handles() {
            if( stdin_write != -1 ) {
                close( stdin_write );
                stdin_write = -1;
            }
            if( stdout_read != -1 ) {
                close( stdout_read );
                stdout_read = -1;
            }
            if( child_pid != -1 ) {
                int status = 0;
                waitpid( child_pid, &status, 0 );
                child_pid = -1;
            }
            running = false;
            warm = false;
            runner_log_path.clear();
            stdout_buffer.clear();
        }

        bool write_all( const std::string &payload, std::string &error ) {
            size_t total = 0;
            while( total < payload.size() ) {
                ssize_t written = write( stdin_write, payload.data() + total,
                                         payload.size() - total );
                if( written < 0 ) {
                    if( errno == EINTR ) {
                        continue;
                    }
                    error = "Failed to write to runner stdin.";
                    return false;
                }
                if( written == 0 ) {
                    break;
                }
                total += static_cast<size_t>( written );
            }
            if( total < payload.size() ) {
                error = "Failed to write full payload to runner stdin.";
                return false;
            }
            return true;
        }

        bool read_response_for_request( const llm_intent_request &request, std::string &out_line,
                                        std::chrono::milliseconds timeout, std::string &error ) {
            const auto start = std::chrono::steady_clock::now();
            while( true ) {
                const auto newline = stdout_buffer.find( '\n' );
                if( newline != std::string::npos ) {
                    const std::string line = stdout_buffer.substr( 0, newline );
                    stdout_buffer.erase( 0, newline + 1 );
                    std::string trimmed = line;
                    if( !trimmed.empty() && trimmed.back() == '\r' ) {
                        trimmed.pop_back();
                    }
                    if( response_from_json( trimmed, request ) ) {
                        out_line = trimmed;
                        return true;
                    }
                    continue;
                }

                if( timeout.count() > 0 &&
                    std::chrono::steady_clock::now() - start > timeout ) {
                    error = "Runner response timed out.";
                    return false;
                }

                fd_set read_fds;
                FD_ZERO( &read_fds );
                FD_SET( stdout_read, &read_fds );

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 50000;

                int sel = select( stdout_read + 1, &read_fds, nullptr, nullptr, &tv );
                if( sel < 0 ) {
                    if( errno == EINTR ) {
                        continue;
                    }
                    error = "Runner stdout select failed.";
                    if( !runner_log_path.empty() && std::filesystem::exists( runner_log_path ) ) {
                        const std::string tail = read_log_tail( runner_log_path, 4096 );
                        if( !tail.empty() ) {
                            error += "\nRunner log tail:\n" + tail;
                        } else {
                            error += "\nSee config/llm_intent_runner.log for details.";
                        }
                    }
                    return false;
                }

                if( sel > 0 && FD_ISSET( stdout_read, &read_fds ) ) {
                    char buffer[4096];
                    ssize_t bytes_read = read( stdout_read, buffer, sizeof( buffer ) );
                    if( bytes_read < 0 ) {
#if EAGAIN != EWOULDBLOCK
                        if( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ) {
#else
                        if( errno == EAGAIN || errno == EINTR ) {
#endif
                            std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                            continue;
                        }
                        error = "Runner stdout read failed.";
                        return false;
                    }
                    if( bytes_read == 0 ) {
                        error = "Runner process exited.";
                        return false;
                    }
                    stdout_buffer.append( buffer, buffer + bytes_read );
                    continue;
                }

                if( child_pid > 0 ) {
                    int status = 0;
                    pid_t ret = waitpid( child_pid, &status, WNOHANG );
                    if( ret > 0 ) {
                        error = "Runner process exited.";
                        return false;
                    }
                }

                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
            }
        }
};
#endif

class llm_intent_manager
{
    private:
        struct look_around_context {
            character_id npc_id;
            std::string npc_name;
            std::vector<std::string> item_names;
        };

    public:
        llm_intent_manager() = default;

        ~llm_intent_manager() {
            stop();
        }

        void enqueue_request( npc &listener, const std::string &player_utterance ) {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            static constexpr int default_max_tokens = 20000;
            llm_intent_request req;
            req.request_id = next_request_id();
            req.npc_id = listener.getID();
            req.npc_name = listener.get_name();
            req.snapshot = build_snapshot_json( listener, player_utterance, req.request_id );
            req.prompt = build_prompt( req.npc_name, player_utterance, req.snapshot );
            req.max_tokens = default_max_tokens;
            req.temperature = get_option<float>( "LLM_INTENT_TEMPERATURE" );
            req.top_p = get_option<float>( "LLM_INTENT_TOP_P" );
            req.repetition_penalty = get_option<float>( "LLM_INTENT_REPETITION_PENALTY" );
            if( get_option<bool>( "DEBUG_LLM_INTENT_LOG" ) ) {
                append_llm_intent_log( string_format( "prompt %s (%s)\n%s\n\n",
                                                      req.npc_name, req.request_id, req.prompt ) );
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                utterance_by_request[req.request_id] = player_utterance;
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void prewarm() {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            const runner_config config = current_runner_config();
            const std::string backend = lower_copy( config.backend );
            if( config.use_api || backend == "api" ) {
                return;
            }
            ensure_worker();
            std::string error;
            if( config.force_npu && config.device != "NPU" ) {
                if( get_option<bool>( "DEBUG_LLM_INTENT_UI" ) ) {
                    add_msg( "LLM intent prewarm skipped: LLM_INTENT_FORCE_NPU requires device NPU." );
                }
                return;
            }
            if( !runner.ensure_running( config, error ) ) {
                if( get_option<bool>( "DEBUG_LLM_INTENT_UI" ) ) {
                    add_msg( "LLM intent prewarm failed: %s", error );
                }
                return;
            }
            if( !warmup_enqueued.exchange( true ) ) {
                llm_intent_request warm;
                warm.request_id = "prewarm";
                warm.npc_id = character_id();
                warm.npc_name = "prewarm";
                warm.snapshot = "{}";
                warm.prompt = build_prompt( "", "", warm.snapshot );
                warm.max_tokens = 8;
                warm.temperature = get_option<float>( "LLM_INTENT_TEMPERATURE" );
                warm.top_p = get_option<float>( "LLM_INTENT_TOP_P" );
                warm.repetition_penalty = get_option<float>( "LLM_INTENT_REPETITION_PENALTY" );
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    request_queue.push( std::move( warm ) );
                }
                cv.notify_one();
            }
        }

        void enqueue_look_around_request( npc &listener, const std::string &player_utterance ) {
            static constexpr int look_max_tokens = 128;
            static constexpr size_t max_item_entries = 60;
            std::vector<look_around_item_entry> items = collect_look_around_items( listener, 5,
                    max_item_entries );
            if( items.empty() ) {
                return;
            }
            std::vector<std::string> ammo = collect_compatible_ammo( listener );
            std::vector<std::string> magazines = collect_compatible_magazines( listener );
            llm_intent_request req;
            req.request_id = next_request_id();
            req.npc_id = listener.getID();
            req.npc_name = listener.get_name();
            req.snapshot = "{}";
            req.prompt = build_look_around_prompt( player_utterance, ammo, magazines, items );
            req.max_tokens = look_max_tokens;
            req.temperature = get_option<float>( "LLM_INTENT_TEMPERATURE" );
            req.top_p = get_option<float>( "LLM_INTENT_TOP_P" );
            req.repetition_penalty = get_option<float>( "LLM_INTENT_REPETITION_PENALTY" );

            look_around_context context;
            context.npc_id = req.npc_id;
            context.npc_name = req.npc_name;
            context.item_names.reserve( items.size() );
            for( const look_around_item_entry &entry : items ) {
                context.item_names.push_back( entry.name );
            }

            if( get_option<bool>( "DEBUG_LLM_INTENT_LOG" ) ) {
                append_llm_intent_log( string_format( "look_around prompt %s (%s)\n%s\n\n",
                                                      req.npc_name, req.request_id, req.prompt ) );
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                look_around_requests.emplace( req.request_id, std::move( context ) );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void process_look_around_response( const llm_intent_response &resp,
                                           const look_around_context &context )
        {
            std::unordered_map<std::string, std::string> allowed;
            allowed.reserve( context.item_names.size() );
            for( const std::string &name : context.item_names ) {
                allowed.emplace( lower_copy( name ), name );
            }
            std::vector<std::string> selected;
            if( resp.ok ) {
                selected = parse_look_around_response( resp.text, allowed );
            }
            if( get_option<bool>( "DEBUG_LLM_INTENT_LOG" ) ) {
                const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                append_llm_intent_log( string_format( "look_around response %s (%s)\n%s\n\n",
                                                      context.npc_name, resp.request_id, payload ) );
            }
            if( npc *target = g->find_npc( context.npc_id ) ) {
                if( target->is_player_ally() ) {
                    target->set_llm_intent_item_targets( selected );
                }
            }
        }

        void process_responses() {
            std::queue<llm_intent_response> local;
            {
                std::lock_guard<std::mutex> lock( mutex );
                std::swap( local, response_queue );
            }

            if( local.empty() ) {
                return;
            }

            const bool debug_ui = get_option<bool>( "DEBUG_LLM_INTENT_UI" );
            const bool debug_log = get_option<bool>( "DEBUG_LLM_INTENT_LOG" );
            while( !local.empty() ) {
                const llm_intent_response &resp = local.front();
                if( resp.request_id == "prewarm" ) {
                    local.pop();
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    auto it = look_around_requests.find( resp.request_id );
                    if( it != look_around_requests.end() ) {
                        look_around_context context = std::move( it->second );
                        look_around_requests.erase( it );
                        process_look_around_response( resp, context );
                        local.pop();
                        continue;
                    }
                }
                std::string parse_error;
                std::string action_error;
                std::string speech;
                std::vector<std::string> actions;
                std::string attack_target;
                std::string speak_text;
                if( resp.ok ) {
                    std::string csv_text = extract_csv_from_text( resp.text );
                    csv_text = sanitize_llm_csv( csv_text );
                    speak_text = strip_speaker_prefix( extract_speech_field( csv_text ) );
                    if( !speak_text.empty() ) {
                        if( g->find_npc( resp.npc_id ) ) {
                            add_msg( _( "%s says: \"%s\"" ), resp.npc_name, speak_text );
                            if( get_option<bool>( "DEBUG_LLM_INTENT_LOG" ) ) {
                                append_llm_intent_log( string_format( "say %s (%s)\n%s\n\n",
                                                                      resp.npc_name, resp.request_id, speak_text ) );
                            }
                        } else if( get_option<bool>( "DEBUG_LLM_INTENT_LOG" ) ) {
                            append_llm_intent_log( string_format( "say failed %s (%s)\n%s\n\n",
                                                                  resp.npc_name, resp.request_id, speak_text ) );
                        }
                    }
                    bool parsed = false;
                    std::string normalized = normalize_csv_separators( csv_text );
                    normalized = sanitize_llm_csv( normalized );
                    parsed = parse_csv_payload( csv_text, speech, actions, attack_target, parse_error );
                    if( !parsed && normalized != csv_text ) {
                        parsed = parse_csv_payload( normalized, speech, actions, attack_target, parse_error );
                    }
                    if( !parsed ) {
                        parse_error.clear();
                        parsed = extract_lenient_csv( csv_text, speech, actions );
                        if( parsed ) {
                            action_error = "Used lenient CSV parsing.";
                        }
                    }
                    if( attack_target.empty() ) {
                        attack_target = extract_attack_target_hint( csv_text );
                    }
                    if( parsed ) {
                        for( const std::string &token : actions ) {
                            if( !is_allowed_action( token ) ) {
                                action_error = "CSV action token not in allowed list.";
                                break;
                            }
                        }
                        if( action_error == "CSV action token not in allowed list." ) {
                            actions.clear();
                            actions.push_back( "idle" );
                        }
                    } else {
                        parse_error = "CSV parse failed.";
                    }
                }

                const bool wants_look_around = std::find( actions.begin(), actions.end(),
                                                "look_around" ) != actions.end();
                if( wants_look_around ) {
                    actions.erase( std::remove( actions.begin(), actions.end(), "look_around" ),
                                   actions.end() );
                }

                if( resp.ok && parse_error.empty() && !actions.empty() ) {
                    std::vector<llm_intent_action> intent_actions;
                    intent_actions.reserve( actions.size() );
                    for( const std::string &token : actions ) {
                        const llm_intent_action action = intent_action_from_token( token );
                        if( action != llm_intent_action::none ) {
                            intent_actions.push_back( action );
                        }
                    }
                    if( !intent_actions.empty() ) {
                        if( npc *target = g->find_npc( resp.npc_id ) ) {
                            if( target->is_player_ally() ) {
                                target->set_llm_intent_actions( intent_actions, resp.request_id, attack_target );
                            }
                        }
                    }
                }
                if( wants_look_around ) {
                    std::string player_utterance;
                    {
                        std::lock_guard<std::mutex> lock( mutex );
                        auto it = utterance_by_request.find( resp.request_id );
                        if( it != utterance_by_request.end() ) {
                            player_utterance = it->second;
                        }
                    }
                    if( npc *target = g->find_npc( resp.npc_id ) ) {
                        if( target->is_player_ally() ) {
                            enqueue_look_around_request( *target, player_utterance );
                        }
                    }
                }
                if( resp.ok && parse_error.empty() ) {
                    if( debug_ui ) {
                        add_msg( "LLM intent response for %s: %s", resp.npc_name, resp.text );
                        if( !action_error.empty() ) {
                            add_msg( "LLM intent warning for %s: %s", resp.npc_name, action_error );
                        }
                    }
                    if( debug_log ) {
                        const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                        append_llm_intent_log( string_format( "response %s (%s)\n%s\n\n",
                                                              resp.npc_name, resp.request_id, payload ) );
                    }
                } else {
                    const std::string err = resp.ok ? parse_error : resp.error;
                    if( debug_ui ) {
                        add_msg( "LLM intent failed for %s: %s", resp.npc_name, err );
                    }
                    if( debug_log ) {
                        const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                        append_llm_intent_log( string_format( "failed %s (%s)\n%s\nraw:\n%s\n\n",
                                                              resp.npc_name, resp.request_id, err, payload ) );
                    }
                }
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    utterance_by_request.erase( resp.request_id );
                }
                local.pop();
            }
        }

    private:
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<llm_intent_request> request_queue;
        std::queue<llm_intent_response> response_queue;
        std::unordered_map<std::string, std::string> utterance_by_request;
        std::unordered_map<std::string, look_around_context> look_around_requests;
        std::thread worker;
        std::atomic<bool> stopping = false;
        std::atomic<int> counter = 0;
        std::atomic<bool> warmup_enqueued = false;
#if defined(_WIN32)
        llm_intent_runner_process runner;
#else
        posix_runner_process runner;
#endif

        void ensure_worker() {
            if( worker.joinable() ) {
                return;
            }
            worker = std::thread( [this]() {
                worker_loop();
            } );
        }

        void stop() {
            stopping = true;
            cv.notify_one();
            if( worker.joinable() ) {
                worker.join();
            }
        }

        std::string next_request_id() {
            return string_format( "req_%d", counter.fetch_add( 1 ) );
        }

        void worker_loop() {
            while( !stopping ) {
                llm_intent_request req;
                {
                    std::unique_lock<std::mutex> lock( mutex );
                    cv.wait( lock, [this]() {
                        return stopping || !request_queue.empty();
                    } );
                    if( stopping ) {
                        break;
                    }
                    req = std::move( request_queue.front() );
                    request_queue.pop();
                }
                llm_intent_response response = handle_request( req );
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    response_queue.push( std::move( response ) );
                }
            }
        }

        llm_intent_response handle_request( const llm_intent_request &req ) {
            llm_intent_response response;
            response.request_id = req.request_id;
            response.npc_id = req.npc_id;
            response.npc_name = req.npc_name;
            const runner_config config = current_runner_config();
            std::string error;
            if( !config.use_api && config.force_npu && config.device != "NPU" ) {
                response.ok = false;
                response.error = "LLM_INTENT_FORCE_NPU requires device NPU.";
                return response;
            }
            if( !runner.ensure_running( config, error ) ) {
                response.ok = false;
                response.error = error;
                return response;
            }
            std::string line;
            const int timeout_ms = get_option<int>( "LLM_INTENT_TIMEOUT_MS" );
            if( !runner.send_request( req, line, error,
                                      std::chrono::milliseconds( timeout_ms ) ) ) {
                runner.terminate();
                response.ok = false;
                response.error = error;
                return response;
            }
            if( auto parsed = response_from_json( line, req ) ) {
                return *parsed;
            }
            response.ok = false;
            response.error = "Runner returned invalid JSON.";
            return response;
        }
};

llm_intent_manager &get_manager()
{
    static llm_intent_manager manager;
    return manager;
}
} // namespace

namespace llm_intent
{
void enqueue_request( npc &listener, const std::string &player_utterance )
{
    get_manager().enqueue_request( listener, player_utterance );
}

void enqueue_request( const npc &listener, const std::string &player_utterance )
{
    get_manager().enqueue_request( const_cast<npc &>( listener ), player_utterance );
}

void enqueue_requests( const std::vector<npc *> &listeners,
                       const std::string &player_utterance )
{
    for( npc *listener : listeners ) {
        if( listener == nullptr ) {
            continue;
        }
        get_manager().enqueue_request( *listener, player_utterance );
    }
}

void prewarm()
{
    get_manager().prewarm();
}

void process_responses()
{
    get_manager().process_responses();
}

void log_event( const std::string &message )
{
    append_llm_intent_log( message + "\n" );
}
} // namespace llm_intent

#include "llm_intent.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <ratio>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "calendar.h"
#include "cata_path.h"
#include "cata_utility.h"
#include "character.h"
#include "character_id.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "dialogue_chatbin.h"
#include "effect.h"
#include "filesystem.h"
#include "flexbuffer_json.h"
#include "game.h"
#include "item.h"
#include "item_location.h"
#include "itype.h"
#include "json.h"
#include "map.h"
#include "map_selector.h"
#include "memory_fast.h"
#include "messages.h"
#include "mod_manager.h"
#include "npc.h"
#include "npc_opinion.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "ret_val.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "vehicle_selector.h"
#include "visitable.h"
#include "worldfactory.h"
#include "vpart_position.h"

#if defined(_WIN32)
#if 1 // HACK: Hack to prevent reordering of #include "platform_win.h" by IWYU
#include "platform_win.h"
#endif
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
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
constexpr const char *llm_intent_log_filename = "llm_intent.log";
constexpr std::streamoff llm_intent_log_rotate_bytes = 50 * 1024 * 1024;

std::filesystem::path central_llm_config_dir_path()
{
    return PATH_INFO::base_path().get_unrelative_path() / "config";
}

std::filesystem::path central_llm_log_path( const char *filename )
{
    return central_llm_config_dir_path() / std::filesystem::u8path( filename );
}

void append_llm_intent_log( const std::string &payload )
{
    if( payload.empty() ) {
        return;
    }
    auto replace_all = []( std::string & text, const std::string & from,
    const std::string & to ) {
        if( from.empty() ) {
            return;
        }
        size_t pos = 0;
        while( ( pos = text.find( from, pos ) ) != std::string::npos ) {
            text.replace( pos, from.size(), to );
            pos += to.size();
        }
    };
    std::string final_payload = payload;
    // Normalize common UTF-8 punctuation so Windows log viewers don't show mojibake.
    replace_all( final_payload, "\xE2\x80\x98", "'" );
    replace_all( final_payload, "\xE2\x80\x99", "'" );
    replace_all( final_payload, "\xE2\x80\x9C", "\"" );
    replace_all( final_payload, "\xE2\x80\x9D", "\"" );
    replace_all( final_payload, "\xE2\x80\x93", "-" );
    replace_all( final_payload, "\xE2\x80\x94", "-" );
    replace_all( final_payload, "\xE2\x80\xA6", "..." );
    replace_all( final_payload, "\xC2\xA0", " " );
    if( final_payload.size() < 2 ||
        final_payload.compare( final_payload.size() - 2, 2, "\n\n" ) != 0 ) {
        final_payload += "\n\n";
    }
    std::lock_guard<std::mutex> lock( llm_intent_log_mutex );
    const std::filesystem::path config_dir = central_llm_config_dir_path();
    std::error_code mkdir_ec;
    std::filesystem::create_directories( config_dir, mkdir_ec );
    const std::filesystem::path log_path = central_llm_log_path( llm_intent_log_filename );
    std::error_code ec;
    if( std::filesystem::exists( log_path, ec ) ) {
        const std::uintmax_t size = std::filesystem::file_size( log_path, ec );
        if( !ec && size >= static_cast<std::uintmax_t>( llm_intent_log_rotate_bytes ) ) {
            for( int i = 1; i <= 9999; ++i ) {
                std::filesystem::path rotated = log_path;
                rotated += std::filesystem::u8path( "." + std::to_string( i ) );
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
    out << final_payload;
}

constexpr const char *llm_prompt_dirname = "llm_prompts";
constexpr const char *npc_action_prompt_filename = "npc_action_prompt.txt";
constexpr const char *npc_ambient_prompt_filename = "npc_ambient_prompt.txt";
constexpr const char *look_around_prompt_filename = "look_around_prompt.txt";
constexpr const char *look_inventory_prompt_filename = "look_inventory_prompt.txt";
constexpr const char *llm_prompt_readme_filename = "README.txt";

std::filesystem::path llm_prompt_override_dir_path()
{
    return ( PATH_INFO::config_dir_path() / llm_prompt_dirname ).get_unrelative_path();
}

std::filesystem::path bundled_llm_prompt_dir_path()
{
    return ( PATH_INFO::datadir_path() / llm_prompt_dirname ).get_unrelative_path();
}

void replace_all_in_place( std::string &text, const std::string &from, const std::string &to )
{
    if( from.empty() ) {
        return;
    }
    size_t pos = 0;
    while( ( pos = text.find( from, pos ) ) != std::string::npos ) {
        text.replace( pos, from.size(), to );
        pos += to.size();
    }
}

std::string render_prompt_template( std::string templ,
                                    const std::initializer_list<std::pair<std::string, std::string>> &replacements )
{
    for( const std::pair<std::string, std::string> &entry : replacements ) {
        replace_all_in_place( templ, entry.first, entry.second );
    }
    return templ;
}

bool prompt_template_has_required_tokens( const std::string &templ,
        const std::initializer_list<std::string_view> &required_tokens )
{
    if( templ.find_first_not_of( " \t\r\n" ) == std::string::npos ) {
        return false;
    }
    for( const std::string_view token : required_tokens ) {
        if( templ.find( token ) == std::string::npos ) {
            return false;
        }
    }
    return true;
}

void seed_llm_prompt_override_files()
{
    const std::filesystem::path override_dir = llm_prompt_override_dir_path();
    if( !assure_dir_exist( override_dir ) ) {
        return;
    }

    const std::filesystem::path bundled_dir = bundled_llm_prompt_dir_path();
    for( const char *filename : { npc_action_prompt_filename, npc_ambient_prompt_filename,
                                  look_around_prompt_filename, look_inventory_prompt_filename,
                                  llm_prompt_readme_filename } ) {
        const std::filesystem::path source = bundled_dir / filename;
        const std::filesystem::path dest = override_dir / filename;
        if( file_exist( source ) && !file_exist( dest ) ) {
            copy_file( source.string(), dest.string() );
        }
    }
}

std::string load_llm_prompt_template( const char *filename, std::string_view fallback_template,
                                      const std::initializer_list<std::string_view> &required_tokens )
{
    static std::once_flag seed_once;
    std::call_once( seed_once, []() {
        seed_llm_prompt_override_files();
    } );

    for( const std::filesystem::path &path : { llm_prompt_override_dir_path() / filename,
                                               bundled_llm_prompt_dir_path() / filename } ) {
        if( !file_exist( path ) ) {
            continue;
        }
        const std::string templ = read_entire_file( path );
        if( prompt_template_has_required_tokens( templ, required_tokens ) ) {
            return templ;
        }
    }

    return std::string( fallback_template );
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
    std::string ollama_url;
    std::string ollama_model;
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
               ollama_url == other.ollama_url &&
               ollama_model == other.ollama_model &&
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
std::string extract_csv_from_text( const std::string &text );

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

std::string trim_copy( std::string_view text )
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
    return std::string( text.substr( start, end - start ) );
}

std::string normalize_item_label( std::string_view text )
{
    std::string stripped = remove_color_tags( text );
    size_t suffix_pos = stripped.find( " > " );
    if( suffix_pos == std::string::npos ) {
        suffix_pos = stripped.find( '>' );
    }
    if( suffix_pos != std::string::npos ) {
        stripped = stripped.substr( 0, suffix_pos );
    }
    std::string out;
    out.reserve( stripped.size() );
    bool last_space = false;
    for( unsigned char c : stripped ) {
        bool allowed = std::isalnum( c ) != 0;
        if( !allowed ) {
            switch( c ) {
                case ' ':
                case '-':
                case '_':
                case '.':
                case ',':
                case '/':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                    allowed = true;
                    break;
                default:
                    break;
            }
        }
        if( allowed ) {
            out.push_back( static_cast<char>( c ) );
            last_space = ( c == ' ' );
        } else if( !last_space ) {
            out.push_back( ' ' );
            last_space = true;
        }
    }
    return trim_copy( out );
}

std::string lower_copy( const std::string &text )
{
    std::string out = text;
    std::transform( out.begin(), out.end(), out.begin(), []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    return out;
}

std::string join_action_tokens( const std::vector<std::string> &actions )
{
    std::string joined;
    for( const std::string &action : actions ) {
        if( !joined.empty() ) {
            joined += ", ";
        }
        joined += action;
    }
    return joined;
}

std::string follower_mode_snapshot_token( const npc &listener )
{
    if( !listener.is_player_ally() ) {
        return "independent";
    }
    if( listener.is_guarding() || listener.get_attitude() == NPCATT_WAIT ) {
        return "guard/hold";
    }
    if( listener.rules.has_flag( ally_rule::follow_close ) ) {
        return "follow-close";
    }
    return "follow-afar";
}

void broadcast_overheard_memory( const npc &speaker, const std::string &speech,
                                 const std::vector<std::string> &actions )
{
    if( !g || ( speech.empty() && actions.empty() ) ) {
        return;
    }
    static constexpr int llm_intent_overhear_volume = 12;
    std::vector<npc *> listeners = g->get_npcs_if( [&]( const npc & guy ) {
        return guy.getID() != speaker.getID() &&
               guy.is_player_ally() &&
               guy.can_hear( speaker.pos_bub(), llm_intent_overhear_volume );
    } );
    for( npc *listener : listeners ) {
        if( listener != nullptr ) {
            listener->add_llm_overheard_memory( speaker.get_name(), speech, actions );
        }
    }
}

struct background_summary_entry {
    std::string background;
    std::string expression;
    std::string source_tag;
};

struct background_summary_cache {
    std::unordered_map<std::string, std::string> trait_to_topic;
    std::unordered_map<std::string, background_summary_entry> summary_by_topic;
    std::unordered_map<std::string, background_summary_entry> summary_by_selector;
    std::vector<std::string> loaded_root_keys;
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

void load_background_trait_to_topic( const cata_path &toc_path,
                                     std::unordered_map<std::string, std::string> &out )
{
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
                    out[trait] = topic;
                }
            }
        }
    } );
}

enum class background_summary_text_target : int {
    topic,
    selector
};

int summary_file_generation_priority( const std::filesystem::path &path )
{
    const std::string filename = path.filename().generic_u8string();
    if( filename.rfind( "generated_", 0 ) == 0 ) {
        return 0;
    }
    return 1;
}

int summary_file_format_priority( const std::filesystem::path &path )
{
    if( path.extension() == std::filesystem::u8path( ".txt" ) ) {
        return 0;
    }
    if( path.extension() == std::filesystem::u8path( ".json" ) ) {
        return 1;
    }
    return 2;
}

void add_summary_id( std::vector<std::string> &out, const std::string &selector )
{
    const std::string normalized = normalize_summary_line( selector );
    if( normalized.empty() ) {
        return;
    }
    if( std::find( out.begin(), out.end(), normalized ) == out.end() ) {
        out.push_back( normalized );
    }
}

void add_summary_ids_from_json( const JsonObject &jo, const std::string &single_key,
                                const std::string &multi_key, std::vector<std::string> &out )
{
    if( jo.has_string( single_key ) ) {
        add_summary_id( out, jo.get_string( single_key ) );
    }
    if( jo.has_array( multi_key ) ) {
        for( const JsonValue entry : jo.get_array( multi_key ) ) {
            if( entry.test_string() ) {
                add_summary_id( out, entry.get_string() );
            }
        }
    }
}

void insert_background_summary_entry( std::unordered_map<std::string, background_summary_entry> &out,
                                      const std::vector<std::string> &ids,
                                      const background_summary_entry &entry_value,
                                      bool overwrite_existing )
{
    for( const std::string &id : ids ) {
        if( id.empty() ) {
            continue;
        }
        if( !overwrite_existing && out.count( id ) > 0 ) {
            continue;
        }
        out[id] = entry_value;
    }
}

void load_background_summary_text_file( const cata_path &summary_path,
                                        std::unordered_map<std::string, background_summary_entry> &out,
                                        bool overwrite_existing )
{
    read_from_file_optional( summary_path, [&]( std::istream & data ) {
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
            const std::string id = normalize_summary_line( parts[0] );
            if( id.empty() ) {
                continue;
            }
            background_summary_entry entry_value;
            entry_value.background = normalize_summary_line( parts[1] );
            entry_value.expression = normalize_summary_line( parts[2] );
            if( parts.size() > 3 ) {
                entry_value.source_tag = normalize_summary_line( parts[3] );
            }
            insert_background_summary_entry( out, { id }, entry_value, overwrite_existing );
        }
    } );
}

void load_background_summary_json_entry( const JsonObject &jo,
        std::unordered_map<std::string, background_summary_entry> &summary_by_topic,
        std::unordered_map<std::string, background_summary_entry> &summary_by_selector,
        bool overwrite_existing )
{
    jo.allow_omitted_members();
    const std::string type = jo.get_string( "type", "npc_personality_summary" );
    if( !type.empty() && type != "npc_personality_summary" ) {
        return;
    }

    background_summary_entry entry_value;
    if( jo.has_string( "your_background" ) ) {
        entry_value.background = normalize_summary_line( jo.get_string( "your_background" ) );
    } else if( jo.has_string( "background" ) ) {
        entry_value.background = normalize_summary_line( jo.get_string( "background" ) );
    }
    if( jo.has_string( "your_expression" ) ) {
        entry_value.expression = normalize_summary_line( jo.get_string( "your_expression" ) );
    } else if( jo.has_string( "expression" ) ) {
        entry_value.expression = normalize_summary_line( jo.get_string( "expression" ) );
    }
    if( jo.has_string( "source_tag" ) ) {
        entry_value.source_tag = normalize_summary_line( jo.get_string( "source_tag" ) );
    }
    if( entry_value.background.empty() || entry_value.expression.empty() ) {
        return;
    }

    std::vector<std::string> topic_ids;
    std::vector<std::string> selector_ids;
    add_summary_ids_from_json( jo, "topic", "topics", topic_ids );
    add_summary_ids_from_json( jo, "selector", "selectors", selector_ids );
    if( topic_ids.empty() && selector_ids.empty() && jo.has_string( "id" ) ) {
        add_summary_id( topic_ids, jo.get_string( "id" ) );
    }

    insert_background_summary_entry( summary_by_topic, topic_ids, entry_value, overwrite_existing );
    insert_background_summary_entry( summary_by_selector, selector_ids, entry_value, overwrite_existing );
}

void load_background_summary_json_file( const cata_path &summary_path,
                                        std::unordered_map<std::string, background_summary_entry> &summary_by_topic,
                                        std::unordered_map<std::string, background_summary_entry> &summary_by_selector,
                                        bool overwrite_existing )
{
    read_from_file_optional_json( summary_path, [&]( const JsonValue & root ) {
        if( root.test_array() ) {
            for( const JsonValue entry : root.get_array() ) {
                if( entry.test_object() ) {
                    load_background_summary_json_entry( entry.get_object(), summary_by_topic,
                                                        summary_by_selector, overwrite_existing );
                }
            }
            return;
        }
        if( !root.test_object() ) {
            return;
        }
        JsonObject jo = root.get_object();
        jo.allow_omitted_members();
        if( jo.has_array( "entries" ) ) {
            for( const JsonValue entry : jo.get_array( "entries" ) ) {
                if( entry.test_object() ) {
                    load_background_summary_json_entry( entry.get_object(), summary_by_topic,
                                                        summary_by_selector, overwrite_existing );
                }
            }
            return;
        }
        load_background_summary_json_entry( jo, summary_by_topic, summary_by_selector,
                                            overwrite_existing );
    } );
}

void load_background_summary_dir( const cata_path &summary_root,
                                  background_summary_text_target text_target,
                                  std::unordered_map<std::string, background_summary_entry> &summary_by_topic,
                                  std::unordered_map<std::string, background_summary_entry> &summary_by_selector,
                                  bool overwrite_existing )
{
    const std::filesystem::path summary_dir = summary_root.get_unrelative_path();
    std::error_code ec;
    if( !std::filesystem::exists( summary_dir, ec ) ) {
        return;
    }

    std::vector<std::filesystem::path> files;
    for( const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(
             summary_dir,
             ec ) ) {
        if( ec ) {
            break;
        }
        if( !entry.is_regular_file( ec ) ) {
            continue;
        }
        const std::filesystem::path extension = entry.path().extension();
        if( extension != std::filesystem::u8path( ".txt" ) &&
            extension != std::filesystem::u8path( ".json" ) ) {
            continue;
        }
        files.push_back( entry.path().filename() );
    }

    std::sort( files.begin(), files.end(), []( const std::filesystem::path &lhs,
    const std::filesystem::path &rhs ) {
        const int lhs_generation = summary_file_generation_priority( lhs );
        const int rhs_generation = summary_file_generation_priority( rhs );
        if( lhs_generation != rhs_generation ) {
            return lhs_generation < rhs_generation;
        }
        const std::string lhs_stem = lhs.stem().generic_u8string();
        const std::string rhs_stem = rhs.stem().generic_u8string();
        if( lhs_stem != rhs_stem ) {
            return lhs_stem < rhs_stem;
        }
        const int lhs_format = summary_file_format_priority( lhs );
        const int rhs_format = summary_file_format_priority( rhs );
        if( lhs_format != rhs_format ) {
            return lhs_format < rhs_format;
        }
        return lhs.generic_u8string() < rhs.generic_u8string();
    } );

    for( const std::filesystem::path &filename : files ) {
        const cata_path full_path = summary_root / filename.generic_u8string();
        if( filename.extension() == std::filesystem::u8path( ".json" ) ) {
            load_background_summary_json_file( full_path, summary_by_topic, summary_by_selector,
                                               overwrite_existing );
            continue;
        }
        if( text_target == background_summary_text_target::topic ) {
            load_background_summary_text_file( full_path, summary_by_topic, overwrite_existing );
        } else {
            load_background_summary_text_file( full_path, summary_by_selector, overwrite_existing );
        }
    }
}

std::vector<cata_path> background_summary_data_roots()
{
    std::vector<cata_path> roots;
    roots.emplace_back( PATH_INFO::datadir_path() / "json" );
    if( world_generator && world_generator->active_world ) {
        for( const mod_id &mod : world_generator->active_world->active_mod_order ) {
            if( mod.is_valid() ) {
                roots.emplace_back( mod->path );
            }
        }
        roots.emplace_back( PATH_INFO::world_base_save_path() / "mods" );
    }
    return roots;
}

std::vector<std::string> background_summary_root_keys( const std::vector<cata_path> &roots )
{
    std::vector<std::string> keys;
    keys.reserve( roots.size() );
    for( const cata_path &root : roots ) {
        keys.push_back( root.get_unrelative_path().generic_u8string() );
    }
    return keys;
}

void add_summary_selector( std::vector<std::string> &out, const std::string &selector )
{
    add_summary_id( out, selector );
}

background_summary_cache &get_background_summaries()
{
    static background_summary_cache cache;
    const std::vector<cata_path> roots = background_summary_data_roots();
    const std::vector<std::string> root_keys = background_summary_root_keys( roots );
    if( cache.loaded && cache.loaded_root_keys == root_keys ) {
        return cache;
    }

    cache = background_summary_cache();
    cache.loaded = true;
    cache.loaded_root_keys = root_keys;

    for( const cata_path &root : roots ) {
        load_background_trait_to_topic( root / "npcs" / "Backgrounds" /
                                        "backgrounds_table_of_contents.json", cache.trait_to_topic );
        load_background_summary_dir( root / "npcs" / "Backgrounds" / "Summaries_short",
                                     background_summary_text_target::topic,
                                     cache.summary_by_topic, cache.summary_by_selector, true );
        load_background_summary_dir( root / "npcs" / "Backgrounds" / "Summaries_extra",
                                     background_summary_text_target::selector,
                                     cache.summary_by_topic, cache.summary_by_selector, true );
    }

    return cache;
}

background_summary_entry get_background_summary_for( const npc &listener )
{
    background_summary_cache &cache = get_background_summaries();

    std::vector<std::string> selectors;
    add_summary_selector( selectors, "name:" + listener.get_name() );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_friend );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_friend_guard );
    add_summary_selector( selectors, "topic:" + listener.chatbin.first_topic );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_leader );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_stranger_friendly );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_stranger_neutral );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_stranger_wary );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_stranger_scared );
    add_summary_selector( selectors, "topic:" + listener.chatbin.talk_stranger_aggressive );
    for( const std::string &selector : selectors ) {
        const auto it = cache.summary_by_selector.find( selector );
        if( it != cache.summary_by_selector.end() ) {
            return it->second;
        }
    }

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

bool is_action_token( std::string_view token )
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
        "hold_position",
        "follow_close",
        "follow_far",
        "equip_gun",
        "equip_melee",
        "equip_bow",
        "panic_on",
        "panic_off",
        "look_around",
        "look_inventory",
        "idle"
    };
    return actions;
}

bool is_move_action( const std::string &token )
{
    std::string lowered = trim_copy( token );
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    return lowered.rfind( "move:", 0 ) == 0 || lowered.rfind( "move ", 0 ) == 0;
}

bool is_allowed_action( const std::string &token )
{
    if( is_move_action( token ) ) {
        return true;
    }
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
    if( token == "hold_position" ) {
        return llm_intent_action::hold_position;
    }
    if( token == "follow_close" ) {
        return llm_intent_action::follow_close;
    }
    if( token == "follow_far" ) {
        return llm_intent_action::follow_far;
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
    if( token == "panic_on" ) {
        return llm_intent_action::panic_on;
    }
    if( token == "panic_off" ) {
        return llm_intent_action::panic_off;
    }
    if( token == "idle" ) {
        return llm_intent_action::none;
    }
    return llm_intent_action::none;
}

bool parse_move_field( const std::string &field, std::vector<std::string> &coords,
                       std::string &terminal_state, std::string &error )
{
    coords.clear();
    terminal_state.clear();
    std::string lowered = trim_copy( field );
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    if( lowered.rfind( "move:", 0 ) == 0 ) {
        lowered = trim_copy( lowered.substr( 5 ) );
    } else if( lowered.rfind( "move ", 0 ) == 0 ) {
        lowered = trim_copy( lowered.substr( 4 ) );
    } else {
        error = "Move field is invalid.";
        return false;
    }
    std::string body = lowered;
    if( body.empty() ) {
        error = "Move field is missing coordinates.";
        return false;
    }
    std::istringstream iss( body );
    std::vector<std::string> parts;
    for( std::string token; iss >> token; ) {
        parts.push_back( token );
    }
    if( parts.size() < 2 ) {
        error = "Move field must include coordinates and terminal state.";
        return false;
    }
    terminal_state = parts.back();
    if( terminal_state != "wait_here" && terminal_state != "hold_position" ) {
        error = "Move field terminal state is invalid.";
        return false;
    }
    parts.pop_back();
    if( parts.empty() || parts.size() > 15 ) {
        error = "Move field must have 1-15 coordinates.";
        return false;
    }
    static const std::set<std::string> valid_coords = { "n", "s", "e", "w", "ne", "nw", "se", "sw" };
    for( const std::string &part : parts ) {
        if( valid_coords.count( part ) == 0 ) {
            error = "Move coordinate is invalid.";
            return false;
        }
        coords.push_back( part );
    }
    return true;
}

void normalize_wait_here_hold_position_pair( std::vector<std::string> &actions )
{
    if( actions.size() == 2 && actions[0] == "wait_here" && actions[1] == "hold_position" ) {
        actions.erase( actions.begin() + 1 );
    }
}

bool parse_csv_payload( std::string_view csv, std::string &speech,
                        std::vector<std::string> &actions,
                        std::string &attack_target,
                        std::vector<std::string> &move_coords,
                        std::string &move_terminal_state,
                        std::string &error )
{
    actions.clear();
    speech.clear();
    attack_target.clear();
    move_coords.clear();
    move_terminal_state.clear();
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
        if( is_move_action( field ) ) {
            if( !move_coords.empty() ) {
                error = "CSV move field repeated.";
                return false;
            }
            if( !parse_move_field( field, move_coords, move_terminal_state, error ) ) {
                return false;
            }
            actions.push_back( trim_copy( field ) );
            if( actions.size() > 3 ) {
                error = "CSV has too many action tokens.";
                return false;
            }
            continue;
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
        actions.emplace_back( "idle" );
    }
    normalize_wait_here_hold_position_pair( actions );
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
    std::string id;
    std::string name;
    int quantity = 0;
    int min_distance = 0;
};

struct look_around_selection {
    std::string name;
    int quantity = -1;
};

struct inventory_item_entry {
    std::string id;
    std::string name;
    item *ptr = nullptr;
};

struct look_inventory_selection {
    std::vector<std::string> wear;
    std::vector<std::string> wield;
    std::vector<std::string> act;
    std::vector<std::string> drop;
};

std::string xml_escape( std::string_view text )
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

std::string strip_think_output( const std::string &text )
{
    for( size_t i = text.size(); i-- > 0; ) {
        if( text[i] != '<' ) {
            continue;
        }
        size_t j = i + 1;
        while( j < text.size() && isspace( static_cast<unsigned char>( text[j] ) ) ) {
            ++j;
        }
        if( j >= text.size() || text[j] != '/' ) {
            continue;
        }
        ++j;
        while( j < text.size() && isspace( static_cast<unsigned char>( text[j] ) ) ) {
            ++j;
        }
        static constexpr std::string_view think_tag = "think";
        size_t k = 0;
        while( k < think_tag.size() && j + k < text.size() &&
               tolower( static_cast<unsigned char>( text[j + k] ) ) == think_tag[k] ) {
            ++k;
        }
        if( k != think_tag.size() ) {
            continue;
        }
        j += k;
        while( j < text.size() && isspace( static_cast<unsigned char>( text[j] ) ) ) {
            ++j;
        }
        if( j >= text.size() || text[j] != '>' ) {
            continue;
        }
        return trim_copy( text.substr( j + 1 ) );
    }
    return text;
}

std::string strip_xml_tags( const std::string &text )
{
    const std::string stripped = strip_think_output( text );
    std::string out;
    out.reserve( stripped.size() );
    bool in_tag = false;
    for( char c : stripped ) {
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

template<typename F>
void for_each_visible_look_around_item( npc &listener, int radius, const F &cb )
{
    map &here = get_map();
    const auto visit_node = [&]( const tripoint_bub_ms & p, const int dist,
    const item_location & base_loc, item & top, const auto & ) -> void {
        top.visit_items( [&]( item * node, item * ) {
            if( node == nullptr ) {
                return VisitResponse::NEXT;
            }
            if( node->is_corpse() ) {
                return VisitResponse::NEXT;
            }
            item_location loc = node == &top ? base_loc : item_location( base_loc, node );
            cb( p, dist, loc, *node );
            return VisitResponse::NEXT;
        } );
    };

    for( const tripoint_bub_ms &p : closest_points_first( listener.pos_bub(), radius ) ) {
        if( !here.sees_some_items( p, listener ) || !listener.sees( here, p ) ) {
            continue;
        }
        const int dist = rl_dist( listener.pos_bub(), p );
        for( item &it : here.i_at( p ) ) {
            visit_node( p, dist, item_location( map_cursor{ p }, &it ), it, visit_node );
        }
        const optional_vpart_position vp = here.veh_at( p );
        if( !vp ) {
            continue;
        }
        const std::optional<vpart_reference> cargo = vp.cargo();
        if( !cargo || cargo->has_feature( "LOCKED" ) ) {
            continue;
        }
        for( item &it : cargo->items() ) {
            visit_node( p, dist,
                        item_location( vehicle_cursor{ cargo->vehicle(), static_cast<ptrdiff_t>( cargo->part_index() ) }, &it ),
                        it, visit_node );
        }
    }
}

std::vector<look_around_item_entry> collect_look_around_items( npc &listener, int radius,
        size_t max_entries )
{
    std::unordered_map<std::string, look_around_item_entry> entries_by_name;
    for_each_visible_look_around_item( listener, radius,
    [&]( const tripoint_bub_ms &, const int dist, const item_location &, const item & it ) {
        const std::string name = normalize_item_label( it.tname( 1, false ) );
        if( name.empty() ) {
            return;
        }
        look_around_item_entry &entry = entries_by_name[name];
        if( entry.name.empty() ) {
            entry.name = name;
            entry.min_distance = dist;
        }
        entry.quantity += it.count();
        entry.min_distance = std::min( entry.min_distance, dist );
    } );

    std::vector<look_around_item_entry> entries;
    entries.reserve( entries_by_name.size() );
    for( auto &entry : entries_by_name ) {
        entries.push_back( std::move( entry.second ) );
    }
    std::sort( entries.begin(), entries.end(),
    []( const look_around_item_entry & lhs, const look_around_item_entry & rhs ) {
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

std::vector<inventory_item_entry> collect_inventory_entries( npc &listener, size_t max_entries )
{
    std::vector<inventory_item_entry> entries;
    entries.reserve( max_entries );
    std::unordered_set<std::string> seen;
    int counter = 0;
    listener.visit_items( [&]( item * it, item * ) {
        if( it == nullptr ) {
            return VisitResponse::NEXT;
        }
        const std::string name = normalize_item_label( it->tname( 1, false ) );
        if( name.empty() ) {
            return VisitResponse::NEXT;
        }
        if( !seen.insert( name ).second ) {
            return VisitResponse::NEXT;
        }
        inventory_item_entry entry;
        entry.id = string_format( "item_%d", ++counter );
        entry.name = name;
        entry.ptr = it;
        entries.push_back( std::move( entry ) );
        if( entries.size() >= max_entries ) {
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );
    return entries;
}

std::string default_look_around_prompt_template()
{
    return R"(<System>Select up to three nearby items from the list for the NPC to view in their surroundings and pick up, grab, search for, or explore around them.Prioritize items that best match the player's request, including named objects like a backpack, knife, or other visible item.Items may be on the ground, in containers, in corpses, or in nearby vehicle cargo; treat them all as visible nearby choices.Return only up to three item ids from the <Items> list, comma-separated. Each id may optionally use :a for all or :N for a quantity, for example item_2:a or item_4:5.If quantity is omitted, it defaults to :a (all visible matching items). Do not return item names, explanations, or any items not listed in <Items>. Return an empty line if nothing visible fits the request.
/no_think
Answer directly. No reasoning.</System>
<UserUtterance>{{player_utterance}}</UserUtterance>
<Items>
{{items_xml}}</Items>
)";
}

std::string default_look_inventory_prompt_template()
{
    return R"(<System>You select items from the NPC inventory to wear, wield, or activate.Return a single line only.Format rules:To wear items, write:
wear: item1, item2
To wield items, write:
wield: item1
To activate items, write:
act: item1, item2
To drop items, write:
drop: item1, item2
You may include any combination, separated by |.Section labels are case-insensitive.Use item ids from the list only.If you need both wear and act, repeat the same id in both sections.
/no_think
Answer directly. No reasoning.</System>
<UserUtterance>{{player_utterance}}</UserUtterance>
<Inventory>
{{inventory_xml}}</Inventory>
<Examples>
  <Example>wear: item_1 | wield: item_2</Example>
  <Example>wield: item_2 | act: item_3</Example>
  <Example>wear: item_4</Example>
  <Example>act: item_3</Example>
</Examples>
)";
}

std::string build_look_around_prompt( const std::string &player_utterance,
                                      const std::vector<look_around_item_entry> &items )
{
    std::ostringstream items_xml;
    for( const look_around_item_entry &entry : items ) {
        items_xml << "  <Item id=\"" << xml_escape( entry.id ) << "\" name=\""
                  << xml_escape( entry.name ) << "\" qty=\""
                  << entry.quantity << "\"/>\n";
    }
    const std::string templ = load_llm_prompt_template( look_around_prompt_filename,
                              default_look_around_prompt_template(),
    { "{{player_utterance}}", "{{items_xml}}" } );
    return render_prompt_template( templ,
    {
        { "{{player_utterance}}", xml_escape( player_utterance ) },
        { "{{items_xml}}", items_xml.str() }
    } );
}

std::string build_look_inventory_prompt( const std::string &player_utterance,
        const std::vector<inventory_item_entry> &inventory )
{
    std::ostringstream inventory_xml;
    for( const inventory_item_entry &entry : inventory ) {
        inventory_xml << "  <Item id=\"" << xml_escape( entry.id ) << "\" name=\""
                      << xml_escape( entry.name ) << "\"/>\n";
    }
    const std::string templ = load_llm_prompt_template( look_inventory_prompt_filename,
                              default_look_inventory_prompt_template(),
    { "{{player_utterance}}", "{{inventory_xml}}" } );
    return render_prompt_template( templ,
    {
        { "{{player_utterance}}", xml_escape( player_utterance ) },
        { "{{inventory_xml}}", inventory_xml.str() }
    } );
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

std::vector<look_around_selection> parse_look_around_response( const std::string &text,
        const std::unordered_map<std::string, std::string> &allowed )
{
    std::string cleaned = trim_copy( strip_xml_tags( text ) );
    if( cleaned.empty() ) {
        return {};
    }
    std::replace( cleaned.begin(), cleaned.end(), '\n', ',' );
    std::vector<look_around_selection> results;
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
        std::string qty_token;
        size_t colon = token.find( ':' );
        if( colon != std::string::npos ) {
            qty_token = trim_copy( token.substr( colon + 1 ) );
            token = trim_copy( token.substr( 0, colon ) );
        }
        std::string lowered = lower_copy( token );
        auto it = allowed.find( lowered );
        if( it == allowed.end() ) {
            continue;
        }
        int quantity = -1;
        if( !qty_token.empty() ) {
            const std::string qty_lower = lower_copy( qty_token );
            if( qty_lower != "a" ) {
                quantity = std::max( 1, atoi( qty_lower.c_str() ) );
            }
        }
        const std::string &normalized = it->second;
        auto existing = std::find_if( results.begin(), results.end(), [&]( const look_around_selection &sel ) {
            return sel.name == normalized;
        } );
        if( existing == results.end() ) {
            results.push_back( look_around_selection{ normalized, quantity } );
            if( results.size() >= 3 ) {
                break;
            }
        } else {
            existing->quantity = quantity;
        }
    }
    return results;
}

look_inventory_selection parse_look_inventory_response( const std::string &text,
        const std::unordered_map<std::string, std::string> &allowed )
{
    look_inventory_selection selection;
    std::string cleaned = trim_copy( strip_xml_tags( text ) );
    if( cleaned.empty() ) {
        return selection;
    }
    std::vector<std::string> segments;
    size_t start = 0;
    while( start < cleaned.size() ) {
        size_t end = cleaned.find( '|', start );
        if( end == std::string::npos ) {
            end = cleaned.size();
        }
        segments.push_back( trim_copy( cleaned.substr( start, end - start ) ) );
        start = end + 1;
    }

    auto add_items = [&]( const std::string & label, const std::string & items_text ) {
        std::vector<std::string> *bucket = nullptr;
        if( label == "wear" ) {
            bucket = &selection.wear;
        } else if( label == "wield" ) {
            bucket = &selection.wield;
        } else if( label == "act" ) {
            bucket = &selection.act;
        } else if( label == "drop" ) {
            bucket = &selection.drop;
        } else {
            return;
        }
        size_t pos = 0;
        while( pos < items_text.size() ) {
            size_t end = items_text.find( ',', pos );
            if( end == std::string::npos ) {
                end = items_text.size();
            }
            std::string token = trim_copy( items_text.substr( pos, end - pos ) );
            pos = end + 1;
            if( token.empty() ) {
                continue;
            }
            std::string lowered = lower_copy( token );
            auto it = allowed.find( lowered );
            if( it == allowed.end() ) {
                continue;
            }
            const std::string &normalized = it->second;
            if( std::find( bucket->begin(), bucket->end(), normalized ) == bucket->end() ) {
                bucket->push_back( normalized );
            }
        }
    };

    for( const std::string &segment : segments ) {
        if( segment.empty() ) {
            continue;
        }
        const size_t colon = segment.find( ':' );
        if( colon == std::string::npos ) {
            continue;
        }
        std::string label = lower_copy( trim_copy( segment.substr( 0, colon ) ) );
        std::string items_text = trim_copy( segment.substr( colon + 1 ) );
        add_items( label, items_text );
    }

    return selection;
}

void apply_look_inventory_actions( npc &listener,
                                   const std::string &request_id,
                                   const std::unordered_map<std::string, item *> &inventory,
                                   const std::unordered_map<std::string, std::string> &id_to_name,
                                   const look_inventory_selection &selection )
{
    auto item_label = [&]( const std::string &item_name ) {
        const auto name_it = id_to_name.find( item_name );
        return name_it == id_to_name.end() ? item_name : name_it->second;
    };

    auto begin_inventory_action = [&]( const std::string &item_name ) {
        listener.begin_llm_action( npc::llm_action_kind::look_inventory,
                                   item_name,
                                   item_label( item_name ),
                                   std::nullopt,
                                   request_id );
        listener.update_llm_action_phase( npc::llm_action_phase::precheck );
    };

    for( const std::string &name : selection.wear ) {
        begin_inventory_action( name );
        auto it = inventory.find( name );
        if( it == inventory.end() ) {
            listener.finish_llm_action( npc::llm_action_phase::blocked, "inventory.item_missing" );
            continue;
        }
        if( !listener.can_wear( *it->second ).success() ) {
            listener.finish_llm_action( npc::llm_action_phase::blocked, "inventory.cannot_wear" );
            continue;
        }
        listener.update_llm_action_phase( npc::llm_action_phase::executing );
        if( listener.wear_item( *it->second, false ).has_value() ) {
            listener.finish_llm_action( npc::llm_action_phase::completed );
        } else {
            listener.finish_llm_action( npc::llm_action_phase::failed, "inventory.wear_failed" );
        }
    }

    for( const std::string &name : selection.wield ) {
        begin_inventory_action( name );
        auto it = inventory.find( name );
        if( it == inventory.end() ) {
            listener.finish_llm_action( npc::llm_action_phase::blocked, "inventory.item_missing" );
            continue;
        }
        if( !listener.can_wield( *it->second ).success() ) {
            listener.finish_llm_action( npc::llm_action_phase::blocked, "inventory.cannot_wield" );
            continue;
        }
        listener.update_llm_action_phase( npc::llm_action_phase::executing );
        if( listener.wield( *it->second ) ) {
            listener.finish_llm_action( npc::llm_action_phase::completed );
        } else {
            listener.finish_llm_action( npc::llm_action_phase::failed, "inventory.wield_failed" );
        }
    }

    for( const std::string &name : selection.act ) {
        begin_inventory_action( name );
        auto it = inventory.find( name );
        if( it == inventory.end() ) {
            listener.finish_llm_action( npc::llm_action_phase::blocked, "inventory.item_missing" );
            continue;
        }
        listener.update_llm_action_phase( npc::llm_action_phase::executing );
        listener.activate_item( *it->second );
        listener.finish_llm_action( npc::llm_action_phase::completed, "",
        {
            "activation_attempted=true"
        } );
    }

    for( const std::string &name : selection.drop ) {
        begin_inventory_action( name );
        auto it = inventory.find( name );
        if( it == inventory.end() ) {
            listener.finish_llm_action( npc::llm_action_phase::blocked, "inventory.item_missing" );
            continue;
        }
        item_location loc( listener, it->second );
        const int count = it->second->count_by_charges() ? std::max( 1, it->second->charges ) : 1;
        drop_locations items;
        items.emplace_back( loc, count );
        listener.update_llm_action_phase( npc::llm_action_phase::executing );
        listener.drop( items, listener.pos_bub(), false );
        listener.finish_llm_action( npc::llm_action_phase::completed, "",
        {
            string_format( "drop_count=%d", count ),
            "drop_attempted=true"
        } );
    }
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

std::string extract_ambient_speech( const std::string &text )
{
    std::string cleaned = extract_csv_from_text( text );
    if( cleaned.empty() ) {
        return {};
    }
    std::string speech = strip_speaker_prefix( extract_speech_field( cleaned ) );
    const size_t newline = speech.find( '\n' );
    if( newline != std::string::npos ) {
        speech = trim_copy( speech.substr( 0, newline ) );
    }
    return strip_wrapping_quotes( speech );
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
                normalize_wait_here_hold_position_pair( actions );
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
    const std::string stripped = strip_think_output( text );
    std::istringstream lines( stripped );
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

int threat_level_for_snapshot( npc &listener, const Creature &critter )
{
    const int distance = rl_dist( listener.pos_bub(), critter.pos_bub() );
    const double threat = threat_score_for( listener, critter, distance );
    const double max_threat = critter.as_monster() ? static_cast<double>( NPC_MONSTER_DANGER_MAX ) :
                              static_cast<double>( NPC_CHARACTER_DANGER_MAX );
    if( max_threat <= 0.0 ) {
        return 0;
    }
    const double clamped = std::clamp( threat, 0.0, max_threat );
    return static_cast<int>( std::round( ( clamped / max_threat ) * 10.0 ) );
}

std::string attitude_label_for_snapshot( npc &listener, const Creature &critter )
{
    switch( listener.attitude_to( critter ) ) {
        case Creature::Attitude::HOSTILE:
            return "hostile";
        case Creature::Attitude::FRIENDLY:
            return "friendly";
        case Creature::Attitude::NEUTRAL:
            return "neutral";
        case Creature::Attitude::ANY:
            break;
    }
    return "unknown";
}

std::string creature_legend_entry( npc &listener, const Creature &critter )
{
    const std::string name = critter.is_avatar() ? "player" :
                             strip_leading_article( sanitize_text( critter.disp_name() ) );
    return string_format( "%s %s threat=%d/10", name, attitude_label_for_snapshot( listener, critter ),
                          threat_level_for_snapshot( listener, critter ) );
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

map_snapshot build_ascii_map_snapshot( npc &listener, const std::string &request_id )
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
            char glyph = ' ';
            if( p == center ) {
                glyph = '|';
            } else if( !here.inbounds( p ) ) {
                glyph = ' ';
            } else if( !listener.sees( here, p ) ) {
                glyph = ' ';
            } else {
                const int cost = here.move_cost( p );
                const int scaled_cost = cost * 50;
                if( Creature *critter = get_creature_tracker().creature_at( p ) ) {
                    if( !listener.sees( here, *critter ) ) {
                        glyph = cost <= 0 ? '6' : ( scaled_cost > 100 ? '0' : '-' );
                    } else if( critter->is_avatar() && player_letter_active ) {
                        auto found = letter_map.find( critter );
                        if( found == letter_map.end() ) {
                            const char base_letter = 'a';
                            const char letter = cost > 100 ||
                                                cost <= 0 ? static_cast<char>( std::toupper( base_letter ) ) : base_letter;
                            letter_map.emplace( critter, letter );
                            legend_entries.emplace_back( letter, creature_legend_entry( listener, *critter ) );
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
                                                             creature_legend_entry( listener, *critter ) );
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
                } else {
                    glyph = '-';
                }
            }
            out_map.push_back( glyph );
        }
        out_map.push_back( '\n' );
    }
    map_snapshot out;
    out.map = std::move( out_map );
    out.legend = build_map_legend( legend_entries );
    listener.set_llm_intent_legend_map( request_id, std::move( legend_targets ) );
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
    out << "id: " << request_id << "\n";
    out << "player name: " << sanitize_text( get_player_character().get_name() ) << "\n";
    const std::string utterance_line = sanitize_text( player_utterance );
    const bool has_player_utterance = !trim_copy( utterance_line ).empty();
    out << "player utterance present: " << ( has_player_utterance ? "true" : "false" ) << "\n";
    out << "player utterance: " << utterance_line << "\n\n";
    const std::vector<npc::llm_intent_memory_entry> memory = listener.get_llm_intent_memory();
    const std::vector<npc::llm_overheard_memory_entry> overheard = listener.get_llm_overheard_memory();
    out << "Recent conversation newest first:\n";
    if( memory.empty() && overheard.empty() ) {
        out << "(none)\n\n";
    } else {
        const size_t line_count = std::max( memory.size(), overheard.size() );
        for( size_t i = 0; i < line_count; ++i ) {
            out << "-";
            bool has_hours = false;
            bool has_segment = false;
            if( i < memory.size() ) {
                const npc::llm_intent_memory_entry &entry = memory[memory.size() - 1 - i];
                const time_duration age = calendar::turn - entry.turn;
                const double hours_ago = to_hours<double>( age );
                std::ostringstream hours_stream;
                hours_stream << std::fixed << std::setprecision( 1 ) << hours_ago;
                out << " hours_ago=" << hours_stream.str();
                has_hours = true;
                if( !entry.player_utterance.empty() ) {
                    out << " player:\"" << sanitize_text( entry.player_utterance ) << "\"";
                    has_segment = true;
                }
                if( !entry.npc_response.empty() ) {
                    out << " You:\"" << sanitize_text( entry.npc_response ) << "\"";
                    has_segment = true;
                }
                if( !entry.actions.empty() ) {
                    out << " actions=[" << join_action_tokens( entry.actions ) << "]";
                    has_segment = true;
                }
            }
            if( i < overheard.size() ) {
                const npc::llm_overheard_memory_entry &entry = overheard[overheard.size() - 1 - i];
                if( !has_hours ) {
                    const time_duration age = calendar::turn - entry.turn;
                    const double hours_ago = to_hours<double>( age );
                    std::ostringstream hours_stream;
                    hours_stream << std::fixed << std::setprecision( 1 ) << hours_ago;
                    out << " hours_ago=" << hours_stream.str();
                }
                if( has_segment ) {
                    out << " |";
                }
                if( !entry.npc_name.empty() ) {
                    out << " " << sanitize_text( entry.npc_name ) << ":\"";
                    out << sanitize_text( entry.npc_response ) << "\"";
                } else if( !entry.npc_response.empty() ) {
                    out << " Ally:\"" << sanitize_text( entry.npc_response ) << "\"";
                }
                if( !entry.actions.empty() ) {
                    out << " actions=[" << join_action_tokens( entry.actions ) << "]";
                }
            }
            out << "\n";
        }
        out << "\n";
    }
    out << "your_name: " << sanitize_text( listener.get_name() ) << "\n";
    const std::string profession = sanitize_text( listener.disp_profession() );
    out << "your_profession: " << ( profession.empty() ? "no_past" : profession ) << "\n";
    const background_summary_entry background_summary = get_background_summary_for( listener );
    if( !background_summary.background.empty() ) {
        out << "your_tone: " << background_summary.background << "\n";
    }
    if( !background_summary.expression.empty() ) {
        out << "your_example_expression: " << background_summary.expression << "\n";
    }
    out << "your_follow_mode: " << follower_mode_snapshot_token( listener ) << "\n";
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

    out << "wielded: ";
    if( wielded ) {
        const item &weapon = *wielded;
        out << "wielded=\"" << sanitize_text( weapon.tname() ) << "\"";
    } else {
        out << "wielded=none";
    }
    out << "\n";

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
        if( combat_guns.size() >= max_items &&
            combat_melee.size() >= max_items ) {
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );

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

    out << "weapons: [";
    for( size_t i = 0; i < combat_items.size(); ++i ) {
        if( i > 0 ) {
            out << ", ";
        }
        out << combat_items[i];
    }
    out << "]\n";
    out << "bandage_possible: " << ( bandage_possible ? "true" : "false" ) << "\n\n";

    const map_snapshot map_data = build_ascii_map_snapshot( listener, request_id );
    out << "legend:\n" << build_snapshot_legend();
    out << "creature legend with attitude and threat level:\n";
    if( map_data.legend.empty() ) {
        out << "(none)\n";
    } else {
        out << map_data.legend << "\n";
    }
    out << "map:\n" << map_data.map;
    return out.str();
}

std::string default_npc_action_prompt_template()
{
    return R"(Situation:
{{snapshot}}
<System>You are controlling a human survivor NPC in a cataclysmic world, exhausted, armed, and trying not to die.Return a single line only, with correct syntax, to be parsed by the game.This line has two to four fields separated by ‘|’ :
<Field 1>The first field is an answer to player_utterance.If player_utterance_present is false, this is a spontaneous check-in with no direct player input.You have decided to team up with the player for now, and must answer as the NPC.Stick to your role, with your emotions and opinions.</Field 1>
<Fields 2-4>Write 1-3 of the following allowed actions exactly:
{{action_list_with_target}}
<Explanation allowed actions>
Snapshot field your_follow_mode can be follow-close, follow-afar, or guard/hold.
If the player directly asks you to wait, stay, hold, guard, or stop, use 'wait_here' or 'hold_position' instead of follow actions.
Only use 'idle' when no action change is needed. Do not use 'idle' as a substitute for wait/hold commands.
'wait_here' to stay put, wait here, stand by, and remain with no special tactical position. Use this for ordinary requests to wait or stay.
'hold_position' to hold this exact spot tactically: a doorway, corner, choke point, post, or guarded position. Use this only for explicit hold-this-position / keep-watch / stakeout style orders, not ordinary waiting.
'follow_close' to walk behind, follow close, come here.
'follow_far' to follow from farther away, hang back, give me space.
'equip_gun' to equip gun, rifle, thrower, get ready to shoot.
'equip_melee' to equip melee, get ready to bash, cut, kick, stab.
'equip_bow' to use bow, crossbow, stealth.
'panic_on' to start running, get out of here.
'panic_off' if convincing, to stop fleeing and get your act together.
'look_around' to view your surroundings and pick-up, grab, search, explore for items around you.
'look_inventory' to look inside your inventory and wear/wield/activate items.
'move: <coordinate> <coordinate> ... <state>' to move step-by-step on your snapshot map. Use N, S, E, W, NE, NW, SE and SW and chain 4 to 15 coordinates. After the coordinates you must also include either 'wait_here' or 'hold_position' to set state.
'attack=<target>' to attack a target with the letter from your map. Any creature with a map letter is a valid explicit target handle, including the player, friendlies, neutrals, and hostiles.
'idle' if none of the above.
</Explanation allowed actions>
</Fields 2-4>
Print only Fields 1-4, separated by | .If you break this format, you have failed.Output a single line with an answer and actions from the allowed list, in fields separated by ‘|’ and no additional text.
<Example Output 1>Blow me.|idle</Example Output 1>
<Example Output 2>Lets put those fucks in the ground.|equip_melee|attack=a</Example Output 2>
<Example Output 3>Providing cover!|wait_here|equip_gun</Example Output 3>
<Example Output 4>Lets get some dinner!|equip_gun|attack=b</Example Output 4>
<Example Output 5>Don't worry, I'm ready to kick some teeth in.|equip_melee</Example Output 5>
<Example Output 6>Locked and loaded.|equip_gun</Example Output 6>
<Example Output 7>Nope, not doing that!|panic_on</Example Output 7>
<Example Output 8>Moving down and holding there.|move: S S S S S hold_position|equip_gun</Example Output 8>
/no_think
Answer directly. No reasoning.
</System>
)";
}

std::string default_npc_ambient_prompt_template()
{
    return R"(Situation:
{{snapshot}}
<System>You are {{npc_name}}, a human survivor NPC speaking to another person in a cataclysmic world.You are not allies, and eyeing them.Even if they seem nice, you never know these days. Everybody does things to survive.Reply deeply in character, informed by the snapshot: your background, your tone, your opinions of the player, and your recent memories.Return exactly one short spoken reply only: 1-3 sentences, no narration, no bullet points, no stage directions, no action tokens, no pipe-separated action line, no tool calls, no menu syntax.If you are unsure, answer briefly and naturally instead of inventing details./no_think
Answer directly. No reasoning.
</System>
<PlayerUtterance>{{player_utterance}}</PlayerUtterance>
)";
}

std::string build_prompt( const std::string &npc_name, const std::string &player_utterance,
                          const std::string &snapshot )
{
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
    action_list_with_target += "attack=<target>, move: <coordinate> <coordinate> ... <state>";
    const std::string templ = load_llm_prompt_template( npc_action_prompt_filename,
                              default_npc_action_prompt_template(),
    { "{{snapshot}}", "{{action_list_with_target}}" } );
    return render_prompt_template( templ,
    {
        { "{{snapshot}}", snapshot },
        { "{{action_list_with_target}}", action_list_with_target },
        { "{{npc_name}}", sanitize_text( npc_name ) },
        { "{{player_utterance}}", sanitize_text( player_utterance ) }
    } );
}

std::string build_ambient_prompt( const std::string &npc_name,
                                  const std::string &player_utterance,
                                  const std::string &snapshot )
{
    const std::string templ = load_llm_prompt_template( npc_ambient_prompt_filename,
                              default_npc_ambient_prompt_template(),
    { "{{snapshot}}", "{{npc_name}}", "{{player_utterance}}" } );
    return render_prompt_template( templ,
    {
        { "{{snapshot}}", snapshot },
        { "{{npc_name}}", sanitize_text( npc_name ) },
        { "{{player_utterance}}", sanitize_text( player_utterance ) }
    } );
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

[[maybe_unused]] std::string resolve_python_from_venv_or_exe( const std::string &path )
{
    std::filesystem::path p = resolve_path( path );
    if( p.empty() ) {
        return path;
    }
    if( std::filesystem::is_directory( p ) ) {
#if defined(_WIN32)
        std::filesystem::path candidate = p / "Scripts" / "python.exe";
        if( std::filesystem::exists( candidate ) ) {
            return candidate.string();
        }
#else
        for( const char *name : { "python3", "python" } ) {
            std::filesystem::path candidate = p / "bin" / name;
            if( std::filesystem::exists( candidate ) ) {
                return candidate.string();
            }
        }
#endif
    }
    return p.string();
}

[[maybe_unused]] runner_config current_runner_config()
{
    static constexpr int default_max_tokens = 20000;
    static constexpr int default_max_prompt_len = 4096;
    runner_config cfg;
    cfg.backend = get_option<std::string>( "LLM_INTENT_BACKEND" );
    cfg.runner_path = "tools/llm_runner/runner.py";
    cfg.model_dir = get_option<std::string>( "LLM_INTENT_MODEL_DIR" );
    cfg.device = get_option<std::string>( "LLM_INTENT_DEVICE" );
    cfg.use_api = cfg.backend == "api" || get_option<bool>( "LLM_INTENT_USE_API" );
    cfg.api_key_env = get_option<std::string>( "LLM_INTENT_API_KEY_ENV" );
    cfg.api_provider = "openai";
    cfg.api_model = get_option<std::string>( "LLM_INTENT_API_MODEL" );
    cfg.ollama_url = get_option<std::string>( "LLM_INTENT_OLLAMA_URL" );
    cfg.ollama_model = get_option<std::string>( "LLM_INTENT_OLLAMA_MODEL" );

    const std::string configured_python = resolve_python_from_venv_or_exe(
                                           get_option<std::string>( "LLM_INTENT_PYTHON" ) );
    if( !configured_python.empty() ) {
        cfg.python_path = configured_python;
    } else if( cfg.backend == "api" ) {
        const std::string api_venv = resolve_python_from_venv_or_exe( "/Users/josefhorvath/ollama/api_env311" );
        if( !api_venv.empty() && std::filesystem::exists( api_venv ) ) {
            cfg.python_path = api_venv;
        } else {
#if defined(_WIN32)
            cfg.python_path = "python";
#else
            cfg.python_path = "/usr/bin/python3";
#endif
        }
    } else if( cfg.backend == "ollama" ) {
#if defined(_WIN32)
        cfg.python_path = "python";
#else
        cfg.python_path = "/usr/bin/python3";
#endif
    } else {
        cfg.python_path = configured_python;
    }

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
            const bool use_api_mode = config.use_api || backend == "api";
            const bool use_ollama_mode = backend == "ollama";
            const bool needs_model = !use_api_mode && !use_ollama_mode;

            if( config.python_path.empty() || config.runner_path.empty() ||
                ( config.model_dir.empty() && needs_model ) ) {
                error = "LLM runner configuration is incomplete.";
                return false;
            }

            std::filesystem::path python_path = resolve_path( config.python_path );
            std::filesystem::path runner_path = resolve_path( config.runner_path );
            std::filesystem::path model_dir = resolve_path( config.model_dir );
            std::filesystem::path log_path = central_llm_log_path( "llm_intent_runner.log" );
            std::error_code mkdir_ec;
            std::filesystem::create_directories( central_llm_config_dir_path(), mkdir_ec );

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
            } else if( use_ollama_mode ) {
                args.push_back( "--ollama-url" );
                args.push_back( config.ollama_url.empty() ? "http://127.0.0.1:11434" : config.ollama_url );
                args.push_back( "--ollama-model" );
                args.push_back( config.ollama_model );
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
            const bool use_api_mode = config.use_api || backend == "api";
            const bool use_ollama_mode = backend == "ollama";
            const bool needs_model = !use_api_mode && !use_ollama_mode;

            if( config.python_path.empty() || config.runner_path.empty() ||
                ( config.model_dir.empty() && needs_model ) ) {
                error = "LLM runner configuration is incomplete.";
                return false;
            }

            std::filesystem::path python_path = resolve_path( config.python_path );
            std::filesystem::path runner_path = resolve_path( config.runner_path );
            std::filesystem::path model_dir = resolve_path( config.model_dir );
            std::filesystem::path log_path = central_llm_log_path( "llm_intent_runner.log" );
            std::error_code mkdir_ec;
            std::filesystem::create_directories( central_llm_config_dir_path(), mkdir_ec );

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
            } else if( use_ollama_mode ) {
                args.push_back( "--ollama-url" );
                args.push_back( config.ollama_url.empty() ? "http://127.0.0.1:11434" : config.ollama_url );
                args.push_back( "--ollama-model" );
                args.push_back( config.ollama_model );
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
        struct pending_primary_request {
            character_id npc_id;
            std::string player_utterance;
        };
        struct look_around_context {
            character_id npc_id;
            std::string npc_name;
            std::vector<look_around_item_entry> items;
        };
        struct look_inventory_context {
            character_id npc_id;
            std::string npc_name;
            std::vector<inventory_item_entry> items;
        };

    public:
        llm_intent_manager() = default;

        ~llm_intent_manager() {
            stop();
        }

        void enqueue_request( npc &listener, const std::string &player_utterance ) {
            queue_primary_request( listener, player_utterance );
        }

        void enqueue_ambient_request( npc &listener, const std::string &player_utterance ) {
            queue_ambient_request( listener, player_utterance );
        }

        void enqueue_requests_serial( const std::vector<npc *> &listeners,
                                      const std::string &player_utterance ) {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                for( npc *listener : listeners ) {
                    if( listener == nullptr ) {
                        continue;
                    }
                    if( pending_primary_npcs.count( listener->getID() ) > 0 ) {
                        continue;
                    }
                    pending_primary_npcs.insert( listener->getID() );
                    pending_primary_requests.push_back( { listener->getID(), player_utterance } );
                }
            }
            dispatch_next_serial_primary_request();
        }

        void queue_primary_request( npc &listener, const std::string &player_utterance ) {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                if( pending_primary_npcs.count( listener.getID() ) > 0 ) {
                    return;
                }
                pending_primary_npcs.insert( listener.getID() );
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
                snapshot_origin_by_request[req.request_id] = listener.pos_abs();
                primary_request_ids.insert( req.request_id );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void queue_ambient_request( npc &listener, const std::string &player_utterance ) {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                if( pending_ambient_npcs.count( listener.getID() ) > 0 ) {
                    return;
                }
                pending_ambient_npcs.insert( listener.getID() );
            }
            static constexpr int ambient_max_tokens = 256;
            llm_intent_request req;
            req.request_id = next_request_id();
            req.npc_id = listener.getID();
            req.npc_name = listener.get_name();
            req.snapshot = build_snapshot_json( listener, player_utterance, req.request_id );
            req.prompt = build_ambient_prompt( req.npc_name, player_utterance, req.snapshot );
            req.max_tokens = ambient_max_tokens;
            req.temperature = get_option<float>( "LLM_INTENT_TEMPERATURE" );
            req.top_p = get_option<float>( "LLM_INTENT_TOP_P" );
            req.repetition_penalty = get_option<float>( "LLM_INTENT_REPETITION_PENALTY" );
            if( get_option<bool>( "DEBUG_LLM_INTENT_LOG" ) ) {
                append_llm_intent_log( string_format( "ambient prompt %s (%s)\n%s\n\n",
                                                      req.npc_name, req.request_id, req.prompt ) );
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                utterance_by_request[req.request_id] = player_utterance;
                ambient_request_ids.insert( req.request_id );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void dispatch_next_serial_primary_request() {
            while( true ) {
                pending_primary_request pending;
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    if( !serial_primary_request_ids.empty() || pending_primary_requests.empty() ) {
                        return;
                    }
                    pending = std::move( pending_primary_requests.front() );
                    pending_primary_requests.pop_front();
                }
                npc *listener = g ? g->find_npc( pending.npc_id ) : nullptr;
                if( listener == nullptr || !listener->is_player_ally() ) {
                    std::lock_guard<std::mutex> lock( mutex );
                    pending_primary_npcs.erase( pending.npc_id );
                    continue;
                }
                if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                    std::lock_guard<std::mutex> lock( mutex );
                    pending_primary_npcs.erase( pending.npc_id );
                    while( !pending_primary_requests.empty() ) {
                        pending_primary_npcs.erase( pending_primary_requests.front().npc_id );
                        pending_primary_requests.pop_front();
                    }
                    return;
                }
                static constexpr int default_max_tokens = 20000;
                llm_intent_request req;
                req.request_id = next_request_id();
                req.npc_id = listener->getID();
                req.npc_name = listener->get_name();
                req.snapshot = build_snapshot_json( *listener, pending.player_utterance, req.request_id );
                req.prompt = build_prompt( req.npc_name, pending.player_utterance, req.snapshot );
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
                    utterance_by_request[req.request_id] = pending.player_utterance;
                    snapshot_origin_by_request[req.request_id] = listener->pos_abs();
                    primary_request_ids.insert( req.request_id );
                    pending_primary_npcs.insert( req.npc_id );
                    serial_primary_request_ids.insert( req.request_id );
                    request_queue.push( std::move( req ) );
                }
                ensure_worker();
                cv.notify_one();
                return;
            }
        }

        bool has_pending_primary_request_for( const character_id &npc_id ) {
            std::lock_guard<std::mutex> lock( mutex );
            return pending_primary_npcs.count( npc_id ) > 0;
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
            for( size_t i = 0; i < items.size(); ++i ) {
                items[i].id = string_format( "item_%d", static_cast<int>( i + 1 ) );
            }
            llm_intent_request req;
            req.request_id = next_request_id();
            req.npc_id = listener.getID();
            req.npc_name = listener.get_name();
            req.snapshot = "{}";
            req.prompt = build_look_around_prompt( player_utterance, items );
            req.max_tokens = look_max_tokens;
            req.temperature = get_option<float>( "LLM_INTENT_TEMPERATURE" );
            req.top_p = get_option<float>( "LLM_INTENT_TOP_P" );
            req.repetition_penalty = get_option<float>( "LLM_INTENT_REPETITION_PENALTY" );

            look_around_context context;
            context.npc_id = req.npc_id;
            context.npc_name = req.npc_name;
            context.items = items;

            append_llm_intent_log( string_format( "look_around request %s (%s)\n%s\n\n",
                                                  req.npc_name,
                                                  req.request_id,
                                                  request_to_json( req ) ) );
            {
                std::lock_guard<std::mutex> lock( mutex );
                look_around_requests.emplace( req.request_id, std::move( context ) );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void process_look_around_response( const llm_intent_response &resp,
                                           const look_around_context &context ) {
            std::unordered_map<std::string, std::string> allowed;
            allowed.reserve( context.items.size() * 2 );
            std::unordered_map<std::string, std::string> id_to_name;
            id_to_name.reserve( context.items.size() );
            std::unordered_map<std::string, std::string> name_to_id;
            name_to_id.reserve( context.items.size() );
            for( const look_around_item_entry &entry : context.items ) {
                const std::string id_key = lower_copy( entry.id );
                const std::string name_key = lower_copy( entry.name );
                allowed.emplace( id_key, entry.name );
                allowed.emplace( name_key, entry.name );
                id_to_name.emplace( entry.id, entry.name );
                name_to_id.emplace( entry.name, entry.id );
            }
            std::vector<look_around_selection> selected;
            if( resp.ok ) {
                selected = parse_look_around_response( resp.text, allowed );
            }
            {
                const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                append_llm_intent_log( string_format( "look_around response %s (%s)\n%s\n\n",
                                                      context.npc_name, resp.request_id, payload ) );
                if( !selected.empty() ) {
                    std::string joined;
                    for( const look_around_selection &sel : selected ) {
                        const std::string &name = sel.name;
                        const auto id_it = name_to_id.find( name );
                        if( !joined.empty() ) {
                            joined += ", ";
                        }
                        if( id_it == name_to_id.end() ) {
                            joined += name;
                        } else {
                            joined += id_it->second + ( sel.quantity < 0 ? ":a: " : string_format(":%d: ", sel.quantity ) ) + name;
                        }
                    }
                    append_llm_intent_log( string_format( "look_around selected %s (%s): %s\n\n",
                                                          context.npc_name, resp.request_id, joined ) );
                }
            }
            if( npc *target = g->find_npc( context.npc_id ) ) {
                if( target->is_player_ally() ) {
                    std::vector<npc::llm_item_target> targets;
                    targets.reserve( selected.size() );
                    for( const look_around_selection &sel : selected ) {
                        targets.push_back( npc::llm_item_target{ sel.name, sel.quantity } );
                    }
                    target->set_llm_intent_item_targets( targets );
                }
            }
        }

        void enqueue_look_inventory_request( npc &listener, const std::string &player_utterance ) {
            static constexpr int look_max_tokens = 256;
            static constexpr size_t max_inventory_entries = 80;
            std::vector<inventory_item_entry> inventory = collect_inventory_entries( listener,
                    max_inventory_entries );
            if( inventory.empty() ) {
                return;
            }
            llm_intent_request req;
            req.request_id = next_request_id();
            req.npc_id = listener.getID();
            req.npc_name = listener.get_name();
            req.snapshot = "{}";
            req.prompt = build_look_inventory_prompt( player_utterance, inventory );
            req.max_tokens = look_max_tokens;
            req.temperature = get_option<float>( "LLM_INTENT_TEMPERATURE" );
            req.top_p = get_option<float>( "LLM_INTENT_TOP_P" );
            req.repetition_penalty = get_option<float>( "LLM_INTENT_REPETITION_PENALTY" );

            look_inventory_context context;
            context.npc_id = req.npc_id;
            context.npc_name = req.npc_name;
            context.items = inventory;

            append_llm_intent_log( string_format( "look_inventory request %s (%s)\n%s\n\n",
                                                  req.npc_name,
                                                  req.request_id,
                                                  request_to_json( req ) ) );
            {
                std::lock_guard<std::mutex> lock( mutex );
                look_inventory_requests.emplace( req.request_id, std::move( context ) );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void process_look_inventory_response( const llm_intent_response &resp,
                                              const look_inventory_context &context ) {
            std::unordered_map<std::string, std::string> allowed;
            allowed.reserve( context.items.size() * 2 );
            std::unordered_map<std::string, item *> inventory;
            inventory.reserve( context.items.size() );
            std::unordered_map<std::string, std::string> id_to_name;
            id_to_name.reserve( context.items.size() );
            for( const inventory_item_entry &entry : context.items ) {
                const std::string id_key = lower_copy( entry.id );
                const std::string name_key = lower_copy( entry.name );
                allowed.emplace( id_key, entry.id );
                allowed.emplace( name_key, entry.id );
                inventory.emplace( entry.id, entry.ptr );
                id_to_name.emplace( entry.id, entry.name );
            }
            look_inventory_selection selection;
            if( resp.ok ) {
                selection = parse_look_inventory_response( resp.text, allowed );
            }
            {
                const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                append_llm_intent_log( string_format( "look_inventory response %s (%s)\n%s\n\n",
                                                      context.npc_name, resp.request_id, payload ) );
            }
            if( npc *target = g->find_npc( context.npc_id ) ) {
                if( target->is_player_ally() ) {
                    apply_look_inventory_actions( *target, resp.request_id, inventory, id_to_name, selection );
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
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    auto it = look_inventory_requests.find( resp.request_id );
                    if( it != look_inventory_requests.end() ) {
                        look_inventory_context context = std::move( it->second );
                        look_inventory_requests.erase( it );
                        process_look_inventory_response( resp, context );
                        local.pop();
                        continue;
                    }
                }
                bool is_ambient_response = false;
                std::string player_utterance;
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    is_ambient_response = ambient_request_ids.count( resp.request_id ) > 0;
                    auto it = utterance_by_request.find( resp.request_id );
                    if( it != utterance_by_request.end() ) {
                        player_utterance = it->second;
                    }
                }
                if( is_ambient_response ) {
                    std::string ambient_error;
                    std::string ambient_speech;
                    if( resp.ok ) {
                        ambient_speech = extract_ambient_speech( resp.text );
                        if( ambient_speech.empty() ) {
                            ambient_error = "Ambient speech missing.";
                        }
                    } else {
                        ambient_error = resp.error;
                    }
                    if( ambient_error.empty() ) {
                        if( npc *target = g->find_npc( resp.npc_id ) ) {
                            add_msg( _( "%s says: \"%s\"" ), resp.npc_name, ambient_speech );
                            target->add_llm_intent_memory( player_utterance, ambient_speech, {} );
                            broadcast_overheard_memory( *target, ambient_speech, {} );
                        }
                        if( debug_log ) {
                            const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                            append_llm_intent_log( string_format( "ambient response %s (%s)\n%s\n\nsay %s (%s)\n%s\n\n",
                                                                  resp.npc_name, resp.request_id, payload,
                                                                  resp.npc_name, resp.request_id,
                                                                  ambient_speech ) );
                        }
                    } else if( debug_log ) {
                        const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                        append_llm_intent_log( string_format( "ambient failed %s (%s)\n%s\nraw:\n%s\n\n",
                                                              resp.npc_name, resp.request_id,
                                                              ambient_error, payload ) );
                    }
                    {
                        std::lock_guard<std::mutex> lock( mutex );
                        utterance_by_request.erase( resp.request_id );
                        snapshot_origin_by_request.erase( resp.request_id );
                        ambient_request_ids.erase( resp.request_id );
                        pending_ambient_npcs.erase( resp.npc_id );
                    }
                    local.pop();
                    continue;
                }
                std::string parse_error;
                std::string action_error;
                std::string speech;
                std::vector<std::string> actions;
                std::string attack_target;
                std::vector<std::string> move_coords;
                std::string move_terminal_state;
                std::optional<tripoint_abs_ms> snapshot_origin;
                std::string speak_text;
                std::string say_log_line;
                std::string say_failed_log_line;
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    auto it = utterance_by_request.find( resp.request_id );
                    if( it != utterance_by_request.end() ) {
                        player_utterance = it->second;
                    }
                    auto origin_it = snapshot_origin_by_request.find( resp.request_id );
                    if( origin_it != snapshot_origin_by_request.end() ) {
                        snapshot_origin = origin_it->second;
                    }
                }
                if( resp.ok ) {
                    std::string csv_text = extract_csv_from_text( resp.text );
                    csv_text = sanitize_llm_csv( csv_text );
                    speak_text = strip_speaker_prefix( extract_speech_field( csv_text ) );
                    if( !speak_text.empty() ) {
                        if( g->find_npc( resp.npc_id ) ) {
                            add_msg( _( "%s says: \"%s\"" ), resp.npc_name, speak_text );
                            if( debug_log ) {
                                say_log_line = string_format( "say %s (%s)\n%s\n\n",
                                                              resp.npc_name, resp.request_id, speak_text );
                            }
                        } else if( debug_log ) {
                            say_failed_log_line = string_format( "say failed %s (%s)\n%s\n\n",
                                                                 resp.npc_name, resp.request_id, speak_text );
                        }
                    }
                    bool parsed = false;
                    std::string normalized = normalize_csv_separators( csv_text );
                    normalized = sanitize_llm_csv( normalized );
                    parsed = parse_csv_payload( csv_text, speech, actions, attack_target,
                                                move_coords, move_terminal_state, parse_error );
                    if( !parsed && normalized != csv_text ) {
                        parsed = parse_csv_payload( normalized, speech, actions, attack_target,
                                                    move_coords, move_terminal_state, parse_error );
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

                std::vector<std::string> memory_actions;
                if( resp.ok && parse_error.empty() ) {
                    memory_actions = actions;
                    if( !attack_target.empty() ) {
                        memory_actions.push_back( "attack=" + attack_target );
                    }
                }

                const bool wants_look_around = std::find( actions.begin(), actions.end(),
                                               "look_around" ) != actions.end();
                if( wants_look_around ) {
                    actions.erase( std::remove( actions.begin(), actions.end(), "look_around" ),
                                   actions.end() );
                }
                const bool wants_look_inventory = std::find( actions.begin(), actions.end(),
                                                  "look_inventory" ) != actions.end();
                if( wants_look_inventory ) {
                    actions.erase( std::remove( actions.begin(), actions.end(), "look_inventory" ),
                                   actions.end() );
                }

                if( resp.ok && parse_error.empty() ) {
                    std::vector<llm_intent_action> intent_actions;
                    intent_actions.reserve( actions.size() );
                    for( const std::string &token : actions ) {
                        const llm_intent_action action = intent_action_from_token( token );
                        if( action != llm_intent_action::none ) {
                            intent_actions.push_back( action );
                        }
                    }
                    if( !intent_actions.empty() || !attack_target.empty() || !move_coords.empty() ) {
                        if( npc *target = g->find_npc( resp.npc_id ) ) {
                            if( target->is_player_ally() ) {
                                target->set_llm_intent_actions( intent_actions, resp.request_id, attack_target );
                                if( !move_coords.empty() ) {
                                    if( !snapshot_origin ) {
                                        snapshot_origin = target->pos_abs();
                                        llm_intent::log_event( string_format( "move target %s (%s): snapshot origin missing, falling back to current pos (%d,%d,%d)",
                                                                              target->get_name(), resp.request_id,
                                                                              snapshot_origin->x(), snapshot_origin->y(), snapshot_origin->z() ) );
                                    }
                                    std::vector<std::string> effective_coords = move_coords;
                                    if( effective_coords.size() == 1 ) {
                                        effective_coords.assign( 4, effective_coords.front() );
                                    } else if( effective_coords.size() == 2 ) {
                                        effective_coords = { effective_coords[0], effective_coords[0],
                                                             effective_coords[1], effective_coords[1] };
                                    }
                                    int dx = 0;
                                    int dy = 0;
                                    for( const std::string &coord : effective_coords ) {
                                        if( coord == "n" ) {
                                            dy -= 1;
                                        } else if( coord == "s" ) {
                                            dy += 1;
                                        } else if( coord == "e" ) {
                                            dx += 1;
                                        } else if( coord == "w" ) {
                                            dx -= 1;
                                        } else if( coord == "ne" ) {
                                            dx += 1;
                                            dy -= 1;
                                        } else if( coord == "nw" ) {
                                            dx -= 1;
                                            dy -= 1;
                                        } else if( coord == "se" ) {
                                            dx += 1;
                                            dy += 1;
                                        } else if( coord == "sw" ) {
                                            dx -= 1;
                                            dy += 1;
                                        }
                                    }
                                    const tripoint_abs_ms final_target( snapshot_origin->x() + dx,
                                                                        snapshot_origin->y() + dy,
                                                                        snapshot_origin->z() );
                                    const llm_intent_action arrival_state = move_terminal_state == "hold_position" ?
                                            llm_intent_action::hold_position : llm_intent_action::wait_here;
                                    llm_intent::log_event( string_format( "move target %s (%s): raw=[%s] effective=[%s] origin=(%d,%d,%d) final=(%d,%d,%d) arrival=%s",
                                                                          target->get_name(), resp.request_id,
                                                                          string_join( move_coords, "," ),
                                                                          string_join( effective_coords, "," ),
                                                                          snapshot_origin->x(), snapshot_origin->y(), snapshot_origin->z(),
                                                                          final_target.x(), final_target.y(), final_target.z(),
                                                                          move_terminal_state ) );
                                    target->set_llm_intent_move_target( final_target, arrival_state );
                                } else {
                                    target->set_llm_intent_move_target( std::nullopt, llm_intent_action::none );
                                }
                            }
                        }
                    }
                }
                if( resp.ok && parse_error.empty() ) {
                    const std::string memory_speech = !speech.empty() ? speech : speak_text;
                    if( npc *target = g->find_npc( resp.npc_id ) ) {
                        if( target->is_player_ally() ) {
                            target->add_llm_intent_memory( player_utterance, memory_speech, memory_actions );
                            broadcast_overheard_memory( *target, memory_speech, memory_actions );
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
                        std::string log_block = string_format( "response %s (%s)\n%s\n\n",
                                                               resp.npc_name, resp.request_id, payload );
                        if( !say_log_line.empty() ) {
                            log_block += say_log_line;
                        } else if( !say_failed_log_line.empty() ) {
                            log_block += say_failed_log_line;
                        }
                        append_llm_intent_log( log_block );
                    }
                } else {
                    const std::string err = resp.ok ? parse_error : resp.error;
                    if( debug_ui ) {
                        add_msg( "LLM intent failed for %s: %s", resp.npc_name, err );
                    }
                    if( debug_log ) {
                        const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                        std::string log_block = string_format( "failed %s (%s)\n%s\nraw:\n%s\n\n",
                                                               resp.npc_name, resp.request_id, err, payload );
                        if( !say_log_line.empty() ) {
                            log_block += say_log_line;
                        } else if( !say_failed_log_line.empty() ) {
                            log_block += say_failed_log_line;
                        }
                        append_llm_intent_log( log_block );
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
                if( wants_look_inventory ) {
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
                            enqueue_look_inventory_request( *target, player_utterance );
                        }
                    }
                }
                bool dispatch_next_serial = false;
                bool is_primary_response = false;
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    utterance_by_request.erase( resp.request_id );
                    snapshot_origin_by_request.erase( resp.request_id );
                    is_primary_response = primary_request_ids.erase( resp.request_id ) > 0;
                    if( is_primary_response ) {
                        pending_primary_npcs.erase( resp.npc_id );
                    }
                    dispatch_next_serial = serial_primary_request_ids.erase( resp.request_id ) > 0;
                }
                if( dispatch_next_serial ) {
                    dispatch_next_serial_primary_request();
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
        std::unordered_map<std::string, tripoint_abs_ms> snapshot_origin_by_request;
        std::unordered_set<std::string> primary_request_ids;
        std::unordered_set<std::string> ambient_request_ids;
        std::deque<pending_primary_request> pending_primary_requests;
        std::set<character_id> pending_primary_npcs;
        std::set<character_id> pending_ambient_npcs;
        std::unordered_set<std::string> serial_primary_request_ids;
        std::unordered_map<std::string, look_around_context> look_around_requests;
        std::unordered_map<std::string, look_inventory_context> look_inventory_requests;
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

void enqueue_ambient_request( npc &listener, const std::string &player_utterance )
{
    get_manager().enqueue_ambient_request( listener, player_utterance );
}

void enqueue_requests( const std::vector<npc *> &listeners,
                       const std::string &player_utterance )
{
    get_manager().enqueue_requests_serial( listeners, player_utterance );
}

void prewarm()
{
    get_manager().prewarm();
}

void process_responses()
{
    get_manager().process_responses();
}

void enqueue_random_requests()
{
    if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
        return;
    }
    const int base_turns = get_option<int>( "LLM_INTENT_RANDOM_CALL" );
    if( g == nullptr ) {
        return;
    }
    if( base_turns <= 0 ) {
        for( npc *listener : g->allies() ) {
            if( listener == nullptr || !listener->is_player_ally() ) {
                continue;
            }
            listener->schedule_next_llm_random_call( 0 );
        }
        return;
    }
    for( npc *listener : g->allies() ) {
        if( listener == nullptr || !listener->is_player_ally() ) {
            continue;
        }
        if( !listener->llm_random_call_due( base_turns ) ) {
            continue;
        }
        if( get_manager().has_pending_primary_request_for( listener->getID() ) ) {
            continue;
        }
        get_manager().enqueue_request( *listener, "" );
        listener->schedule_next_llm_random_call( base_turns );
    }
}

void log_event( const std::string &message )
{
    append_llm_intent_log( message + "\n" );
}

std::string build_snapshot_for_test( npc &listener, const std::string &player_utterance,
                                     const std::string &request_id )
{
    return build_snapshot_json( listener, player_utterance, request_id );
}

std::string build_action_prompt_for_test( const std::string &npc_name,
        const std::string &player_utterance,
        const std::string &snapshot )
{
    return build_prompt( npc_name, player_utterance, snapshot );
}
} // namespace llm_intent

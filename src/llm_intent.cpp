#include "llm_intent.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "calendar.h"
#include "character_id.h"
#include "debug.h"
#include "effect.h"
#include "filesystem.h"
#include "field.h"
#include "game.h"
#include "json.h"
#include "line.h"
#include "map.h"
#include "messages.h"
#include "monster.h"
#include "npc_opinion.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "path_info.h"
#include "string_formatter.h"
#include "units.h"
#include "visitable.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
std::mutex llm_intent_log_mutex;
constexpr const char *llm_intent_log_path = "config/llm_intent_last.log";

void overwrite_llm_intent_log( const std::string &payload )
{
    std::lock_guard<std::mutex> lock( llm_intent_log_mutex );
    std::ofstream out( llm_intent_log_path, std::ios::binary | std::ios::trunc );
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
};

struct llm_intent_response {
    std::string request_id;
    character_id npc_id;
    std::string npc_name;
    bool ok = false;
    std::string text;
    std::string error;
};

struct runner_config {
    std::string python_path;
    std::string runner_path;
    std::string model_dir;
    std::string device;
    int max_tokens = 0;
    int max_prompt_len = 0;
    bool force_npu = false;

    bool operator==( const runner_config &other ) const {
        return python_path == other.python_path &&
               runner_path == other.runner_path &&
               model_dir == other.model_dir &&
               device == other.device &&
               max_tokens == other.max_tokens &&
               max_prompt_len == other.max_prompt_len &&
               force_npu == other.force_npu;
    }

    bool operator!=( const runner_config &other ) const {
        return !( *this == other );
    }
};

void write_tripoint( JsonOut &jsout, const tripoint_bub_ms &pos )
{
    jsout.start_object();
    jsout.member( "x", pos.x() );
    jsout.member( "y", pos.y() );
    jsout.member( "z", pos.z() );
    jsout.end_object();
}

std::string creature_kind( const Creature &critter )
{
    if( critter.is_avatar() ) {
        return "player";
    }
    if( critter.is_npc() ) {
        return "npc";
    }
    if( critter.is_monster() ) {
        return "monster";
    }
    return "creature";
}

std::string sanitize_text( std::string_view text )
{
    return remove_color_tags( text );
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

bool parse_csv_payload( const std::string &csv, std::string &speech,
                        std::vector<std::string> &actions, std::string &error )
{
    actions.clear();
    speech.clear();
    size_t i = 0;
    while( i < csv.size() && std::isspace( static_cast<unsigned char>( csv[i] ) ) ) {
        ++i;
    }
    if( i >= csv.size() || csv[i] != '"' ) {
        error = "CSV speech field must be quoted.";
        return false;
    }
    ++i;
    while( i < csv.size() ) {
        char c = csv[i];
        if( c == '"' ) {
            if( i + 1 < csv.size() && csv[i + 1] == '"' ) {
                speech.push_back( '"' );
                i += 2;
                continue;
            }
            ++i;
            break;
        }
        speech.push_back( c );
        ++i;
    }
    if( i > csv.size() ) {
        error = "CSV speech field missing closing quote.";
        return false;
    }
    while( i < csv.size() && std::isspace( static_cast<unsigned char>( csv[i] ) ) ) {
        ++i;
    }
    if( i >= csv.size() || csv[i] != ',' ) {
        error = "CSV must include at least one action field.";
        return false;
    }
    ++i;
    while( i < csv.size() ) {
        size_t start = i;
        while( i < csv.size() && csv[i] != ',' ) {
            ++i;
        }
        std::string token = trim_copy( csv.substr( start, i - start ) );
        if( !is_action_token( token ) ) {
            error = "CSV action token is invalid.";
            return false;
        }
        actions.push_back( token );
        if( actions.size() > 3 ) {
            error = "CSV has too many action fields.";
            return false;
        }
        if( i >= csv.size() ) {
            break;
        }
        ++i;
    }
    if( actions.empty() ) {
        error = "CSV must include at least one action field.";
        return false;
    }
    return true;
}

std::optional<std::string> extract_csv_from_json( const std::string &text,
        std::string &error )
{
    try {
        std::istringstream in( text );
        TextJsonIn jsin( in );
        TextJsonObject obj = jsin.get_object();
        obj.allow_omitted_members();
        if( !obj.has_member( "csv" ) ) {
            error = "Missing csv field.";
            return std::nullopt;
        }
        return obj.get_string( "csv" );
    } catch( const std::exception & ) {
        error = "LLM output is not valid JSON.";
        return std::nullopt;
    }
}

llm_intent_action action_from_token( const std::string &token )
{
    if( token == "guard_area" ) {
        return llm_intent_action::guard_area;
    }
    if( token == "follow_player" ) {
        return llm_intent_action::follow_player;
    }
    if( token == "clear_overrides" ) {
        return llm_intent_action::clear_overrides;
    }
    if( token == "idle" ) {
        return llm_intent_action::idle;
    }
    return llm_intent_action::none;
}

const std::vector<std::string> &allowed_actions()
{
    static const std::vector<std::string> actions = {
        "guard_area",
        "follow_player",
        "clear_overrides",
        "idle"
    };
    return actions;
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
    std::sort( out.begin(), out.end(), []( const creature_snapshot &lhs,
    const creature_snapshot &rhs ) {
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

void write_creature_list( JsonOut &jsout, npc &listener,
                          const std::vector<creature_snapshot> &list, size_t max_count )
{
    jsout.start_array();
    size_t count = 0;
    for( const creature_snapshot &entry : list ) {
        if( entry.critter == nullptr ) {
            continue;
        }
        jsout.start_object();
        jsout.member( "name", sanitize_text( entry.critter->disp_name() ) );
        jsout.member( "kind", creature_kind( *entry.critter ) );
        jsout.member( "distance", entry.distance );
        jsout.member( "hp_percent", entry.critter->hp_percentage() );
        jsout.member( "threat_score",
                      threat_score_for( listener, *entry.critter, entry.distance ) );
        jsout.end_object();
        if( ++count >= max_count ) {
            break;
        }
    }
    jsout.end_array();
}

std::string build_snapshot_json( npc &listener, const std::string &player_utterance,
                                 const std::string &request_id )
{
    static constexpr int map_radius = 1;
    static constexpr int visible_range = 12;
    static constexpr size_t max_creatures = 5;
    static constexpr size_t max_effects = 6;
    static constexpr size_t max_items = 5;

    std::ostringstream out;
    JsonOut jsout( out );
    const tripoint_bub_ms pos = listener.pos_bub();

    jsout.start_object();
    jsout.member( "request_id", request_id );
    jsout.member( "turn", to_turns<int>( calendar::turn - calendar::turn_zero ) );
    jsout.member( "player_utterance", sanitize_text( player_utterance ) );
    jsout.member( "npc_id", listener.get_unique_id() );
    jsout.member( "npc_name", sanitize_text( listener.get_name() ) );
    jsout.member( "npc_pos" );
    write_tripoint( jsout, pos );

    jsout.member( "npc_state" );
    jsout.start_object();
    jsout.member( "morale", listener.get_morale_level() );
    jsout.member( "hunger", listener.get_hunger() );
    jsout.member( "thirst", listener.get_thirst() );
    jsout.member( "pain", listener.get_pain() );
    jsout.member( "stamina", listener.get_stamina() );
    jsout.member( "stamina_max", listener.get_stamina_max() );
    jsout.member( "sleepiness", listener.get_sleepiness() );
    jsout.member( "hp_percent", listener.hp_percentage() );
    jsout.member( "effects" );
    jsout.start_array();
    size_t effect_count = 0;
    for( const std::reference_wrapper<const effect> &eff_ref : listener.get_effects() ) {
        const effect &eff = eff_ref.get();
        jsout.start_object();
        jsout.member( "id", eff.get_id().str() );
        jsout.member( "intensity", eff.get_intensity() );
        jsout.end_object();
        if( ++effect_count >= max_effects ) {
            break;
        }
    }
    jsout.end_array();
    jsout.end_object();

    jsout.member( "npc_emotions" );
    jsout.start_object();
    jsout.member( "danger_assessment", listener.danger_assessment() );
    jsout.member( "panic", listener.mem_combat.panic );
    jsout.member( "confidence", listener.mem_combat.my_health );
    jsout.member( "emergency", listener.emergency() );
    jsout.end_object();

    jsout.member( "npc_personality" );
    jsout.start_object();
    jsout.member( "aggression", listener.personality.aggression );
    jsout.member( "bravery", listener.personality.bravery );
    jsout.member( "collector", listener.personality.collector );
    jsout.member( "altruism", listener.personality.altruism );
    jsout.end_object();

    jsout.member( "npc_opinion" );
    jsout.start_object();
    jsout.member( "trust", listener.op_of_u.trust );
    jsout.member( "fear", listener.op_of_u.fear );
    jsout.member( "value", listener.op_of_u.value );
    jsout.member( "anger", listener.op_of_u.anger );
    jsout.end_object();

    jsout.member( "threats" );
    write_creature_list( jsout, listener,
                         filter_visible( listener, Creature::Attitude::HOSTILE,
                         visible_range ), max_creatures );

    jsout.member( "friendlies" );
    write_creature_list( jsout, listener,
                         filter_visible( listener, Creature::Attitude::FRIENDLY,
                         visible_range ), max_creatures );

    jsout.member( "overmap_grid" );
    jsout.start_object();
    jsout.member( "radius", map_radius );
    jsout.member( "grid" );
    jsout.start_array();
    const tripoint_abs_omt omt_center = listener.pos_abs_omt();
    for( int dy = -map_radius; dy <= map_radius; ++dy ) {
        jsout.start_array();
        for( int dx = -map_radius; dx <= map_radius; ++dx ) {
            const tripoint_abs_omt omt_cell( omt_center.x() + dx, omt_center.y() + dy, omt_center.z() );
            const oter_id &oter = overmap_buffer.ter( omt_cell );
            const bool has_horde = overmap_buffer.has_horde( omt_cell );
            std::ostringstream cell_out;
            cell_out << oter->get_type_id().str() << "|" << ( has_horde ? "H" : "" );
            jsout.write( cell_out.str() );
        }
        jsout.end_array();
    }
    jsout.end_array();
    jsout.end_object();

    jsout.member( "ruleset" );
    jsout.start_object();
    jsout.member( "attitude", npc_attitude_id( listener.get_attitude() ) );
    jsout.member( "mission", sanitize_text( listener.describe_mission() ) );
    jsout.member( "rules" );
    jsout.start_array();
    for( const auto &entry : ally_rule_strs ) {
        if( listener.rules.has_flag( entry.second.rule ) ) {
            jsout.write( entry.first );
        }
    }
    jsout.end_array();
    jsout.end_object();

    jsout.member( "inventory_summary" );
    jsout.start_object();
    item_location wielded = listener.get_wielded_item();
    jsout.member( "wielded" );
    jsout.start_object();
    if( wielded ) {
        const item &weapon = *wielded;
        jsout.member( "name", sanitize_text( weapon.tname() ) );
        jsout.member( "is_gun", weapon.is_gun() );
        jsout.member( "ammo_remaining", weapon.ammo_remaining() );
        jsout.member( "remaining_ammo_capacity", weapon.remaining_ammo_capacity() );
        jsout.member( "reload_needed", weapon.ammo_required() > 0 &&
                      weapon.ammo_remaining() < weapon.ammo_required() );
    }
    jsout.end_object();

    const units::mass weight = listener.weight_carried();
    const units::mass weight_cap = listener.weight_capacity();
    const units::volume volume = listener.volume_carried();
    const units::volume volume_cap = listener.volume_capacity();
    const int weight_pct = weight_cap > 0_gram
                           ? static_cast<int>( 100.0 * units::to_gram<double>( weight ) /
                                               units::to_gram<double>( weight_cap ) )
                           : 0;
    const int volume_pct = volume_cap > 0_ml
                           ? static_cast<int>( 100.0 * units::to_milliliter<double>( volume ) /
                                               units::to_milliliter<double>( volume_cap ) )
                           : 0;
    jsout.member( "weight_percent", weight_pct );
    jsout.member( "volume_percent", volume_pct );

    std::vector<std::string> usable_items;
    std::vector<std::string> combat_items;
    std::vector<std::string> healing_items;
    listener.visit_items( [&]( item * it, item * ) {
        if( it == nullptr ) {
            return VisitResponse::NEXT;
        }
        if( usable_items.size() < max_items &&
            ( it->is_tool() || it->is_medication() || it->is_medical_tool() ) ) {
            usable_items.push_back( sanitize_text( it->tname() ) );
        }
        if( combat_items.size() < max_items && ( it->is_gun() || it->is_melee() ) ) {
            combat_items.push_back( sanitize_text( it->tname() ) );
        }
        if( healing_items.size() < max_items && ( it->is_medication() || it->is_medical_tool() ) ) {
            healing_items.push_back( sanitize_text( it->tname() ) );
        }
        if( usable_items.size() >= max_items &&
            combat_items.size() >= max_items &&
            healing_items.size() >= max_items ) {
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );

    jsout.member( "usable_items" );
    jsout.start_array();
    for( const std::string &name : usable_items ) {
        jsout.write( name );
    }
    jsout.end_array();

    jsout.member( "combat_items" );
    jsout.start_array();
    for( const std::string &name : combat_items ) {
        jsout.write( name );
    }
    jsout.end_array();

    jsout.member( "healing_items" );
    jsout.start_array();
    for( const std::string &name : healing_items ) {
        jsout.write( name );
    }
    jsout.end_array();
    jsout.end_object();

    jsout.end_object();
    return out.str();
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
    return string_format(
               "You are a game NPC response engine. Return ONLY strict JSON.\n"
               "Return JSON with a single key \"csv\".\n"
               "The \"csv\" value is one CSV line with fields:\n"
               "1) speech text (always quoted)\n"
               "2) action1\n"
               "3) action2 (optional)\n"
               "4) action3 (optional)\n"
               "Actions are single tokens (no whitespace or commas).\n"
               "Allowed actions: %s\n"
               "Context JSON:\n%s\n"
               "Player said: \"%s\"\n"
               "NPC: \"%s\"\n",
               action_list, snapshot, player_utterance, npc_name );
}

std::string request_to_json( const llm_intent_request &request )
{
    std::ostringstream out;
    JsonOut jsout( out );
    jsout.start_object();
    jsout.member( "request_id", request.request_id );
    jsout.member( "prompt", request.prompt );
    jsout.member( "snapshot", request.snapshot );
    jsout.member( "max_tokens", request.max_tokens );
    jsout.end_object();
    return out.str();
}

std::string read_log_tail( const std::filesystem::path &path, std::streamoff max_bytes )
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

std::optional<llm_intent_response> response_from_json( const std::string &line,
        const llm_intent_request &request )
{
    if( !should_attempt_parse( line ) ) {
        return std::nullopt;
    }
    llm_intent_response response;
    response.request_id = request.request_id;
    response.npc_id = request.npc_id;
    response.npc_name = request.npc_name;
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

std::filesystem::path resolve_path( const std::string &path )
{
    if( path.empty() ) {
        return std::filesystem::path();
    }
    std::filesystem::path p( path );
    if( p.is_relative() ) {
        p = std::filesystem::path( PATH_INFO::base_path() ) / p;
    }
    return p;
}

runner_config current_runner_config()
{
    static constexpr int default_max_tokens = 20000;
    static constexpr int default_max_prompt_len = 4096;
    runner_config cfg;
    cfg.python_path = get_option<std::string>( "LLM_INTENT_PYTHON" );
    cfg.runner_path = get_option<std::string>( "LLM_INTENT_RUNNER" );
    cfg.model_dir = get_option<std::string>( "LLM_INTENT_MODEL_DIR" );
    cfg.device = get_option<std::string>( "LLM_INTENT_DEVICE" );
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

        bool send_request( const llm_intent_request &request, std::string &response_line, std::string &error,
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
            if( config.python_path.empty() || config.runner_path.empty() || config.model_dir.empty() ) {
                error = "LLM runner configuration is incomplete.";
                return false;
            }

            std::filesystem::path python_path = resolve_path( config.python_path );
            std::filesystem::path runner_path = resolve_path( config.runner_path );
            std::filesystem::path model_dir = resolve_path( config.model_dir );
            std::filesystem::path cache_dir = model_dir / ".ov_cache";
            std::filesystem::path log_path = PATH_INFO::config_dir_path().get_unrelative_path() /
                                             "llm_intent_runner.log";
            assure_dir_exist( PATH_INFO::config_dir() );

            std::vector<std::string> args;
            args.push_back( python_path.string() );
            args.push_back( runner_path.string() );
            args.push_back( "--model-dir" );
            args.push_back( model_dir.string() );
            args.push_back( "--device" );
            args.push_back( config.device.empty() ? "NPU" : config.device );
            args.push_back( "--max-tokens" );
            args.push_back( std::to_string( config.max_tokens ) );
            args.push_back( "--max-prompt-len" );
            args.push_back( std::to_string( config.max_prompt_len ) );
            args.push_back( "--cache-dir" );
            args.push_back( cache_dir.string() );
            args.push_back( "--log-file" );
            args.push_back( log_path.string() );
            if( config.force_npu ) {
                args.push_back( "--force-npu" );
            }

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
#endif

class llm_intent_manager
{
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
            if( get_option<bool>( "DEBUG_LLM_INTENT" ) ) {
                overwrite_llm_intent_log( string_format( "snapshot %s (%s)\n%s\n",
                                         req.npc_name, req.request_id, req.snapshot ) );
            }
            {
                std::lock_guard<std::mutex> lock( mutex );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
        }

        void prewarm() {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            ensure_worker();
#if defined(_WIN32)
            const runner_config config = current_runner_config();
            std::string error;
            if( config.force_npu && config.device != "NPU" ) {
                if( get_option<bool>( "DEBUG_LLM_INTENT" ) ) {
                    add_msg( "LLM intent prewarm skipped: LLM_INTENT_FORCE_NPU requires device NPU." );
                }
                return;
            }
            if( !runner.ensure_running( config, error ) ) {
                if( get_option<bool>( "DEBUG_LLM_INTENT" ) ) {
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
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    request_queue.push( std::move( warm ) );
                }
                cv.notify_one();
            }
#endif
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

            const bool debug_log = get_option<bool>( "DEBUG_LLM_INTENT" );
            while( !local.empty() ) {
                const llm_intent_response &resp = local.front();
                if( resp.request_id == "prewarm" ) {
                    local.pop();
                    continue;
                }
                std::string parse_error;
                std::string speech;
                std::vector<std::string> actions;
                llm_intent_action action = llm_intent_action::none;
                if( resp.ok ) {
                    const std::optional<std::string> csv_json = extract_csv_from_json( resp.text,
                            parse_error );
                    if( csv_json &&
                        parse_csv_payload( *csv_json, speech, actions, parse_error ) ) {
                        for( const std::string &token : actions ) {
                            action = action_from_token( token );
                            if( action != llm_intent_action::none ) {
                                break;
                            }
                        }
                        if( action == llm_intent_action::none ) {
                            parse_error = "No allowed action token found.";
                        }
                    }
                }

                npc *target = nullptr;
                if( resp.ok && parse_error.empty() ) {
                    target = g->find_npc( resp.npc_id );
                    if( target == nullptr ) {
                        parse_error = "NPC not found for LLM intent response.";
                    }
                }

                if( resp.ok && parse_error.empty() && target != nullptr ) {
                    static constexpr int default_ttl_turns = 20;
                    target->set_llm_intent_override( action, default_ttl_turns,
                                                     resp.request_id, speech );
                }

                if( debug_log ) {
                    if( resp.ok && parse_error.empty() ) {
                        add_msg( "LLM intent response for %s: %s", resp.npc_name, resp.text );
                        overwrite_llm_intent_log( string_format( "response %s (%s)\n%s\n",
                                                 resp.npc_name, resp.request_id, resp.text ) );
                    } else {
                        const std::string err = resp.ok ? parse_error : resp.error;
                        add_msg( "LLM intent failed for %s: %s", resp.npc_name, err );
                        overwrite_llm_intent_log( string_format( "failed %s (%s)\n%s\n",
                                                 resp.npc_name, resp.request_id, err ) );
                    }
                }
                local.pop();
            }
        }

    private:
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<llm_intent_request> request_queue;
        std::queue<llm_intent_response> response_queue;
        std::thread worker;
        std::atomic<bool> stopping = false;
        std::atomic<int> counter = 0;
        std::atomic<bool> warmup_enqueued = false;
#if defined(_WIN32)
        llm_intent_runner_process runner;
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
                llm_intent_response response;
#if defined(_WIN32)
                response = handle_request_windows( req );
#else
                response.request_id = req.request_id;
                response.npc_id = req.npc_id;
                response.npc_name = req.npc_name;
                response.ok = false;
                response.error = "LLM runner is only supported on Windows in this build.";
#endif
                {
                    std::lock_guard<std::mutex> lock( mutex );
                    response_queue.push( std::move( response ) );
                }
            }
        }

#if defined(_WIN32)
        llm_intent_response handle_request_windows( const llm_intent_request &req ) {
            llm_intent_response response;
            response.request_id = req.request_id;
            response.npc_id = req.npc_id;
            response.npc_name = req.npc_name;
            const runner_config config = current_runner_config();
            std::string error;
            if( config.force_npu && config.device != "NPU" ) {
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
#endif
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

void prewarm()
{
    get_manager().prewarm();
}

void process_responses()
{
    get_manager().process_responses();
}
} // namespace llm_intent

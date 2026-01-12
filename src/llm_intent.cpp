#include "llm_intent.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "character_id.h"
#include "creature_tracker.h"
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
constexpr const char *llm_intent_log_path = "config/llm_intent.log";

void append_llm_intent_log( const std::string &payload )
{
    std::lock_guard<std::mutex> lock( llm_intent_log_mutex );
    std::ofstream out( llm_intent_log_path, std::ios::binary | std::ios::app );
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
    std::string raw;
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
        "guard_area",
        "follow_player",
        "use_gun",
        "use_melee",
        "use_bow",
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
    if( token == "guard_area" ) {
        return llm_intent_action::guard_area;
    }
    if( token == "follow_player" ) {
        return llm_intent_action::follow_player;
    }
    if( token == "use_gun" ) {
        return llm_intent_action::use_gun;
    }
    if( token == "use_melee" ) {
        return llm_intent_action::use_melee;
    }
    if( token == "use_bow" ) {
        return llm_intent_action::use_bow;
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
        if( token.empty() ) {
            error = "CSV action token is invalid.";
            return false;
        }
        if( token.size() >= 2 && token.front() == '"' && token.back() == '"' ) {
            token = trim_copy( token.substr( 1, token.size() - 2 ) );
        }
        std::string token_lower = token;
        std::transform( token_lower.begin(), token_lower.end(), token_lower.begin(), []( unsigned char c ) {
            return static_cast<char>( std::tolower( c ) );
        } );
        if( token_lower.rfind( "attack=", 0 ) == 0 ) {
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
        if( !is_action_token( token_lower ) ) {
            error = "CSV action token is invalid.";
            return false;
        }
        if( !is_allowed_action( token_lower ) ) {
            if( !attack_target.empty() ) {
                return true;
            }
        }
        actions.push_back( token_lower );
        if( actions.size() > 3 ) {
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
                            const char letter = cost > 100 || cost <= 0 ? static_cast<char>( std::toupper( base_letter ) ) : base_letter;
                            letter_map.emplace( critter, letter );
                            legend_entries.emplace_back( letter, "player" );
                            glyph = letter;
                        } else {
                            glyph = found->second;
                        }
                    } else if( critter != &listener ) {
                        auto found = letter_map.find( critter );
                        if( found == letter_map.end() ) {
                            if( next_letter <= 'z' ) {
                                const char base_letter = next_letter;
                                const char letter = cost > 100 || cost <= 0 ? static_cast<char>( std::toupper( base_letter ) ) : base_letter;
                                letter_map.emplace( critter, letter );
                                legend_entries.emplace_back( letter, strip_leading_article( sanitize_text( critter->disp_name() ) ) );
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
    return out;
}

std::string build_snapshot_json( npc &listener, const std::string &player_utterance,
                                 const std::string &request_id )
{
    static constexpr int visible_range = 12;
    static constexpr size_t max_creatures = 5;
    static constexpr size_t max_effects = 6;
    static constexpr size_t max_items = 3;

    std::ostringstream out;
    out << "SITUATION\n";
    out << "id: " << request_id << "\n";
    out << "player_name: " << sanitize_text( get_player_character().get_name() ) << "\n";
    out << "player_utterance: " << sanitize_text( player_utterance ) << "\n\n";
    out << "your_name: " << sanitize_text( listener.get_name() ) << "\n";

    out << "your_state: ";
    out << "morale=" << listener.get_morale_level();
    out << " hunger=" << listener.get_hunger();
    out << " thirst=" << listener.get_thirst();
    out << " pain=" << listener.get_pain();
    out << " stamina=" << listener.get_stamina() << "/" << listener.get_stamina_max();
    out << " sleepiness=" << listener.get_sleepiness();
    out << " hp_percent=" << listener.hp_percentage();
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

    out << "your_emotions: ";
    out << "danger_assessment=" << listener.danger_assessment();
    out << " panic=" << listener.mem_combat.panic;
    out << " confidence=" << listener.mem_combat.my_health;
    out << " emergency=" << ( listener.emergency() ? "true" : "false" ) << "\n";

    out << "your_personality: ";
    out << "aggression=" << static_cast<int>( listener.personality.aggression );
    out << " bravery=" << static_cast<int>( listener.personality.bravery );
    out << " collector=" << static_cast<int>( listener.personality.collector );
    out << " altruism=" << static_cast<int>( listener.personality.altruism ) << "\n";

    out << "your_opinion_of_player: ";
    out << "trust=" << listener.op_of_u.trust;
    out << " intimidation=" << listener.op_of_u.fear;
    out << " respect=" << listener.op_of_u.value;
    out << " anger=" << listener.op_of_u.anger << "\n\n";

    const std::vector<creature_snapshot> hostile = filter_visible( listener, Creature::Attitude::HOSTILE,
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
            out << name << "@" << entry.distance << " ts=";
            out << threat_score_for( listener, *entry.critter, entry.distance );
            if( ++count >= max_creatures ) {
                break;
            }
        }
        out << "\n";
    }

    const std::vector<creature_snapshot> friendly = filter_visible( listener, Creature::Attitude::FRIENDLY,
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
            out << name << "@" << entry.distance;
            if( ++count >= max_creatures ) {
                break;
            }
        }
        out << "\n";
    }

    out << "\n";
    out << "ruleset: attitude=" << npc_attitude_id( listener.get_attitude() ) << " rules=[";
    bool first_rule = true;
    for( const auto &entry : ally_rule_strs ) {
        if( listener.rules.has_flag( entry.second.rule ) ) {
            if( !first_rule ) {
                out << " ";
            }
            first_rule = false;
            out << entry.first;
        }
    }
    out << "]\n\n";

    item_location wielded = listener.get_wielded_item();
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

    out << "inventory: ";
    if( wielded ) {
        const item &weapon = *wielded;
        out << "wielded=\"" << sanitize_text( weapon.tname() ) << "\"";
    } else {
        out << "wielded=none";
    }
    out << " weight_percent=" << weight_pct << " volume_percent=" << volume_pct << "\n";

    std::vector<std::string> usable_items;
    std::vector<std::string> combat_guns;
    std::vector<std::string> combat_melee;
    bool bandage_possible = false;
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
                combat_guns.push_back( sanitize_text( it->tname() ) );
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
               "You are a game NPC response engine, supposed to respond to player_utterance. "
               "Return ONLY a single CSV line and nothing else."
               "This CSV line has one to four fields separated by '|':\n"
               "<Field 1>"
               "The first field is an answer to player_utterance."
               "You have decided to team up with the player for now, and must answer as the NPC."
               "Stick to your role, with your emotions and opinions."
               "Use a dark tone, with swear words, fit for a zombie apocalypse."
               "Never repeat the players words."
               "</Field 1>\n"
               "<Fields 2-4>"
               "Write 1-3 of the following allowed actions:"
               "%s\n"
               "<Allowed actions>"
               "guard_area to stay put, keep watch, wait, stand.\n"
               "follow_player to walk behind, follow, run.\n"
               "use_gun to use gun, rifle, thrower.\n"
               "use_melee to bash, cut, kick, in close combat.\n"
               "use_bow to use bow, crossbow, stealth.\n"
               "attack=<target> to target a creature from your map.\n"
               "idle if none of the above.\n"
               "</Allowed actions>"
               "</Fields 2-4>\n"
               "Print nothing else other than Fields 1-4, separated by |"
               "If you break that format, you have failed."
               "Output must be a single line with no markdown or extra text."
               "Absolutely no notes, explanations, examples, or parenthetical text."
               "<Example Output>"
               "Blow me.|idle"
               "</Example Output>\n"
               "<Example Output>"
               "Lets put those fucks in the ground.|use_melee|attack=Zombie"
               "</Example Output>\n"
               "<Example Output>\n"
               "Providing cover!|guard_area|use_gun"
               "</Example Output>\n"
               "</System>\n",
               snapshot, action_list_with_target );
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
                append_llm_intent_log( string_format( "snapshot %s (%s)\n%s\n\n",
                                      req.npc_name, req.request_id, req.snapshot ) );
                append_llm_intent_log( string_format( "prompt %s (%s)\n%s\n\n",
                                      req.npc_name, req.request_id, req.prompt ) );
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
                std::string action_error;
                std::string speech;
                std::vector<std::string> actions;
                std::string attack_target;
                if( resp.ok ) {
                    const std::string csv_text = extract_csv_from_text( resp.text );
                    bool parsed = false;
                    std::string normalized = normalize_csv_separators( csv_text );
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

                if( resp.ok && parse_error.empty() && !speech.empty() ) {
                    if( npc *target = g->find_npc( resp.npc_id ) ) {
                        target->say( speech );
                    }
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
                if( debug_log ) {
                    if( resp.ok && parse_error.empty() ) {
                        add_msg( "LLM intent response for %s: %s", resp.npc_name, resp.text );
                        if( !action_error.empty() ) {
                            add_msg( "LLM intent warning for %s: %s", resp.npc_name, action_error );
                        }
                        const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                        append_llm_intent_log( string_format( "response %s (%s)\n%s\n\n",
                                      resp.npc_name, resp.request_id, payload ) );
                    } else {
                        const std::string err = resp.ok ? parse_error : resp.error;
                        add_msg( "LLM intent failed for %s: %s", resp.npc_name, err );
                        const std::string &payload = resp.raw.empty() ? resp.text : resp.raw;
                        append_llm_intent_log( string_format( "failed %s (%s)\n%s\nraw:\n%s\n\n",
                                      resp.npc_name, resp.request_id, err, payload ) );
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

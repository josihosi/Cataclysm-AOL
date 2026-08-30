#include "bandit_live_world_probe.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "json.h"
#include "avatar.h"
#include "calendar.h"
#include "game.h"
#include "map.h"
#include "monster.h"
#include "mtype.h"

namespace
{
thread_local bandit_live_world_probe::session *active_session = nullptr;
thread_local std::size_t loaded_covert_member_depth = 0;

struct live_transition_stream {
    std::string path;
    std::string run_id;
    std::uint64_t last_sequence = 0;
    bool initialized = false;
};

live_transition_stream transition_stream;

std::uint64_t current_process_pid()
{
#if defined(_WIN32)
    return static_cast<std::uint64_t>( _getpid() );
#else
    return static_cast<std::uint64_t>( getpid() );
#endif
}

class sha256
{
    public:
        void update( const char *data, std::size_t size ) {
            for( std::size_t index = 0; index < size; ++index ) {
                block_[used_++] = static_cast<std::uint8_t>( data[index] );
                if( used_ == block_.size() ) {
                    transform();
                    bits_ += 512;
                    used_ = 0;
                }
            }
        }

        std::string finish() {
            const std::uint64_t bits = bits_ + static_cast<std::uint64_t>( used_ ) * 8;
            block_[used_++] = 0x80;
            if( used_ > 56 ) {
                while( used_ < block_.size() ) {
                    block_[used_++] = 0;
                }
                transform();
                used_ = 0;
            }
            while( used_ < 56 ) {
                block_[used_++] = 0;
            }
            for( int shift = 56; shift >= 0; shift -= 8 ) {
                block_[used_++] = static_cast<std::uint8_t>( bits >> shift );
            }
            transform();
            static constexpr char hex[] = "0123456789abcdef";
            std::string result;
            result.reserve( 64 );
            for( const std::uint32_t value : state_ ) {
                for( int shift = 28; shift >= 0; shift -= 4 ) {
                    result.push_back( hex[( value >> shift ) & 0xf] );
                }
            }
            return result;
        }

    private:
        static std::uint32_t rotate_right( const std::uint32_t value, const int shift ) {
            return value >> shift | value << ( 32 - shift );
        }

        void transform() {
            static constexpr std::array<std::uint32_t, 64> round_constants = {{
                    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
                }
            };
            std::array<std::uint32_t, 64> words = {};
            for( std::size_t index = 0; index < 16; ++index ) {
                words[index] = static_cast<std::uint32_t>( block_[index * 4] ) << 24 |
                               static_cast<std::uint32_t>( block_[index * 4 + 1] ) << 16 |
                               static_cast<std::uint32_t>( block_[index * 4 + 2] ) << 8 |
                               static_cast<std::uint32_t>( block_[index * 4 + 3] );
            }
            for( std::size_t index = 16; index < words.size(); ++index ) {
                const std::uint32_t first = rotate_right( words[index - 15], 7 ) ^
                                            rotate_right( words[index - 15], 18 ) ^ words[index - 15] >> 3;
                const std::uint32_t second = rotate_right( words[index - 2], 17 ) ^
                                             rotate_right( words[index - 2], 19 ) ^ words[index - 2] >> 10;
                words[index] = words[index - 16] + first + words[index - 7] + second;
            }
            std::uint32_t a = state_[0];
            std::uint32_t b = state_[1];
            std::uint32_t c = state_[2];
            std::uint32_t d = state_[3];
            std::uint32_t e = state_[4];
            std::uint32_t f = state_[5];
            std::uint32_t g = state_[6];
            std::uint32_t h = state_[7];
            for( std::size_t index = 0; index < words.size(); ++index ) {
                const std::uint32_t first = rotate_right( e, 6 ) ^ rotate_right( e, 11 ) ^ rotate_right( e, 25 );
                const std::uint32_t choose = ( e & f ) ^ ( ~e & g );
                const std::uint32_t temp_one = h + first + choose + round_constants[index] + words[index];
                const std::uint32_t second = rotate_right( a, 2 ) ^ rotate_right( a, 13 ) ^ rotate_right( a, 22 );
                const std::uint32_t majority = ( a & b ) ^ ( a & c ) ^ ( b & c );
                h = g;
                g = f;
                f = e;
                e = d + temp_one;
                d = c;
                c = b;
                b = a;
                a = temp_one + second + majority;
            }
            state_[0] += a;
            state_[1] += b;
            state_[2] += c;
            state_[3] += d;
            state_[4] += e;
            state_[5] += f;
            state_[6] += g;
            state_[7] += h;
        }

        std::array<std::uint8_t, 64> block_ = {};
        std::array<std::uint32_t, 8> state_ = {{
                0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
            }
        };
        std::uint64_t bits_ = 0;
        std::size_t used_ = 0;
};

std::string hmac_sha256( const std::string &key, const std::string &message )
{
    std::string normalized_key = key;
    if( normalized_key.size() > 64 ) {
        sha256 digest;
        digest.update( normalized_key.data(), normalized_key.size() );
        normalized_key = digest.finish();
    }
    normalized_key.resize( 64, '\0' );
    std::string inner_pad( 64, '\x36' );
    std::string outer_pad( 64, '\x5c' );
    for( std::size_t i = 0; i < 64; ++i ) {
        inner_pad[i] = static_cast<char>( inner_pad[i] ^ normalized_key[i] );
        outer_pad[i] = static_cast<char>( outer_pad[i] ^ normalized_key[i] );
    }
    sha256 inner;
    inner.update( inner_pad.data(), inner_pad.size() );
    inner.update( message.data(), message.size() );
    const std::string inner_digest = inner.finish();
    std::array<std::uint8_t, 32> inner_bytes = {};
    for( std::size_t i = 0; i < inner_bytes.size(); ++i ) {
        unsigned int value = 0;
        std::from_chars( inner_digest.data() + i * 2, inner_digest.data() + i * 2 + 2, value, 16 );
        inner_bytes[i] = static_cast<std::uint8_t>( value );
    }
    sha256 outer;
    outer.update( outer_pad.data(), outer_pad.size() );
    outer.update( reinterpret_cast<const char *>( inner_bytes.data() ), inner_bytes.size() );
    return outer.finish();
}

std::string certification_receipt_facts( const bandit_live_world_probe::transition_event &event )
{
    return event.certification_round_id + "\n" + event.certification_lease_id + "\n" +
           std::to_string( event.certification_save_sequence ) + "\n" +
           event.certification_previous_world_tree_sha256 + "\n" +
           event.certification_previous_world_save_sha256 + "\n" +
           event.certification_current_world_tree_sha256 + "\n" +
           event.certification_current_world_save_sha256 + "\n" +
           std::to_string( event.certification_process_pid );
}

std::string world_tree_sha256( const std::string &world_path )
{
    std::error_code error;
    const std::filesystem::path root( world_path );
    if( world_path.empty() || !std::filesystem::is_directory( root, error ) || error ) {
        return "";
    }
    std::vector<std::filesystem::path> files;
    for( std::filesystem::recursive_directory_iterator iterator( root, error ), end;
         !error && iterator != end; iterator.increment( error ) ) {
        const std::filesystem::path relative = iterator->path().lexically_relative( root );
        const bool excluded = std::any_of( relative.begin(), relative.end(), []( const auto &part ) {
            const std::string value = part.string();
            return value == "harness_runs" || value == "cache" || value == "__pycache__" || value == ".git";
        } );
        if( excluded ) {
            if( iterator->is_directory( error ) && !error ) {
                iterator.disable_recursion_pending();
            }
            continue;
        }
        if( iterator->is_regular_file( error ) && !error ) {
            files.push_back( iterator->path() );
        }
    }
    if( error || files.empty() ) {
        return "";
    }
    std::sort( files.begin(), files.end(), [&root]( const auto &lhs, const auto &rhs ) {
        return lhs.lexically_relative( root ).generic_string() < rhs.lexically_relative( root ).generic_string();
    } );
    sha256 digest;
    static constexpr char domain[] = "caol-world-save:v1\0";
    digest.update( domain, sizeof( domain ) - 1 );
    for( const std::filesystem::path &path : files ) {
        const std::string relative = path.lexically_relative( root ).generic_string();
        std::ifstream input( path, std::ios::binary );
        if( !input ) {
            return "";
        }
        digest.update( relative.data(), relative.size() );
        digest.update( "\0", 1 );
        std::array<char, 8192> bytes = {};
        while( input.good() ) {
            input.read( bytes.data(), bytes.size() );
            digest.update( bytes.data(), static_cast<std::size_t>( input.gcount() ) );
        }
        if( input.bad() ) {
            return "";
        }
        digest.update( "\0", 1 );
    }
    return digest.finish();
}

template<typename Enum>
constexpr std::size_t enum_index( Enum value )
{
    return static_cast<std::size_t>( value );
}

std::int64_t bucket_upper_bound( const std::size_t index )
{
    constexpr std::size_t subdivisions = 64;
    const std::size_t exponent = index / subdivisions;
    const std::size_t subdivision = index % subdivisions;
    const std::uint64_t base = std::uint64_t( 1 ) << exponent;
    std::uint64_t upper = 0;
    if( exponent >= 6 ) {
        upper = base + ( subdivision + 1 ) * ( base >> 6 ) - 1;
    } else {
        const std::uint64_t delta = ( ( subdivision + 1 ) * base - 1 ) / subdivisions;
        upper = base + delta;
    }
    return static_cast<std::int64_t>( std::min<std::uint64_t>(
            upper, std::numeric_limits<std::int64_t>::max() ) );
}

bool is_safe_run_id( const std::string_view value )
{
    return !value.empty() && std::all_of( value.begin(), value.end(), []( const unsigned char c ) {
        return std::isalnum( c ) || c == '-' || c == '_' || c == '.';
    } );
}

bool parse_unsigned_json_member( const std::string_view line, const std::string_view name,
                                 std::uint64_t &value )
{
    const std::string key = "\"" + std::string( name ) + "\"";
    const std::size_t key_pos = line.find( key );
    if( key_pos == std::string_view::npos ) {
        return false;
    }
    std::size_t value_pos = line.find( ':', key_pos + key.size() );
    if( value_pos == std::string_view::npos ) {
        return false;
    }
    ++value_pos;
    while( value_pos < line.size() && std::isspace( static_cast<unsigned char>( line[value_pos] ) ) ) {
        ++value_pos;
    }
    const char *const first = line.data() + value_pos;
    const char *const last = line.data() + line.size();
    const auto parsed = std::from_chars( first, last, value );
    return parsed.ec == std::errc() && parsed.ptr != first;
}

bool parse_string_json_member( const std::string_view line, const std::string_view name,
                               std::string_view &value )
{
    const std::string key = "\"" + std::string( name ) + "\"";
    const std::size_t key_pos = line.find( key );
    if( key_pos == std::string_view::npos ) {
        return false;
    }
    std::size_t value_pos = line.find( ':', key_pos + key.size() );
    if( value_pos == std::string_view::npos ) {
        return false;
    }
    ++value_pos;
    while( value_pos < line.size() && std::isspace( static_cast<unsigned char>( line[value_pos] ) ) ) {
        ++value_pos;
    }
    if( value_pos >= line.size() || line[value_pos] != '"' ) {
        return false;
    }
    const std::size_t value_end = line.find( '"', value_pos + 1 );
    if( value_end == std::string_view::npos ||
        line.substr( value_pos + 1, value_end - value_pos - 1 ).find( '\\' ) != std::string_view::npos ) {
        return false;
    }
    value = line.substr( value_pos + 1, value_end - value_pos - 1 );
    return true;
}

bool initialize_transition_stream( const std::string &path, const std::string &run_id )
{
    transition_stream = {};
    if( !is_safe_run_id( run_id ) ) {
        return false;
    }

    std::error_code error;
    const bool stream_exists = std::filesystem::exists( path, error );
    if( error ) {
        return false;
    }
    if( stream_exists ) {
        std::ifstream input( path, std::ios::binary );
        if( !input ) {
            return false;
        }
        const std::string content { std::istreambuf_iterator<char>( input ),
                                    std::istreambuf_iterator<char>() };
        if( !content.empty() && content.back() != '\n' ) {
            return false;
        }
        std::uint64_t last_sequence = 0;
        std::size_t start = 0;
        while( start < content.size() ) {
            const std::size_t end = content.find( '\n', start );
            if( end == std::string::npos || end == start ) {
                return false;
            }
            const std::string_view line( content.data() + start, end - start );
            std::uint64_t schema_version = 0;
            std::uint64_t sequence = 0;
            std::string_view stored_run_id;
            if( !parse_unsigned_json_member( line, "schema_version", schema_version ) ||
                !parse_unsigned_json_member( line, "sequence", sequence ) ||
                !parse_string_json_member( line, "run_id", stored_run_id ) ||
                schema_version != 1 || stored_run_id != run_id ||
                sequence == 0 || sequence != last_sequence + 1 ) {
                return false;
            }
            last_sequence = sequence;
            start = end + 1;
        }
        transition_stream.last_sequence = last_sequence;
    }
    transition_stream.path = path;
    transition_stream.run_id = run_id;
    transition_stream.initialized = true;
    return true;
}

bool live_transition_stream_enabled()
{
    const char *const path_value = std::getenv( "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH" );
    const char *const run_id_value = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    if( path_value == nullptr || path_value[0] == '\0' || run_id_value == nullptr ||
        run_id_value[0] == '\0' ) {
        transition_stream = {};
        return false;
    }
    const std::string path( path_value );
    const std::string run_id( run_id_value );
    return transition_stream.initialized && transition_stream.path == path &&
           transition_stream.run_id == run_id ? true : initialize_transition_stream( path, run_id );
}

bool append_live_transition_event( const bandit_live_world_probe::transition_event &event )
{
    if( !live_transition_stream_enabled() ||
        transition_stream.last_sequence == std::numeric_limits<std::uint64_t>::max() ) {
        return false;
    }
    std::ofstream output( transition_stream.path, std::ios::app );
    if( !output ) {
        return false;
    }
    const std::uint64_t sequence = transition_stream.last_sequence + 1;
    JsonOut json( output );
    json.start_object();
    json.member( "schema_version", 1 );
    json.member( "sequence", sequence );
    json.member( "run_id", transition_stream.run_id );
    if( event.game_minutes >= 0 ) {
        json.member( "game_minutes", event.game_minutes );
    }
    if( !event.domain.empty() ) {
        json.member( "domain", event.domain );
    }
    if( !event.transition.empty() ) {
        json.member( "transition", event.transition );
    }
    if( !event.outcome.empty() ) {
        json.member( "outcome", event.outcome );
    }
    if( !event.site_id.empty() ) {
        json.member( "site_id", event.site_id );
    }
    if( !event.operation_id.empty() ) {
        json.member( "operation_id", event.operation_id );
    }
    if( event.generation > 0 ) {
        json.member( "generation", event.generation );
    }
    if( event.handoff_epoch >= 0 ) {
        json.member( "handoff_epoch", event.handoff_epoch );
    }
    if( !event.simulation_owner.empty() ) {
        json.member( "simulation_owner", event.simulation_owner );
    }
    if( !event.previous_phase.empty() ) {
        json.member( "previous_state", event.previous_phase );
    }
    if( !event.new_phase.empty() ) {
        json.member( "new_state", event.new_phase );
    }
    if( !event.reason.empty() ) {
        json.member( "reason", event.reason );
    }
    if( !event.actor_ids.empty() ) {
        json.member( "actor_ids" );
        json.start_array();
        for( const std::int64_t actor_id : event.actor_ids ) {
            json.write( actor_id );
        }
        json.end_array();
    }
    if( event.turn >= 0 ) {
        json.member( "turn", event.turn );
    }
    if( !event.fixture_actor_id.empty() ) {
        json.member( "fixture_actor_id", event.fixture_actor_id );
        json.member( "lifecycle_event", event.lifecycle_event );
        json.member( "monster_type", event.monster_type );
        json.member( "absolute_position", event.absolute_position );
        json.member( "relative_position", event.relative_position );
        json.member( "hitpoints", event.hitpoints );
        json.member( "dead", event.dead );
        json.member( "visible", event.visible );
    }
    if( !event.certification_round_id.empty() ) {
        json.member( "certification_save_receipt" );
        json.start_object();
        json.member( "round_id", event.certification_round_id );
        json.member( "event_stream_id", transition_stream.run_id );
        json.member( "lease_id", event.certification_lease_id );
        json.member( "process_pid", event.certification_process_pid );
        json.member( "save_sequence", event.certification_save_sequence );
        json.member( "previous_world_tree_sha256", event.certification_previous_world_tree_sha256 );
        json.member( "previous_world_save_sha256", event.certification_previous_world_save_sha256 );
        json.member( "current_world_tree_sha256", event.certification_current_world_tree_sha256 );
        json.member( "current_world_save_sha256", event.certification_current_world_save_sha256 );
        json.member( "proof", event.certification_proof );
        json.end_object();
    }
    json.end_object();
    output << '\n';
    output.flush();
    if( !output ) {
        return false;
    }
    transition_stream.last_sequence = sequence;
    return true;
}
} // namespace

namespace bandit_live_world_probe
{
void bounded_latency_histogram::add( const std::int64_t nanoseconds ) noexcept
{
    const std::uint64_t value = static_cast<std::uint64_t>( std::max<std::int64_t>( 0,
                                nanoseconds ) );
    if( count_ == 0 ) {
        minimum_ns_ = static_cast<std::int64_t>( value );
    } else {
        minimum_ns_ = std::min<std::int64_t>( minimum_ns_, static_cast<std::int64_t>( value ) );
    }
    maximum_ns_ = std::max<std::int64_t>( maximum_ns_, static_cast<std::int64_t>( value ) );
    if( std::numeric_limits<std::uint64_t>::max() - total_ns_ < value ) {
        total_ns_ = std::numeric_limits<std::uint64_t>::max();
        overflow_ = true;
    } else {
        total_ns_ += value;
    }
    if( count_ == std::numeric_limits<std::uint64_t>::max() ) {
        overflow_ = true;
    } else {
        ++count_;
    }

    if( value == 0 ) {
        if( zero_count_ == std::numeric_limits<std::uint64_t>::max() ) {
            overflow_ = true;
        } else {
            ++zero_count_;
        }
        return;
    }

    std::size_t exponent = 0;
    for( std::uint64_t shifted = value; shifted > 1; shifted >>= 1 ) {
        ++exponent;
    }
    const std::uint64_t base = std::uint64_t( 1 ) << exponent;
    const std::uint64_t delta = value - base;
    const std::size_t subdivision = exponent >= 6 ?
                                    static_cast<std::size_t>( delta / ( base >> 6 ) ) :
                                    static_cast<std::size_t>( delta * subdivisions / base );
    const std::size_t index = exponent * subdivisions +
                              std::min<std::size_t>( subdivision, subdivisions - 1 );
    if( index >= buckets_.size() || buckets_[index] == std::numeric_limits<std::uint32_t>::max() ) {
        overflow_ = true;
    } else {
        ++buckets_[index];
    }
}

latency_summary bounded_latency_histogram::summarize() const noexcept
{
    latency_summary result;
    result.count = count_;
    result.total_ns = total_ns_;
    result.minimum_ns = minimum_ns_;
    result.maximum_ns = maximum_ns_;
    result.overflow = overflow_;
    if( count_ == 0 ) {
        return result;
    }

    const auto percentile = [this]( const std::uint64_t numerator ) {
        const std::uint64_t rank = numerator * ( count_ / 100 ) +
                                   ( numerator * ( count_ % 100 ) + 99 ) / 100;
        std::uint64_t cumulative = zero_count_;
        if( cumulative >= rank ) {
            return std::int64_t( 0 );
        }
        for( std::size_t index = 0; index < buckets_.size(); ++index ) {
            cumulative += buckets_[index];
            if( cumulative >= rank ) {
                return std::min<std::int64_t>( bucket_upper_bound( index ), maximum_ns_ );
            }
        }
        return maximum_ns_;
    };
    result.p50_ns = percentile( 50 );
    result.p95_ns = percentile( 95 );
    result.p99_ns = percentile( 99 );
    return result;
}

struct session::timing_state {
    std::array<bounded_latency_histogram, section_count> inclusive;
    std::array<bounded_latency_histogram, section_count> self;
};

session::session( const collection_mode mode, const std::size_t expected_samples,
                  const std::size_t expected_sites )
{
    previous_ = active_session;
    result_.timings_collected = mode == collection_mode::timings;
    result_.site_services_collected = mode == collection_mode::site_services;
    result_.transition_events_collected = mode == collection_mode::transition_events;
    if( result_.timings_collected ) {
        timing_state_ = std::make_unique<timing_state>();
    }
    ( void )expected_samples;
    if( result_.site_services_collected && expected_sites > 0 ) {
        result_.site_services.reserve( expected_sites );
        site_service_indices_.reserve( expected_sites );
    }
    if( result_.transition_events_collected ) {
        result_.transition_events.reserve( max_transition_events );
    }
    active_session = this;
}

session::~session()
{
    assert( active_session == this );
    assert( stack_depth_ == 0 );
    active_session = previous_;
}

const snapshot &session::result()
{
    if( timing_state_ ) {
        for( std::size_t index = 0; index < section_count; ++index ) {
            result_.sections[index].inclusive = timing_state_->inclusive[index].summarize();
            result_.sections[index].self = timing_state_->self[index].summarize();
        }
    }
    return result_;
}

void session::record_timing( const section target, const std::int64_t inclusive_ns,
                             const std::int64_t self_ns ) noexcept
{
    assert( timing_state_ );
    timing_state_->inclusive[enum_index( target )].add( inclusive_ns );
    timing_state_->self[enum_index( target )].add( self_ns );
}

scoped_section::scoped_section( const section target ) noexcept
{
    session_ = active_session;
    if( session_ == nullptr || !session_->result_.timings_collected ) {
        session_ = nullptr;
        return;
    }
    if( session_->stack_depth_ >= session::max_nested_sections ) {
        session_->result_.stack_overflow = true;
        session_ = nullptr;
        return;
    }

    frame_index_ = session_->stack_depth_;
    session_->stack_[frame_index_] = { target, 0 };
    session_->stack_depth_++;
    started_ = std::chrono::steady_clock::now();
}

scoped_section::~scoped_section()
{
    if( session_ == nullptr ) {
        return;
    }

    const std::int64_t inclusive_ns = std::max<std::int64_t>( 0,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_ ).count() );
    assert( session_->stack_depth_ == frame_index_ + 1 );
    const session::active_frame frame = session_->stack_[frame_index_];
    session_->stack_depth_--;

    const std::int64_t self_ns = std::max<std::int64_t>( 0, inclusive_ns - frame.child_ns );
    session_->record_timing( frame.target, inclusive_ns, self_ns );

    if( session_->stack_depth_ > 0 ) {
        session_->stack_[session_->stack_depth_ - 1].child_ns += inclusive_ns;
    }
}

scoped_loaded_covert_member::scoped_loaded_covert_member( const bool enabled ) noexcept :
    enabled_( enabled )
{
    if( enabled_ ) {
        ++loaded_covert_member_depth;
    }
}

scoped_loaded_covert_member::~scoped_loaded_covert_member()
{
    if( enabled_ ) {
        assert( loaded_covert_member_depth > 0 );
        --loaded_covert_member_depth;
    }
}

scoped_loaded_covert_local_path_solve::scoped_loaded_covert_local_path_solve() noexcept
{
    if( loaded_covert_member_depth == 0 ) {
        return;
    }
    section_.emplace( section::loaded_covert_local_path_solve );
    increment( counter::loaded_covert_local_path_solves );
}

void increment( const counter target, const std::uint64_t amount )
{
    if( active_session == nullptr ) {
        return;
    }
    active_session->result_.counters[enum_index( target )] += amount;
}

void record_site_service( const std::string &site_id, const site_service target,
                          const std::uint64_t amount )
{
    if( active_session == nullptr || !active_session->result_.site_services_collected ) {
        return;
    }

    std::vector<site_service_record> &records = active_session->result_.site_services;
    const auto existing = active_session->site_service_indices_.find( site_id );
    std::size_t record_index = 0;
    if( existing == active_session->site_service_indices_.end() ) {
        record_index = records.size();
        records.push_back( site_service_record{} );
        records.back().site_id = site_id;
        active_session->site_service_indices_.emplace( records.back().site_id, record_index );
    } else {
        record_index = existing->second;
    }
    records[record_index].counts[enum_index( target )] += amount;
}

bool active() noexcept
{
    return active_session != nullptr;
}

bool loaded_covert_member_active() noexcept
{
    return loaded_covert_member_depth > 0;
}

bool transition_events_enabled() noexcept
{
    if( active_session != nullptr && active_session->result_.transition_events_collected ) {
        return true;
    }
    try {
        return live_transition_stream_enabled();
    } catch( ... ) {
        transition_stream = {};
        return false;
    }
}

void record_transition_event( const std::string_view operation_id, const int generation,
                              const std::string_view simulation_owner,
                              const std::string_view previous_phase,
                              const std::string_view new_phase, const std::string_view reason,
                              const int at_minutes )
{
    transition_event event;
    event.operation_id = operation_id;
    event.generation = generation;
    event.simulation_owner = simulation_owner;
    event.previous_phase = previous_phase;
    event.new_phase = new_phase;
    event.reason = reason;
    event.at_minutes = at_minutes;
    event.game_minutes = at_minutes;
    event.domain = "bandit_live_world";
    event.transition = "scout_phase";
    event.outcome = "committed";
    record_live_transition_event( event );
    record_transition_event( std::move( event ) );
}

void record_transition_event( transition_event event )
{
    if( event.reason.empty() ) {
        return;
    }
    event.schema_version = 1;
    if( event.outcome.empty() ) {
        event.outcome = "committed";
    }
    if( event.game_minutes < 0 ) {
        event.game_minutes = event.at_minutes;
    }
    if( active_session == nullptr || !active_session->result_.transition_events_collected ) {
        return;
    }
    if( event.operation_id.size() > max_transition_event_field_length ||
        event.simulation_owner.size() > max_transition_event_field_length ||
        event.previous_phase.size() > max_transition_event_field_length ||
        event.new_phase.size() > max_transition_event_field_length ||
        event.reason.size() > max_transition_event_reason_length ) {
        active_session->result_.dropped_transition_events++;
        return;
    }
    std::vector<transition_event> &events = active_session->result_.transition_events;
    if( events.size() >= max_transition_events ) {
        active_session->result_.dropped_transition_events++;
        return;
    }
    events.push_back( std::move( event ) );
}

void record_live_transition_event( transition_event event )
{
    if( event.reason.empty() ) {
        return;
    }
    event.schema_version = 1;
    if( event.outcome.empty() ) {
        event.outcome = "committed";
    }
    if( event.game_minutes < 0 ) {
        event.game_minutes = event.at_minutes;
    }
    try {
        append_live_transition_event( event );
    } catch( ... ) {
        transition_stream = {};
    }
}

void record_local_pair_handoff_snapshot( const transition_event &event,
        const std::string_view site_payload, const std::string_view owner_transition,
        const std::string_view omt )
{
    const char *const path_value = std::getenv(
                                      "OPENCLAW_HARNESS_LOCAL_PAIR_SNAPSHOT_PATH" );
    const char *const source_sha256 = std::getenv(
                                     "OPENCLAW_HARNESS_RUNTIME_SOURCE_SHA256" );
    const char *const executable_sha256 = std::getenv(
                                         "OPENCLAW_HARNESS_EXECUTABLE_SHA256" );
    if( path_value == nullptr || path_value[0] == '\0' || source_sha256 == nullptr ||
        source_sha256[0] == '\0' || executable_sha256 == nullptr ||
        executable_sha256[0] == '\0' || site_payload.empty() ||
        owner_transition.empty() || omt.empty() || !live_transition_stream_enabled() ||
        event.transition != "local_pair_handoff" || event.outcome != "committed" ||
        event.simulation_owner != "local" || event.run_id.empty() ||
        event.run_id != transition_stream.run_id || event.site_id.empty() ||
        event.operation_id.empty() || event.generation <= 0 || event.handoff_epoch <= 0 ||
        event.game_minutes < 0 || event.actor_ids.size() != 2 ) {
        return;
    }

    const std::filesystem::path path( path_value );
    std::error_code error;
    if( std::filesystem::exists( path, error ) || error ) {
        return;
    }
    const std::filesystem::path temporary = path.string() + ".tmp." + event.run_id;
    std::ofstream output( temporary, std::ios::binary | std::ios::trunc );
    if( !output ) {
        return;
    }
    JsonOut json( output );
    json.start_object();
    json.member( "schema_version", 1 );
    json.member( "run_id", event.run_id );
    json.member( "source" );
    json.start_object();
    json.member( "runtime_source_sha256", source_sha256 );
    json.member( "executable_sha256", executable_sha256 );
    json.end_object();
    json.member( "transition" );
    json.start_object();
    json.member( "domain", event.domain );
    json.member( "transition", event.transition );
    json.member( "outcome", event.outcome );
    json.member( "site_id", event.site_id );
    json.member( "operation_id", event.operation_id );
    json.member( "actor_ids", event.actor_ids );
    json.member( "generation", event.generation );
    json.member( "simulation_owner", event.simulation_owner );
    json.member( "previous_state", event.previous_phase );
    json.member( "new_state", event.new_phase );
    json.member( "handoff_epoch", event.handoff_epoch );
    json.member( "game_minutes", event.game_minutes );
    json.member( "omt", omt );
    json.member( "owner_transition", owner_transition );
    json.end_object();
    json.member( "site_payload" );
    *json.get_stream() << site_payload;
    json.set_need_separator();
    json.end_object();
    output << '\n';
    output.close();
    if( !output ) {
        std::filesystem::remove( temporary, error );
        return;
    }
    std::filesystem::rename( temporary, path, error );
    if( error ) {
        std::filesystem::remove( temporary, error );
    }
}

void record_fixture_monster_lifecycle( const monster &critter, const std::string_view event,
                                       const std::string_view owner )
{
    const char *const selected_actor = std::getenv(
                "OPENCLAW_HARNESS_R019_LIFECYCLE_ACTOR_ID" );
    const char *const selected_run = std::getenv( "OPENCLAW_HARNESS_R019_LIFECYCLE_RUN_ID" );
    const char *const active_run = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    if( selected_actor == nullptr || selected_actor[0] == '\0' || selected_run == nullptr ||
        active_run == nullptr || std::string_view( selected_run ) != active_run ||
        critter.get_value( "caol_fixture_actor_id" ).str() != selected_actor ) {
        return;
    }
    map &here = get_map();
    transition_event record;
    record.domain = "r019_fixture_lifecycle";
    record.transition = std::string( event );
    record.outcome = "diagnostic";
    record.reason = "exact_fixture_actor";
    record.simulation_owner = std::string( owner );
    record.turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    record.game_minutes = to_minutes<int>( calendar::turn - calendar::start_of_cataclysm );
    record.fixture_actor_id = selected_actor;
    record.lifecycle_event = std::string( event );
    record.monster_type = critter.type->id.str();
    record.absolute_position = critter.pos_abs().to_string_writable();
    record.relative_position = critter.pos_bub( here ).to_string_writable();
    record.hitpoints = critter.get_hp();
    record.dead = critter.is_dead();
    record.visible = get_avatar().sees( here, critter );
    record_live_transition_event( std::move( record ) );
}

void record_certification_save_receipt( const int game_minutes, const std::string &world_path )
{
    const auto required_environment = []( const char *const name ) -> std::string {
        const char *const value = std::getenv( name );
        return value != nullptr ? value : "";
    };
    transition_event event;
    event.domain = "certification";
    event.transition = "save_receipt";
    event.outcome = "committed";
    event.reason = "game_save_completed";
    event.game_minutes = game_minutes;
    event.certification_round_id = required_environment( "OPENCLAW_CERTIFICATION_ROUND_ID" );
    event.certification_lease_id = required_environment( "OPENCLAW_CERTIFICATION_LEASE_ID" );
    const std::string capability = required_environment(
                                           "OPENCLAW_CERTIFICATION_SAVE_CAPABILITY" );
    event.certification_previous_world_tree_sha256 = required_environment(
                "OPENCLAW_CERTIFICATION_PREVIOUS_WORLD_TREE_SHA256" );
    event.certification_previous_world_save_sha256 = required_environment(
                "OPENCLAW_CERTIFICATION_PREVIOUS_WORLD_SAVE_SHA256" );
    event.certification_current_world_tree_sha256 = world_tree_sha256( world_path );
    event.certification_current_world_save_sha256 = event.certification_current_world_tree_sha256;
    event.certification_process_pid = current_process_pid();
    const std::string sequence = required_environment( "OPENCLAW_CERTIFICATION_SAVE_SEQUENCE" );
    const auto parsed = std::from_chars( sequence.data(), sequence.data() + sequence.size(),
                                         event.certification_save_sequence );
    if( event.certification_round_id.empty() || event.certification_lease_id.empty() ||
        capability.empty() ||
        event.certification_previous_world_tree_sha256.size() != 64 ||
        event.certification_previous_world_save_sha256.size() != 64 ||
        event.certification_current_world_tree_sha256.size() != 64 ||
        event.certification_current_world_save_sha256.size() != 64 ||
        parsed.ec != std::errc() || parsed.ptr != sequence.data() + sequence.size() ||
        event.certification_save_sequence == 0 ) {
        return;
    }
    event.certification_proof = hmac_sha256( capability, certification_receipt_facts( event ) );
    record_live_transition_event( std::move( event ) );
}

std::string_view to_string( const section target )
{
    static constexpr std::array<std::string_view, section_count> names = {
        "world_serialize",
        "world_deserialize",
        "structural_maintenance",
        "structural_outings",
        "structural_scan",
        "structural_dispatch",
        "live_dispatch_plan",
        "live_dispatch_apply",
        "live_return_apply",
        "loaded_covert_prepass",
        "loaded_covert_member_motor",
        "loaded_covert_overmap_route_solve",
        "loaded_covert_local_path_solve",
    };
    return names[enum_index( target )];
}

std::string_view to_string( const counter target )
{
    static constexpr std::array<std::string_view, counter_count> names = {
        "world_serialize_calls",
        "world_deserialize_calls",
        "structural_maintenance_updates",
        "structural_scan_sites_considered",
        "structural_scan_candidates_sampled",
        "structural_scan_sites_skipped_not_camp",
        "structural_outing_sites_considered",
        "structural_dispatch_sites_considered",
        "live_dispatch_plans",
        "live_dispatch_applies",
        "live_return_applies",
        "loaded_covert_prepass_calls",
        "loaded_covert_members_processed",
        "loaded_covert_overmap_route_solves",
        "loaded_covert_local_path_solves",
    };
    return names[enum_index( target )];
}

std::string_view to_string( const site_service target )
{
    static constexpr std::array<std::string_view, site_service_count> names = {
        "scan_considered",
        "scan_samples",
        "outing_considered",
        "dispatch_considered",
        "live_dispatch_plan",
        "live_return_apply",
    };
    return names[enum_index( target )];
}
} // namespace bandit_live_world_probe

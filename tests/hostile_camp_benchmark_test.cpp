#include "bandit_live_world.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "bandit_live_world_probe.h"
#include "calendar.h"
#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"
#include "rng.h"

namespace
{
using benchmark_clock = std::chrono::steady_clock;
constexpr int legacy_loaded_roster_members = 14;
constexpr int legacy_saturated_leads_per_bandit_site = 12;
constexpr int benchmark_default_calendar_turn = 5220000;
constexpr int benchmark_default_season_length_days = 91;
constexpr int benchmark_max_season_length_days = std::numeric_limits<int>::max() /
        ( 24 * 60 * 60 );

struct benchmark_calendar_configuration {
    int turn = benchmark_default_calendar_turn;
    int season_length_days = benchmark_default_season_length_days;
};

struct cardinality {
    std::size_t sites = 0;
    std::size_t members = 0;
    std::size_t leads = 0;
};

class scoped_rng_restore
{
    public:
        scoped_rng_restore() : previous_( rng_get_engine() ) {}
        ~scoped_rng_restore() {
            rng_get_engine() = previous_;
        }

        scoped_rng_restore( const scoped_rng_restore & ) = delete;
        scoped_rng_restore &operator=( const scoped_rng_restore & ) = delete;

    private:
        cata_default_random_engine previous_;
};

class scoped_calendar_restore
{
    public:
        scoped_calendar_restore() : turn_( calendar::turn ),
            start_of_cataclysm_( calendar::start_of_cataclysm ),
            start_of_game_( calendar::start_of_game ), initial_season_( calendar::initial_season ),
            season_length_days_( to_days<int>( calendar::season_length() ) ),
            eternal_season_( calendar::eternal_season() ),
            eternal_night_( calendar::eternal_night() ), eternal_day_( calendar::eternal_day() ) {}
        ~scoped_calendar_restore() {
            calendar::turn = turn_;
            calendar::start_of_cataclysm = start_of_cataclysm_;
            calendar::start_of_game = start_of_game_;
            calendar::initial_season = initial_season_;
            calendar::set_season_length( season_length_days_ );
            calendar::set_eternal_season( eternal_season_ );
            calendar::set_eternal_night( eternal_night_ );
            calendar::set_eternal_day( eternal_day_ );
        }

        scoped_calendar_restore( const scoped_calendar_restore & ) = delete;
        scoped_calendar_restore &operator=( const scoped_calendar_restore & ) = delete;

    private:
        time_point turn_;
        time_point start_of_cataclysm_;
        time_point start_of_game_;
        season_type initial_season_;
        int season_length_days_;
        bool eternal_season_;
        bool eternal_night_;
        bool eternal_day_;
};

void apply_benchmark_calendar( const benchmark_calendar_configuration &configuration )
{
    calendar::set_season_length( configuration.season_length_days );
    calendar::set_eternal_season( false );
    calendar::set_eternal_night( false );
    calendar::set_eternal_day( false );
    calendar::start_of_cataclysm = calendar::turn_zero;
    calendar::start_of_game = calendar::turn_zero;
    calendar::initial_season = SPRING;
    calendar::turn = time_point::from_turn( configuration.turn );
}

bool benchmark_calendar_matches( const benchmark_calendar_configuration &configuration )
{
    return to_turn<int>( calendar::turn ) == configuration.turn &&
           calendar::start_of_cataclysm == calendar::turn_zero &&
           calendar::start_of_game == calendar::turn_zero && calendar::initial_season == SPRING &&
           to_days<int>( calendar::season_length() ) == configuration.season_length_days &&
           !calendar::eternal_season() && !calendar::eternal_night() && !calendar::eternal_day();
}

std::string environment_string( const char *name, const std::string &fallback = {} )
{
    const char *value = std::getenv( name );
    return value == nullptr ? fallback : value;
}

std::size_t environment_size( const char *name, const std::size_t fallback )
{
    const std::string text = environment_string( name );
    if( text.empty() ) {
        return fallback;
    }

    std::uint64_t value = 0;
    const std::from_chars_result parsed = std::from_chars( text.data(), text.data() + text.size(), value );
    if( parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() ||
        value > std::numeric_limits<std::size_t>::max() ) {
        throw std::runtime_error( std::string( "invalid nonnegative integer in " ) + name );
    }
    return static_cast<std::size_t>( value );
}

std::string serialize_world( const bandit_live_world::world_state &world )
{
    std::ostringstream buffer;
    JsonOut json( buffer, false );
    world.serialize( json );
    return buffer.str();
}

std::uint32_t rotate_right( const std::uint32_t value, const int shift )
{
    return ( value >> shift ) | ( value << ( 32 - shift ) );
}

std::string sha256( const std::string &content )
{
    static constexpr std::array<std::uint32_t, 64> round_constants = { {
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
            0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
            0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
            0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
            0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
        } };
    std::array<std::uint32_t, 8> state = { {
            0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
        } };

    std::vector<std::uint8_t> message( content.begin(), content.end() );
    const std::uint64_t bit_length = static_cast<std::uint64_t>( message.size() ) * 8;
    message.push_back( 0x80U );
    while( message.size() % 64 != 56 ) {
        message.push_back( 0 );
    }
    for( int shift = 56; shift >= 0; shift -= 8 ) {
        message.push_back( static_cast<std::uint8_t>( bit_length >> shift ) );
    }

    for( std::size_t chunk = 0; chunk < message.size(); chunk += 64 ) {
        std::array<std::uint32_t, 64> words = {};
        for( std::size_t index = 0; index < 16; ++index ) {
            const std::size_t offset = chunk + index * 4;
            words[index] = static_cast<std::uint32_t>( message[offset] ) << 24 |
                           static_cast<std::uint32_t>( message[offset + 1] ) << 16 |
                           static_cast<std::uint32_t>( message[offset + 2] ) << 8 |
                           static_cast<std::uint32_t>( message[offset + 3] );
        }
        for( std::size_t index = 16; index < words.size(); ++index ) {
            const std::uint32_t s0 = rotate_right( words[index - 15], 7 ) ^
                                     rotate_right( words[index - 15], 18 ) ^
                                     ( words[index - 15] >> 3 );
            const std::uint32_t s1 = rotate_right( words[index - 2], 17 ) ^
                                     rotate_right( words[index - 2], 19 ) ^
                                     ( words[index - 2] >> 10 );
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for( std::size_t index = 0; index < words.size(); ++index ) {
            const std::uint32_t sum1 = rotate_right( e, 6 ) ^ rotate_right( e, 11 ) ^
                                       rotate_right( e, 25 );
            const std::uint32_t choice = ( e & f ) ^ ( ~e & g );
            const std::uint32_t temporary1 = h + sum1 + choice + round_constants[index] + words[index];
            const std::uint32_t sum0 = rotate_right( a, 2 ) ^ rotate_right( a, 13 ) ^
                                       rotate_right( a, 22 );
            const std::uint32_t majority = ( a & b ) ^ ( a & c ) ^ ( b & c );
            const std::uint32_t temporary2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    std::ostringstream result;
    result << std::hex << std::setfill( '0' );
    for( const std::uint32_t word : state ) {
        result << std::setw( 8 ) << word;
    }
    return result.str();
}

cardinality measure_cardinality( const bandit_live_world::world_state &world )
{
    cardinality result;
    result.sites = world.sites.size();
    for( const bandit_live_world::site_record &site : world.sites ) {
        result.members += site.members.size();
        result.leads += site.intelligence_map.leads.size();
    }
    return result;
}

void add_legacy_saturated_leads( bandit_live_world::site_record &site )
{
    static const std::array<std::pair<int, int>, legacy_saturated_leads_per_bandit_site>
    near_offsets = { {
            { -4, 0 }, { 4, 0 }, { 0, -4 }, { 0, 4 },
            { -5, -1 }, { 5, 1 }, { -1, 5 }, { 1, -5 },
            { -6, 0 }, { 6, 0 }, { 0, -6 }, { 0, 6 },
        } };

    site.intelligence_map.known_radius_omt = 8;
    site.intelligence_map.leads.reserve( near_offsets.size() );
    for( const std::pair<int, int> &offset : near_offsets ) {
        bandit_live_world::camp_map_lead lead;
        lead.kind = bandit_live_world::camp_lead_kind::structural_bounty;
        lead.status = bandit_live_world::camp_lead_status::harvested;
        lead.target_id = "forest";
        lead.omt = tripoint_abs_omt( site.anchor.x() + offset.first,
                                    site.anchor.y() + offset.second, site.anchor.z() );
        lead.lead_id = bandit_live_world::make_structural_bounty_lead_id(
                           site.site_id, lead.omt, lead.target_id );
        lead.source_key = "structural_bounty:forest";
        lead.source_summary = "low structural forest/woods bounty";
        lead.first_seen_minutes = 0;
        lead.last_seen_minutes = 0;
        lead.last_checked_minutes = 0;
        lead.confidence = 1;
        lead.times_harvested = 1;
        lead.last_outcome = "harvested_structural_bounty";
        site.intelligence_map.leads.push_back( std::move( lead ) );
    }
}

bandit_live_world::world_state make_legacy_fixture( const std::size_t site_count,
        const bool saturate_existing_leads )
{
    bandit_live_world::world_state world;
    world.owner_id = "hostile_camp_benchmark_legacy_v1";
    world.sites.reserve( site_count );

    for( std::size_t site_index = 0; site_index < site_count; ++site_index ) {
        const bool structural_camp = site_index % 2 == 0;
        bandit_live_world::site_record site;
        std::ostringstream site_id;
        site_id << "benchmark-site-" << std::setw( 3 ) << std::setfill( '0' ) << site_index;
        site.site_id = site_id.str();
        site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
        site.site_kind = structural_camp ? bandit_live_world::owned_site_kind::bandit_camp :
                         bandit_live_world::owned_site_kind::cannibal_camp;
        site.profile = structural_camp ? bandit_live_world::hostile_site_profile::camp_style :
                       bandit_live_world::hostile_site_profile::cannibal_camp;
        site.source_id = structural_camp ? "bandit_camp" : "cannibal_camp";
        site.headcount = legacy_loaded_roster_members;
        site.anchor = tripoint_abs_omt( static_cast<int>( site_index * 20 ),
                                       static_cast<int>( site_index * 20 ), 0 );
        site.footprint.push_back( site.anchor );
        const tripoint_abs_ms home_base = project_to<coords::ms>( site.anchor );

        for( int member_index = 0; member_index < legacy_loaded_roster_members; ++member_index ) {
            bandit_live_world::member_record member;
            member.npc_id = character_id( static_cast<int>( 200000 + site_index * 100 + member_index ) );
            member.npc_template_id = structural_camp ? "bandit" : "cannibal_hunter";
            member.home_spawn_tile = tripoint_abs_ms( home_base.x() + member_index % 7,
                                     home_base.y() + member_index / 7, home_base.z() );
            member.state = bandit_live_world::member_state::at_home;
            site.members.push_back( member );
            bandit_live_world::spawn_tile_record spawn_tile;
            spawn_tile.tile = member.home_spawn_tile;
            spawn_tile.headcount = 1;
            site.spawn_tiles.push_back( spawn_tile );
        }
        if( saturate_existing_leads && structural_camp ) {
            add_legacy_saturated_leads( site );
        }
        world.sites.push_back( std::move( site ) );
    }

    return world;
}

bandit_live_world_probe::bounded_latency_histogram measure_clock_floor(
    const std::size_t sample_count )
{
    bandit_live_world_probe::bounded_latency_histogram samples;
    for( std::size_t sample = 0; sample < sample_count; ++sample ) {
        const benchmark_clock::time_point started = benchmark_clock::now();
        const benchmark_clock::time_point finished = benchmark_clock::now();
        samples.add( std::chrono::duration_cast<std::chrono::nanoseconds>(
                         finished - started ).count() );
    }
    return samples;
}

void write_latency_summary( JsonOut &json,
                            const bandit_live_world_probe::latency_summary &summary )
{
    json.start_object();
    json.member( "count", summary.count );
    json.member( "total", summary.total_ns );
    json.member( "min", summary.minimum_ns );
    json.member( "p50", summary.p50_ns );
    json.member( "p95", summary.p95_ns );
    json.member( "p99", summary.p99_ns );
    json.member( "max", summary.maximum_ns );
    json.member( "quantiles_are_upper_bounds", summary.quantiles_are_upper_bounds );
    json.member( "relative_resolution_ppm", summary.relative_resolution_ppm );
    json.member( "overflow", summary.overflow );
    json.end_object();
}

void write_cardinality( JsonOut &json, const cardinality &value )
{
    json.start_object();
    json.member( "sites", value.sites );
    json.member( "members", value.members );
    json.member( "leads", value.leads );
    json.end_object();
}

const bandit_live_world_probe::site_service_record *find_site_service(
    const bandit_live_world_probe::snapshot &probe, const std::string &site_id )
{
    const auto iter = std::find_if( probe.site_services.begin(), probe.site_services.end(),
    [&site_id]( const bandit_live_world_probe::site_service_record & record ) {
        return record.site_id == site_id;
    } );
    return iter == probe.site_services.end() ? nullptr : &*iter;
}

void write_probe_result( JsonOut &json, const bandit_live_world_probe::snapshot &probe )
{
    json.start_object();
    json.member( "timings_collected", probe.timings_collected );
    json.member( "site_services_collected", probe.site_services_collected );
    json.member( "stack_overflow", probe.stack_overflow );

    json.member( "sections" );
    json.start_object();
    for( std::size_t index = 0; index < bandit_live_world_probe::section_count; ++index ) {
        const auto target = static_cast<bandit_live_world_probe::section>( index );
        const bandit_live_world_probe::section_samples &samples = probe.sections[index];
        json.member( bandit_live_world_probe::to_string( target ) );
        json.start_object();
        json.member( "calls", samples.inclusive.count );
        json.member( "inclusive_total_ns", samples.inclusive.total_ns );
        json.member( "inclusive_summary_ns" );
        write_latency_summary( json, samples.inclusive );
        json.member( "self_total_ns", samples.self.total_ns );
        json.member( "self_summary_ns" );
        write_latency_summary( json, samples.self );
        json.end_object();
    }
    json.end_object();

    json.member( "counters" );
    json.start_object();
    for( std::size_t index = 0; index < bandit_live_world_probe::counter_count; ++index ) {
        const auto target = static_cast<bandit_live_world_probe::counter>( index );
        json.member( bandit_live_world_probe::to_string( target ), probe.counters[index] );
    }
    json.end_object();
    json.end_object();
}

void write_fairness( JsonOut &json, const bandit_live_world::world_state &world,
                     const bandit_live_world_probe::snapshot &probe )
{
    std::uint64_t minimum_scan_samples = 0;
    std::uint64_t maximum_scan_samples = 0;
    std::uint64_t scan_serviced_sites = 0;
    std::uint64_t eligible_structural_sites = 0;
    std::uint64_t eligible_structural_sites_serviced = 0;
    std::uint64_t minimum_eligible_scan_samples = 0;
    std::uint64_t maximum_eligible_scan_samples = 0;
    bool have_site = false;
    bool have_eligible_site = false;

    json.start_object();
    json.member( "per_site" );
    json.start_array();
    for( const bandit_live_world::site_record &site : world.sites ) {
        const bandit_live_world_probe::site_service_record *record = find_site_service( probe, site.site_id );
        std::array<std::uint64_t, bandit_live_world_probe::site_service_count> counts = {};
        if( record != nullptr ) {
            counts = record->counts;
        }
        const std::uint64_t scan_samples = counts[static_cast<std::size_t>(
                bandit_live_world_probe::site_service::scan_samples )];
        if( !have_site ) {
            minimum_scan_samples = scan_samples;
            have_site = true;
        } else {
            minimum_scan_samples = std::min( minimum_scan_samples, scan_samples );
        }
        maximum_scan_samples = std::max( maximum_scan_samples, scan_samples );
        scan_serviced_sites += scan_samples > 0 ? 1 : 0;
        const bool structurally_eligible = site.profile ==
                                           bandit_live_world::hostile_site_profile::camp_style;
        if( structurally_eligible ) {
            eligible_structural_sites++;
            eligible_structural_sites_serviced += scan_samples > 0 ? 1 : 0;
            if( !have_eligible_site ) {
                minimum_eligible_scan_samples = scan_samples;
                have_eligible_site = true;
            } else {
                minimum_eligible_scan_samples = std::min( minimum_eligible_scan_samples, scan_samples );
            }
            maximum_eligible_scan_samples = std::max( maximum_eligible_scan_samples, scan_samples );
        }

        json.start_object();
        json.member( "site_id", site.site_id );
        for( std::size_t index = 0; index < bandit_live_world_probe::site_service_count; ++index ) {
            const auto target = static_cast<bandit_live_world_probe::site_service>( index );
            json.member( bandit_live_world_probe::to_string( target ), counts[index] );
        }
        json.end_object();
    }
    json.end_array();
    json.member( "site_count", world.sites.size() );
    json.member( "scan_serviced_sites", scan_serviced_sites );
    json.member( "minimum_scan_samples", minimum_scan_samples );
    json.member( "maximum_scan_samples", maximum_scan_samples );
    json.member( "scan_sample_spread", maximum_scan_samples - minimum_scan_samples );
    json.member( "eligible_structural_sites", eligible_structural_sites );
    json.member( "eligible_structural_sites_serviced", eligible_structural_sites_serviced );
    json.member( "minimum_eligible_scan_samples", minimum_eligible_scan_samples );
    json.member( "maximum_eligible_scan_samples", maximum_eligible_scan_samples );
    json.member( "eligible_scan_sample_spread",
                 maximum_eligible_scan_samples - minimum_eligible_scan_samples );
    json.member( "eventual_structural_service",
                 eligible_structural_sites_serviced == eligible_structural_sites );
    json.null_member( "scheduler_wait_updates" );
    json.end_object();
}

void run_workload_update( const std::string &workload, bandit_live_world::world_state &world,
                          const std::size_t update_index, std::string &last_serialized )
{
    if( workload == "serialize" ) {
        last_serialized = serialize_world( world );
        JsonValue value = json_loader::from_string( last_serialized );
        bandit_live_world::world_state restored;
        restored.deserialize( value.get_object() );
        world = std::move( restored );
        return;
    }

    if( workload == "dispatch_return" ) {
        if( world.sites.empty() ) {
            return;
        }
        bandit_live_world::site_record &site = world.sites[update_index % world.sites.size()];
        const tripoint_abs_omt target( site.anchor.x() + 8, site.anchor.y(), site.anchor.z() );
        const bandit_live_world::dispatch_plan plan = bandit_live_world::plan_site_dispatch(
                    site, target, site.site_id + "#benchmark-target" );
        if( !plan.valid || !bandit_live_world::apply_dispatch_plan( site, plan ) ) {
            throw std::runtime_error( "deterministic dispatch-return fixture could not dispatch" );
        }
        bandit_pursuit_handoff::local_outcome outcome;
        outcome.survivors_remaining = static_cast<int>( plan.member_ids.size() );
        outcome.result = bandit_pursuit_handoff::mission_result::scouted;
        outcome.resolution = bandit_pursuit_handoff::lead_resolution::still_valid;
        outcome.posture = bandit_pursuit_handoff::return_posture::escape_home;
        outcome.remaining_pressure =
            bandit_pursuit_handoff::remaining_return_pressure_state::ample;
        const bandit_pursuit_handoff::return_packet packet =
            bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );
        if( !bandit_live_world::apply_return_packet( site, packet ) ) {
            throw std::runtime_error( "deterministic dispatch-return fixture could not write back" );
        }
        return;
    }

    const int now_minutes = static_cast<int>( update_index %
                            static_cast<std::size_t>( std::numeric_limits<int>::max() ) );
    const bool saturated_leads = workload == "lead_saturated";
    const int scan_budget = saturated_leads ? static_cast<int>( world.sites.size() * 4 ) :
                            workload == "structural" ? 4 : 0;
    const int dispatch_cap = workload == "structural" ? 1 : 0;
    bandit_live_world::advance_structural_bounty_maintenance( world, now_minutes, scan_budget,
            dispatch_cap,
    []( const tripoint_abs_omt &omt ) -> std::optional<std::string> {
        return omt.z() == 0 ? std::optional<std::string>( "forest" ) : std::nullopt;
    }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true,
               "deterministic hostile-camp benchmark terrain" };
    } );
}

std::string make_result_json( const std::string &fixture, const std::string &workload,
                              const std::string &fixture_sha256, const std::size_t repetition,
                              const std::string &variant, const unsigned int rng_seed,
                              const benchmark_calendar_configuration &calendar_configuration,
                              const std::size_t updates, const std::size_t clock_floor_samples,
                              const bandit_live_world_probe::latency_summary &update_latency,
                              const bandit_live_world_probe::latency_summary &clock_floor,
                              const std::int64_t wall_time_ns,
                              const bandit_live_world::world_state &terminal_world,
                              const bandit_live_world_probe::snapshot &timing_probe,
                              const bandit_live_world_probe::snapshot &fairness_probe,
                              const std::string &initial_serialized,
                              const std::string &terminal_serialized,
                              const cardinality &initial_cardinality,
                              const cardinality &terminal_cardinality )
{
    std::ostringstream buffer;
    JsonOut json( buffer, false );
    json.start_object();
    json.member( "schema", "caol-hostile-camp-benchmark-result-v1" );
    json.member( "fixture", fixture );
    json.member( "fixture_sha256", fixture_sha256 );
    json.member( "workload", workload );
    json.member( "repetition", repetition );
    json.member( "variant", variant );
    json.member( "rng_seed", rng_seed );
    json.member( "updates", updates );
    json.member( "clock_floor_samples", clock_floor_samples );

    json.member( "calendar" );
    json.start_object();
    json.member( "turn", calendar_configuration.turn );
    json.member( "start_of_cataclysm_turn", 0 );
    json.member( "start_of_game_turn", 0 );
    json.member( "initial_season", "spring" );
    json.member( "season_length_days", calendar_configuration.season_length_days );
    json.member( "eternal_season", false );
    json.member( "eternal_night", false );
    json.member( "eternal_day", false );
    json.member( "reset_before_timing_replay", true );
    json.member( "reset_before_fairness_replay", true );
    json.end_object();

    json.member( "metrics" );
    json.start_object();
    json.member( "wall_time_ns", wall_time_ns );
    json.member( "update_latency_sample_count", update_latency.count );
    json.member( "update_latency_summary_ns" );
    write_latency_summary( json, update_latency );
    json.member( "clock_floor_sample_count", clock_floor.count );
    json.member( "clock_floor_summary_ns" );
    write_latency_summary( json, clock_floor );
    json.null_member( "peak_resident_bytes" );
    json.null_member( "allocation_count" );
    json.null_member( "live_heap_bytes" );
    json.end_object();

    json.member( "probe" );
    write_probe_result( json, timing_probe );

    json.member( "serialization" );
    json.start_object();
    json.member( "initial_bytes", initial_serialized.size() );
    json.member( "terminal_bytes", terminal_serialized.size() );
    json.member( "growth_bytes", static_cast<std::int64_t>( terminal_serialized.size() ) -
                 static_cast<std::int64_t>( initial_serialized.size() ) );
    json.member( "initial_cardinality" );
    write_cardinality( json, initial_cardinality );
    json.member( "terminal_cardinality" );
    write_cardinality( json, terminal_cardinality );
    json.null_member( "compressed_save_bytes" );
    json.end_object();

    json.member( "fairness" );
    write_fairness( json, terminal_world, fairness_probe );

    json.member( "measurement_modes" );
    json.start_object();
    json.member( "latency", "timing replay with fixed counters and per-site recording disabled" );
    json.member( "fairness",
                 "untimed deterministic replay with clocks and timing samples disabled; serialization has no scheduler replay" );
    json.member( "terminal_state_match", true );
    json.end_object();

    json.member( "fixture_model" );
    json.start_object();
    json.member( "profiles", "alternating bandit camp and cannibal camp" );
    json.member( "loaded_roster_members_per_site", legacy_loaded_roster_members );
    json.member( "abstract_bandit_roster_members", 6 );
    json.member( "abstract_cannibal_roster_members", 5 );
    json.member( "active_roster_model", "loaded" );
    json.member( "existing_leads_per_bandit_site",
                 workload == "lead_saturated" ? legacy_saturated_leads_per_bandit_site : 0 );
    json.member( "existing_lead_status",
                 workload == "lead_saturated" ? "harvested" : "none" );
    json.member( "serialize_workload", "world_state serialize plus deserialize round trip" );
    json.member( "structural_workload",
                 "zero-lead bounded scan, dispatch, and structural return" );
    json.member( "lead_saturated_workload",
                 "all current near-ring structural leads already harvested; bounded scan tests "
                 "existing-memory saturation without dispatch" );
    json.member( "dispatch_return_workload",
                 "representative current dispatch and physical return writeback" );
    json.member( "stress_packet", terminal_world.sites.size() == 500 ?
                 "500-site structural fairness stress; excluded from normal scaling budgets" : "none" );
    json.end_object();

    json.member( "deterministic_state" );
    json.start_object();
    json.member( "hash_algorithm", "sha256" );
    json.member( "initial_sha256", sha256( initial_serialized ) );
    json.member( "terminal_sha256", sha256( terminal_serialized ) );
    json.null_member( "raw_initial_serialized" );
    json.null_member( "raw_terminal_serialized" );
    json.end_object();

    json.member( "forward_metrics" );
    json.start_object();
    json.null_member( "route_queries" );
    json.null_member( "route_expansions" );
    json.null_member( "shared_high_level_routes" );
    json.null_member( "scheduler_queue_depth" );
    json.end_object();
    json.end_object();
    return buffer.str();
}

void emit_result( const std::string &output_path, const std::string &result )
{
    if( output_path.empty() ) {
        std::cout << result << '\n';
        return;
    }

    std::ofstream output( output_path, std::ios::binary | std::ios::trunc );
    if( !output ) {
        throw std::runtime_error( "cannot open CAOL_HOSTILE_BENCHMARK_OUTPUT" );
    }
    output << result << '\n';
    if( !output ) {
        throw std::runtime_error( "cannot write CAOL_HOSTILE_BENCHMARK_OUTPUT" );
    }
}
} // namespace

TEST_CASE( "hostile camp latency histogram is bounded and conservative",
           "[bandit][hostile_camp_benchmark_histogram]" )
{
    bandit_live_world_probe::bounded_latency_histogram histogram;
    for( std::int64_t sample = 1; sample <= 100; ++sample ) {
        histogram.add( sample );
    }

    const bandit_live_world_probe::latency_summary summary = histogram.summarize();
    CHECK( summary.count == 100 );
    CHECK( summary.total_ns == 5050 );
    CHECK( summary.minimum_ns == 1 );
    CHECK( summary.p50_ns >= 50 );
    CHECK( summary.p50_ns <= 51 );
    CHECK( summary.p95_ns >= 95 );
    CHECK( summary.p95_ns <= 96 );
    CHECK( summary.p99_ns >= 99 );
    CHECK( summary.p99_ns <= 100 );
    CHECK( summary.maximum_ns == 100 );
    CHECK( summary.quantiles_are_upper_bounds );
    CHECK( summary.relative_resolution_ppm == 15625 );
    CHECK_FALSE( summary.overflow );
}

TEST_CASE( "hostile camp deterministic benchmark driver",
           "[.][bandit][hostile_camp][hostile_camp_benchmark][benchmark]" )
{
    const std::string fixture = environment_string( "CAOL_HOSTILE_BENCHMARK_FIXTURE" );
    const std::string fixture_sha256 = environment_string(
                                           "CAOL_HOSTILE_BENCHMARK_FIXTURE_SHA256" );
    const std::string fixture_hash_kind = environment_string(
            "CAOL_HOSTILE_BENCHMARK_FIXTURE_HASH_KIND", "opaque_sha256" );
    const std::string workload = environment_string( "CAOL_HOSTILE_BENCHMARK_WORKLOAD" );
    const std::size_t repetition = environment_size( "CAOL_HOSTILE_BENCHMARK_REPETITION", 0 );
    const std::size_t site_count = environment_size( "CAOL_HOSTILE_BENCHMARK_SITE_COUNT", 1 );
    const std::size_t updates = environment_size( "CAOL_HOSTILE_BENCHMARK_UPDATES", 10000 );
    const std::size_t clock_floor_samples = environment_size(
            "CAOL_HOSTILE_BENCHMARK_CLOCK_FLOOR_SAMPLES", 10000 );
    const std::size_t configured_calendar_turn = environment_size(
            "CAOL_HOSTILE_BENCHMARK_CALENDAR_TURN", benchmark_default_calendar_turn );
    const std::size_t configured_season_length_days = environment_size(
            "CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS",
            benchmark_default_season_length_days );
    const std::string variant = environment_string( "CAOL_HOSTILE_BENCHMARK_VARIANT",
                                "sites-" + std::to_string( site_count ) );
    const std::string output_path = environment_string( "CAOL_HOSTILE_BENCHMARK_OUTPUT" );
    const std::size_t recorded_seed = environment_size( "CAOL_HOSTILE_BENCHMARK_SEED", 0 );
    const unsigned int effective_seed = Catch::rngSeed();

    REQUIRE( sha256( "abc" ) ==
             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" );
    const bool supported_workload = workload == "idle" || workload == "structural" ||
                                    workload == "lead_saturated" || workload == "serialize" ||
                                    workload == "dispatch_return";
    REQUIRE( supported_workload );
    REQUIRE( site_count <= 1000 );
    REQUIRE( updates > 0 );
    REQUIRE( updates <= 1000000 );
    REQUIRE( clock_floor_samples > 0 );
    REQUIRE( clock_floor_samples <= 1000000 );
    REQUIRE( configured_calendar_turn <= std::numeric_limits<int>::max() );
    REQUIRE( configured_season_length_days > 0 );
    REQUIRE( configured_season_length_days <= benchmark_max_season_length_days );
    REQUIRE( recorded_seed > 0 );
    REQUIRE( recorded_seed <= std::numeric_limits<unsigned int>::max() );
    REQUIRE( effective_seed == static_cast<unsigned int>( recorded_seed ) );
    const std::string expected_fixture = "legacy_" + workload + "_sites_" +
                                         std::to_string( site_count ) + "_v1";
    REQUIRE( fixture == expected_fixture );

    const benchmark_calendar_configuration calendar_configuration = {
        static_cast<int>( configured_calendar_turn ),
        static_cast<int>( configured_season_length_days )
    };
    const bool saturate_existing_leads = workload == "lead_saturated";
    const scoped_calendar_restore restore_calendar;
    apply_benchmark_calendar( calendar_configuration );
    bandit_live_world::world_state world = make_legacy_fixture( site_count,
            saturate_existing_leads );
    const cardinality initial_cardinality = measure_cardinality( world );
    const std::size_t expected_initial_leads = saturate_existing_leads ?
            ( site_count + 1 ) / 2 * legacy_saturated_leads_per_bandit_site : 0;
    REQUIRE( initial_cardinality.leads == expected_initial_leads );
    const std::string initial_serialized = serialize_world( world );
    if( fixture_hash_kind == "serialized_state_sha256" ) {
        REQUIRE( fixture_sha256 == sha256( initial_serialized ) );
    } else {
        const bool supported_hash_kind = fixture_hash_kind == "opaque_sha256" ||
                                         fixture_hash_kind == "generated_case_spec_sha256";
        REQUIRE( supported_hash_kind );
    }
    const scoped_rng_restore restore_rng;
    const bandit_live_world_probe::bounded_latency_histogram clock_floor_histogram =
        measure_clock_floor( clock_floor_samples );
    bandit_live_world_probe::bounded_latency_histogram update_latency_histogram;
    std::string last_serialized;
    bandit_live_world_probe::snapshot timing_probe_result;
    std::int64_t wall_time_ns = 0;

    apply_benchmark_calendar( calendar_configuration );
    rng_set_engine_seed( effective_seed );
    {
        bandit_live_world_probe::session probe_session(
            bandit_live_world_probe::collection_mode::timings, updates, 0 );
        const benchmark_clock::time_point wall_started = benchmark_clock::now();
        for( std::size_t update = 0; update < updates; ++update ) {
            const benchmark_clock::time_point update_started = benchmark_clock::now();
            run_workload_update( workload, world, update, last_serialized );
            update_latency_histogram.add( std::chrono::duration_cast<std::chrono::nanoseconds>(
                                              benchmark_clock::now() - update_started ).count() );
        }
        wall_time_ns = std::max<std::int64_t>( 0,
                                              std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                      benchmark_clock::now() - wall_started ).count() );
        timing_probe_result = probe_session.result();
    }
    REQUIRE( benchmark_calendar_matches( calendar_configuration ) );

    const std::string terminal_serialized = serialize_world( world );
    const cardinality terminal_cardinality = measure_cardinality( world );

    apply_benchmark_calendar( calendar_configuration );
    bandit_live_world::world_state fairness_world = make_legacy_fixture( site_count,
            saturate_existing_leads );
    bandit_live_world_probe::snapshot fairness_probe_result;
    rng_set_engine_seed( effective_seed );
    {
        bandit_live_world_probe::session fairness_session(
            bandit_live_world_probe::collection_mode::site_services, 0, site_count );
        std::string fairness_last_serialized;
        if( workload != "serialize" ) {
            for( std::size_t update = 0; update < updates; ++update ) {
                run_workload_update( workload, fairness_world, update, fairness_last_serialized );
            }
        }
        fairness_probe_result = fairness_session.result();
    }
    REQUIRE( benchmark_calendar_matches( calendar_configuration ) );
    const std::string fairness_terminal_serialized = serialize_world( fairness_world );
    const bandit_live_world_probe::latency_summary update_latency =
        update_latency_histogram.summarize();
    const bandit_live_world_probe::latency_summary clock_floor = clock_floor_histogram.summarize();
    REQUIRE( update_latency.count == updates );
    REQUIRE( clock_floor.count == clock_floor_samples );
    REQUIRE_FALSE( update_latency.overflow );
    REQUIRE_FALSE( clock_floor.overflow );
    REQUIRE_FALSE( initial_serialized.empty() );
    REQUIRE_FALSE( terminal_serialized.empty() );
    if( workload == "serialize" ) {
        REQUIRE( last_serialized == terminal_serialized );
    }
    REQUIRE_FALSE( timing_probe_result.stack_overflow );
    REQUIRE_FALSE( fairness_probe_result.stack_overflow );
    REQUIRE( timing_probe_result.timings_collected );
    REQUIRE_FALSE( timing_probe_result.site_services_collected );
    REQUIRE_FALSE( fairness_probe_result.timings_collected );
    REQUIRE( fairness_probe_result.site_services_collected );
    REQUIRE( fairness_terminal_serialized == terminal_serialized );

    emit_result( output_path, make_result_json( fixture, workload, fixture_sha256, repetition,
                 variant, effective_seed, calendar_configuration, updates, clock_floor_samples, update_latency,
                 clock_floor,
                 wall_time_ns, world, timing_probe_result, fairness_probe_result,
                 initial_serialized, terminal_serialized, initial_cardinality,
                 terminal_cardinality ) );
}

// RUN: %check_clang_tidy %s cata-overmap-party-roster %t -- --load=%cata_plugin --

// Minimal container declarations keep the lint fixture independent of the SDK.
namespace std
{
template<typename T, unsigned N> struct array {};
}

struct roster {
    unsigned size() const;
};

bool read_shared_handoff( const roster &member_reads )
{
    return member_reads.size() <= 2;
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: review this fixed pair size in an overmap roster consumer
}

bool reject_shared_travel( const roster &member_ids )
{
    return 3 <= member_ids.size();
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: review this fixed pair size in an overmap roster consumer
}

// Mi-go settlement reads every issued operation member in terminal_at_home.
namespace migo_tower
{
struct operation_record {
    roster members;
};

bool terminal_at_home( const operation_record &operation )
{
    return operation.members.size() == 2;
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: review this fixed pair size in an overmap roster consumer
}
}

// Population discovery adopts each known resident by identity.
namespace migo_population
{
void discover()
{
    std::array<int, 2> members;
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: review this two-slot array in an overmap roster consumer
}
}

bool restore_shared_settlement( const roster &member_reads, const roster &member_ids )
{
    return member_reads.size() == member_ids.size();
}

// Rider bands persist their merged actors and each casualty by identity.
struct rider_band_registry {
    std::array<int, 2> actor_ids;
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: review this two-slot array in an overmap roster consumer
};

bool select_routine_scout_pair( const roster &members )
{
    return members.size() == 2;
}

bool plan_structural_bounty_outing( const roster &members )
{
    return members.size() == 2;
}

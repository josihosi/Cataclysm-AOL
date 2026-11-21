// RUN: %check_clang_tidy %s cata-horde-light-sample-order %t -- --load=%cata_plugin --

struct sample_key {};
bool operator==( const sample_key &, const sample_key & );
struct horde_entity {
    sample_key light_sample_id;
    sample_key other_id;
};
struct sight_test {
    explicit operator bool() const;
    bool operator()( int ) const;
};
struct horde_map {
    bool attract_entities_to_light( const horde_entity &, const sample_key &, const sight_test & );
};

bool horde_map::attract_entities_to_light( const horde_entity &entity, const sample_key &sample_id,
        const sight_test &visible )
{
    if( ( visible && !visible( 1 ) ) || entity.light_sample_id == sample_id ) {
        // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: visibility is evaluated before the duplicate light sample check. Check the sample ID first to skip the sight test for known samples [cata-horde-light-sample-order]
        return false;
    }
    if( sample_id == entity.light_sample_id || !visible( 2 ) ) {
        return false;
    }
    return !visible( 3 ) || entity.other_id == sample_id;
}

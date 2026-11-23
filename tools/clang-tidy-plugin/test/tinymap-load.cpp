// RUN: %check_clang_tidy %s cata-tinymap-load %t -- --load=%cata_plugin --

struct map {
    void load( int, bool );
    void save();
};
struct tinymap : private map {
    void load( int, bool, bool = false );
    using map::save;
};
struct smallmap : tinymap {};

void prepare( tinymap &body_map, smallmap *derived, map &local, int groups )
{
    for( int tile = 0; tile < groups; ++tile ) {
        body_map.load( tile, false );
        // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: tinymap load may read, generate, and update terrain [cata-tinymap-load]
        body_map.save();
    }
    derived->load( 1, true, true );
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: tinymap load may read, generate, and update terrain [cata-tinymap-load]
    local.load( 1, false );
}

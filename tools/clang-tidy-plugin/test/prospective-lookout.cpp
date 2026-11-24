// RUN: %check_clang_tidy %s cata-prospective-lookout %t -- --load=%cata_plugin --

namespace scout_observation {
struct site_reader {
    bool read_lookout( int origin, int target );
    bool read_watch_signal( int watch, int target );
};
} // namespace scout_observation

bool rank( scout_observation::site_reader &reader, int origin, int target )
{
    return reader.read_lookout( origin, target );
    // CHECK-MESSAGES: :[[@LINE-1]]:{{[0-9]+}}: warning: read_lookout may construct and prepare a remote map for each ray; use overmap terrain to rank prospective lookouts [cata-prospective-lookout]
}

bool observe( scout_observation::site_reader &reader, int watch, int target )
{
    return reader.read_watch_signal( watch, target );
}

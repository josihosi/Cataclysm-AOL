#include "semantic_surface.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <utility>

#if !defined(_WIN32)
#include <fcntl.h>
#endif

#include "debug.h"
#include "do_turn.h"
#include "filesystem.h"
#include "json.h"

namespace
{
thread_local semantic_surface_manager *active_manager = nullptr;

enum class native_record_format {
    semantic,
    diagnostic
};

// Fixed diagnostic tokens only: never print exception text or record contents.
struct native_journal_failure {
    const char *stage = "json_parse";
    const char *field = "none";
    int line = -1;
    int column = -1;
    int error_code = 0;
    const char *error_category = "none";
    std::ios::iostate stream_state = std::ios::goodbit;
};

void native_journal_json_location( const JsonError &error, native_journal_failure &failure )
{
    // JsonError seeks its input to EOF. Its fixed, inputless source header
    // retains the location; copying what() would also copy the failed record.
    if( std::sscanf( error.what(),
                     "Json error: file <unknown source file>, at line %d, character %d:",
                     &failure.line, &failure.column ) != 2 &&
        std::sscanf( error.what(),
                     "::error file=<unknown source file>,line=%d,col=%d::",
                     &failure.line, &failure.column ) != 2 ) {
        failure.line = failure.column = -1;
    }
}

// Keep bounded recent diagnostics plus the current input transaction.  A large
// action catalog is not truncated to meet a diagnostic budget.
class native_semantic_journal
{
    private:
        struct record {
            std::size_t sequence;
            std::string text;
        };

    public:
        native_semantic_journal( std::string path, std::string run ) :
            path( std::move( path ) ), run( std::move( run ) ) {}

        bool append( const std::string &event, native_record_format format,
                     native_journal_failure &failure, bool measure ) {
            std::istringstream input( event );
            TextJsonIn jsin( input );
            const TextJsonObject object = jsin.get_object();
            object.allow_omitted_members();
            const auto read_string = [&]( const char *field ) {
                failure.stage = "record_type";
                failure.field = field;
                return object.get_string( field, "" );
            };
            if( read_string( "run_id" ) != run ) {
                failure.stage = "record_run_mismatch";
                return false;
            }
            const std::string kind = read_string( "event" );
            const bool reduced_performance_interval = format == native_record_format::diagnostic &&
                    kind == "raid_performance_interval" &&
                    read_string( "retention_scope" ) == "max_simulation_row_only" &&
                    event.size() <= 512 * 1024;
            const bool performance_extrema_candidate = format == native_record_format::diagnostic &&
                    kind == "native_performance_extrema" && event.size() <= 64 * 1024 &&
                    read_string( "schema" ) == "caol-native-performance-extrema-v1";
            std::string extrema_instance;
            int extrema_pid = 0;
            int extrema_sequence = 0;
            if( performance_extrema_candidate ) {
                extrema_instance = read_string( "native_process_instance" );
                failure.stage = "record_type";
                failure.field = "native_pid";
                extrema_pid = object.get_int( "native_pid", 0 );
                failure.field = "snapshot_sequence";
                extrema_sequence = object.get_int( "snapshot_sequence", 0 );
            }
            const bool performance_extrema = performance_extrema_candidate && !extrema_instance.empty() &&
                    extrema_pid > 0 && extrema_sequence > performance_extrema_sequence &&
                    ( performance_extrema_instance.empty() ||
                      ( extrema_instance == performance_extrema_instance && extrema_pid == performance_extrema_pid ) );
            const bool slow_candidate = format == native_record_format::diagnostic &&
                    kind == "native_slow_turn_receipts" && event.size() <= 32768 &&
                    read_string( "schema" ) == "caol-native-slow-turn-receipts-v1";
            std::string slow_instance;
            int slow_pid = 0;
            int slow_sequence = 0;
            if( slow_candidate ) {
                slow_instance = read_string( "native_process_instance" );
                failure.stage = "record_type";
                failure.field = "native_pid";
                slow_pid = object.get_int( "native_pid", 0 );
                failure.field = "snapshot_sequence";
                slow_sequence = object.get_int( "snapshot_sequence", 0 );
            }
            const bool slow_receipts = slow_candidate && !slow_instance.empty() && slow_pid > 0 &&
                    slow_sequence > slow_receipts_sequence &&
                    ( slow_receipts_instance.empty() ||
                      ( slow_instance == slow_receipts_instance && slow_pid == slow_receipts_pid ) );
            // Read every field used by this semantic retention branch before
            // changing sequence, pins or trip state. A type refusal cannot
            // become publishable authority on a later compaction retry.
            std::string frame;
            std::string result;
            std::string trip;
            std::string state;
            std::string action;
            bool accepted = false;
            if( format == native_record_format::semantic ) {
                if( kind == "surface_descriptor" ) {
                    frame = read_string( "frame_id" );
                } else if( kind == "surface_receipt" ) {
                    result = read_string( "resulting_frame_id" );
                    failure.stage = "record_type";
                    failure.field = "accepted";
                    accepted = object.get_bool( "accepted", false );
                } else if( kind == "travel" ) {
                    trip = read_string( "travel_id" );
                    state = read_string( "state" );
                } else if( kind == "receipt" ) {
                    action = read_string( "action_id" );
                }
            }
            failure.stage = "retention";
            failure.field = "none";
            const std::string prefix = format == native_record_format::semantic ?
                                       "openclaw_harness_semantic_step: " : "";
            auto row = std::make_shared<const record>( record { ++sequence, prefix + event + '\n' } );
            // Keep at most one explicitly reduced interval through journal
            // compaction. It has diagnostic status and cannot become input
            // authority. A later reduced interval replaces it.
            if( reduced_performance_interval ) {
                pins["performance_reduced"] = row;
            }
            // One producer-owned bounded snapshot carries both extrema. The
            // pin survives ordinary journal compaction, never grants input.
            if( performance_extrema ) {
                performance_extrema_instance = extrema_instance;
                performance_extrema_pid = extrema_pid;
                performance_extrema_sequence = extrema_sequence;
                pins["performance_extrema"] = row;
            }
            if( slow_receipts ) {
                slow_receipts_instance = slow_instance;
                slow_receipts_pid = slow_pid;
                slow_receipts_sequence = slow_sequence;
                pins["performance_slow_receipts"] = row;
            }
            // Diagnostic callers share retention, never semantic framing or
            // current input pins, even if their event names resemble authority.
            if( format == native_record_format::semantic ) {
                if( kind == "surface_descriptor" ) {
                    current_frame = frame;
                    pins["surface"] = row;
                    if( current_frame == accepted_result ) {
                        pins["accepted_successor"] = row;
                    }
                    if( current_frame == receipt_result ) {
                        pins["receipt_successor"] = row;
                    }
                } else if( kind == "surface_receipt" ) {
                    pins["receipt"] = row;
                    // Cached receipt replay can occur after a newer redraw. Its
                    // immutable result still names the previous successor, which
                    // may now exist only in these pins, not the recent window.
                    if( receipt_result != result ) {
                        pins.erase( "receipt_successor" );
                    }
                    receipt_result = result;
                    if( accepted ) {
                        pins["accepted_receipt"] = row;
                        if( accepted_result != result ) {
                            pins.erase( "accepted_successor" );
                        }
                        accepted_result = result;
                        if( result == current_frame &&
                            pins.count( "surface" ) ) {
                            pins["accepted_successor"] = pins["surface"];
                        }
                    }
                    if( result == current_frame &&
                        pins.count( "surface" ) ) {
                        pins["receipt_successor"] = pins["surface"];
                    }
                } else if( kind == "travel" ) {
                    if( current_trip != trip ) {
                        pins.erase( "travel_active" );
                        pins.erase( "travel_progress" );
                        pins.erase( "travel_hostile_boundary" );
                        pins.erase( "travel_refusal" );
                        current_trip = trip;
                    }
                    // The existing travel consumer requires the start and latest
                    // progress of this trip alongside its terminal receipt. Earlier
                    // adverse boundaries remain evidence even after travel resumes.
                    if( state == "active" || state == "progress" ) {
                        pins["travel_" + state] = row;
                    }
                    if( state == "hostile_boundary" && !pins.count( "travel_hostile_boundary" ) ) {
                        pins["travel_hostile_boundary"] = row;
                    }
                    if( ( state == "blocked" || state == "interrupted" ) && !pins.count( "travel_refusal" ) ) {
                        pins["travel_refusal"] = row;
                    }
                    pins["travel"] = row;
                } else if( kind == "frame" || kind == "native_save_completion" ) {
                    pins[kind] = row;
                } else if( kind == "receipt" ) {
                    pins[action.compare( 0, 5, "wait." ) == 0 ? "duration_receipt" :
                                         "movement_receipt"] = row;
                }
            }
            recent.push_back( row );
            recent_bytes += row->text.size();
            while( recent_bytes > diagnostic_bytes || recent.size() > 256 ) {
                recent_bytes -= recent.front()->text.size();
                recent.pop_front();
                ++discarded_recent_records;
            }

            std::size_t pinned_bytes = 0;
            for( const auto &pin : pins ) {
                pinned_bytes += pin.second->text.size();
            }
            // Compact before another write can grow past the live authority
            // plus the diagnostic allowance. Initial publication also replaces
            // any unrelated previous process's file with a fresh generation.
            if( !published || disk_bytes + row->text.size() > diagnostic_bytes + pinned_bytes + 512 ) {
                return compact( failure, measure );
            }
            failure.field = "none";
            failure.stage = "append_open";
            errno = 0;
            std::ofstream output( path, std::ios::binary | std::ios::app );
            if( !output.is_open() ) {
                failure.error_code = errno;
                failure.error_category = errno ? "generic" : "unavailable";
                failure.stream_state = output.rdstate();
                published = false;
                return false;
            }
            failure.stage = "append_write";
            errno = 0;
            output << row->text;
            if( !output ) {
                failure.error_code = errno;
                failure.error_category = errno ? "generic" : "unavailable";
                failure.stream_state = output.rdstate();
                published = false;
                return false;
            }
            failure.stage = "append_close";
            errno = 0;
            output.close();
            if( !output ) {
                failure.error_code = errno;
                failure.error_category = errno ? "generic" : "unavailable";
                failure.stream_state = output.rdstate();
                // A partial append cannot be followed by another JSON row.
                // Rebuild the remembered complete records on the next write.
                published = false;
                return false;
            }
            disk_bytes += row->text.size();
            return true;
        }

    private:
        bool compact( native_journal_failure &failure, bool measure ) {
            raid_perf_scope diagnostic_scope( raid_perf_stage::native_journal_compact, measure &&
                                              raid_perf_journal_active() );
            std::vector<std::shared_ptr<const record>> rows;
            for( const auto &pin : pins ) {
                if( std::find( recent.begin(), recent.end(), pin.second ) == recent.end() &&
                    std::find( rows.begin(), rows.end(), pin.second ) == rows.end() ) {
                    rows.push_back( pin.second );
                }
            }
            rows.insert( rows.end(), recent.begin(), recent.end() );
            std::sort( rows.begin(), rows.end(), []( const auto & left, const auto & right ) {
                return left->sequence < right->sequence;
            } );
            const std::string temporary = path + ".next";
            failure.field = "none";
            failure.stage = "temporary_open";
            errno = 0;
            std::ofstream output( temporary, std::ios::binary | std::ios::trunc );
            if( !output.is_open() ) {
                failure.error_code = errno;
                failure.error_category = errno ? "generic" : "unavailable";
                failure.stream_state = output.rdstate();
                published = false;
                return false;
            }
            failure.stage = "temporary_write";
            errno = 0;
            // This marker makes replacement observable on filesystems whose
            // inode identities are unavailable. It never grants native input.
            const auto instance = std::chrono::system_clock::now().time_since_epoch().count();
            output << "openclaw_harness_semantic_step: ";
            JsonOut header( output );
            header.start_object();
            header.member( "event", "trace_generation" );
            header.member( "run_id", run );
            header.member( "generation", std::to_string( instance ) + '-' +
                           std::to_string( ++generation ) );
            header.member( "recent_records_discarded", discarded_recent_records );
            header.member( "diagnostic_byte_limit", diagnostic_bytes );
            header.end_object();
            output << '\n';
            for( const auto &row : rows ) {
                output << row->text;
            }
            const auto bytes = output.tellp();
            failure.error_code = errno;
            failure.error_category = errno ? "generic" : "unavailable";
            failure.stream_state = output.rdstate();
            bool complete = bytes >= 0 && bool( output );
            if( complete ) {
                failure.stage = "temporary_close";
                errno = 0;
                output.close();
                failure.error_code = errno;
                failure.error_category = errno ? "generic" : "unavailable";
                failure.stream_state = output.rdstate();
                complete = bool( output );
            } else {
                output.close();
            }
            if( complete ) {
                failure.stage = "rename";
                std::error_code error;
                complete = rename_file( std::filesystem::u8path( temporary ),
                                        std::filesystem::u8path( path ), error );
                failure.error_code = error.value();
                failure.error_category = error ? error.category().name() : "none";
            }
            if( !complete ) {
                std::error_code cleanup_error;
                std::filesystem::remove( temporary, cleanup_error );
                published = false;
                return false;
            }
            disk_bytes = static_cast<std::size_t>( bytes );
            published = true;
            return true;
        }

        static constexpr std::size_t diagnostic_bytes = 4 * 1024 * 1024;
        std::string path;
        std::string run;
        std::string current_frame;
        std::string performance_extrema_instance;
        int performance_extrema_pid = 0;
        int performance_extrema_sequence = 0;
        std::string slow_receipts_instance;
        int slow_receipts_pid = 0;
        int slow_receipts_sequence = 0;
        std::string accepted_result;
        std::string receipt_result;
        std::string current_trip;
        std::map<std::string, std::shared_ptr<const record>> pins;
        std::deque<std::shared_ptr<const record>> recent;
        std::size_t recent_bytes = 0;
        std::size_t disk_bytes = 0;
        std::size_t generation = 0;
        std::size_t sequence = 0;
        std::size_t discarded_recent_records = 0;
        bool published = false;
};

bool same_actions( const std::vector<semantic_action_descriptor> &left,
                   const std::vector<semantic_action_descriptor> &right )
{
    if( left.size() != right.size() ) {
        return false;
    }
    for( std::size_t index = 0; index < left.size(); ++index ) {
        if( left[index].id != right[index].id || left[index].stable_id != right[index].stable_id ||
            left[index].label != right[index].label || left[index].enabled != right[index].enabled ) {
            return false;
        }
    }
    return true;
}
bool write_native_event( const std::string &event, native_record_format format, bool measure )
{
    const char *run = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const char *bound = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" );
    const char *path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" );
    if( !run || !bound || !path || !*run || !*path || std::string( run ) != bound ) {
        return false;
    }
    raid_perf_scope diagnostic_scope( raid_perf_stage::native_journal_publish, measure &&
                                      raid_perf_journal_active() );
    static std::mutex mutex;
    const std::lock_guard<std::mutex> lock( mutex );
    static std::string identity;
    static std::unique_ptr<native_semantic_journal> journal;
    static bool failure_reported = false;
    const std::string next_identity = std::string( run ) + '\n' + path;
    if( identity != next_identity ) {
        identity = next_identity;
        journal = std::make_unique<native_semantic_journal>( path, run );
        failure_reported = false;
    }
    bool written = false;
    native_journal_failure failure;
    try {
        written = journal->append( event, format, failure, measure );
    } catch( const JsonError &error ) {
        native_journal_json_location( error, failure );
    } catch( const std::exception & ) {
        // A failed publication is not authority. Keep the failure diagnostic
        // small; never mirror the discarded action catalog into debug.log.
    }
    if( !written && !failure_reported ) {
        DebugLog( D_ERROR, DC_ALL ) << "Native semantic journal publication failed for run " << run
                                    << " stage=" << failure.stage << " field=" << failure.field
                                    << " line=" << failure.line << " column=" << failure.column
                                    << " error_code=" << failure.error_code
                                    << " error_category=" << failure.error_category
                                    << " stream_state=" << static_cast<unsigned>( failure.stream_state );
    } else if( written && failure_reported ) {
        DebugLog( D_INFO, DC_ALL ) << "Native semantic journal publication resumed for run " << run;
    }
    failure_reported = !written;
    return written;
}

} // namespace

void prevent_semantic_wake_descriptor_inheritance()
{
#if !defined(_WIN32)
    // pass_fds makes these inheritable for launcher -> game exec only. Keep
    // both endpoints alive in the game, not in later LLM/other exec children.
    for( const char *key : {
             "OPENCLAW_HARNESS_SEMANTIC_WAKE_READ_FD",
             "OPENCLAW_HARNESS_SEMANTIC_WAKE_KEEPER_FD"
         } ) {
        const char *descriptor = std::getenv( key );
        if( descriptor == nullptr ) {
            continue;
        }
        char *end = nullptr;
        errno = 0;
        const long parsed = std::strtol( descriptor, &end, 10 );
        if( errno != 0 || end == descriptor || *end != '\0' || parsed < 0 ||
            parsed > std::numeric_limits<int>::max() ) {
            throw std::runtime_error( "invalid inherited semantic wake descriptor" );
        }
        const int fd = static_cast<int>( parsed );
        const int flags = fcntl( fd, F_GETFD );
        if( flags < 0 || fcntl( fd, F_SETFD, flags | FD_CLOEXEC ) != 0 ) {
            throw std::runtime_error( "cannot prevent semantic wake descriptor inheritance" );
        }
    }
#endif
}

bool write_native_semantic_event( const std::string &event )
{
    return write_native_event( event, native_record_format::semantic, true );
}

bool write_native_diagnostic_event( const std::string &event, bool measure )
{
    return write_native_event( event, native_record_format::diagnostic, measure );
}

semantic_surface_manager::semantic_surface_manager( std::string run_id ) : run_id_( std::move(
                run_id ) )
{
}

const std::string &semantic_surface_manager::run_id() const
{
    return run_id_;
}

std::string semantic_surface_manager::push( const std::string &kind, const std::string &breadcrumb,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions,
        semantic_action_consumer consumer )
{
    bool recreating_withheld_owner = false;
    for( auto owner = withheld_parent_surface_ids_.begin();
         owner != withheld_parent_surface_ids_.end(); ) {
        const bool owner_is_gone = std::none_of( stack_.begin(), stack_.end(),
        [&owner]( const surface_state & surface ) {
            return surface.descriptor.surface_id == *owner;
        } );
        if( owner_is_gone ) {
            owner = withheld_parent_surface_ids_.erase( owner );
            recreating_withheld_owner = true;
        } else {
            ++owner;
        }
    }
    semantic_surface_descriptor descriptor;
    descriptor.run_id = run_id_;
    descriptor.surface_id = new_surface_id();
    descriptor.kind = kind;
    descriptor.payload = std::move( payload );
    descriptor.valid_actions = std::move( valid_actions );
    for( const surface_state &parent : stack_ ) {
        descriptor.breadcrumbs.push_back( parent.descriptor.breadcrumbs.back() );
    }
    descriptor.breadcrumbs.push_back( breadcrumb );
    const bool continuing_activity = kind == "activity_wait" && activity_poll_ &&
                                     descriptor.payload.count( "activity_generation" ) != 0 &&
                                     descriptor.payload == activity_poll_->payload &&
                                     descriptor.breadcrumbs == activity_poll_->breadcrumbs &&
                                     same_actions( descriptor.valid_actions, activity_poll_->valid_actions );
    if( continuing_activity ) {
        descriptor.surface_id = activity_poll_->surface_id;
        descriptor.frame_id = activity_poll_->frame_id;
    }
    activity_poll_.reset();
    stack_.push_back( { std::move( descriptor ), std::move( consumer ) } );
    if( recreating_withheld_owner ) {
        suppress_parent_republish_ = false;
    }
    if( continuing_activity ) {
        top_ = stack_.back().descriptor;
        if( descriptor_observer_ ) {
            descriptor_observer_( *top_ );
        }
    } else {
        republish_top();
    }
    return stack_.back().descriptor.surface_id;
}

bool semantic_surface_manager::publish( const std::string &surface_id,
                                        std::map<std::string, std::string> payload,
                                        std::vector<semantic_action_descriptor> valid_actions )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    if( top_ && stack_.back().descriptor.payload == payload &&
        same_actions( stack_.back().descriptor.valid_actions, valid_actions ) ) {
        return true;
    }
    stack_.back().descriptor.payload = std::move( payload );
    stack_.back().descriptor.valid_actions = std::move( valid_actions );
    const bool withheld_owner_is_gone = std::any_of( withheld_parent_surface_ids_.begin(),
    withheld_parent_surface_ids_.end(), [this]( const std::string & owner ) {
        return std::none_of( stack_.begin(), stack_.end(), [&owner]( const surface_state & surface ) {
            return surface.descriptor.surface_id == owner;
        } );
    } );
    if( withheld_parent_surface_ids_.count( surface_id ) != 0 || withheld_owner_is_gone ) {
        return true;
    }
    republish_top();
    return true;
}

bool semantic_surface_manager::pop( const std::string &surface_id )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    if( stack_.size() == 1 && top_ && top_->kind == "activity_wait" &&
        top_->payload.count( "activity_generation" ) != 0 &&
        withheld_parent_surface_ids_.count( surface_id ) == 0 ) {
        activity_poll_ = *top_;
    } else {
        activity_poll_.reset();
    }
    const bool withheld_parent_is_next = stack_.size() > 1 &&
                                         withheld_parent_surface_ids_.count(
                                                 stack_[stack_.size() - 2].descriptor.surface_id ) != 0;
    const bool popping_withheld_parent = withheld_parent_surface_ids_.count( surface_id ) != 0;
    if( withheld_parent_is_next || popping_withheld_parent ) {
        stack_.pop_back();
        // The native item-use path is still unwinding.  Neither the stale
        // parent nor the World scope owns a request until the native loop
        // recreates the parent, so leave the manager without an active
        // descriptor for that private transition.
        top_.reset();
        return true;
    }
    if( suppress_parent_republish_ ) {
        stack_.pop_back();
        suppress_parent_republish_ = false;
        if( stack_.empty() ) {
            top_.reset();
        } else {
            top_ = stack_.back().descriptor;
        }
        return true;
    }
    stack_.pop_back();
    republish_top();
    return true;
}

bool semantic_surface_manager::is_top( const std::string &surface_id ) const
{
    return !stack_.empty() && stack_.back().descriptor.surface_id == surface_id;
}

bool semantic_surface_manager::withhold_parent_authority_until_recreated(
    const std::string &surface_id )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    // A committed dialogue can open an item menu that also retires while
    // its own child runs. Track both native owners independently.
    return withheld_parent_surface_ids_.insert( surface_id ).second;
}

const std::optional<semantic_surface_descriptor> &semantic_surface_manager::top() const
{
    return top_;
}

std::vector<semantic_surface_descriptor> semantic_surface_manager::stack() const
{
    std::vector<semantic_surface_descriptor> result;
    result.reserve( stack_.size() );
    for( const surface_state &surface : stack_ ) {
        result.push_back( surface.descriptor );
    }
    return result;
}

bool semantic_surface_manager::submit_request( semantic_action_request request )
{
    if( request.request_id.empty() ) {
        return false;
    }
    const auto completed = completed_requests_.find( request.request_id );
    if( completed != completed_requests_.end() ) {
        if( receipt_observer_ ) {
            receipt_observer_( completed->second );
        }
        return false;
    }
    for( const semantic_action_request &pending : pending_requests_ ) {
        if( pending.request_id == request.request_id ) {
            return false;
        }
    }
    pending_requests_.push_back( std::move( request ) );
    return true;
}

bool semantic_surface_manager::poll_request_transport()
{
    const char *const path_value = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    if( path_value == nullptr || path_value[0] == '\0' ) {
        return false;
    }

    std::ifstream stream( path_value, std::ios::binary );
    if( !stream ) {
        return false;
    }
    const std::size_t offset_before = request_transport_offset_;
    stream.seekg( 0, std::ios::end );
    const std::streamoff end = stream.tellg();
    if( end < 0 ) {
        return false;
    }
    if( static_cast<std::size_t>( end ) < request_transport_offset_ ) {
        request_transport_offset_ = 0;
    }
    stream.seekg( static_cast<std::streamoff>( request_transport_offset_ ) );

    bool received = false;
    std::string line;
    while( std::getline( stream, line ) ) {
        request_transport_offset_ += line.size() + 1;
        if( line.empty() ) {
            continue;
        }
        try {
            std::istringstream json_stream( line );
            TextJsonIn jsin( json_stream );
            TextJsonObject record = jsin.get_object();
            record.allow_omitted_members();
            semantic_action_request request;
            request.run_id = record.get_string( "run_id", "" );
            request.surface_id = record.get_string( "surface_id", "" );
            request.frame_id = record.get_string( "frame_id", "" );
            request.request_id = record.get_string( "request_id", "" );
            request.action_id = record.get_string( "action_id", "" );
            if( record.has_string( "stable_id" ) ) {
                request.stable_id = record.get_string( "stable_id" );
            }
            if( record.has_member( "parameters" ) ) {
                TextJsonObject parameters = record.get_object( "parameters" );
                for( const TextJsonMember &member : parameters ) {
                    request.parameters.emplace( member.name(), member.get_string() );
                }
            }
            // A duplicate can replay its recorded receipt immediately and
            // leaves no pending request.  It still has to wake the input
            // loop so a curses getch() cannot hide that receipt behind a
            // second blocking read.
            received = true;
            const std::string request_id = request.request_id;
            const bool queued = submit_request( std::move( request ) );
            observe_transport( { "jsonl_record", request_id, offset_before,
                                 request_transport_offset_, static_cast<std::size_t>( end ), queued,
                                 transport_wake_pending_ } );
        } catch( const std::exception & ) {
            // A malformed transport record is inert.  It cannot be repaired
            // into a request or produce a gameplay state change.
        }
    }
    if( received ) {
        observe_transport( { "poll_finished", "", offset_before, request_transport_offset_,
                             static_cast<std::size_t>( end ), !pending_requests_.empty(),
                             transport_wake_pending_ } );
    }
    return received;
}

bool semantic_surface_manager::has_pending_request() const
{
    return !pending_requests_.empty();
}

bool semantic_surface_manager::queue_native_intent( const semantic_action_request &request,
        const std::string &intent )
{
    if( intent.empty() || native_intent_ || request.run_id != run_id_ || request.request_id.empty() ) {
        return false;
    }
    native_intent_ = std::make_pair( request.request_id, intent );
    return true;
}

bool semantic_surface_manager::take_native_intent( const std::string &intent )
{
    if( !native_intent_ || native_intent_->second != intent ) {
        return false;
    }
    native_intent_.reset();
    return true;
}

bool semantic_surface_manager::consume_top_request()
{
    if( pending_requests_.empty() || stack_.empty() || !top_ ) {
        return false;
    }

    semantic_action_request request = std::move( pending_requests_.front() );
    pending_requests_.erase( pending_requests_.begin() );
    semantic_action_receipt receipt;
    bool receipt_deferred = false;
    receipt.request_id = request.request_id;
    receipt.requested_run_id = request.run_id;
    receipt.requested_surface_id = request.surface_id;
    receipt.requested_frame_id = request.frame_id;
    receipt.action_id = request.action_id;
    receipt.consuming_surface_id = top_->surface_id;
    receipt.consuming_frame_id = top_->frame_id;

    if( request.run_id != run_id_ ) {
        receipt.rejection_reason = "wrong_run";
    } else if( request.surface_id != top_->surface_id ) {
        receipt.rejection_reason = "wrong_surface";
    } else if( request.frame_id != top_->frame_id ) {
        receipt.rejection_reason = "stale_frame";
    } else {
        const auto action = std::find_if( top_->valid_actions.begin(), top_->valid_actions.end(),
        [&request]( const semantic_action_descriptor & candidate ) {
            return candidate.id == request.action_id && candidate.enabled;
        } );
        if( action == top_->valid_actions.end() ) {
            receipt.rejection_reason = "unadvertised_action";
        } else if( !stack_.back().consumer ) {
            receipt.rejection_reason = "no_native_binding";
        } else {
            const semantic_action_dispatch_result result = stack_.back().consumer( request );
            receipt.accepted = result.accepted;
            receipt.rejection_reason = result.rejection_reason;
            receipt.resulting_frame_id = result.resulting_frame_id;
            receipt.outcome = result.outcome;
            if( receipt.accepted && receipt.resulting_frame_id.empty() &&
                result.defer_receipt_to_successor ) {
                // The consuming callback has selected a native action, but its
                // actual successor is published by the native action path.
                // Receipt that next top frame, never a temporary parent redraw.
                pending_accepted_receipt_ = receipt;
                suppress_parent_republish_ = result.await_child_successor;
                if( result.await_child_successor ) {
                    pending_receipt_child_source_surface_id_ = receipt.consuming_surface_id;
                } else {
                    pending_receipt_child_source_surface_id_.reset();
                }
                receipt_deferred = true;
            }
        }
    }
    completed_requests_.emplace( receipt.request_id, receipt );
    observe_transport( { "request_consumed", receipt.request_id, request_transport_offset_,
                         request_transport_offset_, request_transport_offset_, false,
                         transport_wake_pending_ } );
    if( receipt_observer_ && !receipt_deferred ) {
        receipt_observer_( receipt );
    }
    return receipt.accepted;
}

bool semantic_surface_manager::take_transport_wake()
{
    const bool result = transport_wake_pending_;
    transport_wake_pending_ = false;
    return result;
}

void semantic_surface_manager::mark_transport_wake()
{
    transport_wake_pending_ = true;
    observe_transport( { "wake_marked", "", request_transport_offset_, request_transport_offset_,
                         request_transport_offset_, !pending_requests_.empty(), true } );
}

semantic_action_receipt semantic_surface_manager::reject_request(
    const semantic_action_request &request )
{
    const auto previous = completed_requests_.find( request.request_id );
    if( previous != completed_requests_.end() ) {
        return previous->second;
    }
    semantic_action_receipt receipt;
    receipt.request_id = request.request_id;
    receipt.requested_run_id = request.run_id;
    receipt.requested_surface_id = request.surface_id;
    receipt.requested_frame_id = request.frame_id;
    receipt.action_id = request.action_id;
    if( !top_ ) {
        receipt.rejection_reason = "no_active_surface";
    } else {
        receipt.consuming_surface_id = top_->surface_id;
        receipt.consuming_frame_id = top_->frame_id;
        if( request.run_id != run_id_ ) {
            receipt.rejection_reason = "wrong_run";
        } else if( request.surface_id != top_->surface_id ) {
            receipt.rejection_reason = "wrong_surface";
        } else if( request.frame_id != top_->frame_id ) {
            receipt.rejection_reason = "stale_frame";
        } else {
            receipt.rejection_reason = "no_native_binding";
        }
    }
    if( receipt_observer_ ) {
        receipt_observer_( receipt );
    }
    completed_requests_.emplace( receipt.request_id, receipt );
    return receipt;
}

void semantic_surface_manager::set_descriptor_observer(
    std::function<void( const semantic_surface_descriptor & )> observer )
{
    descriptor_observer_ = std::move( observer );
}

void semantic_surface_manager::set_receipt_observer(
    std::function<void( const semantic_action_receipt & )> observer )
{
    receipt_observer_ = std::move( observer );
}

void semantic_surface_manager::set_transport_observer(
    std::function<void( const semantic_request_transport_event & )> observer )
{
    transport_observer_ = std::move( observer );
}

void semantic_surface_manager::observe_transport( semantic_request_transport_event event ) const
{
    if( transport_observer_ ) {
        transport_observer_( event );
    }
}

std::string semantic_surface_manager::new_surface_id()
{
    return run_id_ + ":surface:" + std::to_string( ++next_surface_id_ );
}

std::string semantic_surface_manager::new_frame_id()
{
    return run_id_ + ":frame:" + std::to_string( ++next_frame_id_ );
}

void semantic_surface_manager::republish_top()
{
    if( stack_.empty() ) {
        top_.reset();
        // A deferred receipt normally belongs to the native successor frame.
        // A terminal native action has no successor, however: its owning
        // scope is simply popped as the UI exits.  Do not strand that accepted
        // request merely because there is no frame to attach to.
        if( pending_accepted_receipt_ && !pending_receipt_child_source_surface_id_ &&
            withheld_parent_surface_ids_.empty() ) {
            completed_requests_[pending_accepted_receipt_->request_id] = *pending_accepted_receipt_;
            if( receipt_observer_ ) {
                receipt_observer_( *pending_accepted_receipt_ );
            }
            pending_accepted_receipt_.reset();
            pending_receipt_child_source_surface_id_.reset();
        }
        return;
    }
    stack_.back().descriptor.frame_id = new_frame_id();
    top_ = stack_.back().descriptor;
    if( descriptor_observer_ ) {
        descriptor_observer_( *top_ );
    }
    if( pending_accepted_receipt_ ) {
        if( pending_receipt_child_source_surface_id_ &&
            *pending_receipt_child_source_surface_id_ == top_->surface_id ) {
            return;
        }
        pending_accepted_receipt_->resulting_frame_id = top_->frame_id;
        completed_requests_[pending_accepted_receipt_->request_id] = *pending_accepted_receipt_;
        if( receipt_observer_ ) {
            receipt_observer_( *pending_accepted_receipt_ );
        }
        pending_accepted_receipt_.reset();
        pending_receipt_child_source_surface_id_.reset();
    }
}

semantic_surface_manager_session::semantic_surface_manager_session(
    semantic_surface_manager &manager ) : previous_( active_manager )
{
    active_manager = &manager;
}

semantic_surface_manager_session::~semantic_surface_manager_session()
{
    active_manager = previous_;
}

semantic_surface_manager *active_semantic_surface_manager()
{
    return active_manager;
}

bool poll_active_semantic_surface_request()
{
    if( active_manager == nullptr ) {
        return false;
    }
    const bool received_transport = active_manager->poll_request_transport();
    // A backend can drain the FIFO byte in an inner input context just as a
    // JSONL record becomes visible.  The request is then queued, but its
    // owning outer context has no wake left to return from its blocking read.
    // Pending work is itself sufficient reason to wake that owner; this is
    // idempotent and leaves receipt dispatch at the native consumer boundary.
    if( !received_transport && !active_manager->has_pending_request() ) {
        return false;
    }
    // A duplicate is already completed and therefore has no pending request,
    // but consuming its transport record replayed the exact receipt.  Wake
    // the input loop in either case so terminal input cannot block that
    // replay behind a physical read.
    active_manager->mark_transport_wake();
    return true;
}

bool take_active_semantic_surface_wake()
{
    return active_manager != nullptr && active_manager->take_transport_wake();
}

semantic_surface_scope::semantic_surface_scope( semantic_surface_manager &manager,
        const std::string &kind, const std::string &breadcrumb,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions,
        semantic_action_consumer consumer, bool interrupt_pending_action ) :
    manager_( &manager )
{
    if( interrupt_pending_action ) {
        interrupted_action_.emplace();
        auto &state = *interrupted_action_;
        // Finish allocating the snapshot before moving any live action state.
        state.withheld_parent_surface_ids = manager.withheld_parent_surface_ids_;
        state.suppress_parent_republish = manager.suppress_parent_republish_;
        state.stack_depth = manager.stack_.size();
        state.receipt.swap( manager.pending_accepted_receipt_ );
        state.child_source_surface_id.swap( manager.pending_receipt_child_source_surface_id_ );
        manager.suppress_parent_republish_ = false;
    }
    try {
        surface_id_ = manager.push( kind, breadcrumb, std::move( payload ),
                                    std::move( valid_actions ), std::move( consumer ) );
    } catch( ... ) {
        if( interrupted_action_ ) {
            abort_interruption();
        }
        throw;
    }
}

semantic_surface_scope::~semantic_surface_scope()
{
    release();
}

semantic_surface_scope::semantic_surface_scope( semantic_surface_scope &&other ) noexcept :
    manager_( other.manager_ ), surface_id_( std::move( other.surface_id_ ) ),
    interrupted_action_( std::move( other.interrupted_action_ ) )
{
    other.manager_ = nullptr;
}

semantic_surface_scope &semantic_surface_scope::operator=( semantic_surface_scope &&other ) noexcept
{
    if( this != &other ) {
        release();
        manager_ = other.manager_;
        surface_id_ = std::move( other.surface_id_ );
        interrupted_action_ = std::move( other.interrupted_action_ );
        other.manager_ = nullptr;
    }
    return *this;
}

const std::string &semantic_surface_scope::surface_id() const
{
    return surface_id_;
}

bool semantic_surface_scope::close()
{
    if( !manager_ ) {
        return true;
    }
    if( !manager_->is_top( surface_id_ ) ) {
        return false;
    }
    if( !interrupted_action_ ) {
        release();
        return true;
    }
    try {
        // Retain the snapshot until publication succeeds: pop can remove its
        // frame and then throw from the restored parent's descriptor observer.
        manager_->withheld_parent_surface_ids_ = interrupted_action_->withheld_parent_surface_ids;
        manager_->suppress_parent_republish_ = interrupted_action_->suppress_parent_republish;
        manager_->pop( surface_id_ );
    } catch( ... ) {
        abort_interruption();
        throw;
    }
    manager_->pending_accepted_receipt_.swap( interrupted_action_->receipt );
    manager_->pending_receipt_child_source_surface_id_.swap(
        interrupted_action_->child_source_surface_id );
    interrupted_action_.reset();
    manager_ = nullptr;
    return true;
}

bool semantic_surface_scope::publish( std::map<std::string, std::string> payload,
                                      std::vector<semantic_action_descriptor> valid_actions )
{
    return manager_ &&
           manager_->publish( surface_id_, std::move( payload ), std::move( valid_actions ) );
}

bool semantic_surface_scope::consume_request()
{
    return manager_ && manager_->is_top( surface_id_ ) && manager_->consume_top_request();
}

void semantic_surface_scope::release()
{
    if( interrupted_action_ ) {
        // No external observers in a destructor, including exception unwind.
        abort_interruption();
        return;
    }
    if( manager_ ) {
        manager_->pop( surface_id_ );
        manager_ = nullptr;
    }
}

void semantic_surface_scope::abort_interruption() noexcept
{
    if( manager_ && interrupted_action_ ) {
        auto &state = *interrupted_action_;
        // push may have installed the diagnostic before an observer threw;
        // pop may already have removed it. Neither path may leave an orphan.
        while( manager_->stack_.size() > state.stack_depth ) {
            manager_->stack_.pop_back();
        }
        // A failed/abandoned publication is not input authority. The existing
        // native owner can explicitly publish again, even with unchanged facts.
        manager_->top_.reset();
        manager_->pending_accepted_receipt_.swap( state.receipt );
        manager_->pending_receipt_child_source_surface_id_.swap( state.child_source_surface_id );
        manager_->withheld_parent_surface_ids_.swap( state.withheld_parent_surface_ids );
        manager_->suppress_parent_republish_ = state.suppress_parent_republish;
        manager_ = nullptr;
        interrupted_action_.reset();
    }
}

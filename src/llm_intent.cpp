#include "llm_intent.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "debug.h"
#include "json.h"
#include "messages.h"
#include "npc.h"
#include "options.h"
#include "path_info.h"
#include "string_formatter.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
struct llm_intent_request {
    std::string request_id;
    std::string npc_name;
    std::string prompt;
    int max_tokens = 0;
};

struct llm_intent_response {
    std::string request_id;
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
    bool force_npu = false;

    bool operator==( const runner_config &other ) const {
        return python_path == other.python_path &&
               runner_path == other.runner_path &&
               model_dir == other.model_dir &&
               device == other.device &&
               max_tokens == other.max_tokens &&
               force_npu == other.force_npu;
    }

    bool operator!=( const runner_config &other ) const {
        return !( *this == other );
    }
};

std::string build_prompt( const std::string &npc_name, const std::string &player_utterance )
{
    return string_format(
               "You are a game NPC intent engine. Return ONLY strict JSON.\n"
               "Schema: { \"intent\": \"...\", \"args\": { ... }, \"ttl_turns\": 120, \"npc_say\": \"...\" }\n"
               "Allowed intents: guard_area, move_to, follow_player, clear_overrides, idle.\n"
               "If unsure, respond with {\"intent\":\"idle\",\"args\":{},\"ttl_turns\":60,\"npc_say\":\"\"}.\n"
               "Player said: \"%s\"\n"
               "NPC: \"%s\"\n",
               player_utterance, npc_name );
}

std::string request_to_json( const llm_intent_request &request )
{
    std::ostringstream out;
    JsonOut jsout( out );
    jsout.start_object();
    jsout.member( "request_id", request.request_id );
    jsout.member( "prompt", request.prompt );
    jsout.member( "max_tokens", request.max_tokens );
    jsout.end_object();
    return out.str();
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
    response.npc_name = request.npc_name;
    try {
        std::istringstream in( line );
        TextJsonIn jsin( in );
        TextJsonObject obj = jsin.get_object();
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
    static constexpr int default_max_tokens = 512;
    runner_config cfg;
    cfg.python_path = get_option<std::string>( "LLM_INTENT_PYTHON" );
    cfg.runner_path = get_option<std::string>( "LLM_INTENT_RUNNER" );
    cfg.model_dir = get_option<std::string>( "LLM_INTENT_MODEL_DIR" );
    cfg.device = get_option<std::string>( "LLM_INTENT_DEVICE" );
    cfg.max_tokens = default_max_tokens;
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
            return read_response_for_request( request, response_line, timeout, error );
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
        runner_config active_config;
        HANDLE child_process = nullptr;
        HANDLE child_thread = nullptr;
        HANDLE stdin_write = nullptr;
        HANDLE stdout_read = nullptr;
        std::string stdout_buffer;

        bool start( const runner_config &config, std::string &error ) {
            if( config.python_path.empty() || config.runner_path.empty() || config.model_dir.empty() ) {
                error = "LLM runner configuration is incomplete.";
                return false;
            }

            std::filesystem::path python_path = resolve_path( config.python_path );
            std::filesystem::path runner_path = resolve_path( config.runner_path );
            std::filesystem::path model_dir = resolve_path( config.model_dir );

            std::vector<std::string> args;
            args.push_back( python_path.string() );
            args.push_back( runner_path.string() );
            args.push_back( "--model-dir" );
            args.push_back( model_dir.string() );
            args.push_back( "--device" );
            args.push_back( config.device.empty() ? "NPU" : config.device );
            args.push_back( "--max-tokens" );
            args.push_back( std::to_string( config.max_tokens ) );
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

        void enqueue_request( const npc &listener, const std::string &player_utterance ) {
            if( !get_option<bool>( "LLM_INTENT_ENABLE" ) ) {
                return;
            }
            static constexpr int default_max_tokens = 512;
            llm_intent_request req;
            req.request_id = next_request_id();
            req.npc_name = listener.get_name();
            req.prompt = build_prompt( req.npc_name, player_utterance );
            req.max_tokens = default_max_tokens;
            {
                std::lock_guard<std::mutex> lock( mutex );
                request_queue.push( std::move( req ) );
            }
            ensure_worker();
            cv.notify_one();
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
                if( debug_log ) {
                    if( resp.ok ) {
                        add_msg( "LLM intent response for %s: %s", resp.npc_name, resp.text );
                    } else {
                        add_msg( "LLM intent failed for %s: %s", resp.npc_name, resp.error );
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
void enqueue_request( const npc &listener, const std::string &player_utterance )
{
    get_manager().enqueue_request( listener, player_utterance );
}

void process_responses()
{
    get_manager().process_responses();
}
} // namespace llm_intent

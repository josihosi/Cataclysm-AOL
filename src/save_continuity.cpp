#include "save_continuity.h"

#include <array>
#include <fstream>
#include <exception>
#include <map>
#include <memory>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <zstd/common/xxhash.h>

#include "catacharset.h"
#include "debug.h"
#include "cata_scope_helpers.h"
#include "filesystem.h"
#include "json.h"
#include "json_loader.h"
#include "output.h"
#include "path_info.h"
#include "translations.h"
#include "zzip.h"

namespace save_continuity
{
namespace
{
namespace fs = std::filesystem;

fs::path normalized( const fs::path &input )
{
    fs::path result = fs::absolute( input ).lexically_normal();
    while( result.has_relative_path() && result.filename().empty() ) {
        result = result.parent_path();
    }
    return result;
}

fs::path storage( const fs::path &world )
{
    const fs::path root = normalized( fs::u8path( PATH_INFO::savedir() ) );
    const fs::path path = normalized( world );
    if( path.parent_path() != root || path.filename() == ".save-continuity" ) {
        throw std::runtime_error( "save protection requires a world in the configured save directory" );
    }
    return root / ".save-continuity" / path.filename();
}

std::string read_bytes( const fs::path &path )
{
    std::ifstream stream( path, std::ios::binary );
    if( !stream ) {
        throw std::runtime_error( "cannot read save protection file: " + path.u8string() );
    }
    std::ostringstream bytes;
    bytes << stream.rdbuf();
    if( stream.bad() ) {
        throw std::runtime_error( "cannot finish reading save protection file" );
    }
    return bytes.str();
}

void write_bytes( const fs::path &path, const std::string &bytes )
{
    // Deliberately not write_to_file: these files are outside the mutable world,
    // and must not recursively invoke its before-write hook.
    std::ofstream stream( path, std::ios::binary | std::ios::trunc );
    stream.exceptions( std::ios::failbit | std::ios::badbit );
    stream.write( bytes.data(), bytes.size() );
    stream.flush();
    stream.close();
}

void write_intent( const fs::path &path, const std::string &bytes )
{
    const fs::path stage = path.string() + ".pending";
    on_out_of_scope cleanup( [&]() {
        std::error_code ec;
        fs::remove( stage, ec );
    } );
    write_bytes( stage, bytes );
    // A failed metadata write must not truncate the identity of the only
    // trusted generation. This is ordinary I/O protection, not an fsync claim.
    if( !rename_file( stage, path ) ) {
        throw std::runtime_error( "cannot publish save protection intent" );
    }
}

std::string digest( const fs::path &path )
{
    std::ifstream stream( path, std::ios::binary );
    if( !stream ) {
        throw std::runtime_error( "cannot read world file for verification" );
    }
    const std::unique_ptr<XXH64_state_t, decltype( &XXH64_freeState )> state(
        XXH64_createState(), &XXH64_freeState );
    if( !state ) {
        throw std::runtime_error( "cannot allocate save verification state" );
    }
    XXH64_reset( state.get(), 0 );
    std::array<char, 64 * 1024> buffer;
    while( stream ) {
        stream.read( buffer.data(), buffer.size() );
        XXH64_update( state.get(), buffer.data(), stream.gcount() );
    }
    if( !stream.eof() ) {
        throw std::runtime_error( "cannot finish verifying world file" );
    }
    return std::to_string( XXH64_digest( state.get() ) );
}

using file_digests = std::map<std::string, std::string>;
file_digests inventory( const fs::path &world )
{
    file_digests files;
    for( const auto &entry : fs::recursive_directory_iterator( world ) ) {
        // A link can refer to shared state outside this world. Do not silently
        // replace that storage topology or omit its contents from a generation.
        if( entry.is_symlink() ) {
            throw std::runtime_error( "save protection cannot copy a linked world entry: " +
                                      entry.path().u8string() );
        }
        if( entry.is_regular_file() ) {
            files.emplace( entry.path().lexically_relative( world ).generic_u8string(),
                           digest( entry.path() ) );
        } else if( !entry.is_directory() ) {
            throw std::runtime_error( "unsupported world file in save protection" );
        }
    }
    if( files.empty() ) {
        throw std::runtime_error( "cannot protect an empty world" );
    }
    return files;
}

struct retirement_record {
    std::string character;
    int character_id;
    fs::path graveyard;
    file_digests files;
    bool captured = false;
    bool archive_started = false;
};

bool character_entry( const fs::path &relative, const std::string &character )
{
    const std::string prefix = base64_encode( character ) + ".";
    for( const auto &component : relative ) {
        const std::string name = component.u8string();
        if( name.compare( 0, prefix.size(), prefix ) == 0 ||
            name == base64_encode( character + "_diary" ) + ".json" ) {
            return true;
        }
    }
    return false;
}

void require_graveyard_path( const fs::path &path )
{
    const fs::path root = normalized( PATH_INFO::graveyarddir_path().get_unrelative_path() );
    const fs::path relative = normalized( path ).lexically_relative( root );
    if( relative.empty() || relative == "." || relative.is_absolute() ||
        *relative.begin() == ".." ) {
        throw std::runtime_error( "graveyard destination is outside its owning root" );
    }
    // A configured graveyard root may itself be a link. Its descendants may
    // not redirect this character's transfer into another owner's directory.
    fs::path current = root;
    for( const auto &component : relative ) {
        current /= component;
        if( fs::is_symlink( current ) ) {
            throw std::runtime_error( "graveyard transfer contains a linked descendant" );
        }
    }
}

retirement_record read_retirement( const JsonObject &json )
{
    retirement_record result;
    result.character = json.get_string( "character" );
    result.character_id = json.get_int( "character_id" );
    result.graveyard = normalized( fs::u8path( json.get_string( "graveyard" ) ) );
    json.read( "files", result.files, true );
    result.captured = json.get_bool( "captured" );
    result.archive_started = json.get_bool( "archive_started", false );
    const fs::path relative_graveyard = result.graveyard.lexically_relative(
                                          normalized( PATH_INFO::graveyarddir_path().get_unrelative_path() ) );
    if( result.character.empty() || result.character_id <= 0 || relative_graveyard.empty() ||
        relative_graveyard.is_absolute() || *relative_graveyard.begin() == ".." ) {
        throw std::runtime_error( "retirement identity or graveyard ownership is invalid" );
    }
    require_graveyard_path( result.graveyard );
    for( const auto &[name, hash] : result.files ) {
        const fs::path relative = fs::u8path( name );
        if( relative.empty() || relative.is_absolute() || relative.lexically_normal() != relative ||
            *relative.begin() == ".." || !character_entry( relative, result.character ) || hash.empty() ) {
            throw std::runtime_error( "retirement contains an unowned character entry" );
        }
    }
    return result;
}

void write_retirement( JsonOut &json, const retirement_record &record )
{
    json.start_object();
    json.member( "character", record.character );
    json.member( "character_id", record.character_id );
    json.member( "graveyard", record.graveyard.u8string() );
    json.member( "files", record.files );
    json.member( "captured", record.captured );
    json.member( "archive_started", record.archive_started );
    json.end_object();
}

void validate_retirement( const retirement_record &record, const file_digests &world_files )
{
    if( !record.captured || !record.archive_started ) {
        throw std::runtime_error( "retirement source capture/transfer did not complete" );
    }
    for( const auto &[name, hash] : world_files ) {
        if( character_entry( fs::u8path( name ), record.character ) ) {
            throw std::runtime_error( "retired character is still present in the protected world" );
        }
    }
    require_graveyard_path( record.graveyard );
    for( const auto &[name, hash] : record.files ) {
        const fs::path archived = record.graveyard / fs::u8path( name );
        require_graveyard_path( archived );
        if( fs::is_symlink( archived ) || !fs::is_regular_file( archived ) || digest( archived ) != hash ) {
            throw std::runtime_error( "affected graveyard transfer failed verification" );
        }
    }
}

struct generation {
    saved_point point;
    std::string timestamp;
    file_digests files;
    std::optional<retirement_record> retirement;
};

generation read_generation( const fs::path &directory, const fs::path &world )
{
    JsonObject json = json_loader::from_string( read_bytes( directory / "manifest.json" ) ).get_object();
    if( json.get_string( "world" ) != world.filename().u8string() ) {
        throw std::runtime_error( "save protection belongs to a different world" );
    }
    generation result;
    result.point.turn = json.get_int( "turn" );
    result.point.character = json.get_string( "character" );
    result.timestamp = json.get_string( "timestamp" );
    json.read( "files", result.files, true );
    if( result.files != inventory( directory / "world" ) ) {
        throw std::runtime_error( "complete save generation failed verification" );
    }
    if( json.has_member( "retirement" ) ) {
        result.retirement = read_retirement( json.get_object( "retirement" ) );
        validate_retirement( *result.retirement, result.files );
    }
    if( read_bytes( world / fs::u8path( PATH_INFO::world_timestamp() ) ) != result.timestamp ) {
        throw std::runtime_error( "working world identity does not match the protected generation" );
    }
    return result;
}

fs::path trusted_directory( const fs::path &world )
{
    const fs::path root = storage( world );
    if( fs::exists( root / "publication-base" ) ) {
        const std::string base = read_bytes( root / "publication-base" );
        if( base.empty() ) {
            return {}; // Failed first publication did not create a prior baseline.
        }
        for( const char *name : { "complete", "previous", "retirement-held" } ) {
            const fs::path directory = root / name;
            if( fs::exists( directory / "manifest.json" ) &&
                digest( directory / "manifest.json" ) == base ) {
                return directory;
            }
        }
        throw std::runtime_error( "prior complete publication identity is unavailable" );
    }
    if( fs::exists( root / "retirement-held" ) ) {
        return root / "retirement-held";
    }
    if( fs::exists( root / "complete" ) ) {
        return root / "complete";
    }
    // An interrupted directory rotation must retain the old complete copy;
    // an uncommitted candidate is never promoted by load.
    return root / "previous";
}

std::optional<retirement_record> pending_retirement( const fs::path &world )
{
    const fs::path intent = storage( world ) / "retirement";
    if( !fs::exists( intent ) ) {
        return std::nullopt;
    }
    const JsonObject json = json_loader::from_string( read_bytes( intent ) ).get_object();
    if( json.get_string( "timestamp" ) !=
        read_bytes( world / fs::u8path( PATH_INFO::world_timestamp() ) ) ) {
        throw std::runtime_error( "pending retirement belongs to a different world" );
    }
    return read_retirement( json.get_object( "retirement" ) );
}

void persist_retirement( const fs::path &world, const retirement_record &record )
{
    std::ostringstream bytes;
    JsonOut json( bytes );
    json.start_object();
    json.member( "timestamp", read_bytes( world / fs::u8path( PATH_INFO::world_timestamp() ) ) );
    json.member( "retirement" );
    write_retirement( json, record );
    json.end_object();
    write_intent( storage( world ) / "retirement", bytes.str() );
    pending_retirement( world );
}

void require_compatible_recovery( const fs::path &world )
{
    if( fs::exists( storage( world ) / "erasing" ) ) {
        throw std::runtime_error( "intentional world reset/deletion cannot be restored" );
    }
    if( fs::exists( storage( world ) / "retirement-held" ) ) {
        throw std::runtime_error( "the living generation is retained as uncertain retirement evidence, not a loadable fallback" );
    }
    if( const auto pending = pending_retirement( world ) ) {
        const fs::path directory = trusted_directory( world );
        const auto committed = directory.empty() || !fs::exists( directory ) ?
                               std::optional<retirement_record>() : read_generation( directory, world ).retirement;
        if( !committed || committed->character != pending->character ||
            committed->character_id != pending->character_id || committed->graveyard != pending->graveyard ||
            committed->files != pending->files ) {
            throw std::runtime_error( "death retirement is incomplete or uncertain; restoring the living generation is refused" );
        }
    }
}

void clear_stage( const fs::path &path )
{
    // Never applied to complete/previous; failed candidate copies are disposable.
    fs::remove_all( path );
}

struct restore_paths {
    fs::path target;
    fs::path root;
    fs::path stage;
    fs::path failed;
    fs::path intent;
};

restore_paths restoration_paths( const fs::path &world )
{
    const fs::path target = fs::canonical( world );
    const fs::path root = target.parent_path() / ".save-continuity" / normalized( world ).filename();
    return { target, root, root / "restore-pending", root / "restore-working", root / "restore-base" };
}

bool finish_completed_restore( const fs::path &world )
{
    if( !fs::exists( world ) ) {
        return false;
    }
    require_compatible_recovery( world );
    const restore_paths paths = restoration_paths( world );
    if( fs::exists( paths.stage ) ) {
        throw std::runtime_error( "previous restore staging remains; refusing a new working write" );
    }
    if( !fs::exists( paths.intent ) && !fs::exists( paths.failed ) ) {
        return false;
    }
    const fs::path directory = trusted_directory( world );
    if( directory.empty() || !fs::exists( paths.intent ) ) {
        throw std::runtime_error( "previous restore ownership is unavailable" );
    }
    const generation prior = read_generation( directory, world );
    if( read_bytes( paths.intent ) != digest( directory / "manifest.json" ) ||
        inventory( paths.target ) != prior.files ) {
        throw std::runtime_error( "previous restore did not establish the verified prior generation" );
    }
    // Only the already verified replacement can authorize old-working cleanup.
    // Finish it before either a load or an in-memory save retry can proceed.
    const fs::path root = storage( world );
    if( directory != root / "complete" ) {
        clear_stage( root / "complete" );
        fs::rename( directory, root / "complete" );
    }
    fs::remove( root / "publication-base" );
    fs::remove_all( paths.failed );
    fs::remove( root / "incomplete" );
    fs::remove( paths.intent );
    fs::remove( root / "retirement" );
    return true;
}
}

bool incomplete( const fs::path &world )
{
    const fs::path root = storage( world );
    if( fs::exists( root / "incomplete" ) || fs::exists( root / "publication-base" ) ||
        fs::exists( root / "retirement" ) || fs::exists( root / "retirement-held" ) ||
        fs::exists( root / "erasing" ) ) {
        return true;
    }
    if( fs::exists( world ) ) {
        const restore_paths paths = restoration_paths( world );
        return fs::exists( paths.intent ) || fs::exists( paths.stage ) || fs::exists( paths.failed );
    }
    return false;
}

void begin_write( const fs::path &world )
{
    if( fs::exists( world ) ) {
        const restore_paths paths = restoration_paths( world );
        if( fs::exists( paths.intent ) || fs::exists( paths.stage ) || fs::exists( paths.failed ) ) {
            finish_completed_restore( world );
        }
    }
    const fs::path root = storage( world );
    if( fs::exists( root / "erasing" ) ) {
        throw std::runtime_error( "working writes cannot replace an intentional world reset/deletion" );
    }
    fs::create_directories( root );
    if( !fs::exists( root / "incomplete" ) ) {
        write_bytes( root / "incomplete", "World publication is incomplete.\n" );
    }
}

void before_file_write( const fs::path &file )
{
    if( file.empty() ) {
        return;
    }
    const fs::path root = normalized( fs::u8path( PATH_INFO::savedir() ) );
    const fs::path relative = normalized( file ).lexically_relative( root );
    if( relative.empty() || relative.is_absolute() || *relative.begin() == ".." ||
        *relative.begin() == ".save-continuity" || std::next( relative.begin() ) == relative.end() ) {
        return;
    }
    const fs::path world = root / *relative.begin();
    const fs::path protected_root = storage( world );
    // Registry discovery may create metadata for a legacy world. That does not
    // manufacture a trusted baseline. Full/standalone gameplay writers call
    // begin_write even before the first protected generation exists.
    if( fs::exists( protected_root / "complete" ) || fs::exists( protected_root / "previous" ) ||
        fs::exists( protected_root / "incomplete" ) ) {
        begin_write( world );
    }
}

namespace
{
void publish_generation( const fs::path &world, int turn, const std::string &character,
                         const std::optional<retirement_record> &retirement )
{
    begin_write( world );
    const fs::path root = storage( world );
    const fs::path stage = root / "candidate";
    clear_stage( stage );
    fs::create_directories( stage );
    const file_digests files = inventory( world );
    fs::copy( world, stage / "world", fs::copy_options::recursive );
    if( inventory( stage / "world" ) != files ) {
        throw std::runtime_error( "new complete save copy failed verification" );
    }
    std::ostringstream bytes;
    JsonOut json( bytes );
    json.start_object();
    json.member( "world", world.filename().u8string() );
    json.member( "turn", turn );
    json.member( "character", character );
    json.member( "timestamp", read_bytes( world / fs::u8path( PATH_INFO::world_timestamp() ) ) );
    json.member( "files", files );
    if( retirement ) {
        validate_retirement( *retirement, files );
        json.member( "retirement" );
        write_retirement( json, *retirement );
    }
    json.end_object();
    write_bytes( stage / "manifest.json", bytes.str() );
    read_generation( stage, world );

    // A late rotation/unlink failure must still offer the previously committed
    // point, not relabel the failed new publication as a successful prior save.
    const fs::path prior = trusted_directory( world );
    if( !prior.empty() && fs::exists( prior ) ) {
        read_generation( prior, world );
        if( prior != root / "complete" ) {
            clear_stage( root / "complete" );
            fs::rename( prior, root / "complete" );
        }
    } else {
        clear_stage( root / "complete" );
    }
    write_intent( root / "publication-base", fs::exists( root / "complete" ) ?
                 digest( root / "complete" / "manifest.json" ) : std::string() );
    if( fs::exists( root / "complete" ) ) {
        // complete is still independently trusted before removing an older
        // rotation leftover; a failure cannot consume the sole fallback.
        read_generation( root / "complete", world );
        fs::remove_all( root / "previous" );
        fs::rename( root / "complete", root / "previous" );
    }
    fs::rename( stage, root / "complete" );
    fs::remove( root / "incomplete" );
    fs::remove( root / "publication-base" );
    // Cleanup failure does not invalidate a successfully published generation.
    std::error_code ec;
    fs::remove_all( root / "previous", ec );
}

} // namespace

bool retirement_refusal_persisted( const fs::path &world )
{
    // This is evidence of refusal, not completed retirement. A malformed or
    // foreign boundary cannot authorize disposal of the only in-memory fact.
    try {
        if( pending_retirement( world ) ) {
            return true;
        }
        const fs::path held = storage( world ) / "retirement-held";
        if( fs::exists( held ) ) {
            read_generation( held, world );
            return true;
        }
    } catch( const std::exception & ) {
        return false;
    }
    return false;
}

void require_ordinary_save( const fs::path &world )
{
    if( pending_retirement( world ) || fs::exists( storage( world ) / "retirement-held" ) ||
        fs::exists( storage( world ) / "erasing" ) ) {
        throw std::runtime_error( "ordinary saving cannot promote an incomplete retirement or world erasure" );
    }
}

void publish_complete( const fs::path &world, int turn, const std::string &character )
{
    require_ordinary_save( world );
    publish_generation( world, turn, character, std::nullopt );
}

bool begin_retirement( const fs::path &world, const std::string &character,
                       int character_id, const fs::path &graveyard, bool saved_character )
{
    const auto pending = pending_retirement( world );
    if( pending ) {
        if( pending->character != character || pending->character_id != character_id ) {
            throw std::runtime_error( "another character already owns the pending retirement" );
        }
    }
    const fs::path trusted = trusted_directory( world );
    if( !trusted.empty() && fs::exists( trusted ) ) {
        const generation prior = read_generation( trusted, world );
        const auto &completed = prior.retirement;
        if( completed && completed->character == character && completed->character_id == character_id ) {
            if( inventory( world ) != prior.files ||
                ( pending && ( pending->graveyard != completed->graveyard ||
                               pending->files != completed->files || !pending->archive_started ) ) ) {
                throw std::runtime_error( "completed retirement does not match the current world/transfer" );
            }
            // The protected generation and external transfer committed, but
            // intent removal may have failed. Finish only that exact commit;
            // never rerun world writers which could recreate archived files.
            fs::remove( storage( world ) / "retirement" );
            return true; // Exactly verified completed callback, not mere absence.
        }
    }
    if( pending && pending->captured ) {
        return false;
    }
    // Initial source capture failed before any archival. Keep its original
    // destination/identity and finish capture on this in-memory retry.
    retirement_record record{ character, character_id, normalized( graveyard ), {}, false, false };
    if( pending ) {
        record = *pending;
    }
    fs::create_directories( storage( world ) );
    // Either durable boundary refuses a living fallback. Attempt both: a
    // failed metadata write can leave a renamed prior generation as evidence;
    // a failed rename can leave a persisted uncertain-retirement intent.
    std::exception_ptr intent_failure;
    try {
        persist_retirement( world, record );
    } catch( ... ) {
        intent_failure = std::current_exception();
    }
    const fs::path held = storage( world ) / "retirement-held";
    if( !trusted.empty() && fs::exists( trusted ) && trusted != held ) {
        fs::rename( trusted, held );
    }
    if( intent_failure ) {
        std::rethrow_exception( intent_failure );
    }
    begin_write( world );
    for( const auto &[name, hash] : inventory( world ) ) {
        if( character_entry( fs::u8path( name ), character ) ) {
            record.files.emplace( name, hash );
        }
    }
    if( saved_character && record.files.empty() ) {
        throw std::runtime_error( "saved character retirement has no owned source files" );
    }
    record.captured = true;
    persist_retirement( world, record );
    return false;
}

void archive_retirement( const fs::path &world )
{
    auto pending = pending_retirement( world );
    if( !pending || !pending->captured ) {
        throw std::runtime_error( "graveyard transfer has no captured owned retirement intent" );
    }
    if( !pending->archive_started ) {
        // The actual last diary interaction can update/add owned files. Capture
        // its final state once, before any source can be removed. A partial
        // archival retry must retain this exact transfer ledger.
        const file_digests final_files = inventory( world );
        for( const auto &[name, hash] : pending->files ) {
            if( final_files.count( name ) == 0 ) {
                throw std::runtime_error( "captured character source disappeared before archival" );
            }
        }
        pending->files.clear();
        for( const auto &[name, hash] : final_files ) {
            if( character_entry( fs::u8path( name ), pending->character ) ) {
                pending->files.emplace( name, hash );
            }
        }
        pending->archive_started = true;
        persist_retirement( world, *pending );
    }
    for( const auto &[name, hash] : pending->files ) {
        const fs::path source = world / fs::u8path( name );
        const fs::path destination = pending->graveyard / fs::u8path( name );
        require_graveyard_path( destination );
        fs::create_directories( destination.parent_path() );
        if( fs::exists( destination ) ) {
            if( fs::is_symlink( destination ) || !fs::is_regular_file( destination ) ||
                digest( destination ) != hash ) {
                throw std::runtime_error( "graveyard destination conflicts with the owned transfer" );
            }
        } else {
            if( !fs::is_regular_file( source ) || fs::is_symlink( source ) || digest( source ) != hash ) {
                throw std::runtime_error( "character source changed before archival" );
            }
            // Copy/verify before removing the source also supports a world
            // symlink on a different volume. A failed archive is never deletion.
            const fs::path stage = destination.string() + ".retirement-pending";
            require_graveyard_path( stage );
            if( fs::exists( stage ) ) {
                throw std::runtime_error( "graveyard staging is already occupied" );
            }
            on_out_of_scope discard_stage( [&]() {
                std::error_code ec;
                fs::remove( stage, ec );
            } );
            fs::copy_file( source, stage );
            if( digest( stage ) != hash ) {
                throw std::runtime_error( "graveyard copy failed verification" );
            }
            fs::rename( stage, destination );
        }
        if( fs::exists( source ) ) {
            if( digest( source ) != hash || !fs::remove( source ) ) {
                throw std::runtime_error( "character exclusion failed after verified archival" );
            }
        }
    }
    validate_retirement( *pending, inventory( world ) );
}

void publish_retirement( const fs::path &world, int turn,
                         const std::string &character, int character_id )
{
    const auto pending = pending_retirement( world );
    if( !pending || pending->character != character || pending->character_id != character_id ) {
        throw std::runtime_error( "retirement publication has no matching physical character" );
    }
    publish_generation( world, turn, character, pending );
    fs::remove( storage( world ) / "retirement" );
}

void begin_erase( const fs::path &world )
{
    const fs::path root = storage( world );
    fs::create_directories( root );
    write_intent( root / "erasing", "Intentional world reset/deletion. No recovery.\n" );
}

std::optional<saved_point> prior_point( const fs::path &world )
{
    const fs::path directory = trusted_directory( world );
    if( directory.empty() || !fs::exists( directory ) ) {
        return std::nullopt;
    }
    return read_generation( directory, world ).point;
}

void restore_prior( const fs::path &world )
{
    require_compatible_recovery( world );
    if( finish_completed_restore( world ) ) {
        return;
    }
    const fs::path directory = trusted_directory( world );
    if( directory.empty() ) {
        throw std::runtime_error( "no previously committed complete generation" );
    }
    const generation prior = read_generation( directory, world );
    // Preserve a root world symlink and keep staging on its target filesystem,
    // outside worldfactory's one-level discovery.
    const restore_paths paths = restoration_paths( world );
    fs::create_directories( paths.root );
    on_out_of_scope clean_stage( [&]() {
        std::error_code ec;
        fs::remove_all( paths.stage, ec );
    } );
    fs::copy( directory / "world", paths.stage, fs::copy_options::recursive );
    if( inventory( paths.stage ) != prior.files ) {
        throw std::runtime_error( "staged restore failed verification" );
    }
    // Caller has not started deserialize. Evict dictionary and parsed-save
    // caches before replacing files; existing owned contexts remain alive.
    zzip::invalidate_world_cache( world );
    json_loader::invalidate_save_cache( normalized( world ).filename().u8string() );
    write_intent( paths.intent, digest( directory / "manifest.json" ) );
    fs::rename( paths.target, paths.failed );
    try {
        fs::rename( paths.stage, paths.target );
    } catch( ... ) {
        fs::rename( paths.failed, paths.target );
        fs::remove( paths.intent );
        throw;
    }
    finish_completed_restore( world );
}

bool prepare_load( const fs::path &world, bool &restored )
{
    restored = false;
    try {
        if( !incomplete( world ) ) {
            return true;
        }
        require_compatible_recovery( world );
        const std::optional<saved_point> prior = prior_point( world );
        if( !prior ) {
            popup( _( "This world has an incomplete save and no trusted complete generation. Loading was canceled. The in-memory game can still retry saving if it is running." ) );
            return false;
        }
        if( !query_yn( _( "This world has an incomplete save. Restore the complete saved point at turn %d (%s)? This rolls back all characters and world-local settings; progress since that point may be lost. Global achievements and last-world hints are not rolled back. No cancels loading." ),
                       prior->turn, prior->character ) ) {
            return false;
        }
        restore_prior( world );
        restored = true;
        return true;
    } catch( const std::exception &error ) {
        popup( _( "Save recovery failed; the incomplete world was not loaded. The complete fallback is retained. %s" ),
               error.what() );
        return false;
    }
}

void forget( const fs::path &world )
{
    zzip::invalidate_world_cache( world );
    json_loader::invalidate_save_cache( normalized( world ).filename().u8string() );
    fs::remove_all( storage( world ) );
}
}

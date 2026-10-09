#pragma once
#ifndef CATA_SRC_SAVE_CONTINUITY_H
#define CATA_SRC_SAVE_CONTINUITY_H

#include <filesystem>
#include <optional>
#include <string>

// Filesystem publication metadata only; no actor or outing state is duplicated.
namespace save_continuity
{
struct saved_point {
    int turn;
    std::string character;
};

void begin_write( const std::filesystem::path &world );
void before_file_write( const std::filesystem::path &file );
void publish_complete( const std::filesystem::path &world, int turn,
                       const std::string &character );
bool incomplete( const std::filesystem::path &world );
std::optional<saved_point> prior_point( const std::filesystem::path &world );
void restore_prior( const std::filesystem::path &world );
// Uses the existing native yes/no owner. A successful restoration is reported
// separately so callers can refresh world options/mods before game::setup.
bool prepare_load( const std::filesystem::path &world, bool &restored );
// A pending retirement/intentional erase is not permission to restore a
// living generation. The graveyard transfer belongs to this publication.
bool begin_retirement( const std::filesystem::path &world, const std::string &character,
                       int character_id, const std::filesystem::path &graveyard, bool saved_character );
void archive_retirement( const std::filesystem::path &world );
void publish_retirement( const std::filesystem::path &world, int turn,
                         const std::string &character, int character_id );
bool retirement_refusal_persisted( const std::filesystem::path &world );
void require_ordinary_save( const std::filesystem::path &world );
void begin_erase( const std::filesystem::path &world );
void forget( const std::filesystem::path &world );
}
#endif

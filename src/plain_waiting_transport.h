#pragma once
#ifndef CATA_SRC_PLAIN_WAITING_TRANSPORT_H
#define CATA_SRC_PLAIN_WAITING_TRANSPORT_H

#include <cstddef>
#include <string>

bool plain_waiting_active();
void plain_waiting_publish_event( const std::string &event );
void plain_waiting_record_turn( size_t generation, double seconds, int game_turn,
                                int game_minutes );

#endif

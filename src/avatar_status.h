#pragma once
#ifndef CATA_SRC_AVATAR_STATUS_H
#define CATA_SRC_AVATAR_STATUS_H

#include <map>
#include <string>

class avatar;

// Read-only World facts from native player status and display getters.
std::map<std::string, std::string> avatar_status_payload( const avatar &viewer );

#endif // CATA_SRC_AVATAR_STATUS_H

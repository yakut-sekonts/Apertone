#pragma once
#include "core/edits/LocalMasks.h"
struct sqlite3;
namespace luma::detail {
std::vector<LocalMask> readLocalMasks(sqlite3 *db, const std::string &assetID);
}

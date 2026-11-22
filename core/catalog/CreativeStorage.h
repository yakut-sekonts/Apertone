#pragma once
#include "core/edits/CreativeSettings.h"
struct sqlite3;
namespace luma::detail {
CreativeSettings readCreative(sqlite3 *db, const std::string &assetID);
void restoreCameraProfile(sqlite3 *db, DcpSelection &selection);
void storeCameraProfile(sqlite3 *db, const DcpSelection &selection);
} // namespace luma::detail

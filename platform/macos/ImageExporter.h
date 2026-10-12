#pragma once
#include "core/edits/EditDocument.h"
#include "core/export/ExportOptions.h"
#include "core/raw/RawDecoder.h"

namespace luma::macos {
void writeImage(const DecodedRaw &decoded, EditDocument edits,
                const std::filesystem::path &destination, ExportOptions options,
                CancellationToken stop = {}, const std::function<void(double)> &progress = {});
}

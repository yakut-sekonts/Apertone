#include "platform/macos/FileHasher.h"
#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace luma::macos {
std::string sha256File(const std::filesystem::path &path, luma::CancellationToken stop) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot read original file.");
    }
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    std::array<char, 65536> buffer{};
    while (stream) {
        if (stop.isCancelled()) {
            throw std::runtime_error("Operation cancelled.");
        }
        stream.read(buffer.data(), buffer.size());
        CC_SHA256_Update(&context, buffer.data(), static_cast<CC_LONG>(stream.gcount()));
    }
    if (!stream.eof()) {
        throw std::runtime_error("Original file read failed.");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
    CC_SHA256_Final(digest.data(), &context);
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        result << std::setw(2) << static_cast<unsigned>(byte);
    }
    return result.str();
}
} // namespace luma::macos

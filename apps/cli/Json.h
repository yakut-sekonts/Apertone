#pragma once
#include <iomanip>
#include <sstream>
#include <string>
namespace luma::cli {
inline std::string quoted(const std::string &value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\')
            output << '\\' << c;
        else if (c < 32)
            output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c)
                   << std::dec;
        else
            output << c;
    }
    output << '"';
    return output.str();
}
} // namespace luma::cli

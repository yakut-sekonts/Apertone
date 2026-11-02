#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace luma {
struct DerivedSourceProvenance {
    std::string id;
    std::string checksum;
    std::int64_t revision = 0;
};

struct DerivedProvenance {
    std::string operation;
    std::string provider;
    std::string providerVersion;
    std::string algorithm;
    std::string algorithmVersion;
    std::string inputContract;
    std::string outputContract;
    std::string model;
    std::string modelVersion;
    bool deterministic = true;
    bool local = true;
    std::vector<DerivedSourceProvenance> sources;
    std::vector<std::pair<std::string, std::string>> parameters;
};

std::string encodeDerivedProvenance(const DerivedProvenance &value);
std::string linearFloatContract(int width, int height, bool alpha);
} // namespace luma

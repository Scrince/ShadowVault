#include "fuzz_entry.hpp"

#include <sv/keyfile/combiner.hpp>
#include <sv/keyfile/keyfile_format.hpp>

#include <fuzzer/FuzzedDataProvider.h>

#include <array>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
    FuzzedDataProvider provider(data, size);

    const auto keyfile_bytes = provider.ConsumeBytes<std::uint8_t>(
        provider.ConsumeIntegralInRange<std::size_t>(0, sv::keyfile::kMaxKeyfileReadSize));
    (void)sv::keyfile::hash_keyfile_bytes(keyfile_bytes);

    const auto password = provider.ConsumeBytesAsString(
        provider.ConsumeIntegralInRange<std::size_t>(0, 256));
    const auto keyfile_count = provider.ConsumeIntegralInRange<std::size_t>(0, sv::keyfile::kMaxKeyfiles);

    std::vector<sv::keyfile::KeyfileHash> hashes;
    hashes.reserve(keyfile_count);
    for (std::size_t i = 0; i < keyfile_count; ++i) {
        sv::keyfile::KeyfileHash hash {};
        provider.ConsumeData(hash.data(), hash.size());
        hashes.push_back(hash);
    }

    (void)sv::keyfile::combine_credentials(password, hashes);

    const auto trailing = provider.ConsumeRemainingBytes<std::uint8_t>();
    if (!trailing.empty()) {
        (void)sv::keyfile::hash_keyfile_bytes(trailing);
    }

    return 0;
}
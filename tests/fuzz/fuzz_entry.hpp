#pragma once

#include <cstddef>
#include <cstdint>

/// libFuzzer entry point (provided by the fuzzer runtime when linked).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);
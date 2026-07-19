#pragma once

#include <sv/mount/mount_types.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace sv::mount {


class ChunkCache {
public:
    explicit ChunkCache(CachePolicy policy = {});

    [[nodiscard]] Result<ConstByteSpan> get(std::uint64_t chunk_id);
    [[nodiscard]] Result<void> put(std::uint64_t chunk_id, Span<const std::uint8_t> plaintext);
    void invalidate_all() noexcept;

private:
    struct Entry {
        std::uint64_t chunk_id = 0;
        std::vector<std::uint8_t> data;
    };

    void evict_if_needed(std::size_t incoming_bytes);

    CachePolicy policy_;
    std::mutex mutex_;
    std::size_t used_bytes_ = 0;
    std::list<Entry> lru_;
    std::unordered_map<std::uint64_t, std::list<Entry>::iterator> index_;
};

} 
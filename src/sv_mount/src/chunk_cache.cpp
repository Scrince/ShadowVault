#include <sv/mount/chunk_cache.hpp>

namespace sv::mount {

ChunkCache::ChunkCache(const CachePolicy policy)
    : policy_(policy) {}

Result<ConstByteSpan> ChunkCache::get(const std::uint64_t chunk_id) {
    std::lock_guard lock(mutex_);
    const auto it = index_.find(chunk_id);
    if (it == index_.end()) {
        return Result<ConstByteSpan>::err(ErrorCode::InvalidArgument, "chunk not cached");
    }

    lru_.splice(lru_.begin(), lru_, it->second);
    return Result<ConstByteSpan>::ok(sv::as_bytes(it->second->data));
}

Result<void> ChunkCache::put(const std::uint64_t chunk_id, const Span<const std::uint8_t> plaintext) {
    std::lock_guard lock(mutex_);

    const auto existing = index_.find(chunk_id);
    if (existing != index_.end()) {
        used_bytes_ -= existing->second->data.size();
        lru_.erase(existing->second);
        index_.erase(existing);
    }

    evict_if_needed(plaintext.size());

    lru_.push_front(Entry {chunk_id, std::vector<std::uint8_t>(plaintext.begin(), plaintext.end())});
    index_.emplace(chunk_id, lru_.begin());
    used_bytes_ += plaintext.size();
    return Result<void>::ok();
}

void ChunkCache::invalidate_all() noexcept {
    std::lock_guard lock(mutex_);
    lru_.clear();
    index_.clear();
    used_bytes_ = 0;
}

void ChunkCache::evict_if_needed(const std::size_t incoming_bytes) {
    while (!lru_.empty() && used_bytes_ + incoming_bytes > policy_.max_bytes) {
        const auto& back = lru_.back();
        used_bytes_ -= back.data.size();
        index_.erase(back.chunk_id);
        lru_.pop_back();
    }
}

} // namespace sv::mount
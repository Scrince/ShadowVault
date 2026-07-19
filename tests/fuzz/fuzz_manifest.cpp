#include "fuzz_entry.hpp"

#include <sv/container/types.hpp>
#include <sv/vault/inode.hpp>
#include <sv/vault/manifest.hpp>

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <vector>

namespace {

sv::vault::ManifestPage make_fuzz_page(FuzzedDataProvider& provider) {
    sv::vault::ManifestPage page {};
    page.header.page_version = provider.ConsumeIntegral<std::uint16_t>();
    page.header.page_type = static_cast<sv::container::ManifestPageType>(
        provider.ConsumeIntegralInRange<std::uint16_t>(
            0,
            static_cast<std::uint16_t>(sv::container::ManifestPageType::FreeBitmap)));
    page.header.generation = provider.ConsumeIntegral<std::uint64_t>();
    page.header.page_index = provider.ConsumeIntegral<std::uint64_t>();

    const std::uint32_t max_entry_count = page.header.page_type == sv::container::ManifestPageType::InodeTable
        ? sv::container::SVLT_MAX_INODES_V1
        : static_cast<std::uint32_t>(sv::vault::kManifestMaxPagePlaintext);
    page.header.entry_count = provider.ConsumeIntegralInRange<std::uint32_t>(0, max_entry_count);

    const std::size_t max_body = page.header.page_type == sv::container::ManifestPageType::InodeTable
        ? static_cast<std::size_t>(page.header.entry_count) * sv::container::SVLT_INODE_SIZE
        : static_cast<std::size_t>(page.header.entry_count);
    const std::size_t bounded_body = std::min(max_body, sv::vault::kManifestMaxPagePlaintext);
    page.body = provider.ConsumeBytes<std::uint8_t>(
        provider.ConsumeIntegralInRange<std::size_t>(0, bounded_body));

    return page;
}

} 

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
    FuzzedDataProvider provider(data, size);

    if (provider.remaining_bytes() >= sv::vault::kManifestPageHeaderSize) {
        const auto header_bytes = provider.ConsumeBytes<std::uint8_t>(sv::vault::kManifestPageHeaderSize);
        (void)sv::vault::decode_manifest_page_header(header_bytes);
    }

    auto inode_page = make_fuzz_page(provider);
    auto bitmap_page = make_fuzz_page(provider);
    bitmap_page.header.generation = inode_page.header.generation;
    (void)sv::vault::manifest_from_pages(inode_page, bitmap_page);

    const std::size_t inode_stride = sv::container::SVLT_INODE_SIZE;
    const std::size_t inode_chunks = inode_page.body.size() / inode_stride;
    for (std::size_t i = 0; i < inode_chunks; ++i) {
        const auto offset = i * inode_stride;
        (void)sv::vault::decode_inode(sv::Span<const std::uint8_t>(
            inode_page.body.data() + offset,
            inode_stride));
    }

    return 0;
}
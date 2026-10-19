#pragma once

#include "update_security.h"

#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace redotchestrator::updater {
    constexpr uint64_t MAX_ARCHIVE_SIZE = 1024ULL * 1024 * 1024;
    constexpr uint64_t MAX_EXTRACTED_SIZE = 1024ULL * 1024 * 1024;
    constexpr size_t MAX_ARCHIVE_ENTRIES = 16384;

    constexpr std::array<std::string_view, 16> REQUIRED_LIBRARY_FEATURES = {
        "linux.x86_64", "linux.debug.x86_64", "linux.arm64", "linux.debug.arm64",
        "macos", "macos.debug", "ios", "ios.debug", "windows.x86_64", "windows.debug.x86_64",
        "android.arm32", "android.debug.arm32", "android.arm64", "android.debug.arm64",
        "web.release.threads.wasm32", "web.release.nothreads.wasm32"
    };
    constexpr std::array<std::string_view, 7> REQUIRED_PACKAGE_DOCUMENTS = {
        "ASSET_LICENSES.md", "AUTHORS.md", "CHANGELOG.md", "LICENSE", "MODIFICATIONS.md", "README.md",
        "THIRD_PARTY_LICENSES/redot-cpp-LICENSE.md"
    };

    struct ArchiveEntry {
        std::string path;
        uint32_t offset;
        uint32_t compressed_size;
        uint32_t size;
        uint32_t crc;
        uint16_t method;
    };

    inline uint32_t zip_u32(std::span<const uint8_t> p_bytes, size_t p_offset) {
        return uint32_t(p_bytes[p_offset]) | (uint32_t(p_bytes[p_offset + 1]) << 8)
            | (uint32_t(p_bytes[p_offset + 2]) << 16) | (uint32_t(p_bytes[p_offset + 3]) << 24);
    }

    inline uint16_t zip_u16(std::span<const uint8_t> p_bytes, size_t p_offset) {
        return uint16_t(p_bytes[p_offset]) | (uint16_t(p_bytes[p_offset + 1]) << 8);
    }

    inline uint32_t archive_crc(std::span<const uint8_t> p_bytes) {
        uint32_t crc = 0xffffffff;
        for (const uint8_t byte : p_bytes) {
            crc ^= byte;
            for (unsigned bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
            }
        }
        return ~crc;
    }

    // Admit only ordinary single-volume ZIPs. ZIP64, encryption and special files are unnecessary
    // for packages bounded below 1 GiB; rejecting them keeps metadata and extraction unambiguous.
    inline bool inspect_archive(std::span<const uint8_t> p_bytes, std::vector<ArchiveEntry>& r_entries) {
        r_entries.clear();
        if (p_bytes.size() < 22 || p_bytes.size() > MAX_ARCHIVE_SIZE) {
            return false;
        }
        size_t end = p_bytes.size() - 22;
        const size_t first = end > 65535 ? end - 65535 : 0;
        while (zip_u32(p_bytes, end) != 0x06054b50 || end + 22 + zip_u16(p_bytes, end + 20) != p_bytes.size()) {
            if (end == first) {
                return false;
            }
            --end;
        }
        const size_t count = zip_u16(p_bytes, end + 10);
        const size_t central_size = zip_u32(p_bytes, end + 12);
        size_t cursor = zip_u32(p_bytes, end + 16);
        if (zip_u16(p_bytes, end + 4) || zip_u16(p_bytes, end + 6)
            || count != zip_u16(p_bytes, end + 8) || !count || count > MAX_ARCHIVE_ENTRIES
            || cursor > end || central_size != end - cursor) {
            return false;
        }
        std::unordered_set<std::string> seen;
        uint64_t total = 0;
        size_t next_local = 0;
        for (size_t index = 0; index < count; ++index) {
            if (cursor > end || end - cursor < 46 || zip_u32(p_bytes, cursor) != 0x02014b50) {
                return false;
            }
            const uint16_t flags = zip_u16(p_bytes, cursor + 8);
            const uint16_t method = zip_u16(p_bytes, cursor + 10);
            const size_t name_size = zip_u16(p_bytes, cursor + 28);
            const size_t record_size = 46 + name_size + zip_u16(p_bytes, cursor + 30) + zip_u16(p_bytes, cursor + 32);
            const uint32_t mode = zip_u32(p_bytes, cursor + 38) >> 16;
            if (record_size > end - cursor || (flags & ~0x800U) || (method != 0 && method != 8)
                || zip_u16(p_bytes, cursor + 34) || ((mode & 0170000) != 0
                    && (mode & 0170000) != 0100000 && (mode & 0170000) != 0040000)) {
                return false;
            }
            ArchiveEntry entry{
                std::string(reinterpret_cast<const char*>(p_bytes.data() + cursor + 46), name_size),
                zip_u32(p_bytes, cursor + 42), zip_u32(p_bytes, cursor + 20),
                zip_u32(p_bytes, cursor + 24), zip_u32(p_bytes, cursor + 16), method
            };
            std::string key = entry.path;
            for (char& character : key) {
                character = ascii_lower(character);
            }
            if (!key.empty() && key.back() == '/') {
                key.pop_back();
            }
            if (!is_safe_plugin_archive_path(entry.path) || !seen.insert(key).second
                || entry.size > MAX_EXTRACTED_SIZE - total || entry.offset != next_local
                || entry.offset > cursor || cursor - entry.offset < 30
                || zip_u32(p_bytes, entry.offset) != 0x04034b50) {
                return false;
            }
            total += entry.size;
            const size_t local = entry.offset;
            const size_t local_name_size = zip_u16(p_bytes, local + 26);
            const size_t header_size = 30 + local_name_size + zip_u16(p_bytes, local + 28);
            if (header_size > cursor - local || local_name_size != name_size
                || std::string_view(reinterpret_cast<const char*>(p_bytes.data() + local + 30), local_name_size) != entry.path
                || zip_u16(p_bytes, local + 6) != flags || zip_u16(p_bytes, local + 8) != method
                || zip_u32(p_bytes, local + 14) != entry.crc
                || zip_u32(p_bytes, local + 18) != entry.compressed_size || zip_u32(p_bytes, local + 22) != entry.size) {
                return false;
            }
            entry.offset += static_cast<uint32_t>(header_size);
            if (entry.compressed_size > cursor - entry.offset
                || (method == 8 && !entry.size)
                || (method == 0 && entry.size != entry.compressed_size)
                || (entry.path.back() == '/' && (entry.size || entry.compressed_size))) {
                return false;
            }
            next_local = size_t(entry.offset) + entry.compressed_size;
            r_entries.push_back(std::move(entry));
            cursor += record_size;
        }
        return cursor == end && next_local == end - central_size;
    }

    inline bool verify_archive_entry(const ArchiveEntry& p_entry, std::span<const uint8_t> p_output) {
        return p_output.size() == p_entry.size && archive_crc(p_output) == p_entry.crc;
    }
}

#pragma once

#include <filesystem>
#include <system_error>
#include <span>
#include <cstdint>
#include <string_view>
#include <vector>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace redotchestrator::updater {
    inline std::filesystem::path update_native_path(std::string_view p_path) {
        return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(p_path.data()), p_path.size()));
    }

    inline bool plain_update_directory(const std::filesystem::path& p_path) {
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(p_path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)
            && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
        std::error_code error;
        const auto status = std::filesystem::symlink_status(p_path, error);
        return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
#endif
    }

    // Check lexical components without resolving through a link. Only the final entry may be absent.
    inline bool plain_update_directory_chain(const std::filesystem::path& p_root,
            const std::filesystem::path& p_path, bool p_allow_missing = false) {
        const auto root = p_root.lexically_normal();
        const auto path = p_path.lexically_normal();
        const auto relative = path.lexically_relative(root);
        if (!root.is_absolute() || !path.is_absolute() || relative.empty() || !plain_update_directory(root)) {
            return false;
        }
        auto current = root;
        for (const auto& component : relative) {
            if (component == ".") { continue; }
            if (component == "..") { return false; }
            current /= component;
            if (plain_update_directory(current)) { continue; }
            std::error_code error;
            const auto status = std::filesystem::symlink_status(current, error);
            return p_allow_missing && current == path && status.type() == std::filesystem::file_type::not_found
                && (!error || error == std::errc::no_such_file_or_directory);
        }
        return true;
    }

    // Pin every directory being traversed on Windows, where junction replacement is otherwise
    // permitted even while ordinary file handles are open. Never open a root through a link.
    class UpdateDirectoryLock {
#ifdef _WIN32
        HANDLE handle = INVALID_HANDLE_VALUE;
#else
        int handle = -1;
#endif
    public:
        explicit UpdateDirectoryLock(const std::filesystem::path& p_path,
                const std::filesystem::path& p_root = {}) {
            if (!plain_update_directory_chain(p_root.empty() ? p_path.root_path() : p_root, p_path)) { return; }
#ifdef _WIN32
            handle = CreateFileW(p_path.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            FILE_ATTRIBUTE_TAG_INFO info{};
            if (handle != INVALID_HANDLE_VALUE && (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info))
                || !(info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))) {
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
            }
#else
            handle = open(p_path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
#endif
        }
        UpdateDirectoryLock(const UpdateDirectoryLock&) = delete;
        UpdateDirectoryLock& operator=(const UpdateDirectoryLock&) = delete;
        bool valid() const {
#ifdef _WIN32
            return handle != INVALID_HANDLE_VALUE;
#else
            return handle >= 0;
#endif
        }
#ifdef _WIN32
        static std::vector<uint8_t> prepare_rename(const std::filesystem::path& p_destination) {
            const DWORD name_size = static_cast<DWORD>(p_destination.native().size() * sizeof(wchar_t));
            std::vector<uint8_t> storage(sizeof(FILE_RENAME_INFO) + name_size);
            auto* info = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
            info->ReplaceIfExists = FALSE;
            info->RootDirectory = nullptr;
            info->FileNameLength = name_size;
            memcpy(info->FileName, p_destination.c_str(), name_size);
            return storage;
        }
        bool rename_to(std::vector<uint8_t>& p_info) const {
            return valid() && SetFileInformationByHandle(handle, FileRenameInfo, p_info.data(),
                static_cast<DWORD>(p_info.size()));
        }
#endif
        ~UpdateDirectoryLock() {
#ifdef _WIN32
            if (valid()) { CloseHandle(handle); }
#else
            if (valid()) { close(handle); }
#endif
        }
    };

    inline bool write_update_file(const std::filesystem::path& p_path, std::span<const uint8_t> p_bytes,
            const std::filesystem::path& p_root = {}) {
        if (!plain_update_directory_chain(p_root.empty() ? p_path.root_path() : p_root, p_path.parent_path())) {
            return false;
        }
#ifdef _WIN32
        HANDLE file = CreateFileW(p_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) { return false; }
        DWORD written = 0;
        const bool ok = WriteFile(file, p_bytes.data(), static_cast<DWORD>(p_bytes.size()), &written, nullptr)
            && written == p_bytes.size() && FlushFileBuffers(file);
        const bool closed = CloseHandle(file);
        return ok && closed;
#else
        const int file = open(p_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
        if (file < 0) { return false; }
        size_t offset = 0;
        while (offset < p_bytes.size()) {
            const ssize_t written = write(file, p_bytes.data() + offset, p_bytes.size() - offset);
            if (written <= 0) { close(file); return false; }
            offset += static_cast<size_t>(written);
        }
        const bool ok = fsync(file) == 0;
        return close(file) == 0 && ok;
#endif
    }

    inline bool remove_update_tree(const std::filesystem::path& p_path, const std::filesystem::path& p_root = {}) {
        namespace fs = std::filesystem;
        const auto root = p_root.empty() ? p_path.root_path() : p_root;
        if (!plain_update_directory_chain(root, p_path.parent_path())) { return false; }
        std::error_code error;
        const auto status = fs::symlink_status(p_path, error);
        if (error == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found) {
            return true;
        }
        if (error) { return false; }
        if (!fs::is_directory(status) || fs::is_symlink(status)) {
            return fs::remove(p_path, error) && !error;
        }
        {
            UpdateDirectoryLock lock(p_path, root);
            if (!lock.valid()) { return false; }
            fs::directory_iterator iterator(p_path, error), end;
            while (!error && iterator != end) {
                if (!remove_update_tree(iterator->path(), root)) { return false; }
                iterator.increment(error);
            }
            if (error) { return false; }
        }
        return fs::remove(p_path, error) && !error;
    }

    enum class UpdateRecovery { NONE, RESTORE_BACKUP, CONFIRM_STARTED, DISCARD_STAGING, KEEP_PENDING };

    inline UpdateRecovery update_recovery(bool p_record, bool p_target, bool p_backup, bool p_new_version) {
        if (!p_record) { return UpdateRecovery::NONE; }
        if (!p_target && p_backup) { return UpdateRecovery::RESTORE_BACKUP; }
        if (p_target && p_new_version) { return UpdateRecovery::CONFIRM_STARTED; }
        if (p_target && !p_backup) { return UpdateRecovery::DISCARD_STAGING; }
        return UpdateRecovery::KEEP_PENDING;
    }
}

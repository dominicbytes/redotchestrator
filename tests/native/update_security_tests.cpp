#include "editor/updater/update_security.h"
#include "editor/updater/update_archive.h"
#include "editor/updater/update_filesystem.h"
#include <fstream>
#include <iterator>
#include <string>

#include <iostream>
#include <string_view>
#include <random>

namespace {
    int failures = 0;

    std::filesystem::path reserve_test_directory() {
        std::random_device random;
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        for (unsigned attempt = 0; attempt < 128; ++attempt) {
            const auto path = parent / ("redotchestrator-security-test-" + std::to_string(random()) + "-" + std::to_string(random()));
            std::error_code error;
            if (std::filesystem::create_directory(path, error)) { return path; }
            if (error && error != std::errc::file_exists) { return {}; }
        }
        return {};
    }

    void expect(bool p_condition, std::string_view p_name) {
        if (!p_condition) {
            std::cerr << "FAIL: " << p_name << '\n';
            ++failures;
        }
    }
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--paths") {
        for (std::string path; std::getline(std::cin, path);) {
            std::cout << redotchestrator::updater::is_safe_plugin_archive_path(path) << '\n';
        }
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--tags") {
        for (std::string value; std::getline(std::cin, value);) {
            std::cout << redotchestrator::updater::is_valid_release_tag(value) << '\n';
        }
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--contract") {
        std::cout << "limit:asset:" << redotchestrator::updater::MAX_ARCHIVE_SIZE << '\n';
        std::cout << "limit:expanded:" << redotchestrator::updater::MAX_EXTRACTED_SIZE << '\n';
        std::cout << "limit:entries:" << redotchestrator::updater::MAX_ARCHIVE_ENTRIES << '\n';
        for (const auto value : redotchestrator::updater::REQUIRED_LIBRARY_FEATURES) {
            std::cout << "feature:" << value << '\n';
        }
        for (const auto value : redotchestrator::updater::REQUIRED_PACKAGE_DOCUMENTS) {
            std::cout << "document:" << value << '\n';
        }
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--remove-tree") {
        return redotchestrator::updater::remove_update_tree(redotchestrator::updater::update_native_path(argv[2])) ? 0 : 1;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--extract-file") {
        const auto path = redotchestrator::updater::update_native_path(argv[2]);
        redotchestrator::updater::UpdateDirectoryLock lock(path);
        const std::array<uint8_t, 1> marker = { 42 };
        return lock.valid() && redotchestrator::updater::write_update_file(path / "exclusive.txt", marker) ? 0 : 1;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--archive") {
        std::ifstream input(redotchestrator::updater::update_native_path(argv[2]), std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        std::vector<redotchestrator::updater::ArchiveEntry> entries;
        return redotchestrator::updater::inspect_archive(bytes, entries) ? 0 : 1;
    }
    using namespace redotchestrator::updater;

    expect(is_safe_plugin_archive_path("addons/orchestrator/orchestrator.gdextension"), "valid descriptor path");
    expect(is_safe_plugin_archive_path("addons/orchestrator/icons/Logo 16x16.svg"), "valid nested path");
    expect(is_safe_plugin_archive_path("addons/orchestrator/"), "valid addon directory entry");
    expect(!is_safe_plugin_archive_path("project.godot"), "reject outside addon");
    expect(!is_safe_plugin_archive_path("addons/orchestrator/../escape.txt"), "reject parent traversal");
    expect(!is_safe_plugin_archive_path("addons/orchestrator//escape.txt"), "reject empty segment");
    expect(!is_safe_plugin_archive_path("addons/orchestrator\\escape.txt"), "reject backslash path");
    expect(!is_safe_plugin_archive_path("addons/orchestrator/C:/escape.txt"), "reject drive path");
    expect(!is_safe_plugin_archive_path("addons/orchestrator/CON"), "reject Windows device name");
    expect(!is_safe_plugin_archive_path("addons/orchestrator/file. "), "reject ambiguous Windows suffix");

    expect(is_valid_sha256("0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEF"), "valid SHA-256");
    expect(!is_valid_sha256("0123456789abcdef"), "reject short digest");
    expect(!is_valid_sha256("g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), "reject non-hex digest");

    expect(is_valid_plugin_asset_name("redotchestrator-v2.5.stable-plugin.zip"), "valid asset name");
    expect(!is_valid_plugin_asset_name("godot-orchestrator-v2.5.stable-plugin.zip"), "reject upstream asset name");
    expect(!is_valid_plugin_asset_name("redotchestrator-../v2.5-plugin.zip"), "reject unsafe asset name");
    expect(!is_valid_plugin_asset_name("redotchestrator-next-plugin.zip"), "reject unversioned asset name");

    expect(is_valid_release_tag("v2.5.stable"), "valid short release tag");
    expect(is_valid_release_tag("v2.5.1.rc2"), "valid maintenance release tag");
    expect(!is_valid_release_tag("v2.5.beta"), "reject unsupported release status");
    expect(!is_valid_release_tag("v2.5.stable/escape"), "reject release tag path");
    expect(!is_valid_release_tag("v2.5.0.stable.extra"), "reject extra release tag component");

    expect(is_valid_redot_compatibility("v26.2.0"), "valid Redot compatibility");
    expect(!is_valid_redot_compatibility("26.2.0"), "reject unprefixed Redot compatibility");
    expect(!is_valid_redot_compatibility("v26.2.0.extra"), "reject extra Redot compatibility component");

    expect(is_redot_version_supported("v26.2.0", "v26.3.0", "v26.2.0"), "range accepts 26.2");
    expect(is_redot_version_supported("v26.2.0", "v26.3.0", "v26.3.0"), "range accepts 26.3");
    expect(!is_redot_version_supported("v26.2.0", "v26.3.0", "v26.3.1"), "reject untested later patch");
    expect(!is_redot_version_supported("v26.2.0", "v26.3.0", "v26.4.0"), "reject future minor");
    expect(!is_redot_version_supported("v26.2.0", "v26.3.0", "v4.5.2"), "reject inherited Godot version");
    expect(!is_redot_version_supported("v26.2.0", "v26.3.0", "v26.1.0"), "reject older runtime");
    expect(is_redot_version_supported("v26.2.0", "", "v26.2.1"), "legacy manifest accepts same-line patch");
    expect(!is_redot_version_supported("v26.2.0", "", "v26.3.0"), "legacy manifest rejects 26.3");
    expect(!is_valid_redot_range("v26.3.0", "v26.2.0"), "reject reversed range");
    expect(!is_valid_redot_range("v26.2.0", "v27.0.0"), "reject cross-major range");
    expect(!is_valid_redot_range("v26.2.0", "invalid"), "reject malformed maximum");
    expect(!is_valid_redot_range("v999999999999999999.2.0", ""), "reject numeric overflow");

    expect(is_allowed_release_asset_url(
        "https://github.com/dominicbytes/redotchestrator/releases/download/v2.5.stable/redotchestrator-v2.5.stable-plugin.zip",
        "v2.5.stable"),
        "valid release URL");
    expect(!is_allowed_release_asset_url(
        "https://github.com/CraterCrash/godot-orchestrator/releases/download/v2.5.stable/redotchestrator-v2.5.stable-plugin.zip",
        "v2.5.stable"),
        "reject upstream release URL");
    expect(!is_allowed_release_asset_url(
        "https://github.com/dominicbytes/redotchestrator/releases/download/v2.5.stable/redotchestrator-v2.5.stable-plugin.zip?raw=1",
        "v2.5.stable"),
        "reject release URL query");
    expect(!is_allowed_release_asset_url(
        "https://github.com/dominicbytes/redotchestrator/releases/download/v2.6.stable/redotchestrator-v2.6.stable-plugin.zip",
        "v2.5.stable"),
        "reject mismatched release tag");
    expect(!is_allowed_release_asset_url(
        "https://github.com/dominicbytes/redotchestrator/releases/download/v2.5.stable/extra/redotchestrator-v2.5.stable-plugin.zip",
        "v2.5.stable"),
        "reject extra URL path segment");

    expect(is_allowed_release_notes_url(
        "https://github.com/dominicbytes/redotchestrator/releases/tag/v2.5.stable"),
        "valid release notes URL");
    expect(!is_allowed_release_notes_url("file:///C:/Windows/System32/calc.exe"), "reject local release notes URL");

    expect(!is_valid_release_tag("v9223372036854775808.2.stable"), "reject release number overflow");
    expect(!is_valid_release_tag("v2.5.rc9223372036854775808"), "reject status number overflow");
    expect(!is_safe_plugin_archive_path((std::string("addons/orchestrator/a") + std::string("\0b", 2))), "reject embedded NUL");
    expect(!is_safe_plugin_archive_path("addons/orchestrator/caf\xc3\xa9.txt"), "reject Unicode path");

    expect(update_recovery(false, true, true, true) == UpdateRecovery::NONE, "orphan backup is preserved");
    expect(update_recovery(true, false, true, false) == UpdateRecovery::RESTORE_BACKUP, "recover missing target");
    expect(update_recovery(true, true, true, true) == UpdateRecovery::CONFIRM_STARTED, "confirm new version startup");
    expect(update_recovery(true, true, true, false) == UpdateRecovery::KEEP_PENDING, "old version cannot confirm new addon");
    expect(update_recovery(true, true, false, false) == UpdateRecovery::DISCARD_STAGING, "unstarted transaction needs cleanup");
    expect(update_recovery(true, false, false, false) == UpdateRecovery::KEEP_PENDING, "missing recovery data remains pending");

    // One ordinary stored ZIP, built here so malformed lengths never allocate large fixtures.
    std::vector<uint8_t> zip;
    const auto put = [&zip](uint32_t value, size_t size) {
        for (size_t byte = 0; byte < size; ++byte) { zip.push_back(static_cast<uint8_t>(value >> (byte * 8))); }
    };
    const std::string name = "addons/orchestrator/orchestrator.gdextension";
    const std::string payload = "descriptor";
    const uint32_t crc = archive_crc({ reinterpret_cast<const uint8_t*>(payload.data()), payload.size() });
    put(0x04034b50, 4); put(20, 2); put(0, 2); put(0, 2); put(0, 4); put(crc, 4);
    put(static_cast<uint32_t>(payload.size()), 4); put(static_cast<uint32_t>(payload.size()), 4);
    put(static_cast<uint32_t>(name.size()), 2); put(0, 2);
    zip.insert(zip.end(), name.begin(), name.end());
    zip.insert(zip.end(), payload.begin(), payload.end());
    const uint32_t central = static_cast<uint32_t>(zip.size());
    put(0x02014b50, 4); put(20, 2); put(20, 2); put(0, 2); put(0, 2); put(0, 4); put(crc, 4);
    put(static_cast<uint32_t>(payload.size()), 4); put(static_cast<uint32_t>(payload.size()), 4);
    put(static_cast<uint32_t>(name.size()), 2); put(0, 2); put(0, 2); put(0, 2); put(0, 2); put(0, 4); put(0, 4);
    zip.insert(zip.end(), name.begin(), name.end());
    const uint32_t central_size = static_cast<uint32_t>(zip.size()) - central;
    put(0x06054b50, 4); put(0, 2); put(0, 2); put(1, 2); put(1, 2); put(central_size, 4); put(central, 4); put(0, 2);
    std::vector<ArchiveEntry> entries;
    expect(inspect_archive(zip, entries) && entries.size() == 1, "accept bounded stored ZIP");
    expect(verify_archive_entry(entries[0], { zip.data() + entries[0].offset, entries[0].size }), "verify member CRC and length");
    expect(!verify_archive_entry(entries[0], {}), "read error cannot become empty file");
    auto changed = zip;
    changed[entries[0].offset] ^= 1;
    expect(!verify_archive_entry(entries[0], { changed.data() + entries[0].offset, entries[0].size }), "reject CRC mismatch");
    changed = zip;
    changed[central + 24 + 3] = 0x60;
    expect(!inspect_archive(changed, entries), "reject oversized metadata before allocation");
    changed = zip;
    changed[central + 38 + 3] = 0xa0;
    expect(!inspect_archive(changed, entries), "reject Unix symlink member");
    changed = zip;
    changed[central + 8] = 1;
    expect(!inspect_archive(changed, entries), "reject encrypted ZIP");
    changed = zip;
    changed.resize(changed.size() - 1);
    expect(!inspect_archive(changed, entries), "reject truncated ZIP");
    ArchiveEntry empty{ "addons/orchestrator/empty", 0, 0, 0, 0, 0 };
    expect(verify_archive_entry(empty, {}), "accept explicitly stored valid empty member");

    const auto verified_snapshot = zip;
    zip[30 + name.size()] ^= 1;
    expect(inspect_archive(verified_snapshot, entries)
        && verify_archive_entry(entries[0], { verified_snapshot.data() + entries[0].offset, entries[0].size }),
        "extract the verified snapshot despite replacement of the download buffer");
    const std::unordered_map<std::string, uint32_t> directory_library = { { "bin/plugin.so/not-a-library.txt", 1 },
        { "bin/ios.xcframework/plugin", 1 }, { "bin/ios.bundle/plugin", 1 }, { "bin/empty.so", 0 }, { "bin/valid.so", 1 } };
    expect(!has_update_library(directory_library, "linux.x86_64", "bin/plugin.so"), "reject directory in place of desktop library");
    expect(!has_update_library(directory_library, "macos", "bin/ios.xcframework"), "reject desktop xcframework bundle");
    expect(!has_update_library(directory_library, "ios.extra", "bin/ios.xcframework"), "reject unknown iOS bundle feature");
    expect(!has_update_library(directory_library, "ios", "bin/ios.bundle"), "reject non-xcframework directory");
    expect(has_update_library(directory_library, "ios", "bin/ios.xcframework"), "accept iOS xcframework");
    expect(has_update_library(directory_library, "ios.debug", "bin/ios.xcframework"), "accept debug iOS xcframework");
    expect(has_update_library(directory_library, "linux.x86_64", "bin/valid.so"), "accept exact nonempty library");
    expect(!has_update_library(directory_library, "linux.x86_64", "bin/empty.so"), "reject empty library");

    const auto test_directory = reserve_test_directory();
    expect(!test_directory.empty(), "exclusively reserve native filesystem test directory");
    if (test_directory.empty()) { return 1; }
    std::error_code filesystem_error;
    const auto output = test_directory / "exclusive.txt";
    const std::array<uint8_t, 1> marker = { 42 };
    expect(write_update_file(output, marker), "create staged file exclusively");
    expect(!write_update_file(output, marker), "never reopen an existing staged file");
#ifdef _WIN32
    {
        const auto stage = test_directory / "stage";
        expect(std::filesystem::create_directory(stage, filesystem_error), "create owned staging test directory");
        expect(write_update_file(stage / "verified.txt", marker, test_directory), "write verified stage payload");
        UpdateDirectoryLock lock(stage, test_directory);
        expect(lock.valid(), "pin staging root");
        const auto replacement = test_directory / "renamed-stage";
        expect(!std::filesystem::exists(replacement), "pin test destination starts absent");
        std::filesystem::rename(stage, replacement, filesystem_error);
        expect(bool(filesystem_error), "pinned root cannot be substituted during extraction");
        if (!filesystem_error) {
            std::filesystem::rename(replacement, stage, filesystem_error);
        }
        auto rename_info = UpdateDirectoryLock::prepare_rename(replacement);
        expect(lock.rename_to(rename_info), "rename the pinned stage by handle");
        expect(!std::filesystem::exists(stage) && plain_update_directory(replacement), "activate pinned directory identity");
        std::ifstream installed(replacement / "verified.txt", std::ios::binary);
        expect(installed.get() == marker[0], "activate the verified payload");
    }
#endif
    expect(remove_update_tree(test_directory), "clean native filesystem test directory");

    if (failures == 0) {
        std::cout << "redotchestrator updater security tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

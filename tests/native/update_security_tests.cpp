#include "editor/updater/update_security.h"

#include <iostream>
#include <string_view>

namespace {
    int failures = 0;

    void expect(bool p_condition, std::string_view p_name) {
        if (!p_condition) {
            std::cerr << "FAIL: " << p_name << '\n';
            ++failures;
        }
    }
}

int main() {
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

    if (failures == 0) {
        std::cout << "redotchestrator updater security tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

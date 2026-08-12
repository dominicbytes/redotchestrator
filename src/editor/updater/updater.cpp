// This file is part of the Godot Orchestrator project.
//
// Copyright (c) 2023-present Crater Crash Studios LLC and its contributors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//		http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "editor/updater/updater.h"

#include "common/callable_lambda.h"
#include "common/godot_version.h"
#include "common/macros.h"
#include "common/scene_utils.h"
#include "common/settings.h"
#include "common/string_utils.h"
#include "core/godot/scene_string_names.h"
#include "editor/plugins/orchestrator_editor_plugin.h"
#include "editor/updater/update_security.h"

#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/editor_paths.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/link_button.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/zip_reader.hpp>

#include <limits>
#include <string_view>

namespace {
    constexpr int64_t MAX_UPDATE_ASSET_SIZE = 1024LL * 1024LL * 1024LL;
    constexpr int64_t MAX_EXTRACTED_SIZE = 1024LL * 1024LL * 1024LL;

    std::string_view as_utf8_view(const CharString& p_string) {
        return { p_string.get_data(), static_cast<size_t>(p_string.length()) };
    }

    bool is_safe_archive_path(const String& p_path) {
        const CharString utf8 = p_path.utf8();
        return redotchestrator::updater::is_safe_plugin_archive_path(as_utf8_view(utf8));
    }

    bool is_valid_sha256(const String& p_digest) {
        const CharString utf8 = p_digest.utf8();
        return redotchestrator::updater::is_valid_sha256(as_utf8_view(utf8));
    }

    bool is_valid_asset_name(const String& p_name) {
        const CharString utf8 = p_name.utf8();
        return redotchestrator::updater::is_valid_plugin_asset_name(as_utf8_view(utf8));
    }

    bool is_valid_release_tag(const String& p_tag) {
        const CharString utf8 = p_tag.utf8();
        return redotchestrator::updater::is_valid_release_tag(as_utf8_view(utf8));
    }

    bool is_plugin_asset_name_for_tag(const String& p_name, const String& p_tag) {
        const CharString name_utf8 = p_name.utf8();
        const CharString tag_utf8 = p_tag.utf8();
        return redotchestrator::updater::is_plugin_asset_name_for_tag(
            as_utf8_view(name_utf8), as_utf8_view(tag_utf8));
    }

    bool is_valid_redot_compatibility(const String& p_version) {
        const CharString utf8 = p_version.utf8();
        return redotchestrator::updater::is_valid_redot_compatibility(as_utf8_view(utf8));
    }

    bool is_allowed_asset_url(const String& p_url, const String& p_tag) {
        const CharString utf8 = p_url.utf8();
        const CharString tag_utf8 = p_tag.utf8();
        return redotchestrator::updater::is_allowed_release_asset_url(
            as_utf8_view(utf8), as_utf8_view(tag_utf8));
    }

    bool is_allowed_release_notes_url(const String& p_url) {
        const CharString utf8 = p_url.utf8();
        return redotchestrator::updater::is_allowed_release_notes_url(as_utf8_view(utf8));
    }

    Error remove_tree(const String& p_path) {
        if (FileAccess::file_exists(p_path)) {
            return DirAccess::remove_absolute(p_path);
        }
        if (!DirAccess::dir_exists_absolute(p_path)) {
            return OK;
        }

        Ref<DirAccess> directory = DirAccess::open(p_path);
        if (directory.is_null()) {
            return DirAccess::get_open_error();
        }
        directory->set_include_hidden(true);
        directory->set_include_navigational(false);
        Error error = directory->list_dir_begin();
        if (error != OK) {
            return error;
        }

        for (String name = directory->get_next(); !name.is_empty(); name = directory->get_next()) {
            const String child_path = p_path.path_join(name);
            const bool is_directory = directory->current_is_dir();
            const bool is_link = directory->is_link(name);
            error = is_directory && !is_link ? remove_tree(child_path) : DirAccess::remove_absolute(child_path);
            if (error != OK) {
                directory->list_dir_end();
                return error;
            }
        }
        directory->list_dir_end();
        return DirAccess::remove_absolute(p_path);
    }

    String get_addon_path() {
        return ProjectSettings::get_singleton()->globalize_path("res://addons/orchestrator").simplify_path();
    }

    String get_update_stage_path() {
        return get_addon_path().get_base_dir().path_join(".redotchestrator-update-stage");
    }

    String get_update_backup_path() {
        return get_addon_path().get_base_dir().path_join(".redotchestrator-update-backup");
    }
}

OrchestratorVersion::Build OrchestratorVersion::Build::parse(const String& p_build) {
    int pos = 0;
    while (pos < p_build.length() && !String::chr(p_build[pos]).is_valid_int()) {
        pos++;
    }
    Build build;
    build.name = p_build.substr(0, pos);
    build.version = p_build.substr(pos).to_int();
    return build;
}

String OrchestratorVersion::Build::to_string() const {
    return version == 0 ? name : vformat("%s%d", name, version);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OrchestratorVersion

OrchestratorVersion OrchestratorVersion::parse(const String& p_tag_version) {
    OrchestratorVersion version;

    const PackedStringArray parts = (p_tag_version.begins_with("v")
        ? p_tag_version.substr(1).split(".")
        : p_tag_version.split("."));

    if (parts.size() >= 1 && parts[0].is_valid_int()) {
        version.major = parts[0].to_int();
    }

    if (parts.size() >= 2 && parts[1].is_valid_int()) {
        version.minor = parts[1].to_int();
    }

    if (parts.size() >= 3 && parts[2].is_valid_int()) {
        version.patch = parts[2].to_int();
    } else if (parts.size() >= 3) {
        version.build = Build::parse(parts[2]);
    }

    if (parts.size() >= 4) {
        version.build = Build::parse(parts[3]);
    }

    return version;
}

bool OrchestratorVersion::is_after(const OrchestratorVersion& p_other) const {
    // List of builds that are in "release" order, i.e. stable comes first, development last, etc.
    static PackedStringArray builds = Array::make("stable", "rc", "dev");

    // This major version is after the other
    if (major > p_other.major) {
        return true;
    }

    // This minor version is after the other
    if (major == p_other.major && minor > p_other.minor) {
        return true;
    }

    // This patch version is after the other
    if (major == p_other.major && minor == p_other.minor && patch > p_other.patch) {
        return true;
    }

    if (major == p_other.major && minor == p_other.minor && patch == p_other.patch) {
        const int64_t build_name_index = builds.find(build.name);
        const int64_t other_build_name_index = builds.find(p_other.build.name);
        if (build_name_index < other_build_name_index) {
            return true;
        }

        if (build_name_index == other_build_name_index && build.version > p_other.build.version) {
            return true;
        }
    }

    return false;
}

bool OrchestratorVersion::is_equal(const OrchestratorVersion& p_other) const {
    return major == p_other.major
        && minor == p_other.minor
        && patch == p_other.patch
        && build.name == p_other.build.name
        && build.version == p_other.build.version;
}

bool OrchestratorVersion::is_compatible(const OrchestratorVersion& p_other) const {
    // Redot releases use a year-like major line. Compatibility is intentionally
    // limited to the manifest's major/minor line unless a later manifest says otherwise.
    return p_other.major == major && p_other.minor == minor && p_other.patch >= patch;
}

String OrchestratorVersion::to_string() const {
    return vformat("%d.%d.%d.%s", major, minor, patch, build.to_string());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OrchestratorUpdaterReleaseNotesDialog

void OrchestratorUpdaterReleaseNotesDialog::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY) {
        connect(SceneStringName(canceled), callable_mp_lambda(this, [this] { queue_free(); }));
        connect(SceneStringName(confirmed), callable_mp_lambda(this, [this] { queue_free(); }));
    }
}

OrchestratorUpdaterReleaseNotesDialog::OrchestratorUpdaterReleaseNotesDialog() {
    set_title("Release Notes");

    _text = memnew(RichTextLabel);
    _text->set_use_bbcode(true);
    _text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    _text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(_text);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OrchestratorUpdaterVersionPicker

void OrchestratorUpdaterVersionPicker::_set_button_enable_state(bool p_enabled) {
    get_ok_button()->set_disabled(!p_enabled);
    _show_release_notes->set_disabled(!p_enabled);
}

void OrchestratorUpdaterVersionPicker::_check_redot_compatibility() {
    TreeItem* selected = _tree->get_selected();
    if (selected) {
        if (!selected->get_meta("compatible", false)) {
            AcceptDialog* notify = memnew(AcceptDialog);
            notify->set_title("Redot version incompatible");
            notify->set_text("This Redotchestrator release does not support your current Redot version.");
            add_child(notify);
            notify->connect(SceneStringName(canceled), callable_mp_lambda(this, [notify] { notify->queue_free(); }));
            notify->connect(SceneStringName(confirmed), callable_mp_lambda(this, [notify] { notify->queue_free(); }));
            notify->popup_centered();
            return;
        }

        _request_download();
    }
}

void OrchestratorUpdaterVersionPicker::_request_download() {
    TreeItem* selected = _tree->get_selected();
    if (selected) {
        const String download_url = selected->get_meta("download_url");
        const String sha256 = selected->get_meta("sha256");
        const int64_t asset_size = selected->get_meta("asset_size", 0);
        const String tag = selected->get_meta("tag");
        if (!is_allowed_asset_url(download_url, tag)
            || !is_valid_sha256(sha256)
            || asset_size <= 0
            || asset_size > MAX_UPDATE_ASSET_SIZE
            || tag.is_empty()) {
            _show_failure("The selected release failed Redotchestrator's integrity policy.");
            return;
        }

        _expected_sha256 = sha256.to_lower();
        _expected_asset_size = asset_size;
        _expected_tag = tag;

        get_ok_button()->release_focus();
        _set_button_enable_state(false);
        _tree->deselect_all();

        DirAccess::remove_absolute(_download->get_download_file());
        _download->set_body_size_limit(static_cast<int32_t>(asset_size + 1));
        const Error error = _download->request(download_url);
        if (error == OK) {
            _progress->set_indeterminate(true);
            set_process(true);
        } else {
            _show_failure(vformat("Unable to start the download (error %d).", error));
        }
    }
}

void OrchestratorUpdaterVersionPicker::_handle_custom_action(const StringName& p_action) {
    if (p_action.match("show_release_notes")) {
        OS::get_singleton()->shell_open(_tree->get_selected()->get_meta("release_url"));
        // OrchestratorUpdaterReleaseNotesDialog* dialog = memnew(OrchestratorUpdaterReleaseNotesDialog);
        // dialog->set_text(_tree->get_selected()->get_meta("release_notes"));
        // dialog->set_exclusive(true);
        // dialog->set_transient(true);
        // dialog->popup_exclusive_centered_ratio(this, 0.3);
    }
}

void OrchestratorUpdaterVersionPicker::_download_completed(int p_result, int p_code, const PackedStringArray& p_headers, const PackedByteArray& p_data) {
    static_cast<void>(p_headers);
    static_cast<void>(p_data);

    _progress->set_visible(false);
    _progress->set_indeterminate(false);

    set_process(false);

    if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
        _show_failure(vformat("Download failed (result %d, HTTP %d).", p_result, p_code));
        return;
    }

    const String file_name = _download->get_download_file();
    Ref<FileAccess> downloaded_file = FileAccess::open(file_name, FileAccess::READ);
    if (downloaded_file.is_null() || downloaded_file->get_length() != _expected_asset_size) {
        DirAccess::remove_absolute(file_name);
        _show_failure("The downloaded file size does not match the trusted release manifest.");
        return;
    }

    const String actual_sha256 = FileAccess::get_sha256(file_name).to_lower();
    if (actual_sha256 != _expected_sha256) {
        DirAccess::remove_absolute(file_name);
        _show_failure("The downloaded file failed SHA-256 verification and was removed.");
        return;
    }

    _install();
}

void OrchestratorUpdaterVersionPicker::_show_failure(const String& p_message) {
    set_process(false);
    _progress->set_visible(false);
    _progress->set_indeterminate(false);
    _status->set_visible(true);
    _status->set_text(p_message);
    _set_button_enable_state(false);
}

void OrchestratorUpdaterVersionPicker::_restart_editor() {
    EI->restart_editor(true);
}

void OrchestratorUpdaterVersionPicker::_install() {
    _status->set_visible(true);
    _status->set_text("Installing, please wait...");
    const String file_name = _download->get_download_file();

    Ref<ZIPReader> reader = memnew(ZIPReader);
    if (reader->open(file_name) != OK) {
        _show_failure("The verified download is not a readable ZIP archive.");
        return;
    }

    const String target_path = get_addon_path();
    const String stage_path = get_update_stage_path();
    const String backup_path = get_update_backup_path();
    const auto fail_install = [this, &reader, &stage_path](const String& p_message) {
        reader->close();
        remove_tree(stage_path);
        _show_failure(p_message);
    };

    Error error = remove_tree(stage_path);
    if (error != OK || DirAccess::make_dir_recursive_absolute(stage_path) != OK) {
        fail_install("Unable to create a clean staging directory for the update.");
        return;
    }

    const PackedStringArray files = reader->get_files();
    HashMap<String, bool> extracted_paths;
    int64_t extracted_size = 0;
    int64_t extracted_file_count = 0;
    bool has_extension_descriptor = false;
    const String archive_prefix = "addons/orchestrator/";

    for (const String& file : files) {
        if (!is_safe_archive_path(file)) {
            fail_install(vformat("The archive contains a forbidden path: %s", file));
            return;
        }

        const String relative_path = file.substr(archive_prefix.length());
        if (relative_path.is_empty()) {
            continue;
        }

        const String collision_key = relative_path.to_lower();
        if (extracted_paths.has(collision_key)) {
            fail_install(vformat("The archive contains a duplicate path: %s", file));
            return;
        }
        extracted_paths[collision_key] = true;

        const String destination = stage_path.path_join(relative_path);
        if (file.ends_with("/")) {
            if (DirAccess::make_dir_recursive_absolute(destination) != OK) {
                fail_install(vformat("Unable to stage directory: %s", relative_path));
                return;
            }
            continue;
        }

        const PackedByteArray contents = reader->read_file(file);
        extracted_size += contents.size();
        if (extracted_size > MAX_EXTRACTED_SIZE) {
            fail_install("The archive exceeds Redotchestrator's extracted-size limit.");
            return;
        }

        if (DirAccess::make_dir_recursive_absolute(destination.get_base_dir()) != OK) {
            fail_install(vformat("Unable to create the staging path for: %s", relative_path));
            return;
        }

        Ref<FileAccess> output = FileAccess::open(destination, FileAccess::WRITE);
        if (output.is_null() || !output->is_open()) {
            fail_install(vformat("Unable to stage file: %s", relative_path));
            return;
        }
        output->store_buffer(contents);
        const Error write_error = output->get_error();
        output->close();
        if (write_error != OK) {
            fail_install(vformat("Unable to finish writing staged file: %s", relative_path));
            return;
        }

        ++extracted_file_count;
        has_extension_descriptor = has_extension_descriptor || relative_path == "orchestrator.gdextension";
    }
    reader->close();

    if (extracted_file_count == 0 || !has_extension_descriptor) {
        remove_tree(stage_path);
        _show_failure("The archive is not a complete Redotchestrator plugin package.");
        return;
    }

    error = remove_tree(backup_path);
    if (error != OK) {
        remove_tree(stage_path);
        _show_failure("Unable to clear the previous update backup. No project files were changed.");
        return;
    }

    error = DirAccess::rename_absolute(target_path, backup_path);
    if (error != OK) {
        remove_tree(stage_path);
        _show_failure("Unable to back up the installed plugin. No project files were changed.");
        return;
    }

    error = DirAccess::rename_absolute(stage_path, target_path);
    if (error != OK) {
        const Error rollback_error = DirAccess::rename_absolute(backup_path, target_path);
        if (rollback_error == OK) {
            remove_tree(stage_path);
        }
        const String message = rollback_error == OK
            ? "Unable to activate the staged update; the previous plugin was restored."
            : vformat("Update and automatic rollback failed. Restore '%s' to '%s' before reopening the project.",
                backup_path, target_path);
        _show_failure(message);
        return;
    }

    DirAccess::remove_absolute(file_name);
    _status->set_visible(false);

    AcceptDialog* dialog = memnew(AcceptDialog);
    dialog->set_title("Redotchestrator Update Installed");
    dialog->set_text(vformat(
        "Redotchestrator %s was verified, staged, and installed atomically. Restart Redot to load it.",
        _expected_tag));
    dialog->set_ok_button_text("Restart");
    add_child(dialog);

    dialog->connect(SceneStringName(confirmed), callable_mp_lambda(this, [this, dialog] {
        dialog->queue_free();

        Timer* timer = memnew(Timer);
        timer->set_one_shot(true);
        timer->set_wait_time(0.5f);
        timer->set_autostart(true);
        timer->connect("timeout", callable_mp_this(_restart_editor));
        add_child(timer);
    }));

    emit_signal("install_completed");
    dialog->popup_centered();
}

void OrchestratorUpdaterVersionPicker::_cancel_and_close() {
    if (_download) {
        const int status = _download->get_http_client_status();
        if (status == HTTPClient::STATUS_BODY) {
            set_process(false);

            _download->cancel_request();

            _progress->set_indeterminate(false);
            _progress->set_visible(false);

            _status->set_visible(false);
        }
    }

    hide();
}

void OrchestratorUpdaterVersionPicker::_filter_changed(int p_index) {
    _update_tree(p_index == 1);
}

void OrchestratorUpdaterVersionPicker::_update_tree(bool p_stable_only) {
    _tree->clear();
    _tree->create_item();
    for (const ReleaseItem& release_item : _releases) {
        if (p_stable_only) {
            OrchestratorVersion tag_version = OrchestratorVersion::parse(release_item.release.tag);
            if (!tag_version.build.is_stable()) {
                continue;
            }
        }

        int64_t unix_time = Time::get_singleton()->get_unix_time_from_datetime_string(release_item.release.published);

        TreeItem* item = _tree->get_root()->create_child();
        item->set_text(0, release_item.release.tag);
        item->set_text(1, release_item.manifest.redot_compatibility);
        item->set_text(2, vformat("%s", release_item.release.prerelease ? "Yes" : "No"));
        item->set_text(3, Time::get_singleton()->get_datetime_string_from_unix_time(unix_time, true));
        item->set_text(4, String::humanize_size(release_item.release.asset_size));

        item->set_meta("download_url", release_item.release.plugin_asset_url);
        item->set_meta("sha256", release_item.manifest.sha256);
        item->set_meta("asset_size", release_item.manifest.asset_size);
        item->set_meta("tag", release_item.release.tag);

        const String release_url = StringUtils::default_if_empty(release_item.blog_url, release_item.release.release_url);
        item->set_meta("release_url", release_url);

        const OrchestratorVersion compat_version = OrchestratorVersion::parse(release_item.manifest.redot_compatibility);
        if (!compat_version.is_compatible(_godot_version)) {
            item->add_button(0, SceneUtils::get_editor_icon("KeyXScale"), -1, true, "Your Redot version is not compatible");
            item->set_meta("compatible", false);
        } else {
            item->add_button(0, SceneUtils::get_editor_icon("KeyCall"));
            item->set_meta("compatible", true);
        }
    }
}

void OrchestratorUpdaterVersionPicker::_update_notify_settings() {
    OrchestratorSettings* settings = OrchestratorSettings::get_singleton();
    settings->set_notify_prerelease_builds(_notify_any_release->is_pressed());
}

void OrchestratorUpdaterVersionPicker::update_tree() {
    _update_tree(_release_filter->get_selected() == 1);
}

void OrchestratorUpdaterVersionPicker::clear_releases() {
    _set_button_enable_state(false);
    _releases.clear();
}

void OrchestratorUpdaterVersionPicker::add_release(const OrchestratorRelease& p_release, const OrchestratorReleaseManifest& p_manifest, const String& p_blog_url) {
    ReleaseItem item;
    item.release = p_release;
    item.manifest = p_manifest;
    item.blog_url = p_blog_url;
    _releases.push_back(item);
}

void OrchestratorUpdaterVersionPicker::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (is_visible()) {
                OrchestratorSettings* settings = OrchestratorSettings::get_singleton();
                _notify_any_release->set_pressed_no_signal(settings->is_notify_about_prereleases());

                _update_tree();

                _tree->deselect_all();

                get_ok_button()->release_focus();
                _set_button_enable_state(false);

                _progress->set_value_no_signal(0);
            }
            break;
        }
        case NOTIFICATION_READY: {
            _download->connect("request_completed", callable_mp_this(_download_completed));
            _release_filter->connect(SceneStringName(item_selected), callable_mp_this(_filter_changed));
            _notify_any_release->connect(SceneStringName(pressed), callable_mp_this(_update_notify_settings));
            _tree->connect(SceneStringName(item_activated), callable_mp_this(_check_redot_compatibility));
            _tree->connect(SceneStringName(item_selected), callable_mp_this(_set_button_enable_state).bind(true));

            connect("custom_action", callable_mp_this(_handle_custom_action));
            connect(SceneStringName(confirmed), callable_mp_this(_check_redot_compatibility));
            connect(SceneStringName(canceled), callable_mp_this(_cancel_and_close));
            break;
        }
        case NOTIFICATION_PROCESS: {
            // Make the progress bar visible again when retrying the download.
            _progress->set_visible(true);
            _status->set_visible(true);

            if (_download->get_downloaded_bytes() > 0) {
                _progress->set_max(_download->get_body_size());
                _progress->set_value(_download->get_downloaded_bytes());
            }

            int client_status = _download->get_http_client_status();
            if (client_status == HTTPClient::STATUS_BODY) {
                if (_download->get_body_size() > 0) {
                    _progress->set_indeterminate(false);
                    _status->set_text(vformat("Downloading (%s / %s)...",
                        String::humanize_size(_download->get_downloaded_bytes()),
                        String::humanize_size(_download->get_body_size())));
                } else {
                    _progress->set_indeterminate(true);
                    _status->set_text(vformat("Downloading... (%s)",
                        String::humanize_size(_download->get_downloaded_bytes())));
                }
            }
            break;
        }
    }
}

void OrchestratorUpdaterVersionPicker::_bind_methods() {
    ADD_SIGNAL(MethodInfo("install_completed"));
}

OrchestratorUpdaterVersionPicker::OrchestratorUpdaterVersionPicker() {
    GodotVersionInfo gd_version;

    // Generate the editor's current Redot version
    // Used in compatibility checks
    _godot_version = OrchestratorVersion::parse(vformat(
        "v%d.%d.%d", gd_version.major(), gd_version.minor(), gd_version.patch()));

    set_title("Select Redotchestrator Version");

    set_ok_button_text("Download & Install");
    set_cancel_button_text("Close");
    set_hide_on_ok(false);

    _show_release_notes = add_button("Show Release Notes", false, "show_release_notes");

    VBoxContainer* vbox = memnew(VBoxContainer);
    add_child(vbox);

    HBoxContainer* hbox = memnew(HBoxContainer);
    vbox->add_child(hbox);

    _release_filter = memnew(OptionButton);
    _release_filter->add_item("All releases");
    _release_filter->add_item("Stable only");
    SceneUtils::add_margin_child(hbox, "Filter:", _release_filter);

    Label* spacer = memnew(Label);
    spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hbox->add_child(spacer);

    _notify_any_release = memnew(CheckBox);
    _notify_any_release->set_text("Notify about pre-release versions");
    _notify_any_release->set_focus_mode(Control::FOCUS_NONE);
    hbox->add_child(_notify_any_release);

    _tree = memnew(Tree);
    _tree->set_hide_root(true);
    _tree->set_select_mode(Tree::SELECT_ROW);
    _tree->set_columns(5);
    _tree->set_column_titles_visible(true);
    _tree->set_column_title(0, "Version");
    _tree->set_column_title_alignment(0, HORIZONTAL_ALIGNMENT_LEFT);
    _tree->set_column_title(1, "Redot Compatibility");
    _tree->set_column_title_alignment(1, HORIZONTAL_ALIGNMENT_LEFT);
    _tree->set_column_title(2, "Pre-release");
    _tree->set_column_title_alignment(2, HORIZONTAL_ALIGNMENT_LEFT);
    _tree->set_column_title(3, "Published");
    _tree->set_column_title_alignment(3, HORIZONTAL_ALIGNMENT_LEFT);
    _tree->set_column_title(4, "Size");
    _tree->set_column_title_alignment(4, HORIZONTAL_ALIGNMENT_LEFT);
    _tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    _tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    vbox->add_child(_tree);

    _progress = memnew(ProgressBar);
    _progress->set_visible(false);
    vbox->add_child(_progress);

    _status = memnew(Label);
    _status->set_visible(false);
    vbox->add_child(_status);

    _download = memnew(HTTPRequest);
    const String cache_dir = EI->get_editor_paths()->get_cache_dir();
    _download->set_download_file(cache_dir.path_join("tmp_redotchestrator_update.zip"));
    _download->set_use_threads(true);
    _download->set_accept_gzip(false);
    _download->set_timeout(60.0);
    add_child(_download);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OrchestratorUpdaterButton

Error OrchestratorUpdaterButton::_send_http_request(const String& p_url, const String& p_filename, const Callable& p_callback) {
    if (p_url != VERSION_RELEASES_URL && p_url != VERSION_MANIFESTS_URL) {
        return ERR_INVALID_PARAMETER;
    }

    DirAccess::remove_absolute(p_filename);
    HTTPRequest* request = memnew(HTTPRequest);
    request->set_download_file(p_filename);
    request->set_use_threads(true);
    request->set_accept_gzip(false);
    request->set_body_size_limit(8 * 1024 * 1024);
    request->set_max_redirects(2);
    request->set_timeout(30.0);
    add_child(request);

    request->connect(
        "request_completed",
        callable_mp_lambda(this, [=] (int p_result, int p_code, const PackedStringArray& p_headers, const PackedByteArray& p_data) {
            static_cast<void>(p_headers);
            static_cast<void>(p_data);
            if (p_result == HTTPRequest::RESULT_SUCCESS && p_code == 200) {
                p_callback.call();
            } else {
                DirAccess::remove_absolute(p_filename);
            }
            request->queue_free();
        }),
        CONNECT_ONE_SHOT);

    PackedStringArray headers;
    headers.push_back("Accept: application/vnd.github+json");
    headers.push_back("X-GitHub-Api-Version: 2022-11-28");
    headers.push_back("User-Agent: redotchestrator-updater/2.5");
    const Error error = request->request(p_url, headers);
    if (error != OK) {
        request->queue_free();
    }

    return error;
}

void OrchestratorUpdaterButton::_process_release_manifests() {
    _manifests.clear();

    const String cache_dir = EI->get_editor_paths()->get_cache_dir();
    const PackedByteArray bytes = FileAccess::get_file_as_bytes(cache_dir.path_join("tmp_redotchestrator_release_manifests.json"));
    const Variant parsed = JSON::parse_string(bytes.get_string_from_utf8());
    if (parsed.get_type() != Variant::ARRAY) {
        return;
    }
    const Array data = parsed;

    for (int index = 0; index < data.size(); ++index) {
        if (data[index].get_type() != Variant::DICTIONARY) {
            continue;
        }
        const Dictionary& release = data[index];
        if (release.has("version")
            && release.has("redot_compatibility")
            && release.has("asset_name")
            && release.has("sha256")
            && release.has("asset_size")) {
            OrchestratorReleaseManifest manifest;
            manifest.name = release["version"];
            manifest.redot_compatibility = release["redot_compatibility"];
            manifest.asset_name = release["asset_name"];
            manifest.sha256 = String(release["sha256"]).to_lower();
            manifest.asset_size = release["asset_size"];

            if (release.has("blog_url") && is_allowed_release_notes_url(release["blog_url"])) {
                manifest.blog_url = release["blog_url"];
            }

            const OrchestratorVersion compatibility = OrchestratorVersion::parse(manifest.redot_compatibility);
            if (!is_valid_release_tag(manifest.name)
                || !is_valid_redot_compatibility(manifest.redot_compatibility)
                || compatibility.major <= 0
                || compatibility.minor < 0
                || !is_plugin_asset_name_for_tag(manifest.asset_name, manifest.name)
                || !is_valid_sha256(manifest.sha256)
                || manifest.asset_size <= 0
                || manifest.asset_size > MAX_UPDATE_ASSET_SIZE) {
                continue;
            }

            _manifests[manifest.name] = manifest;
        }
    }

    if (!_manifests.is_empty() && !_releases.is_empty()) {
        _update_picker();
    }
}

void OrchestratorUpdaterButton::_process_releases() {
    _releases.clear();

    const String cache_dir = EI->get_editor_paths()->get_cache_dir();
    const PackedByteArray bytes = FileAccess::get_file_as_bytes(cache_dir.path_join("tmp_redotchestrator_releases.json"));
    const Variant parsed = JSON::parse_string(bytes.get_string_from_utf8());
    if (parsed.get_type() != Variant::ARRAY) {
        return;
    }
    const Array data = parsed;

    for (int index = 0; index < data.size(); ++index) {
        if (data[index].get_type() != Variant::DICTIONARY) {
            continue;
        }
        const Dictionary& published_release = data[index];
        if (!published_release.has("tag_name")
            || !published_release.has("html_url")
            || !published_release.has("body")
            || !published_release.has("draft")
            || !published_release.has("prerelease")
            || !published_release.has("published_at")
            || !published_release.has("assets")
            || published_release["assets"].get_type() != Variant::ARRAY) {
            continue;
        }

        OrchestratorRelease release;
        release.tag = published_release["tag_name"];
        release.release_url = published_release["html_url"];
        release.body = published_release["body"];
        release.draft = published_release["draft"];
        release.prerelease = published_release["prerelease"];
        release.published = published_release["published_at"];
        if (!is_valid_release_tag(release.tag) || !is_allowed_release_notes_url(release.release_url)) {
            continue;
        }

        const Array assets = published_release["assets"];
        if (!assets.is_empty()) {
            for (int asset_index = 0; asset_index < assets.size(); ++asset_index) {
                if (assets[asset_index].get_type() != Variant::DICTIONARY) {
                    continue;
                }
                const Dictionary& asset_release = assets[asset_index];
                if (!asset_release.has("name")
                    || !asset_release.has("browser_download_url")
                    || !asset_release.has("digest")
                    || !asset_release.has("size")) {
                    continue;
                }

                const String name = asset_release["name"];
                const String download_url = asset_release["browser_download_url"];
                const String digest = asset_release["digest"];
                const int64_t size = asset_release["size"];
                if (is_plugin_asset_name_for_tag(name, release.tag)
                    && is_allowed_asset_url(download_url, release.tag)
                    && digest.begins_with("sha256:")
                    && is_valid_sha256(digest.substr(7))
                    && size > 0
                    && size <= MAX_UPDATE_ASSET_SIZE) {
                    release.plugin_asset_name = name;
                    release.plugin_asset_url = download_url;
                    release.plugin_asset_digest = digest.substr(7).to_lower();
                    release.asset_size = size;
                    break;
                }
            }
        }

        // If it has no download artifact, skip it
        if (release.plugin_asset_url.is_empty()) {
            continue;
        }

        if (!OrchestratorVersion::parse(release.tag).is_after(_plugin_version)) {
            continue;
        }

        _releases.push_back(release);
    }

    if (!_manifests.is_empty() && !_releases.is_empty()) {
        _update_picker();
    }
}

void OrchestratorUpdaterButton::_update_picker() {
    if (_releases.is_empty() || _manifests.is_empty()) {
        return;
    }

    OrchestratorSettings* settings=  OrchestratorSettings::get_singleton();
    const bool notify_pre_releases = settings->is_notify_about_prereleases();

    _picker->clear_releases();

    bool releases_added = false;
    for (const OrchestratorRelease& release : _releases) {
        // If the release is marked as Draft or Prerelease+NoNotify from GitHub, skip.
        if (release.draft || (release.prerelease && !notify_pre_releases)) {
            continue;
        }

        // In case a dev/rc build is not marked pre-release but the user wants only stable releases,
        // check the build name and filter as a last resort.
        OrchestratorVersion version = OrchestratorVersion::parse(release.tag);
        if(!version.build.is_stable() && !notify_pre_releases) {
            continue;
        }

        if (!_manifests.has(release.tag)) {
            continue;
        }

        const OrchestratorReleaseManifest manifest = _manifests.get(release.tag);
        if (manifest.asset_name != release.plugin_asset_name
            || manifest.asset_size != release.asset_size
            || manifest.sha256 != release.plugin_asset_digest) {
            continue;
        }
        _picker->add_release(release, manifest, manifest.blog_url);

        releases_added = true;
    }

    set_visible(releases_added);

    if (releases_added) {
        _button->set_text("An update is available!");
    }

    if (_picker->is_visible()) {
        _picker->update_tree();
    }
}

void OrchestratorUpdaterButton::_show_update_dialog() {
    _picker->popup_centered_ratio(0.4);
}

void OrchestratorUpdaterButton::_check_for_updates() {
    const String cache_dir = EI->get_editor_paths()->get_cache_dir();

    const String releases_path = cache_dir.path_join("tmp_redotchestrator_releases.json");
    _send_http_request(VERSION_RELEASES_URL, releases_path, callable_mp_this(_process_releases));

    const String manifests_path = cache_dir.path_join("tmp_redotchestrator_release_manifests.json");
    _send_http_request(VERSION_MANIFESTS_URL, manifests_path, callable_mp_this(_process_release_manifests));
}

void OrchestratorUpdaterButton::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            set_visible(false);

            const Error stage_cleanup = remove_tree(get_update_stage_path());
            if (stage_cleanup != OK) {
                WARN_PRINT(vformat("Unable to remove stale Redotchestrator update staging directory (error %d).", stage_cleanup));
            }
            const Error backup_cleanup = remove_tree(get_update_backup_path());
            if (backup_cleanup != OK) {
                WARN_PRINT(vformat("Unable to remove the previous Redotchestrator update backup (error %d).", backup_cleanup));
            }

            if (DisplayServer::get_singleton()->get_name() == "headless") {
                break;
            }

            Timer* timer = memnew(Timer);
            timer->set_wait_time(60 * 60); // every hour
            timer->set_autostart(true);
            add_child(timer);

            MarginContainer* margin = memnew(MarginContainer);
            margin->add_theme_constant_override("margin_left", 4);
            margin->add_theme_constant_override("margin_right", 4);
            add_child(margin);

            _button = memnew(Button);
            _button->set_text("...");
            _button->set_tooltip_text("An update is available for Redotchestrator");
            _button->add_theme_color_override(SceneStringName(font_color), Color(0, 1, 0));
            _button->add_theme_color_override("font_hover_color", Color(0, 1, 0));
            _button->set_vertical_icon_alignment(VERTICAL_ALIGNMENT_CENTER);
            _button->set_focus_mode(FOCUS_NONE);
            _button->set_v_size_flags(SIZE_SHRINK_CENTER);
            margin->add_child(_button);

            _picker = memnew(OrchestratorUpdaterVersionPicker);
            add_child(_picker);

            _check_for_updates();

            timer->connect("timeout", callable_mp_this(_check_for_updates));
            _button->connect(SceneStringName(pressed), callable_mp_this(_show_update_dialog));

            ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_this(_update_picker));
            break;
        }
        case NOTIFICATION_EXIT_TREE: {
            const Callable settings_changed = callable_mp_this(_update_picker);
            if (ProjectSettings::get_singleton()->is_connected("settings_changed", settings_changed)) {
                ProjectSettings::get_singleton()->disconnect("settings_changed", settings_changed);
            }
            set_visible(false);

            while (get_child_count() > 0) {
                if (Node* child = get_child(0)) {
                    remove_child(child);
                    memdelete(child);
                }
            }
            break;
        }
    }
}

OrchestratorUpdaterButton::OrchestratorUpdaterButton() {
    // Generate current plugin version
    // Used in resolving what patches exist
    _plugin_version = OrchestratorVersion::parse(vformat(
        "v%d.%d.%d.%s", VERSION_MAJOR, VERSION_MINOR, VERSION_MAINTENANCE, VERSION_STATUS));
}

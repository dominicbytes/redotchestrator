// This file is part of the Redotchestrator project.
//
// Copyright (c) 2023-present Crater Crash Studios LLC and its contributors.
// Copyright (c) 2026-present Redotchestrator contributors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#pragma once

#include <string_view>

namespace redotchestrator::updater {
    inline constexpr std::string_view ADDON_ARCHIVE_PREFIX = "addons/orchestrator/";
    inline constexpr std::string_view RELEASE_ASSET_URL_PREFIX =
        "https://github.com/dominicbytes/redotchestrator/releases/download/";

    inline char ascii_lower(char p_character) {
        return p_character >= 'A' && p_character <= 'Z' ? p_character + ('a' - 'A') : p_character;
    }

    inline bool ascii_equals_case_insensitive(std::string_view p_left, std::string_view p_right) {
        if (p_left.size() != p_right.size()) {
            return false;
        }
        for (size_t index = 0; index < p_left.size(); ++index) {
            if (ascii_lower(p_left[index]) != ascii_lower(p_right[index])) {
                return false;
            }
        }
        return true;
    }

    inline bool is_windows_reserved_name(std::string_view p_segment) {
        const size_t extension = p_segment.find('.');
        const std::string_view base = p_segment.substr(0, extension);
        if (ascii_equals_case_insensitive(base, "con")
            || ascii_equals_case_insensitive(base, "prn")
            || ascii_equals_case_insensitive(base, "aux")
            || ascii_equals_case_insensitive(base, "nul")) {
            return true;
        }
        if (base.size() == 4) {
            const std::string_view prefix = base.substr(0, 3);
            const char suffix = base[3];
            return suffix >= '1' && suffix <= '9'
                && (ascii_equals_case_insensitive(prefix, "com")
                    || ascii_equals_case_insensitive(prefix, "lpt"));
        }
        return false;
    }

    inline bool is_safe_path_segment(std::string_view p_segment) {
        if (p_segment.empty() || p_segment == "." || p_segment == ".." || p_segment.size() > 255) {
            return false;
        }
        if (p_segment.front() == ' ' || p_segment.back() == ' ' || p_segment.back() == '.') {
            return false;
        }
        for (const char character : p_segment) {
            if (character < 0x20 || character > 0x7e
                || character == '<' || character == '>' || character == ':' || character == '"'
                || character == '/' || character == '\\' || character == '|' || character == '?' || character == '*') {
                return false;
            }
        }
        return !is_windows_reserved_name(p_segment);
    }

    inline bool is_safe_plugin_archive_path(std::string_view p_path) {
        if (p_path.size() > 4096 || !p_path.starts_with(ADDON_ARCHIVE_PREFIX)) {
            return false;
        }

        size_t segment_start = 0;
        while (segment_start < p_path.size()) {
            const size_t separator = p_path.find('/', segment_start);
            const size_t segment_end = separator == std::string_view::npos ? p_path.size() : separator;
            if (!is_safe_path_segment(p_path.substr(segment_start, segment_end - segment_start))) {
                return false;
            }
            if (separator == std::string_view::npos || separator + 1 == p_path.size()) {
                break;
            }
            segment_start = separator + 1;
        }
        return true;
    }

    inline bool is_valid_sha256(std::string_view p_digest) {
        if (p_digest.size() != 64) {
            return false;
        }
        for (const char character : p_digest) {
            if (!((character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f')
                || (character >= 'A' && character <= 'F'))) {
                return false;
            }
        }
        return true;
    }

    inline bool is_ascii_digits(std::string_view p_value) {
        if (p_value.empty()) {
            return false;
        }
        for (const char character : p_value) {
            if (character < '0' || character > '9') {
                return false;
            }
        }
        return true;
    }

    inline bool is_valid_release_status(std::string_view p_status) {
        if (p_status == "stable") {
            return true;
        }
        for (const std::string_view prefix : { std::string_view("rc"), std::string_view("dev") }) {
            if (p_status.starts_with(prefix)) {
                const std::string_view suffix = p_status.substr(prefix.size());
                return suffix.empty() || is_ascii_digits(suffix);
            }
        }
        return false;
    }

    inline bool is_valid_release_tag(std::string_view p_tag) {
        if (p_tag.size() < 4 || p_tag.size() > 64 || p_tag.front() != 'v') {
            return false;
        }

        std::string_view components[4];
        size_t component_count = 0;
        size_t component_start = 1;
        bool reached_end = false;
        while (component_start <= p_tag.size() && component_count < 4) {
            const size_t separator = p_tag.find('.', component_start);
            const size_t component_end = separator == std::string_view::npos ? p_tag.size() : separator;
            components[component_count++] = p_tag.substr(component_start, component_end - component_start);
            if (separator == std::string_view::npos) {
                reached_end = true;
                break;
            }
            component_start = separator + 1;
        }

        if (!reached_end || (component_count != 3 && component_count != 4)) {
            return false;
        }
        if (!is_ascii_digits(components[0]) || !is_ascii_digits(components[1])) {
            return false;
        }
        if (component_count == 4 && !is_ascii_digits(components[2])) {
            return false;
        }
        return is_valid_release_status(components[component_count - 1]);
    }

    inline bool is_valid_redot_compatibility(std::string_view p_version) {
        if (p_version.size() < 6 || p_version.size() > 32 || p_version.front() != 'v') {
            return false;
        }

        size_t component_start = 1;
        for (size_t component = 0; component < 3; ++component) {
            const size_t separator = p_version.find('.', component_start);
            const bool final_component = component == 2;
            if ((final_component && separator != std::string_view::npos)
                || (!final_component && separator == std::string_view::npos)) {
                return false;
            }
            const size_t component_end = final_component ? p_version.size() : separator;
            if (!is_ascii_digits(p_version.substr(component_start, component_end - component_start))) {
                return false;
            }
            component_start = component_end + 1;
        }
        return true;
    }

    inline bool is_valid_plugin_asset_name(std::string_view p_name) {
        constexpr std::string_view prefix = "redotchestrator-";
        constexpr std::string_view suffix = "-plugin.zip";
        if (!p_name.starts_with(prefix) || !p_name.ends_with(suffix)
            || p_name.size() <= prefix.size() + suffix.size()) {
            return false;
        }
        const std::string_view tag = p_name.substr(prefix.size(), p_name.size() - prefix.size() - suffix.size());
        return is_safe_path_segment(p_name) && is_valid_release_tag(tag);
    }

    inline bool is_plugin_asset_name_for_tag(std::string_view p_name, std::string_view p_tag) {
        constexpr std::string_view prefix = "redotchestrator-";
        constexpr std::string_view suffix = "-plugin.zip";
        return is_valid_release_tag(p_tag)
            && is_valid_plugin_asset_name(p_name)
            && p_name.size() == prefix.size() + p_tag.size() + suffix.size()
            && p_name.substr(prefix.size(), p_tag.size()) == p_tag;
    }

    inline bool is_allowed_release_asset_url(std::string_view p_url, std::string_view p_tag) {
        if (!p_url.starts_with(RELEASE_ASSET_URL_PREFIX)
            || p_url.find('?') != std::string_view::npos
            || p_url.find('#') != std::string_view::npos
            || p_url.find('\\') != std::string_view::npos) {
            return false;
        }
        const std::string_view relative = p_url.substr(RELEASE_ASSET_URL_PREFIX.size());
        const size_t file_separator = relative.find('/');
        return file_separator != std::string_view::npos
            && relative.find('/', file_separator + 1) == std::string_view::npos
            && relative.substr(0, file_separator) == p_tag
            && is_plugin_asset_name_for_tag(relative.substr(file_separator + 1), p_tag);
    }

    inline bool is_allowed_release_notes_url(std::string_view p_url) {
        constexpr std::string_view prefix = "https://github.com/dominicbytes/redotchestrator/releases/";
        if (!p_url.starts_with(prefix) || p_url.size() <= prefix.size()) {
            return false;
        }
        for (const char character : p_url) {
            if (character < 0x20 || character > 0x7e || character == '\\') {
                return false;
            }
        }
        return true;
    }
}

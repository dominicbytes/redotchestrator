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
#include "orchestration/serialization/binary/binary_format.h"

void OrchestrationBinaryFormat::save_unicode_string(const Ref<FileAccess>& p_file, const String& p_value, bool p_bit_on_length) {
    ERR_FAIL_COND_MSG(p_file.is_null(), "Cannot save unicode string when file reference is not valid.");

    CharString utf8 = p_value.utf8();

    size_t length = utf8.length() + 1;
    if (p_bit_on_length) {
        length |= 0x80000000;
    }

    p_file->store_32(length);
    p_file->store_buffer(reinterpret_cast<const uint8_t*>(utf8.get_data()), utf8.length() + 1);
}

Error OrchestrationBinaryFormat::read_unicode_string(const Ref<FileAccess>& p_file, String& r_value) {
    r_value = String();
    ERR_FAIL_COND_V_MSG(p_file.is_null(), ERR_FILE_CANT_READ, "Cannot read unicode string when file reference is not valid.");

    // A zero length is a valid empty string, and is also what a truncated file yields at EOF.
    const uint32_t size = p_file->get_32();
    if (size == 0) {
        return OK;
    }

    ERR_FAIL_COND_V_MSG(size > p_file->get_length() - p_file->get_position(), ERR_FILE_CORRUPT,
        "String length exceeds the remaining file size, file is corrupt.");

    Vector<char> buffer;
    buffer.resize(size);
    ERR_FAIL_COND_V_MSG(p_file->get_buffer(reinterpret_cast<uint8_t*>(buffer.ptrw()), size) != size, ERR_FILE_CORRUPT,
        "Failed to read string, file is corrupt.");

    r_value = String::utf8(buffer.ptr(), size);
    return OK;
}

Error OrchestrationBinaryFormat::copy_unicode_string(const Ref<FileAccess>& p_from, const Ref<FileAccess>& p_to) {
    String value;
    if (const Error err = read_unicode_string(p_from, value)) {
        return err;
    }
    save_unicode_string(p_to, value);
    return OK;
}
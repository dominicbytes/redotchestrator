# Keep the pinned 26.2 bindings usable with newer engine-owned Variant types.
# Patch a build-tree copy; the submodule remains byte-for-byte pinned.
SET(variant_source "${CMAKE_CURRENT_SOURCE_DIR}/extern/godot-cpp/src/variant/variant.cpp")
FILE(READ "${variant_source}" variant_code)
SET(old_cleanup "if (unlikely(needs_deinit[get_type()]))")
SET(new_cleanup "if (unlikely(get_type() >= VARIANT_MAX || needs_deinit[get_type()]))")
STRING(FIND "${variant_code}" "${old_cleanup}" cleanup_position)
IF (cleanup_position EQUAL -1)
    MESSAGE(FATAL_ERROR "Pinned redot-cpp Variant cleanup changed; review the forward-compatibility patch")
ENDIF ()
STRING(REPLACE "${old_cleanup}" "${new_cleanup}" variant_code "${variant_code}")
# Keep it out of _generated/, whose sources are globbed into the extension target.
SET(patched_variant "${CMAKE_CURRENT_BINARY_DIR}/redot-forward-compat/variant.cpp")
FILE(CONFIGURE OUTPUT "${patched_variant}" CONTENT "${variant_code}" @ONLY)
GET_TARGET_PROPERTY(binding_sources godot-cpp SOURCES)
LIST(REMOVE_ITEM binding_sources "${variant_source}")
SET_PROPERTY(TARGET godot-cpp PROPERTY SOURCES "${binding_sources};${patched_variant}")

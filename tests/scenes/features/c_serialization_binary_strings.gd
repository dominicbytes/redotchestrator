extends Node

# Hand-built binary (.os) files exercising string encodings the engine's binary format
# allows but the plugin's own writer rarely produces: inline (non string table) property
# names, empty strings, and a file truncated inside its header. None of these may crash
# or print errors (stderr must stay empty).

const DIR := "user://c_serialization_binary_strings"


func _ready() -> void:
	_clean(DIR)
	DirAccess.make_dir_recursive_absolute(DIR)

	var inline_path := DIR + "/inline.os"
	_write_compressed(inline_path, _build(true))
	var inline_script: Script = ResourceLoader.load(inline_path, "", ResourceLoader.CACHE_MODE_IGNORE)
	print("inline loaded: ", inline_script != null)
	print("inline base type: ", inline_script.get_instance_base_type())

	var table_path := DIR + "/table.os"
	_write_compressed(table_path, _build(false))
	var table_script: Script = ResourceLoader.load(table_path, "", ResourceLoader.CACHE_MODE_IGNORE)
	print("table loaded: ", table_script != null)
	print("table base type: ", table_script.get_instance_base_type())

	# Keep only the fixed-size part of the header, so the resource type string is cut off.
	var truncated_path := DIR + "/truncated.os"
	_write_compressed(truncated_path, _build(false).slice(0, 28))
	print("truncated uid: ", ResourceLoader.get_resource_uid(truncated_path))

	_clean(DIR)


# Builds a minimal orchestration whose only property is base_type = "Node".
# The property name is either stored inline or through the string table.
func _build(inline_name: bool) -> PackedByteArray:
	var buffer := StreamPeerBuffer.new()
	buffer.put_data("GDOS".to_ascii_buffer())
	buffer.put_u32(0) # little endian
	buffer.put_u32(0) # single precision
	buffer.put_u32(4) # format version
	buffer.put_u32(4)
	buffer.put_u32(5)
	buffer.put_u32(0)
	_put_string(buffer, "OScript")
	buffer.put_u32(0) # flags
	for _i in 10:
		buffer.put_u32(0) # reserved

	# String table: index 0 is a zero-length entry, written without even a terminator.
	if inline_name:
		buffer.put_u32(1)
		buffer.put_u32(0)
	else:
		buffer.put_u32(2)
		buffer.put_u32(0)
		_put_string(buffer, "base_type")

	buffer.put_u32(0) # external resources
	buffer.put_u32(1) # internal resources
	_put_string(buffer, "local://0")
	var offset_position := buffer.get_position()
	buffer.put_u64(0)

	var offset := buffer.get_position()
	_put_string(buffer, "Orchestration")
	buffer.put_u32(2) # properties
	if inline_name:
		_put_inline_name(buffer, "base_type")
	else:
		buffer.put_u32(1)
	buffer.put_u32(5) # VARIANT_STRING
	_put_string(buffer, "Node")

	# A string value with a zero length, also what a reader sees at the end of a truncated file.
	_put_inline_name(buffer, "metadata/empty")
	buffer.put_u32(5) # VARIANT_STRING
	buffer.put_u32(0)

	buffer.put_data("GDOS".to_ascii_buffer())

	buffer.seek(offset_position)
	buffer.put_u64(offset)
	return buffer.data_array


func _put_string(buffer: StreamPeerBuffer, value: String) -> void:
	var bytes := value.to_utf8_buffer()
	bytes.push_back(0)
	buffer.put_u32(bytes.size())
	buffer.put_data(bytes)


# Inline strings set the high bit of the length instead of referencing the string table.
func _put_inline_name(buffer: StreamPeerBuffer, value: String) -> void:
	var bytes := value.to_utf8_buffer()
	bytes.push_back(0)
	buffer.put_u32(0x80000000 | bytes.size())
	buffer.put_data(bytes)


func _write_compressed(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open_compressed(path, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()


func _clean(path: String) -> void:
	if not DirAccess.dir_exists_absolute(path):
		return
	for file in DirAccess.get_files_at(path):
		DirAccess.remove_absolute(path + "/" + file)
	DirAccess.remove_absolute(path)

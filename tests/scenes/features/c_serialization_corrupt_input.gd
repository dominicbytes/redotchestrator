extends Node

# Damaged or crafted orchestration files used to crash the loaders. Each must now fail
# (or recover) with an error message instead, and the scene must run to completion.

const DIR := "user://c_serialization_corrupt_input"

const DANGLING_CONNECTION := """[orchestration type="OScript" format=4]

[resource]
base_type = &"Node"
connections = [1, 0, 2, 0]
"""

const NULL_ENTRIES := """[orchestration type="OScript" format=4]

[resource]
base_type = &"Node"
variables = [null]
functions = [null]
signals = [null]
connections = [1, 0, 2]
nodes = [null]
graphs = [null]
"""


func _ready() -> void:
	_clean(DIR)
	DirAccess.make_dir_recursive_absolute(DIR)

	_check_text("dangling_connection.torch", DANGLING_CONNECTION)
	_check_text("null_entries.torch", NULL_ENTRIES)
	_check_text("deep_text.torch", _deep_text(3000))
	_check_binary("bad_string_index.os", _bad_string_index())
	_check_binary("bad_string_length.os", _bad_string_length())

	# The same file twice, once intact and once with only the final string's length word damaged.
	# The damaged copy must fail to load rather than keep an empty string in place of the text.
	var valid := _check_binary("valid_final_string.os", _final_string(false))
	if valid:
		var resaved := DIR + "/valid_final_string.torch"
		ResourceSaver.save(valid, resaved)
		printerr("valid_final_string.os text kept: ", FileAccess.get_file_as_string(resaved).contains('metadata/text = "KEEP_THIS_TEXT"'))
	_check_binary("bad_final_string_length.os", _final_string(true))

	_clean(DIR)
	printerr("Corrupt input handled")


# A script is usable when it loaded and can be instantiated.
func _report(file_name: String, script: Script) -> Script:
	var usable := script != null and script.can_instantiate()
	printerr("%s usable: %s" % [file_name, usable])
	return script if usable else null


func _check_text(file_name: String, text: String) -> Script:
	var path := DIR + "/" + file_name
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(text)
	file.close()
	return _report(file_name, ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE))


func _check_binary(file_name: String, bytes: PackedByteArray) -> Script:
	var path := DIR + "/" + file_name
	var file := FileAccess.open_compressed(path, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()
	return _report(file_name, ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE))


func _deep_text(depth: int) -> String:
	return '[orchestration type="OScript" format=4]\n\n[resource]\nbase_type = &"Node"\nmetadata/deep = %s%s\n' % ["[".repeat(depth), "]".repeat(depth)]


# A property name that references a string table entry that does not exist.
func _bad_string_index() -> PackedByteArray:
	var buffer := _begin([])
	buffer.put_u32(7)
	buffer.put_u32(1) # VARIANT_NIL
	return _end(buffer)


# A string value whose length runs past the end of the file.
func _bad_string_length() -> PackedByteArray:
	var buffer := _begin(["metadata/text"])
	buffer.put_u32(0)
	buffer.put_u32(5) # VARIANT_STRING
	buffer.put_u32(0x7FFFFF00)
	return _end(buffer)


# A valid base type followed by a final non-empty string property. When damaged, only the
# length word of that string changes, its payload stays in the file.
func _final_string(damaged: bool) -> PackedByteArray:
	var buffer := _begin(["base_type", "metadata/text"], 2)
	buffer.put_u32(0)
	buffer.put_u32(5) # VARIANT_STRING
	_put_string(buffer, "Node")
	buffer.put_u32(1)
	buffer.put_u32(5) # VARIANT_STRING
	var text := "KEEP_THIS_TEXT".to_utf8_buffer()
	text.push_back(0)
	buffer.put_u32(0x7FFFFF00 if damaged else text.size())
	buffer.put_data(text)
	return _end(buffer)


# Writes the header and resource table of a binary orchestration, followed by the start
# of the main resource, whose property names and values the caller appends.
func _begin(strings: Array, property_count := 1) -> StreamPeerBuffer:
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

	buffer.put_u32(strings.size())
	for value: String in strings:
		_put_string(buffer, value)

	buffer.put_u32(0) # external resources
	buffer.put_u32(1) # internal resources
	_put_string(buffer, "local://0")
	buffer.put_u64(buffer.get_position() + 8)

	_put_string(buffer, "Orchestration")
	buffer.put_u32(property_count)
	return buffer


func _end(buffer: StreamPeerBuffer) -> PackedByteArray:
	buffer.put_data("GDOS".to_ascii_buffer())
	return buffer.data_array


func _put_string(buffer: StreamPeerBuffer, value: String) -> void:
	var bytes := value.to_utf8_buffer()
	bytes.push_back(0)
	buffer.put_u32(bytes.size())
	buffer.put_data(bytes)


func _clean(path: String) -> void:
	if not DirAccess.dir_exists_absolute(path):
		return
	for file in DirAccess.get_files_at(path):
		DirAccess.remove_absolute(path + "/" + file)
	DirAccess.remove_absolute(path)

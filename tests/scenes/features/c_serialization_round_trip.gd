extends Node

# Round trips values through the text (.torch) and binary (.os) formats, then checks
# that re-saving keeps the UID, that set_uid keeps the format version, and that
# relative external paths in .os files resolve against the file's own directory.
# Any error printed by the loaders or savers fails this test (stderr must stay empty).

const DIR := "user://c_serialization_round_trip"

# set_uid replaces the file through a res:// directory accessor, the same as the engine's own savers.
const RES_DIR := "res://scenes/features/c_serialization_round_trip_tmp"

const SOURCE := """[orchestration type="OScript" format=4]

[resource]
base_type = &"Node"
metadata/absolute = NodePath("/root/Main")
metadata/subnames = NodePath("Sprite2D:position:x")
metadata/absolute_subnames = NodePath("/root/Main/Player:frame")
metadata/property_only = NodePath(":modulate:a")
metadata/vectors = PackedVector4Array(1, 2, 3, 4, 5, 6, 7, 8)
metadata/empty_string = ""
metadata/empty_name = &""
metadata/strings = PackedStringArray("", "x", "")
metadata/mixed = ["", &"", NodePath("")]
"""

const FORMAT_3 := """[orchestration type="OScript" format=3]

[resource]
base_type = &"Node"
"""

const RELATIVE := """[orchestration type="OScript" load_steps=2 format=4]

[ext_resource type="Resource" path="%s" id="1"]

[resource]
base_type = &"Node"
metadata/dependency = ExtResource("1")
"""


func _ready() -> void:
	_clean(DIR)
	_clean(RES_DIR)
	DirAccess.make_dir_recursive_absolute(DIR + "/nested")
	DirAccess.make_dir_recursive_absolute(RES_DIR)

	_round_trip()
	_uid_is_stable("rt.torch")
	_uid_is_stable("rt.os")
	_set_uid_keeps_format()
	_relative_dependency()

	_clean(DIR)
	_clean(RES_DIR)


func _write(path: String, text: String) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(text)
	file.close()


func _load(path: String) -> Resource:
	return ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE)


func _metadata_lines(path: String) -> PackedStringArray:
	var lines := PackedStringArray()
	for line in FileAccess.get_file_as_string(path).split("\n"):
		if line.begins_with("metadata/"):
			lines.push_back(line)
	return lines


func _round_trip() -> void:
	_write(DIR + "/source.torch", SOURCE)

	var script := _load(DIR + "/source.torch")
	print("text loaded: ", script != null)
	print("binary saved: ", ResourceSaver.save(script, DIR + "/rt.os"))

	var binary := _load(DIR + "/rt.os")
	print("binary loaded: ", binary != null)
	print("text saved: ", ResourceSaver.save(binary, DIR + "/rt.torch"))

	var expected := _metadata_lines(DIR + "/source.torch")
	var actual := _metadata_lines(DIR + "/rt.torch")
	for line in actual:
		print(line)
	print("round trip matches: ", expected == actual)


func _uid_is_stable(file_name: String) -> void:
	var path := DIR + "/" + file_name
	var before := ResourceLoader.get_resource_uid(path)
	var script := _load(path)
	print("%s resaved: %d" % [file_name, ResourceSaver.save(script, path)])
	var after := ResourceLoader.get_resource_uid(path)
	print("%s uid stable: %s" % [file_name, before != ResourceUID.INVALID_ID and before == after])


func _set_uid_keeps_format() -> void:
	var path := RES_DIR + "/format_3.torch"
	_write(path, FORMAT_3)

	var uid := ResourceUID.create_id()
	print("set_uid: ", ResourceSaver.set_uid(path, uid))

	var header := FileAccess.open(path, FileAccess.READ).get_line()
	var expected := '[orchestration type="OScript" format=3 uid="%s"]' % ResourceUID.id_to_text(uid)
	print("set_uid header kept format: ", header == expected)
	print("set_uid left temp file: ", FileAccess.file_exists(path + ".uidren"))


func _relative_dependency() -> void:
	var dependency := DIR + "/nested/dependency.tres"
	print("dependency saved: ", ResourceSaver.save(Resource.new(), dependency))

	var source := DIR + "/nested/relative.torch"
	_write(source, RELATIVE % dependency)

	var script := _load(source)
	var path := DIR + "/nested/relative.os"
	print("relative saved: ", ResourceSaver.save(script, path, ResourceSaver.FLAG_RELATIVE_PATHS))
	print("relative stored: ", ResourceLoader.get_dependencies(path))

	var binary := _load(path)
	print("relative loaded: ", binary != null)
	print("relative resaved: ", ResourceSaver.save(binary, DIR + "/nested/relative_rt.torch"))
	var text := FileAccess.get_file_as_string(DIR + "/nested/relative_rt.torch")
	print("relative resolved: ", text.contains('path="%s"' % dependency))
	for line in text.split("\n"):
		if line.begins_with("[ext_resource"):
			print("ext_resource tag on one line: ", line.ends_with("]"))


func _clean(path: String) -> void:
	if not DirAccess.dir_exists_absolute(path):
		return
	for directory in DirAccess.get_directories_at(path):
		_clean(path + "/" + directory)
	for file in DirAccess.get_files_at(path):
		DirAccess.remove_absolute(path + "/" + file)
	DirAccess.remove_absolute(path)

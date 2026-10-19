extends Node

func _ready() -> void:
	var version := Engine.get_version_info()
	if version["major"] == 26 and version["minor"] >= 3:
		# Keep the 26.3-only property type out of the 26.2 editor import pass.
		var path := "user://b_unsupported_struct.torch"
		var file := FileAccess.open(path, FileAccess.WRITE)
		file.store_string('[orchestration type="OScript" load_steps=2 format=4]\n[obj type="OScriptVariable" id="V"]\nname = "record"\ncategory = "Default"\nclassification = "type:Struct"\ntype = 39\n[resource]\nbase_type = &"Node"\nvariables = Array[OScriptVariable]([SubResource("V")])\n')
		file.close()
		Engine.print_error_messages = false
		var script = ResourceLoader.load(path, "OScript", ResourceLoader.CACHE_MODE_IGNORE)
		Engine.print_error_messages = true
		DirAccess.remove_absolute(path)
		Utils.check(script == null or not script.can_instantiate())
	print("Struct compatibility checked")

extends Node

func _ready() -> void:
	var variable = ClassDB.instantiate("OScriptVariable")
	var runtime: Dictionary = Engine.get_version_info()
	if runtime["major"] == 26 and runtime["minor"] >= 3:
		# Compile new syntax only on the engine that provides it, so 26.2 still imports.
		var factory := GDScript.new()
		factory.source_code = "extends RefCounted\nstruct Payload:\n\tvar value: RefCounted\nfunc make(value):\n\treturn Payload.new(value)\n"
		assert(factory.reload() == OK)
		var instance = factory.new()
		for _index in range(64):
			var held := RefCounted.new()
			var weak: WeakRef = weakref(held)
			var payload = instance.make(held)
			variable.set_default_value(payload)
			held = null
			payload = null
			assert(weak.get_ref() != null)
			variable.set_default_value(null)
			assert(weak.get_ref() == null, "Replacing an engine-owned Struct must release its payload")
	else:
		variable.set_default_value("baseline")
		variable.set_default_value(null)
	print("Variant cleanup passed")

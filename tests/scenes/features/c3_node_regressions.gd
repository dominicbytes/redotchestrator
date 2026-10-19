extends Node

func _ready() -> void:
	_check_collection_defaults()
	print("Collection default regressions passed")

func _check_collection_defaults() -> void:
	var array_variable = ClassDB.instantiate("OScriptVariable")
	var array_info := {
		"name": &"items", "type": TYPE_ARRAY, "hint": PROPERTY_HINT_ARRAY_TYPE,
		"hint_string": "int", "usage": PROPERTY_USAGE_DEFAULT,
	}
	array_variable.set_property_info(array_info)
	assert(array_variable.get_default_value().get_typed_builtin() == TYPE_INT)
	array_variable.set_default_value([7, 8])
	array_variable.set_property_info(array_info)
	assert(array_variable.get_default_value() == [7, 8])
	assert(array_variable.get_default_value().get_typed_builtin() == TYPE_INT)
	array_info["hint_string"] = "float"
	array_variable.set_property_info(array_info)
	assert(array_variable.get_default_value() == [7.0, 8.0])
	assert(array_variable.get_default_value().get_typed_builtin() == TYPE_FLOAT)
	array_variable.set_property_info(array_info)
	assert(array_variable.get_default_value() == [7.0, 8.0])

	var dictionary_variable = ClassDB.instantiate("OScriptVariable")
	var dictionary_info := {
		"name": &"mapping", "type": TYPE_DICTIONARY, "hint": PROPERTY_HINT_DICTIONARY_TYPE,
		"hint_string": "String;int", "usage": PROPERTY_USAGE_DEFAULT,
	}
	dictionary_variable.set_property_info(dictionary_info)
	assert(dictionary_variable.get_default_value().get_typed_value_builtin() == TYPE_INT)
	dictionary_variable.set_default_value({"score": 7})
	dictionary_variable.set_property_info(dictionary_info)
	assert(dictionary_variable.get_default_value() == {"score": 7})
	assert(dictionary_variable.get_default_value().get_typed_key_builtin() == TYPE_STRING)
	assert(dictionary_variable.get_default_value().get_typed_value_builtin() == TYPE_INT)
	dictionary_info["hint_string"] = "String;float"
	dictionary_variable.set_property_info(dictionary_info)
	assert(dictionary_variable.get_default_value() == {"score": 7.0})
	assert(dictionary_variable.get_default_value().get_typed_value_builtin() == TYPE_FLOAT)
	dictionary_variable.set_property_info(dictionary_info)
	assert(dictionary_variable.get_default_value() == {"score": 7.0})

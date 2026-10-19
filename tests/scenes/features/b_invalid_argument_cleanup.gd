extends Node

func _ready() -> void:
	var script = load("res://scenes/features/b_invalid_argument_cleanup.torch")
	var instance = script.new()
	var payload := RefCounted.new()
	var reference_count := payload.get_reference_count()
	for _index in range(16):
		Engine.print_error_messages = false
		instance.callv("probe", [payload, []])
		Engine.print_error_messages = true
		Utils.check(payload.get_reference_count() == reference_count)
	instance.callv("probe", [payload, 42])
	Utils.check(payload.get_reference_count() == reference_count)
	instance.free()
	print("Rejected arguments release their references")

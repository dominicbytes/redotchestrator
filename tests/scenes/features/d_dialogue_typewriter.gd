extends Node

func _ready() -> void:
	var dialogue = preload("res://addons/orchestrator/scenes/dialogue_message.tscn").instantiate()
	dialogue.dialogue_data = {
		"character_name": "Speaker",
		"message": "Typewriter regression",
		"options": {},
	}
	add_child(dialogue)
	var label: Label = dialogue.speaker_text
	print("initial characters hidden: ", label.visible_characters == 0)
	await get_tree().process_frame
	print("first visible frame hidden: ", dialogue.visible and label.visible_characters == 0)
	await dialogue._current_tween.finished
	print("message completed: ", label.visible_characters == label.text.length())
	dialogue.show_message("Speaker", "Next message", {})
	print("next message reset: ", label.visible_characters == 0)
	await get_tree().process_frame
	await dialogue._current_tween.finished
	dialogue.show_message_finished.connect(func(): print("dialogue finished: ", dialogue.selection == -1))
	dialogue.next_button.pressed.emit()

extends CharacterBody3D

@export var speed = 5.0
@export var mic_active = false

@onready var odin_manager = $"/root/Main/OdinManager"
@onready var mic_toggle = $"/root/Main/UI/Panel/VBoxContainer/MicToggle"

func _ready():
	print("ready player one")
	mic_toggle.button_pressed = mic_active
	
	_update_mic_state()

func _physics_process(_delta):
	var input_dir = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	var direction = (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
	if direction:
		velocity.x = direction.x * speed
		velocity.z = direction.z * speed
	else:
		velocity.x = move_toward(velocity.x, 0, speed)
		velocity.z = move_toward(velocity.z, 0, speed)
	move_and_slide()
	
func _on_mic_toggle_pressed():
	mic_active = mic_toggle.button_pressed
	_update_mic_state()
	
func _update_mic_state():
	print("updating mic state")
	if odin_manager and odin_manager.is_room_connected():
		odin_manager.set_microphone_active(mic_active)
		print("mic state updated")

extends Node3D

@export var access_key: String = ""
@export var room_id: String = "test_room"

@onready var odin_manager = $OdinManager
@onready var join_button = $UI/Panel/VBoxContainer/JoinButton
@onready var leave_button = $UI/Panel/VBoxContainer/LeaveButton
@onready var status_label = $UI/Panel/VBoxContainer/StatusLabel

func _ready():
	odin_manager.set_access_key(access_key)
	odin_manager.set_room_id(room_id)
	odin_manager.set_user_id("user_" + str(randi() % 1000))
	
	leave_button.disabled = true
	status_label.text = "ready..."

func _on_join_button_pressed():
	if access_key.is_empty():
		status_label.text = "Error: No access key provided"
		return
		
	var token = odin_manager.generate_room_token(room_id, odin_manager.get_user_id())
	if token.is_empty():
		status_label.text = "Error: Failed to generate token"
		return
	
	status_label.text = "Connecting..."
	
	var server_url = odin_manager.get_server_url()
	odin_manager.join_room(server_url, token)
	
	join_button.disabled = true

func _on_leave_button_pressed():
	odin_manager.leave_room()
	
	join_button.disabled = false
	leave_button.disabled = true
	status_label.text = "Disconnected"

func _on_odin_manager_connection_state_changed(state):
	status_label.text = "Connection: " + state

func _on_odin_manager_room_joined(peer_id):
	status_label.text = "Connected (Peer ID: " + peer_id + ")"
	
	join_button.disabled = true
	leave_button.disabled = false

func _on_odin_manager_peer_joined(peer_id):
	print("Peer joined: ", peer_id)

func _on_odin_manager_peer_left(peer_id):
	print("Peer left: ", peer_id)

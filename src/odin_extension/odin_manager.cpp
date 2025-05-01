#include "odin_manager.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/time.hpp>

namespace godot {
    OdinManager::OdinManager()
        :   room_handle(nullptr),
            local_audio_stream(nullptr),
            is_connected(false),
            microphone_active(false),
            use_spatial_audio(true),
            voice_activity_detection_enabled(true),
            voice_activity_detection_attack_probability(0.9),
            voice_activity_detection_release_probability(0.8),
            volume_gate_enabled(false),
            volume_gate_attack_loudness(-30),
            volume_gate_release_loudness(-40),
            echo_canceller_enabled(true),
            high_pass_filter_enabled(false),
            noise_suppression_level(OdinNoiseSuppressionLevel_Moderate),
            transient_suppressor_enabled(false) {
        server_url = "https://gateway.odin.4players.io";
    }

    OdinManager::~OdinManager() {
        shutdown();
    }

    void OdinManager::_process(double delta) {
        if (!room_handle || !is_connected) {
            return;
        }

        if (microphone_active) {
            process_microphone_audio();
        }

        if (is_connected && room_handle) {
            for (const auto& stream_entry : audio_streams) {
                const OdinMediaStreamHandle stream_handle = stream_entry.first;
                AudioStreamPlayer* player = stream_entry.second;

                constexpr size_t buffer_size = 1024;
                float audio_buffer[buffer_size];

                const OdinReturnCode return_code = odin_audio_read_data(stream_handle, audio_buffer, buffer_size);

                if (odin_is_error(return_code)) {
                    UtilityFunctions::print("Error reading audio data: ", get_error_message(return_code));
                } else {
                    update_audio_playback(stream_handle, audio_buffer, buffer_size);
                }
            }
        }

        if (is_connected && room_handle && use_spatial_audio && spatial_target) {
            const Vector3 local_position = spatial_target->get_global_position();
            const OdinReturnCode return_code = odin_room_update_position(
                *room_handle,
                local_position.x,
                local_position.y,
                local_position.z
            );
            if (odin_is_error(return_code)) {
                UtilityFunctions::print("Error updating room position: ", get_error_message(return_code));
            }
        }
    }

    static void handle_odin_event(OdinRoomHandle room, const OdinEvent *event, void* data) {
        OdinManager* manager = static_cast<OdinManager*>(data);
        switch (event->tag) {
            case OdinEvent_RoomConnectionStateChanged: {
                const OdinRoomConnectionState state = event->room_connection_state_changed.state;
                switch (state) {
                    case OdinRoomConnectionState_Connected:
                        manager->emit_signal("connection_state_changed", "connected");
                        break;
                    case OdinRoomConnectionState_Connecting:
                        manager->emit_signal("connection_state_changed", "connecting");
                        break;
                    case OdinRoomConnectionState_Disconnected:
                        manager->emit_signal("connection_state_changed", "disconnected");
                        break;
                    default:
                        break;
                }
                break;
            }
            case OdinEvent_Joined: {
                const uint64_t own_peer_id = event->joined.own_peer_id;
                manager->set_own_peer_id(own_peer_id);
                manager->emit_signal("room_joined", String::num_int64(own_peer_id));
                break;
            }
            case OdinEvent_RoomUserDataChanged: {
                const uint8_t *user_data = event->room_user_data_changed.room_user_data;
                size_t user_data_len = event->room_user_data_changed.room_user_data_len;
                // TODO: Handle room user data change
                break;
            }
            case OdinEvent_PeerJoined: {
                const uint64_t peer_id = event->peer_joined.peer_id;
                manager->add_peer(peer_id);
                manager->emit_signal("peer_joined", String::num_uint64(peer_id));
                break;
            }
            case OdinEvent_PeerLeft: {
                const uint64_t peer_id = event->peer_left.peer_id;
                manager->remove_peer(peer_id);
                manager->emit_signal("peer_left", String::num_uint64(peer_id));
                break;
            }
            case OdinEvent_PeerUserDataChanged: {
                const uint64_t peer_id = event->peer_user_data_changed.peer_id;
                const uint8_t *user_data = event->peer_user_data_changed.peer_user_data;
                const size_t user_data_len = event->peer_user_data_changed.peer_user_data_len;
                manager->update_peer_user_data(peer_id, user_data, user_data_len);
                manager->emit_signal("peer_user_data_changed", peer_id);
                break;
            }
            case OdinEvent_MediaAdded: {
                const uint64_t peer_id = event->media_added.peer_id;
                const OdinMediaStreamHandle media = event->media_added.media_handle;
                manager->add_media_stream(peer_id, media);
                uint16_t out_media_id;
                const OdinReturnCode result = odin_media_stream_media_id(media, &out_media_id);
                if (odin_is_error(result)) {
                    return;
                }
                manager->emit_signal("media_added", String::num_int64(peer_id), String::num_uint64(out_media_id));
                break;
            }
            case OdinEvent_MediaRemoved: {
                const uint64_t peer_id = event->media_removed.peer_id;
                const OdinMediaStreamHandle media = event->media_removed.media_handle;
                manager->remove_media_stream(peer_id, media);
                uint16_t out_media_id;
                const OdinReturnCode result = odin_media_stream_media_id(media, &out_media_id);
                if (odin_is_error(result)) {
                    return;
                }
                manager->emit_signal("media_removed", String::num_int64(peer_id), String::num_uint64(out_media_id));
                break;
            }
            case OdinEvent_MediaActiveStateChanged: {
                const uint64_t peer_id = event->media_active_state_changed.peer_id;
                const OdinMediaStreamHandle media = event->media_active_state_changed.media_handle;
                const bool active = event->media_active_state_changed.active;
                manager->update_media_active_state(peer_id, media, active);
                uint16_t out_media_id;
                const OdinReturnCode result = odin_media_stream_media_id(media, &out_media_id);
                if (odin_is_error(result)) {
                    return;
                }
                manager->emit_signal("media_active_state_changed", String::num_int64(peer_id), String::num_uint64(out_media_id), active);
                break;
            }
            case OdinEvent_MessageReceived: {
                const uint64_t peer_id = event->message_received.peer_id;
                const uint8_t *message = event->message_received.data;
                const size_t message_len = event->message_received.data_len;
                PackedByteArray byte_array;
                byte_array.resize(message_len);
                memcpy(byte_array.ptrw(), message, message_len);
                manager->emit_signal("message_received", String::num_int64(peer_id), byte_array);
                break;
            }
            default:
                break;
        }
    }

    void OdinManager::_bind_methods() {
        ClassDB::bind_method(D_METHOD("startup"), &OdinManager::startup);
        ClassDB::bind_method(D_METHOD("shutdown"), &OdinManager::shutdown);
        ClassDB::bind_method(D_METHOD("join_room", "server_url", "room_token"), &OdinManager::join_room);
        ClassDB::bind_method(D_METHOD("leave_room"), &OdinManager::leave_room);
        ClassDB::bind_method(D_METHOD("generate_room_token", "room_id", "user_id"), &OdinManager::generate_room_token);

        ClassDB::bind_method(D_METHOD("set_microphone_active", "active"), &OdinManager::set_microphone_active);
        ClassDB::bind_method(D_METHOD("get_microphone_active"), &OdinManager::get_microphone_active);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "microphone_active"), "set_microphone_active", "get_microphone_active");

        ClassDB::bind_method(D_METHOD("set_voice_activity_detection_enabled", "enabled"), &OdinManager::set_voice_activity_detection_enabled);
        ClassDB::bind_method(D_METHOD("get_voice_activity_detection_enabled"), &OdinManager::get_voice_activity_detection_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "voice_activity_detection_enabled"), "set_voice_activity_detection_enabled", "get_voice_activity_detection_enabled");

        ClassDB::bind_method(D_METHOD("set_voice_activity_detection_attack_probability", "probability"), &OdinManager::set_voice_activity_detection_attack_probability);
        ClassDB::bind_method(D_METHOD("get_voice_activity_detection_attack_probability"), &OdinManager::get_voice_activity_detection_attack_probability);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "voice_activity_detection_attack_probability"), "set_voice_activity_detection_attack_probability", "get_voice_activity_detection_attack_probability");

        ClassDB::bind_method(D_METHOD("set_voice_activity_detection_release_probability", "probability"), &OdinManager::set_voice_activity_detection_release_probability);
        ClassDB::bind_method(D_METHOD("get_voice_activity_detection_release_probability"), &OdinManager::get_voice_activity_detection_release_probability);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "voice_activity_detection_release_probability"), "set_voice_activity_detection_release_probability", "get_voice_activity_detection_release_probability");

        ClassDB::bind_method(D_METHOD("set_volume_gate_enabled", "enabled"), &OdinManager::set_volume_gate_enabled);
        ClassDB::bind_method(D_METHOD("get_volume_gate_enabled"), &OdinManager::get_volume_gate_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "volume_gate_enabled"), "set_volume_gate_enabled", "get_volume_gate_enabled");

        ClassDB::bind_method(D_METHOD("set_volume_gate_attack_loudness", "loudness"), &OdinManager::set_volume_gate_attack_loudness);
        ClassDB::bind_method(D_METHOD("get_volume_gate_attack_loudness"), &OdinManager::get_volume_gate_attack_loudness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_gate_attack_loudness"), "set_volume_gate_attack_loudness", "get_volume_gate_attack_loudness");

        ClassDB::bind_method(D_METHOD("set_volume_gate_release_loudness", "loudness"), &OdinManager::set_volume_gate_release_loudness);
        ClassDB::bind_method(D_METHOD("get_volume_gate_release_loudness"), &OdinManager::get_volume_gate_release_loudness);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_gate_release_loudness"), "set_volume_gate_release_loudness", "get_volume_gate_release_loudness");

        ClassDB::bind_method(D_METHOD("set_echo_canceller_enabled", "enabled"), &OdinManager::set_echo_canceller_enabled);
        ClassDB::bind_method(D_METHOD("get_echo_canceller_enabled"), &OdinManager::get_echo_canceller_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "echo_canceller_enabled"), "set_echo_canceller_enabled", "get_echo_canceller_enabled");

        ClassDB::bind_method(D_METHOD("set_high_pass_filter_enabled", "enabled"), &OdinManager::set_high_pass_filter_enabled);
        ClassDB::bind_method(D_METHOD("get_high_pass_filter_enabled"), &OdinManager::get_high_pass_filter_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "high_pass_filter_enabled"), "set_high_pass_filter_enabled", "get_high_pass_filter_enabled");

        ClassDB::bind_method(D_METHOD("set_noise_suppression_level", "level"), &OdinManager::set_noise_suppression_level);
        ClassDB::bind_method(D_METHOD("get_noise_suppression_level"), &OdinManager::get_noise_suppression_level);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_suppression_level"), "set_noise_suppression_level", "get_noise_suppression_level");

        ClassDB::bind_method(D_METHOD("set_transient_suppressor_enabled", "enabled"), &OdinManager::set_transient_suppressor_enabled);
        ClassDB::bind_method(D_METHOD("get_transient_suppressor_enabled"), &OdinManager::get_transient_suppressor_enabled);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transient_suppressor_enabled"), "set_transient_suppressor_enabled", "get_transient_suppressor_enabled");

        ADD_SIGNAL(MethodInfo("room_left"));

        ADD_SIGNAL(MethodInfo("connection_state_changed", PropertyInfo(Variant::STRING, "state")));
        ADD_SIGNAL(MethodInfo("room_joined", PropertyInfo(Variant::STRING, "peer_id")));
        ADD_SIGNAL(MethodInfo("peer_joined", PropertyInfo(Variant::STRING, "peer_id")));
        ADD_SIGNAL(MethodInfo("peer_left", PropertyInfo(Variant::STRING, "peer_id")));
        ADD_SIGNAL(MethodInfo("peer_user_data_changed", PropertyInfo(Variant::STRING, "peer_id")));
        ADD_SIGNAL(MethodInfo("media_added", PropertyInfo(Variant::STRING, "peer_id"), PropertyInfo(Variant::STRING, "media_id")));
        ADD_SIGNAL(MethodInfo("media_removed", PropertyInfo(Variant::STRING, "peer_id"), PropertyInfo(Variant::STRING, "media_id")));
        ADD_SIGNAL(MethodInfo("media_active_state_changed", PropertyInfo(Variant::STRING, "peer_id"), PropertyInfo(Variant::STRING, "media_id"), PropertyInfo(Variant::BOOL, "active")));
        ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::STRING, "peer_id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "message")));
    }

    void OdinManager::startup() const {
        const bool res = odin_startup(ODIN_VERSION);
        if (!res) {
            UtilityFunctions::print("Failed to initialize ODIN library.");
            return;
        }

        UtilityFunctions::print("ODIN manager initialized.");
    }

    void OdinManager::shutdown() {
        if (is_connected) {
            leave_room();
        }
        odin_shutdown();
        UtilityFunctions::print("ODIN manager shut down.");
    }

    void OdinManager::join_room(const String& server_url, const String& room_token) {

        if (is_connected) {
            leave_room();
        }

        room_handle = reinterpret_cast<OdinRoomHandle *>(odin_room_create());
        if (!room_handle) {
            UtilityFunctions::print("Failed to create ODIN room.");
            return;
        }

        odin_room_set_event_callback(*room_handle, handle_odin_event, this);

        const OdinReturnCode result = odin_room_join(*room_handle, server_url.utf8().get_data(), room_token.utf8().get_data());
        if (odin_is_error(result)) {
            UtilityFunctions::print("Failed to join ODIN room: ", get_error_message(result));
            odin_room_destroy(*room_handle);
            room_handle = nullptr;
            return;
        }
        // TODO: idk what the error code is yet...

        is_connected = true;
        UtilityFunctions::print("Joined ODIN room successfully");
    }

    void OdinManager::leave_room() {
        if (!is_connected || !room_handle) {
            return;
        }

        odin_room_close(*room_handle);
        odin_room_destroy(*room_handle);
        room_handle = nullptr;
        is_connected = false;

        UtilityFunctions::print("Leaving ODIN room successfully");
        emit_signal("room_left");
    }

    String OdinManager::generate_room_token(const String& room_id, const String &user_id) const {
        if (access_key.is_empty()) {
            UtilityFunctions::print("No access key provided");
            return "";
        }

        OdinTokenGenerator *generator = odin_token_generator_create(access_key.utf8().get_data());
        if (!generator) {
            UtilityFunctions::print("Failed to create token generator");
            return "";
        }

        char token[512];
        const OdinReturnCode result = odin_token_generator_create_token(generator, room_id.utf8().get_data(), user_id.utf8().get_data(), token, sizeof(token));
        if (odin_is_error(result)) {
            UtilityFunctions::print("Failed to generate token: ", get_error_message(result));
            odin_token_generator_destroy(generator);
            return "";
        }

        return String(token);
    }

    void OdinManager::configure_audio_processing() const {
        if (!is_connected || !room_handle) {
            return;
        }

        const OdinApmConfig apm_config = {
            .voice_activity_detection = voice_activity_detection_enabled,
            .voice_activity_detection_attack_probability = voice_activity_detection_attack_probability,
            .voice_activity_detection_release_probability = voice_activity_detection_release_probability,
            .volume_gate = volume_gate_enabled,
            .volume_gate_attack_loudness = volume_gate_attack_loudness,
            .volume_gate_release_loudness = volume_gate_release_loudness,
            .echo_canceller = echo_canceller_enabled,
            .high_pass_filter = high_pass_filter_enabled,
            .noise_suppression_level = noise_suppression_level,
            .transient_suppressor = transient_suppressor_enabled,
        };

        const OdinReturnCode result = odin_room_configure_apm(*room_handle, apm_config);
        if (odin_is_error(result)) {
            UtilityFunctions::print("Failed to configure audio processing: ", get_error_message(result));
            return;
        }
    }

    void OdinManager::set_microphone_active(const bool active) {
        microphone_active = active;

        if (!is_connected || !room_handle) {
            return;
        }

        if (active) {
            if (!local_audio_stream) {
                local_audio_stream = new OdinMediaStreamHandle(odin_audio_stream_create({.channel_count = 1, .sample_rate = 48000 }));

                if (!local_audio_stream) {
                    UtilityFunctions::print("Failed to create local media stream");
                    return;
                }

                const OdinReturnCode res = odin_room_add_media(*room_handle, *local_audio_stream);
                if (odin_is_error(res)) {
                    UtilityFunctions::print("Failed to add local media stream to room: ", get_error_message(res));
                    odin_media_stream_destroy(*local_audio_stream);
                    local_audio_stream = nullptr;
                    return;
                }

                setup_audio_capture();
                UtilityFunctions::print("Local microphone added to room");


            }
        } else if (local_audio_stream) {
            cleanup_audio_capture();
            odin_media_stream_destroy(*local_audio_stream);
            local_audio_stream = nullptr;
            UtilityFunctions::print("Microphone deactivated");
        }
    }

    bool OdinManager::get_microphone_active() const {
        return microphone_active;
    }

    void OdinManager::set_voice_activity_detection_enabled(const bool enabled) {
        voice_activity_detection_enabled = enabled;
    }

    bool OdinManager::get_voice_activity_detection_enabled() const {
        return voice_activity_detection_enabled;
    }

    void OdinManager::set_voice_activity_detection_attack_probability(const float probability) {
        voice_activity_detection_attack_probability = probability;
    }

    float OdinManager::get_voice_activity_detection_attack_probability() const {
        return voice_activity_detection_attack_probability;
    }

    void OdinManager::set_voice_activity_detection_release_probability(const float probability) {
        voice_activity_detection_release_probability = probability;
    }

    float OdinManager::get_voice_activity_detection_release_probability() const {
        return voice_activity_detection_release_probability;
    }

    void OdinManager::set_volume_gate_enabled(const bool enabled) {
        volume_gate_enabled = enabled;
    }

    bool OdinManager::get_volume_gate_enabled() const {
        return volume_gate_enabled;
    }

    void OdinManager::set_volume_gate_attack_loudness(const float loudness) {
        volume_gate_attack_loudness = loudness;
    }

    float OdinManager::get_volume_gate_attack_loudness() const {
        return volume_gate_attack_loudness;
    }

    void OdinManager::set_volume_gate_release_loudness(const float loudness) {
        volume_gate_release_loudness = loudness;
    }

    float OdinManager::get_volume_gate_release_loudness() const {
        return volume_gate_release_loudness;
    }

    void OdinManager::set_echo_canceller_enabled(const bool enabled) {
        echo_canceller_enabled = enabled;
    }

    bool OdinManager::get_echo_canceller_enabled() const {
        return echo_canceller_enabled;
    }

    void OdinManager::set_high_pass_filter_enabled(const bool enabled) {
        high_pass_filter_enabled = enabled;
    }

    bool OdinManager::get_high_pass_filter_enabled() const {
        return high_pass_filter_enabled;
    }

    void OdinManager::set_noise_suppression_level(int level) {
        noise_suppression_level = static_cast<OdinNoiseSuppressionLevel>(level);
    }

    int OdinManager::get_noise_suppression_level() const {
        return static_cast<int>(noise_suppression_level);
    }

    void OdinManager::set_transient_suppressor_enabled(const bool enabled) {
        transient_suppressor_enabled = enabled;
    }

    bool OdinManager::get_transient_suppressor_enabled() const {
        return transient_suppressor_enabled;
    }

    bool OdinManager::is_room_connected() const {
        return is_connected;
    }

    Array OdinManager::get_peers_in_room() const {
        // get peers from peers
        Array peers_array;

        if (!is_connected || !room_handle) {
            return peers_array;
        }

        for (const auto& peer : peers) {
            Dictionary peer_info;

            peer_info["id"] = String::num_int64(peer.second.id);
            peer_info["is_connected"] = peer.second.is_connected;
            peers_array.push_back(peer_info);
        }
        return peers_array;
    }

    void OdinManager::set_access_key(const String& key) {
        access_key = key;
    }

    String OdinManager::get_access_key() const {
        return access_key;
    }

    void OdinManager::set_server_url(const String& url) {
        server_url = url;
    }

    String OdinManager::get_server_url() const {
        return server_url;
    }

    void OdinManager::set_room_id(const String& id) {
        room_id = id;
    }

    String OdinManager::get_room_id() const {
        return room_id;
    }

    void OdinManager::set_user_id(const String& id) {
        user_id = id;
    }

    String OdinManager::get_user_id() const {
        return user_id;
    }


    void OdinManager::setup_audio_capture() {
        capture_buffer = memnew_arr(float, BUFFER_SIZE);
        microphone_stream.instantiate();

        microphone_player = memnew(AudioStreamPlayer);
        add_child(microphone_player);
        microphone_player->set_stream(microphone_stream);

        mic_bus_index = AudioServer::get_singleton()->get_bus_count();
        AudioServer::get_singleton()->add_bus();
        AudioServer::get_singleton()->set_bus_name(mic_bus_index, "Microphone");

        audio_capture.instantiate();
        AudioServer::get_singleton()->add_bus_effect(mic_bus_index, audio_capture);

        microphone_player->set_bus("Microphone");
        microphone_player->play();

        UtilityFunctions::print("Microphone capture setup complete");
    }

    void OdinManager::cleanup_audio_capture() {
        if (microphone_player) {
            microphone_player->stop();
            remove_child(microphone_player);
            memdelete(microphone_player);
            microphone_player = nullptr;
        }

        if (mic_bus_index >= 0) {
            AudioServer::get_singleton()->remove_bus(mic_bus_index);
            mic_bus_index = -1;
        }

        if (capture_buffer) {
            memdelete_arr(capture_buffer);
            capture_buffer = nullptr;
        }

        microphone_stream.unref();
        audio_capture.unref();

        UtilityFunctions::print("Microphone capture release complete");
    }

    void OdinManager::process_microphone_audio() const {
        if (!is_connected || !room_handle || !local_audio_stream || !audio_capture.is_valid() || !microphone_active) {
            return;
        }

        const int available_frames = audio_capture->get_frames_available();

        if (available_frames <= 0) {
            return;
        }

        PackedVector2Array captured_audio = audio_capture->get_buffer(available_frames);

        const int frame_count = captured_audio.size();
        const int max_frames = MIN(frame_count, BUFFER_SIZE);

        for (int i = 0; i < max_frames; i++) {
            capture_buffer[i] = (captured_audio[i].x + captured_audio[i].y) * 0.5f;
        }

        const OdinReturnCode return_code = odin_audio_push_data(
            *local_audio_stream,
            capture_buffer,
            max_frames
        );

        if (odin_is_error(return_code)) {
            static uint64_t last_error_time = 0;
            const uint64_t current_time = Time::get_singleton()->get_ticks_msec();
            if (current_time - last_error_time > 5000) {
                UtilityFunctions::print("Error pushing audio data: ", get_error_message(return_code));
                last_error_time = current_time;
            }
        }

        audio_capture->clear_buffer();
    }

    void OdinManager::update_audio_playback(const OdinMediaStreamHandle stream_handle, const float* audio_buffer, const size_t buffer_size) {
        const Ref<AudioStreamGeneratorPlayback> playback = get_or_create_playback(stream_handle);

        if (playback.is_null()) {
            UtilityFunctions::print("Failed to get or create audio playback");
            return;
        }

        for (size_t i = 0; i < buffer_size; i++) {
            const float sample = CLAMP(audio_buffer[i], -1.0f, 1.0f);
            playback->push_frame(Vector2(sample, sample));
        }
    }

    Ref<AudioStreamGeneratorPlayback> OdinManager::get_or_create_playback(const OdinMediaStreamHandle stream) {
        if (stream_playbacks.find(stream) != stream_playbacks.end()) {
            return stream_playbacks[stream];
        }

        Ref<AudioStreamGenerator> generator;
        generator.instantiate();
        generator->set_mix_rate(SAMPLE_RATE);
        generator->set_buffer_length(0.1);

        AudioStreamPlayer* player = memnew(AudioStreamPlayer);
        add_child(player);
        player->set_stream(generator);
        player->play();

        Ref<AudioStreamGeneratorPlayback> playback = player->get_stream_playback();

        stream_generators[stream] = generator;
        stream_playbacks[stream] = playback;

        audio_streams[stream] = player;

        return playback;
    }

    String OdinManager::get_error_message(const OdinReturnCode error_code) const {
        char error_buffer[256] = {0};
        odin_error_format(error_code, error_buffer, sizeof(error_buffer));
        return String(error_buffer);
    }

    void OdinManager::set_spatial_target(const NodePath& path) {
        spatial_target_path = path;
        spatial_target = nullptr;

        if (spatial_target_path.is_empty()) {
            return;
        }

        Node* node = get_node_or_null(spatial_target_path);
        if (node) {
            spatial_target = Object::cast_to<Node3D>(node);
            if (!spatial_target) {
                UtilityFunctions::print("Warning: Node at path ", spatial_target_path, " is not a Node3D");
            }
        } else {
            UtilityFunctions::print("Warning: Could not find node at path ", spatial_target_path);
        }
    }

    NodePath OdinManager::get_spatial_target() const {
        return spatial_target_path;
    }

    // peer mgmt
    void OdinManager::set_own_peer_id(const uint64_t id) {
        own_peer_id = id;
    }


    uint64_t OdinManager::get_own_peer_id() const {
        return own_peer_id;
    }


    void OdinManager::add_peer(const uint64_t peer_id) {
        if (peers.find(peer_id) == peers.end()) {
            PeerInfo peer_info;
            peer_info.id = peer_id;
            peer_info.is_connected = true;
            peers[peer_id] = peer_info;

            UtilityFunctions::print("Peer joined: ", peer_id);
        }
    }

    void OdinManager::remove_peer(const uint64_t peer_id) {
        const auto it = peers.find(peer_id);
        if (it != peers.end()) {
            for (OdinMediaStreamHandle media : it->second.media_streams) {
                media_to_peer.erase(media);

                auto stream_it = audio_streams.find(media);
                if (stream_it != audio_streams.end()) {
                    AudioStreamPlayer* player = stream_it->second;
                    if (player) {
                        player->stop();
                        remove_child(player);
                        memdelete(player);
                    }
                    audio_streams.erase(stream_it);
                }

                stream_generators.erase(media);
                stream_playbacks.erase(media);
            }

            peers.erase(it);
            UtilityFunctions::print("Peer left: ", String::num_uint64(peer_id));
        }

    }

    void OdinManager::update_peer_user_data(const uint64_t peer_id, const uint8_t *data, const size_t data_len) {
        const auto it = peers.find(peer_id);
        if (it != peers.end()) {
            it->second.user_data.resize(data_len);
            if (data_len > 0) {
                memcpy(it->second.user_data.ptrw(), data, data_len);
            }
            UtilityFunctions::print("Peer user data updated: ", String::num_uint64(peer_id));
        }
    }

    void OdinManager::add_media_stream(const uint64_t peer_id, const OdinMediaStreamHandle media) {
        media_to_peer[media] = peer_id;
        const auto it = peers.find(peer_id);
        if (it != peers.end()) {
            it->second.media_streams.push_back(media);
        }
        const OdinMediaStreamType media_type = odin_media_stream_type(media);
        if (media_type == OdinMediaStreamType_Audio) {
            get_or_create_playback(media);
            UtilityFunctions::print("Audio media added from peer: ", String::num_uint64(peer_id));
        } else {
            // unsupported. log warning
            UtilityFunctions::print("Warning: Unsupported media type from peer: ", String::num_uint64(peer_id));
        }
    }

    void OdinManager::remove_media_stream(const uint64_t peer_id, const OdinMediaStreamHandle media) {
        const auto peer_it = peers.find(peer_id);
        if (peer_it != peers.end()) {
            auto& media_list = peer_it->second.media_streams;
            media_list.erase(
                std::remove(media_list.begin(), media_list.end(), media),
                media_list.end()
            );
        }
        media_to_peer.erase(media);
        const auto stream_it = audio_streams.find(media);
        if (stream_it != audio_streams.end()) {
            AudioStreamPlayer* player = stream_it->second;
            if (player) {
                player->stop();
                remove_child(player);
                memdelete(player);
            }
            audio_streams.erase(stream_it);
        }

        stream_generators.erase(media);
        stream_playbacks.erase(media);

        UtilityFunctions::print("Media removed from peer: ", String::num_uint64(peer_id));
    }

    void OdinManager::update_media_active_state(const uint64_t peer_id, const OdinMediaStreamHandle media, const bool active) {
        const auto stream_it = audio_streams.find(media);
        if (stream_it != audio_streams.end()) {
            AudioStreamPlayer* player = stream_it->second;
            if (player) {
                if (active) {
                    // Resume playback
                    player->play();
                } else {
                    // Pause playback
                    player->stop();
                }
            }
        }
        UtilityFunctions::print("Media active state changed for peer ", String::num_uint64(peer_id), " to ", active ? "active" : "inactive");
    }




}


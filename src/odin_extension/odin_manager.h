#ifndef ODIN_MANAGER_H
#define ODIN_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_microphone.hpp>
#include <godot_cpp/classes/audio_effect_capture.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>

#include "odin.h"

#include <map>
#include <vector>


namespace godot {
struct PeerInfo {
    uint64_t id;
    PackedByteArray user_data;
    bool is_connected;
    std::vector<OdinMediaStreamHandle> media_streams;
};

class OdinManager : public Node {
    GDCLASS(OdinManager, Node);
private:
    NodePath spatial_target_path;
    Node3D* spatial_target = nullptr;

    OdinRoomHandle room_handle;
    OdinMediaStreamHandle* local_audio_stream;

    String access_key;
    String server_url;
    String room_id;
    String user_id;

    uint64_t own_peer_id = 0;

    bool is_connected;
    bool microphone_active;
    bool use_spatial_audio;

    bool voice_activity_detection_enabled;
    float voice_activity_detection_attack_probability;
    float voice_activity_detection_release_probability;
    bool volume_gate_enabled;
    float volume_gate_attack_loudness;
    float volume_gate_release_loudness;
    bool echo_canceller_enabled;
    bool high_pass_filter_enabled;
    OdinNoiseSuppressionLevel noise_suppression_level;
    bool transient_suppressor_enabled;

    std::map<OdinMediaStreamHandle, AudioStreamPlayer*> audio_streams;
    std::map<uint64_t, PeerInfo> peers;
    std::map<OdinMediaStreamHandle, uint64_t> media_to_peer;

    std::map<OdinMediaStreamHandle, Ref<AudioStreamGeneratorPlayback>> stream_playbacks;
    std::map<OdinMediaStreamHandle, Ref<AudioStreamGenerator>> stream_generators;
    const int SAMPLE_RATE = 48000;
    Ref<AudioStreamGeneratorPlayback> get_or_create_playback(OdinMediaStreamHandle stream);

    Ref<AudioStreamMicrophone> microphone_stream;
    AudioStreamPlayer* microphone_player = nullptr;
    Ref<AudioEffectCapture> audio_capture;
    int mic_bus_index = -1;

    float* capture_buffer = nullptr;
    const int BUFFER_SIZE = 8192;

    void update_audio_playback(OdinMediaStreamHandle stream_handle, const float* audio_buffer, size_t buffer_size);
    void setup_audio_capture();
    void cleanup_audio_capture();
    void process_microphone_audio() const;
    void configure_audio_processing() const;

    String get_error_message(OdinReturnCode error_code) const;

protected:
    static void _bind_methods();
public:
    OdinManager();
    ~OdinManager() override;

    void _process(double delta) override;
    void startup() const;
    void shutdown();
    void join_room(const String& server_url, const String& room_token);
    void leave_room();
    String generate_room_token(const String& room_id, const String& user_id) const;

    void set_own_peer_id(uint64_t id);
    uint64_t get_own_peer_id() const;

    void add_peer(uint64_t peer_id);
    void remove_peer(uint64_t peer_id);
    void update_peer_user_data(uint64_t peer_id, const uint8_t* data, size_t data_len);

    void add_media_stream(uint64_t peer_id, OdinMediaStreamHandle media);
    void remove_media_stream(uint64_t peer_id, OdinMediaStreamHandle media);
    void update_media_active_state(uint64_t peer_id, OdinMediaStreamHandle media, bool active);

    void set_microphone_active(bool active);
    bool get_microphone_active() const;

    void set_voice_activity_detection_enabled(bool enabled);
    bool get_voice_activity_detection_enabled() const;

    void set_voice_activity_detection_attack_probability(float probability);
    float get_voice_activity_detection_attack_probability() const;

    void set_voice_activity_detection_release_probability(float probability);
    float get_voice_activity_detection_release_probability() const;

    void set_volume_gate_enabled(bool enabled);
    bool get_volume_gate_enabled() const;

    void set_volume_gate_attack_loudness(float loudness);
    float get_volume_gate_attack_loudness() const;

    void set_volume_gate_release_loudness(float loudness);
    float get_volume_gate_release_loudness() const;

    void set_echo_canceller_enabled(bool enabled);
    bool get_echo_canceller_enabled() const;

    void set_high_pass_filter_enabled(bool enabled);
    bool get_high_pass_filter_enabled() const;

    void set_noise_suppression_level(int level);
    int get_noise_suppression_level() const;

    void set_transient_suppressor_enabled(bool enabled);
    bool get_transient_suppressor_enabled() const;

    void set_access_key(const String& key);
    String get_access_key() const;

    void set_server_url(const String& url);
    String get_server_url() const;

    void set_room_id(const String& id);
    String get_room_id() const;

    void set_user_id(const String& id);
    String get_user_id() const;

    bool is_room_connected() const;
    Array get_peers_in_room() const;

    void set_spatial_target(const NodePath& path);
    NodePath get_spatial_target() const;
};
}

#endif // ODIN_MANAGER_H
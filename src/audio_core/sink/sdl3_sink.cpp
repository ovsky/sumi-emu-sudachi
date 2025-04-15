// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>
#include <vector>
#include <SDL3/SDL.h>

#include "audio_core/common/common.h"
#include "audio_core/sink/sdl3_sink.h"
#include "audio_core/sink/sink_stream.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"

namespace AudioCore::Sink {
/**
 * SDL sink stream, responsible for sinking samples to hardware.
 */
class SDLSinkStream final : public SinkStream {
public:
    /**
     * Create a new sink stream.
     *
     * @param device_channels_ - Number of channels supported by the hardware.
     * @param system_channels_ - Number of channels the audio systems expect.
     * @param output_device    - Name of the output device to use for this stream.
     * @param input_device     - Name of the input device to use for this stream.
     * @param type_            - Type of this stream.
     * @param system_          - Core system.
     * @param event            - Event used only for audio renderer, signalled on buffer consume.
     */
    SDLSinkStream(u32 device_channels_, u32 system_channels_, const std::string& output_device,
                  const std::string& input_device, StreamType type_, Core::System& system_)
        : SinkStream{system_, type_} {
        system_channels = system_channels_;
        device_channels = device_channels_;

        SDL_AudioSpec spec{};
        spec.channels = static_cast<u8>(device_channels);
        spec.freq = TargetSampleRate;
        spec.format = SDL_AUDIO_S16;

        std::string device_name{output_device};
        bool capture{false};
        if (type == StreamType::In) {
            device_name = input_device;
            capture = true;
        }

        if (device_name.empty())
            stream = SDL_OpenAudioDeviceStream(capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                                       : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                               &spec, &DataCallback, this);
        else {
            int count = 0;
            SDL_AudioDeviceID* devices = nullptr;
            if (capture)
                devices = SDL_GetAudioRecordingDevices(&count);
            else
                devices = SDL_GetAudioPlaybackDevices(&count);

            for (int i = 0; i < count; ++i)
                if (auto deviceID = devices[i]; SDL_GetAudioDeviceName(deviceID) == device_name)
                    stream = SDL_OpenAudioDeviceStream(deviceID, &spec, &DataCallback, this);
        }

        if (stream == nullptr) {
            LOG_CRITICAL(Audio_Sink, "Error opening SDL audio stream: {}", SDL_GetError());
            return;
        }
    }

    /**
     * Destroy the sink stream.
     */
    ~SDLSinkStream() override {
        LOG_DEBUG(Service_Audio, "Destructing SDL stream {}", name);
        Finalize();
    }

    /**
     * Finalize the sink stream.
     */
    void Finalize() override {
        if (stream == nullptr) {
            return;
        }

        Stop();
        SDL_DestroyAudioStream(stream);
    }

    /**
     * Start the sink stream.
     *
     * @param resume - Set to true if this is resuming the stream a previously-active stream.
     *                 Default false.
     */
    void Start(bool resume = false) override {
        if (stream == nullptr || !paused)
            return;

        paused = false;
        SDL_ResumeAudioStreamDevice(stream);
    }

    /**
     * Stop the sink stream.
     */
    void Stop() override {
        if (stream == nullptr || paused)
            return;

        SignalPause();
        SDL_PauseAudioStreamDevice(stream);
    }

private:
    /**
     * Main callback from SDL. Either expects samples from us (audio render/audio out), or will
     * provide samples to be copied (audio in).
     *
     * @param userdata - Custom data pointer passed along, points to a SDLSinkStream.
     * @param stream   - Buffer of samples to be filled or read.
     * @param len      - Length of the stream in bytes.
     */
    static void DataCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
                             int total_amount) {
        auto* impl = static_cast<SDLSinkStream*>(userdata);

        if (!impl) {
            return;
        }

        const std::size_t num_channels = impl->GetDeviceChannels();
        const std::size_t frame_size = num_channels;
        const std::size_t num_frames{total_amount / num_channels / sizeof(s16)};

        if (impl->type == StreamType::In) {
            std::span<const s16> input_buffer{reinterpret_cast<const s16*>(stream),
                                              num_frames * frame_size};
            impl->ProcessAudioIn(input_buffer, num_frames);
        } else {
            std::span<s16> output_buffer{reinterpret_cast<s16*>(stream), num_frames * frame_size};
            impl->ProcessAudioOutAndRender(output_buffer, num_frames);
        }
    }

    /// SDL device id of the opened input/output device
    SDL_AudioStream* stream = nullptr;
};

SDLSink::SDLSink(std::string_view target_device_name) {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == false) {
            LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem audio failed: {}", SDL_GetError());
            return;
        }
    }

    if (target_device_name != auto_device_name && !target_device_name.empty()) {
        output_device = target_device_name;
    } else {
        output_device.clear();
    }

    device_channels = 2;
}

SDLSink::~SDLSink() = default;

SinkStream* SDLSink::AcquireSinkStream(Core::System& system, u32 system_channels_,
                                       const std::string&, StreamType type) {
    system_channels = system_channels_;
    SinkStreamPtr& stream = sink_streams.emplace_back(std::make_unique<SDLSinkStream>(
        device_channels, system_channels, output_device, input_device, type, system));
    return stream.get();
}

void SDLSink::CloseStream(SinkStream* stream) {
    for (size_t i = 0; i < sink_streams.size(); i++) {
        if (sink_streams[i].get() == stream) {
            sink_streams[i].reset();
            sink_streams.erase(sink_streams.begin() + i);
            break;
        }
    }
}

void SDLSink::CloseStreams() {
    sink_streams.clear();
}

f32 SDLSink::GetDeviceVolume() const {
    if (sink_streams.empty()) {
        return 1.0f;
    }

    return sink_streams[0]->GetDeviceVolume();
}

void SDLSink::SetDeviceVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetDeviceVolume(volume);
    }
}

void SDLSink::SetSystemVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetSystemVolume(volume);
    }
}

std::vector<std::string> ListSDLSinkDevices(bool capture) {
    std::vector<std::string> device_list;

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == false) {
            LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem audio failed: {}", SDL_GetError());
            return {};
        }
    }

    int count = 0;
    auto devices = SDL_GetAudioPlaybackDevices(&count);

    for (int i = 0; i < count; ++i)
        if (auto name = SDL_GetAudioDeviceName(devices[i]))
            device_list.emplace_back(name);

    return device_list;
}

bool IsSDLSuitable() {
#if !defined(HAVE_SDL3)
    return false;
#else
    // Check SDL can init
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == false) {
            LOG_ERROR(Audio_Sink, "SDL failed to init, it is not suitable. Error: {}",
                      SDL_GetError());
            return false;
        }
    }

    // We can set any latency frequency we want with SDL, so no need to check that.

    // Check we can open a device with standard parameters
    SDL_AudioSpec spec{};
    spec.freq = TargetSampleRate;
    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;

    SDL_AudioSpec obtained{};
    auto stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

    if (stream == nullptr) {
        LOG_ERROR(Audio_Sink, "SDL failed to open a stream, it is not suitable. Error: {}",
                  SDL_GetError());
        return false;
    }

    SDL_DestroyAudioStream(stream);
    return true;
#endif
}

} // namespace AudioCore::Sink

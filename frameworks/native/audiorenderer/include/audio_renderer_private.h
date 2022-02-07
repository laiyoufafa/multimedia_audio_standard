/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_RENDERER_PRIVATE_H
#define AUDIO_RENDERER_PRIVATE_H

#include "audio_interrupt_callback.h"
#include "audio_renderer.h"
#include "audio_stream.h"

namespace OHOS {
namespace AudioStandard {
constexpr uint32_t INVALID_SESSION_ID = static_cast<uint32_t>(-1);

class AudioRendererPrivate : public AudioRenderer {
public:
    int32_t GetFrameCount(uint32_t &frameCount) const override;
    int32_t GetLatency(uint64_t &latency) const override;
    int32_t SetParams(const AudioRendererParams params) override;
    int32_t GetParams(AudioRendererParams &params) const override;
    int32_t GetRendererInfo(AudioRendererInfo &rendererInfo) const override;
    int32_t GetStreamInfo(AudioStreamInfo &streamInfo) const override;
    bool Start() override;
    int32_t Write(uint8_t *buffer, size_t bufferSize) override;
    RendererState GetStatus() const override;
    bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const override;
    bool Drain() const override;
    bool Pause() const override;
    bool Stop() const override;
    bool Flush() const override;
    bool Release() const override;
    int32_t GetBufferSize(size_t &bufferSize) const override;
    int32_t SetAudioRendererDesc(AudioRendererDesc audioRendererDesc) const override;
    int32_t SetStreamType(AudioStreamType audioStreamType) const override;
    int32_t SetVolume(float volume) const override;
    float GetVolume() const override;
    int32_t SetRenderRate(AudioRendererRate renderRate) const override;
    AudioRendererRate GetRenderRate() const override;
    int32_t SetRendererCallback(const std::shared_ptr<AudioRendererCallback> &callback) override;

    std::shared_ptr<AudioStream> audioStream_;
    AudioRendererInfo rendererInfo_ = {};

    explicit AudioRendererPrivate(AudioStreamType audioStreamType);
    ~AudioRendererPrivate();

private:
    std::shared_ptr<AudioRendererCallback> callback_ = nullptr;
    std::shared_ptr<AudioInterruptCallback> audioInterruptCallback_ = nullptr;
    AudioInterrupt audioInterrupt_ =
        {STREAM_USAGE_UNKNOWN, CONTENT_TYPE_UNKNOWN, AudioStreamType::STREAM_DEFAULT, 0};
    uint32_t sessionID_ = INVALID_SESSION_ID;
};

class AudioInterruptCallbackImpl : public AudioInterruptCallback {
public:
    explicit AudioInterruptCallbackImpl(const std::shared_ptr<AudioStream> &audioStream,
        const AudioInterrupt &audioInterrupt);
    virtual ~AudioInterruptCallbackImpl();

    void OnInterrupt(const InterruptEvent &interruptEvent) override;
    void SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback);
private:
    void NotifyEvent(const InterruptEvent &interruptEvent);
    void HandleAndNotifyForcedEvent(const InterruptEvent &interruptEvent);
    void NotifyForcePausedToResume(const InterruptEvent &interruptEvent);
    bool HandleForceDucking(const InterruptEvent &interruptEvent);
    std::shared_ptr<AudioStream> audioStream_;
    std::weak_ptr<AudioRendererCallback> callback_;
    std::shared_ptr<AudioRendererCallback> cb;
    AudioInterrupt audioInterrupt_ {};
    bool isForcePaused_ = false;
    bool isForceDucked_ = false;
    float instanceVolBeforeDucking_ = 0.2f;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERER_PRIVATE_H
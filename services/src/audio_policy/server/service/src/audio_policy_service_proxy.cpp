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

#include "audio_policy_service_proxy.h"
#include "media_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioPolicyServiceProxy::AudioPolicyServiceProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioService>(impl)
{
}

int32_t AudioPolicyServiceProxy::GetMaxVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    return 0;
}

int32_t AudioPolicyServiceProxy::GetMinVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    return 0;
}

int32_t AudioPolicyServiceProxy::SetMicrophoneMute(bool isMute)
{
    return 0;
}

bool AudioPolicyServiceProxy::IsMicrophoneMute()
{
    return false;
}

int32_t AudioPolicyServiceProxy::SetAudioScene(list<DeviceType> &activeDeviceList, AudioScene audioScene)
{
    MEDIA_DEBUG_LOG("[AudioPolicyServiceProxy] SetAudioScene %{public}d", audioScene);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    int32_t size = activeDeviceList.size();
    MEDIA_DEBUG_LOG("[AudioPolicyServiceProxy] Size of active device list %{public}d", size);
    data.WriteInt32(size);
    for (auto i = activeDeviceList.begin(); i != activeDeviceList.end(); ++i) {
        data.WriteInt32(static_cast<int32_t>(*i));
    }

    data.WriteInt32(static_cast<int32_t>(audioScene));

    int32_t error = Remote()->SendRequest(SET_AUDIO_SCENE, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("SetAudioScene failed, error: %d", error);
        return false;
    }

    int32_t result = reply.ReadInt32();
    MEDIA_DEBUG_LOG("[AudioPolicyServiceProxy] SetAudioScene result %{public}d", result);
    return result;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyServiceProxy::GetDevices(DeviceFlag deviceFlag)
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;
    return deviceInfo;
}

const std::string AudioPolicyServiceProxy::GetAudioParameter(const std::string key)
{
    return "";
}

void AudioPolicyServiceProxy::SetAudioParameter(const std::string key, const std::string value)
{
    return;
}
} // namespace AudioStandard
} // namespace OHOS
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

#include "audio_capturer_napi.h"
#include "audio_manager_napi.h"
#include "audio_parameters_napi.h"
#include "hilog/log.h"

#include "securec.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_capturerConstructor = nullptr;
std::unique_ptr<AudioParameters> AudioCapturerNapi::sAudioParameters_ = nullptr;
std::unique_ptr<AudioCapturerOptions> AudioCapturerNapi::sAudioCapturerOptions_ = nullptr;
napi_ref AudioCapturerNapi::capturerState_ = nullptr;
napi_ref AudioCapturerNapi::sampleFormat_ = nullptr;

namespace {
    const int ARGS_ONE = 1;
    const int ARGS_TWO = 2;
    const int ARGS_THREE = 3;

    const int PARAM0 = 0;
    const int PARAM1 = 1;
    const int PARAM2 = 2;

    const int ERROR = -1;
    const int SUCCESS = 0;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioCapturerNapi"};

#define GET_PARAMS(env, info, num) \
    size_t argc = num;             \
    napi_value argv[num] = {0};    \
    napi_value thisVar = nullptr;  \
    void *data;                    \
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data)
}

AudioCapturerNapi::AudioCapturerNapi()
    : audioCapturer_(nullptr), contentType_(CONTENT_TYPE_MUSIC), streamUsage_(STREAM_USAGE_MEDIA),
      deviceRole_(INPUT_DEVICE), deviceType_(DEVICE_TYPE_MIC), sourceType_(SOURCE_TYPE_MIC),
      capturerFlags_(0), env_(nullptr), wrapper_(nullptr) {}

AudioCapturerNapi::~AudioCapturerNapi()
{
    if (wrapper_ != nullptr) {
        napi_delete_reference(env_, wrapper_);
    }
}

void AudioCapturerNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioCapturerNapi *>(nativeObject);
        delete obj;
    }
}

static AudioSampleFormat GetNativeAudioSampleFormat(int32_t napiSampleFormat)
{
    AudioSampleFormat format = INVALID_WIDTH;

    switch (napiSampleFormat) {
        case AudioCapturerNapi::SAMPLE_FORMAT_U8:
            format = SAMPLE_U8;
            break;
        case AudioCapturerNapi::SAMPLE_FORMAT_S16LE:
            format = SAMPLE_S16LE;
            break;
        case AudioCapturerNapi::SAMPLE_FORMAT_S24LE:
            format = SAMPLE_S24LE;
            break;
        case AudioCapturerNapi::SAMPLE_FORMAT_S32LE:
            format = SAMPLE_S32LE;
            break;
        default:
            format = INVALID_WIDTH;
            HiLog::Error(LABEL, "Unknown sample format requested by JS, Set it to default INVALID_WIDTH!");
            break;
    }

    return format;
}

static AudioCapturerNapi::AudioSampleFormat GetJsAudioSampleFormat(int32_t nativeSampleFormat)
{
    AudioCapturerNapi::AudioSampleFormat format = AudioCapturerNapi::SAMPLE_FORMAT_INVALID;

    switch (nativeSampleFormat) {
        case SAMPLE_U8:
            format = AudioCapturerNapi::AudioSampleFormat::SAMPLE_FORMAT_U8;
            break;
        case SAMPLE_S16LE:
            format = AudioCapturerNapi::AudioSampleFormat::SAMPLE_FORMAT_S16LE;
            break;
        case SAMPLE_S24LE:
            format = AudioCapturerNapi::AudioSampleFormat::SAMPLE_FORMAT_S24LE;
            break;
        case SAMPLE_S32LE:
            format = AudioCapturerNapi::AudioSampleFormat::SAMPLE_FORMAT_S32LE;
            break;
        default:
            format = AudioCapturerNapi::AudioSampleFormat::SAMPLE_FORMAT_INVALID;
            HiLog::Error(LABEL, "Unknown sample format returned from native, Set it to default SAMPLE_FORMAT_INVALID!");
            break;
    }

    return format;
}

napi_value AudioCapturerNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_capturer_properties[] = {
        DECLARE_NAPI_FUNCTION("getCapturerInfo", GetCapturerInfo),
        DECLARE_NAPI_FUNCTION("getStreamInfo", GetStreamInfo),
        DECLARE_NAPI_FUNCTION("start", Start),
        DECLARE_NAPI_FUNCTION("read", Read),
        DECLARE_NAPI_FUNCTION("getAudioTime", GetAudioTime),
        DECLARE_NAPI_FUNCTION("stop", Stop),
        DECLARE_NAPI_FUNCTION("release", Release),
        DECLARE_NAPI_FUNCTION("getBufferSize", GetBufferSize),
        DECLARE_NAPI_GETTER("state", GetState)
    };

    napi_property_descriptor static_prop[] = {
        DECLARE_NAPI_STATIC_FUNCTION("createAudioCapturer", CreateAudioCapturer),
        DECLARE_NAPI_PROPERTY("AudioState", CreateCapturerStateObject(env)),
        DECLARE_NAPI_PROPERTY("AudioSampleFormat", CreateAudioSampleFormatObject(env)),
    };

    status = napi_define_class(env, AUDIO_CAPTURER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_capturer_properties) / sizeof(audio_capturer_properties[PARAM0]),
        audio_capturer_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, refCount, &g_capturerConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_CAPTURER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            status = napi_define_properties(env, exports,
                                            sizeof(static_prop) / sizeof(static_prop[PARAM0]), static_prop);
            if (status == napi_ok) {
                return exports;
            }
        }
    }

    HiLog::Error(LABEL, "Failure in AudioCapturerNapi::Init()");

    return result;
}

napi_value AudioCapturerNapi::CreateAudioSampleFormatObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (int i = AudioCapturerNapi::SAMPLE_FORMAT_INVALID; i <= AudioCapturerNapi::SAMPLE_FORMAT_S32LE; i++) {
            switch (i) {
                case AudioCapturerNapi::SAMPLE_FORMAT_INVALID:
                    propName = "SAMPLE_FORMAT_INVALID";
                    break;
                case AudioCapturerNapi::SAMPLE_FORMAT_U8:
                    propName = "SAMPLE_FORMAT_U8";
                    break;
                case AudioCapturerNapi::SAMPLE_FORMAT_S16LE:
                    propName = "SAMPLE_FORMAT_S16LE";
                    break;
                case AudioCapturerNapi::SAMPLE_FORMAT_S24LE:
                    propName = "SAMPLE_FORMAT_S24LE";
                    break;
                case AudioCapturerNapi::SAMPLE_FORMAT_S32LE:
                    propName = "SAMPLE_FORMAT_S32LE";
                    break;
                default:
                    HiLog::Error(LABEL, "CreateAudioSampleFormatObject: No prop with this value try next value!");
                    continue;
            }
            status = AddNamedProperty(env, result, propName, i);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &sampleFormat_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateAudioSampleFormatObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_status AudioCapturerNapi::AddNamedProperty(napi_env env, napi_value object,
                                                const std::string name, int32_t enumValue)
{
    napi_status status;
    napi_value enumNapiValue;

    status = napi_create_int32(env, enumValue, &enumNapiValue);
    if (status == napi_ok) {
        status = napi_set_named_property(env, object, name.c_str(), enumNapiValue);
    }

    return status;
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

napi_value AudioCapturerNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioCapturerNapi> obj = make_unique<AudioCapturerNapi>();
    if (obj != nullptr) {
        obj->env_ = env;
        obj->sourceType_ = sAudioCapturerOptions_->capturerInfo.sourceType;
        obj->capturerFlags_ = sAudioCapturerOptions_->capturerInfo.capturerFlags;
        obj->audioCapturer_ = AudioCapturer::Create(*sAudioCapturerOptions_);

        status = napi_wrap(env, thisVar, static_cast<void*>(obj.get()),
                           AudioCapturerNapi::Destructor, nullptr, &(obj->wrapper_));
        if (status == napi_ok) {
            obj.release();
            return thisVar;
        }
    }

    HiLog::Error(LABEL, "Failed in AudioCapturerNapi::Construct()!");
    return result;
}

napi_value AudioCapturerNapi::CreateAudioCapturer(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_status status;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);
    NAPI_ASSERT(env, argc >= ARGS_ONE, "requires 1 parameters minimum");

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    if (asyncContext == nullptr) {
        return result;
    }

    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);
        if (i == PARAM0 && valueType == napi_object) {
            if (!ParseCapturerOptions(env, argv[i], &(asyncContext->capturerOptions))) {
                HiLog::Error(LABEL, "Parsing of capturer options failed");
                return result;
            }
        } else if (i == PARAM1 && valueType == napi_function) {
            napi_create_reference(env, argv[i], REFERENCE_CREATION_COUNT, &asyncContext->callbackRef);
            break;
        } else {
            NAPI_ASSERT(env, false, "type mismatch");
        }
    }

    if (asyncContext->callbackRef == nullptr) {
        napi_create_promise(env, &asyncContext->deferred, &result);
    } else {
        napi_get_undefined(env, &result);
    }

    napi_value resource = nullptr;
    napi_create_string_utf8(env, "CreateAudioCapturer", NAPI_AUTO_LENGTH, &resource);

    status = napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            auto context = static_cast<AudioCapturerAsyncContext *>(data);
            context->status = SUCCESS;
        },
        GetCapturerAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
    if (status != napi_ok) {
        result = nullptr;
    } else {
        status = napi_queue_async_work(env, asyncContext->work);
        if (status == napi_ok) {
            asyncContext.release();
        } else {
            result = nullptr;
        }
    }

    return result;
}

napi_value AudioCapturerNapi::CreateCapturerStateObject(napi_env env)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_value result = nullptr;
    napi_status status;
    std::string propName;
    int32_t refCount = 1;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: audioCapturerStateMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop in CreateCapturerStateObject!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, refCount, &capturerState_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateCapturerStateObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

void AudioCapturerNapi::CommonCallbackRoutine(napi_env env, AudioCapturerAsyncContext* &asyncContext,
                                              const napi_value &valueParam)
{
    napi_value result[ARGS_TWO] = {0};
    napi_value retVal;

    if (!asyncContext->status) {
        napi_get_undefined(env, &result[PARAM0]);
        result[PARAM1] = valueParam;
    } else {
        napi_value message = nullptr;
        napi_create_string_utf8(env, "Error, Operation not supported or Failed", NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &result[PARAM0]);
        napi_get_undefined(env, &result[PARAM1]);
    }

    if (asyncContext->deferred) {
        if (!asyncContext->status) {
            napi_resolve_deferred(env, asyncContext->deferred, result[PARAM1]);
        } else {
            napi_reject_deferred(env, asyncContext->deferred, result[PARAM0]);
        }
    } else {
        napi_value callback = nullptr;
        napi_get_reference_value(env, asyncContext->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, asyncContext->callbackRef);
    }
    napi_delete_async_work(env, asyncContext->work);

    delete asyncContext;
    asyncContext = nullptr;
}

void AudioCapturerNapi::GetCapturerAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value valueParam = nullptr;
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            unique_ptr<AudioCapturerOptions> capturerOptions = make_unique<AudioCapturerOptions>();
            capturerOptions->streamInfo.samplingRate = asyncContext->capturerOptions.streamInfo.samplingRate;
            capturerOptions->streamInfo.encoding = asyncContext->capturerOptions.streamInfo.encoding;
            capturerOptions->streamInfo.format = asyncContext->capturerOptions.streamInfo.format;
            capturerOptions->streamInfo.channels = asyncContext->capturerOptions.streamInfo.channels;
            capturerOptions->capturerInfo.sourceType = asyncContext->capturerOptions.capturerInfo.sourceType;
            capturerOptions->capturerInfo.capturerFlags = asyncContext->capturerOptions.capturerInfo.capturerFlags;

            valueParam = CreateAudioCapturerWrapper(env, capturerOptions);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: GetCapturerAsyncCallbackComplete is Null!");
    }
}


void AudioCapturerNapi::SetFunctionAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::AudioCapturerInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            (void)napi_create_object(env, &valueParam);
            SetValueInt32(env, "source", static_cast<int32_t>(asyncContext->sourceType), valueParam);
            SetValueInt32(env, "capturerFlags", static_cast<int32_t>(asyncContext->capturerFlags), valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerInfoAsyncCallbackComplete* is Null!");
    }
}

void AudioCapturerNapi::AudioStreamInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            (void)napi_create_object(env, &valueParam);
            SetValueInt32(env, "samplingRate", static_cast<int32_t>(asyncContext->samplingRate), valueParam);
            SetValueInt32(env, "channels", static_cast<int32_t>(asyncContext->audioChannel), valueParam);
            SetValueInt32(env, "sampleFormat", static_cast<int32_t>(asyncContext->audioSampleFormat), valueParam);
            SetValueInt32(env, "encodingType", static_cast<int32_t>(asyncContext->audioEncoding), valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::ReadAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            uint8_t *native = nullptr;
            napi_create_arraybuffer(env, asyncContext->bytesRead, reinterpret_cast<void **>(&native), &valueParam);
            if (memcpy_s(native, asyncContext->bytesRead, asyncContext->buffer, asyncContext->bytesRead)) {
                valueParam = nullptr;
            }

            free(asyncContext->buffer);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::VoidAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_boolean(env, asyncContext->isTrue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::GetIntValueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_int32(env, asyncContext->intValue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::GetInt64ValueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_int64(env, asyncContext->time, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

napi_value AudioCapturerNapi::GetCapturerInfo(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetCapturerInfo", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                AudioCapturerInfo capturerInfo = {};
                context->status = context->objectInfo->audioCapturer_->GetCapturerInfo(capturerInfo);
                if (context->status == SUCCESS) {
                    context->sourceType = capturerInfo.sourceType;
                    context->capturerFlags = capturerInfo.capturerFlags;
                }
            },
            AudioCapturerInfoAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetStreamInfo(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetStreamInfo", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);

                AudioStreamInfo streamInfo;
                context->status = context->objectInfo->audioCapturer_->GetStreamInfo(streamInfo);
                if (context->status == SUCCESS) {
                    context->audioSampleFormat = GetJsAudioSampleFormat(streamInfo.format);
                    context->samplingRate = streamInfo.samplingRate;
                    context->audioChannel = streamInfo.channels;
                    context->audioEncoding = streamInfo.encoding;
                }
            },
            AudioStreamInfoAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Start(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Start", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->isTrue = context->objectInfo->audioCapturer_->Start();
                context->status = SUCCESS;
            },
            VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Read(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_THREE);
    NAPI_ASSERT(env, argc >= ARGS_TWO, "requires 2 parameters minimum");

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if ((i == PARAM0) && (valueType == napi_number)) {
                napi_get_value_uint32(env, argv[i], &asyncContext->userSize);
            } else if ((i == PARAM1) && (valueType == napi_boolean)) {
                napi_get_value_bool(env, argv[i], &asyncContext->isBlocking);
            } else if (i == PARAM2 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Read", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->status = ERROR;
                uint32_t userSize = context->userSize;
                auto buffer = std::make_unique<uint8_t[]>(userSize);
                if (!buffer) {
                    return;
                }

                size_t bytesRead = 0;
                while (bytesRead < context->userSize) {
                    int32_t len = context->objectInfo->audioCapturer_->Read(*(buffer.get() + bytesRead),
                                                                            userSize - bytesRead,
                                                                            context->isBlocking);
                    if (len >= 0) {
                        bytesRead += len;
                    } else {
                        bytesRead = len;
                        break;
                    }
                }

                if (bytesRead > 0) {
                    context->bytesRead = bytesRead;
                    context->buffer = buffer.get();
                    buffer.release();
                    context->status = SUCCESS;
                }
            },
            ReadAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetAudioTime(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetAudioTime", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->status = ERROR;
                Timestamp timestamp;
                if (context->objectInfo->audioCapturer_->GetAudioTime(timestamp, Timestamp::Timestampbase::MONOTONIC)) {
                    const uint64_t secToNanosecond = 1000000000;
                    context->time = timestamp.time.tv_nsec + timestamp.time.tv_sec * secToNanosecond;
                    context->status = SUCCESS;
                }
            },
            GetInt64ValueAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Stop(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Stop", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->isTrue = context->objectInfo->audioCapturer_->Stop();
                context->status = SUCCESS;
            },
            VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Release(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Release", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->isTrue = context->objectInfo->audioCapturer_->Release();
                context->status = SUCCESS;
            },
            VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetBufferSize(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                break;
            } else {
                NAPI_ASSERT(env, false, "type mismatch");
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetBufferSize", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                size_t bufferSize;
                context->status = context->objectInfo->audioCapturer_->GetBufferSize(bufferSize);
                if (context->status == SUCCESS) {
                    context->intValue = bufferSize;
                }
            },
            GetIntValueAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetState(napi_env env, napi_callback_info info)
{
    napi_value jsThis = nullptr;
    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    size_t argCount = 0;
    napi_status status = napi_get_cb_info(env, info, &argCount, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return undefinedResult;
    }

    AudioCapturerNapi *capturerNapi = nullptr;
    status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&capturerNapi));
    if (status != napi_ok || capturerNapi == nullptr) {
        HiLog::Error(LABEL, "Failed to get instance");
        return undefinedResult;
    }

    if (capturerNapi->audioCapturer_ == nullptr) {
        HiLog::Error(LABEL, "No memory");
        return undefinedResult;
    }

    int32_t capturerState = capturerNapi->audioCapturer_->GetStatus();

    napi_value jsResult = nullptr;
    status = napi_create_int32(env, capturerState, &jsResult);
    if (status != napi_ok) {
        HiLog::Error(LABEL, "napi_create_int32 error");
        return undefinedResult;
    }

    HiLog::Info(LABEL, "AudioCapturerNapi: GetState Complete, Current state: %{public}d", capturerState);
    return jsResult;
}

bool AudioCapturerNapi::ParseCapturerOptions(napi_env env, napi_value root, AudioCapturerOptions *opts)
{
    napi_value res = nullptr;

    if (napi_get_named_property(env, root, "capturerInfo", &res) == napi_ok) {
        ParseCapturerInfo(env, res, &(opts->capturerInfo));
    }

    if (napi_get_named_property(env, root, "streamInfo", &res) == napi_ok) {
        ParseStreamInfo(env, res, &(opts->streamInfo));
    }

    return true;
}

bool AudioCapturerNapi::ParseCapturerInfo(napi_env env, napi_value root, AudioCapturerInfo *capturerInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "source", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        capturerInfo->sourceType = static_cast<SourceType>(intValue);
    }

    if (napi_get_named_property(env, root, "capturerFlags", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &(capturerInfo->capturerFlags));
    }

    return true;
}

bool AudioCapturerNapi::ParseStreamInfo(napi_env env, napi_value root, AudioStreamInfo* streamInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "samplingRate", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->samplingRate = static_cast<AudioSamplingRate>(intValue);
    }

    if (napi_get_named_property(env, root, "channels", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->channels = static_cast<AudioChannel>(intValue);
    }

    if (napi_get_named_property(env, root, "sampleFormat", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->format = GetNativeAudioSampleFormat(intValue);
    }

    if (napi_get_named_property(env, root, "encodingType", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->encoding = static_cast<AudioEncodingType>(intValue);
    }

    return true;
}

napi_value AudioCapturerNapi::CreateAudioCapturerWrapper(napi_env env, unique_ptr<AudioCapturerOptions> &captureOptions)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    if (captureOptions != nullptr) {
        status = napi_get_reference_value(env, g_capturerConstructor, &constructor);
        if (status == napi_ok) {
            sAudioCapturerOptions_ = move(captureOptions);
            status = napi_new_instance(env, constructor, 0, nullptr, &result);
            sAudioCapturerOptions_.release();
            if (status == napi_ok) {
                return result;
            }
        }
        HiLog::Error(LABEL, "Failed in CreateAudioRendererWrapper, %{public}d", status);
    }

    napi_get_undefined(env, &result);

    return result;
}
}
}
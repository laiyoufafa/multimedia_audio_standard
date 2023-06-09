/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef AUDIO_TIMER_H
#define AUDIO_TIMER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace OHOS {
namespace AudioStandard {
const int WAIT_TIMEOUT_IN_SECS = 5;

class AudioTimer {
public:
    AudioTimer()
    {
        timeoutDuration = WAIT_TIMEOUT_IN_SECS;
        isTimerStarted = false;
        isTimedOut     = false;
        exitLoop       =  false;
        timerLoop = std::thread(&AudioTimer::TimerLoopFunc, this);
    }

    ~AudioTimer()
    {
        exitLoop = true;
        isTimerStarted = false;
        timerCtrl.notify_one();
        isTimerStarted = true;
        timerCtrl.notify_one();
        timerLoop.join();
    }

    void StartTimer(uint32_t duration)
    {
        timeoutDuration = duration;
        isTimerStarted = true;
        timerCtrl.notify_one();
    }

    void StopTimer()
    {
        isTimerStarted = false;
        if (!isTimedOut) {
            timerCtrl.notify_one();
        }
    }

    bool IsTimeOut()
    {
        return isTimedOut;
    }

    virtual void OnTimeOut() = 0;

    volatile std::atomic<bool> isTimedOut;

private:
    std::thread timerLoop;
    std::condition_variable timerCtrl;
    volatile std::atomic<bool> isTimerStarted;
    std::mutex timerMutex;
    volatile bool exitLoop;
    uint32_t timeoutDuration;

    void TimerLoopFunc()
    {
        while (true) {
            std::unique_lock<std::mutex> lck(timerMutex);
            timerCtrl.wait(lck, [this] { return CheckTimerStarted(); });
            isTimedOut = false;
            if (exitLoop) break;
            if (!timerCtrl.wait_for(lck, std::chrono::seconds(timeoutDuration),
                [this] { return CheckTimerStopped(); })) {
                isTimedOut = true;
                isTimerStarted = false;
                OnTimeOut();
            }
        }
    }

    bool CheckTimerStarted()
    {
        return this->isTimerStarted;
    }

    bool CheckTimerStopped()
    {
        return !this->isTimerStarted;
    }
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_TIMER_H

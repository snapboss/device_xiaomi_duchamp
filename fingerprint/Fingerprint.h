/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include "thread/WorkerThread.h"

#include "FingerprintConfig.h"
#include "FingerprintEngine.h"
#include "Session.h"

using ::aidl::android::hardware::biometrics::fingerprint::FingerprintSensorType;
using ::aidl::android::hardware::biometrics::fingerprint::ISession;
using ::aidl::android::hardware::biometrics::fingerprint::ISessionCallback;
using ::aidl::android::hardware::biometrics::fingerprint::SensorProps;

namespace aidl::android::hardware::biometrics::fingerprint {

class Fingerprint : public BnFingerprint {
  public:
    Fingerprint();

    ndk::ScopedAStatus getSensorProps(std::vector<SensorProps>* out) override;

    ndk::ScopedAStatus createSession(int32_t sensorId, int32_t userId,
                                     const std::shared_ptr<ISessionCallback>& cb,
                                     std::shared_ptr<ISession>* out) override;

    static FingerprintConfig& cfg() {
        static FingerprintConfig* cfg = nullptr;
        if (cfg == nullptr) {
            cfg = new FingerprintConfig();
            cfg->init();
        }
        return *cfg;
    }

    static void notify(const fingerprint_msg_t* msg);

  private:
    std::unique_ptr<FingerprintEngine> mEngine;
    WorkerThread mWorker;
    std::shared_ptr<Session> mSession;
    FingerprintSensorType mSensorType;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint

/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>
#include <aidl/android/hardware/biometrics/fingerprint/SensorLocation.h>
#include <fstream>
#include <future>
#include <string>
#include <vector>

#include "LockoutTracker.h"
#include "fingerprint-xiaomi.h"

namespace aidl::android::hardware::biometrics::fingerprint {

class FingerprintEngine {
  public:
    FingerprintEngine();

    void setActiveGroup(int userId);
    void onAcquired(int32_t result, int32_t vendorCode);
    void generateChallengeImpl(ISessionCallback* cb);
    void revokeChallengeImpl(ISessionCallback* cb, int64_t challenge);
    void enrollImpl(ISessionCallback* cb, const keymaster::HardwareAuthToken& hat,
                    const std::future<void>& cancel);
    void authenticateImpl(ISessionCallback* cb, int64_t operationId,
                          const std::future<void>& cancel);
    void detectInteractionImpl(ISessionCallback* cb, const std::future<void>& cancel);
    void enumerateEnrollmentsImpl(ISessionCallback* cb);
    void removeEnrollmentsImpl(ISessionCallback* cb, const std::vector<int32_t>& enrollmentIds);
    void getAuthenticatorIdImpl(ISessionCallback* cb);
    void invalidateAuthenticatorIdImpl(ISessionCallback* cb);
    void resetLockoutImpl(ISessionCallback* cb, const keymaster::HardwareAuthToken& hat);

    ndk::ScopedAStatus onPointerDownImpl(int32_t pointerId, int32_t x, int32_t y, float minor,
                                         float major);
    ndk::ScopedAStatus onPointerUpImpl(int32_t pointerId);
    ndk::ScopedAStatus onUiReadyImpl();

    std::vector<SensorLocation> getSensorLocations();

    static std::pair<AcquiredInfo, int32_t> convertAcquiredInfo(int32_t code);
    static std::pair<Error, int32_t> convertError(int32_t code);
    bool checkSensorLockout(ISessionCallback*);
    LockoutTracker mLockoutTracker;

  private:
    fingerprint_device_t* openFingerprintHal(const char* class_name, const char* module_id);
    void setFodStatus(int value);
    void setFingerStatus(bool pressed);
    void clearLockout(ISessionCallback* cb, bool dueToTimeout = false);

    template <typename T>
    void set(const std::string& path, const T& value);

    void startLockoutTimer(int64_t timeout, ISessionCallback* cb);
    void lockoutTimerExpired(ISessionCallback* cb);

    fingerprint_device_t* mDevice;

    bool isLockoutTimerSupported;
    bool isLockoutTimerStarted = false;
    bool isLockoutTimerAborted = false;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint

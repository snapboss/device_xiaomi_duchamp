/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/binder_to_string.h>
#include <stdint.h>
#include <string>

#define LOCKOUT_TIMED_THRESHOLD 5
#define LOCKOUT_TIMED_DURATION 10000
#define LOCKOUT_PERMANENT_THRESHOLD 20

namespace aidl::android::hardware::biometrics::fingerprint {

class LockoutTracker {
  public:
    LockoutTracker() : mFailedCount(0) {}
    ~LockoutTracker() {}

    enum class LockoutMode : int8_t { kNone = 0, kTimed, kPermanent };

    void reset(bool dueToTimeout = false);
    LockoutMode getMode();
    void addFailedAttempt();
    int64_t getLockoutTimeLeft();

  private:
    int32_t mFailedCount;
    int64_t mLockoutTimedStart;
    LockoutMode mCurrentMode;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint

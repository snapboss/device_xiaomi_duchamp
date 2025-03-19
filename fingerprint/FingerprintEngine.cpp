/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FingerprintEngine.h"
#include "Fingerprint.h"

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include <fingerprint.sysprop.h>

#include "util/CancellationSignal.h"
#include "util/Util.h"

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;

namespace aidl::android::hardware::biometrics::fingerprint {

namespace {

template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

static bool readBool(int fd) {
    char c;
    int rc;

    rc = lseek(fd, 0, SEEK_SET);
    if (rc) {
        LOG(ERROR) << "failed to seek fd, err: " << rc;
        return false;
    }

    rc = read(fd, &c, sizeof(char));
    if (rc != 1) {
        LOG(ERROR) << "failed to read bool from fd, err: " << rc;
        return false;
    }

    return c != '0';
}

}  // anonymous namespace

FingerprintEngine::FingerprintEngine() : isLockoutTimerSupported(true) {
    if (mDevice) {
        LOG(INFO) << "Fingerprint HAL already opened";
    } else {
        for (auto& [module] : kModules) {
            std::string class_name;
            std::string class_module_id;

            auto parts = Util::split(module, "|");

            if (parts.size() == 2) {
                class_name = parts[0];
                class_module_id = parts[1];
            } else {
                class_name = module;
                class_module_id = FINGERPRINT_HARDWARE_MODULE_ID;
            }

            mDevice = openFingerprintHal(class_name.c_str(), class_module_id.c_str());
            if (!mDevice) {
                LOG(ERROR) << "Can't open HAL module, class: " << class_name.c_str()
                           << ", module_id: " << class_module_id.c_str();
                continue;
            }
            LOG(INFO) << "Opened fingerprint HAL, class: " << class_name.c_str()
                      << ", module_id: " << class_module_id.c_str();
            break;
        }
        if (!mDevice) {
            LOG(ERROR) << "Can't open any fingerprint HAL module";
        }
        init(mDevice);
    }
}

void FingerprintEngine::setActiveGroup(int userId) {
    LOG(INFO) << __func__;
    auto path = std::format("/data/vendor_de/{}/fpdata/", userId);
    uint64_t error = mDevice->setActiveGroup(mDevice, userId, path.c_str());
    if (error) {
        LOG(INFO) << "Failed to set active group: " << error;
    }
}

fingerprint_device_t* FingerprintEngine::openFingerprintHal(const char* class_name,
                                                            const char* module_id) {
    const hw_module_t* hw_mdl = nullptr;

    LOG(INFO) << "Opening fingerprint hal library...";
    if (hw_get_module_by_class(module_id, class_name, &hw_mdl) != 0) {
        LOG(ERROR) << "Can't open fingerprint HW Module";
        return nullptr;
    }

    if (!hw_mdl) {
        LOG(ERROR) << "No valid fingerprint module";
        return nullptr;
    }

    auto module = reinterpret_cast<const fingerprint_module_t*>(hw_mdl);
    if (!module->common.methods->open) {
        LOG(ERROR) << "No valid open method";
        return nullptr;
    }

    hw_device_t* device = nullptr;
    if (module->common.methods->open(hw_mdl, nullptr, &device) != 0) {
        LOG(ERROR) << "Can't open fingerprint methods";
        return nullptr;
    }

    if (module->common.module_api_version != FINGERPRINT_MODULE_API_VERSION_2_1) {
        LOG(ERROR) << "Hardware version dosesn't match FINGERPRINT_MODULE_API_VERSION_2_1: "
                   << module->common.module_api_version;
        return nullptr;
    }

    auto fp_device = reinterpret_cast<fingerprint_device_t*>(device);
    if (fp_device->set_notify(fp_device, Fingerprint::notify) != 0) {
        LOG(ERROR) << "Can't register fingerprint module callback";
        return nullptr;
    }

    return fp_device;
}

void FingerprintEngine::init(fingerprint_device_t* device) {
    mDevice = device;
    touch_fd_ = ::android::base::unique_fd(open(TOUCH_DEV_PATH, O_RDWR));

    std::thread([this]() {
        int fd = open(FOD_PRESS_STATUS_PATH, O_RDONLY);
        if (fd < 0) {
            LOG(ERROR) << "failed to open fd, err: " << fd;
            return;
        }

        struct pollfd fodPressStatusPoll = {
                .fd = fd,
                .events = POLLERR | POLLPRI,
                .revents = 0,
        };

        while (true) {
            int rc = poll(&fodPressStatusPoll, 1, -1);
            if (rc < 0) {
                LOG(ERROR) << "failed to poll fd, err: " << rc;
                continue;
            }

            mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS,
                            readBool(fd) ? PARAM_FOD_PRESSED : PARAM_FOD_RELEASED);
        }
    }).detach();
}

void FingerprintEngine::onAcquired(int32_t result, int32_t vendorCode) {
    LOG(INFO) << __func__ << " result: " << result << " vendorCode: " << vendorCode;
    if (static_cast<AcquiredInfo>(result) == AcquiredInfo::GOOD) {
        setFingerDown(false);
        setFodStatus(FOD_STATUS_OFF);
    } else if (vendorCode == 21 || vendorCode == 23) {
        /*
         * vendorCode = 21 waiting for fingerprint authentication
         * vendorCode = 23 waiting for fingerprint enroll
         */
        setFodStatus(FOD_STATUS_ON);
    }
}

void FingerprintEngine::setFodStatus(int value) {
    int buf[MAX_BUF_SIZE] = {TOUCH_ID, Touch_Fod_Enable, value};
    ioctl(touch_fd_.get(), TOUCH_IOC_SET_CUR_VALUE, &buf);
}

void FingerprintEngine::setFingerDown(bool pressed) {
    mDevice->extCmd(mDevice, COMMAND_NIT, pressed ? PARAM_NIT_FOD : PARAM_NIT_NONE);

    int buf[MAX_BUF_SIZE] = {TOUCH_ID, Touch_Fod_Enable, pressed ? 1 : 0};
    ioctl(touch_fd_.get(), TOUCH_IOC_SET_CUR_VALUE, &buf);

    set(DISP_PARAM_PATH, std::string(DISP_PARAM_LOCAL_HBM_MODE) + " " +
                                 (pressed ? DISP_PARAM_LOCAL_HBM_ON : DISP_PARAM_LOCAL_HBM_OFF));
}

void FingerprintEngine::generateChallengeImpl(ISessionCallback* /*cb*/) {
    LOG(INFO) << __func__;
    mDevice->generateChallenge(mDevice);
}

void FingerprintEngine::revokeChallengeImpl(ISessionCallback* /*cb*/, int64_t challenge) {
    LOG(INFO) << __func__;
    uint64_t error = mDevice->revokeChallenge(mDevice, challenge);
    if (error) {
        LOG(ERROR) << "Failed to revoke challenge=" << challenge << " error=" << error;
    }
}

void FingerprintEngine::enrollImpl(ISessionCallback* cb, const keymaster::HardwareAuthToken& hat,
                                   const std::future<void>& /*cancel*/) {
    LOG(INFO) << __func__;

    hw_auth_token_t authToken;
    translate(hat, authToken);
    int error = mDevice->enroll(mDevice, &authToken);
    if (error) {
        LOG(ERROR) << "enroll failed: " << error;
        cb->onError(Error::UNABLE_TO_PROCESS, error);
    }
}

void FingerprintEngine::authenticateImpl(ISessionCallback* cb, int64_t operationId,
                                         const std::future<void>& /*cancel*/) {
    LOG(INFO) << __func__;

    int error = mDevice->authenticate(mDevice, operationId);
    if (error) {
        LOG(ERROR) << "authenticate failed: " << error;
        cb->onError(Error::UNABLE_TO_PROCESS, error);
    }
}

void FingerprintEngine::detectInteractionImpl(ISessionCallback* cb,
                                              const std::future<void>& /*cancel*/) {
    LOG(INFO) << __func__;

    auto detectInteractionSupported = Fingerprint::cfg().get<bool>("detect_interaction");
    if (!detectInteractionSupported) {
        LOG(ERROR) << "Detect interaction is not supported";
        cb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }
}

void FingerprintEngine::enumerateEnrollmentsImpl(ISessionCallback* cb) {
    LOG(INFO) << __func__;
    int error = mDevice->enumerate(mDevice);
    if (error) {
        LOG(ERROR) << "enumerate failed: " << error;
        cb->onError(Error::UNABLE_TO_PROCESS, error);
    }
}

void FingerprintEngine::removeEnrollmentsImpl(ISessionCallback* /*cb*/,
                                              const std::vector<int32_t>& enrollmentIds) {
    LOG(INFO) << __func__;
    mDevice->remove(mDevice, enrollmentIds.data(), enrollmentIds.size());
}

void FingerprintEngine::getAuthenticatorIdImpl(ISessionCallback* /*cb*/) {
    LOG(INFO) << __func__;
    mDevice->getAuthenticatorId(mDevice);
}

void FingerprintEngine::invalidateAuthenticatorIdImpl(ISessionCallback* /*cb*/) {
    LOG(INFO) << __func__;
    mDevice->invalidateAuthenticatorId(mDevice);
}

void FingerprintEngine::resetLockoutImpl(ISessionCallback* cb,
                                         const keymaster::HardwareAuthToken& hat) {
    LOG(INFO) << __func__;
    if (hat.mac.empty()) {
        LOG(ERROR) << "Fail: hat in resetLockout()";
        cb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }
    clearLockout(cb);
    if (isLockoutTimerStarted) isLockoutTimerAborted = true;
}

void FingerprintEngine::clearLockout(ISessionCallback* cb, bool dueToTimeout) {
    cb->onLockoutCleared();
    mLockoutTracker.reset(dueToTimeout);
}

ndk::ScopedAStatus FingerprintEngine::onPointerDownImpl(int32_t /*pointerId*/, int32_t /*x*/, int32_t /*y*/,
                                                        float /*minor*/, float /*major*/) {
    LOG(INFO) << __func__;

    setFingerDown(true);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FingerprintEngine::onPointerUpImpl(int32_t /*pointerId*/) {
    LOG(INFO) << __func__;

    setFingerDown(false);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FingerprintEngine::onUiReadyImpl() {
    LOG(INFO) << __func__;
    return ndk::ScopedAStatus::ok();
}

std::vector<SensorLocation> FingerprintEngine::getSensorLocations() {
    std::vector<SensorLocation> locations;

    auto loc = Fingerprint::cfg().get<std::string>("sensor_location");
    auto entries = ::android::base::Split(loc, ",");

    for (const auto& entry : entries) {
        auto isValidStr = false;
        auto dim = ::android::base::Split(entry, "|");

        if (dim.size() != 3 and dim.size() != 4) {
            if (!loc.empty()) {
                LOG(INFO) << "Invalid sensor location input (x|y|radius) or (x|y|radius|display): "
                          << loc.c_str();
            }
        } else {
            int32_t x, y, r;
            std::string d;
            isValidStr = ParseInt(dim[0], &x) && ParseInt(dim[1], &y) && ParseInt(dim[2], &r);
            if (dim.size() == 4) {
                d = dim[3];
                isValidStr = isValidStr && !d.empty();
            }
            if (isValidStr)
                locations.push_back({.sensorLocationX = x,
                                     .sensorLocationY = y,
                                     .sensorRadius = r,
                                     .display = d});
        }
    }

    return locations;
}

std::pair<AcquiredInfo, int32_t> FingerprintEngine::convertAcquiredInfo(int32_t code) {
    std::pair<AcquiredInfo, int32_t> res;
    if (code > FINGERPRINT_ACQUIRED_VENDOR_BASE) {
        res.first = AcquiredInfo::VENDOR;
        res.second = code - FINGERPRINT_ACQUIRED_VENDOR_BASE;
    } else {
        res.first = (AcquiredInfo)code;
        res.second = 0;
    }
    return res;
}

std::pair<Error, int32_t> FingerprintEngine::convertError(int32_t code) {
    std::pair<Error, int32_t> res;
    if (code > FINGERPRINT_ERROR_VENDOR_BASE) {
        res.first = Error::VENDOR;
        res.second = code - FINGERPRINT_ERROR_VENDOR_BASE;
    } else {
        res.first = (Error)code;
        res.second = 0;
    }
    return res;
}

bool FingerprintEngine::checkSensorLockout(ISessionCallback* cb) {
    LockoutTracker::LockoutMode lockoutMode = mLockoutTracker.getMode();
    if (lockoutMode == LockoutTracker::LockoutMode::kPermanent) {
        LOG(ERROR) << "Fail: lockout permanent";
        cb->onLockoutPermanent();
        isLockoutTimerAborted = true;
        return true;
    } else if (lockoutMode == LockoutTracker::LockoutMode::kTimed) {
        int64_t timeLeft = mLockoutTracker.getLockoutTimeLeft();
        LOG(ERROR) << "Fail: lockout timed " << timeLeft;
        cb->onLockoutTimed(timeLeft);
        if (isLockoutTimerSupported && !isLockoutTimerStarted) startLockoutTimer(timeLeft, cb);
        return true;
    }
    return false;
}

void FingerprintEngine::startLockoutTimer(int64_t timeout, ISessionCallback* cb) {
    LOG(INFO) << __func__;
    std::function<void(ISessionCallback*)> action =
            std::bind(&FingerprintEngine::lockoutTimerExpired, this, std::placeholders::_1);
    std::thread([timeout, action, cb]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        action(cb);
    }).detach();

    isLockoutTimerStarted = true;
}
void FingerprintEngine::lockoutTimerExpired(ISessionCallback* cb) {
    LOG(INFO) << __func__;
    if (!isLockoutTimerAborted) {
        clearLockout(cb, true);
    }
    isLockoutTimerStarted = false;
    isLockoutTimerAborted = false;
}
}  // namespace aidl::android::hardware::biometrics::fingerprint

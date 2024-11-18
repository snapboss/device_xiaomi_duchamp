/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Fingerprint.h"
#include "Session.h"

#include <android-base/properties.h>
#include <fingerprint.sysprop.h>

#include <android-base/logging.h>
#include <android-base/strings.h>

namespace aidl::android::hardware::biometrics::fingerprint {

namespace {
constexpr size_t MAX_WORKER_QUEUE_SIZE = 5;
constexpr int MAX_ENROLLMENTS_PER_USER = 5;
constexpr bool SUPPORTS_NAVIGATION_GESTURES = true;
constexpr char HW_COMPONENT_ID[] = "fingerprintSensor";
constexpr char HW_VERSION[] = "vendor/model/revision";
constexpr char FW_VERSION[] = "1.01";
constexpr char SERIAL_NUMBER[] = "00000001";
constexpr char SW_COMPONENT_ID[] = "matchingAlgorithm";
constexpr char SW_VERSION[] = "vendor/version/revision";

}  // namespace

static Fingerprint* sInstance;

Fingerprint::Fingerprint() : mWorker(MAX_WORKER_QUEUE_SIZE) {
    sInstance = this;  // keep track of the most recent instance

    std::string sensorTypeProp = Fingerprint::cfg().get<std::string>("type");
    if (sensorTypeProp == "udfps" || sensorTypeProp == "udfps_optical") {
        if (sensorTypeProp == "udfps") {
            mSensorType = FingerprintSensorType::UNDER_DISPLAY_ULTRASONIC;
        } else {
            mSensorType = FingerprintSensorType::UNDER_DISPLAY_OPTICAL;
        }
    } else if (sensorTypeProp == "side") {
        mSensorType = FingerprintSensorType::POWER_BUTTON;
    } else if (sensorTypeProp == "home") {
        mSensorType = FingerprintSensorType::HOME_BUTTON;
    } else if (sensorTypeProp == "rear") {
        mSensorType = FingerprintSensorType::REAR;
    } else {
        mSensorType = FingerprintSensorType::UNKNOWN;
        UNIMPLEMENTED(FATAL) << "unrecognized or unimplemented fingerprint behavior: "
                             << sensorTypeProp;
    }

    mEngine = std::make_unique<FingerprintEngine>();
    LOG(INFO) << "sensorTypeProp:" << sensorTypeProp;
}

void Fingerprint::notify(const fingerprint_msg_t* msg) {
    Fingerprint* thisPtr = sInstance;
    if (thisPtr == nullptr || thisPtr->mSession == nullptr || thisPtr->mSession->isClosed()) {
        LOG(ERROR) << "Receiving callbacks before a session is opened.";
        return;
    }
    thisPtr->mSession->notify(msg);
}

ndk::ScopedAStatus Fingerprint::getSensorProps(std::vector<SensorProps>* out) {
    std::vector<common::ComponentInfo> componentInfo = {
            {HW_COMPONENT_ID, HW_VERSION, FW_VERSION, SERIAL_NUMBER, "" /* softwareVersion */},
            {SW_COMPONENT_ID, "" /* hardwareVersion */, "" /* firmwareVersion */,
             "" /* serialNumber */, SW_VERSION}};
    auto sensorId = Fingerprint::cfg().get<std::int32_t>("sensor_id");
    auto sensorStrength = Fingerprint::cfg().get<std::int32_t>("sensor_strength");
    auto navigationGuesture = Fingerprint::cfg().get<bool>("navigation_guesture");
    auto detectInteraction = Fingerprint::cfg().get<bool>("detect_interaction");
    auto displayTouch = Fingerprint::cfg().get<bool>("display_touch");
    auto controlIllumination = Fingerprint::cfg().get<bool>("control_illumination");

    common::CommonProps commonProps = {sensorId, (common::SensorStrength)sensorStrength,
                                       MAX_ENROLLMENTS_PER_USER, componentInfo};

    std::vector<SensorLocation> sensorLocations = mEngine->getSensorLocations();

    std::vector<std::string> sensorLocationStrings;
    std::transform(sensorLocations.begin(), sensorLocations.end(),
                   std::back_inserter(sensorLocationStrings),
                   [](const SensorLocation& obj) { return obj.toString(); });

    LOG(INFO) << "sensor type:" << ::android::internal::ToString(mSensorType)
              << " location:" << ::android::base::Join(sensorLocationStrings, ", ").c_str();

    *out = {{commonProps, mSensorType, sensorLocations, navigationGuesture, detectInteraction,
             displayTouch, controlIllumination, std::nullopt}};

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Fingerprint::createSession(int32_t sensorId, int32_t userId,
                                              const std::shared_ptr<ISessionCallback>& cb,
                                              std::shared_ptr<ISession>* out) {
    CHECK(mSession == nullptr || mSession->isClosed()) << "Open session already exists!";

    mSession = SharedRefBase::make<Session>(sensorId, userId, cb, mEngine.get(), &mWorker);
    *out = mSession;

    mSession->linkToDeath(cb->asBinder().get());

    LOG(INFO) << __func__ << ": sensorId:" << sensorId << " userId:" << userId;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::biometrics::fingerprint

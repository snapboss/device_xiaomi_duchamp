#
# Copyright (C) 2023 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from device makefile.
$(call inherit-product, device/xiaomi/duchamp/device.mk)

# Inherit some common stuff.
$(call inherit-product, vendor/aosp/config/common_full_phone.mk)

TARGET_FACE_UNLOCK_SUPPORTED := true
USE_PIXEL_CHARGER := true
TARGET_SUPPORTS_NOW_PLAYING := true
TARGET_SUPPORTS_PREBUILT_UPDATABLE_APEX := false
TARGET_SUPPORTS_CLEAR_CALLING := true
TARGET_SUPPORTS_CALL_RECORDING := true
TARGET_SUPPORTS_ADPATIVE_CHARGING := true
CUSTOM_BUILD_TYPE := Unofficial
IS_SIGNED := true
CUSTOM_MAINTAINER := Luxured
TARGET_BOOT_ANIMATION_RES := 1080
PRODUCT_NO_CAMERA := true


PRODUCT_NAME := aosp_duchamp
PRODUCT_DEVICE := duchamp
PRODUCT_MANUFACTURER := Xiaomi
PRODUCT_BRAND := POCO
PRODUCT_MODEL := 2311DRK48G
PRODUCT_SYSTEM_NAME := duchamp_global

PRODUCT_GMS_CLIENTID_BASE := android-xiaomi

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="duchamp_global-user 15 AP3A.240905.015.A2 OS2.0.100.0.VNLMIXM release-keys" \
    BuildFingerprint=POCO/duchamp_global/duchamp:14/UP1A.230905.011/OS2.0.100.0.VNLMIXM:user/release-keys \
    DeviceProduct=$(PRODUCT_SYSTEM_NAME)



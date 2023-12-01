LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := splatapult

LOCAL_CFLAGS += -Werror

# This should be set via an environment var
# ANDROID_VCPKG_DIR := C:/msys64/home/hyperlogic/code/vcpkg/installed/arm64-android

LOCAL_C_INCLUDES := \
                    $(LOCAL_PATH)/../../../../../../../src/ \
					$(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
					$(LOCAL_PATH)/../../../../../OpenXr/Include \
					$(LOCAL_PATH)/../../../../../3rdParty/khronos/openxr/OpenXR-SDK/include/ \
					$(LOCAL_PATH)/../../../../../3rdParty/khronos/openxr/OpenXR-SDK/src/common/ \
					$(ANDROID_VCPKG_DIR)/include \

LOCAL_SRC_PATH := ../../../../../../../src
LOCAL_SRC_FILES	:= 	$(LOCAL_SRC_PATH)/core/log.cpp \
					$(LOCAL_SRC_PATH)/core/util.cpp \
					$(LOCAL_SRC_PATH)/core/xrbuddy.cpp \
					$(LOCAL_SRC_PATH)/android_main.cpp \

LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog

LOCAL_LDFLAGS := -u ANativeActivity_onCreate

LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := openxr_loader

include $(BUILD_SHARED_LIBRARY)

$(call import-module,OpenXR/Projects/AndroidPrebuilt/jni)
$(call import-module,android/native_app_glue)

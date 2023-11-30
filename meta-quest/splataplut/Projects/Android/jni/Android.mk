LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := scenemodel

LOCAL_CFLAGS += -Werror
LOCAL_CFLAGS += -DANDROID_NDK

LOCAL_C_INCLUDES := \
                    $(LOCAL_PATH)/../../../../../../../src/ \
					$(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
					$(LOCAL_PATH)/../../../../../OpenXr/Include \
					$(LOCAL_PATH)/../../../../../3rdParty/khronos/openxr/OpenXR-SDK/include/ \
					$(LOCAL_PATH)/../../../../../3rdParty/khronos/openxr/OpenXR-SDK/src/common/

# 
LOCAL_SRC_FILES	:= 	../../../../../../../src/android_main.cpp \
					../../../../../../../src/core/log.cpp \

LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog

LOCAL_LDFLAGS := -u ANativeActivity_onCreate

LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := openxr_loader

include $(BUILD_SHARED_LIBRARY)

$(call import-module,OpenXR/Projects/AndroidPrebuilt/jni)
$(call import-module,android/native_app_glue)

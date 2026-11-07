LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_MODULE := zygisk
LOCAL_SRC_FILES := fd_reopener.cpp utils.cpp map_parser.cpp mountinfo_parser.cpp modules.cpp main.cpp atexit.cpp config.cpp
LOCAL_STATIC_LIBRARIES := libcxx libfdutils
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include jni/libcxx/Android.mk
include jni/aosp_fd_utils/Android.mk

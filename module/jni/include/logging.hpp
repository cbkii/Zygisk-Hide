#pragma once
#include <android/log.h>
#include <cstring>
#include <cerrno>
#include <cinttypes>

static constexpr auto TAG = "ZygiskHide/JNI";
#ifndef NDEBUG
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#else
#define LOGD(...) ((void)0)
#endif
// Keep exceptional failures visible even in release; app identities are debug-only.
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/*
 * Copyright (C) 2020 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <cstdlib>
#include <pthread.h>

namespace {
    struct AtexitEntry {
        void (*fn)(void *);  // the __cxa_atexit callback
        void *arg;          // argument for `fn` callback
    };

    AtexitEntry *g_array = nullptr;
    size_t capacity = 8;
    size_t count = 0;
    pthread_mutex_t g_atexit_mutex = PTHREAD_MUTEX_INITIALIZER;
    inline void atexit_lock() {
        pthread_mutex_lock(&g_atexit_mutex);
    }

    inline void atexit_unlock() {
        pthread_mutex_unlock(&g_atexit_mutex);
    }

    // Register a function to be called either when a library is unloaded (dso != nullptr), or when the
    // program exits (dso == nullptr). The `dso` argument is typically the address of a hidden
    // __dso_handle variable. This function is also used as the backend for the atexit function.
    //
    // See https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor.
    //
    extern "C" [[gnu::used]] int __cxa_atexit(void (*func)(void *), void *arg, void * /* dso */) { // NOLINT(bugprone-reserved-identifier)
        int result = -1;

        if (func != nullptr) {
            atexit_lock();
            if (!g_array) {
                g_array = reinterpret_cast<AtexitEntry*>(malloc(capacity * sizeof(AtexitEntry)));
                if (!g_array) { atexit_unlock(); return -1; }
            }
            if (count == capacity) [[unlikely]] {
                const size_t nextCapacity = capacity * 2;
                auto *next = reinterpret_cast<AtexitEntry*>(realloc(g_array, nextCapacity * sizeof(AtexitEntry)));
                if (!next) { atexit_unlock(); return -1; }
                g_array = next;
                capacity = nextCapacity;
            }
            g_array[count++] = {func, arg};
            result = 0;
            atexit_unlock();
        }

        return result;
    }

    // This function will be called by __on_dlclose, which is a destructor of dso
    // https://cs.android.com/android/platform/superproject/main/+/main:bionic/libc/arch-common/bionic/crtbegin_so.c;l=34;drc=5501003be73b73de59044b44b12f6e20ba6e0021
    extern "C" [[gnu::used]] void __cxa_finalize(void * /* dso */) { // NOLINT(bugprone-reserved-identifier)
        atexit_lock();
        // Pop before invoking user code. Recursive finalization or a callback
        // registering another callback can replace/free the array while unlocked.
        while (count > 0) {
            const AtexitEntry entry = g_array[--count];
            atexit_unlock();
            entry.fn(entry.arg);
            atexit_lock();
        }
        free(g_array);
        g_array = nullptr;
        count = 0;
        capacity = 8;
        atexit_unlock();
    }
}
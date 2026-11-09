#include <cstdlib>
#include <cstdint>
#include <vector>
#include <cassert>
static bool failAllocation = false;
static void *testMalloc(size_t size) { return failAllocation ? nullptr : std::malloc(size); }
static void *testRealloc(void *p, size_t size) { return failAllocation ? nullptr : std::realloc(p, size); }
// Rename only the included implementation's entry points; do not replace the
// host process's real C++ runtime finalizer.
#define malloc testMalloc
#define realloc testRealloc
#define __cxa_atexit testRegister
#define __cxa_finalize testFinalize
#include "../module/jni/atexit.cpp"
#undef malloc
#undef realloc
#undef __cxa_atexit
#undef __cxa_finalize
static std::vector<int> calls;
static void record(void *arg) { calls.push_back(static_cast<int>(reinterpret_cast<intptr_t>(arg))); }
static void add(void *) { calls.push_back(2); assert(testRegister(record, reinterpret_cast<void *>(3), nullptr) == 0); }
static void nested(void *) { calls.push_back(2); testFinalize(nullptr); }
static void nestedAndRegister(void *) {
    calls.push_back(2);
    testFinalize(nullptr);
    assert(testRegister(record, reinterpret_cast<void *>(3), nullptr) == 0);
}
int main() {
    failAllocation = true;
    assert(testRegister(record, nullptr, nullptr) == -1);
    failAllocation = false;
    assert(testRegister(record, reinterpret_cast<void *>(1), nullptr) == 0);
    assert(testRegister(record, reinterpret_cast<void *>(2), nullptr) == 0);
    testFinalize(nullptr); testFinalize(nullptr);
    assert((calls == std::vector<int>{2, 1}));
    calls.clear();
    testRegister(record, reinterpret_cast<void *>(1), nullptr);
    testRegister(add, nullptr, nullptr);
    testFinalize(nullptr);
    assert((calls == std::vector<int>{2, 3, 1}));
    calls.clear();
    testRegister(record, reinterpret_cast<void *>(1), nullptr);
    testRegister(nested, nullptr, nullptr);
    testFinalize(nullptr);
    assert((calls == std::vector<int>{2, 1}));
    calls.clear();
    testRegister(record, reinterpret_cast<void *>(1), nullptr);
    testRegister(nestedAndRegister, nullptr, nullptr);
    testFinalize(nullptr);
    assert((calls == std::vector<int>{2, 1, 3}));
    calls.clear();
    for (int i = 0; i < 8; ++i) assert(testRegister(record, reinterpret_cast<void *>(1), nullptr) == 0);
    failAllocation = true;
    assert(testRegister(record, nullptr, nullptr) == -1);
    failAllocation = false;
    assert(testRegister(record, reinterpret_cast<void *>(9), nullptr) == 0);
    testFinalize(nullptr); testFinalize(nullptr);
    assert(calls.size() == 9 && calls[0] == 9);
}

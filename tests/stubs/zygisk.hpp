#pragma once
#include <cstdint>
#include <sys/types.h>
using jstring = const char *;
struct JNIEnv {};
namespace zygisk {
struct AppSpecializeArgs { int uid; jstring nice_name = "com.example"; jstring app_data_dir = "/data/user/0/com.example"; };
struct ServerSpecializeArgs {};
enum Option { FORCE_DENYLIST_UNMOUNT, DLCLOSE_MODULE_LIBRARY };
enum StateFlag : uint32_t { PROCESS_GRANTED_ROOT = 1, PROCESS_ON_DENYLIST = 2 };
struct Api {
    unsigned flags = PROCESS_ON_DENYLIST;
    int unloads = 0, commits = 0, failCommit = 0;
    uint32_t getFlags() { return flags; }
    int connectCompanion() { return -1; }
    bool exemptFd(int) { return false; }
    void setOption(Option option) { if (option == DLCLOSE_MODULE_LIBRARY) ++unloads; }
    bool pltHookCommit() { return ++commits != failCommit; }
};
struct ModuleBase {
    virtual ~ModuleBase() = default;
    virtual void onLoad(Api *, JNIEnv *) {}
    virtual void preAppSpecialize(AppSpecializeArgs *) {}
    virtual void postAppSpecialize(const AppSpecializeArgs *) {}
    virtual void preServerSpecialize(ServerSpecializeArgs *) {}
};
}
#define REGISTER_ZYGISK_MODULE(...)
#define REGISTER_ZYGISK_COMPANION(...)

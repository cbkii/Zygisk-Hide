#include <unistd.h>
#include <sched.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <cstdint>
#include "zygisk.hpp"
#include "logging.hpp"
#include "utils.hpp"
#include "modules.hpp"
#include "fd_reopener.hpp"
#include "config.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

// Per-child state. No heap-allocated, self-destroying callback, persistent
// syscall interception, executable allocation or additional SELinux rule.
static int (*old_unshare)(int) = nullptr;
static int (*old_setresuid)(uid_t, uid_t, uid_t) = nullptr;
static bool unshareHooked = false, uidHooked = false, pending = false;
static bool namespaceReady = false;
static int companionFd = -1;

static void closeCompanion() {
    if (companionFd >= 0) close(companionFd);
    companionFd = -1;
}

static bool runCleanup() {
    if (!pending) return namespaceReady;
    pending = false;
    FDReopener::ScopedRegularReopener reopener;
    if (!old_unshare || old_unshare(CLONE_NEWNS) != 0) {
        LOGW("Unable to create private mount namespace: %s", strerror(errno));
        closeCompanion();
        return false;
    }
    if (mount(nullptr, "/", nullptr, MS_SLAVE | MS_REC, nullptr) != 0) {
        LOGW("Unable to isolate mount propagation: %s", strerror(errno));
        closeCompanion();
        return false;
    }
    namespaceReady = true;
    uint8_t result = 0;
    auto until = Runtime::deadline();
    uint8_t tag = static_cast<uint8_t>(Config::Request::SWITCH_NS);
    // Pass the namespace itself rather than trusting a caller-supplied PID.
    int ns = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
    bool ok = companionFd >= 0 && ns >= 0 &&
              Runtime::writeExact(companionFd, &tag, sizeof(tag), until) &&
              Runtime::sendFd(companionFd, ns, until) &&
              Runtime::readExact(companionFd, &result, sizeof(result), until) && result == 1;
    if (ns >= 0) close(ns);
    closeCompanion();
    if (!ok) {
        LOGW("Companion cleanup failed or timed out; attempting local unmount only");
        if (!doUnmount()) LOGW("Local mount cleanup incomplete");
    }
    return true;
}

static int new_unshare(int flags) {
    // Do not consume the one-shot callback on an unrelated namespace request.
    if ((flags & CLONE_NEWNS) && runCleanup()) flags &= ~CLONE_NEWNS;
    return flags ? (old_unshare ? old_unshare(flags) : ::unshare(flags)) : 0;
}
static int new_setresuid(uid_t real, uid_t effective, uid_t saved) {
    runCleanup();
    return old_setresuid ? old_setresuid(real, effective, saved) : ::setresuid(real, effective, saved);
}

class ZygiskModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }
    void preAppSpecialize(AppSpecializeArgs *args) override {
        uint32_t flags = api->getFlags();
        if ((flags & zygisk::StateFlag::PROCESS_GRANTED_ROOT) || !Utils::isUserAppUID(args->uid)) return;
        std::string process = Utils::jstringToUtf8(env, args->nice_name);
        std::string package = Runtime::packageFromDataDir(Utils::jstringToUtf8(env, args->app_data_dir), args->uid);
        uint8_t selectionFlags = 0;
        if (flags & zygisk::StateFlag::PROCESS_ON_DENYLIST) selectionFlags |= Config::MANAGER_SELECTED;
        if (args->is_child_zygote && *args->is_child_zygote) selectionFlags |= Config::CHILD_ZYGOTE;
        int queryFd = api->connectCompanion();
        auto cleanup = Config::queryCleanup(queryFd, process, package, selectionFlags);
        if (queryFd >= 0) close(queryFd);
        if (cleanup == Config::Cleanup::None) return;
        if (cleanup == Config::Cleanup::Loader) {
            LOGD("Loader unmount requested for process=%s uid=%d", process.c_str(), args->uid);
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            return;
        }
        LOGD("Local cleanup selected for process=%s package=%s uid=%d", process.c_str(), package.c_str(), args->uid);
        companionFd = api->connectCompanion();
        if (companionFd >= 0 && !api->exemptFd(companionFd)) closeCompanion();

        // Set tracking before committing: a failed commit can still have changed
        // a relocation. postAppSpecialize always attempts restoration.
        unshareHooked = true;
        if (!hook("unshare", new_unshare, &old_unshare) || !old_unshare) {
            LOGW("unshare import unavailable; runtime cleanup skipped");
            closeCompanion();
            return;
        }
        uidHooked = true;
        if (!hook("setresuid", new_setresuid, &old_setresuid) || !old_setresuid) {
            LOGW("setresuid import unavailable; runtime cleanup skipped");
            closeCompanion();
            return;
        }
        pending = true;
    }
    void preServerSpecialize(ServerSpecializeArgs *) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const AppSpecializeArgs *) override {
        pending = false;
        closeCompanion();
        bool restored = true;
        if (uidHooked) restored = old_setresuid && hook("setresuid", old_setresuid) && restored;
        if (unshareHooked) restored = old_unshare && hook("unshare", old_unshare) && restored;
        // Never unload code that a relocation may still reference. A failed
        // restoration deliberately retains this DSO and reports degraded state.
        if (restored) api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        else LOGE("Hook restoration failed; retaining module library to avoid dangling code pointers");
    }
private:
    template <typename T> bool hook(const char *symbol, T *replacement, T **original = nullptr) {
        return Utils::hookPLTByName(api, "libandroid_runtime.so", symbol,
                                   reinterpret_cast<void *>(replacement), reinterpret_cast<void **>(original)) &&
               api->pltHookCommit();
    }
    Api *api = nullptr;
    JNIEnv *env = nullptr;
};

void zygisk_companion_handler(int fd) {
    auto until = Runtime::deadline();
    uint8_t tag = 0;
    if (!Runtime::readExact(fd, &tag, sizeof(tag), until)) return;
    if (tag == static_cast<uint8_t>(Config::Request::CLEANUP_QUERY)) {
        uint8_t flags = 0;
        std::string process, package;
        if (!Runtime::readExact(fd, &flags, sizeof(flags), until) ||
            (flags & ~(Config::MANAGER_SELECTED | Config::CHILD_ZYGOTE)) ||
            !Config::readString(fd, process, until) || !Config::readString(fd, package, until)) return;
        uint8_t result = static_cast<uint8_t>(Config::selectCleanup(process, package, flags));
        Runtime::writeExact(fd, &result, sizeof(result), until);
    } else if (tag == static_cast<uint8_t>(Config::Request::SWITCH_NS)) {
        int ns = Runtime::receiveFd(fd, until);
        if (ns < 0) return;
        int status = Utils::forkAndInvoke([ns]() {
            if (setns(ns, CLONE_NEWNS) != 0) return 1;
            return doUnmount() ? 0 : 2;
        });
        close(ns);
        uint8_t result = Runtime::childSucceeded(status) ? 1 : 0;
        Runtime::writeExact(fd, &result, sizeof(result), until);
    } else LOGW("Unknown companion request: %u", tag);
}
REGISTER_ZYGISK_MODULE(ZygiskModule)
REGISTER_ZYGISK_COMPANION(zygisk_companion_handler)

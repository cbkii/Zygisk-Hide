#include <unistd.h>
#include <sched.h>
#include <sys/mount.h>

#include <cstdint>
#include <functional>

#include "zygisk.hpp"
#include "logging.hpp"
#include "utils.hpp"
#include "modules.hpp"
#include "fd_reopener.hpp"
#include "config.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static std::function<void()> callbackFunction = []() {};

/*
 * [What's the purpose of this hook?]
 * Hooking unshare is necessary to stop Zygote from calling unshare a second time,
 * because that breaks the FDs. We handle this by reopening FDs,
 * allowing us to call unshare twice safely in our callback.
 *
 * [Doesn't Android already call unshare?]
 * Android's use of unshare changes with each major version, so we always call unshare
 * in preAppSpecialize.
 * > Android 5: Sometimes calls unshare
 * > Android 6: Always calls unshare
 * > Android 7-11: Sometimes calls unshare
 * > Android 12-14: Always calls unshare
 */
DCL_HOOK_FUNC(static int, unshare, int flags)
{
    callbackFunction();
    // Do not allow CLONE_NEWNS.
    flags &= ~(CLONE_NEWNS);
    if (!flags)
    {
        // If CLONE_NEWNS was the only flag, skip the call.
        errno = 0;
        return 0;
    }
    return old_unshare(flags);
}

/*
 * [What's the purpose of this hook?]
 * Hooking setresuid ensures we can execute code while we still have CAP_SYS_ADMIN,
 * which is necessary for some operations.
 * This hook is necessary because setresuid is called unconditionally,
 * and we need to perform actions before this syscall.
 */
DCL_HOOK_FUNC(static int, setresuid, uid_t ruid, uid_t euid, uid_t suid)
{
    callbackFunction();
    return old_setresuid(ruid, euid, suid);
}

/*
 * [Why is this function needed?]
 * This function unconditionally calls unshare to create a new mount namespace.
 * It ensures that the new namespace is isolated but still allows propagation of mount
 * events from the parent namespace by setting the root as MS_SLAVE.
 */
static bool new_mount_ns()
{
    /*
     * Unconditional unshare.
     */
    ASSERT_DO(new_mount_ns, old_unshare(CLONE_NEWNS) != -1, return false);

    /*
     * Mount the app mount namespace's root as MS_SLAVE, so every mount/umount from
     * Zygote shared pre-specialization namespace is propagated to this one.
     */
    ASSERT_DO(new_mount_ns, mount("rootfs", "/", NULL, (MS_SLAVE | MS_REC), NULL) != -1, return false);
    return true;
}

class ZygiskModule : public zygisk::ModuleBase
{
public:
    void onLoad(Api *api, JNIEnv *env) override
    {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override
    {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);

        uint32_t flags = api->getFlags();
        bool isRoot = (flags & zygisk::StateFlag::PROCESS_GRANTED_ROOT) != 0;
        bool isOnDenylist = (flags & zygisk::StateFlag::PROCESS_ON_DENYLIST) != 0;
        bool isChildZygote = args->is_child_zygote != NULL && *args->is_child_zygote;

        // Cheap checks first, same as before: rooted processes and non-user
        // apps are never candidates for hiding regardless of any list.
        if (isRoot || !Utils::isUserAppUID(args->uid))
        {
            LOGD("Skipping ppid=%d uid=%d isChildZygote=%d", getppid(), args->uid, isChildZygote);
            return;
        }

        std::string niceName = Utils::jstringToUtf8(env, args->nice_name);

        // The manager's own denylist ("Exclude Modifications"/"Unmount
        // Modules") and the module's WebUI per-app list are two
        // independent, additive sources for the same decision: being
        // listed in EITHER one is enough to hide this app.
        bool isInWebUiList = false;
        {
            int configFd = api->connectCompanion();
            if (configFd != -1)
            {
                isInWebUiList = Config::queryIsPackageHidden(configFd, niceName);
                close(configFd);
            }
            else
            {
                LOGW("preAppSpecialize: failed to connect companion for WebUI config query, pkg=%s", niceName.c_str());
            }
        }

        bool shouldHide = isOnDenylist || isInWebUiList;
        if (!shouldHide)
        {
            LOGD("Skipping ppid=%d uid=%d isChildZygote=%d pkg=%s (not denylisted, not in WebUI list)",
                 getppid(), args->uid, isChildZygote, niceName.c_str());
            return;
        }
        LOGD("Processing ppid=%d uid=%d isChildZygote=%d pkg=%s (denylist=%d webui=%d)",
             getppid(), args->uid, isChildZygote, niceName.c_str(), isOnDenylist, isInWebUiList);

        ASSERT_DO(preAppSpecialize, hookPLTByName("libandroid_runtime.so", "unshare", new_unshare, &old_unshare), return);
        ASSERT_DO(preAppSpecialize, hookPLTByName("libandroid_runtime.so", "setresuid", new_setresuid, &old_setresuid), return);

        int companionFd = -1;
        ASSERT_LOG(preAppSpecialize, (companionFd = api->connectCompanion()) != -1);
        ASSERT_LOG(preAppSpecialize, companionFd != -1 && api->exemptFd(companionFd));

        callbackFunction = [fd = companionFd]()
        {
            // Call only once per process.
            callbackFunction = []() {};
            FDReopener::ScopedRegularReopener srr;
            
            if (!new_mount_ns())
                return;

            bool result = false;
            if (fd != -1)
            {
                do
                {
                    uint8_t tag = static_cast<uint8_t>(Config::Request::SWITCH_NS);
                    ASSERT_DO(callbackFunction, write(fd, &tag, sizeof(tag)) == sizeof(tag), break);
                    pid_t pid = getpid();
                    ASSERT_DO(callbackFunction, write(fd, &pid, sizeof(pid)) == sizeof(pid), break);
                    ASSERT_DO(callbackFunction, read(fd, &result, sizeof(result)) == sizeof(result), break);
                } while (false);
                close(fd);
            }

            if (result)
                LOGD("Invoking the companion was successful.");
            else
            {
                LOGW("Invoking the companion failed. Functionality will be limited in Zygote context!");
                doUnmount();
            }

            doHideZygisk();
        };
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override
    {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override
    {
        if (old_unshare != nullptr)
            ASSERT_LOG(postAppSpecialize, hookPLTByName("libandroid_runtime.so", "unshare", old_unshare));
        if (old_setresuid != nullptr)
            ASSERT_LOG(postAppSpecialize, hookPLTByName("libandroid_runtime.so", "setresuid", old_setresuid));
    }

    template <typename T>
    bool hookPLTByName(const std::string &libName, const std::string &symbolName, T *hookFunction, T **originalFunction = nullptr)
    {
        return Utils::hookPLTByName(api, libName, symbolName, (void *)hookFunction, (void **)originalFunction) && api->pltHookCommit();
    }

private:
    Api *api;
    JNIEnv *env;
};

void zygisk_companion_handler(int fd)
{
    uint8_t reqType = 0;
    ASSERT_DO(zygisk_companion_handler, read(fd, &reqType, sizeof(reqType)) == sizeof(reqType), return);

    if (reqType == static_cast<uint8_t>(Config::Request::CONFIG_QUERY))
    {
        // Answer a WebUI per-app config lookup. This is the only place in
        // the whole module that reads config.json -- always in the
        // companion's unrestricted root context, never from Zygote/app
        // process space.
        uint16_t len = 0;
        ASSERT_DO(zygisk_companion_handler, read(fd, &len, sizeof(len)) == sizeof(len), return);

        std::string pkg;
        if (len > 0)
        {
            pkg.resize(len);
            ASSERT_DO(zygisk_companion_handler, read(fd, pkg.data(), len) == (ssize_t)len, return);
        }

        uint8_t result = Config::isPackageHidden(pkg) ? 1 : 0;
        ASSERT_LOG(zygisk_companion_handler, write(fd, &result, sizeof(result)) == sizeof(result));
        return;
    }

    if (reqType == static_cast<uint8_t>(Config::Request::SWITCH_NS))
    {
        pid_t pid;
        ASSERT_DO(zygisk_companion_handler, read(fd, &pid, sizeof(pid)) == sizeof(pid), return);
        LOGD("zygisk_companion_handler processing namespace of pid=%d", pid);

        // setns requires the caller to be single-threaded
        bool result = WIFEXITED(Utils::forkAndInvoke(
            [pid]()
            {
                ASSERT_DO(zygisk_companion_handler, Utils::switchMountNS(pid), return 1);
                doUnmount();
                doRemount();
                doMrProp();
                return 0;
            }));

        ASSERT_LOG(zygisk_companion_handler, write(fd, &result, sizeof(result)) == sizeof(result));
        return;
    }

    LOGW("zygisk_companion_handler: unknown request type %d", reqType);
}

REGISTER_ZYGISK_MODULE(ZygiskModule)
REGISTER_ZYGISK_COMPANION(zygisk_companion_handler)

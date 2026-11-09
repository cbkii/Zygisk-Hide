// Exercise production callback sequencing with a fake public Zygisk API.
// This tests lifecycle decisions, not the real loader ABI or SELinux access.
#include <cassert>
#include <sys/mount.h>

static int unshareCalls = 0, unshareFlags = 0, unshareResult = 0;
static int mountCalls = 0, mountResult = 0;
static int fakeMount(const char *, const char *, const char *, unsigned long, const void *) {
    ++mountCalls;
    return mountResult;
}
#define mount fakeMount
#include "../module/jni/main.cpp"
#undef mount

static int originalUnshare(int flags) {
    ++unshareCalls;
    unshareFlags = flags;
    return unshareResult;
}
static int originalUid(uid_t, uid_t, uid_t) { return 0; }
std::string Utils::jstringToUtf8(JNIEnv *, jstring s) { return s ? s : ""; }
int Utils::isUserAppUID(int uid) { return uid >= 10000; }
bool Utils::hookPLTByName(zygisk::Api *, const std::string &, const std::string &name, void *, void **old) {
    if (old) *old = name == "unshare" ? reinterpret_cast<void *>(originalUnshare) : reinterpret_cast<void *>(originalUid);
    return true;
}
int Utils::forkAndInvoke(const std::function<int()> &) { return -1; }
bool doUnmount() { return false; }
FDReopener::ScopedRegularReopener::ScopedRegularReopener() = default;
FDReopener::ScopedRegularReopener::~ScopedRegularReopener() = default;
static void reset() {
    old_unshare = nullptr; old_setresuid = nullptr;
    unshareHooked = uidHooked = pending = privateNamespace = false;
    unshareCalls = unshareFlags = unshareResult = mountCalls = mountResult = 0;
    companionFd = -1;
}
int main() {
    for (int failure : {0, 1, 2, 3, 4}) {
        reset();
        zygisk::Api api; api.failCommit = failure;
        ZygiskModule module; module.onLoad(&api, nullptr);
        zygisk::AppSpecializeArgs args{10123};
        module.preAppSpecialize(&args);
        assert(api.unloads == 0); // Never request unload while a hook may remain.
        assert(api.commits == (failure == 1 ? 1 : 2));
        module.postAppSpecialize(&args);
        assert(api.unloads == (failure == 3 || failure == 4 ? 0 : 1));
        assert(!pending && companionFd == -1);
    }
    reset();
    zygisk::Api api; api.flags = zygisk::PROCESS_GRANTED_ROOT | zygisk::PROCESS_ON_DENYLIST;
    ZygiskModule module; module.onLoad(&api, nullptr);
    zygisk::AppSpecializeArgs args{10123};
    module.preAppSpecialize(&args); module.postAppSpecialize(&args);
    assert(api.commits == 0 && api.unloads == 1);
    reset();
    api = {}; api.flags = 0;
    module.preAppSpecialize(&args); module.postAppSpecialize(&args);
    assert(api.commits == 0 && api.unloads == 1);
    reset();
    api = {}; args.uid = 1000;
    module.preAppSpecialize(&args); module.postAppSpecialize(&args);
    assert(api.commits == 0 && api.unloads == 1);
    for (unsigned managerFlags : {0u, unsigned(zygisk::PROCESS_ON_DENYLIST)}) {
        reset();
        api = {};
        api.flags = managerFlags;
        int sockets[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
        api.nextCompanionFd = sockets[0];
        uint8_t reply = static_cast<uint8_t>(Config::Cleanup::Loader);
        assert(write(sockets[1], &reply, 1) == 1);
        bool child = true;
        args = {10123, "com.example_zygote", nullptr, &child};
        module.preAppSpecialize(&args);
        assert(api.unmountRequests == 1 && api.companionConnections == 1);
        uint8_t request[2]{};
        assert(Runtime::readExact(sockets[1], request, sizeof(request), Runtime::deadline()));
        assert(request[0] == static_cast<uint8_t>(Config::Request::CLEANUP_QUERY));
        assert(request[1] == (Config::CHILD_ZYGOTE | (managerFlags ? Config::MANAGER_SELECTED : 0)));
        close(sockets[1]);
        module.postAppSpecialize(&args);
        assert(api.commits == 0 && api.unloads == 1 && companionFd == -1);
    }
    reset();
    old_unshare = originalUnshare;
    pending = true;
    assert(new_unshare(CLONE_NEWNET) == 0 && pending);
    assert(unshareCalls == 1 && mountCalls == 0);

    reset();
    old_unshare = originalUnshare;
    pending = true;
    assert(new_unshare(CLONE_NEWNS | CLONE_NEWNET) == 0);
    assert(unshareCalls == 1 && unshareFlags == (CLONE_NEWNS | CLONE_NEWNET));
    assert(mountCalls == 1 && !pending);
    assert(new_unshare(CLONE_NEWNS) == 0); // Also applies if restoration retained the hook.
    assert(unshareCalls == 2 && mountCalls == 1 && unshareFlags == CLONE_NEWNS);

    reset();
    old_unshare = originalUnshare;
    old_setresuid = originalUid;
    pending = true;
    mountResult = -1;
    assert(new_unshare(CLONE_NEWNS) == 0 && pending && privateNamespace);
    mountResult = 0;
    assert(new_setresuid(10123, 10123, 10123) == 0);
    assert(unshareCalls == 1 && mountCalls == 2 && !pending);

    reset();
    old_unshare = originalUnshare;
    old_setresuid = originalUid;
    pending = true;
    unshareResult = -1;
    assert(new_unshare(CLONE_NEWNS) == -1 && pending && !privateNamespace);
    unshareResult = 0;
    assert(new_setresuid(10123, 10123, 10123) == 0);
    assert(unshareCalls == 2 && mountCalls == 1 && !pending);
}

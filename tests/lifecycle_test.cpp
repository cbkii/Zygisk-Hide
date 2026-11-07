// Exercise production callback sequencing with a fake public Zygisk API.
// This tests lifecycle decisions, not the real loader ABI or SELinux access.
#include <cassert>
#include "../module/jni/main.cpp"
static int originalUnshare(int) { return 0; }
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
    unshareHooked = uidHooked = pending = namespaceReady = false;
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
    reset();
    old_unshare = originalUnshare;
    pending = true;
    assert(new_unshare(CLONE_NEWNET) == 0 && pending); // Unrelated call must not consume cleanup.
}

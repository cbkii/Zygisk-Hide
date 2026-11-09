#include <string>
#include <cstring>
#include <string_view>
#include <set>
#include <atomic>
#include <unordered_map>
#include <cstdint>
#include <sys/mount.h>
#include "runtime_support.hpp"

#include "zygisk.hpp"
#include "logging.hpp"
#include "map_parser.hpp"
#include "mountinfo_parser.hpp"
#include "utils.hpp"

using namespace Parsers;

static const std::set<std::string> mountdir_list = {"/data/adb", "/debug_ramdisk"};
static const std::set<std::string> fsname_list = {"KSU", "APatch", "magisk", "worker"};
static bool shouldUnmount(const mountinfo_entry &mount, const mountinfo_root_resolver &root_resolver)
{
    const auto true_root = root_resolver.resolveRootOf(mount);
    const auto &mount_point = mount.getMountPoint();
    const auto &type = mount.getFilesystemType();

    // Unmount all mounts from and to directories in mountdir_list
    for (const auto &mountdir : mountdir_list)
    {
        if (Runtime::pathWithin(true_root, mountdir) || Runtime::pathWithin(mount_point, mountdir))
            return true;

        // Unmount all overlayfs with lowerdir/upperdir/workdir in mountdir_list
        if (type == "overlay")
        {
            const auto &options = mount.getSuperOptions();

            for (const char *key : {"lowerdir", "upperdir", "workdir"}) {
                auto option = options.find(key);
                if (option == options.end()) continue;
                std::string_view layers = option->second;
                while (!layers.empty()) {
                    auto colon = layers.find(':');
                    if (Runtime::pathWithin(layers.substr(0, colon), mountdir)) return true;
                    if (colon == std::string_view::npos) break;
                    layers.remove_prefix(colon + 1);
                }
            }
        }
    }

    // Unmount all module overlayfs and tmpfs
    if ((type == "overlay" || type == "tmpfs") && fsname_list.contains(mount.getMountSource()))
        return true;

    return false;
}

bool doUnmount()
{
    const auto &mount_infos = parseSelfMountinfo(false);
    if (mount_infos.empty()) return false;
    bool complete = true;
    auto root_resolver = mountinfo_root_resolver(mount_infos);

    for (auto it = mount_infos.rbegin(); it != mount_infos.rend(); it++)
    {
        if (shouldUnmount(*it, root_resolver))
        {
            const auto &mount_point_cstr = it->getMountPoint().c_str();
            if (umount2(mount_point_cstr, MNT_DETACH) == 0)
                LOGD("umount2(\"%s\", MNT_DETACH) returned 0", mount_point_cstr);
            else {
                complete = false;
                LOGW("umount2(\"%s\", MNT_DETACH) returned -1: %d (%s)", mount_point_cstr, errno, strerror(errno));
            }
        }
    }
    return complete;
}

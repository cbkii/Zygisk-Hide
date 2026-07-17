#pragma once
#include <string>
#include <cstdint>

/*
 * Support for the module's WebUI, which lets the user toggle root/Zygisk
 * hiding on a per-app basis independently of the root manager's own
 * denylist ("Exclude Modifications" / "Unmount Modules" toggle).
 *
 * The WebUI (webroot/index.html) writes a flat JSON object mapping
 * package name -> bool at CONFIG_PATH, e.g.:
 *   { "com.example.app": true, "com.other.app": true }
 * Only truthy entries are treated as "hide this app"; the WebUI simply
 * omits an app's key when it's toggled off.
 *
 * This file is read by the Zygisk companion process (root, unrestricted
 * SELinux domain), never directly from within the Zygote/app process,
 * since preAppSpecialize runs in a much more restricted context.
 */
namespace Config
{
    constexpr const char *CONFIG_PATH = "/data/adb/modules/zygisk-hide/config.json";

    // Companion request protocol tags. Sent as the first byte of any
    // message to zygisk_companion_handler so it can dispatch correctly.
    enum class Request : uint8_t
    {
        SWITCH_NS = 1,
        CONFIG_QUERY = 2,
    };

    /*
     * Server-side: reads CONFIG_PATH fresh (no caching, so WebUI edits
     * take effect on the very next app launch) and checks whether
     * packageName is present with a truthy value.
     * Safe to call from the companion only.
     */
    bool isPackageHidden(const std::string &packageName);

    /*
     * Client-side: sends a CONFIG_QUERY request for packageName over an
     * already-connected companion fd and blocks for the boolean answer.
     * Does NOT close companionFd; caller owns its lifetime.
     * Returns false (i.e. "don't additionally hide") on any I/O error,
     * so a broken/missing config can never crash or wedge app startup.
     */
    bool queryIsPackageHidden(int companionFd, const std::string &packageName);
}

# Pixel 9a / Magisk Alpha qualification

## Reference and scope

User-supplied `app-release.apk`, inspected as data (not installed/executed):

- SHA-256: `e31eb8b7a9c8920b6749185230896094c2dc90cca1f5dcbfac534c92dbe602f8`
- Manifest package: `io.github.vvb2060.magisk`
- Manifest and `assets/util_functions.sh`: version `96221b69-alpha`, code `31000`.
- Manifest compile SDK: 37 (Android 17). This does not establish the phone's OS.

Although supplied as an Alpha 30 reference, the APK itself declares code 31000.
No APK bytes, keys, attestations or personal device dumps are committed here.

Target runtime: Pixel 9a `tegu`, Android 16, Alpha code 31000, Zygisk Next 1.5.2,
PIF v4.9-inject-s and TEESimulator v4.0. No Magisk-private database, structure,
SELinux type renaming or loader offset is assumed. Zygisk API v4 remains unchanged.

## Recovery before installation

Keep a bootable recovery path and the current module configuration. Test one hide
implementation at a time. Do not change kernel, root implementation, keybox and
Hide version in the same comparison. No GMS/GSF/keystore data deletion is needed.

If a target app crashes or module restoration errors appear, disable Zygisk Hide
in the root manager and reboot. With an available root shell:

```sh
su -c 'touch /data/adb/modules/zygisk-hide/disable'
```

Reboot. Config remains in `/data/adb/zygisk-hide`; disabling stops the module, not
PIF/TEE or other modules. Recovery without a working shell depends on the existing
root manager/device recovery setup; this ZIP does not create a recovery facility.

## Controlled comparisons

Record a baseline with Hide disabled. First compare a minimal stack of Alpha,
one Zygisk provider, PIF and TEESimulator. Next add Hide; finally restore Vector,
HMA and other interception modules one at a time. Reboot between states because
loaded policy and app zygotes can outlive a toggle.

Keep the keybox, PIF profile, detector version and loader memory mode unchanged
through a comparison. Capture main and isolated-service processes, not only the
launcher PID. Anonymous RX region size/address alone is not module attribution.

Run this read-only diagnostic for each observed target PID:

```sh
su -c 'sh /data/adb/modules/zygisk-hide/diagnose.sh PID'
```

Replace `PID` with the numeric process ID. Its privileged view does not establish
which `/proc` entries a detector app can access. Do not publish unrestricted phone
logs; the included command intentionally avoids keybox and account data.

| Gate | Required evidence | Current status |
| --- | --- | --- |
| Native build | All four ABIs, release/debug ZIP validation and checksums | CI required |
| Startup | Three cold boots and repeated normal/isolated app launches without new crashes or ANRs | Not tested on device |
| Injection | PIF still loads in GMS and the selected Hide paths actually run | Not tested on device |
| Unloading | No `Hook restoration failed` event; Hide DSO absent after successful cleanup | Not tested on device |
| Memory | Compare anonymous RX mappings across minimal/full stacks; attribute owner before a proposed fix | Existing ZN/other-module mappings remain outside this module |
| SELinux | Enforcing unchanged; compare detector policy findings after disabling rule providers and rebooting | Global Magisk/Vector/ZN policy is not removed by Hide |
| Mounts | Selected app/isolated namespace cleaned; identify exact external PID if detector still flags one | Not tested on device |
| Google | Record checker name/version, Classic or Standard API, full error or returned verdict, and Play certification separately | Not tested on device |
| Rollback | Disable and reboot restores baseline application behaviour | Not tested on device |

For Integrity errors, record the API family: Classic `-17` is
`CLIENT_TRANSIENT_ERROR`; Standard `-17` is `REQUEST_HASH_TOO_LONG`. These are
request errors, not returned Basic/Device/Strong labels. See Google's
[error reference](https://developer.android.com/google/play/integrity/error-codes).

## Remaining SELinux/memory work

The previous Hunter result identified `MAGISK_EXCLUSIVE_CONTEXT`,
`SYSTEM_SERVER_EXECMEM`, `XPOSED_POLICY` and `ZYGISK_NEXT_POLICY`. Those are not
created by this module's config parser or mount namespace. Do not add another
allow rule or a userspace `selinux_check_access` hook and describe that as a fix:
a detector using another policy interface or raw syscall can still observe the
underlying state. Source-provider changes require their own review and validation.

The previous two anonymous RX regions (880640 and 827392 bytes) predated Hide
installation. This patch cannot establish their owner. A clean Hide unload is a
necessary regression gate, not proof those regions disappear. Use loader/module
A/B evidence before changing memory handling; never unmap unidentified code.

## November 8 audit and alpha2 comparison

The archived main-process snapshot (PID 6293) showed two unlabeled private RX
regions of 860 KiB and 808 KiB, separate from the ART JIT memfds. That snapshot
contained zero matching root mount-marker rows. It did not sample the app zygote
or the isolated processes visible in the process inventory. Package enumeration
failed with a Binder transaction error, leaving no usable flagged-UID inventory.
Zygisk Hide did not appear in the module inventory for this capture.

The effective policy sample contains Magisk socket access and broad
`magisk_file` and `xposed_data` allows. The active files attribute the broad
`magisk_file` grants and `system_server` execmem to Zygisk Next, the Xposed
rules to Vector, and netd execmem to hostsredirect. The presence of a rule in
a module's installed files by itself does not identify all its live providers.

The [ReZygisk injector](https://github.com/PerformanC/ReZygisk/blob/4aff785a1b82250ab0be7b5e4cae5cdb2f4d988b/loader/src/injector/hook.c)
and [NeoZygisk injector](https://github.com/JingMatrix/NeoZygisk/blob/cc0bbba7c2e0215b0f3fe1fb27e2ee88ade9e688/loader/src/injector/hook.cpp)
restore their own hooks before unmapping their own loader ranges. They have
loader-level knowledge of those ranges and the return path. Both also support
provider-controlled mount cleanup through the public module option. Their
[ReZygisk rules](https://github.com/PerformanC/ReZygisk/blob/4aff785a1b82250ab0be7b5e4cae5cdb2f4d988b/module/src/sepolicy.rule)
and [NeoZygisk rules](https://github.com/JingMatrix/NeoZygisk/blob/cc0bbba7c2e0215b0f3fe1fb27e2ee88ade9e688/module/src/sepolicy.rule)
still include `system_server` execmem. NeoZygisk's optional path remapping
creates anonymous copies, which would change the shape of this detector's evidence
without establishing ownership of the existing two regions.

Run the same capture after each reboot and after opening Duck and exercising
the probes so that its helpers are live:

```sh
su -c 'sh /data/adb/modules/zygisk-hide/diagnose.sh --package com.eltavine.duckdetector'
```

1. Capture with Hide disabled, then enabled with its default local backend.
2. Enable **Use loader unmount for selected processes**, reboot, and repeat.
   Keep the provider, its memory mode, Vector, HMA and the target lists fixed.
3. Compare per-process mount marker counts, anonymous RX signatures and
   loader/Hunter/Duck results. A debug build logs which backend ran. The
   message “Loader unmount requested” records a request, so the namespace
   snapshot is the verification.
4. If testing the loader separately, Zygisk Next
   [v1.5.1 introduced the zygote blacklist](https://github.com/LSPosed/ZygiskNext/releases/tag/v1.5.1).
   Only consider a separate A/B if the installed loader identifies a specific
   detector zygote in that UI. Shared zygotes affect unrelated apps. Record the
   original setting, reboot for each comparison, and restore it if helpers or
   dependent features fail. A zygote exclusion may still leave inherited
   mappings from its parent and global SELinux policy remains loaded.
5. For the policy comparison, disable Vector separately and reboot before
   rechecking Xposed rules. The hostsredirect netd rule has its own separate
   comparison. Changes to a loaded loader policy require testing at the
   provider level.

The app-zygote matcher strips the AOSP
[`_zygote` suffix](https://github.com/aosp-mirror/platform_frameworks_base/blob/main/core/java/android/os/AppZygote.java)
only when the loader identifies the specialization as a child zygote. Scope
that reaches an isolated helper depends on the callbacks the loader actually
delivers. The package diagnostic also supports a numeric PID for custom names.

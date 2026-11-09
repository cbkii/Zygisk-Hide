# Zygisk Hide

Zygisk Hide cleans up the filesystem view of selected Android apps. Choose
targets in the WebUI or use the root manager's denylist. Hide handles cleanup
itself by default and can ask the Zygisk provider to do it instead. Read-only
diagnostics help compare the main app process, helpers and app zygote.

This fork builds on [snake-4's Zygisk Assistant](https://github.com/snake-4/Zygisk-Assistant)
and uses the per-app WebUI from [kgr-online's Zygisk Hide](https://github.com/kgr-online/Zygisk-Hide).
It uses Zygisk API v4 on Magisk, KernelSU and APatch.

## Fork build v3.2.0

The build targets Android 14+ and Magisk 26+, including Magisk Alpha version
code 31000. Runtime qualification for Alpha 31000 with Zygisk Next 1.5.2 on
the Pixel 9a running Android 16 is tracked in the
[Pixel qualification guide](docs/PIXEL_QUALIFICATION.md).

### What's changed

- Covers conventional `package:process` helpers and custom process names when
  Android supplies a package-specific `app_data_dir`. Manager denylist selection
  remains an independent, additive source. Unattributed isolated processes need
  the manager's process selection; they are not blanket-selected by UID range.
- Restores temporary specialisation hooks and keeps the library mapped when
  local cleanup installs hooks. Loader-managed cleanup requests unloading
  without installing its own hooks.
- Bounds companion socket I/O, transfers namespace FDs instead of arbitrary PIDs,
  and treats child failure exits as failures.
- Removes the heuristic write into `libnativebridge.so`, property-area rewriting
  and `/data` error-mode remounting from native cleanup. Those operations do not
  remove another module's anonymous executable memory.
- Adds no SELinux policy, executable-memory allocation or persistent syscall hook.
  Existing Magisk, Vector and Zygisk Next policy can still be detectable.
- Keeps configuration outside the module directory so upgrades preserve it.

## Installation and configuration

1. Disable another enabled Zygisk Assistant/Hide instance and reboot first.
2. Install the release ZIP through your root manager and reboot. CI also provides
   debug ZIPs that log process names for short diagnostic comparisons.
3. Use **one** Zygisk provider. With **Zygisk Next**, leave Magisk's built-in
   Zygisk **off**. With Magisk's own provider, enable built-in Zygisk instead.
4. Full denylist enforcement/exclusion can prevent this and PIF from loading.
   With Zygisk Next and PIF, use a configuration that still permits injection
   (for the Pixel qualification target, `Unmount Only`), then verify actual module loading.
5. Add target packages in WebUI or the manager denylist. The two lists are
   additive; switching a WebUI toggle off does not override the manager list.
6. Restart target apps. Reboot for an isolated-service/app-zygote comparison.

Use KSWebUI Standalone or another compatible root WebUI host on Magisk. The module
cannot add WebUI support to a manager that does not provide that interface.

Configuration: `/data/adb/zygisk-hide/config.json`, owned by root, mode `0600`:

```json
{
  "com.example": true,
  "com.example:custom_helper": true
}
```

The schema is a flat map of plain package/process names to booleans (64 KiB and
1024-entry limits). Duplicate keys, escaped names and unsupported values are
rejected. Invalid WebUI config does not cancel independent manager selection.
The installer migrates an existing module-directory config without deleting it.

Default operation leaves global properties alone to avoid competing with PIF and
TEE modules. For legacy upstream boot-property scripts only, explicitly create
`/data/adb/zygisk-hide/legacy-props` and reboot. Remove that marker and reboot to
stop those scripts. This does not change hardware attestation or lock a bootloader.

## Loader unmount mode

Enable **Use loader unmount for selected processes** in WebUI to request
`FORCE_DENYLIST_UNMOUNT` through Zygisk API v4. This skips Hide's own temporary
mount hooks and namespace companion work. Selection still comes from WebUI or
the manager list. It uses one bounded companion query, then requests unloading
at the end of specialization.

The default is Hide's existing local cleanup. The loader option returns no
success status. Verify that your provider honours it and that the target's
namespace is clean. Reopen the app to test changes, and reboot to recreate
its app zygote and isolated children.

The packaged diagnostic accepts a numeric PID or `--package PACKAGE`. Package
mode reports the main process, conventional helpers and the app zygote. Compare
mapping categories and region sizes across runs, since addresses change with ASLR.
Use `--inline-scope [PROCESS ...]` to see which exact processes Inline Hook
Invalidate is configured to target and which remap mode each uses. The scope
report reads saved configuration.
Use a rebooted comparison to verify actual process behaviour.

## Detector and Integrity limitations

This build does **not** claim to erase Magisk/Vector/Zygisk Next rules from loaded
SELinux policy, remove another module's memory mappings, or clean every other
process's namespace. A process-local filesystem namespace cannot remove kernel
SELinux types. Renaming anonymous executable mappings would not resolve their
origin, so this module does not do that.

Certification and Play Integrity require separate evidence from Google. Local
Hunter/Duck results are not Google verdicts. See [the Pixel qualification procedure](docs/PIXEL_QUALIFICATION.md).

## Build and validation

```sh
git submodule update --init --recursive
./tests/run.sh
./gradlew moduleZipRelease moduleZipDebug
python3 tests/test_packaging.py module/build/outputs/zip/*.zip
```

Java 17, Android SDK 34 and NDK `28.0.13004108` are used by CI. The native module
supports 16 KiB page alignment. Host tests cover config, process selection,
short/dead-peer/timeout IPC and hook restoration failures; they do not simulate
Android permissions or prove the loader ABI works on a phone.

This fork uses manual ZIP installation. To roll back, disable Zygisk Hide in
your root manager and reboot. Your target settings are kept separately.

MIT licence. Original credits and licence notices are retained in the source.

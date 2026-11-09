## v3.2.0

- Improve Magisk Alpha 31000 compatibility through Zygisk API v4. Apply target
  settings to named helpers and recognised app zygotes, and preserve settings
  across upgrades.
- Restore temporary hooks and keep the library loaded after local hook
  installation, as required by Zygisk API v4. Handle repeated finalisation
  safely and report companion cleanup failures.
- Keep local cleanup as the default. You can ask the Zygisk provider to unmount
  for selected processes. Verify the result in the target's mount namespace.
- Remove native-bridge patching and global property and remount work from native
  cleanup. Legacy boot-property scripts are opt-in.
- Add read-only process-family diagnostics for namespaces, mounts and executable
  mappings, plus exact-process scope checks for Inline Hook Invalidate.
- Install manually through your root manager and reboot. Disable the module
  and reboot to roll back.

## 3.0.1
* Added WebUI (webroot) for per-app root/Zygisk hide toggling, independent of the manager's own denylist.
* Hide decision is now additive: manager denylist OR WebUI config, either is enough.
* Added a "Show system apps" toggle to the WebUI app list.

## 2.1.4
* Fixed a problem causing Zygisk Assistant to be detectable.
* Updated prop scripts.
* Compiled with a newer compiler.

## 2.1.3
* Restored Shamiko v1.1.1 compatibility.
* Fixed bootloop on some Xiaomi devices.
- Removed unnecessary mount ID regeneration.

## 2.1.2
+ Added scripts to reset sensitive props.

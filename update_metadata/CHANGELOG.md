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

## v3.2.0-alpha1 (development)

- Target Alpha 31000 through the unchanged public Zygisk API v4.
- Fix subprocess selection, failed-child reporting, bounded companion I/O,
  namespace FD handoff and unload-after-restoration lifecycle.
- Remove heuristic native-bridge editing and native global-property/remount work.
- Default global boot-property scripts to opt-in; add no SELinux rule.
- Persist and atomically save target config outside module updates.
- Add host regression tests, package validation and Pixel qualification guidance.
- Fix repeated/recursive finalization and allocation-failure handling during module unload.
- Disable the alpha update feed pending a qualified release.
- Device qualification and Google verdicts remain unverified.

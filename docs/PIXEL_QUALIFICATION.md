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

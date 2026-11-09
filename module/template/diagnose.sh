#!/system/bin/sh
# Read-only root-side evidence. No keybox, accounts, app data or memory bytes.
usage() {
    echo 'Usage: diagnose.sh PID | diagnose.sh --package PACKAGE | diagnose.sh --inline-scope [PROCESS ...]'
    exit 2
}

package=
scope_only=
script_dir=${0%/*}
[ "$script_dir" != "$0" ] || script_dir=.
case "$1" in
    --inline-scope) scope_only=1; shift ;;
    --package)
        [ "$#" = 2 ] || usage
        package=$2
        case "$package" in ''|*[!A-Za-z0-9_.]*) usage ;; esac
        ;;
    ''|*[!0-9]*) usage ;;
    *) [ "$#" = 1 ] || usage ;;
esac
[ "$(id -u)" = 0 ] || { echo 'Root required'; exit 2; }

if [ -n "$scope_only" ]; then
    if [ "$#" = 0 ]; then
        set -- com.google.android.gms com.google.android.gms.unstable com.google.android.gms.persistent \
            com.android.vending com.google.android.gsf com.google.android.apps.walletnfcrel
    fi
    [ "$#" -le 24 ] || usage
    for process do
        case "$process" in ''|*[!A-Za-z0-9_.:]*) usage ;; esac
        [ "${#process}" -le 512 ] || usage
    done

    inline=/data/adb/modules/inline_hook_invalidate
    if [ ! -f "$inline/module.prop" ]; then
        echo 'inline_module=absent'
        exit 0
    fi
    state=enabled
    [ ! -e "$inline/disable" ] || state=disabled
    [ ! -e "$inline/remove" ] || state=removal-pending
    printf 'inline_module=%s\n' "$state"
    awk '/^version=/ {print; exit}' "$inline/module.prop"
    echo 'Module markers describe disk state. Reboot after changing module enablement.'
    config="$inline/config.txt"
    if [ ! -f "$config" ] || [ -L "$config" ] || [ ! -r "$config" ]; then
        echo 'inline_config=missing, unreadable or symlinked'
        exit 1
    fi
    size=$(stat -c '%s' "$config") || exit 1
    case "$size" in ''|*[!0-9]*) exit 1 ;; esac
    if [ "$size" -gt 65536 ]; then
        echo 'inline_config=exceeds diagnostic size limit'
        exit 1
    fi
    stat -c 'inline_config_mode=%a' "$config"
    version_code=$(awk -F= '/^versionCode=[0-9]+$/ {print $2; exit}' "$inline/module.prop")
    echo 'Exact process scope from the saved config. Runtime remap activity requires separate evidence.'
    awk -v "version_code=${version_code:-1}" -v "targets=$*" -f "$script_dir/diagnose-inline.awk" "$config"
    exit $?
fi

if [ -n "$package" ]; then
    # Exact name, private process and AOSP app-zygote naming. Capture at most
    # 24 processes. Names alone do not prove ownership for custom aliases.
    processes=$(ps -A -o PID,ARGS) || exit 1
    targets=$(printf '%s\n' "$processes" | awk -v pkg="$package" '
        $1 ~ /^[0-9]+$/ && ($2 == pkg || $2 == pkg "_zygote" || index($2, pkg ":") == 1) {
            if (++count <= 24) print $1
        }
        END { if (count > 24) print "TRUNCATED" }
    ')
    [ -n "$targets" ] || { echo 'No matching live processes. Open the detector and run its probes first.'; exit 1; }
else
    targets=$1
fi

echo 'Root-side view. An unprivileged detector has different access.'
echo '=== Build and loader ==='
getprop ro.product.device
getprop ro.build.version.release
getprop ro.build.version.sdk
magisk -v
magisk -V
for module in zygisksu zygisk-hide; do
    file="/data/adb/modules/$module/module.prop"
    if [ -r "$file" ]; then
        printf '%s ' "$module"
        awk '/^version=/ {print; exit}' "$file"
    fi
done
if [ -f /data/adb/zygisk-hide/loader-unmount ] && [ ! -L /data/adb/zygisk-hide/loader-unmount ]; then
    echo 'hide_cleanup_setting=loader'
else
    echo 'hide_cleanup_setting=local'
fi
for name in denylist_enforce denylist_policy memory_type; do
    file="/data/adb/zygisksu/$name"
    [ ! -r "$file" ] || { printf '%s=' "$name"; head -c 32 "$file"; echo; }
done

for pid in $targets; do
    if [ "$pid" = TRUNCATED ]; then
        echo 'Process listing capped at 24 matches.'
        break
    fi
    printf '\n=== PID %s ===\n' "$pid"
    if [ ! -r "/proc/$pid/maps" ]; then
        echo 'Process disappeared or maps are unreadable.'
        continue
    fi
    name=$(tr '\000' '\n' < "/proc/$pid/cmdline" | head -n 1)
    if [ -n "$package" ]; then
        case "$name" in "$package"|"${package}_zygote"|"$package":*) ;; *) echo 'Process name changed after enumeration.'; continue ;; esac
    fi
    printf 'process=%s\n' "$name"
    awk '/^(Name|PPid|Uid|Gid):/' "/proc/$pid/status"
    cat "/proc/$pid/attr/current" 2>/dev/null
    readlink "/proc/$pid/ns/mnt"
    echo '=== Executable mapping signatures (address-independent sizes in bytes) ==='
    awk -f "$script_dir/diagnose-maps.awk" "/proc/$pid/maps" || echo 'Mapping read failed.'
    echo '=== Root-side mount markers (first 80) ==='
    awk '/\/data\/adb|debug_ramdisk|magisk|KSU|APatch/ { if (++n <= 80) print }
        END { print "mount_marker_rows=" n+0 }' "/proc/$pid/mountinfo" || echo 'Mount table read failed.'
done
echo 'Anonymous executable regions require attribution. Size matches across processes suggest a relationship.'
echo '=== SELinux and installed rule providers ==='
cat /sys/fs/selinux/enforce
for dir in /data/adb/modules/*; do
    [ -f "$dir/sepolicy.rule" ] || continue
    state=enabled
    [ ! -e "$dir/disable" ] || state=disabled
    [ ! -e "$dir/remove" ] || state=removal-pending
    printf '%s: %s\n' "$state" "${dir##*/}"
    awk '/magisk_|xposed|zygisk|execmem/ { if (++n <= 20) print }' "$dir/sepolicy.rule"
done
echo 'Loaded policy can retain rules until reboot. This section lists installed rule files.'
echo '=== Module log (last 40 entries) ==='
logcat -d -t 40 -s ZygiskHide/JNI 2>/dev/null

#!/system/bin/sh
# Read-only root-side evidence. No keybox, accounts, app data or memory bytes.
usage() {
    echo 'Usage: diagnose.sh PID | diagnose.sh --package PACKAGE'
    exit 2
}

package=
script_dir=${0%/*}
[ "$script_dir" != "$0" ] || script_dir=.
case "$1" in
    --package)
        [ "$#" = 2 ] || usage
        package=$2
        case "$package" in ''|*[!A-Za-z0-9_.]*) usage ;; esac
        ;;
    ''|*[!0-9]*) usage ;;
    *) [ "$#" = 1 ] || usage ;;
esac
[ "$(id -u)" = 0 ] || { echo 'Root required'; exit 2; }

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

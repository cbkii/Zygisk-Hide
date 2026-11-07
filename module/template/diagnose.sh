#!/system/bin/sh
# Read-only, bounded root-side evidence. No keybox, accounts or app data.
case "$1" in
    ''|*[!0-9]*) echo 'Usage: su -c "sh /data/adb/modules/zygisk-hide/diagnose.sh PID"'; exit 2 ;;
esac
[ "$(id -u)" = 0 ] || { echo 'Root required'; exit 2; }
pid=$1
[ -r "/proc/$pid/maps" ] || { echo 'PID missing or maps unreadable'; exit 1; }
echo 'Root-side diagnostic view; accessibility differs from an unprivileged detector.'
echo '=== Build and loader ==='
getprop ro.product.device
getprop ro.build.version.release
getprop ro.build.version.sdk
magisk -v
magisk -V
for name in denylist_enforce denylist_policy memory_type; do
    file="/data/adb/zygisksu/$name"
    [ ! -r "$file" ] || { printf '%s=' "$name"; head -c 32 "$file"; echo; }
done
echo '=== Process identity ==='
awk '/^(Name|Uid|Gid):/' "/proc/$pid/status"
cat "/proc/$pid/attr/current" 2>/dev/null
readlink "/proc/$pid/ns/mnt"
echo '=== Executable mappings requiring attribution ==='
awk '$2 ~ /x/ {
    if (NF < 6) { anonymous++; print "ANONYMOUS", $0 }
    else if ($0 ~ /zygisk|magisk|lsposed|vector|hma|\/data\/adb/) print "NAMED_MODULE", $0
} END { print "anonymous_executable_regions=" anonymous+0 }' "/proc/$pid/maps"
echo 'Anonymous executable memory alone does not identify its owner or prove injection.'
echo '=== Root-side mount markers (first 80) ==='
awk '/\/data\/adb|debug_ramdisk|magisk|KSU|APatch/ { if (++n <= 80) print }' "/proc/$pid/mountinfo"
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
echo 'Loaded policy can retain rules until reboot; this lists installed files, not a policy verdict.'
echo '=== Module failures (last 40 log entries) ==='
logcat -d -t 40 -s ZygiskHide/JNI 2>/dev/null

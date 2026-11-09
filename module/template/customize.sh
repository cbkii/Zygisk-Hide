# Executed by the root manager's module installer.
[ "$API" -ge 34 ] || abort '! This native build requires Android 14 or newer.'
if [ -n "$MAGISK_VER_CODE" ] && [ "$MAGISK_VER_CODE" -lt 26000 ]; then
    abort '! Zygisk API v4 requires Magisk 26.0 or newer.'
fi
other=/data/adb/modules/zygisk-assistant
if [ -f "$other/module.prop" ] && [ ! -e "$other/disable" ] && [ ! -e "$other/remove" ]; then
    abort '! Disable Zygisk Assistant and reboot before installing Zygisk Hide.'
fi
state=/data/adb/zygisk-hide
[ ! -L "$state" ] || abort '! Refusing symlinked configuration directory.'
umask 077
mkdir -p "$state" || abort '! Cannot create configuration directory.'
[ ! -L "$state/config.json" ] || abort '! Refusing symlinked configuration.'
if [ ! -e "$state/config.json" ]; then
    for old in /data/adb/modules/zygisk-hide/config.json /data/adb/modules/zygisk-assistant/config.json; do
        if [ -f "$old" ] && [ ! -L "$old" ]; then
            cp "$old" "$state/config.json" || abort '! Cannot migrate configuration.'
            break
        fi
    done
fi
[ -e "$state/config.json" ] || printf '{}\n' > "$state/config.json"
set_perm "$state" 0 0 0700
set_perm "$state/config.json" 0 0 0600
set_perm "$MODPATH/diagnose.sh" 0 0 0755
ui_print '- Default: namespace cleanup; no SELinux policy additions or native-bridge memory edit.'
ui_print '- For Zygisk Next, keep Magisk built-in Zygisk OFF.'
ui_print '- Alpha 31000 targeted; on-device qualification is still required.'

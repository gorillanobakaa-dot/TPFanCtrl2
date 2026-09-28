# Gorilla TPFanControl: after install ($1 = 1) or upgrade ($1 = 2).
p=/sys/module/thinkpad_acpi/parameters/fan_control
if [ -r "$p" ] && [ "$(cat "$p")" != "Y" ]; then
    if modprobe -r thinkpad_acpi 2>/dev/null && modprobe thinkpad_acpi 2>/dev/null; then
        echo "gorilla-fan: thinkpad_acpi reloaded with fan_control=1"
    else
        modprobe thinkpad_acpi 2>/dev/null || true
        echo "gorilla-fan: restart the computer once so the fan can be set (thinkpad_acpi is in use)"
    fi
fi
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload || true
    systemctl enable gorilla-fan.service || true
    systemctl restart gorilla-fan.service || true
fi
if [ ! -e /proc/acpi/ibm/fan ]; then
    echo "gorilla-fan: no ThinkPad fan interface on this computer; the service stays idle and changes nothing."
fi
exit 0

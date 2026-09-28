# Gorilla TPFanControl: after erase or upgrade. The settings the service
# created (/etc/gorilla-fan.conf) are kept; delete that file to start afresh.
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload || true
fi
exit 0

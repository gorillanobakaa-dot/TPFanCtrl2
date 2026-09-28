# Gorilla TPFanControl: before erase ($1 = 0) or before an upgrade removes the
# old version ($1 = 1). Only an erase stops the service; stopping runs
# `gorilla-fan release`, which hands the fan back to the BIOS.
if [ "$1" = "0" ] && [ -d /run/systemd/system ]; then
    systemctl stop gorilla-fan.service || true
    systemctl disable gorilla-fan.service || true
fi
exit 0

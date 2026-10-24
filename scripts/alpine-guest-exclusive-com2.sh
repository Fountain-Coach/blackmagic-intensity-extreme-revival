#!/bin/sh
# In Alpine Linux GUEST only, before starting the COM2 Docker frame bridge.
# Prevent Alpine's ttyS1 login prompt from competing with /dev/ttyS1 media.
set -eu
if [ "$(uname -s)" != Linux ] || [ "$(id -u)" != 0 ] ||
   [ ! -c /dev/ttyS1 ] || [ ! -f /etc/inittab ]; then
    echo 'stage=guest_uart result=invalid_environment' >&2
    exit 20
fi
if grep '^ttyS1::respawn:' /etc/inittab >/dev/null 2>&1; then
    # This changes ONLY the ephemeral Alpine guest's /etc/inittab.
    cp /etc/inittab /tmp/fc-inittab-before-serial-guest
    sed -i 's/^ttyS1::respawn:/#ttyS1::respawn:/' /etc/inittab
    kill -HUP 1
    sleep 1
fi
if grep '^ttyS1::respawn:' /etc/inittab >/dev/null 2>&1; then
    echo 'stage=guest_uart result=getty_still_configured' >&2
    exit 21
fi
if ps -ef | grep -E '[g]etty.*ttyS1|[/]bin/login -- ' | grep -v '[g]rep' >/dev/null 2>&1; then
    echo 'stage=guest_uart result=live_login_reader_still_present' >&2
    exit 22
fi
echo 'stage=guest_uart result=exclusive_device_available tty=/dev/ttyS1'
echo 'stage=guest_uart physical_blackmagic_access=0 hdmi_capture_verified=0'

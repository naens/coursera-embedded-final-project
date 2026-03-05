#!/bin/sh

# Mount essential filesystems (not done automatically when using init=)
mount -t proc none /proc
mount -t sysfs none /sys

# KeDei 6.2 TFT display
modprobe spi_bcm2835
modprobe kedei62

# XT keyboard GPIO driver
modprobe xtkbd
modprobe xt-gpio

loadfont < /etc/fonts/ter-i16b.psf

# Remount rootfs read-write so 8086tiny can write to disk images
mount -o remount,rw /

# Run 8086tiny in a new session on tty1.
# Do NOT use "exec setsid" here: PID 1 is a session leader, so setsid
# would fork and exit the parent (PID 1), causing a kernel panic.
# Instead, run setsid in the background and wait for it.
setsid sh -c '
    exec 0<>/dev/tty1 >/dev/tty1 2>&1
    /root/8086tiny/8086tiny /root/8086tiny/bios.bin /root/floppy.img
' &
wait

poweroff -f

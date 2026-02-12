#!/bin/sh
# board/myboard/post-image.sh

# Append to config.txt
cat >> "${BINARIES_DIR}/rpi-firmware/config.txt" <<EOF
dtoverlay=kedei62,rotate=90
dtparam=spi=on
EOF

# Modify cmdline.txt if needed
sed -i 's/console=tty1/console=tty1 fbcon=map:1/' \
    "${BINARIES_DIR}/rpi-firmware/cmdline.txt"

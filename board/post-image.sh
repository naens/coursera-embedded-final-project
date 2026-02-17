#!/bin/sh
# board/myboard/post-image.sh

# Append to config.txt
config_file="${BINARIES_DIR}/rpi-firmware/config.txt"
if ! grep -qF -- "kedei62" "$config_file"; then
    # Ensure file ends with a newline before appending
    [ -s "$config_file" ] && ! [ "$(tail -c 1 "$config_file")" = "" ] && echo >> "$config_file"
    cat >> "$config_file" <<EOF
dtoverlay=pi3-disable-wifi
dtoverlay=pi3-disable-bt
dtoverlay=kedei62,rotate=90
dtparam=spi=on

dtoverlay=button_drv
EOF
fi


# Modify cmdline.txt if needed
cmdline_file="${BINARIES_DIR}/rpi-firmware/cmdline.txt"
if ! grep -qF -- "fbcon=map" "$cmdline_file"
then
    sed -i 's/console=tty1/console=tty1 fbcon=map:1/' "$cmdline_file"
fi

#!/bin/sh

# things to include into the file system from other directories
cp -v ../button_py/* ${TARGET_DIR}/root/button_py

#sed -i \
#    -e 's/^console::/#console::/' \
#    -e 's,^tty1::.*$,tty1::respawn:/sbin/getty -L -n -l /bin/sh tty1 0 vt100,' \
#    ${TARGET_DIR}/etc/inittab

sed -i \
    -e 's,^tty1::.*$,tty1::respawn:/sbin/getty -L -n -l /bin/autologin tty1 0 vt100,' \
    ${TARGET_DIR}/etc/inittab

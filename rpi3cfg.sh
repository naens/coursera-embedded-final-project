#!/usr/bin/env bash

TMP_DEFCONFIG=$(mktemp /tmp/genrpi3_defconfig_XXX)
TMP_DIFF_CFG=$(mktemp /tmp/genrpi3_diff_cfg_XXX)
TMP_RPI3_DEFCONFIG=$(mktemp /tmp/genrpi3_rpi3defconfig_XXX)

BUILDROOT=buildroot
BUILDROOT_RPI3=buildroot_rpi3
EXTERNAL=$(realpath external)
RPI0_DEFCONFIG=${BUILDROOT}/configs/raspberrypi0_defconfig
RPI3_DEFCONFIG=${BUILDROOT}/configs/raspberrypi3_defconfig

# save config from current buildroot
make -C ${BUILDROOT} savedefconfig  BR2_DEFCONFIG=${TMP_DEFCONFIG}

# diff with rpi0 defconfig
diff --new-line-format='%L' --unchanged-line-format='' \
     --old-line-format='' ${RPI0_DEFCONFIG} ${TMP_DEFCONFIG} \
     > ${TMP_DIFF_CFG} || true

# merge defconfig with rpi3
cat ${RPI3_DEFCONFIG} ${TMP_DIFF_CFG} > ${TMP_RPI3_DEFCONFIG}

# generate rpi3 config
make -C ${BUILDROOT_RPI3} defconfig BR2_EXTERNAL=${EXTERNAL} \
     BR2_DEFCONFIG=${TMP_RPI3_DEFCONFIG}

# cleanup
rm ${TMP_DEFCONFIG}
rm ${TMP_DIFF_CFG}
rm ${TMP_RPI3_DEFCONFIG}

################################################################################
#
# kedei
#
################################################################################

KEDEI_VERSION = '28e7bc61b962b2970cee65b31537dabbbc9cff78'
KEDEI_SITE = 'git@github.com:naens/FBTFT_KeDei_35_lcd_v62.git'
KEDEI_SITE_METHOD = git
KEDEI_OVERRIDE_SRCDIR = $(TOPDIR)/../kedei
KEDEI_INSTALL_IMAGES = YES

KEDEI_MODULE_MAKE_OPTS=$(TARGET_CONFIGURE_OPTS)

$(eval $(kernel-module))

define KEDEI_BUILD_DT_OVERLAY
	$(LINUX_DIR)/scripts/dtc/dtc -@ -I dts -O dtb \
		-o $(@D)/kedei.dtbo $(@D)/kedei.dts
endef

KEDEI_POST_BUILD_HOOKS += KEDEI62_BUILD_DT_OVERLAY

define KEDEI_INSTALL_IMAGES_CMDS
	$(INSTALL) -D -m 0644 $(@D)/kedei.dtbo \
		$(BINARIES_DIR)/rpi-firmware/overlays/kedei.dtbo
endef

$(eval $(generic-package))

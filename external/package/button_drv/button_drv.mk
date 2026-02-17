##############################################################
#
# button
#
##############################################################

BUTTON_DRV_VERSION = 0.3
BUTTON_DRV_SITE = $(TOPDIR)/../button_drv
BUTTON_DRV_SITE_METHOD = local
BUTTON_DRV_INSTALL_IMAGES = YES

$(eval $(kernel-module))

define BUTTON_DRV_BUILD_DT_OVERLAY
	$(LINUX_DIR)/scripts/dtc/dtc -@ -I dts -O dtb \
		-o $(@D)/button_drv.dtbo $(@D)/button_drv.dts
endef
BUTTON_DRV_POST_BUILD_HOOKS += BUTTON_DRV_BUILD_DT_OVERLAY

define BUTTON_DRV_INSTALL_IMAGES_CMDS
	$(INSTALL) -D -m 0644 $(@D)/button_drv.dtbo \
		$(BINARIES_DIR)/rpi-firmware/overlays/button_drv.dtbo
endef

$(eval $(generic-package))

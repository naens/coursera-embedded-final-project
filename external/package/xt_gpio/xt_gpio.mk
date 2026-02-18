##############################################################
#
# xt_gpio
#
##############################################################

XT_GPIO_VERSION = 0.1
XT_GPIO_SITE = $(BR2_EXTERNAL_project_base_PATH)/../xt-gpio
XT_GPIO_SITE_METHOD = local
XT_GPIO_INSTALL_IMAGES = YES

$(eval $(kernel-module))

define XT_GPIO_BUILD_DT_OVERLAY
	$(TARGET_CC) -E -nostdinc -I$(LINUX_DIR)/scripts/dtc/include-prefixes \
		-undef -D__DTS__ -x assembler-with-cpp \
		-o $(@D)/.xt-gpio.dtbo.dts.tmp $(@D)/xt-gpio.dts
	$(LINUX_DIR)/scripts/dtc/dtc -@ -H epapr -O dtb \
		-o $(@D)/xt-gpio.dtbo $(@D)/.xt-gpio.dtbo.dts.tmp
endef
XT_GPIO_POST_BUILD_HOOKS += XT_GPIO_BUILD_DT_OVERLAY

define XT_GPIO_INSTALL_IMAGES_CMDS
	$(INSTALL) -D -m 0644 $(@D)/xt-gpio.dtbo \
		$(BINARIES_DIR)/rpi-firmware/overlays/xt-gpio.dtbo
endef

$(eval $(generic-package))

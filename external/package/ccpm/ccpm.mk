################################################################################
#
# ccp/m-86
#
################################################################################

CCPM_VERSION = '66836d2f20517d5c6c2cbb2d9c37761f0575734e'
CCPM_SITE = 'git@gitlab.com:ccpm-86/ccpm.git'
CCPM_SITE_METHOD = git


define CCPM_BUILD_CMDS
	$(MAKE) -C $(@D) all
endef

define CCPM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/image/floppy.img $(TARGET_DIR)/root/floppy.img
endef

$(eval $(generic-package))

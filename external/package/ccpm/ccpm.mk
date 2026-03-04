################################################################################
#
# ccp/m-86
#
################################################################################

CCPM_VERSION = '2915deb5ab4f9d3e128c791ad49ed32520cf2c4b'
CCPM_SITE = 'git@gitlab.com:ccpm-86/ccpm.git'
CCPM_SITE_METHOD = git


define CCPM_BUILD_CMDS
	$(MAKE) -C $(@D) all
endef

define CCPM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/image/floppy.img $(TARGET_DIR)/root/floppy.img
endef

$(eval $(generic-package))

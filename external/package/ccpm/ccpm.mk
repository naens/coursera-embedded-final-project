################################################################################
#
# ccp/m-86
#
################################################################################

CCPM_VERSION = 'c6c13967325329120adaf815683e34f978b12cbc'
CCPM_SITE = 'git@gitlab.com:ccpm-86/ccpm.git'
CCPM_SITE_METHOD = git


define CCPM_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

define CCPM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/image/floppy.img $(TARGET_DIR)/root/floppy0.img
endef

$(eval $(generic-package))

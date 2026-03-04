################################################################################
#
# 8086tiny
#
################################################################################

8086TINY_VERSION = '81bac1a056754545ec2c3e00205f099bb030c121'
8086TINY_SITE = 'git@github.com:naens/8086tiny.git'
8086TINY_SITE_METHOD = git


define 8086TINY_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define 8086TINY_INSTALL_TARGET_CMDS
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/root/8086tiny
	$(INSTALL) -D -m 0755 $(@D)/8086tiny $(TARGET_DIR)/root/8086tiny/
	$(INSTALL) -D -m 0755 $(@D)/bios.bin $(TARGET_DIR)/root/8086tiny/
	$(INSTALL) -D -m 0755 $(@D)/border.bin $(TARGET_DIR)/root
endef

$(eval $(generic-package))

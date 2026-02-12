################################################################################
#
# 8086tiny
#
################################################################################

8086TINY_VERSION = '9b19ccc23c16cc61e4495a05705af47b120338f0'
8086TINY_SITE = 'git@github.com:naens/8086tiny.git'
8086TINY_SITE_METHOD = git


define 8086TINY_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) no_graphics
endef

define 8086TINY_INSTALL_TARGET_CMDS
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/root/8086tiny
	$(INSTALL) -D -m 0755 $(@D)/8086tiny $(TARGET_DIR)/root/8086tiny/
	$(INSTALL) -D -m 0755 $(@D)/bios $(TARGET_DIR)/root/8086tiny/
	$(INSTALL) -D -m 0755 $(@D)/runme $(TARGET_DIR)/root/8086tiny/
	$(INSTALL) -D -m 0755 $(@D)/fd.img $(TARGET_DIR)/root/8086tiny/
endef

$(eval $(generic-package))

################################################################################
#
# kedei
#
################################################################################

KEDEI_VERSION = '968b9399f1fdcdedce1611bea53448dc5fbcad3f'
KEDEI_SITE = 'git@github.com:naens/FBTFT_KeDei_35_lcd_v62.git'
KEDEI_SITE_METHOD = git
#KEDEI_OVERRIDE_SRCDIR = ../kedei

KEDEI_MODULE_MAKE_OPTS=$(TARGET_CONFIGURE_OPTS)

$(eval $(kernel-module))
$(eval $(generic-package))

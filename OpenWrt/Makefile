#
# Copyright (C) 2016 Jean-Christophe Rona <jc@rona.fr>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=rf-ctrl
PKG_VERSION:=0.6
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/rf-ctrl
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=433 MHz OOK based devices control utility
  DEPENDS:=+libpthread +libusb-1.0 $(ICONV_DEPENDS)
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		USE_EXTERNAL_LIBICONV="true"
endef

define Package/rf-ctrl/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rf-ctrl $(1)/usr/bin/
endef

$(eval $(call BuildPackage,rf-ctrl))

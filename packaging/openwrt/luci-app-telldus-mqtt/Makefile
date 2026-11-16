#
# LuCI pages for telldus-mqtt: Telldus TellStick (433 MHz) to MQTT bridge.
#
# This is free software, licensed under the GNU Lesser General Public
# License version 2.1 (see LICENSE in the telldus-mqtt repository).
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for telldus-mqtt
LUCI_DESCRIPTION:=Status, sensors, switches and broker settings of telldus-mqtt.
LUCI_DEPENDS:=+luci-base +rpcd-mod-ucode +telldus-mqtt
LUCI_PKGARCH:=all

PKG_LICENSE:=LGPL-2.1

include ../../luci.mk

# call BuildPackage - OpenWrt buildroot signature

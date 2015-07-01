# Copyright 2014-present Facebook. All Rights Reserved.
SUMMARY = "Utilities"
DESCRIPTION = "Various utilities"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=eb723b61539feef013de476e68b5c50a"

SRC_URI = "file://ast-functions \
           file://us_console.sh \
           file://sol.sh \
           file://power_led.sh \
           file://post_led.sh \
           file://reset_usb.sh \
           file://setup-gpio.sh \
           file://setup_rov.sh \
           file://mdio.py \
           file://bcm5396.py \
           file://bcm5396_util.py \
           file://mount_data0.sh \
           file://eth0_mac_fixup.sh \
           file://wedge_power.sh \
           file://power-on.sh \
           file://wedge_us_mac.sh \
           file://setup_switch.py \
           file://create_vlan_intf \
           file://watch-fc.sh \
           file://fcswitcher.sh \
           file://rc.early \
           file://rc.local \
           file://src \
           file://COPYING \
          "

pkgdir = "utils"

S = "${WORKDIR}"

binfiles = "us_console.sh sol.sh power_led.sh post_led.sh \
  reset_usb.sh mdio.py setup_rov.sh wedge_power.sh wedge_us_mac.sh \
  bcm5396.py bcm5396_util.py setup_switch.py watch-fc.sh"

DEPENDS_append = "update-rc.d-native"

do_install() {
  dst="${D}/usr/local/fbpackages/${pkgdir}"
  install -d $dst
  install -m 644 ast-functions ${dst}/ast-functions
  localbindir="${D}/usr/local/bin"
  install -d ${localbindir}
  for f in ${binfiles}; do
      install -m 755 $f ${dst}/${f}
      ln -s ../fbpackages/${pkgdir}/${f} ${localbindir}/${f}
  done

  # common lib and include files
  install -d ${D}${includedir}/facebook
  install -m 0644 src/include/log.h ${D}${includedir}/facebook/log.h
  install -m 0644 src/include/i2c-dev.h ${D}${includedir}/facebook/i2c-dev.h

  # init
  #install -d ${D}${sysconfdir}/init.d
  #install -d ${D}${sysconfdir}/rcS.d
  # the script to mount /mnt/data
  #install -m 0755 ${WORKDIR}/mount_data0.sh ${D}${sysconfdir}/init.d/mount_data0.sh
  #update-rc.d -r ${D} mount_data0.sh start 03 S .
  #install -m 0755 ${WORKDIR}/rc.early ${D}${sysconfdir}/init.d/rc.early
  #update-rc.d -r ${D} rc.early start 04 S .
  #install -m 755 setup-gpio.sh ${D}${sysconfdir}/init.d/setup-gpio.sh
  #update-rc.d -r ${D} setup-gpio.sh start 59 S .
  # create VLAN intf automatically
 # install -d ${D}/${sysconfdir}/network/if-up.d
 # install -m 755 create_vlan_intf ${D}${sysconfdir}/network/if-up.d/create_vlan_intf
  # networking is done after rcS, any start level within rcS
  # for mac fixup should work
  #install -m 755 eth0_mac_fixup.sh ${D}${sysconfdir}/init.d/eth0_mac_fixup.sh
  #update-rc.d -r ${D} eth0_mac_fixup.sh start 70 S .
  #install -m 755 power-on.sh ${D}${sysconfdir}/init.d/power-on.sh
  #update-rc.d -r ${D} power-on.sh start 85 S .
  #install -m 755 fcswitcher.sh ${D}${sysconfdir}/init.d/fcswitcher.sh
  #update-rc.d -r ${D} fcswitcher.sh start 90 S .
  #install -m 0755 ${WORKDIR}/rc.local ${D}${sysconfdir}/init.d/rc.local
  #update-rc.d -r ${D} rc.local start 99 2 3 4 5 .
}

FILES_${PN} += "/usr/local ${sysconfdir}"

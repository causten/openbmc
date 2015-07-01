# Copyright 2014-present Facebook. All Rights Reserved.
SUMMARY = "Power and Boot"
DESCRIPTION = "The utilities to power on, off, and boot"
SECTION = "base"
PR = "r1"
LICENSE = "CLOSED"

SRC_URI = "file://power.c \
          file://Makefile \
          "

S = "${WORKDIR}"

binfiles = "power \
           "


pkgdir = "fan-ctrl"

do_install() {
#  dst="${D}/usr/local/fbpackages/${pkgdir}"
  bin="${D}/usr/local/bin"
#  install -d $dst
  install -d $bin
  for f in ${binfiles}; do
    install -m 755 $f ${bin}/$f
  done
}

FBPACKAGEDIR = "${prefix}/local/fbpackages"

FILES_${PN} = "${FBPACKAGEDIR}/fan_ctrl ${prefix}/local/bin ${sysconfdir} "

# Inhibit complaints about .debug directories for the fand binary:

INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INHIBIT_PACKAGE_STRIP = "1"

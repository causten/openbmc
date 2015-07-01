# Copyright 2014-present IBM. All Rights Reserved.
SUMMARY = "Flash Utility"
DESCRIPTION = "The utilities to flash PNOR."
SECTION = "base"
PR = "r1"
LICENSE = "CLOSED"

DEPENDS_append = "libwedge-eeprom update-rc.d-native"

SRC_URI = "file://ccan/endian/test/run.c \
           file://ccan/endian/test/compile_ok-constant.c \
           file://ccan/endian/endian.h \
           file://ccan/array_size/test/run.c \
           file://ccan/array_size/test/compile_fail-function-param.c \
           file://ccan/array_size/test/compile_fail.c \
           file://ccan/array_size/array_size.h \
           file://ccan/build_assert/test/compile_fail-expr.c \
           file://ccan/build_assert/test/run-BUILD_ASSERT_OR_ZERO.c \
           file://ccan/build_assert/test/compile_ok.c \
           file://ccan/build_assert/test/compile_fail.c \
           file://ccan/build_assert/build_assert.h \
           file://config.h \
           file://get_arch.sh \
           file://io.h \
           file://libflash/test/test-flash.c \
           file://libflash/test/Makefile \
           file://libflash/libflash-priv.h \
           file://libflash/libflash.c \
           file://libflash/libflash.h \
           file://libflash/ffs.h \
           file://libflash/libffs.h \
           file://libflash/libffs.c \
           file://Makefile \
           file://pflash.c \
           file://powerpc_io.c \
           file://progress.c \
           file://progress.h \
           file://sfc-ctrl.c \
           file://sfc-ctrl.h \
           file://arm_io.c \
           file://ast.h \
           file://ast-sf-ctrl.c \
           file://ccan/Makefile.inc \
           file://ccan/built-in.o \
           file://ccan/container_of/container_of.h \
           file://ccan/container_of/test/compile_fail-bad-type.c \
           file://ccan/container_of/test/compile_fail-types.c \
           file://ccan/container_of/test/compile_fail-var-types.c \
           file://ccan/container_of/test/run.c \
           file://ccan/check_type/check_type.h \
           file://ccan/check_type/test/compile_fail-check_type_unsigned.c \
           file://ccan/check_type/test/compile_fail-check_type.c \
           file://ccan/check_type/test/run.c \
           file://ccan/check_type/test/compile_fail-check_types_match.c \
           "

S = "${WORKDIR}"

binfiles = "pflash \
           "

pkgdir = "fan-ctrl"

do_install() {
  #dst="${D}/usr/local/fbpackages/${pkgdir}"
  bin="${D}/usr/local/bin"
  #install -d $dst
  install -d $bin
  for f in ${binfiles}; do
    install -m 755 $f ${bin}/$f
  done
#  install -m 755 setup-fan.sh ${D}${sysconfdir}/init.d/setup-fan.sh
#  update-rc.d -r ${D} setup-fan.sh start 91 S .
}

FBPACKAGEDIR = "${prefix}/local/fbpackages"

FILES_${PN} = "${FBPACKAGEDIR}/fan_ctrl ${prefix}/local/bin ${sysconfdir} "

# Inhibit complaints about .debug directories for the fand binary:

INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INHIBIT_PACKAGE_STRIP = "1"

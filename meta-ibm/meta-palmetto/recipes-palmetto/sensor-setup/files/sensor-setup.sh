#!/bin/sh
#
# Copyright 2014-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#

### BEGIN INIT INFO
# Provides:          sensor-setup
# Required-Start:    power-on
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description: Power on micro-server
### END INIT INFO

# Eventually, this will be used to configure the various (mostly
# i2c-based) sensors, once we have a kernel version that supports
# doing this more dynamically.
#
# For now, we're using it to install the lm75 and pmbus module so that it
# can detect the fourth temperature sensor, which is located
# on the uServer, which doesn't get power until power-on executes.
#
# Similarly, the pmbus sensor seems to have an easier time of
# detecting the NCP4200 buck converters after poweron.  This has not
# been carefully explored.

#modprobe lm75
#modprobe pmbus

# Enable the ADC inputs;  adc5 - adc9 should be connected to
# 1V, 1.03V, 5V, 3.3V, and 2.5V.

#echo 1 > /sys/devices/platform/ast_adc.0/adc5_en
#echo 1 > /sys/devices/platform/ast_adc.0/adc6_en
#echo 1 > /sys/devices/platform/ast_adc.0/adc7_en
#echo 1 > /sys/devices/platform/ast_adc.0/adc8_en
#echo 1 > /sys/devices/platform/ast_adc.0/adc9_en

#LPC
devmem 0x1e789080 32 0x00000500 #Enable LPC FWH cycles, Enable LPC to AHB bridge
#devmem 0x1e789088 32 0x30000C00 #64M PNOR
#devmem 0x1e78908C 32 0xFE0003FF
devmem 0x1e789088 32 0x30000E00 #32M PNOR
devmem 0x1e78908C 32 0xFE0001FF

#flash controller
devmem 0x1e630000 32 0x00000003
devmem 0x1e630004 32 0x00002404

#UART
devmem 0x1e6e2084 32 0x00fff0c0  #Enable UART1
devmem 0x1e783000 32 0x00000000 #Set Baud rate divisor -> 13 (Baud 115200)
devmem 0x1e783004 32 0x00000000 #Set Baud rate divisor -> 13 (Baud 115200)
devmem 0x1e783008 32 0x000000c1 #Disable Parity, 1 stop bit, 8 bits
devmem 0x1E78909C 32 0x00000000 #Set routing UART1 -> COM 1


#GPIO
devmem 0x1e6e2070 32 0x120CE406
devmem 0x1e6e2080 32 0xCB000000
devmem 0x1e6e2088 32 0x01C000FF
devmem 0x1e6e208c 32 0xC1C000FF
devmem 0x1e6e2090 32 0x003FA009

#devmem 0x1E780000 32 0x9F82FCE7
#devmem 0x1E780004 32 0x0370E677
#devmem 0x1E780020 32 0xCFC8F7FD
#devmem 0x1E780024 32 0xC738F202

devmem 0x1E780000 32 0x13008CE7
devmem 0x1E780004 32 0x0370E677
devmem 0x1E780020 32 0xDF48F7FF
devmem 0x1E780024 32 0xC738F202

#run this to setup addressing mode in flash controller
#pflash -t




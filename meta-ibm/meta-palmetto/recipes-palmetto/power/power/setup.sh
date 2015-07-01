watchdog_ctrl.sh off
pflash -t

#LPC
devmem 0x1e789080 32 0x00000500 #Enable LPC FWH cycles, Enable LPC to AHB bridge
#devmem 0x1e789088 32 0x30000C00 #64M#
#devmem 0x1e78908C 32 0xFE0003FF
devmem 0x1e789088 32 0x30000E00 #32M
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

devmem 0x1E780000 32 0x9F82FCE7
devmem 0x1E780004 32 0x0370E677
devmem 0x1E780020 32 0xCFC8F7FD
devmem 0x1E780024 32 0xC738F202
power --off
power --on
power --boot


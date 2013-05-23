#!/bin/sh

#generate a u-boot u-env.
list="0x80200000 kernel.bin
0x82000000 ds.elf
0x82800000 rs.elf
0x83000000 pm.elf
0x83800000 sched.elf
0x84000000 vfs.elf
0x84800000 memory.elf
0x85000000 log.elf
0x85800000 tty.elf
0x86000000 mfs.elf
0x86800000 vm.elf
0x87000000 pfs.elf
0x87800000 init.elf
0x88000000 cmdline.txt"

#
# PREFIX for loading file over tftp to allow hosting multiple
# version/devices.
NETBOOT_PREFIX=""
NETBOOT="no"
BOOT="mmcbootcmd"

while getopts "p:n?" c
do
        case "$c" in
        \?)
                echo "Usage: $0 [-p netboot_prefix] -n" >&2
                exit 1
        	;;
        n)
		# genrate netbooting uEnv.txt
                BOOT="netbootcmd"
                NETBOOT="yes"
		;;
        p)
                NETBOOT_PREFIX=$OPTARG
		;;
	esac
done

function fill_cmd(){
	#load == load method like fatload mmc 0:1
	#prefix is an optional directory containing the ending /
	load=$1
	prefix=$2
	export IFS=" " 
	echo $list | while true
	do
		if ! read -r mem addr
		then
			break
		fi
		#e.g. ; fatloat mmc 0:1 0x82000000 mydir/ds.elf
		echo -n "; $load $mem $prefix$addr"
	done
}


echo "# Set the command to be executed"
echo "uenvcmd=run $BOOT"
echo 
echo "# With cmdline/bootargs in cmdline.txt"
echo "mmcbootcmd=echo starting from MMC ; mmc part 0; mw.b 0x88000000 0 16384 $(fill_cmd "fatload mmc 0:1" "") ; dcache off ; icache off ; go 0x80200000"
echo 
echo "# Netbooting."
echo "serverip=192.168.12.10"
echo "ipaddr=192.168.12.62"
echo "usbnet_devaddr=e8:03:9a:24:f9:10"
echo "usbethaddr=e8:03:9a:24:f9:11"
echo "netbootcmd=echo starting from TFTP; usb start; mw.b 0x88000000 0 16384 $(fill_cmd "tftp" "$NETBOOT_PREFIX") ; dcache off ; icache off ; go 0x80200000"
exit 0

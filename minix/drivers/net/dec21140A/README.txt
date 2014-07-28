README file for the Dec21140A ethernet board driver as emulated by 
Microsoft VirtualPC 2007.

created August 2009, Nicolas Tittley (first.last@gmail)

LIMITATIONS:
------------

This driver supports only the Dec21140A as emulated by VPC2007. It is
untested in any other environment and will probably panic if you use it
outside VPC2007.

The driver supports bridged, nat and local network settings. See the
next section for a remark on seting up a nat environment.

Only one card can be used at a time, do not activate multiple network
cards in VPC2007, the driver will panic.

NOTE FOR USERS CONFIGURING VPC2007 TO USE NAT:

in /usr/etc/rc comment out the following three lines:

trap '' 2
intr -t 20 hostaddr -h
trap 2

VPC2007 does not play well with hostaddr and it will hang the boot process 
until you CTRL-C out of it. 

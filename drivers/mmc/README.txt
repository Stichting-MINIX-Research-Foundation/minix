# rm mmc 
mknod /dev/mmc b 26 0                                       
service up /usr/sbin/mmcblk -dev /dev/mmc              
fdisk /dev/mmc 
service down mmcblk


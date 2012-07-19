# rm mmc 
mknod /dev/mmc0   b 26 0
mknod /dev/mmc0p0 b 26 1
mknod /dev/mmc0p1 b 26 2
mknod /dev/mmc0p2 b 26 3
mknod /dev/mmc0p3 b 26 4

mknod /dev/mmc0p0s0 b 26 128
mknod /dev/mmc0p0s1 b 26 129
mknod /dev/mmc0p0s2 b 26 130
mknod /dev/mmc0p0s3 b 26 131

mknod /dev/mmc0p1s0 b 26 132
mknod /dev/mmc0p1s1 b 26 133
mknod /dev/mmc0p1s2 b 26 134
mknod /dev/mmc0p1s3 b 26 135

mknod /dev/mmc0p2s0 b 26 132
mknod /dev/mmc0p2s1 b 26 133
mknod /dev/mmc0p2s2 b 26 134
mknod /dev/mmc0p2s3 b 26 135

mknod /dev/mmc0p3s0 b 26 136
mknod /dev/mmc0p3s1 b 26 137
mknod /dev/mmc0p3s2 b 26 138
mknod /dev/mmc0p3s3 b 26 139

#for part in 0 1 
#
#for mmc in 0
#do
#	mknod /dev/mmc$mmc b 26 0
#	for part in 0 1 2 3
#	do
#	mknod /dev/mmc$mmc b 26 0
#	done
#done

#service -c up /usr/sbin/mmcblk -dev /dev/mmc              
#fdisk /dev/mmc 
#service down mmcblk


#!/bin/sh

. ./support.sh

# The following commented-out examples of how to run blocktest for certain
# driver and device pairs. The syntax of the calls is:
#
#   block_test <device> <parameters>
#
# <device> is the path to a device to run blocktest on. This may be a full
# disk, a partition, or a subpartition. If possible, give blocktest the whole
# disk; otherwise preferably the first partition with a size of slightly over
# 8GB (for ATA) (better yet: slightly over 128GB); even fewer tests can be done
# if you give it only a subpartition.
#
# <parameters> is a comma-separated list of parameters for blocktest. The
# following parameters are supported and in fact expected:
#
# rw (or) ro   Specifying "rw" will let blocktest write to the target
#              partition. This allows for a lot more tests, but keep in mind
#              that any data on the partition (and, if the driver misbehaves,
#              on other partitions and possibly other disks) WILL BE DESTROYED.
#              Use "ro" for read-only mediums, such as CD-ROMs.
# sector       Sector size, in bytes. This should be 512 for ATA devices, and
#              2048 for ATAPI devices. The default is 512.
# min_read     Minimum size of a read request. This must be at least 1 and at
#              most the sector size, and the sector size must be divisible by
#              it. A value other than the sector size allows blocktest to test
#              sub-sector reads.
# min_write    Minimum size of a write request. This must be at least 1 and at
#              most the sector size. Sub-sector write support is not common in
#              drivers, and therefore not yet well tested by blocktest. This
#              parameter is optional; if omitted, the sector size is used.
# element      Minimum size of a vector element within a larger I/O request.
#              This must be at least 1 and at most min_read, and min_read must
#              be divisible by this value. The idea is that several small
#              elements may add up to the minimum read size.
# max          Maximum size of any request. This should be a multiple of the
#              sector size. Blocktest will not test what happens when this
#              value is exceeded, but it will generate large requests up to
#              this value. For drivers that do not have a maximum request size,
#              simply use some large value (typically several megabytes).
#
# Before commenting out any entry, you MUST edit the device name for that
# entry, or you WILL risk losing arbitrary data. You may run multiple tests in
# parallel, on different devices. Note that at_wini has no maximum request
# size, so an arbitray size is used. Finally, a disclaimer: a buggy device
# driver may destroy any data it has access to, so use at your own risk.

# AT_WINI ATA TEST (for IDE disk devices)

#block_test /dev/c0d1 "rw,sector=512,min_read=512,element=2,max=16777216"

# AT_WINI ATAPI TEST (for IDE CD-ROM devices)

#block_test /dev/c0d2 "ro,sector=2048,min_read=2,element=2,max=16777216"

# AHCI ATA TEST (for SATA disk devices)

#block_test /dev/c2d0 "rw,sector=512,min_read=2,element=2,max=4194304"

# AHCI ATAPI TEST (for SATA CD-ROM devices)

#block_test /dev/c2d1 "ro,sector=2048,min_read=2,element=2,max=4194304"

# VND TEST (for configured vnode disk devices)

#block_test /dev/vnd0 "rw,min_read=1,min_write=1,element=1,max=16777216"

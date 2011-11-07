#!/bin/sh

# The blocktest driver expects the following parameters:
#
# device       Path to the device node to perform the test on. This may be a
#              full disk, a partition, or a subpartition. If possible, give
#              blocktest the whole disk; otherwise preferably the first
#              partition with a size of slightly over 8GB (for ATA) (better
#              yet: slightly over 128GB); even fewer tests can be done if you
#              give it only a subpartition.
# rw (or) ro   Specifying "rw" will let blocktest write to the target
#              partition. This allows for a lot more tests, but keep in mind
#              that any data on the partition (and, if the driver misbehaves,
#              on other partitions and possibly other disks) WILL BE DESTROYED.
#              Use "ro" for read-only mediums, such as CD-ROMs.
# sector       Sector size, in bytes. This should be 512 for ATA devices, and
#              2048 for ATAPI devices.
# min_read     Minimum size of a read request. This must be at least 2 and at
#              most the sector size, and the sector size must be divisible by
#              it. A value other than the sector size allows blocktest to test
#              sub-sector reads. Sub-sector writes are currently not supported
#              by any driver and hence not by blocktest, so there is no
#              matching "min_write" (yet).
# element      Minimum size of a vector element within a larger I/O request.
#              This must be at least 2 and at most min_read, and min_read must
#              be divisible by this value. The idea is that several small
#              elements may add up to the minimum read size.
# max          Maximum size of any request. This should be a multiple of the
#              sector size. Blocktest will not test what happens when this
#              value is exceeded, but it will generate large requests up to
#              this value. For drivers that do not have a maximum request size,
#              simply use some large value (typically several megabytes).

# The following are examples of how to configure blocktest for certain driver
# and device pairs. Before commenting out any entry, you MUST edit the "device"
# option for that entry, or you WILL risk losing arbitrary data. You may run
# multiple tests in parallel (on different devices), but you will then have to
# give them different labels. Note that at_wini has no maximum request size, so
# an arbitray size is used. Finally, a disclaimer: a buggy device driver may
# destroy any data it has access to, so use at your own risk.

# AT_WINI ATA TEST (for IDE disk devices)

#service up `pwd`/blocktest -script /etc/rs.single -args "device=/dev/c0d1,rw,sector=512,min_read=512,element=2,max=16777216" -config system.conf -label blocktest_0

# AT_WINI ATAPI TEST (for IDE CD-ROM devices)

#service up `pwd`/blocktest -script /etc/rs.single -args "device=/dev/c0d2,ro,sector=2048,min_read=2,element=2,max=16777216" -config system.conf -label blocktest_0

# AHCI ATA TEST (for SATA disk devices)

#service up `pwd`/blocktest -script /etc/rs.single -args "device=/dev/c2d0,rw,sector=512,min_read=2,element=2,max=4194304" -config system.conf -label blocktest_0

# AHCI ATAPI TEST (for SATA CD-ROM devices)

#service up `pwd`/blocktest -script /etc/rs.single -args "device=/dev/c2d1,ro,sector=2048,min_read=2,element=2,max=4194304" -config system.conf -label blocktest_0

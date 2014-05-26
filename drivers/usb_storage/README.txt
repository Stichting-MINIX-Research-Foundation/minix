-------------------------------------------------------------------------------
*                           INFORMATION:                                      *
-------------------------------------------------------------------------------
README file for "USB Mass Storage driver" that uses DDEkit and libblockdriver.

created march-april 2014, JPEmbedded (info@jpembedded.eu)

-------------------------------------------------------------------------------
*                           KNOWN LIMITATIONS:                                *
-------------------------------------------------------------------------------
-Hardcoded interface number for bulk-only reset.
-Hardcoded configuration number for simple enumeration.
-Call to ddekit_minix_create_msg_q in _ddekit_usb_thread uses base that
 overlaps that of blockdriver's (in mass_storage_task) so initialization
 must be done in fixed order.
-Some of DDEKit's functions are declared in source files as they are missing
 from headers.
-DDEKit has 'init' but no 'deinit' call, so memory is spilled.
-Hardcoded geometry.
-LUN always set to 0.
-SIGTERM handler uses exit instead of DDEkit semaphores.
-mass_storage.conf taken from dde-linux26-usb-drivers.
-Subpartitioning does not seem to work.
-Type ddekit_usb_dev is not defined in any header file but two variants of it
 should exist (client and server).
-Magic number in URB setup buffer assignment as there is no header for that
 (like usb_ch9.h for descriptors).
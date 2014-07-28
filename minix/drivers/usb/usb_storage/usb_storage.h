/*
 * Minix3 USB mass storage driver definitions
 */

#ifndef _USB_STORAGE_H_
#define _USB_STORAGE_H_

#include <minix/driver.h>		/* struct device */
#include <minix/type.h>			/* vir_bytes */
/* TODO: no header for ddekit_usb_dev */

#include "urb_helper.h"

/* Number of handled peripherals (USB devices) */
#define MAX_PERIPHS			(1)
/* Number of handled disks per driver */
#define MAX_DRIVES			(4)
/* 4 partitions per disk */
#define PART_PER_DISK			(4)
/* 4 sub partitions per partition */
#define SUBPART_PER_PART		(4)
/* 16 sub partitions per disk */
#define SUBPART_PER_DISK		(PART_PER_DISK * SUBPART_PER_PART)

/*---------------------------*
 *    declared types         *
 *---------------------------*/
/* Information on opened mass storage drives */
typedef struct mass_storage_drive {

	struct device disk;			/* disk device */
	struct device part[PART_PER_DISK];	/* partition devices */
	struct device subpart[SUBPART_PER_DISK];/* sub-partition devices */
	unsigned long open_ct;			/* opening counter */
	int drive_idx;				/* Index of this drive */
}
mass_storage_drive;

/* Information on attached peripherals (USB devices) */
typedef struct mass_storage_periph {

	mass_storage_drive drives[MAX_DRIVES];	/* Possible drive info */
	struct ddekit_usb_dev * dev;		/* DDEKit device handler */
	unsigned int interfaces;		/* Interfaces bitmap */
	urb_ep_config ep_in;			/* Bulk IN endpoint */
	urb_ep_config ep_out;			/* Bulk OUT endpoint */
}
mass_storage_periph;

/* Structure for the information on current state of driver */
typedef struct mass_storage_state {

	/* DDEKit device handlers */
	mass_storage_periph periph[MAX_PERIPHS];

	/* Currently used peripheral */
	mass_storage_periph * cur_periph;

	/* Currently used drive */
	mass_storage_drive * cur_drive;

	/* Currently used device (drive/partition/sub-partition) */
	struct device * cur_device;

	/* Driver instance */
	long instance;
}
mass_storage_state;

/* State of current IO vector array for transfer operations */
typedef struct iov_state {

	vir_bytes base_addr;		/* Address to read/write or grant ID */
	vir_bytes remaining_bytes;	/* How many bytes remain in vector */
	vir_bytes offset;		/* How many bytes were copied */
	unsigned int iov_idx;		/* Index of currently selected vector */
}
iov_state;

#endif /* !_USB_STORAGE_H_ */

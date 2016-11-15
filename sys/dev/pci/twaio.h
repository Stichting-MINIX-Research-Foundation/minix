/*	$NetBSD: twaio.h,v 1.6 2015/09/06 06:01:00 dholland Exp $ */
/*	$wasabi: twaio.h,v 1.8 2006/04/27 17:12:39 wrstuden Exp $ */

/*-
 * Copyright (c) 2003-04 3ware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *$FreeBSD: src/sys/dev/twa/twa_ioctl.h,v 1.1 2004/03/30 03:45:59 vkashyap Exp $
 */

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


#ifndef _DEV_PCI_TWAIO_H_
#define	_DEV_PCI_TWAIO_H_

#include <sys/ioccom.h>


#define TWA_AEN_NOT_RETRIEVED	0x1
#define TWA_AEN_RETRIEVED	0x2

#define TWA_ERROR_AEN_NO_EVENTS	0x1003           /* No more events */
#define TWA_ERROR_AEN_OVERFLOW	0x1004           /* AEN clobber occurred */

#define TWA_ERROR_IOCTL_LOCK_NOT_HELD		0x1001   /* Not locked */
#define TWA_ERROR_IOCTL_LOCK_ALREADY_HELD	0x1002   /* Already locked */


struct twa_scan_bus_packet {
	uint32_t	unit;
} __packed;

struct tw_cl_event_packet {
	uint32_t	sequence_id;
	uint32_t	time_stamp_sec;
	uint16_t	aen_code;
	uint8_t		severity;
	uint8_t		retrieved;
	uint8_t		repeat_count;
	uint8_t		parameter_len;
	uint8_t		parameter_data[98];
	uint32_t	event_src;
	uint8_t		severity_str[20];
} __packed;

struct tw_cl_lock_packet {
	uint32_t	timeout_msec;
	uint32_t	time_remaining_msec;
	uint32_t	force_flag;
} __packed;


struct tw_cl_compatibility_packet {
	uint8_t		driver_version[32];/* driver version */
	uint16_t	working_srl;	/* driver & firmware negotiated srl */
	uint16_t	working_branch;	/* branch # of the firmware that the driver is compatible with */
	uint16_t	working_build;	/* build # of the firmware that the driver is compatible with */
} __packed;


struct twa_driver_packet {
	uint32_t	control_code;
	uint32_t	status;
	uint32_t	unique_id;
	uint32_t	sequence_id;
	uint32_t	os_status;
	uint32_t	buffer_length;
} __packed;

/* Account for differences between 32/64 bit system. Offsets into memory
 * are anticipated for driver/firmware command packets and having a 
 * variable sized pointer depending on architecture add 4 bytes to any offset
 * after the pdata declaration
 */
#define TW_SIZEOF_VOIDPTR		(sizeof(void *)) 

struct twa_ioctl_9k {
	struct twa_driver_packet	twa_drvr_pkt;
	void				*pdata; /* points to data_buf */
	int8_t				padding[488 - TW_SIZEOF_VOIDPTR];
	struct twa_command_packet	twa_cmd_pkt;
	int8_t				data_buf[1];
} __packed;

typedef struct twa_ioctl_with_payload {
	struct twa_driver_packet	twa_drvr_pkt;
	int8_t				padding[488];
	struct twa_command_packet	twa_cmd_pkt;
	union {
		struct tw_cl_event_packet		event_pkt;
		struct tw_cl_lock_packet 		lock_pkt;
		struct tw_cl_compatibility_packet	compat_pkt;
		int8_t					data_buf[1];
	} payload;
}  __packed TWA_IOCTL_WITH_PAYLOAD;

/*
 * We need the structure below to ensure that the first byte of
 * data_buf is not overwritten by the kernel, after we return
 * from the ioctl call.  Note that twa_cmd_pkt has been reduced
 * to an array of 1024 bytes even though it's actually 2048 bytes
 * in size.  This is because, we don't expect requests from user
 * land requiring 2048 (273 sg elements) byte cmd pkts.
 */
typedef struct twa_ioctl_no_data_buf {
	struct twa_driver_packet	twa_drvr_pkt;
	void				*pdata; /* points to data_buf */
	int8_t				padding[488 - TW_SIZEOF_VOIDPTR];
	struct twa_command_packet	twa_cmd_pkt;
}  __packed TWA_IOCTL_NO_DATA_BUF;


/*
 * Get the device external name of the specified array unit.
 */
 /* WASABI */
struct twa_unitname {
	int		tn_unit;
	char		tn_name[16];	/* XXX sizeof(dev->dv_xname) */
};

#define TW_OSL_IOCTL_SCAN_BUS		_IO ('T', 200)

#define TW_OSL_IOCTL_FIRMWARE_PASS_THROUGH \
	_IOWR('T', 202, TWA_IOCTL_NO_DATA_BUF)

#define TW_CL_IOCTL_GET_FIRST_EVENT	_IOWR('T', 203, TWA_IOCTL_WITH_PAYLOAD)
#define TW_CL_IOCTL_GET_LAST_EVENT	_IOWR('T', 204, TWA_IOCTL_WITH_PAYLOAD)
#define TW_CL_IOCTL_GET_NEXT_EVENT	_IOWR('T', 205, TWA_IOCTL_WITH_PAYLOAD)
#define TW_CL_IOCTL_GET_PREVIOUS_EVENT	_IOWR('T', 206, TWA_IOCTL_WITH_PAYLOAD)
#define TW_CL_IOCTL_GET_LOCK		_IOWR('T', 207, TWA_IOCTL_WITH_PAYLOAD)
#define TW_CL_IOCTL_RELEASE_LOCK	_IOWR('T', 208, TWA_IOCTL_WITH_PAYLOAD)
#define TW_CL_IOCTL_GET_COMPATIBILITY_INFO 	\
	_IOWR('T', 209, TWA_IOCTL_WITH_PAYLOAD)
/* WASABI */
#define	TWA_IOCTL_GET_UNITNAME		_IOWR('T', 220, struct twa_unitname)

#endif /* _DEV_PCI_TWAIO_H_ */

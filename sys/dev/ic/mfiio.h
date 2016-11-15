/*-
 * Copyright (c) 2006 IronPort Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *	
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <dev/ic/mfireg.h>
#include <sys/ioctl.h>

/*
 * these structures are used for LSI's MegaCli on both FreeBSD and Linux
 * We will also use them in the NetBSD driver.
 */

#define MAX_SPACE_FOR_SENSE_PTR         32
union mfi_sense_ptr {
	uint8_t         sense_ptr_data[MAX_SPACE_FOR_SENSE_PTR];
	void            *user_space;
	struct {
		uint32_t        low;
		uint32_t        high; 
	} addr;
} __packed;


#define MAX_IOCTL_SGE	16

struct mfi_ioc_packet {
	uint16_t	mfi_adapter_no;
	uint16_t	mfi_pad1;	
	uint32_t	mfi_sgl_off;	
	uint32_t	mfi_sge_count;
	uint32_t	mfi_sense_off;
	uint32_t	mfi_sense_len;
	union {
		uint8_t raw[128];	
		struct mfi_frame_header hdr;
	} mfi_frame;

	struct iovec mfi_sgl[MAX_IOCTL_SGE];
} __packed;

struct mfi_ioc_aen {
	uint16_t	aen_adapter_no;
	uint16_t	aen_pad1;
	uint32_t	aen_seq_num;
	uint32_t	aen_class_locale;
} __packed;

#define MFI_CMD	 	_IOWR('M', 1, struct mfi_ioc_packet)
#define MFI_SET_AEN	_IOW('M', 3, struct mfi_ioc_aen)

#ifdef _LP64
struct iovec32 {
	u_int32_t	iov_base;
	int		iov_len;
};

struct mfi_ioc_packet32 {
	uint16_t	mfi_adapter_no;
	uint16_t	mfi_pad1;	
	uint32_t	mfi_sgl_off;	
	uint32_t	mfi_sge_count;
	uint32_t	mfi_sense_off;
	uint32_t	mfi_sense_len;
	union {
		uint8_t raw[128];	
		struct mfi_frame_header hdr;
	} mfi_frame;

	struct iovec32 mfi_sgl[MAX_IOCTL_SGE];
} __packed;
#define MFI_CMD32	_IOWR('M', 1, struct mfi_ioc_packet32)
#endif

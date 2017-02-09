/*	$NetBSD: mpt.h,v 1.8 2014/09/27 16:14:16 jmcneill Exp $	*/

/*
 * Copyright (c) 2000, 2001 by Greg Ansley
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

/*
 * mpt.h:
 *
 * Generic definitions for LSI Fusion adapters.
 *
 * Adapted from the FreeBSD "mpt" driver by Jason R. Thorpe for
 * Wasabi Systems, Inc.
 */

#ifndef _DEV_IC_MPT_H_
#define	_DEV_IC_MPT_H_

#include <dev/ic/mpt_netbsd.h>

#define MPT_OK (0)
#define MPT_FAIL (0x10000)

/* Register Offset to chip registers */
#define MPT_OFFSET_DOORBELL     0x00
#define MPT_OFFSET_SEQUENCE     0x04
#define MPT_OFFSET_DIAGNOSTIC   0x08
#define MPT_OFFSET_TEST         0x0C
#define MPT_OFFSET_INTR_STATUS  0x30
#define MPT_OFFSET_INTR_MASK    0x34
#define MPT_OFFSET_REQUEST_Q    0x40
#define MPT_OFFSET_REPLY_Q      0x44
#define MPT_OFFSET_HOST_INDEX   0x50
#define MPT_OFFSET_FUBAR        0x90

#define MPT_DIAG_SEQUENCE_1     0x04
#define MPT_DIAG_SEQUENCE_2     0x0b
#define MPT_DIAG_SEQUENCE_3     0x02
#define MPT_DIAG_SEQUENCE_4     0x07
#define MPT_DIAG_SEQUENCE_5     0x0d

/* Bit Maps for DOORBELL register */
enum DB_STATE_BITS {
	MPT_DB_STATE_RESET   =    0x00000000,
	MPT_DB_STATE_READY   =    0x10000000,
	MPT_DB_STATE_RUNNING =    0x20000000,
	MPT_DB_STATE_FAULT   =    0x40000000,
	MPT_DB_STATE_MASK    =    0xf0000000
};

#define MPT_STATE(v) ((enum DB_STATE_BITS)((v) & MPT_DB_STATE_MASK))

#define MPT_DB_LENGTH_SHIFT (16)
#define MPT_DB_DATA_MASK (0xffff)

#define MPT_DB_DB_USED            0x08000000
#define MPT_DB_IS_IN_USE(v) (((v) & MPT_DB_DB_USED) != 0)

/*
 * "Whom" initializor values
 */
#define	MPT_DB_INIT_NOONE       0x00
#define	MPT_DB_INIT_BIOS        0x01
#define	MPT_DB_INIT_ROMBIOS     0x02
#define	MPT_DB_INIT_PCIPEER     0x03
#define	MPT_DB_INIT_HOST        0x04
#define	MPT_DB_INIT_MANUFACTURE 0x05

#define MPT_WHO(v)	\
	((v & MPI_DOORBELL_WHO_INIT_MASK) >> MPI_DOORBELL_WHO_INIT_SHIFT)

/* Function Maps for DOORBELL register */
enum DB_FUNCTION_BITS {
	MPT_FUNC_IOC_RESET    =    0x40000000,
	MPT_FUNC_UNIT_RESET   =    0x41000000,
	MPT_FUNC_HANDSHAKE    =    0x42000000,
	MPT_FUNC_REPLY_REMOVE =    0x43000000,
	MPT_FUNC_MASK         =    0xff000000
};

/* Function Maps for INTERRUPT request register */
enum _MPT_INTR_REQ_BITS {
	MPT_INTR_DB_BUSY      =    0x80000000,
	MPT_INTR_REPLY_READY  =    0x00000008,
	MPT_INTR_DB_READY     =    0x00000001
};

#define MPT_DB_IS_BUSY(v) (((v) & MPT_INTR_DB_BUSY) != 0)
#define MPT_DB_INTR(v)    (((v) & MPT_INTR_DB_READY) != 0)
#define MPT_REPLY_INTR(v) (((v) & MPT_INTR_REPLY_READY) != 0)

/* Function Maps for INTERRUPT make register */
enum _MPT_INTR_MASK_BITS {
	MPT_INTR_REPLY_MASK   =    0x00000008,
	MPT_INTR_DB_MASK      =    0x00000001
};

/* Function Maps for DIAGNOSTIC make register */
enum _MPT_DIAG_BITS {
	MPT_DIAG_ENABLED      =    0x00000080,
	MPT_DIAG_FLASHBAD     =    0x00000040,
	MPT_DIAG_RESET_HIST   =    0x00000020,
	MPT_DIAG_TTLI         =    0x00000008,
	MPT_DIAG_RESET_IOC    =    0x00000004,
	MPT_DIAG_ARM_DISABLE  =    0x00000002,
	MPT_DIAG_DME          =    0x00000001
};

/* Magic addresses in diagnostic memory space */
#define MPT_DIAG_IOP_BASE        (0x00000000)
#define MPT_DIAG_IOP_SIZE                      (0x00002000)
#define MPT_DIAG_GPIO            (0x00030010)
#define MPT_DIAG_IOPQ_REG_BASE0  (0x00050004)
#define MPT_DIAG_IOPQ_REG_BASE1  (0x00051004)
#define MPT_DIAG_MEM_CFG_BASE    (0x00040000)
#define MPT_DIAG_CTX0_BASE       (0x000E0000)
#define MPT_DIAG_CTX0_SIZE                     (0x00002000)
#define MPT_DIAG_CTX1_BASE       (0x001E0000)
#define MPT_DIAG_CTX1_SIZE                     (0x00002000)
#define MPT_DIAG_FLASH_BASE      (0x00800000)
#define MPT_DIAG_RAM_BASE        (0x01000000)
#define MPT_DIAG_RAM_SIZE                      (0x00400000)

/* GPIO bit assignments */
#define MPT_DIAG_GPIO_SCL	(0x00010000)
#define MPT_DIAG_GPIO_SDA_OUT	(0x00008000)
#define MPT_DIAG_GPIO_SDA_IN	(0x00004000)

#define MPT_REPLY_EMPTY   (0xffffffff)    /* Reply Queue Empty Symbol */
#define MPT_CONTEXT_REPLY (0x80000000)
#define MPT_CONTEXT_MASK  (~0xE0000000)

#ifdef _KERNEL
int mpt_soft_reset(mpt_softc_t *);
void mpt_hard_reset(mpt_softc_t *);
int mpt_recv_handshake_reply(mpt_softc_t *, size_t, void *);

void mpt_send_cmd(mpt_softc_t *, request_t *);
void mpt_free_reply(mpt_softc_t *, u_int32_t);
void mpt_enable_ints(mpt_softc_t *);
void mpt_disable_ints(mpt_softc_t *);
u_int32_t mpt_pop_reply_queue(mpt_softc_t *);
int mpt_hw_init(mpt_softc_t *);
int mpt_init(mpt_softc_t *, u_int32_t);
int mpt_reset(mpt_softc_t *);
int mpt_send_handshake_cmd(mpt_softc_t *, size_t, void *);
request_t * mpt_get_request(mpt_softc_t *);
void mpt_free_request(mpt_softc_t *, request_t *);
int mpt_intr(void *);
void mpt_check_doorbell(mpt_softc_t *);

int mpt_read_cfg_header(mpt_softc_t *, int, int, int, fCONFIG_PAGE_HEADER *);
int mpt_read_cfg_page(mpt_softc_t *, int, fCONFIG_PAGE_HEADER *);
int mpt_write_cfg_page(mpt_softc_t *, int, fCONFIG_PAGE_HEADER *);

/* mpt_debug.c functions */
void mpt_print_reply(void *);
void mpt_print_db(u_int32_t);
void mpt_print_config_reply(void *);
char *mpt_ioc_diag(u_int32_t);
const char *mpt_req_state(enum mpt_req_state);
void mpt_print_scsi_io_request(MSG_SCSI_IO_REQUEST *);
void mpt_print_config_request(void *);
void mpt_print_request(void *);

/********************************** Endianess *********************************/
#define	MPT_2_HOST64(ptr, tag)	ptr->tag = le64toh(ptr->tag)
#define	MPT_2_HOST32(ptr, tag)	ptr->tag = le32toh(ptr->tag)
#define	MPT_2_HOST16(ptr, tag)	ptr->tag = le16toh(ptr->tag)

#define	HOST_2_MPT64(ptr, tag)	ptr->tag = htole64(ptr->tag)
#define	HOST_2_MPT32(ptr, tag)	ptr->tag = htole32(ptr->tag)
#define	HOST_2_MPT16(ptr, tag)	ptr->tag = htole16(ptr->tag)

#if	_BYTE_ORDER == _BIG_ENDIAN
void mpt2host_sge_simple_union(SGE_SIMPLE_UNION *);
void mpt2host_iocfacts_reply(MSG_IOC_FACTS_REPLY *);
void mpt2host_portfacts_reply(MSG_PORT_FACTS_REPLY *);
void mpt2host_config_page_scsi_port_0(fCONFIG_PAGE_SCSI_PORT_0 *);
void mpt2host_config_page_scsi_port_1(fCONFIG_PAGE_SCSI_PORT_1 *);
void host2mpt_config_page_scsi_port_1(fCONFIG_PAGE_SCSI_PORT_1 *);
void mpt2host_config_page_scsi_port_2(fCONFIG_PAGE_SCSI_PORT_2 *);
void mpt2host_config_page_scsi_device_0(fCONFIG_PAGE_SCSI_DEVICE_0 *);
void host2mpt_config_page_scsi_device_0(fCONFIG_PAGE_SCSI_DEVICE_0 *);
void mpt2host_config_page_scsi_device_1(fCONFIG_PAGE_SCSI_DEVICE_1 *);
void host2mpt_config_page_scsi_device_1(fCONFIG_PAGE_SCSI_DEVICE_1 *);
void mpt2host_config_page_fc_port_0(fCONFIG_PAGE_FC_PORT_0 *);
void mpt2host_config_page_fc_port_1(fCONFIG_PAGE_FC_PORT_1 *);
void host2mpt_config_page_fc_port_1(fCONFIG_PAGE_FC_PORT_1 *);
void mpt2host_config_page_raid_vol_0(fCONFIG_PAGE_RAID_VOL_0 *);
void mpt2host_config_page_raid_phys_disk_0(fCONFIG_PAGE_RAID_PHYS_DISK_0 *);
void mpt2host_config_page_raid_phys_disk_0(fCONFIG_PAGE_RAID_PHYS_DISK_0 *);
void mpt2host_config_page_ioc_2(fCONFIG_PAGE_IOC_2 *);
#else
#define	mpt2host_sge_simple_union(x)		do { ; } while (0)
#define	mpt2host_iocfacts_reply(x)		do { ; } while (0)
#define	mpt2host_portfacts_reply(x)		do { ; } while (0)
#define	mpt2host_config_page_scsi_port_0(x)	do { ; } while (0)
#define	host2mpt_config_page_scsi_device_0(x)	do { ; } while (0)
#define	mpt2host_config_page_scsi_port_1(x)	do { ; } while (0)
#define	host2mpt_config_page_scsi_port_1(x)	do { ; } while (0)
#define	mpt2host_config_page_scsi_port_2(x)	do { ; } while (0)
#define	mpt2host_config_page_scsi_device_0(x)	do { ; } while (0)
#define	mpt2host_config_page_scsi_device_1(x)	do { ; } while (0)
#define	host2mpt_config_page_scsi_device_1(x)	do { ; } while (0)
#define	mpt2host_config_page_fc_port_0(x)	do { ; } while (0)
#define	mpt2host_config_page_fc_port_1(x)	do { ; } while (0)
#define	host2mpt_config_page_fc_port_1(x)	do { ; } while (0)
#define	mpt2host_config_page_raid_vol_0(x)	do { ; } while (0)
#define	mpt2host_config_page_raid_phys_disk_0(x)			\
	do { ; } while (0)
#define mpt2host_config_page_ioc_2(x)		do { ; } while (0)
#endif

#endif /* _KERNEL */

#endif /* _DEV_IC_MPT_H_ */

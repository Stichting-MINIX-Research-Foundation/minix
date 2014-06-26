/*
 * SCSI commands related definitions
 */

#ifndef _SCSI_H_
#define _SCSI_H_

#if 0
#include <sys/endian.h>				/* be16dec... */
#else
#define be16enc(base, val) {						\
		(base)[0] = (((val) >> 8) & 0xff);			\
		(base)[1] = ((val) & 0xff);				\
		}
#define be16dec(base)							\
		(((base)[0] << 8) | (base)[1])
#define be32enc(base, val) {						\
		(base)[0] = (((val) >> 24) & 0xff);			\
		(base)[1] = (((val) >> 16) & 0xff);			\
		(base)[2] = (((val) >>  8) & 0xff);			\
		(base)[3] = ((val) & 0xff);				\
		}
#define be32dec(base)							\
		(((base)[0] << 24) | ((base)[1] << 16) |		\
		((base)[2] << 8) | (base)[3])
#endif

#include "bulk.h"

#define SCSI_INQUIRY				(0x12)
#define SCSI_MODE_SENSE				(0x5A)
#define SCSI_READ				(0x28)
#define SCSI_READ_CAPACITY			(0x25)
#define SCSI_REQUEST_SENSE			(0x03)
#define SCSI_TEST_UNIT_READY			(0x00)
#define SCSI_WRITE				(0x2A)

#define SCSI_INQUIRY_DATA_LEN			(36)
#define SCSI_INQUIRY_CMD_LEN			(6)

#define SCSI_MODE_SENSE_FLEX_DATA_LEN		(32)
#define SCSI_MODE_SENSE_CMD_LEN			(12)

#define SCSI_READ_DATA_LEN			(0)
#define SCSI_READ_CMD_LEN			(10)

#define SCSI_READ_CAPACITY_DATA_LEN		(8)
#define SCSI_READ_CAPACITY_CMD_LEN		(10)

#define SCSI_REQUEST_SENSE_DATA_LEN		(18)
#define SCSI_REQUEST_SENSE_CMD_LEN		(6)

#define SCSI_TEST_DATA_LEN			(0)
#define SCSI_TEST_CMD_LEN			(6)

#define SCSI_WRITE_DATA_LEN			(0)
#define SCSI_WRITE_CMD_LEN			(10)

/* These macros are immune to unaligned access
 * so they can be used on any address */
/* 1 Byte SCSI operation */
#define SCSI_WR1(base, offset, value)\
		(((uint8_t*)(base))[offset] = value)
#define SCSI_RD1(base, offset)\
		(((uint8_t*)(base))[offset])
#define SCSI_SET1(base, offset, value)\
		(((uint8_t*)(base))[offset] |= value)
/* 2 Byte SCSI operation */
#define SCSI_WR2(base, offset, value)\
		be16enc( &(((uint8_t*)(base))[offset]), value )
#define SCSI_RD2(base, offset)\
		be16dec( &(((uint8_t*)(base))[offset]) )
/* 4 Byte SCSI operation */
#define SCSI_WR4(base, offset, value)\
		be32enc( &(((uint8_t*)(base))[offset]), value )
#define SCSI_RD4(base, offset)\
		be32dec( &(((uint8_t*)(base))[offset]) )

#define SCSI_SET_INQUIRY_OP_CODE(x)		SCSI_WR1((x), 0, SCSI_INQUIRY)
#define SCSI_SET_INQUIRY_EVPD(x)		SCSI_SET1((x), 1, 0x01)
#define SCSI_SET_INQUIRY_CMDDT(x)		SCSI_SET1((x), 1, 0x02)
#define SCSI_SET_INQUIRY_PAGE_CODE(x, code)	SCSI_WR1((x), 2, code)
#define SCSI_SET_INQUIRY_ALLOC_LEN(x, len)	SCSI_WR1((x), 4, len)

#define SCSI_GET_INQUIRY_PERIPH_QUALIF(x)	((SCSI_RD1(x, 0) >> 5) & 0x7)
#define SCSI_GET_INQUIRY_VENDOR_NAME(x)		((const char *)(&((x)[8])))
#define SCSI_INQUIRY_VENDOR_NAME_LEN		(8)
#define SCSI_GET_INQUIRY_PRODUCT_NAME(x)	((const char *)(&((x)[16])))
#define SCSI_INQUIRY_PRODUCT_NAME_LEN		(16)

#define SCSI_MODE_SENSE_FLEXIBLE_DISK_PAGE	(0x5)
#define SCSI_SET_MODE_SENSE_OP_CODE(x)		SCSI_WR1((x), 0, \
							SCSI_MODE_SENSE)
#define SCSI_SET_MODE_SENSE_PAGE_CODE(x, code)	SCSI_SET1((x), 2, \
							(code)&0x3F)
#define SCSI_GET_MODE_SENSE_CYLINDERS(x)	SCSI_RD2((x), 8)
#define SCSI_GET_MODE_SENSE_HEADS(x)		SCSI_RD1((x), 4)
#define SCSI_GET_MODE_SENSE_SECTORS(x)		SCSI_RD1((x), 5)

#define SCSI_SET_READ_OP_CODE(x)		SCSI_WR1((x), 0, SCSI_READ)
#define SCSI_SET_READ_LBA(x, lba)		SCSI_WR4((x), 2, (lba))
#define SCSI_SET_READ_BLEN(x, len)		SCSI_WR2((x), 7, (len))

#define SCSI_SET_READ_CAPACITY_OP_CODE(x)	SCSI_WR1((x), 0, \
							SCSI_READ_CAPACITY)
#define SCSI_SET_READ_CAPACITY_LBA(x, lba)	SCSI_WR4((x), 2, (lba))
#define SCSI_SET_READ_CAPACITY_PMI(x)		SCSI_SET1((x), 8, 0x01)
#define SCSI_GET_READ_CAPACITY_LBA(x)		SCSI_RD4((x), 0)
#define SCSI_GET_READ_CAPACITY_BLEN(x)		SCSI_RD4((x), 4)

#define SCSI_SET_REQUEST_SENSE_OP_CODE(x)	SCSI_WR1((x), 0, \
							SCSI_REQUEST_SENSE)
#define SCSI_SET_REQUEST_SENSE_ALLOC(x, alloc)	SCSI_WR1((x), 4, (alloc))

#define SCSI_SET_TEST_OP_CODE(x)		SCSI_WR1((x), 0, \
							SCSI_TEST_UNIT_READY)

#define SCSI_SET_WRITE_OP_CODE(x)		SCSI_WR1((x), 0, SCSI_WRITE)
#define SCSI_SET_WRITE_LBA(x, lba)		SCSI_WR4((x), 2, (lba))
#define SCSI_SET_WRITE_BLEN(x, len)		SCSI_WR2((x), 7, (len))

typedef struct scsi_transfer {

	unsigned int lba;			/* logical block address */
	unsigned int length;			/* transfer length */
}
scsi_transfer;

/*---------------------------*
 *    declared functions     *
 *---------------------------*/
int create_scsi_cmd(mass_storage_cbw *, int, struct scsi_transfer *);
int check_inquiry_reply(uint8_t *);
int check_read_capacity_reply(uint8_t *, uint32_t *, uint32_t *);
int check_mode_sense_reply(uint8_t *, unsigned *, unsigned *, unsigned *);
int check_csw(mass_storage_csw *, unsigned int);

#endif /* !_SCSI_H_ */

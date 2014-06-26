/*
 * SCSI commands related implementation
 */

#include <minix/blockdriver.h>				/* SECTOR_SIZE */

#include <assert.h>
#include <string.h>					/* strncpy */

#include "common.h"
#include "scsi.h"

/*---------------------------*
 *    declared functions     *
 *---------------------------*/
/* To work correctly cbw->CBWCB must be zeroed before calling these: */
static int create_inquiry_scsi_cmd(mass_storage_cbw *);
static int create_test_scsi_cmd(mass_storage_cbw *);
static int create_read_capacity_scsi_cmd(mass_storage_cbw *);
static int create_write_scsi_cmd(mass_storage_cbw *, scsi_transfer *);
static int create_read_scsi_cmd(mass_storage_cbw *, scsi_transfer *);
static int create_mode_sense_scsi_cmd(mass_storage_cbw *);
static int create_request_sense_scsi_cmd(mass_storage_cbw *);

/*---------------------------*
 *    defined functions      *
 *---------------------------*/
/*===========================================================================*
 *    create_scsi_cmd                                                        *
 *===========================================================================*/
int
create_scsi_cmd(mass_storage_cbw * cbw, int cmd, scsi_transfer * info)
{
	MASS_DEBUG_DUMP;

	assert(NULL != cbw);

	switch (cmd) {
		case SCSI_INQUIRY:
			return create_inquiry_scsi_cmd(cbw);
		case SCSI_TEST_UNIT_READY:
			return create_test_scsi_cmd(cbw);
		case SCSI_READ_CAPACITY:
			return create_read_capacity_scsi_cmd(cbw);
		case SCSI_WRITE:
			return create_write_scsi_cmd(cbw, info);
		case SCSI_READ:
			return create_read_scsi_cmd(cbw, info);
		case SCSI_MODE_SENSE:
			return create_mode_sense_scsi_cmd(cbw);
		case SCSI_REQUEST_SENSE:
			return create_request_sense_scsi_cmd(cbw);
		default:
			MASS_MSG("Invalid SCSI command!");
			return EXIT_FAILURE;
	}
}


/*===========================================================================*
 *    create_inquiry_scsi_cmd                                                *
 *===========================================================================*/
static int
create_inquiry_scsi_cmd(mass_storage_cbw * cbw)
{
	MASS_DEBUG_DUMP;

	cbw->dCBWDataTransferLength = SCSI_INQUIRY_DATA_LEN;
	cbw->bCBWFlags = CBW_FLAGS_IN;
	cbw->bCDBLength = SCSI_INQUIRY_CMD_LEN;

	SCSI_SET_INQUIRY_OP_CODE(cbw->CBWCB);
	SCSI_SET_INQUIRY_PAGE_CODE(cbw->CBWCB, 0);
	SCSI_SET_INQUIRY_ALLOC_LEN(cbw->CBWCB, SCSI_INQUIRY_DATA_LEN);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    create_test_scsi_cmd                                                   *
 *===========================================================================*/
static int
create_test_scsi_cmd(mass_storage_cbw * cbw)
{
	MASS_DEBUG_DUMP;

	cbw->bCDBLength = SCSI_TEST_CMD_LEN;

	SCSI_SET_TEST_OP_CODE(cbw->CBWCB);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    create_read_capacity_scsi_cmd                                          *
 *===========================================================================*/
static int
create_read_capacity_scsi_cmd(mass_storage_cbw * cbw)
{
	MASS_DEBUG_DUMP;

	cbw->dCBWDataTransferLength = SCSI_READ_CAPACITY_DATA_LEN;
	cbw->bCBWFlags = CBW_FLAGS_IN;
	cbw->bCDBLength = SCSI_READ_CAPACITY_CMD_LEN;

	SCSI_SET_READ_CAPACITY_OP_CODE(cbw->CBWCB);
	SCSI_SET_READ_CAPACITY_LBA(cbw->CBWCB, 0x00);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    create_write_scsi_cmd                                                  *
 *===========================================================================*/
static int
create_write_scsi_cmd(mass_storage_cbw * cbw, scsi_transfer * info)
{
	MASS_DEBUG_DUMP;

	assert(NULL != info);
	assert(0 == (info->length % SECTOR_SIZE));

	cbw->dCBWDataTransferLength = info->length;
	cbw->bCBWFlags = CBW_FLAGS_OUT;
	cbw->bCDBLength = SCSI_WRITE_CMD_LEN;

	SCSI_SET_WRITE_OP_CODE(cbw->CBWCB);
	SCSI_SET_WRITE_LBA(cbw->CBWCB, info->lba);
	SCSI_SET_WRITE_BLEN(cbw->CBWCB, info->length / SECTOR_SIZE);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    create_read_scsi_cmd                                                   *
 *===========================================================================*/
static int
create_read_scsi_cmd(mass_storage_cbw * cbw, scsi_transfer * info)
{
	MASS_DEBUG_DUMP;

	assert(NULL != info);
	assert(0 == (info->length % SECTOR_SIZE));

	cbw->dCBWDataTransferLength = info->length;
	cbw->bCBWFlags = CBW_FLAGS_IN;
	cbw->bCDBLength = SCSI_READ_CMD_LEN;

	SCSI_SET_READ_OP_CODE(cbw->CBWCB);
	SCSI_SET_READ_LBA(cbw->CBWCB, info->lba);
	SCSI_SET_READ_BLEN(cbw->CBWCB, info->length / SECTOR_SIZE);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    create_mode_sense_scsi_cmd                                             *
 *===========================================================================*/
static int
create_mode_sense_scsi_cmd(mass_storage_cbw * cbw)
{
	MASS_DEBUG_DUMP;

	cbw->dCBWDataTransferLength = SCSI_MODE_SENSE_FLEX_DATA_LEN;
	cbw->bCBWFlags = CBW_FLAGS_IN;
	cbw->bCDBLength = SCSI_MODE_SENSE_CMD_LEN;

	SCSI_SET_MODE_SENSE_OP_CODE(cbw->CBWCB);
	SCSI_SET_MODE_SENSE_PAGE_CODE(cbw->CBWCB,
					SCSI_MODE_SENSE_FLEXIBLE_DISK_PAGE);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    create_request_sense_scsi_cmd                                          *
 *===========================================================================*/
static int
create_request_sense_scsi_cmd(mass_storage_cbw * cbw)
{
	MASS_DEBUG_DUMP;

	cbw->dCBWDataTransferLength = SCSI_REQUEST_SENSE_DATA_LEN;
	cbw->bCBWFlags = CBW_FLAGS_IN;
	cbw->bCDBLength = SCSI_REQUEST_SENSE_CMD_LEN;

	SCSI_SET_REQUEST_SENSE_OP_CODE(cbw->CBWCB);
	SCSI_SET_REQUEST_SENSE_ALLOC(cbw->CBWCB, SCSI_REQUEST_SENSE_DATA_LEN);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    check_inquiry_reply                                                    *
 *===========================================================================*/
int
check_inquiry_reply(uint8_t * scsi_reply)
{
	char vendor_name[SCSI_INQUIRY_VENDOR_NAME_LEN + 1];
	char product_name[SCSI_INQUIRY_PRODUCT_NAME_LEN + 1];

	MASS_DEBUG_DUMP;

	/* Stop condition for printing as strncpy() does not add it */
	vendor_name[SCSI_INQUIRY_VENDOR_NAME_LEN] = '\0';
	product_name[SCSI_INQUIRY_PRODUCT_NAME_LEN] = '\0';

	if (SCSI_GET_INQUIRY_PERIPH_QUALIF(scsi_reply)) {
		MASS_MSG("Device not connected");
		return EXIT_FAILURE;
	}

	strncpy(vendor_name, SCSI_GET_INQUIRY_VENDOR_NAME(scsi_reply),
		SCSI_INQUIRY_VENDOR_NAME_LEN);
	strncpy(product_name, SCSI_GET_INQUIRY_PRODUCT_NAME(scsi_reply),
		SCSI_INQUIRY_PRODUCT_NAME_LEN);

	MASS_DEBUG_MSG("Vendor name: %s", vendor_name);
	MASS_DEBUG_MSG("Product name %s", product_name);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    check_read_capacity_reply                                              *
 *===========================================================================*/
int
check_read_capacity_reply(uint8_t * scsi_reply, uint32_t * lba, uint32_t * blen)
{
	MASS_DEBUG_DUMP;

	*lba = SCSI_GET_READ_CAPACITY_LBA(scsi_reply);
	*blen = SCSI_GET_READ_CAPACITY_BLEN(scsi_reply);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    check_mode_sense_reply                                                 *
 *===========================================================================*/
int
check_mode_sense_reply(uint8_t * scsi_reply, unsigned * cyl,
			unsigned * head, unsigned * sect)
{
	MASS_DEBUG_DUMP;

	*cyl = SCSI_GET_MODE_SENSE_CYLINDERS(scsi_reply);
	*head = SCSI_GET_MODE_SENSE_HEADS(scsi_reply);
	*sect = SCSI_GET_MODE_SENSE_SECTORS(scsi_reply);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    check_csw                                                              *
 *===========================================================================*/
int
check_csw(mass_storage_csw * csw, unsigned int tag)
{
	MASS_DEBUG_DUMP;

	if (csw->dCSWTag != tag) {
		MASS_MSG("CSW tag mismatch!");
		return EXIT_FAILURE;
	}

	if (CSW_SIGNATURE != csw->dCSWSignature) {
		MASS_MSG("CSW signature mismatch!");
		return EXIT_FAILURE;
	}

	if (CSW_STATUS_GOOD != csw->bCSWStatus) {
		MASS_MSG("CSW status error (0x%02X)!", csw->bCSWStatus);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

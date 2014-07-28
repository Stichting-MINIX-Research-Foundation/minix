/*
 * "Bulk only transfer" related externally visible info
 */

#ifndef _BULK_H_
#define _BULK_H_

/*---------------------------*
 *    declared types         *
 *---------------------------*/
#define BULK_PACKED __attribute__((__packed__))

#define CBW_SIGNATURE		(0x43425355)
#define CBW_FLAGS_OUT		(0x00)
#define CBW_FLAGS_IN		(0x80)
#define CBW_CB_LENGTH		(16)

/* Command Block Wrapper */
typedef struct BULK_PACKED mass_storage_cbw {
	uint32_t	dCBWSignature;
	uint32_t	dCBWTag;
	uint32_t	dCBWDataTransferLength;
	uint8_t		bCBWFlags;
	uint8_t		bCBWLUN;		/* 4 bits */
	uint8_t		bCDBLength;		/* 5 bits */
	uint8_t		CBWCB[CBW_CB_LENGTH];
}
mass_storage_cbw;

#define CSW_SIGNATURE		(0x53425355)
#define CSW_STATUS_GOOD		(0x0)
#define CSWS_TATUS_FAILED	(0x1)
#define CSW_STATUS_PHASE	(0x2)

/* Command Status Wrapper */
typedef struct BULK_PACKED mass_storage_csw {
	uint32_t	dCSWSignature;
	uint32_t	dCSWTag;
	uint32_t	dCSWDataResidue;
	uint8_t		bCSWStatus;
}
mass_storage_csw;

#undef BULK_PACKED

/*---------------------------*
 *    declared functions     *
 *---------------------------*/
void init_cbw(mass_storage_cbw *, unsigned int);
void init_csw(mass_storage_csw *);

#endif /* !_BULK_H_ */

/*
 * "Bulk only transfer" related implementation
 */

#include <assert.h>
#include <string.h>			/* memset */

#include "common.h"
#include "bulk.h"

/*===========================================================================*
 *    init_cbw                                                               *
 *===========================================================================*/
void
init_cbw(mass_storage_cbw * cbw, unsigned int tag)
{
	assert(NULL != cbw);

	/* Clearing "Command Block Wrapper" */
	memset(cbw, 0, sizeof(*cbw));

	/* Filling Command Block Wrapper */
	cbw->dCBWSignature = CBW_SIGNATURE;
	cbw->dCBWTag = tag;
	cbw->bCBWLUN = 0;
}


/*===========================================================================*
 *    init_csw                                                               *
 *===========================================================================*/
void
init_csw(mass_storage_csw * csw)
{
	assert(NULL != csw);

	/* Clearing "Command Status Wrapper" so we can receive data into it */
	memset(csw, 0, sizeof(*csw));
}

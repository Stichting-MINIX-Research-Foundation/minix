/*
 * hermes.c
 *
 * This file contains the lower level access functions for Prism based 
 * wireless cards. The file is based on hermes.c of the Linux kernel
 *
 * Adjusted to Minix by Stevens Le Blond <slblond@few.vu.nl> 
 *		    and Michael Valkering <mjvalker@cs.vu.nl>
 */

/* Original copyright notices from Linux hermes.c
 * 
 * Copyright (C) 2000, David Gibson, Linuxcare Australia 
 * <hermes@gibson.dropbear.id.au>
 * Copyright (C) 2001, David Gibson, IBM <hermes@gibson.dropbear.id.au>
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.
 */

#include "hermes.h"

/*****************************************************************************
 *            milli_delay                                                    *
 *                                                                           *
 * Wait msecs milli seconds                                                  *
 *****************************************************************************/
static void milli_delay(unsigned int msecs)
{
	micro_delay((long)msecs * 1000);
}


/*****************************************************************************
 *            hermes_issue_cmd                                               *
 *                                                                           *
 * Issue a command to the chip. Waiting for it to complete is the caller's   *
 * problem. The only thing we have to do first is to see whether we can      *
 * actually write something in the CMD register: is it unbusy?               *
 * Returns -EBUSY if the command register is busy, 0 on success.             *
 *****************************************************************************/
static int hermes_issue_cmd (hermes_t * hw, u16_t cmd, u16_t param0) {
	int k = HERMES_CMD_BUSY_TIMEOUT;
	u16_t reg;

	/* First wait for the command register to unbusy */
	reg = hermes_read_reg (hw, HERMES_CMD);
	while ((reg & HERMES_CMD_BUSY) && k) {
		k--;
		micro_delay (1);
		reg = hermes_read_reg (hw, HERMES_CMD);
	}
	/* it takes too long. Bailing out */
	if (reg & HERMES_CMD_BUSY) {
		printf("Hermes: HERMES_CMD_BUSY timeout\n");
		return -EBUSY;
	}

	/* write the values to the right registers */
	hermes_write_reg (hw, HERMES_PARAM2, 0);
	hermes_write_reg (hw, HERMES_PARAM1, 0);
	hermes_write_reg (hw, HERMES_PARAM0, param0);
	hermes_write_reg (hw, HERMES_CMD, cmd);
	return 0;
}

/*****************************************************************************
 *            hermes_struct_init                                             *
 *                                                                           *
 * Initialize the hermes structure fields                                    *
 *****************************************************************************/
void hermes_struct_init (hermes_t * hw, u32_t address,
		    int io_space, int reg_spacing) {
	hw->iobase = address;
	hw->io_space = io_space;
	hw->reg_spacing = reg_spacing;
	hw->inten = 0x0;
}


/*****************************************************************************
 *            hermes_cor_reset                                               *
 *                                                                           *
 * This is the first step in initializing the card's firmware and hardware:  *
 * write HERMES_PCI_COR_MASK to the Configuration Option Register            *
 *****************************************************************************/
int hermes_cor_reset (hermes_t *hw) {
	int k;
	u16_t reg;

	/* Assert the reset until the card notice */
	hermes_write_reg (hw, HERMES_PCI_COR, HERMES_PCI_COR_MASK);

	milli_delay (HERMES_PCI_COR_ONT);

	/* Give time for the card to recover from this hard effort */
	hermes_write_reg (hw, HERMES_PCI_COR, 0x0000);

	milli_delay (HERMES_PCI_COR_OFFT);

	/* The card is ready when it's no longer busy */
	k = HERMES_PCI_COR_BUSYT;
	reg = hermes_read_reg (hw, HERMES_CMD);
	while (k && (reg & HERMES_CMD_BUSY)) {
		k--;
		milli_delay (1);
		reg = hermes_read_reg (hw, HERMES_CMD);
	}

	/* Did we timeout ? */
	if (reg & HERMES_CMD_BUSY) {
		printf ("Busy timeout after resetting the COR\n");
		return -1;
	}

	return (0);
}


/*****************************************************************************
 *            hermes_present                                                 *
 *                                                                           *
 * Check whether we have access to the card. Does the SWSUPPORT0 contain the *
 * value we put in it earlier?                                               *
 *****************************************************************************/
static int hermes_present (hermes_t * hw) {
	int i = hermes_read_reg (hw, HERMES_SWSUPPORT0) == HERMES_MAGIC;
	if (!i)
		printf("Hermes: Error, card not present?\n");
	return i;
}


/*****************************************************************************
 *            hermes_init                                                    *
 *                                                                           *
 * Initialize the card                                                       *
 *****************************************************************************/
int hermes_init (hermes_t * hw)
{
	u32_t status, reg, resp0;
	int err = 0;
	int k;

	/* We don't want to be interrupted while resetting the chipset. By 
	 * setting the control mask for hardware interrupt generation to 0,
	 * we won't be disturbed*/
	hw->inten = 0x0;
	hermes_write_reg (hw, HERMES_INTEN, 0);

	/* Acknowledge any pending events waiting for acknowledgement. We 
	 * assume there won't be any important to take care off */
	hermes_write_reg (hw, HERMES_EVACK, 0xffff);

	/* Normally it's a "can't happen" for the command register to
	 * be busy when we go to issue a command because we are
	 * serializing all commands.  However we want to have some
	 * chance of resetting the card even if it gets into a stupid
	 * state, so we actually wait to see if the command register
	 * will unbusy itself here. */
	k = HERMES_CMD_BUSY_TIMEOUT;
	reg = hermes_read_reg (hw, HERMES_CMD);
	while (k && (reg & HERMES_CMD_BUSY)) {
		if (reg == 0xffff) {
			/* Special case - the card has probably 
			 *  been removed, so don't wait for the 
			 *  timeout */
			printf("Hermes: Card removed?\n");
			return -ENODEV;
		}

		k--;
		micro_delay (1);
		reg = hermes_read_reg (hw, HERMES_CMD);
	}

	/* No need to explicitly handle the timeout - if we've timed
	 * out hermes_issue_cmd() will probably return -EBUSY below.
	 * But i check to be sure :-) */
	if (reg & HERMES_CMD_BUSY) {
		printf("Hermes: Timeout waiting for the CMD_BUSY to unset\n");
		return -EBUSY;
	}

	/* According to the documentation, EVSTAT may contain
	 * obsolete event occurrence information.  We have to acknowledge
	 * it by writing EVACK. */
	reg = hermes_read_reg (hw, HERMES_EVSTAT);
	hermes_write_reg (hw, HERMES_EVACK, reg);

	err = hermes_issue_cmd (hw, HERMES_CMD_INIT, 0);
	if (err){
		printf("Hermes: errornr: 0x%x issueing HERMES_CMD_INIT\n",
			 err);
		return err;
	}

	/* here we start waiting for the above command,CMD_INIT, to complete.
	 * Completion is noticeable when the HERMES_EV_CMD bit in the 
	 * HERMES_EVSTAT register is set to 1 */
	reg = hermes_read_reg (hw, HERMES_EVSTAT);
	k = HERMES_CMD_INIT_TIMEOUT;
	while ((!(reg & HERMES_EV_CMD)) && k) {
		k--;
		micro_delay (10);
		reg = hermes_read_reg (hw, HERMES_EVSTAT);
	}


	/* the software support register 0 (there are 3) is filled with a 
	 * magic number. With this one can test the availability of the card */
	hermes_write_reg (hw, HERMES_SWSUPPORT0, HERMES_MAGIC);

	if (!hermes_present (hw)) {
		printf("Hermes: Card not present?: got mag. nr.0x%x\n",
			 hermes_read_reg (hw, HERMES_SWSUPPORT0));
	}

	if (!(reg & HERMES_EV_CMD)) {
		printf("hermes @ %x: Timeout waiting for card to reset\n",
			hw->iobase);
		return -ETIMEDOUT;
	}

	status = hermes_read_reg (hw, HERMES_STATUS);
	resp0 = hermes_read_reg (hw, HERMES_RESP0);

	/* after having issued the command above, the completion set a bit in 
	 * the EVSTAT register. This has to be acknowledged, as follows */
	hermes_write_reg (hw, HERMES_EVACK, HERMES_EV_CMD);

	/* Was the status, the result of the issued command, ok? */
	/* The expression below should be zero. Non-zero means an error */
	if (status & HERMES_STATUS_RESULT) {
		printf("Hermes:Result of INIT_CMD wrong.error value: 0x%x\n",
			(status & HERMES_STATUS_RESULT) >> 8);
		err = -EIO;
	}

	return err;
}

/*****************************************************************************
 *            hermes_docmd_wait                                              *
 *                                                                           *
 * Issue a command to the chip, and (busy) wait for it to complete.          *
 *****************************************************************************/
int hermes_docmd_wait (hermes_t * hw, u16_t cmd, u16_t parm0,
		   hermes_response_t * resp) {
	int err;
	int k;
	u16_t reg;
	u16_t status;

	err = hermes_issue_cmd (hw, cmd, parm0);
	if (err) {
		printf("hermes @ %x: Error %d issuing command.\n",
			 hw->iobase, err);
		return err;
	}

	/* Reads the Event status register. When the command has completed,
	 * the fourth bit in the HERMES_EVSTAT register is a 1. We will be
	 * waiting for that to happen */
	reg = hermes_read_reg (hw, HERMES_EVSTAT);
	k = HERMES_CMD_COMPL_TIMEOUT;
	while ((!(reg & HERMES_EV_CMD)) && k) {
		k--;
		micro_delay (10);
		reg = hermes_read_reg (hw, HERMES_EVSTAT);
	}

	/* check for a timeout: has the command still not completed? */
	if (!(reg & HERMES_EV_CMD)) {
		printf("hermes @ %x: Timeout waiting for command \
		completion.\n", hw->iobase);
		err = -ETIMEDOUT;
		return err;
	}

	status = hermes_read_reg (hw, HERMES_STATUS);
	/* some commands result in results residing in response registers.
	 * They have to be read before the acknowledgement below.
	 */
	if (resp) {
		resp->status = status;
		resp->resp0 = hermes_read_reg (hw, HERMES_RESP0);
		resp->resp1 = hermes_read_reg (hw, HERMES_RESP1);
		resp->resp2 = hermes_read_reg (hw, HERMES_RESP2);
	}

	/* After issueing a Command, the card expects an Acknowledgement */
	hermes_write_reg (hw, HERMES_EVACK, HERMES_EV_CMD);

	/* check whether there has been a valid value in the Status register. 
	 * the high order bits should have at least some value */
	if (status & HERMES_STATUS_RESULT) {
		printf("Hermes: EIO\n");
		err = -EIO;
	}

	return err;
}


/*****************************************************************************
 *            hermes_allocate                                                *
 *                                                                           *
 * Allocate bufferspace in the card, which will be then available for        *
 * writing by the host, TX buffers. The card will try to find enough memory  *
 * (creating a list of 128 byte blocks) and will return a pointer to the     *
 * first block. This pointer is a pointer to the frame identifier (fid),     *
 * holding information and data of the buffer. The fid is like a file        *
 * descriptor, a value indicating some resource                              *
 *****************************************************************************/
int hermes_allocate (hermes_t * hw, u16_t size, u16_t * fid) {
	int err = 0;
	int k;
	u16_t reg;

	if ((size < HERMES_ALLOC_LEN_MIN) || (size > HERMES_ALLOC_LEN_MAX))	{
		printf("Hermes: Invalid size\n");
		return -EINVAL;
	}

	/* Issue a allocation request to the card, waiting for the command 
	 * to complete */
	err = hermes_docmd_wait (hw, HERMES_CMD_ALLOC, size, NULL);
	if (err) {
		printf( "Hermes: docmd_wait timeout\n");
		return err;
	}

	/* Read the status event register to know whether the allocation
	 * succeeded. The HERMES_EV_ALLOC bit should be set */
	reg = hermes_read_reg (hw, HERMES_EVSTAT);
	k = HERMES_ALLOC_COMPL_TIMEOUT;
	while ((!(reg & HERMES_EV_ALLOC)) && k) {
		k--;
		micro_delay (10);
		reg = hermes_read_reg (hw, HERMES_EVSTAT);
	}

	/* tired of waiting to complete. Abort. */
	if (!(reg & HERMES_EV_ALLOC)) {
		printf("hermes @ %x:Timeout waiting for frame allocation\n",
			 hw->iobase);
		return -ETIMEDOUT;
	}

	/* When we come here, everything has gone well. The pointer to the
	 * fid is in the ALLOCFID register. This fid is later on used
	 * to access this buffer */
	*fid = hermes_read_reg (hw, HERMES_ALLOCFID);

	/* always acknowledge the receipt of an event */
	hermes_write_reg (hw, HERMES_EVACK, HERMES_EV_ALLOC);

	return 0;
}



/*****************************************************************************
 *            hermes_bap_seek                                                *
 *                                                                           *
 * Set up a Buffer Access Path (BAP) to read a particular chunk of data      *
 * from card's internal buffer. Setting a bap register is like doing a fseek *
 * system call: setting an internal pointer to the right place in a buffer   *
 *****************************************************************************/
static int hermes_bap_seek (hermes_t * hw, int bap, u16_t id, u16_t offset) {

	/* There are 2 BAPs. This can be used to use the access buffers
	 * concurrently: 1 for writing in the TX buffer and 1 for reading
	 * a RX buffer in case of an RX interrupt.
	 * The BAP consists of 2 registers, together with which one can
	 * point to a single byte in the required buffer (additionally 
	 * there is a third register, but that one is not used in this 
	 * function, the data register). With the SELECT register one chooses 
	 * the fid, with the OFFSET register one chooses the offset in the fid 
	 * buffer */
	int sreg = bap ? HERMES_SELECT1 : HERMES_SELECT0;
	int oreg = bap ? HERMES_OFFSET1 : HERMES_OFFSET0;
	int k;
	u16_t reg;

	/* Check whether the offset is not too large, and whether it is a
	 * number of words. Offset can't be odd */
	if ((offset > HERMES_BAP_OFFSET_MAX) || (offset % 2)) {
		printf("Hermes: Offset error\n");
		return -EINVAL;
	}

	/* We can't write to the offset register when the busy flag is set. If 
	 * it is set, wait to automatically reset*/
	k = HERMES_BAP_BUSY_TIMEOUT;
	reg = hermes_read_reg (hw, oreg);
	while ((reg & HERMES_OFFSET_BUSY) && k)	{
		k--;
		micro_delay (1);
		reg = hermes_read_reg (hw, oreg);
	}

	/* For some reason, the busy flag didn't reset automatically. Return */
	if (reg & HERMES_OFFSET_BUSY) {
		printf("Hermes: HERMES_OFFSET_BUSY still set, oreg: 0x%x\n",
				 reg);
		return -ETIMEDOUT;
	}

	/* Now we actually set up the transfer. Write the fid in the select 
	 * register, and the offset in the offset register */
	hermes_write_reg (hw, sreg, id);
	hermes_write_reg (hw, oreg, offset);

	/* Wait for the BAP to be ready. This means that at first the 
	 * OFFSET_BUSY bit is set by the card once we have written the values 
	 * above. We wait until the card has done its internal processing and 
	 * unset the OFFSET_BUSY bit */
	k = HERMES_BAP_BUSY_TIMEOUT;
	reg = hermes_read_reg (hw, oreg);
	while ((reg & (HERMES_OFFSET_BUSY | HERMES_OFFSET_ERR)) && k) {
		k--;
		micro_delay (1);
		reg = hermes_read_reg (hw, oreg);
	}

	/* Busy bit didn't reset automatically */
	if (reg & HERMES_OFFSET_BUSY) {
		printf("Hermes: Error with fid 0x%x. Err: 0x%x\n", id, reg);
		return -ETIMEDOUT;
	}

	/* There has gone something wrong: offset is outside the buffer 
	 * boundary or the fid is not correct */
	if (reg & HERMES_OFFSET_ERR) {
		printf("Hermes: Error with fid 0x%x. Err: 0x%x\n", id, reg);
		return -EIO;
	}

	/* If we arrive here, the buffer can be accessed through the data 
	 * register associated with the BAP */
	return 0;
}


/*****************************************************************************
 *            hermes_bap_pread                                               *
 *                                                                           *
 * Read a block of data from the chip's buffer, via the BAP. len must be     *
 * even.                                                                     *
 *****************************************************************************/
int hermes_bap_pread (hermes_t * hw, int bap, void *buf, unsigned len,
		  u16_t id, u16_t offset) {
	/* The data register is the access point for the buffer made
	 * available by setting the BAP right. Which BAP does the user 
	 * want to use? there are 2 of them */
	int dreg = bap ? HERMES_DATA1 : HERMES_DATA0;
	int err = 0;

	/* reading (and writing) data goes a word a time, so should be even */
	if ((len % 2))	{
		printf("Hermes: Error in length to be read\n");
		return -EINVAL;
	}

	/* Set the cards internal pointer to the right fid and to the right
	 * offset */
	err = hermes_bap_seek (hw, bap, id, offset);
	if (err) {
		printf("Hermes: error hermes_bap_seek in hermes_bap_pread\n");
		return err;
	}
	/* Actually do the transfer. The length is divided by 2 because
	 * transfers go a word at a time as far as the card is concerned */
	hermes_read_words (hw, dreg, buf, len / 2);

	return err;
}

/*****************************************************************************
 *            hermes_write_words                                             *
 *                                                                           *
 * Write a sequence of words of the buffer to the card                       *
 *****************************************************************************/
void hermes_write_words (hermes_t * hw, int off, const void *buf, 
						unsigned count) {
	int i = 0;

	for (i = 0; i < count; i++)	{
		hermes_write_reg (hw, off, *((u16_t *) buf + i));
	}
}

/*****************************************************************************
 *            hermes_bap_pwrite                                              *
 *                                                                           *
 * Write a block of data to the chip's buffer, via the BAP. len must be even.*
 *****************************************************************************/
int hermes_bap_pwrite (hermes_t * hw, int bap, const void *buf, unsigned len,
		   u16_t id, u16_t offset) {

	/* This procedure is quite the same as the hermes_bap_read */
	int dreg = bap ? HERMES_DATA1 : HERMES_DATA0;
	int err = 0;

	if ((len % 2)) {
		printf("Hermes: Error in length to be written\n");
		return -EINVAL;
	}

	/* Set the cards internal pointer to the right fid and to the right
	 * offset */
	err = hermes_bap_seek (hw, bap, id, offset);
	if (err) {
		printf("Hermes: hermes_bap_seek error in hermes_bap_pwrite\n");
		return err;

	}

	/* Actually do the transfer */
	hermes_write_words (hw, dreg, buf, len / 2);

	return err;
}



/*****************************************************************************
 *            hermes_set_irqmask                                             *
 *                                                                           *
 * Which events should the card respond to with an interrupt?                *
 *****************************************************************************/
int hermes_set_irqmask (hermes_t * hw, u16_t events) {
	hw->inten = events;
	hermes_write_reg (hw, HERMES_INTEN, events);
	
	/* Compare written value with read value to check whether things 
	 * succeeded */
	if (hermes_read_reg (hw, HERMES_INTEN) != events) {
		printf("Hermes: error setting irqmask\n");
		return 1;
	}

	return (0);
}

/*****************************************************************************
 *            hermes_set_irqmask                                             *
 *                                                                           *
 * Which events does the card respond to with an interrupt?                  *
 *****************************************************************************/
u16_t hermes_get_irqmask (hermes_t * hw) {
	return hermes_read_reg (hw, HERMES_INTEN);
}


/*****************************************************************************
 *            hermes_read_ltv                                                *
 *                                                                           *
 * Read a Length-Type-Value record from the card. These are configurable     *
 * parameters in the cards firmware, like wepkey, essid, mac address etc.    *
 * Another name for them are 'rids', Resource Identifiers. See hermes_rids.h *
 * for all available rids                                                    *
 * If length is NULL, we ignore the length read from the card, and           *
 * read the entire buffer regardless. This is useful because some of         *
 * the configuration records appear to have incorrect lengths in             *
 * practice.                                                                 *
 *****************************************************************************/
int hermes_read_ltv (hermes_t * hw, int bap, u16_t rid, unsigned bufsize,
		 u16_t * length, void *buf) {
	int err = 0;
	int dreg = bap ? HERMES_DATA1 : HERMES_DATA0;
	u16_t rlength, rtype;
	unsigned nwords;

	if ((bufsize % 2))	{
		printf("Hermes: error in bufsize\n");
		return -EINVAL;
	}

	err = hermes_docmd_wait (hw, HERMES_CMD_ACCESS, rid, NULL);
	if (err) {
		printf("Hermes: error hermes_docmd_wait in hermes_read_ltv\n");
		return err;
	}

	err = hermes_bap_seek (hw, bap, rid, 0);
	if (err) {
		printf("Hermes: error hermes_bap_seek in hermes_read_ltv\n");
		return err;
	}

	rlength = hermes_read_reg (hw, dreg);

	if (!rlength) {
		printf( "Hermes: Error rlength\n");
		return -ENOENT;
	}

	rtype = hermes_read_reg (hw, dreg);

	if (length)
		*length = rlength;

	if (rtype != rid) {
		printf("hermes @ %x: hermes_read_ltv(): rid  (0x%04x)",
			hw->iobase, rid);
		printf("does not match type (0x%04x)\n", rtype);
	}

	if (HERMES_RECLEN_TO_BYTES (rlength) > bufsize) {
		printf("hermes @ %x: Truncating LTV record from ",
			hw->iobase);
		printf("%d to %d bytes. (rid=0x%04x, len=0x%04x)\n",
			HERMES_RECLEN_TO_BYTES (rlength), bufsize, rid,
			rlength);
	}
	nwords = MIN ((unsigned) rlength - 1, bufsize / 2);
	hermes_read_words (hw, dreg, buf, nwords);

	return 0;
}


/*****************************************************************************
 *            hermes_write_ltv                                               *
 *                                                                           *
 * Write a Length-Type-Value record to the card. These are configurable      *
 * parameters in the cards firmware, like wepkey, essid, mac address etc.    *
 * Another name for them are 'rids', Resource Identifiers. See hermes_rids.h *
 * for all available rids                                                    *
 *****************************************************************************/
int hermes_write_ltv (hermes_t * hw, int bap, u16_t rid,
		  u16_t length, const void *value) {
	int dreg = bap ? HERMES_DATA1 : HERMES_DATA0;
	int err = 0;
	unsigned count;

	if (length == 0) {
		printf("Hermes: length==0 in hermes_write_ltv\n");
		return -EINVAL;
	}

	err = hermes_bap_seek (hw, bap, rid, 0);
	if (err) {
		printf("Hermes: error hermes_bap_seek in hermes_write_ltv\n");
		return err;
	}

	hermes_write_reg (hw, dreg, length);
	hermes_write_reg (hw, dreg, rid);

	count = length - 1;

	hermes_write_words (hw, dreg, value, count);

	err = hermes_docmd_wait (hw, HERMES_CMD_ACCESS | HERMES_CMD_WRITE,
				 rid, NULL);
	if (err)
		printf("Hermes: error hermes_docmd_wait in hermes_write_ltv\n");

	return err;
}


/*****************************************************************************
 *            hermes_write_wordrec                                           *
 *                                                                           *
 * A shorthand for hermes_write_ltv when the field is 2 bytes long           *
 *****************************************************************************/
int hermes_write_wordrec (hermes_t * hw, int bap, u16_t rid, u16_t word) {

	u16_t rec;
	int err;
	rec = (word);

	err = hermes_write_ltv (hw, bap, rid,
				HERMES_BYTES_TO_RECLEN (sizeof (rec)), &rec);

	if (err)
		printf("Hermes: error in write_wordrec\n");
	return err;
}


/*****************************************************************************
 *            hermes_read_wordrec                                            *
 *                                                                           *
 * A shorthand for hermes_read_ltv when the field is 2 bytes long            *
 *****************************************************************************/
int hermes_read_wordrec (hermes_t * hw, int bap, u16_t rid, u16_t * word) {
	u16_t rec;
	int err;

	err = hermes_read_ltv (hw, bap, rid, sizeof (rec), NULL, &rec);
	*word = (rec);
	if (err)
		printf("Hermes: Error in read_wordrec\n");
	return err;
}


/*****************************************************************************
 *            hermes_read_words                                              *
 *                                                                           *
 * Read a sequence of words from the card to the buffer                      *
 *****************************************************************************/
void hermes_read_words (hermes_t * hw, int off, void *buf, unsigned count) {
	int i = 0;
	u16_t reg;

	for (i = 0; i < count; i++)	{
		reg = hermes_read_reg (hw, off);
		*((u16_t *) buf + i) = (u16_t) reg;
	}
}


/*****************************************************************************
 *            hermes_read_reg                                                *
 *                                                                           *
 * Read a value from a certain register. Currently only memory mapped        *
 * registers are supported, but accessing I/O spaced registers should be     *
 * quite trivial                                                             *
 *****************************************************************************/
u16_t hermes_read_reg (const hermes_t * hw, u16_t off) {
	int v = 0;
	v = *((int *)(hw->locmem + (off << hw->reg_spacing)));
	return (u16_t) v;
}

/*****************************************************************************
 *            hermes_write_reg                                               *
 *                                                                           *
 * Write a value to a certain register. Currently only memory mapped         *
 * registers are supported, but accessing I/O spaced registers should be     *
 * quite trivial                                                             *
 *****************************************************************************/
void hermes_write_reg (const hermes_t * hw, u16_t off, u16_t val) {
	int v = (int) val;	
	*(int *)(hw->locmem + (off << hw->reg_spacing)) = v;
}

